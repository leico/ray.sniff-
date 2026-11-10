#include "byte_ring.h"

#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <thread>
#include <vector>

using raysniff::ByteRing;

namespace {

std::vector<std::uint8_t> bytes(std::initializer_list<int> values) {
    std::vector<std::uint8_t> v;
    for (int x : values) v.push_back(static_cast<std::uint8_t>(x));
    return v;
}

std::vector<std::uint8_t> read_all(ByteRing& ring) {
    auto view = ring.begin_read();
    std::vector<std::uint8_t> out;
    for (std::uint64_t i = 0; i < view.size(); ++i) out.push_back(view[i]);
    ring.commit_read(view, view.size());
    return out;
}

// Thread tests are much slower under ThreadSanitizer.
#if defined(__has_feature)
#if __has_feature(thread_sanitizer)
constexpr std::uint64_t kStressBytes = 2'000'000;
#else
constexpr std::uint64_t kStressBytes = 50'000'000;
#endif
#else
constexpr std::uint64_t kStressBytes = 50'000'000;
#endif

} // namespace

TEST_CASE("byte ring: capacity must be a power of two", "[ring]") {
    CHECK(ByteRing::create(4096));
    CHECK_FALSE(ByteRing::create(0));
    CHECK_FALSE(ByteRing::create(3000));
}

TEST_CASE("byte ring: bytes come out in the order they went in", "[ring]") {
    auto ring = ByteRing::create(16);
    REQUIRE(ring);

    const auto a = bytes({1, 2, 3});
    const auto b = bytes({4, 5});
    REQUIRE(ring->try_write(a.data(), a.size()));
    REQUIRE(ring->try_write(b.data(), b.size()));
    CHECK(ring->unread_bytes() == 5);

    CHECK(read_all(*ring) == bytes({1, 2, 3, 4, 5}));
    CHECK(ring->unread_bytes() == 0);
}

TEST_CASE("byte ring: partial reads keep the rest", "[ring]") {
    auto ring = ByteRing::create(16);
    const auto a = bytes({1, 2, 3, 4});
    REQUIRE(ring->try_write(a.data(), a.size()));

    auto view = ring->begin_read();
    REQUIRE(view.size() == 4);
    CHECK(view.position() == 0);
    ring->commit_read(view, 1);

    view = ring->begin_read();
    CHECK(view.position() == 1);
    CHECK(view.size() == 3);
    CHECK(view[0] == 2);
}

TEST_CASE("byte ring: data wraps around the end of the buffer", "[ring]") {
    auto ring = ByteRing::create(8);
    const auto first = bytes({1, 2, 3, 4, 5, 6});
    REQUIRE(ring->try_write(first.data(), first.size()));
    read_all(*ring);  // positions are now 6, the next write wraps

    const auto second = bytes({7, 8, 9, 10, 11});
    REQUIRE(ring->try_write(second.data(), second.size()));
    CHECK(read_all(*ring) == second);
}

TEST_CASE("byte ring: the whole capacity can be used", "[ring]") {
    auto ring = ByteRing::create(8);
    const std::vector<std::uint8_t> full(8, 0x42);
    CHECK(ring->try_write(full.data(), full.size()));
    CHECK(ring->unread_bytes() == 8);
}

TEST_CASE("byte ring: a write that does not fit is rejected as a whole", "[ring]") {
    auto ring = ByteRing::create(8);
    const auto five = bytes({1, 2, 3, 4, 5});
    REQUIRE(ring->try_write(five.data(), five.size()));

    const auto four = bytes({6, 7, 8, 9});
    CHECK_FALSE(ring->try_write(four.data(), four.size()));
    CHECK(ring->unread_bytes() == 5);
    CHECK(read_all(*ring) == five);  // nothing of the rejected write is visible
}

