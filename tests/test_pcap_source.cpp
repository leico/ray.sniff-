#include "pcap_source.h"

#include "pcap_file.h"

#include <catch2/catch_test_macros.hpp>

#include <unistd.h>

using raysniff::CaptureError;
using raysniff::CaptureResult;
using raysniff::PcapSource;
using namespace raysniff_test;

namespace {

int count_packets(PcapSource& source) {
    int count = 0;
    pcap_pkthdr* header = nullptr;
    const u_char* data = nullptr;
    while (pcap_next_ex(source.handle(), &header, &data) == 1) ++count;
    return count;
}

} // namespace

TEST_CASE("pcap source: opens a capture file", "[pcap]") {
    const TempPcapFile file({udp_packet(1, 10), tcp_packet(2, 10)});
    CaptureResult result;
    auto source = PcapSource::open_offline(file.path(), result);
    REQUIRE(source);
    CHECK(result.ok());
    CHECK(source->offline());
    CHECK(source->name() == file.path());
    CHECK(count_packets(*source) == 2);

    std::uint32_t dropped = 0;
    CHECK_FALSE(source->dropped_packets(dropped));  // files have no statistics
}

TEST_CASE("pcap source: a missing file is an error", "[pcap]") {
    CaptureResult result;
    auto source = PcapSource::open_offline("/nonexistent/raysniff.pcap", result);
    CHECK_FALSE(source);
    CHECK(result.error == CaptureError::other);
    CHECK_FALSE(result.message.empty());
}

TEST_CASE("pcap source: the filter selects packets", "[pcap]") {
    const TempPcapFile file({udp_packet(1, 10), tcp_packet(2, 10), udp_packet(3, 10)});
    CaptureResult result;
    auto source = PcapSource::open_offline(file.path(), result);
    REQUIRE(source);

    REQUIRE(source->set_filter("udp").ok());
    CHECK(source->filter() == "udp");
    CHECK(count_packets(*source) == 2);
}

TEST_CASE("pcap source: an invalid filter keeps the current one", "[pcap]") {
    const TempPcapFile file({udp_packet(1, 10), tcp_packet(2, 10)});
    CaptureResult result;
    auto source = PcapSource::open_offline(file.path(), result);
    REQUIRE(source);
    REQUIRE(source->set_filter("tcp").ok());

    const CaptureResult bad = source->set_filter("not a filter (");
    CHECK(bad.error == CaptureError::bad_filter);
    CHECK_FALSE(bad.message.empty());
    CHECK(source->filter() == "tcp");
    CHECK(count_packets(*source) == 1);  // still filtered by "tcp"
}

TEST_CASE("pcap source: filters can be checked without a handle", "[pcap]") {
    CHECK(raysniff::check_filter("").ok());
    CHECK(raysniff::check_filter("udp port 8000").ok());
    CHECK(raysniff::check_filter("ip broadcast").ok());  // needs a netmask
    CHECK(raysniff::check_filter("udp port").error == CaptureError::bad_filter);
}

TEST_CASE("pcap source: a device without permission reports permission_denied", "[pcap][live]") {
    // Only meaningful where /dev/bpf* is not accessible (no ChmodBPF, not root).
    if (access("/dev/bpf0", R_OK) == 0) {
        SKIP("this environment can capture; nothing to check");
    }
    CaptureResult result;
    auto source = PcapSource::open_live("lo0", result);
    CHECK_FALSE(source);
    CHECK(result.error == CaptureError::permission_denied);
}
