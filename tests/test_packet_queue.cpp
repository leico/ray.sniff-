#include "packet_queue.h"

#include <catch2/catch_test_macros.hpp>

#include <vector>

using raysniff::PacketQueue;

TEST_CASE("packet queue: written and dropped packets are counted", "[queue]") {
    auto queue = PacketQueue::create(8);
    REQUIRE(queue);

    const std::vector<std::uint8_t> five(5, 1);
    const std::vector<std::uint8_t> four(4, 2);
    queue->push(five.data(), five.size());  // fits
    queue->push(four.data(), four.size());  // does not fit: dropped as a whole

    const auto s = queue->stats().snapshot();
    CHECK(s.written_packets == 1);
    CHECK(s.written_bytes == 5);
    CHECK(s.dropped_packets == 1);
    CHECK(s.dropped_bytes == 4);
    CHECK(queue->ring().unread_bytes() == 5);
}

TEST_CASE("packet queue: resetstats does not touch the queue contents", "[queue]") {
    auto queue = PacketQueue::create(16);
    const std::vector<std::uint8_t> packet(6, 3);
    queue->push(packet.data(), packet.size());

    queue->stats().reset();
    const auto s = queue->stats().snapshot();
    CHECK(s.written_packets == 0);
    CHECK(s.written_bytes == 0);
    CHECK(queue->ring().unread_bytes() == 6);  // queue size is not a counter

    queue->push(packet.data(), packet.size());
    CHECK(queue->stats().snapshot().written_packets == 1);
}

TEST_CASE("packet queue: clear does not reset the stats", "[queue]") {
    auto queue = PacketQueue::create(16);
    const std::vector<std::uint8_t> packet(6, 3);
    queue->push(packet.data(), packet.size());

    queue->ring().request_clear();
    queue->ring().begin_read();
    CHECK(queue->ring().unread_bytes() == 0);
    CHECK(queue->stats().snapshot().written_packets == 1);
}

TEST_CASE("packet queue: resize replaces the ring and keeps the stats", "[queue]") {
    auto queue = PacketQueue::create(16);
    const std::vector<std::uint8_t> packet(6, 3);
    queue->push(packet.data(), packet.size());

    REQUIRE(queue->resize(64));
    CHECK(queue->ring().capacity() == 64);
    CHECK(queue->ring().unread_bytes() == 0);
    CHECK(queue->stats().snapshot().written_packets == 1);
}

TEST_CASE("packet queue: a failed resize keeps the current ring", "[queue]") {
    auto queue = PacketQueue::create(16);
    const std::vector<std::uint8_t> packet(6, 3);
    queue->push(packet.data(), packet.size());

    CHECK_FALSE(queue->resize(100));  // not a power of two
    CHECK(queue->ring().capacity() == 16);
    CHECK(queue->ring().unread_bytes() == 6);
}
