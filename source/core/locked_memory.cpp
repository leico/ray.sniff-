#include "locked_memory.h"

#include <sys/mman.h>

#include <cerrno>
#include <new>

namespace raysniff {

std::unique_ptr<LockedMemory> LockedMemory::allocate(std::size_t size) {
    if (size == 0) return nullptr;

    void* p = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);
    if (p == MAP_FAILED) return nullptr;

    // mlock() also faults in every page, so no page fault happens later.
    const bool locked = (mlock(p, size) == 0);
    const int lock_errno = locked ? 0 : errno;

    auto* memory = new (std::nothrow) LockedMemory(static_cast<std::uint8_t*>(p), size, locked, lock_errno);
    if (memory == nullptr) {
        if (locked) munlock(p, size);
        munmap(p, size);
        return nullptr;
    }
    return std::unique_ptr<LockedMemory>(memory);
}

LockedMemory::LockedMemory(std::uint8_t* data, std::size_t size, bool locked, int lock_errno)
    : data_(data), size_(size), locked_(locked), lock_errno_(lock_errno) {}

LockedMemory::~LockedMemory() {
    if (locked_) munlock(data_, size_);
    munmap(data_, size_);
}

} // namespace raysniff
