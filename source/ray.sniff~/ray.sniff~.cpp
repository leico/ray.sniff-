// ray.sniff~ : outputs the raw bytes of network packets as an audio signal.
//
// The design is in docs/design.md (7 for this object). Everything that does
// not need Max lives in source/core/ and is tested there; this file only
// connects it to Max.
//
// Threads (design.md 3):
//   main       messages, attributes, opening and closing the capture
//   scheduler  the 100 ms clock: polls DSP and capture state, wakes the qelem
//   audio      perform64: reads the queue (its only reader)
//   capture    inside CaptureSession: writes the queue (its only writer)
// Messages and attribute changes that arrive on another thread are deferred
// to the main thread, so the session, the gate, the stats baselines and the
// ring pointer (replaced by @queuesize) are only touched there. Only
// @bitdepth is handled on any thread (an atomic store).

#include "capture_gate.h"
#include "capture_session.h"
#include "packet_queue.h"
#include "sample_reader.h"

#include "ext.h"
#include "ext_obex.h"
#include "ext_systhread.h"
#include "z_dsp.h"

#include <atomic>
#include <cstdint>
#include <cstring>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace {

constexpr long kDefaultBitdepth = 16;  // design.md 7.1
constexpr double kPollIntervalMs = 100.0;

enum class DeferredKind {
    message,    // send the message `name`
    attribute,  // set the attribute `name`
};

// A message or an attribute value that arrived on another thread and waits
// for the main thread.
struct DeferredCall {
    DeferredKind kind;
    t_symbol* name;
    std::vector<t_atom> args;
};

// Calls handed from other threads to the main thread. push() from any thread
// except the audio thread, take() on the main thread; the lock is held only
// inside these two. The audio thread never takes it.
class DeferredCalls {
public:
    void push(DeferredCall call) {
        std::lock_guard<std::mutex> lock(mutex_);
        calls_.push_back(std::move(call));
    }

    // Takes every call pushed so far, in the order they arrived, and leaves
    // none behind. The caller runs them without the lock.
    std::vector<DeferredCall> take() {
        std::vector<DeferredCall> taken;
        std::lock_guard<std::mutex> lock(mutex_);
        taken.swap(calls_);
        return taken;
    }

private:
    std::mutex mutex_;
    std::vector<DeferredCall> calls_;
};

// t_raysniff is allocated by Max (object_alloc) without running C++
// constructors, so its C++ parts are created with new and kept by pointer:
// Engine (below) and DeferredCalls (above).
//
// Engine is the path from the capture to the audio: the queue, the capture,
// and the decision to open or close it. It uses no Max types. The members are
// declared in this order so that the session (and its capture thread) is
// destroyed before the queue it writes to.
struct Engine {
    explicit Engine(std::unique_ptr<raysniff::PacketQueue> q)
        : queue(std::move(q)), ring(&queue->ring()), session(*queue) {}

    // Replaces the queue's ring and keeps `ring` pointing at the new one.
    // Only while DSP and the capture are stopped (@queuesize, design.md 7.2).
    bool resize_queue(std::size_t bytes) {
        if (!queue->resize(bytes)) return false;
        ring = &queue->ring();
        return true;
    }

    std::unique_ptr<raysniff::PacketQueue> queue;
    // What perform64 reads. Holding the ring itself keeps the audio thread
    // away from PacketQueue's stats, which the capture thread keeps writing.
    // Always &queue->ring(): change the queue only through resize_queue().
    raysniff::ByteRing* ring;
    std::atomic<int> bytes_per_sample{raysniff::bytes_per_sample(kDefaultBitdepth)};

    raysniff::CaptureSession session;
    raysniff::CaptureGate gate;  // main thread only
};

} // namespace

struct t_raysniff {
    t_pxobject ob;
    void* dumpout;
    void* poll_clock;
    void* update_qelem;
    void* deferred_qelem;

    // Scheduler thread only (raysniff_poll): what the previous poll saw.
    bool polled_dsp;
    raysniff::CaptureState polled_state;

    // Attributes (main thread).
    long bitdepth;
    t_symbol* device;  // "" chooses the default device
    t_symbol* filter;  // the whole BPF expression as one symbol
    long queuesize;    // KB

    Engine* engine;
    DeferredCalls* deferred;
};

static t_class* raysniff_class = nullptr;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

