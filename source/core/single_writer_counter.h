// A 64-bit counter written by one thread and read by any (docs/atomic.md §3).

#pragma once

#include <atomic>
#include <cstdint>

namespace raysniff {

// Only one thread may call add(). With a single writer, nothing can be
// written between the load and the store, so a plain load + store is enough
// and no read-modify-write (fetch_add) is needed. The counter only counts:
// it hands over no other data, so relaxed ordering is enough.
//
// Do not use this for a counter that more than one thread writes.
class SingleWriterCounter {
public:
    // The single writer only.
    void add(std::uint64_t n) {
        const std::uint64_t current = value_.load(std::memory_order_relaxed);
        value_.store(current + n, std::memory_order_relaxed);
    }

    // Any thread.
    std::uint64_t load() const { return value_.load(std::memory_order_relaxed); }

private:
    std::atomic<std::uint64_t> value_{0};

    static_assert(std::atomic<std::uint64_t>::is_always_lock_free);
};

} // namespace raysniff
