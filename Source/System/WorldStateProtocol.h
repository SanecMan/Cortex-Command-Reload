#pragma once

#include "UTF8.h"

#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace RTE::WorldStateProtocol {

inline constexpr std::uint32_t c_Magic = 0x31524343; // "CCR1" on the wire.
inline constexpr std::uint16_t c_Version = 6;
inline constexpr std::size_t c_HeaderSize = 16;
inline constexpr std::size_t c_MaxPacketSize = 4 * 1024 * 1024;
inline constexpr std::size_t c_MaxObjects = 65535;
inline constexpr std::size_t c_MaxStringBytes = 1024;
inline constexpr std::uint16_t c_ObjectFlagActor = 0x0001;
inline constexpr std::uint16_t c_ObjectFlagItem = 0x0002;
inline constexpr std::uint16_t c_ObjectFlagParticle = 0x0004;
inline constexpr std::uint16_t c_ObjectFlagTransientPixel = 0x0008;
inline constexpr std::int16_t c_NoTeam = -1;
inline constexpr std::int16_t c_MaxTeamCount = 4;
inline constexpr std::uint32_t c_InputElementCount = 34;
inline constexpr std::uint8_t c_PlayerSlotCount = 4;
inline constexpr std::int32_t c_MaxMouseDelta = 8192;
inline constexpr std::int16_t c_MaxMouseWheelDelta = 16;
inline constexpr std::size_t c_TerrainPatchHeaderSize = 25;
inline constexpr std::size_t c_MaxTerrainPatchBytes = c_MaxPacketSize - c_HeaderSize - c_TerrainPatchHeaderSize;

enum class MessageType : std::uint8_t {
	ClientHello = 1,
	InputCommand = 2,
	WorldSnapshot = 3,
	Disconnect = 4,
	ClientAssignment = 5,
	TerrainPatch = 6
};

enum class TerrainLayer : std::uint8_t {
	Material = 0,
	Foreground = 1,
	Background = 2
};

struct ObjectState {
	std::uint64_t NetworkId = 0;
	std::string ClassName;
	std::string ModuleName;
	std::string PresetName;
	float PositionX = 0.0F;
	float PositionY = 0.0F;
	float VelocityX = 0.0F;
	float VelocityY = 0.0F;
	float Rotation = 0.0F;
	float AngularVelocity = 0.0F;
	float Health = 0.0F;
	std::uint16_t SpriteFrame = 0;
	std::int16_t Team = -1;
	std::uint16_t Flags = 0;
	bool HFlipped = false;
	std::uint8_t PixelMaterialId = 0;
	std::uint16_t PixelColorIndex = 0;
	float PixelMass = 0.0F;
	std::uint32_t PixelLifetime = 0;
	float PixelSharpness = 1.0F;
};

struct Snapshot {
	std::uint32_t Tick = 0;
	std::uint32_t SceneRevision = 0;
	std::string ActivityClassName;
	std::string ActivityPreset;
	std::string ActivityModuleName;
	std::string SceneModuleName;
	std::string ScenePreset;
	std::vector<ObjectState> Objects;
};

struct InputCommand {
	std::uint32_t ClientTick = 0;
	std::uint64_t HeldElements = 0;
	std::int32_t MouseDeltaX = 0;
	std::int32_t MouseDeltaY = 0;
	std::int16_t MouseWheelDelta = 0;
	std::uint8_t MouseButtonsHeld = 0;
	std::int16_t AnalogMoveX = 0;
	std::int16_t AnalogMoveY = 0;
	std::int16_t AnalogAimX = 0;
	std::int16_t AnalogAimY = 0;
	bool ResetActivityVote = false;
	bool RestartActivityVote = false;
};

struct ClientAssignment {
	std::int8_t PlayerSlot = -1; // -1 means connected as a spectator.
};

struct TerrainPatch {
	std::uint32_t SceneRevision = 0;
	std::uint32_t X = 0;
	std::uint32_t Y = 0;
	std::uint32_t Width = 0;
	std::uint32_t Height = 0;
	TerrainLayer Layer = TerrainLayer::Material;
	std::vector<std::uint8_t> Pixels;
};

