#include "packet_queue.h"

namespace raysniff {

long normalize_queue_size_kb(long requested_kb) {
    if (requested_kb <= kQueueSizeMinKb) return kQueueSizeMinKb;
    if (requested_kb >= kQueueSizeMaxKb) return kQueueSizeMaxKb;

    long size = kQueueSizeMinKb;
    while (size < requested_kb) size <<= 1;
    return size;
}

// ---------------------------------------------------------------------------
// QueueStats
// ---------------------------------------------------------------------------

namespace {

// Adds n to a counter that only one thread writes. A plain load + store is
// enough there; it is NOT a read-modify-write, so it must not be used for a
// counter with more than one writer (use fetch_add for that).
void single_writer_add(std::atomic<std::uint64_t>& counter, std::uint64_t n) {
    const std::uint64_t current = counter.load(std::memory_order_relaxed);
    counter.store(current + n, std::memory_order_relaxed);
}

} // namespace

void QueueStats::count_written(std::size_t bytes) {
    single_writer_add(written_packets_, 1);
    single_writer_add(written_bytes_, bytes);
}

void QueueStats::count_dropped(std::size_t bytes) {
    single_writer_add(dropped_packets_, 1);
    single_writer_add(dropped_bytes_, bytes);
}

QueueStatsSnapshot QueueStats::totals() const {
    QueueStatsSnapshot s;
    s.written_packets = written_packets_.load(std::memory_order_relaxed);
    s.written_bytes = written_bytes_.load(std::memory_order_relaxed);
    s.dropped_packets = dropped_packets_.load(std::memory_order_relaxed);
    s.dropped_bytes = dropped_bytes_.load(std::memory_order_relaxed);
    return s;
}

QueueStatsSnapshot QueueStats::snapshot() const {
    const QueueStatsSnapshot t = totals();
    QueueStatsSnapshot s;
    s.written_packets = t.written_packets - baseline_.written_packets;
    s.written_bytes = t.written_bytes - baseline_.written_bytes;
    s.dropped_packets = t.dropped_packets - baseline_.dropped_packets;
    s.dropped_bytes = t.dropped_bytes - baseline_.dropped_bytes;
    return s;
}

void QueueStats::reset() {
    baseline_ = totals();
}

// ---------------------------------------------------------------------------
// PacketQueue
// ---------------------------------------------------------------------------

std::unique_ptr<PacketQueue> PacketQueue::create(std::size_t capacity_bytes) {
    auto ring = ByteRing::create(capacity_bytes);
    if (!ring) return nullptr;
    return std::unique_ptr<PacketQueue>(new PacketQueue(std::move(ring)));
}

PacketQueue::PacketQueue(std::unique_ptr<ByteRing> ring) : ring_(std::move(ring)) {}

void PacketQueue::push(const std::uint8_t* data, std::size_t size) {
    if (ring_->try_write(data, size)) stats_.count_written(size);
    else                              stats_.count_dropped(size);
}

bool PacketQueue::resize(std::size_t capacity_bytes) {
    auto ring = ByteRing::create(capacity_bytes);
    if (!ring) return false;
    ring_ = std::move(ring);
    return true;
}

} // namespace raysniff
