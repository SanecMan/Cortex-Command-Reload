#include "fmod/fmod.hpp"
#include "fmod/fmod_errors.h"
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

// Standalone SDK/resource regression. No engine, renderer, Lua or output device.
int main(int argc, char** argv) {
    const std::filesystem::path data = argc > 1 ? argv[1] : "Data";
    std::vector<std::string> paths;
    for (const auto& file : std::filesystem::recursive_directory_iterator(data)) {
        if (!file.is_regular_file()) continue;
        const auto extension = file.path().extension().string();
        if (extension != ".flac" && extension != ".wav" && extension != ".ogg" && extension != ".mp3") continue;
        const auto utf8 = file.path().generic_u8string();
        paths.emplace_back(reinterpret_cast<const char*>(utf8.data()), utf8.size());
    }
    if (paths.empty()) { std::cerr << "No audio files for regression\n"; return 1; }
    auto check = [](FMOD_RESULT result) {
        if (result == FMOD_OK) return true;
        std::cerr << FMOD_ErrorString(result) << '\n'; return false;
    };
    for (int iteration = 0; iteration < 3; ++iteration) {
        FMOD::System* system = nullptr;
        if (!check(FMOD::System_Create(&system)) || !check(system->setOutput(FMOD_OUTPUTTYPE_NOSOUND)) ||
            !check(system->init(1024, FMOD_INIT_NORMAL, nullptr))) return 2;
        std::vector<FMOD::Sound*> sounds;
        for (const auto& path : paths) {
            FMOD::Sound* sound = nullptr;
            if (!check(system->createSound(path.c_str(), FMOD_CREATESAMPLE | FMOD_3D | FMOD_NONBLOCKING, nullptr, &sound))) {
                for (auto* loaded : sounds) loaded->release();
                system->release(); return 3;
            }
            sounds.push_back(sound);
        }
        for (auto* sound : sounds) if (!check(sound->release())) return 4;
        if (!check(system->release())) return 5;
        std::cout << "async_sound_shutdown=passed iteration=" << iteration << " samples=" << sounds.size() << '\n' << std::flush;
    }
}