namespace Detail {
	inline void WriteUnsigned(std::vector<std::uint8_t>& output, std::uint64_t value, std::size_t byteCount) {
		for (std::size_t byte = 0; byte < byteCount; ++byte) {
			output.push_back(static_cast<std::uint8_t>((value >> (byte * 8)) & 0xFF));
		}
	}

	inline bool ReadUnsigned(std::span<const std::uint8_t> input, std::size_t& offset, std::size_t byteCount, std::uint64_t& value) {
		if (offset > input.size() || byteCount > input.size() - offset) {
			return false;
		}
		value = 0;
		for (std::size_t byte = 0; byte < byteCount; ++byte) {
			value |= static_cast<std::uint64_t>(input[offset++]) << (byte * 8);
		}
		return true;
	}

	inline void WriteFloat(std::vector<std::uint8_t>& output, float value) {
		WriteUnsigned(output, std::bit_cast<std::uint32_t>(value), sizeof(value));
	}

	inline bool ReadFloat(std::span<const std::uint8_t> input, std::size_t& offset, float& value) {
		std::uint64_t bits = 0;
		if (!ReadUnsigned(input, offset, sizeof(value), bits)) {
			return false;
		}
		value = std::bit_cast<float>(static_cast<std::uint32_t>(bits));
		return std::isfinite(value);
	}

	inline bool WriteString(std::vector<std::uint8_t>& output, const std::string& value) {
		if (value.size() > c_MaxStringBytes || !UTF8::IsValid(value)) {
			return false;
		}
		WriteUnsigned(output, value.size(), sizeof(std::uint16_t));
		output.insert(output.end(), value.begin(), value.end());
		return true;
	}

	inline bool ReadString(std::span<const std::uint8_t> input, std::size_t& offset, std::string& value) {
		std::uint64_t length = 0;
		if (!ReadUnsigned(input, offset, sizeof(std::uint16_t), length) || length > c_MaxStringBytes || offset > input.size() || length > input.size() - offset) {
			return false;
		}
		value.assign(reinterpret_cast<const char*>(input.data() + offset), static_cast<std::size_t>(length));
		offset += static_cast<std::size_t>(length);
		return UTF8::IsValid(value);
	}

	inline bool ValidObject(const ObjectState& object) {
		return object.NetworkId != 0 && !object.ClassName.empty() && object.ClassName.size() <= c_MaxStringBytes &&
		       object.ModuleName.size() <= c_MaxStringBytes && object.PresetName.size() <= c_MaxStringBytes &&
		       UTF8::IsValid(object.ClassName) && UTF8::IsValid(object.ModuleName) && UTF8::IsValid(object.PresetName) &&
		       std::isfinite(object.PositionX) && std::isfinite(object.PositionY) && std::isfinite(object.VelocityX) &&
		       std::isfinite(object.VelocityY) && std::isfinite(object.Rotation) && std::isfinite(object.AngularVelocity) &&
		       std::isfinite(object.Health) && object.Team >= c_NoTeam && object.Team < c_MaxTeamCount &&
		       (object.Flags == c_ObjectFlagActor || object.Flags == c_ObjectFlagItem || object.Flags == c_ObjectFlagParticle || object.Flags == c_ObjectFlagTransientPixel) &&
		       object.PixelColorIndex <= 255 && std::isfinite(object.PixelMass) && object.PixelMass >= 0.0F && std::isfinite(object.PixelSharpness);
	}
} // namespace Detail

