#include "byte_ring.h"

#include <algorithm>
#include <cstring>

namespace raysniff {

std::unique_ptr<ByteRing> ByteRing::create(std::size_t capacity) {
    const bool power_of_two = capacity != 0 && (capacity & (capacity - 1)) == 0;
    if (!power_of_two) return nullptr;

    auto memory = LockedMemory::allocate(capacity);
    if (!memory) return nullptr;
    return std::unique_ptr<ByteRing>(new ByteRing(std::move(memory)));
}

ByteRing::ByteRing(std::unique_ptr<LockedMemory> memory)
    : memory_(std::move(memory)),
      buffer_(memory_->data()),
      capacity_(memory_->size()),
      index_mask_(memory_->size() - 1) {}

bool ByteRing::try_write(const std::uint8_t* data, std::size_t n) {
    if (n == 0) return true;

    const std::uint64_t produced = produced_.load(std::memory_order_relaxed);  // only we write it
    // acquire: the consumer has finished reading the bytes it gave back.
    const std::uint64_t consumed = consumed_.load(std::memory_order_acquire);

    const std::uint64_t free_bytes = capacity_ - (produced - consumed); // capacity_ - unread
    if (n > free_bytes) return false;

    // Copy in at most two parts when the data wraps around the end.
    const std::size_t write_start   = static_cast<std::size_t>(produced & index_mask_);
    const std::size_t first_length  = std::min(n, capacity_ - write_start); // up to the end of the buffer
    const std::size_t second_length = n - first_length;                     // from the start of the buffer (0 if no wrap)
    std::memcpy(buffer_ + write_start, data, first_length);
    std::memcpy(buffer_, data + first_length, second_length);

    // release: the bytes become visible to the consumer together with produced_.
    produced_.store(produced + n, std::memory_order_release);
    return true;
}

std::uint64_t ByteRing::unread_bytes() const {
    // Load consumed_ first: produced_ can only have grown since, so the
    // difference never goes negative.
    const std::uint64_t consumed = consumed_.load(std::memory_order_acquire);
    const std::uint64_t produced = produced_.load(std::memory_order_acquire);
    return produced - consumed;
}

} // namespace raysniff
