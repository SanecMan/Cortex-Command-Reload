#include "ContentFile.h"
#include "ConsoleMan.h"
#include "fmod/fmod.hpp"
#include "fmod/fmod_errors.h"

namespace RTE {
std::size_t ContentFile::ReleaseLoadedSounds() {
    std::size_t released = 0;
    for (const auto& [path, sound] : s_LoadedSamples) {
        if (!sound) continue;
        // release waits for nonblocking loads to finish. Leaving these samples
        // to System teardown reproduced a worker-thread crash in FMOD 2.02.20.
        const auto result = sound->release();
        if (result == FMOD_OK) ++released;
        else g_ConsoleMan.PrintString("ERROR: Failed to release cached sound " + path + ": " + FMOD_ErrorString(result));
    }
    s_LoadedSamples.clear();
    return released;
}
}
