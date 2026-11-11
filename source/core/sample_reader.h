// Turns queued packet bytes into audio samples (design.md 7.6).

#pragma once

#include "byte_ring.h"

#include <cstdint>

namespace raysniff {

// Bytes per sample for a bit depth, or 0 if the bit depth is not supported.
constexpr int bytes_per_sample(long bitdepth) {
    switch (bitdepth) {
        case 8:  return 1;
        case 16: return 2;
        case 24: return 3;
        case 32: return 4;
        default: return 0;
    }
}

// Fills out[0, frames) from the ring and returns how many samples came from
// it. Audio thread only (the ring's consumer). `bytes` must be 1 to 4.
//
// Each sample takes `bytes` bytes, the first one being the lowest
// (little-endian), and is scaled as -(value - peak) / peak with
// peak = 2^(8 * bytes - 1); a value of 0 stays 0. Only whole samples are
// read: a remainder shorter than one sample stays in the ring for the next
// call, and the samples that could not be filled are 0. The bytes are read
// one by one straight from the ring, without a scratch buffer.
inline long read_samples(ByteRing& ring, int bytes, double* out, long frames) {
    const ByteRing::ReadView view = ring.begin_read();  // also applies a pending clear

    // Whole samples in the queue (a shorter remainder is left for later), and
    // how many of them this block takes.
    const std::uint64_t available_samples = view.size() / static_cast<std::uint64_t>(bytes);
    const long read_count = available_samples < static_cast<std::uint64_t>(frames) ? 
                            static_cast<long>(available_samples) : frames;
    // Signed 64 bit, so that value - peak can go below 0 (an unsigned
    // subtraction would wrap around) and 2^31 fits.
    const std::int64_t peak = std::int64_t{1} << (8 * bytes - 1);

    std::uint64_t i = 0;
    for (long s = 0; s < read_count; ++s) {
        std::uint32_t value = 0;
        for (int b = 0; b < bytes; ++b) value |= static_cast<std::uint32_t>(view[i++]) << (8 * b);
        const std::int64_t centered = static_cast<std::int64_t>(value) - peak;  // exact
        out[s] = (value == 0) ? 0.0 : -static_cast<double>(centered) / static_cast<double>(peak);
    }
    for (long s = read_count; s < frames; ++s) out[s] = 0.0;

    ring.commit_read(view, i);
    return read_count;
}

} // namespace raysniff
