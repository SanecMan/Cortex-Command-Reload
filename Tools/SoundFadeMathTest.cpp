#include "SoundFadeMath.h"
#include <climits>
#include <iostream>
int main() {
    using namespace RTE;
    std::uint64_t end = 0;
    if (SoundFadeSamples(-1, 48000) || SoundFadeSamples(100, 0) ||
        SoundFadeSamples(1000, 48000) != 48000 ||
        SoundFadeSamples(INT_MAX, 48000) != 103079215056ULL ||
        !SoundFadeEnd(1024, 4800, end) || end != 5824 ||
        SoundFadeEnd(UINT64_MAX - 1, 2, end)) return 1;
    std::cout << "sound_fade_math_test=passed\n";
}