// Main thread (deferred_qelem). Runs the calls in the order they arrived.
// A qelem rather than defer_low(): qelem_free() in raysniff_free cancels a
// pending run, so this never runs for a freed object.
static void raysniff_run_deferred(t_raysniff* x) {
    for (DeferredCall& call : x->deferred->take()) {
        const long argc = static_cast<long>(call.args.size());
        if (call.kind == DeferredKind::attribute) object_attr_setvalueof(x, call.name, argc, call.args.data());
        else                                      object_method_typed   (x, call.name, argc, call.args.data(), nullptr);
    }
}

// On a thread other than the main one, keeps a copy of the call for
// raysniff_run_deferred and returns true. On the main thread returns false,
// and the caller goes on.
static bool defer_call(t_raysniff* x, DeferredKind kind, const char* name, long argc, const t_atom* argv) {
    if (systhread_ismainthread()) return false;
    x->deferred->push({kind, gensym(name), std::vector<t_atom>(argv, argv + argc)});
    qelem_set(x->deferred_qelem);
    return true;
}

static void report_open(t_raysniff* x, const raysniff::CaptureResult& result) {
    if (result.ok()) {
        if (!result.message.empty()) object_warn((t_object*)x, "%s", result.message.c_str());
        const raysniff::CaptureSession& session = x->engine->session;
        object_post((t_object*)x, "capturing on %s%s%s", session.source_name().c_str(),
                    session.filter().empty() ? "" : ", filter: ", session.filter().c_str());
        return;
    }
    object_error((t_object*)x, "%s", result.message.c_str());
    if (result.error == raysniff::CaptureError::permission_denied) {
        // design.md 6: the object does not change permissions itself.
        object_error((t_object*)x, "capturing needs access to /dev/bpf*. Either:");
        object_error((t_object*)x, "  1. brew install --cask wireshark-chmodbpf (then log in again)");
        object_error((t_object*)x, "  2. start Max as root");
    }
}

static void report_lock(t_raysniff* x) {
    const raysniff::ByteRing& ring = *x->engine->ring;
    if (ring.locked()) return;
    // design.md 4 (3): keep running without the lock, but say so.
    object_warn((t_object*)x, "can't lock the queue in memory (%s); audio may drop out when memory is low",
                std::strerror(ring.lock_errno()));
}

static void output_value(t_raysniff* x, const char* group, const char* name, std::uint64_t value) {
    t_atom atoms[2];
    atom_setsym (&atoms[0], gensym(name));
    atom_setlong(&atoms[1], static_cast<t_atom_long>(value));
    outlet_anything(x->dumpout, gensym(group), 2, atoms);
}

// Joins atoms into one string, as the legacy filter arguments and @filter do.
static std::string join_atoms(long argc, const t_atom* argv) {
    std::string joined;
    for (long i = 0; i < argc; ++i) {
        std::string token;
        switch (atom_gettype(argv + i)) {
            case A_LONG:  token = std::to_string(atom_getlong (argv + i)); break;
            case A_FLOAT: token = std::to_string(atom_getfloat(argv + i)); break;
            case A_SYM:   token = atom_getsym(argv + i)->s_name;           break;
            default: continue;
        }
        if (!joined.empty()) joined += ' ';
        joined += token;
    }
    return joined;
}

// ---------------------------------------------------------------------------
// Opening and closing (design.md 7.4)
// ---------------------------------------------------------------------------

// Main thread. Asks the gate what to do now and does it.
static void raysniff_update(t_raysniff* x) {
    Engine& engine = *x->engine;
    const bool dsp_on = sys_getdspobjdspstate((t_object*)x) != 0;
    const bool failed = engine.session.state() == raysniff::CaptureState::failed;

    switch (engine.gate.update(dsp_on, engine.session.is_open(), failed)) {
        case raysniff::GateAction::none:
            break;
        case raysniff::GateAction::open: {
            const raysniff::CaptureResult result = engine.session.open_live(x->device->s_name);
            report_open(x, result);
            engine.gate.open_result(result.ok());
            break;
        }
        case raysniff::GateAction::close:
            engine.session.close();
            break;
        case raysniff::GateAction::close_after_failure: {
            std::string reason;
            if (engine.session.error(reason)) object_error((t_object*)x, "%s", reason.c_str());
            engine.session.close();
            break;
        }
    }
}

// Scheduler thread. Wakes the main thread only when something changed.
static void raysniff_poll(t_raysniff* x) {
    const bool dsp_on = sys_getdspobjdspstate((t_object*)x) != 0;
    const raysniff::CaptureState state = x->engine->session.state();  // any thread
    if (dsp_on != x->polled_dsp || state != x->polled_state) {
        x->polled_dsp   = dsp_on;
        x->polled_state = state;
        qelem_set(x->update_qelem);
    }
    clock_fdelay(x->poll_clock, kPollIntervalMs);
}

