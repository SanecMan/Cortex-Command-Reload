#pragma once
#include <atomic>
#include <chrono>
#include <cstdint>
#include <exception>
#include <random>
#if defined(_WIN32)
#include <process.h>
#else
#include <unistd.h>
#endif

namespace RakNet { namespace GuidDetail {
inline std::uint64_t Mix(std::uint64_t value) {
    value = (value ^ (value >> 30)) * 0xbf58476d1ce4e5b9ULL;
    value = (value ^ (value >> 27)) * 0x94d049bb133111ebULL;
    return value ^ (value >> 31);
}
// A process-local sequence prevents same-clock-tick collisions. Independent
// random seeds separate processes; these IDs are not authentication secrets.
inline std::uint64_t Next() {
    static const std::uint64_t seed = [] {
#if defined(_WIN32)
        const auto process = _getpid();
#else
        const auto process = getpid();
#endif
        std::uint64_t value = Mix(static_cast<std::uint64_t>(std::chrono::steady_clock::now().time_since_epoch().count())) ^
                              Mix(static_cast<std::uint64_t>(process));
        try {
            std::random_device random;
            value ^= static_cast<std::uint64_t>(random()) << 32;
            value ^= static_cast<std::uint64_t>(random());
        } catch (const std::exception&) {
            // Retain the process/time seed when OS entropy is unavailable.
        }
        return value;
    }();
    static std::atomic<std::uint64_t> sequence{0};
    for (;;) {
        const auto value = Mix(seed + sequence.fetch_add(1, std::memory_order_relaxed) * 0x9e3779b97f4a7c15ULL);
        if (value != 0 && value != UINT64_MAX) return value;
    }
}
}}
