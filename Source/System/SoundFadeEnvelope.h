#pragma once
#include <algorithm>
#include <cmath>
#include <limits>
namespace RTE {
class SoundFadeEnvelope {
public:
    void Advance(double now) {
        if (now < m_LastTime) return;
        if (!m_GroupPaused) m_Remaining = std::max(0.0, m_Remaining - (now - m_LastTime) * m_Rate);
        m_LastTime = now;
    }
    bool Start(double now, double duration, float gain = 1) {
        if (!std::isfinite(duration) || !std::isfinite(gain) || gain < 0 || gain > 1) return false;
        Advance(now); m_Duration = m_Remaining = std::max(0.0, duration); m_StartGain = gain;
        return true;
    }
    void PauseGroup(double now, bool paused) { Advance(now); m_GroupPaused = paused; }
    void Pitch(double now, double rate) { Advance(now); if (std::isfinite(rate) && rate > 0) m_Rate = rate; }
    double Remaining(double now) { Advance(now); return m_Remaining; }
    bool Expired(double now) { return Remaining(now) <= 0; }
    float Gain(double now) {
        Advance(now);
        if (m_Remaining <= 0) return 0;
        if (!std::isfinite(m_Duration)) return 1;
        return m_StartGain * static_cast<float>(std::clamp(m_Remaining / m_Duration, 0.0, 1.0));
    }
private:
    double m_LastTime = 0;
    double m_Remaining = std::numeric_limits<double>::infinity(), m_Duration = m_Remaining;
    float m_StartGain = 1;
    bool m_GroupPaused = false;
    double m_Rate = 1;
};
}
