#include "SceneLoadGeneration.h"
#include <iostream>
int main() {
    using namespace RTE::SceneLoadGeneration;
    Observer observer;
    if (observer.Changed(0) || !observer.Changed(1) || observer.Changed(1)) return 1;
    // No address or preset change is needed to recognize another load.
    if (!observer.Changed(2) || observer.Changed(2) || !observer.Changed(3)) return 2;
    if (!observer.Changed(0) || observer.Changed(0)) return 3;
    const auto before = Get(); Advance();
    if (Get() != before + 1) return 4;
    std::cout << "scene_load_generation=passed\n";
}
