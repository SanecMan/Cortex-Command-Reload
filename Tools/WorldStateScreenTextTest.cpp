#include "WorldStateProtocol.h"
#include <iostream>

// Codec-only regression: no engine globals, content, renderer or audio device.
int main() {
    using namespace RTE::WorldStateProtocol;
    auto check = [](bool passed, const char* name) {
        if (!passed) std::cerr << "FAILED: " << name << '\n';
        return passed;
    };
    Snapshot original, decoded;
    original.Tick = 73;
    for (auto& player : original.Players) {
        player.ActiveHuman = true;
        player.ScreenText = "HUD: Привет, Ёжик!\nActivity message";
        player.TextBlinkInterval = 250;
        player.TextCentered = true;
    }
    std::vector<std::uint8_t> packet;
    if (!check(EncodeSnapshot(original, 19, packet) && DecodeSnapshot(packet, decoded) && decoded.Players == original.Players,
               "Unicode and presentation round trip")) return 1;
    const auto valid = packet;
    for (auto& player : original.Players) player.ScreenText.assign(c_MaxScreenTextBytes, 'x');
    if (!check(EncodeSnapshot(original, 19, packet) && DecodeSnapshot(packet, decoded) && decoded.Players == original.Players,
               "maximum text length")) return 1;
    original.Players[0].ScreenText.push_back('x');
    if (!check(!EncodeSnapshot(original, 19, packet), "oversized text rejection")) return 1;
    original = decoded;
    original.Players[0].ScreenText.assign(1, '\xFF');
    if (!check(!EncodeSnapshot(original, 19, packet), "invalid UTF-8 rejection")) return 1;
    original = decoded;
    original.Players[0].TextBlinkInterval = std::numeric_limits<std::uint32_t>::max();
    if (!check(!EncodeSnapshot(original, 19, packet), "out-of-range blink rejection")) return 1;
    // With no inventory, the final record ends in blink[4], centered[1], inventory-count[2].
    packet = valid; packet[packet.size() - 3] = 2;
    if (!check(!DecodeSnapshot(packet, decoded), "invalid centered flag")) return 1;
    packet = valid;
    std::fill(packet.end() - 7, packet.end() - 3, 0xFF);
    if (!check(!DecodeSnapshot(packet, decoded), "invalid blink payload")) return 1;
    const std::size_t textLength = std::string("HUD: Привет, Ёжик!\nActivity message").size();
    const auto lastText = valid.size() - 7 - textLength;
    packet = valid; packet[lastText] = 0xFF;
    if (!check(!DecodeSnapshot(packet, decoded), "invalid text payload")) return 1;
    packet = valid; packet[lastText - 2] = 1; packet[lastText - 1] = 0x40;
    if (!check(!DecodeSnapshot(packet, decoded), "oversized declared length")) return 1;
    packet = valid; packet[4] = 18;
    if (!check(!DecodeSnapshot(packet, decoded), "old wire version rejection")) return 1;
    packet = valid; packet.pop_back();
    if (!check(!DecodeSnapshot(packet, decoded), "truncated packet rejection")) return 1;
    std::cout << "world_state_screen_text_codec=passed version=" << c_Version << '\n';
    return 0;
}
