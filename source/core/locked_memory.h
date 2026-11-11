// Page-aligned memory pinned to physical memory (design.md 4-③).

#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>

namespace raysniff {

// Anonymous memory from mmap(), locked with mlock() so that touching it never
// causes a page fault, even under memory pressure. Failing to lock is not an
// error (design decision: keep running, report it): check locked().
class LockedMemory {
public:
    // Returns nullptr if size is 0 or the memory cannot be mapped.
    static std::unique_ptr<LockedMemory> allocate(std::size_t size);

    ~LockedMemory();
    LockedMemory(const LockedMemory&) = delete;
    LockedMemory& operator=(const LockedMemory&) = delete;

    std::uint8_t* data() const { return data_; }
    std::size_t size() const { return size_; }
    bool locked() const { return locked_; }
    int lock_errno() const { return lock_errno_; }  // errno from mlock(), 0 if locked

private:
    LockedMemory(std::uint8_t* data, std::size_t size, bool locked, int lock_errno);

    std::uint8_t* const data_;
    const std::size_t size_;
    const bool locked_;
    const int lock_errno_;
};

} // namespace raysniff
