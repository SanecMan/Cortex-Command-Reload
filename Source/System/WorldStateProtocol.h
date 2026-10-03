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
inline constexpr std::uint16_t c_Version = 1;
inline constexpr std::size_t c_HeaderSize = 16;
inline constexpr std::size_t c_MaxPacketSize = 4 * 1024 * 1024;
inline constexpr std::size_t c_MaxObjects = 65535;
inline constexpr std::size_t c_MaxStringBytes = 1024;
inline constexpr std::uint16_t c_ObjectFlagActor = 0x0001;
inline constexpr std::uint16_t c_ObjectFlagItem = 0x0002;
inline constexpr std::uint16_t c_ObjectFlagParticle = 0x0004;

enum class MessageType : std::uint8_t {
	ClientHello = 1,
	InputCommand = 2,
	WorldSnapshot = 3,
	Disconnect = 4
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
	std::int16_t Team = -1;
	std::uint16_t Flags = 0;
};

struct Snapshot {
	std::uint32_t Tick = 0;
	std::uint32_t SceneRevision = 0;
	std::string ActivityPreset;
	std::string ScenePreset;
	std::vector<ObjectState> Objects;
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
		       std::isfinite(object.Health);
	}
} // namespace Detail

inline bool EncodeSnapshot(const Snapshot& snapshot, std::uint32_t sequence, std::vector<std::uint8_t>& packet) {
	packet.clear();
	if (snapshot.Objects.size() > c_MaxObjects || snapshot.ActivityPreset.size() > c_MaxStringBytes || snapshot.ScenePreset.size() > c_MaxStringBytes ||
	    !UTF8::IsValid(snapshot.ActivityPreset) || !UTF8::IsValid(snapshot.ScenePreset)) {
		return false;
	}

	std::unordered_set<std::uint64_t> networkIds;
	networkIds.reserve(snapshot.Objects.size());
	std::vector<std::uint8_t> payload;
	payload.reserve(32 + snapshot.Objects.size() * 64);
	Detail::WriteUnsigned(payload, snapshot.Tick, sizeof(snapshot.Tick));
	Detail::WriteUnsigned(payload, snapshot.SceneRevision, sizeof(snapshot.SceneRevision));
	if (!Detail::WriteString(payload, snapshot.ActivityPreset) || !Detail::WriteString(payload, snapshot.ScenePreset)) {
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
		Detail::WriteUnsigned(payload, static_cast<std::uint16_t>(object.Team), sizeof(object.Team));
		Detail::WriteUnsigned(payload, object.Flags, sizeof(object.Flags));
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
	if (!Detail::ReadString(packet, offset, decoded.ActivityPreset) || !Detail::ReadString(packet, offset, decoded.ScenePreset) ||
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
		    !Detail::ReadFloat(packet, offset, object.Health) || !Detail::ReadUnsigned(packet, offset, sizeof(object.Team), value)) {
			return false;
		}
		object.Team = std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(value));
		if (!Detail::ReadUnsigned(packet, offset, sizeof(object.Flags), value)) return false;
		object.Flags = static_cast<std::uint16_t>(value);
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
