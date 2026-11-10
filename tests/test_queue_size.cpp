#include "packet_queue.h"

#include <catch2/catch_test_macros.hpp>

using raysniff::normalize_queue_size_kb;

TEST_CASE("queue size: powers of two in range are kept", "[queuesize]") {
    CHECK(normalize_queue_size_kb(128) == 128);
    CHECK(normalize_queue_size_kb(1024) == 1024);
    CHECK(normalize_queue_size_kb(1048576) == 1048576);
}

TEST_CASE("queue size: other values are rounded up to a power of two", "[queuesize]") {
    CHECK(normalize_queue_size_kb(129) == 256);
    CHECK(normalize_queue_size_kb(1000) == 1024);
    CHECK(normalize_queue_size_kb(1025) == 2048);
    CHECK(normalize_queue_size_kb(524289) == 1048576);
}

TEST_CASE("queue size: out of range values are clamped", "[queuesize]") {
    CHECK(normalize_queue_size_kb(-1) == 128);
    CHECK(normalize_queue_size_kb(0) == 128);
    CHECK(normalize_queue_size_kb(64) == 128);
    CHECK(normalize_queue_size_kb(2000000) == 1048576);
}

TEST_CASE("queue size: default is 1 MB", "[queuesize]") {
    CHECK(raysniff::kQueueSizeDefaultKb == 1024);
    CHECK(normalize_queue_size_kb(raysniff::kQueueSizeDefaultKb) == raysniff::kQueueSizeDefaultKb);
}