inline bool EncodeTerrainPatch(const TerrainPatch& patch, std::uint32_t sequence, std::vector<std::uint8_t>& packet) {
	if (patch.SceneRevision == 0 || patch.Width == 0 || patch.Height == 0 || patch.Width > c_MaxTerrainPatchBytes / patch.Height ||
	    patch.Pixels.size() != static_cast<std::size_t>(patch.Width) * patch.Height || patch.Pixels.size() > c_MaxTerrainPatchBytes ||
	    static_cast<std::uint8_t>(patch.Layer) > static_cast<std::uint8_t>(TerrainLayer::Background)) {
		return false;
	}
	packet.clear();
	packet.reserve(c_HeaderSize + c_TerrainPatchHeaderSize + patch.Pixels.size());
	Detail::WriteUnsigned(packet, c_Magic, sizeof(c_Magic));
	Detail::WriteUnsigned(packet, c_Version, sizeof(c_Version));
	Detail::WriteUnsigned(packet, static_cast<std::uint8_t>(MessageType::TerrainPatch), sizeof(std::uint8_t));
	Detail::WriteUnsigned(packet, 0, sizeof(std::uint8_t));
	Detail::WriteUnsigned(packet, sequence, sizeof(sequence));
	Detail::WriteUnsigned(packet, c_TerrainPatchHeaderSize + patch.Pixels.size(), sizeof(std::uint32_t));
	Detail::WriteUnsigned(packet, patch.SceneRevision, sizeof(patch.SceneRevision));
	Detail::WriteUnsigned(packet, patch.X, sizeof(patch.X));
	Detail::WriteUnsigned(packet, patch.Y, sizeof(patch.Y));
	Detail::WriteUnsigned(packet, patch.Width, sizeof(patch.Width));
	Detail::WriteUnsigned(packet, patch.Height, sizeof(patch.Height));
	Detail::WriteUnsigned(packet, static_cast<std::uint8_t>(patch.Layer), sizeof(std::uint8_t));
	Detail::WriteUnsigned(packet, patch.Pixels.size(), sizeof(std::uint32_t));
	packet.insert(packet.end(), patch.Pixels.begin(), patch.Pixels.end());
	return packet.size() <= c_MaxPacketSize;
}

inline bool DecodeTerrainPatch(std::span<const std::uint8_t> packet, TerrainPatch& patch, std::uint32_t* sequence = nullptr) {
	if (packet.size() < c_HeaderSize + c_TerrainPatchHeaderSize || packet.size() > c_MaxPacketSize) {
		return false;
	}
	std::size_t offset = 0;
	std::uint64_t magic = 0, version = 0, messageType = 0, reserved = 0, sequenceValue = 0, payloadSize = 0;
	if (!Detail::ReadUnsigned(packet, offset, sizeof(c_Magic), magic) || magic != c_Magic ||
	    !Detail::ReadUnsigned(packet, offset, sizeof(c_Version), version) || version != c_Version ||
	    !Detail::ReadUnsigned(packet, offset, sizeof(std::uint8_t), messageType) || messageType != static_cast<std::uint8_t>(MessageType::TerrainPatch) ||
	    !Detail::ReadUnsigned(packet, offset, sizeof(std::uint8_t), reserved) || reserved != 0 ||
	    !Detail::ReadUnsigned(packet, offset, sizeof(std::uint32_t), sequenceValue) ||
	    !Detail::ReadUnsigned(packet, offset, sizeof(std::uint32_t), payloadSize) || payloadSize != packet.size() - c_HeaderSize) {
		return false;
	}
	TerrainPatch decoded;
	std::uint64_t value = 0;
	if (!Detail::ReadUnsigned(packet, offset, sizeof(decoded.SceneRevision), value)) return false;
	decoded.SceneRevision = static_cast<std::uint32_t>(value);
	if (!Detail::ReadUnsigned(packet, offset, sizeof(decoded.X), value)) return false;
	decoded.X = static_cast<std::uint32_t>(value);
	if (!Detail::ReadUnsigned(packet, offset, sizeof(decoded.Y), value)) return false;
	decoded.Y = static_cast<std::uint32_t>(value);
	if (!Detail::ReadUnsigned(packet, offset, sizeof(decoded.Width), value)) return false;
	decoded.Width = static_cast<std::uint32_t>(value);
	if (!Detail::ReadUnsigned(packet, offset, sizeof(decoded.Height), value)) return false;
	decoded.Height = static_cast<std::uint32_t>(value);
	if (!Detail::ReadUnsigned(packet, offset, sizeof(std::uint8_t), value) || value > static_cast<std::uint8_t>(TerrainLayer::Background)) return false;
	decoded.Layer = static_cast<TerrainLayer>(value);
	if (!Detail::ReadUnsigned(packet, offset, sizeof(std::uint32_t), value) || decoded.SceneRevision == 0 || decoded.Width == 0 || decoded.Height == 0 ||
	    decoded.Width > c_MaxTerrainPatchBytes / decoded.Height || value != static_cast<std::size_t>(decoded.Width) * decoded.Height ||
	    value > c_MaxTerrainPatchBytes || value != packet.size() - offset) {
		return false;
	}
	decoded.Pixels.assign(packet.begin() + static_cast<std::ptrdiff_t>(offset), packet.end());
	patch = std::move(decoded);
	if (sequence) {
		*sequence = static_cast<std::uint32_t>(sequenceValue);
	}
	return true;
}

