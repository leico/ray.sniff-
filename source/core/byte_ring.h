// Lock-free single-producer / single-consumer byte FIFO (design.md 4-③).

#pragma once

#include "locked_memory.h"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>

namespace raysniff {

// Threads:
//   producer (capture thread)  try_write()
//   consumer (audio thread)    begin_read(), commit_read()
//   any thread                 request_clear(), unread_bytes(), capacity(), locked()
//
// produced_ and consumed_ are running totals: they only increase and are not
// wrapped at capacity_. The number of unread bytes is always
// produced_ - consumed_, so "empty" (0) and "full" (capacity_) never look the
// same. A buffer index is obtained with `total & index_mask_`.
class ByteRing {
public:
    // capacity must be a non-zero power of two. Returns nullptr otherwise, or
    // if the memory cannot be allocated.
    static std::unique_ptr<ByteRing> create(std::size_t capacity);

    ByteRing(const ByteRing&) = delete;
    ByteRing& operator=(const ByteRing&) = delete;

    // ---- producer ------------------------------------------------------------

    // Writes all n bytes, or nothing when there is not enough free space.
    bool try_write(const std::uint8_t* data, std::size_t n);

    // ---- consumer ------------------------------------------------------------

    // Unread bytes at the time of begin_read().
    class ReadView {
    public:
        std::uint64_t size() const { return size_; }
        // Running total of the first byte in the view (its position in the stream).
        std::uint64_t position() const { return start_; }
        // i must be less than size().
        std::uint8_t operator[](std::uint64_t i) const { return ring_->buffer_[(start_ + i) & ring_->index_mask_]; }

    private:
        friend class ByteRing;
        ReadView(const ByteRing* ring, std::uint64_t start, std::uint64_t size)
            : ring_(ring), start_(start), size_(size) {}

        // A nested class may read the private members of ByteRing.
        const ByteRing* ring_;
        std::uint64_t start_;  // consumed_ at begin_read()
        std::uint64_t size_;   // produced_ - consumed_ at begin_read()
    };

    // Applies a pending clear, then returns the unread bytes.
    // Never blocks, allocates or frees memory.
    ReadView begin_read();

    // Removes the first n bytes of the view from the queue. n <= view.size().
    void commit_read(const ReadView& view, std::uint64_t n);

    // ---- any thread ----------------------------------------------------------

    // The consumer discards all unread bytes at its next begin_read().
    // Only the consumer writes consumed_, so clear is handed over as a request.
    void request_clear() { clear_requested_.store(true, std::memory_order_release); }

    // A pending clear is not reflected until the consumer applies it.
    std::uint64_t unread_bytes() const;

    std::size_t capacity() const { return capacity_; }
    bool locked() const { return memory_->locked(); }
    int lock_errno() const { return memory_->lock_errno(); }

private:
    explicit ByteRing(std::unique_ptr<LockedMemory> memory);

    // Apple Silicon uses 128-byte cache lines (Intel: 64). Keeping the two
    // positions on separate lines avoids false sharing between the threads.
    static constexpr std::size_t kCacheLine = 128;

    const std::unique_ptr<LockedMemory> memory_;
    std::uint8_t* const buffer_;
    const std::size_t capacity_;
    const std::uint64_t index_mask_;  // capacity_ - 1, if capacity_ == 16 then 15, (0000 1111)b

    alignas(kCacheLine) std::atomic<std::uint64_t> produced_{0};  // written by the producer only
    alignas(kCacheLine) std::atomic<std::uint64_t> consumed_{0};  // written by the consumer only
    alignas(kCacheLine) std::atomic<bool> clear_requested_{false};

    static_assert(std::atomic<std::uint64_t>::is_always_lock_free);
    static_assert(std::atomic<bool>::is_always_lock_free);
};

// The consumer side is inline: it runs on the audio thread for every block.

inline ByteRing::ReadView ByteRing::begin_read() {
    // Take the clear request before loading produced_, so that a clear discards
    // everything written up to the moment the request is seen.
    const bool clear = clear_requested_.exchange(false, std::memory_order_acquire);
    const std::uint64_t produced = produced_.load(std::memory_order_acquire);
    std::uint64_t consumed = consumed_.load(std::memory_order_relaxed);

    if (clear) {
        consumed = produced;
        consumed_.store(consumed, std::memory_order_release);
    }
    return ReadView(this, consumed, produced - consumed);
}

inline void ByteRing::commit_read(const ReadView& view, std::uint64_t n) {
    // release: the producer must not overwrite these bytes before we have read them.
    consumed_.store(view.start_ + n, std::memory_order_release);
}

} // namespace raysniff