// ---------------------------------------------------------------------------
// Messages (design.md 7.3)
// ---------------------------------------------------------------------------

static void raysniff_int(t_raysniff* x, long n) {
    t_atom a;
    atom_setlong(&a, n);
    if (defer_call(x, DeferredKind::message, "int", 1, &a)) return;
    x->engine->gate.set_requested(n > 0);
    raysniff_update(x);
}

// clear and size go to the main thread too: @queuesize replaces the ring
// there, so another thread could otherwise reach a ring that was just freed.
static void raysniff_clear(t_raysniff* x) {
    if (defer_call(x, DeferredKind::message, "clear", 0, nullptr)) return;
    x->engine->ring->request_clear();  // the audio thread applies it at its next block
}

static void raysniff_stats(t_raysniff* x) {
    if (defer_call(x, DeferredKind::message, "stats", 0, nullptr)) return;
    Engine& engine = *x->engine;
    const raysniff::QueueStatsSnapshot queue_snapshot = engine.queue->stats().snapshot();

    output_value(x, "pcap", "dropped_packets" , engine.session.dropped_packets());
    output_value(x, "queue", "locked"         , engine.ring->locked() ? 1 : 0);
    output_value(x, "queue", "size"           , engine.ring->unread_bytes());

    output_value(x, "queue", "written_packets", queue_snapshot.written_packets);
    output_value(x, "queue", "written_bytes"  , queue_snapshot.written_bytes);
    output_value(x, "queue", "dropped_packets", queue_snapshot.dropped_packets);
    output_value(x, "queue", "dropped_bytes"  , queue_snapshot.dropped_bytes);
}

static void raysniff_resetstats(t_raysniff* x) {
    if (defer_call(x, DeferredKind::message, "resetstats", 0, nullptr)) return;
    Engine& engine = *x->engine;
    engine.queue->stats().reset();
    engine.session.reset_dropped_packets();
}

// Deprecated: use `stats` (queue size). Kept for old patches.
static void raysniff_size(t_raysniff* x) {
    if (defer_call(x, DeferredKind::message, "size", 0, nullptr)) return;
    t_atom a;
    atom_setlong(&a, static_cast<t_atom_long>(x->engine->ring->unread_bytes()));
    outlet_anything(x->dumpout, gensym("size"), 1, &a);
}

static void raysniff_assist(t_raysniff*, void*, long m, long a, char* s) {
    constexpr size_t kAssistMax = 256;
    if (m == ASSIST_INLET) {
        snprintf(s, kAssistMax, "(int) 1 to capture while DSP is on, 0 to stop; clear, stats, resetstats");
    } else if (a == 0) {
        snprintf(s, kAssistMax, "(signal) packet bytes as audio");
    } else {
        snprintf(s, kAssistMax, "dumpout: stats (pcap ..., queue ...), size");
    }
}

// ---------------------------------------------------------------------------
// Attributes (design.md 7.2)
// ---------------------------------------------------------------------------

static t_max_err raysniff_set_bitdepth(t_raysniff* x, void*, long argc, t_atom* argv) {
    if (argc < 1) return MAX_ERR_GENERIC;
    const long bitdepth = atom_getlong(argv);
    const int bytes = raysniff::bytes_per_sample(bitdepth);
    if (bytes == 0) {
        object_error((t_object*)x, "unsupported bitdepth %ld (use 8, 16, 24 or 32)", bitdepth);
        return MAX_ERR_GENERIC;
    }
    x->bitdepth = bitdepth;
    // Read by perform64 at the start of each block; nothing else depends on it.
    x->engine->bytes_per_sample.store(bytes, std::memory_order_relaxed);
    return MAX_ERR_NONE;
}

static t_max_err raysniff_set_device(t_raysniff* x, void*, long argc, t_atom* argv) {
    if (defer_call(x, DeferredKind::attribute, "device", argc, argv)) return MAX_ERR_NONE;
    t_symbol* device = (argc > 0 && atom_gettype(argv) == A_SYM) ? atom_getsym(argv) : gensym("");
    Engine& engine = *x->engine;

    if (engine.session.is_open()) {
        // Opens the new device first; on failure the current one keeps running.
        const raysniff::CaptureResult result = engine.session.open_live(device->s_name);
        report_open(x, result);
        if (!result.ok()) return MAX_ERR_GENERIC;
    }
    x->device = device;
    engine.gate.config_changed();
    raysniff_update(x);
    return MAX_ERR_NONE;
}

