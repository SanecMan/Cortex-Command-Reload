#include "WorldStateProtocol.h"
#include <iostream>
#include <limits>
int main() {
    using namespace RTE::WorldStateProtocol;
    Snapshot source, decoded;
    source.Objectives = {{"Цель: Ёжик 🙂", 23.5F, -18.25F, 0, 0}, {"Enemy", 500, 600, 1, 3}};
    std::vector<std::uint8_t> bytes;
    auto check = [&](bool passed, const char* name) { if (!passed) std::cerr << name << '\n'; return passed; };
    if (!check(EncodeSnapshot(source, 1, bytes) && DecodeSnapshot(bytes, decoded) && decoded.Objectives == source.Objectives, "roundtrip")) return 1;
    const auto valid = bytes;
    const std::string marker = source.Objectives[0].Description;
    const auto text = static_cast<std::size_t>(std::search(bytes.begin(), bytes.end(), marker.begin(), marker.end(),
        [](std::uint8_t byte, char character) { return byte == static_cast<unsigned char>(character); }) - bytes.begin());
    if (!check(text < bytes.size(), "text marker")) return 1;
    bytes[text] = 0xFF;
    if (!check(!DecodeSnapshot(bytes, decoded), "invalid utf8")) return 1;
    bytes = valid; bytes[text - 2] = 1; bytes[text - 1] = 16;
    if (!check(!DecodeSnapshot(bytes, decoded), "declared text overflow")) return 1;
    bytes = valid; bytes[text + marker.size()] = 0; bytes[text + marker.size() + 1] = 0;
    bytes[text + marker.size() + 2] = 0x80; bytes[text + marker.size() + 3] = 0x7F;
    if (!check(!DecodeSnapshot(bytes, decoded), "wire infinite position")) return 1;
    bytes = valid; bytes[text - 4] = 1; bytes[text - 3] = 1;
    if (!check(!DecodeSnapshot(bytes, decoded), "count overflow")) return 1;
    bytes = valid; bytes[text + marker.size() + 10] = 4;
    if (!check(!DecodeSnapshot(bytes, decoded), "invalid arrow")) return 1;
    bytes = valid; bytes[text + marker.size() + 8] = 4;
    if (!check(!DecodeSnapshot(bytes, decoded), "invalid team")) return 1;
    bytes = valid; bytes.resize(text + marker.size() + 4);
    if (!check(!DecodeSnapshot(bytes, decoded), "truncated objective")) return 1;
    source.Objectives[0].X = std::numeric_limits<float>::infinity();
    if (!check(!EncodeSnapshot(source, 1, bytes), "nonfinite position")) return 1;
    source.Objectives[0].X = 0; source.Objectives[0].Description.assign(c_MaxObjectiveTextBytes + 1, 'a');
    if (!check(!EncodeSnapshot(source, 1, bytes), "text overflow")) return 1;
    source.Objectives.assign(c_MaxObjectives + 1, {});
    if (!check(!EncodeSnapshot(source, 1, bytes), "list overflow")) return 1;
    source.Objectives.clear();
    if (!check(EncodeSnapshot(source, 1, bytes) && DecodeSnapshot(bytes, decoded) && decoded.Objectives.empty(), "clear")) return 1;
    std::cout << "world_state_objectives_codec=passed version=" << c_Version << '\n';
}
