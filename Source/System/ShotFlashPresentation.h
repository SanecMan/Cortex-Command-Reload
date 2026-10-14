#pragma once
#include <chrono>
#include <cstdint>
#include <limits>

namespace RTE {
// A recent shot survives snapshot gaps, but repeated pose application must not
// restart its flash. This clock never depends on client simulation stepping.
class ShotFlashPresentation {
public:
    static constexpr std::uint16_t c_NoShotAge = 65535;
    static constexpr std::uint16_t c_FlashWindowMS = 100;
    static std::uint64_t NowMS() {
        return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count());
    }
    void Observe(std::uint32_t sequence, std::uint16_t ageMS, std::uint64_t nowMS) {
        if (m_Initialized && sequence == m_Sequence) return;
        m_Initialized = true; m_Sequence = sequence; m_Expires = 0;
        if (ageMS >= c_FlashWindowMS) return;
        const std::uint64_t remaining = c_FlashWindowMS - ageMS;
        m_Expires = nowMS > std::numeric_limits<std::uint64_t>::max() - remaining ?
            std::numeric_limits<std::uint64_t>::max() : nowMS + remaining;
    }
    bool Visible(std::uint64_t nowMS) const { return m_Initialized && nowMS < m_Expires; }
private:
    bool m_Initialized = false;
    std::uint32_t m_Sequence = 0;
    std::uint64_t m_Expires = 0;
};
}
