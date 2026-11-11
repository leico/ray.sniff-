// A pcap handle opened from a device or from a capture file (design.md 5).

#pragma once

#include <pcap/pcap.h>

#include <cstdint>
#include <memory>
#include <string>
#include <utility>

namespace raysniff {

// Settings of a live capture (design.md 4-①②).
constexpr int kSnapLength = 65535;                  // max bytes kept per packet
constexpr int kReadTimeoutMs = 1;                   // lets the capture thread check for stop requests
constexpr int kKernelBufferBytes = 8 * 1024 * 1024; // the kernel caps this at debug.bpf_bufsize_cap (32 MB)

enum class CaptureError {
    none,
    permission_denied,   // no access to /dev/bpf* (design.md 6)
    no_such_device,
    no_default_device,
    bad_filter,
    other,
};

struct CaptureResult {
    CaptureError error = CaptureError::none;
    std::string message;  // the reason on failure; a warning (or empty) on success

    bool ok() const { return error == CaptureError::none; }

    static CaptureResult failure(CaptureError error, std::string message) {
        CaptureResult result;
        result.error = error;
        result.message = std::move(message);
        return result;
    }
};

// Threads: a PcapSource is used by one thread at a time. While a
// CaptureThread reads from it, the main thread must not touch it.
class PcapSource {
public:
    // Opens a network device (must not be empty) with the settings above.
    // On success `result.message` may hold a warning from pcap_activate().
    static std::unique_ptr<PcapSource> open_live(const std::string& device, CaptureResult& result);

    // Opens a capture file (.pcap). Used by the tests.
    static std::unique_ptr<PcapSource> open_offline(const std::string& path, CaptureResult& result);

    ~PcapSource();
    PcapSource(const PcapSource&) = delete;
    PcapSource& operator=(const PcapSource&) = delete;

    // Compiles and installs a BPF filter. An empty expression accepts every
    // packet. On failure the current filter stays installed.
    CaptureResult set_filter(const std::string& expression);

    // Reads ps_drop. Returns false for capture files, which have no statistics.
    bool dropped_packets(std::uint32_t& dropped);

    pcap_t* handle() const { return handle_; }
    const std::string& name() const { return name_; }      // device name or file path
    const std::string& filter() const { return filter_; }  // the installed filter
    bool offline() const { return offline_; }

private:
    PcapSource(pcap_t* handle, std::string name, bool offline, bpf_u_int32 netmask);

    pcap_t* const handle_;
    const std::string name_;
    const bool offline_;
    const bpf_u_int32 netmask_;  // used by filters such as "ip broadcast"
    std::string filter_;
};

// Checks the syntax of a filter without an open handle (compiled for
// Ethernet). Lets an invalid @filter be rejected while nothing is captured.
CaptureResult check_filter(const std::string& expression);

} // namespace raysniff
