// Test helper: take everything out of a ring or queue, as the consumer does.

#pragma once

#include "byte_ring.h"
#include "packet_queue.h"

#include <cstdint>
#include <vector>

namespace raysniff_test {

// Reads and removes every unread byte. Call it from the consumer side only.
inline std::vector<std::uint8_t> read_all(raysniff::ByteRing& ring) {
    auto view = ring.begin_read();
    std::vector<std::uint8_t> out;
    out.reserve(view.size());
    for (std::uint64_t i = 0; i < view.size(); ++i) out.push_back(view[i]);
    ring.commit_read(view, view.size());
    return out;
}

inline std::vector<std::uint8_t> read_all(raysniff::PacketQueue& queue) {
    return read_all(queue.ring());
}

} // namespace raysniff_test