static t_max_err raysniff_set_filter(t_raysniff* x, void*, long argc, t_atom* argv) {
    if (defer_call(x, DeferredKind::attribute, "filter", argc, argv)) return MAX_ERR_NONE;
    const std::string expression = join_atoms(argc, argv);
    Engine& engine = *x->engine;

    // While closed this only checks the syntax. On failure the old filter stays.
    const raysniff::CaptureResult result = engine.session.set_filter(expression);
    if (!result.ok()) {
        object_error((t_object*)x, "%s", result.message.c_str());
        return MAX_ERR_GENERIC;
    }
    x->filter = gensym(expression.c_str());
    engine.gate.config_changed();
    raysniff_update(x);
    return MAX_ERR_NONE;
}

static t_max_err raysniff_set_queuesize(t_raysniff* x, void*, long argc, t_atom* argv) {
    if (defer_call(x, DeferredKind::attribute, "queuesize", argc, argv)) return MAX_ERR_NONE;
    if (argc < 1) return MAX_ERR_GENERIC;
    const long requested = atom_getlong(argv);
    const long size = raysniff::normalize_queue_size_kb(requested);
    if (size != requested) object_post((t_object*)x, "queuesize %ld KB -> %ld KB", requested, size);
    if (size == x->queuesize) return MAX_ERR_NONE;

    Engine& engine = *x->engine;
    // perform64 holds the ring and the capture thread writes to it.
    if (sys_getdspobjdspstate((t_object*)x) || engine.session.is_open()) {
        object_warn((t_object*)x, "queuesize can be changed only while DSP and the capture are stopped; ignored");
        return MAX_ERR_GENERIC;
    }
    if (!engine.resize_queue(static_cast<std::size_t>(size) * 1024)) {
        object_error((t_object*)x, "can't allocate a queue of %ld KB; keeping %ld KB", size, x->queuesize);
        return MAX_ERR_GENERIC;
    }
    x->queuesize = size;
    report_lock(x);
    return MAX_ERR_NONE;
}

// ---------------------------------------------------------------------------
// DSP
// ---------------------------------------------------------------------------

static void raysniff_perform64(t_raysniff* x, t_object*, double**, long, double** outs, long,
                               long sampleframes, long, void*) {
    Engine& engine = *x->engine;
    raysniff::read_samples(*engine.ring, engine.bytes_per_sample.load(std::memory_order_relaxed), outs[0],
                           sampleframes);
}

static void raysniff_dsp64(t_raysniff* x, t_object* dsp64, short*, double, long, long) {
    object_method(dsp64, gensym("dsp_add64"), x, raysniff_perform64, 0, nullptr);
    qelem_set(x->update_qelem);  // DSP is starting: check now rather than at the next poll
}

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

// The legacy positional arguments (design.md 7.1):
//   <bitdepth> [device] [ip] [netmask] [filter ...]
// ip and netmask are ignored. Attributes given with @ are applied afterwards
// and win.
static void apply_legacy_arguments(t_raysniff* x, long argc, t_atom* argv) {
    if (argc >= 1) {
        const long bitdepth = atom_gettype(argv) == A_LONG ? atom_getlong(argv) : 0;
        if (raysniff::bytes_per_sample(bitdepth) != 0) {
            object_attr_setlong(x, gensym("bitdepth"), bitdepth);
        } else {
            object_error((t_object*)x, "1st argument must be a bitdepth (8, 16, 24 or 32); using %ld",
                         kDefaultBitdepth);
        }
    } else {
        object_error((t_object*)x, "no bitdepth given (8, 16, 24 or 32); using %ld", kDefaultBitdepth);
    }
    if (argc >= 2 && atom_gettype(argv + 1) == A_SYM) {
        object_attr_setsym(x, gensym("device"), atom_getsym(argv + 1));
    }
    if (argc >= 5) {
        object_attr_setvalueof(x, gensym("filter"), argc - 4, argv + 4);
    }
}

