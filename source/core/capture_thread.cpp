#include "capture_thread.h"

#include <chrono>
#include <system_error>

namespace raysniff {

namespace {

// How often the capture thread reads pcap_stats() (design.md 7.5).
constexpr std::chrono::milliseconds kStatsInterval{100};

} // namespace

CaptureThread::CaptureThread(PacketQueue& queue, PcapDropCounter& drops)
    : queue_(queue), drops_(drops) {}

CaptureThread::~CaptureThread() {
    stop();
}

void CaptureThread::start(PcapSource& source) {
    if (thread_.joinable()) return;  // still running: stop() first

    // Everything the thread reads or may overwrite is set before it is created.
    // Creating a std::thread synchronizes, so run() sees these values. In
    // particular `running` must come first: a short file may finish (and store
    // `finished`) before std::thread's constructor even returns, and storing
    // `running` afterwards would hide that.
    error_.clear();
    stop_requested_.store(false, std::memory_order_relaxed);
    state_.store(CaptureState::running, std::memory_order_release);
    try {
        thread_ = std::thread(&CaptureThread::run, this, &source);
    } catch (const std::system_error& e) {
        error_ = std::string("can't start the capture thread: ") + e.what();
        state_.store(CaptureState::failed, std::memory_order_release);
    }
}

void CaptureThread::stop() {
    if (thread_.joinable()) {
        stop_requested_.store(true, std::memory_order_relaxed);
        thread_.join();
    }
    state_.store(CaptureState::stopped, std::memory_order_release);
}

void CaptureThread::run(PcapSource* source) {
    pcap_t* const handle = source->handle();

    // A new handle, or a filter change, may or may not have reset the kernel
    // counter. Start from whatever it says now.
    std::uint32_t dropped = 0;
    const bool source_has_stats = source->dropped_packets(dropped);
    if (source_has_stats) drops_.rebase(dropped);
    auto next_stats = std::chrono::steady_clock::now() + kStatsInterval;

    // Stays `stopped` when the loop ends by stop(); stop() itself stores that state.
    CaptureState final_state = CaptureState::stopped;
    while (!stop_requested_.load(std::memory_order_relaxed)) {
        pcap_pkthdr* header = nullptr;
        const u_char* data = nullptr;
        const int result = pcap_next_ex(handle, &header, &data);

        if (result == PCAP_ERROR_BREAK) {
            // End of a capture file. (A live capture would return this only
            // after pcap_breakloop(), which is not used: stop() relies on the
            // read timeout instead, so the value always means "end of file".)
            final_state = CaptureState::finished;
            break;
        }
        if (result < 0) {
            error_ = std::string("capture stopped: ") + pcap_geterr(handle);
            final_state = CaptureState::failed;
            break;
        }

        // 1: a packet. 0: read timeout of a live capture, nothing arrived.
        if (result == 1) queue_.push(data, header->caplen);

        if (source_has_stats) {
            const auto now = std::chrono::steady_clock::now();
            if (now >= next_stats) {
                if (source->dropped_packets(dropped)) drops_.update(dropped);
                next_stats = now + kStatsInterval;
            }
        }
    }

    if (source_has_stats && source->dropped_packets(dropped)) drops_.update(dropped);
    if (final_state != CaptureState::stopped) state_.store(final_state, std::memory_order_release);
}

} // namespace raysniff
