#include "capture_session.h"

#include "pcap_device.h"

#include <utility>
#include <cassert>

namespace raysniff {

CaptureSession::CaptureSession(PacketQueue& queue) : thread_(queue, drops_) {}

CaptureSession::~CaptureSession() {
    close();
}

CaptureResult CaptureSession::open_live(const std::string& device) {
    std::string name = device;
    if (name.empty()) {
        std::string error;
        name = find_default_device(error);
        if (name.empty()) return CaptureResult::failure(CaptureError::no_default_device, "can't choose a device: " + error);
    }

    CaptureResult try_open_result;

    auto source = PcapSource::open_live(name, try_open_result);
    if( ! source) return try_open_result;

    const CaptureResult try_installed_result = install(std::move(source));
    if( ! try_installed_result.ok() ) return try_installed_result;

    return try_open_result; // may carry a warning from pcap_activate()
}

CaptureResult CaptureSession::open_offline(const std::string& path) {
    CaptureResult try_open_result;

    auto source = PcapSource::open_offline(path, try_open_result);
    if( ! source) return try_open_result;

    return install(std::move(source)); // files open without warnings
}

// source must not be null; the callers check it.
CaptureResult CaptureSession::install(std::unique_ptr<PcapSource> source) {
    assert(source);

    // Prepare the new source completely before touching the running one.
    const CaptureResult filtered = source->set_filter(filter_);
    if (!filtered.ok()) return filtered;

    thread_.stop();
    source_ = std::move(source);  // closes the previous handle, if any
    thread_.start(*source_);
    return {};
}

void CaptureSession::close() {
    thread_.stop();
    source_.reset();
}

CaptureResult CaptureSession::set_filter(const std::string& expression) {
    if (!source_) {
        CaptureResult result = check_filter(expression);
        if (result.ok()) filter_ = expression;
        return result;
    }

    // A pcap handle must not be used by two threads at once.
    thread_.stop();
    CaptureResult result = source_->set_filter(expression);
    if (result.ok()) filter_ = expression;
    thread_.start(*source_);
    return result;
}

} // namespace raysniff