inline bool EncodeClientAssignment(const ClientAssignment& assignment, std::uint32_t sequence, std::vector<std::uint8_t>& packet) {
	if (assignment.PlayerSlot < -1 || assignment.PlayerSlot >= c_PlayerSlotCount) {
		return false;
	}
	packet.clear();
	packet.reserve(c_HeaderSize + 1);
	Detail::WriteUnsigned(packet, c_Magic, sizeof(c_Magic));
	Detail::WriteUnsigned(packet, c_Version, sizeof(c_Version));
	Detail::WriteUnsigned(packet, static_cast<std::uint8_t>(MessageType::ClientAssignment), sizeof(std::uint8_t));
	Detail::WriteUnsigned(packet, 0, sizeof(std::uint8_t));
	Detail::WriteUnsigned(packet, sequence, sizeof(sequence));
	Detail::WriteUnsigned(packet, 1, sizeof(std::uint32_t));
	Detail::WriteUnsigned(packet, assignment.PlayerSlot < 0 ? 0xFF : static_cast<std::uint8_t>(assignment.PlayerSlot), sizeof(std::uint8_t));
	return true;
}

inline bool DecodeClientAssignment(std::span<const std::uint8_t> packet, ClientAssignment& assignment, std::uint32_t* sequence = nullptr) {
	if (packet.size() != c_HeaderSize + 1) {
		return false;
	}
	std::size_t offset = 0;
	std::uint64_t magic = 0, version = 0, messageType = 0, reserved = 0, sequenceValue = 0, payloadSize = 0, slot = 0;
	if (!Detail::ReadUnsigned(packet, offset, sizeof(c_Magic), magic) || magic != c_Magic ||
	    !Detail::ReadUnsigned(packet, offset, sizeof(c_Version), version) || version != c_Version ||
	    !Detail::ReadUnsigned(packet, offset, sizeof(std::uint8_t), messageType) || messageType != static_cast<std::uint8_t>(MessageType::ClientAssignment) ||
	    !Detail::ReadUnsigned(packet, offset, sizeof(std::uint8_t), reserved) || reserved != 0 ||
	    !Detail::ReadUnsigned(packet, offset, sizeof(std::uint32_t), sequenceValue) ||
	    !Detail::ReadUnsigned(packet, offset, sizeof(std::uint32_t), payloadSize) || payloadSize != 1 ||
	    !Detail::ReadUnsigned(packet, offset, sizeof(std::uint8_t), slot) || (slot != 0xFF && slot >= c_PlayerSlotCount)) {
		return false;
	}
	assignment.PlayerSlot = slot == 0xFF ? -1 : static_cast<std::int8_t>(slot);
	if (sequence) {
		*sequence = static_cast<std::uint32_t>(sequenceValue);
	}
	return true;
}

