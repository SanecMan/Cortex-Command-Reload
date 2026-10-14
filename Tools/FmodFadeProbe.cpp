#include "fmod/fmod.hpp"
#include <cstdio>
#include <cmath>
#include "SoundFadeMath.h"

// Silent empirical check of the installed FMOD build, not an engine test.
int main() {
    FMOD::System* system = nullptr;
    if (FMOD::System_Create(&system) != FMOD_OK || !system) return 1;
    if (system->setOutput(FMOD_OUTPUTTYPE_NOSOUND_NRT) != FMOD_OK ||
        system->setSoftwareFormat(48000, FMOD_SPEAKERMODE_STEREO, 0) != FMOD_OK ||
        system->init(8, FMOD_INIT_NORMAL, nullptr) != FMOD_OK) return 1;
    FMOD::Sound* sound = nullptr; FMOD::Channel* channel = nullptr; FMOD::ChannelGroup* probeGroup = nullptr;
    if (system->createChannelGroup("fade-probe", &probeGroup) != FMOD_OK) return 1;
    if (system->createSound("Data/Base.rte/Sounds/GUIs/SlicePicked.flac", FMOD_2D | FMOD_LOOP_NORMAL, nullptr, &sound) != FMOD_OK ||
        system->playSound(sound, probeGroup, false, &channel) != FMOD_OK) return 1;
    channel->setLoopCount(-1); channel->setVolume(0.5F);
    system->update(); system->update();
    float base = 0, faded = 0, volume = 0;
    channel->getAudibility(&base);
    unsigned long long clock = 0; channel->getDSPClock(nullptr, &clock);
    channel->addFadePoint(clock, 0.5F);
    system->update(); system->update();
    channel->getVolume(&volume); channel->getAudibility(&faded);
    std::printf("base_audibility=%g fade_audibility=%g base_volume=%g\n", base, faded, volume);
    channel->removeFadePoints(0, ~0ULL); channel->getDSPClock(nullptr, &clock);
    channel->addFadePoint(clock, 1.0F); system->update(); system->update();
    float corrected = 0; channel->getAudibility(&corrected);
    std::printf("corrected_start_audibility=%g\n", corrected);
    channel->setPaused(true);
    FMOD::ChannelGroup* group = nullptr; channel->getChannelGroup(&group);
    unsigned long long groupBefore = 0, groupAfter = 0;
    channel->getDSPClock(nullptr, &groupBefore); group->setPaused(true);
    for (int i = 0; i < 5; ++i) system->update();
    channel->getDSPClock(nullptr, &groupAfter); group->setPaused(false);
    std::printf("group_paused_parent_clock_delta=%llu\n", groupAfter - groupBefore);
    unsigned long long normalBefore = 0, normalAfter = 0, pitchedAfter = 0;
    channel->getDSPClock(nullptr, &normalBefore);
    for (int i = 0; i < 10; ++i) system->update();
    channel->getDSPClock(nullptr, &normalAfter); group->setPitch(0.5F);
    for (int i = 0; i < 10; ++i) system->update();
    channel->getDSPClock(nullptr, &pitchedAfter); group->setPitch(1.0F);
    std::printf("normal_parent_clock_delta=%llu pitched_parent_clock_delta=%llu\n", normalAfter - normalBefore, pitchedAfter - normalAfter);
    unsigned long long pausedClock = 0; channel->getDSPClock(nullptr, &pausedClock);
    channel->addFadePoint(pausedClock + 4800, 0);
    for (int i = 0; i < 20; ++i) system->update();
    unsigned long long after = 0; bool playing = false;
    channel->getDSPClock(nullptr, &after); channel->isPlaying(&playing);
    std::printf("paused_parent_clock_delta=%llu still_playing_after_fade=%d\n", after - pausedClock, playing);
    channel->setDelay(0, after + 4800, true);
    channel->setPaused(false);
    for (int i = 0; i < 20; ++i) system->update();
    playing = false; const auto result = channel->isPlaying(&playing);
    std::printf("scheduled_stop_result=%d playing=%d\n", result, playing);
    system->close(); system->release();
    const bool passed = std::abs(base - 0.5F) < 0.001F && std::abs(faded - 0.25F) < 0.001F && std::abs(corrected - 0.5F) < 0.001F &&
        std::abs(volume - 0.5F) < 0.001F && after > pausedClock && groupAfter == groupBefore &&
        pitchedAfter - normalAfter < normalAfter - normalBefore &&
        (result == FMOD_ERR_INVALID_HANDLE || (result == FMOD_OK && !playing));
    std::printf("fmod_fade_semantics=%s\n", passed ? "passed" : "failed");
    return passed ? 0 : 1;
}
