#pragma once
#include "WorldStateProtocol.h"

namespace RTE::WorldStateSoundProtocol {
// Inner audio payload schema; transport integration uses the common session
// envelope. Never serialize native FMOD channels, pointers or size_t hashes.
inline constexpr std::uint32_t Magic = 0x31414343; // CCA1
inline constexpr std::uint16_t Version = 3;
inline constexpr std::size_t MaxEvents = 256, MaxBytes = 256 * 1024;
enum class Action : std::uint8_t { Play, Stop, Position, Volume, Pitch, Pause, Fade, GlobalPitch, BusPause, Pan, Panning };
struct Event {
    Action Type = Action::Stop;
    std::uint64_t Container = 0, Voice = 0;
    std::int8_t Player = -1;
    std::string Module, Resource;
    float OffsetX = 0, OffsetY = 0;
    float X = 0, Y = 0, Gain = 1, Rate = 1, Attenuation = 0, Pan = 0, Panning = 1;
    std::int32_t Loops = 0;
    std::uint8_t Bus = 0;
    std::uint16_t Priority = 128;
    bool Immobile = false, AffectedByGlobalPitch = true, Paused = false;
    std::uint32_t FadeMilliseconds = 0;
    bool operator==(const Event&) const = default;
};
namespace Detail {
using namespace RTE::WorldStateProtocol::Detail;
inline bool RelativeResource(const std::string& value) {
    if (value.empty() || value.size() > RTE::WorldStateProtocol::c_MaxStringBytes ||
        !RTE::UTF8::IsValid(value) || value.find('\0') != std::string::npos ||
        value.front() == '/' || value.find_first_of(":\\") != std::string::npos) return false;
    std::size_t offset = 0;
    while (offset < value.size()) {
        const auto end = value.find('/', offset);
        const auto part = value.substr(offset, end == std::string::npos ? end : end - offset);
        if (part.empty() || part == "." || part == "..") return false;
        if (end == std::string::npos) return true;
        offset = end + 1;
    }
    return false;
}
inline bool Valid(const Event& event) {
    if (event.Type == Action::GlobalPitch)
        return !event.Container && event.Player == -1 && event.Bus > 0 && event.Bus <= 7 && std::isfinite(event.Rate) && event.Rate >= 0.125F && event.Rate <= 8;
    if (event.Type == Action::BusPause) return !event.Container && event.Player == -1 && event.Bus <= 2;
    if (!event.Container || event.Player < -1 || event.Player >= RTE::WorldStateProtocol::c_PlayerSlotCount) return false;
    switch (event.Type) {
        case Action::Play:
            return event.Voice && RelativeResource(event.Module) && event.Module.find('/') == std::string::npos &&
                event.Module.ends_with(".rte") && RelativeResource(event.Resource) && event.Bus <= 2 && event.Priority <= 256 && event.Loops >= -1 &&
                std::isfinite(event.OffsetX) && std::isfinite(event.OffsetY) && std::isfinite(event.X) && std::isfinite(event.Y) && std::isfinite(event.Gain) && event.Gain >= 0 && event.Gain <= 10 &&
                std::isfinite(event.Rate) && event.Rate > 0 && event.Rate <= 64 && std::isfinite(event.Attenuation) && event.Attenuation >= 0 &&
                std::isfinite(event.Pan) && event.Pan >= -1 && event.Pan <= 1 && std::isfinite(event.Panning) && event.Panning >= 0 && event.Panning <= 10;
        case Action::Stop: case Action::Pause: return true;
        case Action::Position: return std::isfinite(event.X) && std::isfinite(event.Y);
        case Action::Volume: return std::isfinite(event.Gain) && event.Gain >= 0 && event.Gain <= 10;
        case Action::Pitch: return std::isfinite(event.Rate) && event.Rate > 0 && event.Rate <= 64;
        case Action::Pan: return std::isfinite(event.Pan) && event.Pan >= -1 && event.Pan <= 1;
        case Action::Panning: return std::isfinite(event.Panning) && event.Panning >= 0 && event.Panning <= 10;
        case Action::GlobalPitch: case Action::BusPause: return false;
        case Action::Fade: return event.FadeMilliseconds <= static_cast<std::uint32_t>(std::numeric_limits<std::int32_t>::max());
    }
    return false;
}

}
inline bool Encode(std::span<const Event> events, std::vector<std::uint8_t>& bytes) {
    using namespace Detail;
    if (events.empty() || events.size() > MaxEvents) return false;
    std::vector<std::uint8_t> output;
    WriteUnsigned(output, Magic, 4); WriteUnsigned(output, Version, 2); WriteUnsigned(output, events.size(), 2);
    for (const auto& event : events) {
        if (!Valid(event)) return false;
        WriteUnsigned(output, static_cast<std::uint8_t>(event.Type), 1);
        WriteUnsigned(output, event.Container, 8); WriteUnsigned(output, static_cast<std::uint8_t>(event.Player), 1);
        switch (event.Type) {
            case Action::Play:
                WriteUnsigned(output, event.Voice, 8);
                if (!WriteString(output, event.Module) || !WriteString(output, event.Resource)) return false;
                for (float value : {event.X, event.Y, event.Gain, event.Rate, event.Attenuation, event.Pan, event.Panning, event.OffsetX, event.OffsetY}) WriteFloat(output, value);
                WriteUnsigned(output, static_cast<std::uint32_t>(event.Loops), 4);
                WriteUnsigned(output, event.Bus, 1); WriteUnsigned(output, event.Priority, 2);
                WriteUnsigned(output, event.Immobile, 1); WriteUnsigned(output, event.AffectedByGlobalPitch, 1);
                WriteUnsigned(output, event.Paused, 1); break;
            case Action::Position: WriteFloat(output, event.X); WriteFloat(output, event.Y); break;
            case Action::Volume: WriteFloat(output, event.Gain); break;
            case Action::Pitch: WriteFloat(output, event.Rate); break;
            case Action::Pause: WriteUnsigned(output, event.Paused, 1); break;
            case Action::Fade: WriteUnsigned(output, event.FadeMilliseconds, 4); break;
            case Action::GlobalPitch: WriteUnsigned(output, event.Bus, 1); WriteFloat(output, event.Rate); break;
            case Action::BusPause: WriteUnsigned(output, event.Bus, 1); WriteUnsigned(output, event.Paused, 1); break;
            case Action::Pan: WriteFloat(output, event.Pan); break;
            case Action::Panning: WriteFloat(output, event.Panning); break;
            case Action::Stop: break;
        }
        if (output.size() > MaxBytes) return false;
    }
    bytes = std::move(output); return true;
}
inline bool Decode(std::span<const std::uint8_t> bytes, std::vector<Event>& events) {
    using namespace Detail;
    if (bytes.size() < 8 || bytes.size() > MaxBytes) return false;
    std::size_t offset = 0; std::uint64_t value = 0;
    if (!ReadUnsigned(bytes, offset, 4, value) || value != Magic || !ReadUnsigned(bytes, offset, 2, value) || value != Version ||
        !ReadUnsigned(bytes, offset, 2, value) || !value || value > MaxEvents) return false;
    const auto count = static_cast<std::size_t>(value);
    std::vector<Event> decoded; decoded.reserve(count);
    auto boolean = [&](bool& target) {
        if (!ReadUnsigned(bytes, offset, 1, value) || value > 1) return false;
        target = value != 0; return true;
    };
    for (std::size_t index = 0; index < count; ++index) {
        Event event;
        if (!ReadUnsigned(bytes, offset, 1, value) || value > static_cast<std::uint8_t>(Action::Panning)) return false;
        event.Type = static_cast<Action>(value);
        if (!ReadUnsigned(bytes, offset, 8, event.Container) || !ReadUnsigned(bytes, offset, 1, value)) return false;
        event.Player = std::bit_cast<std::int8_t>(static_cast<std::uint8_t>(value));
        switch (event.Type) {
            case Action::Play:
                if (!ReadUnsigned(bytes, offset, 8, event.Voice) || !ReadString(bytes, offset, event.Module) || !ReadString(bytes, offset, event.Resource)) return false;
                for (float* target : {&event.X, &event.Y, &event.Gain, &event.Rate, &event.Attenuation, &event.Pan, &event.Panning, &event.OffsetX, &event.OffsetY})
                    if (!ReadFloat(bytes, offset, *target)) return false;
                if (!ReadUnsigned(bytes, offset, 4, value)) return false;
                event.Loops = std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(value));
                if (!ReadUnsigned(bytes, offset, 1, value)) return false;
                event.Bus = static_cast<std::uint8_t>(value);
                if (!ReadUnsigned(bytes, offset, 2, value)) return false;
                event.Priority = static_cast<std::uint16_t>(value);
                if (!boolean(event.Immobile) || !boolean(event.AffectedByGlobalPitch) || !boolean(event.Paused)) return false;
                break;
            case Action::Position: if (!ReadFloat(bytes, offset, event.X) || !ReadFloat(bytes, offset, event.Y)) return false; break;
            case Action::Volume: if (!ReadFloat(bytes, offset, event.Gain)) return false; break;
            case Action::Pitch: if (!ReadFloat(bytes, offset, event.Rate)) return false; break;
            case Action::Pause: if (!boolean(event.Paused)) return false; break;
            case Action::Fade:
                if (!ReadUnsigned(bytes, offset, 4, value)) return false;
                event.FadeMilliseconds = static_cast<std::uint32_t>(value); break;
            case Action::GlobalPitch:
                if (!ReadUnsigned(bytes, offset, 1, value) || !ReadFloat(bytes, offset, event.Rate)) return false;
                event.Bus = static_cast<std::uint8_t>(value); break;
            case Action::BusPause:
                if (!ReadUnsigned(bytes, offset, 1, value)) return false;
                event.Bus = static_cast<std::uint8_t>(value);
                if (!boolean(event.Paused)) return false;
                break;
            case Action::Pan: if (!ReadFloat(bytes, offset, event.Pan)) return false; break;
            case Action::Panning: if (!ReadFloat(bytes, offset, event.Panning)) return false; break;
            case Action::Stop: break;
        }
        if (!Valid(event)) return false;
        decoded.push_back(std::move(event));
    }
    if (offset != bytes.size()) return false;
    events = std::move(decoded); return true;
}
inline bool EncodePacket(std::uint32_t sceneRevision, std::uint32_t sequence, std::span<const Event> events, std::vector<std::uint8_t>& packet) {
    using namespace Detail;
    std::vector<std::uint8_t> body;
    if (!sceneRevision || !sequence || !Encode(events, body) || body.size() > MaxBytes - RTE::WorldStateProtocol::c_HeaderSize - 4) return false;
    packet.clear();
    WriteUnsigned(packet, RTE::WorldStateProtocol::c_Magic, 4);
    WriteUnsigned(packet, RTE::WorldStateProtocol::c_Version, 2);
    WriteUnsigned(packet, static_cast<std::uint8_t>(RTE::WorldStateProtocol::MessageType::SoundEvents), 1);
    WriteUnsigned(packet, 0, 1); WriteUnsigned(packet, sequence, 4); WriteUnsigned(packet, body.size() + 4, 4);
    WriteUnsigned(packet, sceneRevision, 4);
    packet.insert(packet.end(), body.begin(), body.end()); return true;
}
inline bool DecodePacket(std::span<const std::uint8_t> packet, std::uint32_t& sceneRevision, std::uint32_t& sequence, std::vector<Event>& events) {
    using namespace Detail;
    if (packet.size() < RTE::WorldStateProtocol::c_HeaderSize + 4 || packet.size() > MaxBytes) return false;
    std::size_t offset = 0; std::uint64_t value = 0, serial = 0, revision = 0;
    if (!ReadUnsigned(packet, offset, 4, value) || value != RTE::WorldStateProtocol::c_Magic ||
        !ReadUnsigned(packet, offset, 2, value) || value != RTE::WorldStateProtocol::c_Version ||
        !ReadUnsigned(packet, offset, 1, value) || value != static_cast<std::uint8_t>(RTE::WorldStateProtocol::MessageType::SoundEvents) ||
        !ReadUnsigned(packet, offset, 1, value) || value != 0 || !ReadUnsigned(packet, offset, 4, serial) || !serial ||
        !ReadUnsigned(packet, offset, 4, value) || value != packet.size() - RTE::WorldStateProtocol::c_HeaderSize ||
        !ReadUnsigned(packet, offset, 4, revision) || !revision || !Decode(packet.subspan(offset), events)) return false;
    sceneRevision = static_cast<std::uint32_t>(revision); sequence = static_cast<std::uint32_t>(serial); return true;
}
}
