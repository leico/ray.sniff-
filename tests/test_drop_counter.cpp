#include "drop_counter.h"

#include <catch2/catch_test_macros.hpp>

using raysniff::PcapDropCounter;

TEST_CASE("drop counter: adds the difference between readings", "[drops]") {
    PcapDropCounter drops;
    drops.rebase(10);
    drops.update(15);
    drops.update(15);
    drops.update(22);
    CHECK(drops.snapshot() == 12);
}

TEST_CASE("drop counter: survives the 32-bit counter wrapping around", "[drops]") {
    PcapDropCounter drops;
    drops.rebase(0xFFFFFFF0u);
    drops.update(0x00000010u);  // 0x10 past the wrap
    CHECK(drops.snapshot() == 0x20);
}

TEST_CASE("drop counter: rebase keeps the total when the handle is reopened", "[drops]") {
    PcapDropCounter drops;
    drops.rebase(0);
    drops.update(5);

    drops.rebase(0);  // new handle: the kernel counter starts again
    drops.update(3);
    CHECK(drops.snapshot() == 8);

    drops.rebase(100);  // filter change that did not reset the kernel counter
    drops.update(101);
    CHECK(drops.snapshot() == 9);
}

TEST_CASE("drop counter: reset starts the snapshot from 0 and keeps the total", "[drops]") {
    PcapDropCounter drops;
    drops.rebase(0);
    drops.update(5);
    CHECK(drops.snapshot() == 5);

    drops.reset();
    CHECK(drops.snapshot() == 0);

    drops.update(8);
    CHECK(drops.snapshot() == 3);  // only 5 -> 8; it would be 8 if reset() had zeroed the total
}