static void* raysniff_new(t_symbol*, long argc, t_atom* argv) {
    auto queue = raysniff::PacketQueue::create(static_cast<std::size_t>(raysniff::kQueueSizeDefaultKb) * 1024);
    if (!queue) {
        object_error(nullptr, "ray.sniff~: can't allocate the queue");
        return nullptr;
    }

    auto* x = static_cast<t_raysniff*>(object_alloc(raysniff_class));
    if (!x) return nullptr;

    x->engine       = new Engine(std::move(queue));
    x->deferred     = new DeferredCalls;
    x->polled_dsp   = false;
    x->polled_state = raysniff::CaptureState::stopped;
    x->bitdepth     = kDefaultBitdepth;
    x->device       = gensym("");
    x->filter       = gensym("");
    x->queuesize    = raysniff::kQueueSizeDefaultKb;

    dsp_setup(&x->ob, 0);
    x->dumpout = outlet_new(x, nullptr);  // right outlet (created first)
    object_obex_store(x, gensym("dumpout"), static_cast<t_object*>(x->dumpout));  // replies to getbitdepth etc.
    outlet_new(x, "signal");              // left outlet

    x->update_qelem   = qelem_new(x, (method)raysniff_update);
    x->deferred_qelem = qelem_new(x, (method)raysniff_run_deferred);
    x->poll_clock     = clock_new(x, (method)raysniff_poll);

    // The object is always created; nothing is opened here (design.md 7.4).
    apply_legacy_arguments(x, attr_args_offset(static_cast<short>(argc), argv), argv);
    attr_args_process(x, static_cast<short>(argc), argv);
    report_lock(x);

    clock_fdelay(x->poll_clock, kPollIntervalMs);
    return x;
}

static void raysniff_free(t_raysniff* x) {
    dsp_free(&x->ob);  // perform64 is no longer called
    if (x->poll_clock) {
        clock_unset(x->poll_clock);
        object_free(x->poll_clock);
    }
    if (x->update_qelem)   qelem_free(x->update_qelem);
    if (x->deferred_qelem) qelem_free(x->deferred_qelem);  // drops calls not run yet
    delete x->engine;   // stops the capture thread, then frees the queue
    delete x->deferred; // after qelem_free: nothing takes from it any more
}

void ext_main(void*) {
    t_class* c = class_new("ray.sniff~", (method)raysniff_new, (method)raysniff_free,
                           sizeof(t_raysniff), nullptr, A_GIMME, 0);

    class_addmethod(c, (method)raysniff_int,        "int",        A_LONG, 0);
    class_addmethod(c, (method)raysniff_clear,      "clear",      0);
    class_addmethod(c, (method)raysniff_stats,      "stats",      0);
    class_addmethod(c, (method)raysniff_resetstats, "resetstats", 0);
    class_addmethod(c, (method)raysniff_size,       "size",       0);
    class_addmethod(c, (method)raysniff_dsp64,      "dsp64",      A_CANT, 0);
    class_addmethod(c, (method)raysniff_assist,     "assist",     A_CANT, 0);
    class_addmethod(c, (method)object_obex_dumpout, "dumpout",    A_CANT, 0);

    // Choices shown in the inspector.
    constexpr const char* kBitdepthChoices = "8 16 24 32";
    // Powers of two from raysniff::kQueueSizeMinKb to raysniff::kQueueSizeMaxKb.
    constexpr const char* kQueueSizeChoicesKb =
        "128 256 512 1024 2048 4096 8192 16384 32768 65536 131072 262144 524288 1048576";

    CLASS_ATTR_LONG     (c, "bitdepth", 0, t_raysniff, bitdepth);
    CLASS_ATTR_ACCESSORS(c, "bitdepth", nullptr, raysniff_set_bitdepth);
    CLASS_ATTR_ENUM     (c, "bitdepth", 0, kBitdepthChoices);
    CLASS_ATTR_LABEL    (c, "bitdepth", 0, "Bit Depth");

    CLASS_ATTR_SYM      (c, "device", 0, t_raysniff, device);
    CLASS_ATTR_ACCESSORS(c, "device", nullptr, raysniff_set_device);
    CLASS_ATTR_LABEL    (c, "device", 0, "Device (empty: automatic)");

    CLASS_ATTR_SYM      (c, "filter", 0, t_raysniff, filter);
    CLASS_ATTR_ACCESSORS(c, "filter", nullptr, raysniff_set_filter);
    CLASS_ATTR_LABEL    (c, "filter", 0, "BPF Filter");

    CLASS_ATTR_LONG     (c, "queuesize", 0, t_raysniff, queuesize);
    CLASS_ATTR_ACCESSORS(c, "queuesize", nullptr, raysniff_set_queuesize);
    CLASS_ATTR_ENUM     (c, "queuesize", 0, kQueueSizeChoicesKb);
    CLASS_ATTR_LABEL    (c, "queuesize", 0, "Queue Size (KB)");

    class_dspinit(c);
    class_register(CLASS_BOX, c);
    raysniff_class = c;
}