inline bool EncodeInputCommand(const InputCommand& command, std::uint32_t sequence, std::vector<std::uint8_t>& packet) {
	constexpr std::uint64_t validElementMask = (std::uint64_t{1} << c_InputElementCount) - 1;
	if ((command.HeldElements & ~validElementMask) != 0 || command.MouseDeltaX < -c_MaxMouseDelta || command.MouseDeltaX > c_MaxMouseDelta ||
	    command.MouseDeltaY < -c_MaxMouseDelta || command.MouseDeltaY > c_MaxMouseDelta ||
	    command.MouseWheelDelta < -c_MaxMouseWheelDelta || command.MouseWheelDelta > c_MaxMouseWheelDelta ||
	    command.AnalogMoveX == std::numeric_limits<std::int16_t>::min() || command.AnalogMoveY == std::numeric_limits<std::int16_t>::min() ||
	    command.AnalogAimX == std::numeric_limits<std::int16_t>::min() || command.AnalogAimY == std::numeric_limits<std::int16_t>::min()) {
		return false;
	}
	std::vector<std::uint8_t> payload;
	payload.reserve(32);
	Detail::WriteUnsigned(payload, command.ClientTick, sizeof(command.ClientTick));
	Detail::WriteUnsigned(payload, command.HeldElements, sizeof(command.HeldElements));
	Detail::WriteUnsigned(payload, std::bit_cast<std::uint32_t>(command.MouseDeltaX), sizeof(command.MouseDeltaX));
	Detail::WriteUnsigned(payload, std::bit_cast<std::uint32_t>(command.MouseDeltaY), sizeof(command.MouseDeltaY));
	Detail::WriteUnsigned(payload, std::bit_cast<std::uint16_t>(command.MouseWheelDelta), sizeof(command.MouseWheelDelta));
	Detail::WriteUnsigned(payload, command.MouseButtonsHeld, sizeof(command.MouseButtonsHeld));
	Detail::WriteUnsigned(payload, std::bit_cast<std::uint16_t>(command.AnalogMoveX), sizeof(command.AnalogMoveX));
	Detail::WriteUnsigned(payload, std::bit_cast<std::uint16_t>(command.AnalogMoveY), sizeof(command.AnalogMoveY));
	Detail::WriteUnsigned(payload, std::bit_cast<std::uint16_t>(command.AnalogAimX), sizeof(command.AnalogAimX));
	Detail::WriteUnsigned(payload, std::bit_cast<std::uint16_t>(command.AnalogAimY), sizeof(command.AnalogAimY));
	const std::uint8_t votes = static_cast<std::uint8_t>((command.ResetActivityVote ? 1 : 0) | (command.RestartActivityVote ? 2 : 0));
	Detail::WriteUnsigned(payload, votes, sizeof(votes));
	packet.clear();
	Detail::WriteUnsigned(packet, c_Magic, sizeof(c_Magic));
	Detail::WriteUnsigned(packet, c_Version, sizeof(c_Version));
	Detail::WriteUnsigned(packet, static_cast<std::uint8_t>(MessageType::InputCommand), sizeof(std::uint8_t));
	Detail::WriteUnsigned(packet, 0, sizeof(std::uint8_t));
	Detail::WriteUnsigned(packet, sequence, sizeof(sequence));
	Detail::WriteUnsigned(packet, payload.size(), sizeof(std::uint32_t));
	packet.insert(packet.end(), payload.begin(), payload.end());
	return packet.size() <= c_MaxPacketSize;
}

