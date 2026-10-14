#pragma once
#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

namespace RTE {
// Playback lifetime only; no renderer, audio device or wall-clock dependency.
class SoundPlaybackTimeline {
public:
    void Advance(double now) {
        const double elapsed = std::max(0.0, now - m_LastTime);
        m_LastTime = now;
        if (!m_Paused && !m_GroupPaused) for (auto& voice : m_Voices) {
            voice.Remaining -= elapsed * voice.Rate;
            voice.Phase = std::fmod(voice.Phase + elapsed * voice.Rate, voice.Length);
            voice.FadeRemaining -= elapsed;
        }
        std::erase_if(m_Voices, [](const Voice& voice) { return voice.Remaining <= 0 || voice.FadeRemaining <= 0; });
    }
    bool Add(double now, double length, int loops, double rate) {
        Advance(now);
        if (!std::isfinite(length) || length <= 0 || loops < -1 || !std::isfinite(rate) || rate <= 0 || m_Voices.size() >= 128) return false;
        m_Voices.push_back({length, loops == -1 ? std::numeric_limits<double>::infinity() : length * (static_cast<double>(loops) + 1), rate});
        return true;
    }
    bool Playing(double now) { Advance(now); return !m_Voices.empty(); }
    std::size_t Count(double now) { Advance(now); return m_Voices.size(); }
    double Phase(double now) { Advance(now); return m_Voices.empty() ? 0 : m_Voices.front().Phase; }
    double FadeRemaining(double now) { Advance(now); return m_Voices.empty() ? 0 : m_Voices.front().FadeRemaining; }
    bool Seek(double now, double position) {
        if (!std::isfinite(position) || position < 0) return false;
        Advance(now);
        if (m_Voices.empty()) return false;
        for (auto& voice : m_Voices) {
            const double target = std::fmod(position, voice.Length);
            if (std::isfinite(voice.Remaining)) voice.Remaining = std::max(0.0, voice.Remaining + voice.Phase - target);
            voice.Phase = target;
        }
        Advance(now);
        return !m_Voices.empty();
    }
    void Pause(double now, bool value) { Advance(now); m_Paused = value; }
    void PauseGroup(double now, bool value) { Advance(now); m_GroupPaused = value; }
    void Pitch(double now, double rate) { Advance(now); if (std::isfinite(rate) && rate > 0) for (auto& voice : m_Voices) voice.Rate = rate; }
    void MultiplyPitch(double now, double multiplier) { Advance(now); if (std::isfinite(multiplier) && multiplier > 0) for (auto& voice : m_Voices) voice.Rate *= multiplier; }
    void Fade(double now, double duration) { Advance(now); for (auto& voice : m_Voices) voice.FadeRemaining = std::max(0.0, duration); Advance(now); }
    void FinishLoops(double now) { Advance(now); for (auto& voice : m_Voices) voice.Remaining = std::min(voice.Remaining, voice.Length - voice.Phase); }
private:
    struct Voice {
        double Length, Remaining, Rate;
        double Phase = 0;
        double FadeRemaining = std::numeric_limits<double>::infinity();
    };
    std::vector<Voice> m_Voices;
    double m_LastTime = 0;
    bool m_Paused = false, m_GroupPaused = false;
};

inline bool TestSoundPlaybackTimeline() {
    SoundPlaybackTimeline once;
    if (!once.Add(0, 100, 0, 1) || !once.Playing(99) || once.Playing(100)) return false;
    SoundPlaybackTimeline loops;
    if (!loops.Add(0, 100, 2, 1) || !loops.Playing(299) || loops.Playing(300)) return false;
    SoundPlaybackTimeline paused;
    paused.Add(0, 100, 0, 1); paused.Pause(25, true);
    if (!paused.Playing(1000)) return false;
    paused.Pause(1000, false); paused.Pitch(1025, 2);
    if (!paused.Playing(1049) || paused.Playing(1050)) return false;
    SoundPlaybackTimeline infinite;
    infinite.Add(0, 100, -1, 1);
    if (!infinite.Playing(10025)) return false;
    infinite.FinishLoops(10025);
    if (!infinite.Playing(10099) || infinite.Playing(10100)) return false;
    SoundPlaybackTimeline fade;
    fade.Add(0, 100, -1, 1); fade.Fade(10, 25);
    if (!fade.Playing(34) || fade.Playing(35)) return false;
    SoundPlaybackTimeline overlap;
    overlap.Add(0, 100, 0, 1); overlap.Add(50, 100, 0, 1);
    if (!overlap.Playing(100) || overlap.Playing(150)) return false;
    SoundPlaybackTimeline group;
    group.Add(0, 100, 0, 1); group.PauseGroup(20, true); group.Pause(20, true);
    group.PauseGroup(1000, false);
    if (!group.Playing(2000)) return false;
    group.Pause(2000, false); group.MultiplyPitch(2000, 2);
    if (!group.Playing(2039) || group.Playing(2040)) return false;
    return !group.Add(2040, 0, 0, 1) && !group.Add(2040, 100, -2, 1) && !group.Add(2040, 100, 0, 0);
}
}
