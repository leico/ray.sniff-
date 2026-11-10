// Total of pcap's dropped packets (design.md 7.5).

#pragma once

#include "single_writer_counter.h"

#include <cstdint>

namespace raysniff {

// pcap_stats() reports ps_drop as a 32-bit counter of the current handle: it
// wraps around, and it starts again (from 0, or from wherever the kernel
// left it) when the handle is reopened or its filter is changed. This class
// turns those readings into one 64-bit total since the object was created.
//
// Threads: rebase() and update() on the capture thread only (the single
// writer); snapshot() and reset() on the main thread.
class PcapDropCounter {
public:
    // Takes `current` as the new starting point without adding anything.
    // Called whenever the capture thread starts reading a handle, so that a
    // reset (or no reset) of the kernel counter in between does not matter.
    void rebase(std::uint32_t current) { last_ = current; }

    // Adds the drops since the previous reading. Unsigned subtraction gives
    // the right difference even when the 32-bit counter has wrapped.
    void update(std::uint32_t current) {
        const std::uint32_t delta = current - last_;
        last_ = current;
        total_.add(delta);
    }

    // Drops since the last reset() (resetstats, design.md 7.4). Like
    // QueueStats, reset() records a baseline instead of zeroing total_, so the
    // capture thread stays its only writer. Main thread only.
    std::uint64_t snapshot() const { return total_.load() - baseline_; }
    void reset() { baseline_ = total_.load(); }

private:
    std::uint32_t last_ = 0;      // capture thread only
    SingleWriterCounter total_;   // written by the capture thread only
    std::uint64_t baseline_ = 0;  // main thread only
};

} // namespace raysniff