TEST_CASE("byte ring: clear is applied by the consumer", "[ring]") {
    auto ring = ByteRing::create(16);
    const auto a = bytes({1, 2, 3});
    REQUIRE(ring->try_write(a.data(), a.size()));

    ring->request_clear();
    CHECK(ring->unread_bytes() == 3);  // not applied yet

    auto view = ring->begin_read();    // the consumer applies it here
    CHECK(view.size() == 0);
    CHECK(ring->unread_bytes() == 0);

    const auto b = bytes({4, 5});
    REQUIRE(ring->try_write(b.data(), b.size()));
    CHECK(read_all(*ring) == b);       // later data is kept
}

TEST_CASE("byte ring: zero-length writes succeed and do nothing", "[ring]") {
    auto ring = ByteRing::create(8);
    CHECK(ring->try_write(nullptr, 0));
    CHECK(ring->unread_bytes() == 0);
}

// ---------------------------------------------------------------------------
// Concurrency. Run these under ThreadSanitizer (design.md 8).
//
// The producer writes the byte `position & 0xFF` at every stream position.
// Because clear only moves the read position, the byte at any position the
// consumer sees must still equal `position & 0xFF`, with or without clears.
// ---------------------------------------------------------------------------

namespace {

// Writes at least kStressBytes (the last chunk may go past it) and returns the
// exact number of bytes written.
void produce(ByteRing& ring, std::atomic<bool>& done, std::uint64_t& total_written) {
    std::uint8_t chunk[1514];
    std::uint64_t written = 0;  // equals produced_, since we are the only producer
    std::uint32_t random = 1;
    while (written < kStressBytes) {
        random = random * 1664525u + 1013904223u;
        const std::size_t n = 1 + (random >> 8) % sizeof chunk;
        for (std::size_t k = 0; k < n; ++k) chunk[k] = static_cast<std::uint8_t>((written + k) & 0xFF);
        if (ring.try_write(chunk, n)) {
            written += n;
        } else {
            std::this_thread::yield();
        }
    }
    total_written = written;  // read by the main thread after join()
    done.store(true, std::memory_order_release);
}

struct ConsumeResult {
    std::uint64_t read = 0;
    std::uint64_t mismatches = 0;
    std::uint64_t last_position = 0;
};

ConsumeResult consume(ByteRing& ring, const std::atomic<bool>& done) {
    ConsumeResult result;
    for (;;) {
        const bool finished = done.load(std::memory_order_acquire);
        auto view = ring.begin_read();
        // Read in small steps, like an audio thread does.
        const std::uint64_t n = view.size() < 512 ? view.size() : 512;
        for (std::uint64_t i = 0; i < n; ++i) {
            if (view[i] != static_cast<std::uint8_t>((view.position() + i) & 0xFF)) ++result.mismatches;
        }
        ring.commit_read(view, n);
        result.read += n;
        result.last_position = view.position() + n;
        if (finished && view.size() == n) break;  // everything written has been seen
    }
    return result;
}

} // namespace

TEST_CASE("byte ring: concurrent producer and consumer keep every byte in order", "[ring][thread]") {
    auto ring = ByteRing::create(4096);  // small, so the data wraps very often
    std::atomic<bool> done{false};
    std::uint64_t written = 0;

    std::thread producer(produce, std::ref(*ring), std::ref(done), std::ref(written));
    const ConsumeResult result = consume(*ring, done);
    producer.join();

    CHECK(result.mismatches == 0);
    CHECK(written >= kStressBytes);
    CHECK(result.read == written);
}

TEST_CASE("byte ring: concurrent clear requests never corrupt the data", "[ring][thread]") {
    auto ring = ByteRing::create(4096);
    std::atomic<bool> done{false};
    std::uint64_t written = 0;

    std::thread producer(produce, std::ref(*ring), std::ref(done), std::ref(written));
    std::thread clearer([&] {
        while (!done.load(std::memory_order_acquire)) {
            ring->request_clear();
            std::this_thread::sleep_for(std::chrono::microseconds(200));
        }
    });
    const ConsumeResult result = consume(*ring, done);
    producer.join();
    clearer.join();

    CHECK(result.mismatches == 0);
    CHECK(result.read <= written);
    CHECK(result.last_position == written);  // the reader still reaches the end of the stream
}
