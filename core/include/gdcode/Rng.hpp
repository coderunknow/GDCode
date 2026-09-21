#pragma once

#include <cstdint>

namespace gdcode {

/// Deterministic PRNG (xorshift64*).
///
/// Deliberately NOT std::mt19937 + std::uniform_*_distribution: the C++
/// standard does not portably specify distribution output, so "same source +
/// same seed -> same level" would break across compilers/libraries. This one
/// is a few lines of integer math and behaves identically everywhere.
class Rng {
public:
    explicit Rng(std::uint64_t seed) { reseed(seed); }

    void reseed(std::uint64_t seed) {
        // Never allow the all-zero state.
        m_state = seed ? seed : 0x9E3779B97F4A7C15ull;
    }

    std::uint64_t nextU64() {
        std::uint64_t x = m_state;
        x ^= x >> 12;
        x ^= x << 25;
        x ^= x >> 27;
        m_state = x;
        return x * 0x2545F4914F6CDD1Dull;
    }

    /// Uniform integer in [lo, hi] (inclusive). Requires lo <= hi.
    std::int64_t nextInt(std::int64_t lo, std::int64_t hi) {
        if (hi <= lo) return lo;
        std::uint64_t range = static_cast<std::uint64_t>(hi - lo) + 1;
        return lo + static_cast<std::int64_t>(nextU64() % range);
    }

private:
    std::uint64_t m_state = 0;
};

} // namespace gdcode