inline bool DecodeInputCommand(std::span<const std::uint8_t> packet, InputCommand& command, std::uint32_t* sequence = nullptr) {
	if (packet.size() != c_HeaderSize + 32) return false;
	std::size_t offset = 0;
	std::uint64_t magic = 0, version = 0, messageType = 0, reserved = 0, sequenceValue = 0, payloadSize = 0, value = 0;
	if (!Detail::ReadUnsigned(packet, offset, sizeof(c_Magic), magic) || magic != c_Magic ||
	    !Detail::ReadUnsigned(packet, offset, sizeof(c_Version), version) || version != c_Version ||
	    !Detail::ReadUnsigned(packet, offset, sizeof(std::uint8_t), messageType) || messageType != static_cast<std::uint8_t>(MessageType::InputCommand) ||
	    !Detail::ReadUnsigned(packet, offset, sizeof(std::uint8_t), reserved) || reserved != 0 ||
	    !Detail::ReadUnsigned(packet, offset, sizeof(std::uint32_t), sequenceValue) ||
	    !Detail::ReadUnsigned(packet, offset, sizeof(std::uint32_t), payloadSize) || payloadSize != 32) return false;

	InputCommand decoded;
	if (!Detail::ReadUnsigned(packet, offset, sizeof(decoded.ClientTick), value)) return false;
	decoded.ClientTick = static_cast<std::uint32_t>(value);
	if (!Detail::ReadUnsigned(packet, offset, sizeof(decoded.HeldElements), value)) return false;
	decoded.HeldElements = value;
	constexpr std::uint64_t validElementMask = (std::uint64_t{1} << c_InputElementCount) - 1;
	if ((decoded.HeldElements & ~validElementMask) != 0 || !Detail::ReadUnsigned(packet, offset, sizeof(decoded.MouseDeltaX), value)) return false;
	decoded.MouseDeltaX = std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(value));
	if (!Detail::ReadUnsigned(packet, offset, sizeof(decoded.MouseDeltaY), value)) return false;
	decoded.MouseDeltaY = std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(value));
	if (!Detail::ReadUnsigned(packet, offset, sizeof(decoded.MouseWheelDelta), value)) return false;
	decoded.MouseWheelDelta = std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(value));
	if (!Detail::ReadUnsigned(packet, offset, sizeof(decoded.MouseButtonsHeld), value)) return false;
	decoded.MouseButtonsHeld = static_cast<std::uint8_t>(value);
	if (!Detail::ReadUnsigned(packet, offset, sizeof(decoded.AnalogMoveX), value)) return false;
	decoded.AnalogMoveX = std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(value));
	if (!Detail::ReadUnsigned(packet, offset, sizeof(decoded.AnalogMoveY), value)) return false;
	decoded.AnalogMoveY = std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(value));
	if (!Detail::ReadUnsigned(packet, offset, sizeof(decoded.AnalogAimX), value)) return false;
	decoded.AnalogAimX = std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(value));
	if (!Detail::ReadUnsigned(packet, offset, sizeof(decoded.AnalogAimY), value)) return false;
	decoded.AnalogAimY = std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(value));
	if (!Detail::ReadUnsigned(packet, offset, sizeof(std::uint8_t), value) || (value & ~std::uint64_t{3}) != 0 ||
	    decoded.MouseDeltaX < -c_MaxMouseDelta || decoded.MouseDeltaX > c_MaxMouseDelta ||
	    decoded.MouseDeltaY < -c_MaxMouseDelta || decoded.MouseDeltaY > c_MaxMouseDelta ||
	    decoded.MouseWheelDelta < -c_MaxMouseWheelDelta || decoded.MouseWheelDelta > c_MaxMouseWheelDelta ||
	    decoded.AnalogMoveX == std::numeric_limits<std::int16_t>::min() || decoded.AnalogMoveY == std::numeric_limits<std::int16_t>::min() ||
	    decoded.AnalogAimX == std::numeric_limits<std::int16_t>::min() || decoded.AnalogAimY == std::numeric_limits<std::int16_t>::min()) return false;
	decoded.ResetActivityVote = (value & 1) != 0;
	decoded.RestartActivityVote = (value & 2) != 0;
	command = decoded;
	if (sequence) *sequence = static_cast<std::uint32_t>(sequenceValue);
	return true;
}

