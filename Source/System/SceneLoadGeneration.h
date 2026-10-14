#pragma once
#include <atomic>
#include <cstdint>

namespace RTE::SceneLoadGeneration {
namespace Detail {
inline std::atomic<std::uint64_t> Generation{0};
}
inline std::uint64_t Get() { return Detail::Generation.load(std::memory_order_relaxed); }
inline void Advance() { Detail::Generation.fetch_add(1, std::memory_order_relaxed); }

// A successful reload changes identity even when allocator addresses and preset
// names are identical. This observer has no engine/device dependencies.
class Observer {
public:
    bool Changed(std::uint64_t generation) {
        if (generation == m_Last) return false;
        m_Last = generation;
        return true;
    }
private:
    std::uint64_t m_Last = 0;
};
}
