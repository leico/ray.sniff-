#include "sample_reader.h"

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <vector>

using Catch::Approx;
using raysniff::ByteRing;
using raysniff::bytes_per_sample;
using raysniff::read_samples;

namespace {

void write(ByteRing& ring, std::initializer_list<int> values) {
    std::vector<std::uint8_t> v;
    for (int x : values) v.push_back(static_cast<std::uint8_t>(x));
    REQUIRE(ring.try_write(v.data(), v.size()));
}

} // namespace

TEST_CASE("sample reader: bit depths", "[samples]") {
    CHECK(bytes_per_sample(8) == 1);
    CHECK(bytes_per_sample(16) == 2);
    CHECK(bytes_per_sample(24) == 3);
    CHECK(bytes_per_sample(32) == 4);
    CHECK(bytes_per_sample(0) == 0);
    CHECK(bytes_per_sample(12) == 0);
}

TEST_CASE("sample reader: 8 bit values", "[samples]") {
    auto ring = ByteRing::create(16);
    write(*ring, {0x00, 0x80, 0x01, 0xFF});
    double out[4];

    CHECK(read_samples(*ring, 1, out, 4) == 4);
    CHECK(out[0] == 0.0);                       // 0 stays 0
    CHECK(out[1] == 0.0);                       // peak is the middle
    CHECK(out[2] == Approx(127.0 / 128.0));     // -(1 - 128) / 128
    CHECK(out[3] == Approx(-127.0 / 128.0));    // -(255 - 128) / 128
}

TEST_CASE("sample reader: bytes are little-endian", "[samples]") {
    auto ring = ByteRing::create(16);
    write(*ring, {0x01, 0x00, 0x00, 0x80});
    double out[2];

    CHECK(read_samples(*ring, 2, out, 2) == 2);
    CHECK(out[0] == Approx(32767.0 / 32768.0));  // value 0x0001
    CHECK(out[1] == 0.0);                        // value 0x8000 = peak
}

TEST_CASE("sample reader: 24 and 32 bit", "[samples]") {
    auto ring = ByteRing::create(16);
    double out[1];

    write(*ring, {0x00, 0x00, 0xC0});  // 0xC00000
    CHECK(read_samples(*ring, 3, out, 1) == 1);
    CHECK(out[0] == Approx(-0.5));

    write(*ring, {0xFF, 0xFF, 0xFF, 0xFF});  // 0xFFFFFFFF
    CHECK(read_samples(*ring, 4, out, 1) == 1);
    CHECK(out[0] == Approx(-(4294967295.0 - 2147483648.0) / 2147483648.0));
}

TEST_CASE("sample reader: an empty queue gives silence", "[samples]") {
    auto ring = ByteRing::create(16);
    double out[3] = {1.0, 1.0, 1.0};

    CHECK(read_samples(*ring, 2, out, 3) == 0);
    CHECK(out[0] == 0.0);
    CHECK(out[1] == 0.0);
    CHECK(out[2] == 0.0);
}

TEST_CASE("sample reader: a remainder shorter than one sample waits for the next call", "[samples]") {
    auto ring = ByteRing::create(16);
    write(*ring, {0x01, 0x00, 0x01});  // one 16-bit sample and one byte
    double out[3] = {1.0, 1.0, 1.0};

    CHECK(read_samples(*ring, 2, out, 3) == 1);
    CHECK(out[0] == Approx(32767.0 / 32768.0));
    CHECK(out[1] == 0.0);
    CHECK(out[2] == 0.0);
    CHECK(ring->unread_bytes() == 1);  // the 0x01 is kept

    write(*ring, {0x00});              // completes the sample 0x0001
    CHECK(read_samples(*ring, 2, out, 1) == 1);
    CHECK(out[0] == Approx(32767.0 / 32768.0));
    CHECK(ring->unread_bytes() == 0);
}

TEST_CASE("sample reader: reads no more than the block", "[samples]") {
    auto ring = ByteRing::create(16);
    write(*ring, {0x01, 0x02, 0x03, 0x04});
    double out[2];

    CHECK(read_samples(*ring, 1, out, 2) == 2);
    CHECK(ring->unread_bytes() == 2);
}

TEST_CASE("sample reader: a pending clear is applied first", "[samples]") {
    auto ring = ByteRing::create(16);
    write(*ring, {0x01, 0x02});
    ring->request_clear();
    double out[2] = {1.0, 1.0};

    CHECK(read_samples(*ring, 1, out, 2) == 0);
    CHECK(out[0] == 0.0);
    CHECK(out[1] == 0.0);
    CHECK(ring->unread_bytes() == 0);
}