inline bool EncodeSnapshot(const Snapshot& snapshot, std::uint32_t sequence, std::vector<std::uint8_t>& packet) {
	packet.clear();
	if (snapshot.Objects.size() > c_MaxObjects || snapshot.ActivityClassName.size() > c_MaxStringBytes || snapshot.ActivityPreset.size() > c_MaxStringBytes ||
	    snapshot.ActivityModuleName.size() > c_MaxStringBytes || snapshot.SceneModuleName.size() > c_MaxStringBytes || snapshot.ScenePreset.size() > c_MaxStringBytes ||
	    !UTF8::IsValid(snapshot.ActivityClassName) || !UTF8::IsValid(snapshot.ActivityPreset) || !UTF8::IsValid(snapshot.ActivityModuleName) ||
	    !UTF8::IsValid(snapshot.SceneModuleName) || !UTF8::IsValid(snapshot.ScenePreset)) {
		return false;
	}

	std::unordered_set<std::uint64_t> networkIds;
	networkIds.reserve(snapshot.Objects.size());
	std::vector<std::uint8_t> payload;
	payload.reserve(32 + snapshot.Objects.size() * 64);
	Detail::WriteUnsigned(payload, snapshot.Tick, sizeof(snapshot.Tick));
	Detail::WriteUnsigned(payload, snapshot.SceneRevision, sizeof(snapshot.SceneRevision));
	if (!Detail::WriteString(payload, snapshot.ActivityClassName) || !Detail::WriteString(payload, snapshot.ActivityPreset) ||
	    !Detail::WriteString(payload, snapshot.ActivityModuleName) || !Detail::WriteString(payload, snapshot.SceneModuleName) ||
	    !Detail::WriteString(payload, snapshot.ScenePreset)) {
		return false;
	}
	Detail::WriteUnsigned(payload, snapshot.Objects.size(), sizeof(std::uint32_t));
	for (const ObjectState& object : snapshot.Objects) {
		if (!Detail::ValidObject(object) || !networkIds.insert(object.NetworkId).second) {
			return false;
		}
		Detail::WriteUnsigned(payload, object.NetworkId, sizeof(object.NetworkId));
		if (!Detail::WriteString(payload, object.ClassName) || !Detail::WriteString(payload, object.ModuleName) || !Detail::WriteString(payload, object.PresetName)) {
			return false;
		}
		Detail::WriteFloat(payload, object.PositionX);
		Detail::WriteFloat(payload, object.PositionY);
		Detail::WriteFloat(payload, object.VelocityX);
		Detail::WriteFloat(payload, object.VelocityY);
		Detail::WriteFloat(payload, object.Rotation);
		Detail::WriteFloat(payload, object.AngularVelocity);
		Detail::WriteFloat(payload, object.Health);
		Detail::WriteUnsigned(payload, object.SpriteFrame, sizeof(object.SpriteFrame));
		Detail::WriteUnsigned(payload, static_cast<std::uint16_t>(object.Team), sizeof(object.Team));
		Detail::WriteUnsigned(payload, object.Flags, sizeof(object.Flags));
		Detail::WriteUnsigned(payload, object.HFlipped ? 1 : 0, sizeof(std::uint8_t));
		if (object.Flags == c_ObjectFlagTransientPixel) {
			Detail::WriteUnsigned(payload, object.PixelMaterialId, sizeof(object.PixelMaterialId));
			Detail::WriteUnsigned(payload, object.PixelColorIndex, sizeof(object.PixelColorIndex));
			Detail::WriteFloat(payload, object.PixelMass);
			Detail::WriteUnsigned(payload, object.PixelLifetime, sizeof(object.PixelLifetime));
			Detail::WriteFloat(payload, object.PixelSharpness);
		}
		if (payload.size() + c_HeaderSize > c_MaxPacketSize) {
			return false;
		}
	}

	packet.reserve(c_HeaderSize + payload.size());
	Detail::WriteUnsigned(packet, c_Magic, sizeof(c_Magic));
	Detail::WriteUnsigned(packet, c_Version, sizeof(c_Version));
	Detail::WriteUnsigned(packet, static_cast<std::uint8_t>(MessageType::WorldSnapshot), sizeof(std::uint8_t));
	Detail::WriteUnsigned(packet, 0, sizeof(std::uint8_t));
	Detail::WriteUnsigned(packet, sequence, sizeof(sequence));
	Detail::WriteUnsigned(packet, payload.size(), sizeof(std::uint32_t));
	packet.insert(packet.end(), payload.begin(), payload.end());
	return true;
}

