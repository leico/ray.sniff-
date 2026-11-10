#include "pcap_source.h"

#include <utility>

namespace raysniff {

namespace {

CaptureError error_kind(int status) {
    switch (status) {
    case PCAP_ERROR_PERM_DENIED:
    case PCAP_ERROR_PROMISC_PERM_DENIED:
        return CaptureError::permission_denied;
    case PCAP_ERROR_NO_SUCH_DEVICE:
        return CaptureError::no_such_device;
    default:
        return CaptureError::other;
    }
}

// Compiles `expression` for `handle`. On success the caller owns `program`.
CaptureResult compile(pcap_t* handle, const std::string& expression, bpf_u_int32 netmask, bpf_program& program) {
    if (pcap_compile(handle, &program, expression.c_str(), 1, netmask) == -1) {
        return CaptureResult::failure(CaptureError::bad_filter, "can't compile filter \"" + expression + "\": " + pcap_geterr(handle));
    }
    return {};
}

} // namespace

std::unique_ptr<PcapSource> PcapSource::open_live(const std::string& device, CaptureResult& result) {
    result = {};
    char errbuf[PCAP_ERRBUF_SIZE] = {};

    pcap_t* p = pcap_create(device.c_str(), errbuf);
    if (p == nullptr) {
        result = CaptureResult::failure(CaptureError::other, std::string("pcap_create failed: ") + errbuf);
        return nullptr;
    }

    const auto fail = [&](CaptureError error, std::string message) -> std::unique_ptr<PcapSource> {
        result = CaptureResult::failure(error, std::move(message));
        pcap_close(p);
        return nullptr;
    };

    if (pcap_set_snaplen(p, kSnapLength) != 0)            return fail(CaptureError::other, "pcap_set_snaplen failed");
    if (pcap_set_timeout(p, kReadTimeoutMs) != 0)         return fail(CaptureError::other, "pcap_set_timeout failed");
    if (pcap_set_promisc(p, 1) != 0)                      return fail(CaptureError::other, "pcap_set_promisc failed");
    if (pcap_set_buffer_size(p, kKernelBufferBytes) != 0) return fail(CaptureError::other, "pcap_set_buffer_size failed");

    const int status = pcap_activate(p);
    if (status < 0) {
        return fail(error_kind(status), "can't open device \"" + device + "\": " + pcap_geterr(p));
    }
    if (status > 0) {
        result.message = "warning on device \"" + device + "\": " + pcap_geterr(p);
    }

    // Only filters that refer to the netmask (e.g. "ip broadcast") need it.
    bpf_u_int32 net = 0;
    bpf_u_int32 netmask = 0;
    if (pcap_lookupnet(device.c_str(), &net, &netmask, errbuf) == -1) netmask = PCAP_NETMASK_UNKNOWN;

    return std::unique_ptr<PcapSource>(new PcapSource(p, device, false, netmask));
}

std::unique_ptr<PcapSource> PcapSource::open_offline(const std::string& path, CaptureResult& result) {
    result = {};
    char errbuf[PCAP_ERRBUF_SIZE] = {};

    pcap_t* p = pcap_open_offline(path.c_str(), errbuf);
    if (p == nullptr) {
        result = CaptureResult::failure(CaptureError::other, "can't open file \"" + path + "\": " + errbuf);
        return nullptr;
    }
    return std::unique_ptr<PcapSource>(new PcapSource(p, path, true, PCAP_NETMASK_UNKNOWN));
}

PcapSource::PcapSource(pcap_t* handle, std::string name, bool offline, bpf_u_int32 netmask)
    : handle_(handle), name_(std::move(name)), offline_(offline), netmask_(netmask) {}

PcapSource::~PcapSource() {
    pcap_close(handle_);
}

CaptureResult PcapSource::set_filter(const std::string& expression) {
    bpf_program program{};
    CaptureResult result = compile(handle_, expression, netmask_, program);
    if (!result.ok()) return result;

    const int status = pcap_setfilter(handle_, &program);
    pcap_freecode(&program);
    if (status == -1) {
        return CaptureResult::failure(CaptureError::bad_filter, "can't set filter \"" + expression + "\": " + pcap_geterr(handle_));
    }
    filter_ = expression;
    return {};
}

bool PcapSource::dropped_packets(std::uint32_t& dropped) {
    if (offline_) return false;
    pcap_stat stats{};
    if (pcap_stats(handle_, &stats) != 0) return false;
    dropped = stats.ps_drop;
    return true;
}

CaptureResult check_filter(const std::string& expression) {
    pcap_t* dead = pcap_open_dead(DLT_EN10MB, kSnapLength);
    if (dead == nullptr) return CaptureResult::failure(CaptureError::other, "pcap_open_dead failed");

    // Any known netmask will do for a syntax check; PCAP_NETMASK_UNKNOWN
    // would reject valid filters such as "ip broadcast".
    bpf_program program{};
    CaptureResult result = compile(dead, expression, 0, program);
    if (result.ok()) pcap_freecode(&program);
    pcap_close(dead);
    return result;
}

} // namespace raysniff
