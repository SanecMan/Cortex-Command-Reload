#pragma once
#include <cstdint>
#include <limits>

namespace RTE {
inline std::uint64_t SoundFadeSamples(int milliseconds, int sampleRate) {
    if (milliseconds <= 0 || sampleRate <= 0) return 0;
    return static_cast<std::uint64_t>(milliseconds) * static_cast<std::uint64_t>(sampleRate) / 1000;
}
inline bool SoundFadeEnd(std::uint64_t clock, std::uint64_t samples, std::uint64_t& end) {
    if (samples > std::numeric_limits<std::uint64_t>::max() - clock) return false;
    end = clock + samples; return true;
}
}
