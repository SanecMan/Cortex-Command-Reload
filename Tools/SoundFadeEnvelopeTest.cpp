#include "SoundPlaybackTimeline.h"
#include <cmath>
#include <iostream>
int main() {
    using namespace RTE;
    auto equal = [](float a, float b) { return std::abs(a - b) < 0.0001F; };
    SoundFadeEnvelope fade;
    if (!fade.Start(0, 100) || !equal(fade.Gain(25), 0.75F)) return 1;
    fade.PauseGroup(25, true);
    if (!equal(fade.Gain(500), 0.75F) || fade.Remaining(500) != 75) return 1;
    fade.PauseGroup(500, false);
    if (!equal(fade.Gain(525), 0.5F) || !fade.Expired(575)) return 1;
    SoundFadeEnvelope late;
    if (!late.Start(0, 50, 0.5F) || !equal(late.Gain(25), 0.25F) || !late.Expired(50)) return 1;
    SoundPlaybackTimeline individual;
    individual.Add(0, 100, -1, 1); individual.Pause(0, true); individual.Fade(0, 100);
    if (individual.Phase(50) != 0 || !equal(individual.FadeGain(50), 0.5F) || individual.Playing(100)) return 1;
    SoundPlaybackTimeline group;
    group.Add(0, 100, -1, 1); group.Fade(0, 100); group.PauseGroup(25, true);
    if (group.FadeRemaining(500) != 75 || !equal(group.FadeGain(500), 0.75F)) return 1;
    group.PauseGroup(500, false);
    if (!group.Playing(574) || group.Playing(575)) return 1;
    SoundFadeEnvelope backwards;
    backwards.Start(100, 100); backwards.Gain(125); backwards.Gain(110);
    if (!equal(backwards.Gain(150), 0.5F)) return 1;
    SoundFadeEnvelope pitched;
    pitched.Start(0, 100); pitched.Pitch(0, 0.5);
    if (!equal(pitched.Gain(100), 0.5F) || pitched.Expired(199) || !pitched.Expired(200)) return 1;
    std::cout << "sound_fade_envelope_test=passed\n";
}
