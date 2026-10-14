#include "SoundPlaybackTimeline.h"
#include <cmath>
#include <iostream>

int main() {
    using RTE::SoundPlaybackTimeline;
    SoundPlaybackTimeline loop;
    if (!loop.Add(0, 100, -1, 1) || !loop.Seek(0, 250) || loop.Phase(25) != 75) return 1;
    loop.Pause(25, true);
    if (loop.Phase(500) != 75) return 1;
    loop.Pause(500, false); loop.Pitch(500, 2);
    if (loop.Phase(510) != 95) return 1;
    loop.FinishLoops(510);
    if (!loop.Playing(512) || loop.Playing(513)) return 1;
    SoundPlaybackTimeline finite;
    if (!finite.Add(0, 100, 0, 1) || !finite.Seek(0, 80) || !finite.Playing(19) || finite.Playing(20)) return 1;
    SoundPlaybackTimeline invalid;
    invalid.Add(0, 100, -1, 1);
    if (invalid.Seek(0, -1) || invalid.Seek(0, std::nan("")) || invalid.Phase(0) != 0) return 1;
    SoundPlaybackTimeline empty;
    if (empty.Seek(0, 10)) return 1;
    std::cout << "sound_playback_seek_test=passed\n";
}
