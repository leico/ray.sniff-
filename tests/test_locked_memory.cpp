#include "locked_memory.h"

#include <catch2/catch_test_macros.hpp>

#include <cstring>

using raysniff::LockedMemory;

TEST_CASE("locked memory: allocates writable, page-aligned memory", "[memory]") {
    constexpr std::size_t kSize = 1024 * 1024;
    auto memory = LockedMemory::allocate(kSize);
    REQUIRE(memory);
    CHECK(memory->size() == kSize);
    CHECK(reinterpret_cast<std::uintptr_t>(memory->data()) % 4096 == 0);

    std::memset(memory->data(), 0xAB, kSize);
    CHECK(memory->data()[0] == 0xAB);
    CHECK(memory->data()[kSize - 1] == 0xAB);
}

TEST_CASE("locked memory: is locked when the system allows it", "[memory]") {
    auto memory = LockedMemory::allocate(1024 * 1024);
    REQUIRE(memory);
    if (memory->locked()) {
        CHECK(memory->lock_errno() == 0);
    } else {
        // Not a failure by design (keep running without the lock), but worth seeing.
        WARN("mlock failed, errno " << memory->lock_errno());
        CHECK(memory->lock_errno() != 0);
    }
}

TEST_CASE("locked memory: size 0 is rejected", "[memory]") {
    CHECK_FALSE(LockedMemory::allocate(0));
}
