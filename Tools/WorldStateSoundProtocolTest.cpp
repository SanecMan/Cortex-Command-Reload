#include "WorldStateSoundProtocol.h"
#include "WorldStateSoundQueue.h"
#include <iostream>

int main() {
    using namespace RTE::WorldStateSoundProtocol;
    Event play;
    play.Type = Action::Play; play.Container = 7; play.Voice = 9;
    play.Module = "Мод.rte"; play.Resource = "Sounds/Ёжик.flac";
    play.Loops = -1; play.Priority = 256; play.Bus = 2;
    play.Immobile = true; play.Paused = true;
    std::vector<Event> source{play};
    for (auto action : {Action::Stop, Action::Position, Action::Volume, Action::Pitch, Action::Pause, Action::Fade}) {
        Event event; event.Type = action; event.Container = 7; event.Player = 3;
        if (action == Action::Position) { event.X = -12; event.Y = 34; }
        if (action == Action::Volume) event.Gain = 0.5F;
        if (action == Action::Pitch) event.Rate = 2;
        if (action == Action::Pause) event.Paused = true;
        if (action == Action::Fade) event.FadeMilliseconds = 250;
        source.push_back(event);
    }
    auto check = [](bool passed, const char* name) { if (!passed) std::cerr << "FAILED " << name << '\n'; return passed; };
    std::vector<std::uint8_t> bytes;
    std::vector<Event> decoded;
    if (!check(Encode(source, bytes) && Decode(bytes, decoded) && decoded == source, "actions and Unicode round trip")) return 1;
    const auto valid = bytes;
	std::uint32_t sceneRevision = 0, sequence = 0;
	if (!check(EncodePacket(3, 42, source, bytes) && DecodePacket(bytes, sceneRevision, sequence, decoded) &&
	    sceneRevision == 3 && sequence == 42 && decoded == source, "session envelope")) return 1;
	bytes[4] = static_cast<std::uint8_t>(RTE::WorldStateProtocol::c_Version - 1);
	if (!check(!DecodePacket(bytes, sceneRevision, sequence, decoded), "foreign session version")) return 1;
	if (!check(!EncodePacket(0, 42, source, bytes) && !EncodePacket(3, 0, source, bytes), "invalid envelope identity")) return 1;
    if (!check(Encode(std::vector<Event>{play}, bytes), "single play encoding")) return 1;
    bytes.back() = 2;
    if (!check(!Decode(bytes, decoded), "invalid boolean rejection")) return 1;
    if (!check(Encode(std::vector<Event>{play}, bytes), "single play reset")) return 1;
    const std::size_t positionOffset = 8 + 10 + 8 + 2 + play.Module.size() + 2 + play.Resource.size();
    std::vector<std::uint8_t> nan;
    RTE::WorldStateProtocol::Detail::WriteFloat(nan, std::numeric_limits<float>::quiet_NaN());
    std::copy(nan.begin(), nan.end(), bytes.begin() + positionOffset);
    if (!check(!Decode(bytes, decoded), "invalid numeric payload rejection")) return 1;
    for (std::size_t size = 0; size < valid.size(); ++size) {
        if (!check(!Decode(std::span<const std::uint8_t>(valid).first(size), decoded), "truncation rejection")) return 1;
    }
    bytes = valid; bytes.push_back(0);
    if (!check(!Decode(bytes, decoded), "trailing bytes rejection")) return 1;
    bytes = valid; bytes[8] = 255;
    if (!check(!Decode(bytes, decoded), "unknown action rejection")) return 1;
    bytes = valid; bytes[6] = 1; bytes[7] = 1;
    if (!check(!Decode(bytes, decoded), "oversized count rejection")) return 1;
    for (auto path : {"../secret.wav", "Sounds/../secret.wav", "C:/secret.wav", "/secret.wav", "Sounds\\secret.wav", "Sounds//secret.wav"}) {
        auto invalid = play; invalid.Resource = path;
        if (!check(!Encode(std::vector<Event>{invalid}, bytes), "unsafe resource rejection")) return 1;
    }
    auto invalid = play; invalid.Priority = 257;
    if (!check(!Encode(std::vector<Event>{invalid}, bytes), "priority limit")) return 1;
    invalid = play; invalid.Gain = std::numeric_limits<float>::quiet_NaN();
    if (!check(!Encode(std::vector<Event>{invalid}, bytes), "NaN rejection")) return 1;
    invalid = play; invalid.Module.assign(1, '\xFF');
    if (!check(!Encode(std::vector<Event>{invalid}, bytes), "invalid UTF-8 rejection")) return 1;
    std::vector<Event> maximum(MaxEvents, play);
    if (!check(Encode(maximum, bytes) && Decode(bytes, decoded) && decoded.size() == MaxEvents, "maximum batch")) return 1;
    maximum.push_back(play);
    if (!check(!Encode(maximum, bytes), "batch limit")) return 1;
    Event stop; stop.Container = 1;
    const std::vector<std::uint8_t> golden{0x43,0x43,0x41,0x31,1,0,1,0,1,1,0,0,0,0,0,0,0,0xFF};
    if (!check(Encode(std::vector<Event>{stop}, bytes) && bytes == golden, "fixed little-endian layout")) return 1;
    RTE::WorldStateSoundQueue queue;
    if (!check(queue.Push(2, 10, source) && queue.Drain(1).empty() && queue.Size() == source.size(), "future scene retained")) return 1;
    if (!check(queue.Drain(2) == source && queue.Size() == 0 && queue.Drain(2).empty(), "single consumption")) return 1;
    if (!check(queue.Push(2, 10, source) && queue.Push(2, 9, source) && queue.Drain(2).empty(), "late duplicate and older sequence rejected")) return 1;
    if (!check(queue.Push(2, 11, source) && queue.Drain(3).empty() && queue.Size() == 0, "stale scene discarded")) return 1;
    queue.Clear();
    if (!check(queue.Push(1, 1, source) && queue.Drain(1) == source, "reconnect resets sequence")) return 1;
    queue.Clear();
    for (std::uint32_t index = 1; index <= RTE::WorldStateSoundQueue::MaxBatches; ++index)
        if (!check(queue.Push(1, index, {stop}), "batch capacity")) return 1;
    if (!check(!queue.Push(1, 33, {stop}) && queue.Size() == 32 && queue.Drain(1).size() == 32, "batch overflow preserves lifecycle events")) return 1;
    queue.Clear();
    maximum.resize(MaxEvents);
    for (std::uint32_t index = 1; index <= 8; ++index)
        if (!check(queue.Push(1, index, maximum), "event capacity")) return 1;
    if (!check(!queue.Push(1, 9, {stop}) && queue.Size() == 2048 && queue.Drain(1).size() == 2048, "event overflow preserves lifecycle events")) return 1;
    queue.Clear();
    if (!check(queue.Push(1, UINT32_MAX, {stop}) && queue.Push(1, 1, {stop}) && queue.Drain(1).size() == 2, "sequence wrap")) return 1;
    queue.Clear();
    if (!check(queue.Push(UINT32_MAX, 1, {stop}) && queue.Push(1, 2, {stop}) && queue.Drain(1).size() == 1 && queue.Size() == 0, "scene wrap")) return 1;
    if (!check(!queue.Push(0, 1, {stop}) && !queue.Push(1, 0, {stop}), "queue identity validation")) return 1;
    std::cout << "sound_protocol_test=passed\nsound_queue_test=passed\n";
    return 0;
}
