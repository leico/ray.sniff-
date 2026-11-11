// The capture thread: pcap -> PacketQueue (design.md 3, 5).

#pragma once

#include "drop_counter.h"
#include "packet_queue.h"
#include "pcap_source.h"

#include <atomic>
#include <string>
#include <thread>

namespace raysniff {

enum class CaptureState {
    stopped,   // not started, or stop() was called
    running,
    finished,  // reached the end of a capture file
    failed,    // pcap reported an error; see error()
};

// Threads: start() and stop() on the main thread. The thread itself is the
// only writer of the queue and of the drop counter. state() may be read from
// any thread.
//
// The thread does not report anything by itself (design.md 5): it writes
// error_ and then stores `failed` (release). error() reads error_ only after
// it has seen `failed` (acquire), so the reason is complete.
class CaptureThread {
public:
    CaptureThread(PacketQueue& queue, PcapDropCounter& drops);
    ~CaptureThread();
    CaptureThread(const CaptureThread&) = delete;
    CaptureThread& operator=(const CaptureThread&) = delete;

    // Starts reading `source`, which must stay alive until stop(). The thread
    // must be stopped (call stop() first, also after finished / failed).
    void start(PcapSource& source);

    // Asks the thread to exit and waits for it. The thread checks the request
    // after every packet and every read timeout (kReadTimeoutMs), so this
    // returns within a few milliseconds. Afterwards state() is `stopped`.
    void stop();

    CaptureState state() const { return state_.load(std::memory_order_acquire); }

    // If state() is `failed`, copies the reason into `reason` and returns true.
    // Otherwise leaves `reason` unchanged and returns false. Checking the state
    // here (acquire) is what makes reading error_ safe.
    bool error(std::string& reason) const {
        if (state() != CaptureState::failed) return false;
        reason = error_;
        return true;
    }

private:
    void run(PcapSource* source);

    PacketQueue& queue_;
    PcapDropCounter& drops_;
    std::thread thread_;
    std::atomic<bool> stop_requested_{false};
    std::atomic<CaptureState> state_{CaptureState::stopped};
    std::string error_;  // written by the thread before it stores `failed`
};

} // namespace raysniff
