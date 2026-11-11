// What the Max object uses to capture: open, close, change device or filter
// (design.md 5, 7.2).

#pragma once

#include "capture_thread.h"
#include "drop_counter.h"
#include "packet_queue.h"
#include "pcap_source.h"

#include <cstdint>
#include <memory>
#include <string>

namespace raysniff {

// Threads: every member function is for the main thread, except state(),
// which any thread may call.
//
// The queue belongs to the caller (the Max object), so its contents survive
// a device change, and it may be resized while the session is closed.
class CaptureSession {
public:
    explicit CaptureSession(PacketQueue& queue);
    ~CaptureSession();
    CaptureSession(const CaptureSession&) = delete;
    CaptureSession& operator=(const CaptureSession&) = delete;

    // Opens a device ("" chooses the default one) or a capture file, installs
    // the current filter, and starts capturing. When something is already
    // open, it keeps running until the new source is ready; on failure it is
    // left as it was. On success the result may carry a warning.
    CaptureResult open_live(const std::string& device);
    CaptureResult open_offline(const std::string& path);

    void close();
    bool is_open() const { return source_ != nullptr; }

    // While open: stops the thread, installs the filter, restarts the thread.
    // While closed: only checks the syntax; the filter is used at the next
    // open. On failure the previous filter stays.
    CaptureResult set_filter(const std::string& expression);

    const std::string& filter() const { return filter_; }
    std::string source_name() const { return source_ ? source_->name() : std::string(); }

    CaptureState state() const { return thread_.state(); }
    bool error(std::string& reason) const { return thread_.error(reason); }  // see CaptureThread::error

    // pcap dropped_packets since the last reset_dropped_packets() (resetstats).
    std::uint64_t dropped_packets() const { return drops_.snapshot(); }
    void reset_dropped_packets() { drops_.reset(); }

private:
    CaptureResult install(std::unique_ptr<PcapSource> source);

    PcapDropCounter drops_;
    std::unique_ptr<PcapSource> source_;  // declared before thread_, so the thread is stopped first
    CaptureThread thread_;
    std::string filter_;
};

} // namespace raysniff
