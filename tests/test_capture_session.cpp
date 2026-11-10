#include "capture_session.h"

#include "pcap_file.h"
#include "read_all.h"

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <thread>

using raysniff::CaptureError;
using raysniff::CaptureSession;
using raysniff::CaptureState;
using raysniff::PacketQueue;
using namespace raysniff_test;

namespace {

// Waits for the capture thread to reach the end of the file.
bool wait_until_finished(const CaptureSession& session) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (session.state() == CaptureState::running) {
        if (std::chrono::steady_clock::now() > deadline) return false;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    return session.state() == CaptureState::finished;
}

} // namespace

TEST_CASE("capture session: packets of a file end up in the queue", "[capture]") {
    const std::vector<Packet> packets{udp_packet(1, 10), tcp_packet(2, 20), udp_packet(3, 30)};
    const TempPcapFile file(packets);
    auto queue = PacketQueue::create(4096);
    CaptureSession session(*queue);

    REQUIRE(session.open_offline(file.path()).ok());
    CHECK(session.is_open());
    CHECK(session.source_name() == file.path());
    REQUIRE(wait_until_finished(session));

    CHECK(read_all(*queue) == concat(packets));
    CHECK(queue->stats().snapshot().written_packets == 3);
    CHECK(session.dropped_packets() == 0);  // files have no pcap statistics
}

TEST_CASE("capture session: a filter set while closed is used when opening", "[capture]") {
    const auto udp1 = udp_packet(1, 10);
    const auto tcp = tcp_packet(2, 10);
    const auto udp2 = udp_packet(3, 10);
    const TempPcapFile file({udp1, tcp, udp2});
    auto queue = PacketQueue::create(4096);
    CaptureSession session(*queue);

    REQUIRE(session.set_filter("udp").ok());
    REQUIRE(session.open_offline(file.path()).ok());
    REQUIRE(wait_until_finished(session));

    CHECK(read_all(*queue) == concat({udp1, udp2}));
}

TEST_CASE("capture session: an invalid filter is rejected and the old one stays", "[capture]") {
    auto queue = PacketQueue::create(4096);
    CaptureSession session(*queue);
    REQUIRE(session.set_filter("tcp").ok());

    const auto result = session.set_filter("tcp port");
    CHECK(result.error == CaptureError::bad_filter);
    CHECK(session.filter() == "tcp");
}

TEST_CASE("capture session: changing the filter while open restarts the thread", "[capture]") {
    const TempPcapFile file({udp_packet(1, 10)});
    auto queue = PacketQueue::create(4096);
    CaptureSession session(*queue);
    REQUIRE(session.open_offline(file.path()).ok());
    REQUIRE(wait_until_finished(session));

    REQUIRE(session.set_filter("udp").ok());
    CHECK(session.filter() == "udp");
    CHECK(wait_until_finished(session));  // the file is already at its end

    const auto bad = session.set_filter("udp port");
    CHECK(bad.error == CaptureError::bad_filter);
    CHECK(session.filter() == "udp");
    CHECK(session.is_open());
}

TEST_CASE("capture session: a failed open keeps the current source", "[capture]") {
    const TempPcapFile file({udp_packet(1, 10)});
    auto queue = PacketQueue::create(4096);
    CaptureSession session(*queue);
    REQUIRE(session.open_offline(file.path()).ok());

    const auto result = session.open_offline("/nonexistent/raysniff.pcap");
    CHECK_FALSE(result.ok());
    CHECK(session.is_open());
    CHECK(session.source_name() == file.path());
}

TEST_CASE("capture session: an open that fails on the filter keeps the current source", "[capture]") {
    // "ether host" is valid for Ethernet but not for loopback (DLT_NULL, as
    // on lo0), so the second file opens but its filter cannot be installed.
    const TempPcapFile ethernet({udp_packet(1, 10)});
    const TempPcapFile loopback({Packet(20, 0x55)}, DLT_NULL);
    auto queue = PacketQueue::create(4096);
    CaptureSession session(*queue);

    REQUIRE(session.set_filter("ether host 02:02:02:02:02:02").ok());
    REQUIRE(session.open_offline(ethernet.path()).ok());

    const auto result = session.open_offline(loopback.path());
    CHECK(result.error == CaptureError::bad_filter);
    CHECK(session.source_name() == ethernet.path());
    CHECK(session.filter() == "ether host 02:02:02:02:02:02");
}

TEST_CASE("capture session: opening another file replaces the source", "[capture]") {
    const TempPcapFile first({udp_packet(1, 10)});
    const TempPcapFile second({tcp_packet(2, 10)});
    auto queue = PacketQueue::create(4096);
    CaptureSession session(*queue);

    REQUIRE(session.open_offline(first.path()).ok());
    REQUIRE(wait_until_finished(session));

    REQUIRE(session.open_offline(second.path()).ok());
    CHECK(session.source_name() == second.path());
    REQUIRE(wait_until_finished(session));
    // The queue is not cleared by a change of source (design.md 7.2 @device).
    CHECK(read_all(*queue) == concat({udp_packet(1, 10), tcp_packet(2, 10)}));
}

TEST_CASE("capture session: a full queue drops whole packets", "[capture]") {
    const auto packet = udp_packet(7, 58);  // 100 bytes
    REQUIRE(packet.size() == 100);
    const TempPcapFile file({packet, packet, packet, packet, packet});
    auto queue = PacketQueue::create(256);  // room for 2 packets
    CaptureSession session(*queue);

    REQUIRE(session.open_offline(file.path()).ok());
    REQUIRE(wait_until_finished(session));

    const auto stats = queue->stats().snapshot();
    CHECK(stats.written_packets == 2);
    CHECK(stats.dropped_packets == 3);
    CHECK(stats.dropped_bytes == 300);
    CHECK(read_all(*queue) == concat({packet, packet}));
}

TEST_CASE("capture session: close stops the thread", "[capture]") {
    const TempPcapFile file({udp_packet(1, 10)});
    auto queue = PacketQueue::create(4096);
    CaptureSession session(*queue);
    REQUIRE(session.open_offline(file.path()).ok());

    session.close();
    CHECK_FALSE(session.is_open());
    CHECK(session.state() == CaptureState::stopped);
    CHECK(session.source_name().empty());
}
