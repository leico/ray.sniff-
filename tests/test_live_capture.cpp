// Captures from a real device (lo0). These need access to /dev/bpf*, so they
// are skipped unless the tests run as root or with ChmodBPF installed:
//
//   sudo ./build-test/tests/raysniff_tests "[live]"

#include "capture_session.h"
#include "pcap_source.h"

#include "read_all.h"

#include <catch2/catch_test_macros.hpp>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <string>
#include <thread>
#include <vector>

using raysniff::CaptureError;
using raysniff::CaptureResult;
using raysniff::CaptureSession;
using raysniff::CaptureState;
using raysniff::PacketQueue;
using raysniff::PcapSource;
using raysniff_test::read_all;

namespace {

constexpr const char* kLoopback = "lo0";

// Skips the current test when this environment can't open devices.
void require_capture_permission() {
    CaptureResult result;
    auto source = PcapSource::open_live(kLoopback, result);
    if (!source && result.error == CaptureError::permission_denied) {
        SKIP("no access to /dev/bpf*: run as root or install ChmodBPF");
    }
    REQUIRE(source);
}

// A UDP socket bound to 127.0.0.1 on a free port. Packets sent to it are
// delivered (no ICMP "port unreachable"), and nothing else uses the port.
class LoopbackUdpSocket {
public:
    LoopbackUdpSocket() {
        fd_ = socket(AF_INET, SOCK_DGRAM, 0);
        REQUIRE(fd_ >= 0);
        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        addr.sin_port = 0;  // let the kernel choose
        REQUIRE(bind(fd_, reinterpret_cast<sockaddr*>(&addr), sizeof addr) == 0);
        socklen_t length = sizeof addr;
        REQUIRE(getsockname(fd_, reinterpret_cast<sockaddr*>(&addr), &length) == 0);
        port_ = ntohs(addr.sin_port);
    }
    ~LoopbackUdpSocket() { close(fd_); }
    LoopbackUdpSocket(const LoopbackUdpSocket&) = delete;
    LoopbackUdpSocket& operator=(const LoopbackUdpSocket&) = delete;

    std::uint16_t port() const { return port_; }

    // Sends `payload` to itself.
    void send_to_self(const std::string& payload) const {
        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        addr.sin_port = htons(port_);
        const ssize_t sent = sendto(fd_, payload.data(), payload.size(), 0,
                                    reinterpret_cast<const sockaddr*>(&addr), sizeof addr);
        REQUIRE(sent == static_cast<ssize_t>(payload.size()));
    }

private:
    int fd_ = -1;
    std::uint16_t port_ = 0;
};

bool contains(const std::vector<std::uint8_t>& haystack, const std::string& needle) {
    return std::search(haystack.begin(), haystack.end(), needle.begin(), needle.end()) != haystack.end();
}

} // namespace

TEST_CASE("live capture: a packet sent over lo0 ends up in the queue", "[live]") {
    require_capture_permission();

    LoopbackUdpSocket socket;
    auto queue = PacketQueue::create(1 << 20);
    CaptureSession session(*queue);
    REQUIRE(session.set_filter("udp port " + std::to_string(socket.port())).ok());
    REQUIRE(session.open_live(kLoopback).ok());
    REQUIRE(session.state() == CaptureState::running);

    const std::string payload = "ray.sniff~ live capture test";
    socket.send_to_self(payload);

    // The capture thread delivers it within a few read timeouts.
    std::vector<std::uint8_t> captured;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (!contains(captured, payload) && std::chrono::steady_clock::now() < deadline) {
        const auto more = read_all(*queue);
        captured.insert(captured.end(), more.begin(), more.end());
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    CHECK(contains(captured, payload));
    CHECK(queue->stats().snapshot().written_packets >= 1);
    CHECK(session.state() == CaptureState::running);
}

TEST_CASE("live capture: close returns quickly even when no packet arrives", "[live]") {
    // stop() relies on the 1 ms read timeout of BPF (design.md 5). If the read
    // did not time out on a quiet device, close() would hang here.
    require_capture_permission();

    LoopbackUdpSocket socket;  // its port is ours, so nothing matches the filter
    auto queue = PacketQueue::create(1 << 20);
    CaptureSession session(*queue);
    REQUIRE(session.set_filter("udp port " + std::to_string(socket.port())).ok());

    for (int round = 0; round < 5; ++round) {
        REQUIRE(session.open_live(kLoopback).ok());
        // Let the thread settle inside pcap_next_ex.
        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        const auto start = std::chrono::steady_clock::now();
        session.close();
        const auto elapsed = std::chrono::steady_clock::now() - start;

        CHECK(elapsed < std::chrono::milliseconds(100));
        CHECK(session.state() == CaptureState::stopped);
    }
    CHECK(queue->stats().snapshot().written_packets == 0);
}

TEST_CASE("live capture: a device provides pcap statistics", "[live]") {
    require_capture_permission();

    CaptureResult result;
    auto source = PcapSource::open_live(kLoopback, result);
    REQUIRE(source);
    CHECK_FALSE(source->offline());

    std::uint32_t dropped = 0xFFFFFFFFu;
    CHECK(source->dropped_packets(dropped));
    CHECK(dropped == 0);  // a handle that has not read anything has dropped nothing
}