inline bool DecodeSnapshot(std::span<const std::uint8_t> packet, Snapshot& snapshot, std::uint32_t* sequence = nullptr) {
	if (packet.size() < c_HeaderSize || packet.size() > c_MaxPacketSize) {
		return false;
	}
	std::size_t offset = 0;
	std::uint64_t magic = 0, version = 0, messageType = 0, reserved = 0, sequenceValue = 0, payloadSize = 0;
	if (!Detail::ReadUnsigned(packet, offset, sizeof(c_Magic), magic) || magic != c_Magic ||
	    !Detail::ReadUnsigned(packet, offset, sizeof(c_Version), version) || version != c_Version ||
	    !Detail::ReadUnsigned(packet, offset, sizeof(std::uint8_t), messageType) || messageType != static_cast<std::uint8_t>(MessageType::WorldSnapshot) ||
	    !Detail::ReadUnsigned(packet, offset, sizeof(std::uint8_t), reserved) || reserved != 0 ||
	    !Detail::ReadUnsigned(packet, offset, sizeof(std::uint32_t), sequenceValue) ||
	    !Detail::ReadUnsigned(packet, offset, sizeof(std::uint32_t), payloadSize) || payloadSize != packet.size() - c_HeaderSize) {
		return false;
	}

	Snapshot decoded;
	std::uint64_t value = 0;
	if (!Detail::ReadUnsigned(packet, offset, sizeof(decoded.Tick), value)) return false;
	decoded.Tick = static_cast<std::uint32_t>(value);
	if (!Detail::ReadUnsigned(packet, offset, sizeof(decoded.SceneRevision), value)) return false;
	decoded.SceneRevision = static_cast<std::uint32_t>(value);
	if (!Detail::ReadString(packet, offset, decoded.ActivityClassName) || !Detail::ReadString(packet, offset, decoded.ActivityPreset) ||
	    !Detail::ReadString(packet, offset, decoded.ActivityModuleName) || !Detail::ReadString(packet, offset, decoded.SceneModuleName) ||
	    !Detail::ReadString(packet, offset, decoded.ScenePreset) ||
	    !Detail::ReadUnsigned(packet, offset, sizeof(std::uint32_t), value) || value > c_MaxObjects) {
		return false;
	}
	const std::uint64_t objectCount = value;
	decoded.Objects.reserve(static_cast<std::size_t>(objectCount));
	std::unordered_set<std::uint64_t> networkIds;
	networkIds.reserve(static_cast<std::size_t>(objectCount));
	for (std::uint64_t index = 0; index < objectCount; ++index) {
		ObjectState object;
		if (!Detail::ReadUnsigned(packet, offset, sizeof(object.NetworkId), value)) return false;
		object.NetworkId = value;
		if (!Detail::ReadString(packet, offset, object.ClassName) || !Detail::ReadString(packet, offset, object.ModuleName) || !Detail::ReadString(packet, offset, object.PresetName) ||
		    !Detail::ReadFloat(packet, offset, object.PositionX) || !Detail::ReadFloat(packet, offset, object.PositionY) ||
		    !Detail::ReadFloat(packet, offset, object.VelocityX) || !Detail::ReadFloat(packet, offset, object.VelocityY) ||
		    !Detail::ReadFloat(packet, offset, object.Rotation) || !Detail::ReadFloat(packet, offset, object.AngularVelocity) ||
		    !Detail::ReadFloat(packet, offset, object.Health) || !Detail::ReadUnsigned(packet, offset, sizeof(object.SpriteFrame), value)) {
			return false;
		}
		object.SpriteFrame = static_cast<std::uint16_t>(value);
		if (!Detail::ReadUnsigned(packet, offset, sizeof(object.Team), value)) return false;
		object.Team = std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(value));
		if (!Detail::ReadUnsigned(packet, offset, sizeof(object.Flags), value)) return false;
		object.Flags = static_cast<std::uint16_t>(value);
		if (!Detail::ReadUnsigned(packet, offset, sizeof(std::uint8_t), value) || value > 1) return false;
		object.HFlipped = value != 0;
		if (object.Flags == c_ObjectFlagTransientPixel) {
			if (!Detail::ReadUnsigned(packet, offset, sizeof(object.PixelMaterialId), value)) return false;
			object.PixelMaterialId = static_cast<std::uint8_t>(value);
			if (!Detail::ReadUnsigned(packet, offset, sizeof(object.PixelColorIndex), value)) return false;
			object.PixelColorIndex = static_cast<std::uint16_t>(value);
			if (!Detail::ReadFloat(packet, offset, object.PixelMass) || !Detail::ReadUnsigned(packet, offset, sizeof(object.PixelLifetime), value)) return false;
			object.PixelLifetime = static_cast<std::uint32_t>(value);
			if (!Detail::ReadFloat(packet, offset, object.PixelSharpness)) return false;
		}
		if (!Detail::ValidObject(object) || !networkIds.insert(object.NetworkId).second) return false;
		decoded.Objects.push_back(std::move(object));
	}
	if (offset != packet.size()) {
		return false;
	}
	snapshot = std::move(decoded);
	if (sequence) *sequence = static_cast<std::uint32_t>(sequenceValue);
	return true;
}

} // namespace RTE::WorldStateProtocol
