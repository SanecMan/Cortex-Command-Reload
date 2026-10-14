#pragma once
#include <chrono>
namespace RTE {
// Audio lifetime follows real elapsed time, independently of simulation scale
// and scene timer resets. Group pause and pitch are applied by the timelines.
inline double SoundPlaybackClockMilliseconds() {
    return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now().time_since_epoch()).count();
}
}
