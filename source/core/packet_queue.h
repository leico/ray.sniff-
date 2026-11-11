// Packet queue between the capture thread and the audio thread (design.md 4-③, 7.2, 7.5).

#pragma once

#include "byte_ring.h"
#include "single_writer_counter.h"

#include <cstddef>
#include <cstdint>
#include <memory>

namespace raysniff {

// @queuesize, in KB.
constexpr long kQueueSizeMinKb     =  128;        // larger than one packet (max 64 KB)
constexpr long kQueueSizeMaxKb     = 1024 * 1024; // 1 GB
constexpr long kQueueSizeDefaultKb = 1024;        // 1 MB

// Clamps to [kQueueSizeMinKb, kQueueSizeMaxKb] and rounds up to a power of two.
long normalize_queue_size_kb(long requested_kb);

struct QueueStatsSnapshot {
    std::uint64_t written_packets = 0;
    std::uint64_t written_bytes   = 0;
    std::uint64_t dropped_packets = 0;
    std::uint64_t dropped_bytes   = 0;
};

// Counters reported by the stats message. They are separate from the ring
// positions: reset() must not touch the positions, and the counters keep
// accumulating when the ring is replaced by resize().
//
// Threads: count_*() on the capture thread only; snapshot() and reset() on
// the main thread only. reset() records a baseline instead of writing the
// counters, so each counter keeps a single writer.
class QueueStats {
public:
    void count_written(std::size_t bytes);
    void count_dropped(std::size_t bytes);

    QueueStatsSnapshot snapshot() const;  // values since the last reset()
    void reset();

private:
    QueueStatsSnapshot totals() const;

    SingleWriterCounter written_packets_;  // written by the capture thread only
    SingleWriterCounter written_bytes_;
    SingleWriterCounter dropped_packets_;
    SingleWriterCounter dropped_bytes_;
    QueueStatsSnapshot  baseline_;  // main thread only
};

class PacketQueue {
public:
    // Returns nullptr if capacity_bytes is not a power of two or the memory
    // cannot be allocated.
    static std::unique_ptr<PacketQueue> create(std::size_t capacity_bytes);

    // Capture thread. Writes the whole packet, or drops it when the queue is
    // full (no partial writes, no automatic removal of old data).
    void push(const std::uint8_t* data, std::size_t size);

    // Replaces the ring with a new, empty one. Must only be called while
    // neither the capture thread nor the audio thread uses the queue.
    // On failure the current ring is kept and false is returned.
    bool resize(std::size_t capacity_bytes);

          ByteRing&   ring ()       { return *ring_; }
    const ByteRing&   ring () const { return *ring_; }
          QueueStats& stats()       { return stats_; }

private:
    explicit PacketQueue(std::unique_ptr<ByteRing> ring);

    std::unique_ptr<ByteRing> ring_;
    QueueStats stats_;
};

} // namespace raysniff
