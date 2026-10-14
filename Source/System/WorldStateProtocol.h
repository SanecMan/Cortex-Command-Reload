#pragma once

#include "UTF8.h"
#include "WorldStateAttachment.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <string>
#include <unordered_set>
#include <unordered_map>
#include <utility>
#include <vector>

namespace RTE::WorldStateProtocol {

inline constexpr std::uint32_t c_Magic = 0x31524343; // "CCR1" on the wire.
inline constexpr std::uint16_t c_Version = 28;
inline constexpr std::size_t c_TeamCount = 4;
inline constexpr std::int8_t c_NoActivity = -1;
inline constexpr std::int8_t c_MaxActivityState = 6;
inline constexpr std::size_t c_HeaderSize = 16;
inline constexpr std::size_t c_MaxPacketSize = 4 * 1024 * 1024;
inline constexpr std::size_t c_MaxObjects = 65535;
inline constexpr std::size_t c_MaxStringBytes = 1024;
inline constexpr std::size_t c_MaxScreenTextBytes = 16384;
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
	TerrainPatch = 6,
	PurchaseRequest = 7,
	PurchaseResult = 8,
	PurchasePolicy = 9,
	SoundEvents = 10
};

enum class TerrainLayer : std::uint8_t {
	Material = 0,
	Foreground = 1,
	Background = 2
};

struct HandDeviceState {
	bool Present = false;
	std::string ClassName, ModuleName, PresetName;
	bool HasMagazine = false;
	std::string MagazineClassName, MagazineModuleName, MagazinePresetName;
	std::int32_t MagazineRounds = 0;
	bool operator==(const HandDeviceState&) const = default;
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
	bool HasEquippedDevice = false;
	std::string EquippedDeviceClassName;
	std::string EquippedDeviceModuleName;
	std::string EquippedDevicePresetName;
	bool HasEquippedDeviceMagazine = false;
	std::string EquippedDeviceMagazineClassName;
	std::string EquippedDeviceMagazineModuleName;
	std::string EquippedDeviceMagazinePresetName;
	std::int32_t EquippedDeviceMagazineRounds = 0;
	bool HasOwnMagazine=false;
	std::string OwnMagazineClassName, OwnMagazineModuleName, OwnMagazinePresetName;
	std::int32_t OwnMagazineRounds=0;
	HandDeviceState BackgroundDevice;
	bool HasAttachmentState = false;
	std::vector<AttachmentState> Attachments;
	bool HasEmitterState = false, EmissionEnabled = false, FlashVisible = false;
	DeviceVisualState DeviceVisual;
	float AimAngle = 0;
	std::uint8_t ActorStatus = 0;
	bool operator==(const ObjectState&) const = default;
};

struct PlayerState {
	std::uint64_t ControlledActorId = 0;
	float CameraX = 0.0F;
	float CameraY = 0.0F;
	std::int16_t Team = c_NoTeam;
	bool ActiveHuman = false;
	std::string ScreenText;
	std::uint32_t TextBlinkInterval = 0;
	bool TextCentered = false;
	bool operator==(const PlayerState&) const = default;
};

struct InventoryItemState {
	std::uint64_t ParentNetworkId=0;
	std::uint16_t Slot=0;
	ObjectState Object;
	bool operator==(const InventoryItemState&) const = default;
};
inline constexpr std::size_t c_MaxInventoryItems=8192, c_MaxActorInventory=512, c_MaxInventoryDepth=16;

struct Snapshot {
	std::uint32_t Tick = 0;
	std::uint32_t SceneRevision = 0;
	std::int8_t ActivityState = c_NoActivity;
	std::array<float, c_TeamCount> TeamFunds{};
	std::string ActivityClassName;
	std::string ActivityPreset;
	std::string ActivityModuleName;
	std::string SceneModuleName;
	std::string ScenePreset;
	std::vector<ObjectState> Objects;
	std::vector<InventoryItemState> Inventory;
	bool InventoryComplete=true; // Capture rejects oversized/malformed ownership trees instead of truncating them.
	std::array<PlayerState, c_PlayerSlotCount> Players{};
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

	inline bool WriteString(std::vector<std::uint8_t>& output, const std::string& value, std::size_t maxBytes = c_MaxStringBytes) {
		if (value.size() > maxBytes || value.size() > 65535 || !UTF8::IsValid(value)) {
			return false;
		}
		WriteUnsigned(output, value.size(), sizeof(std::uint16_t));
		output.insert(output.end(), value.begin(), value.end());
		return true;
	}

	inline bool ReadString(std::span<const std::uint8_t> input, std::size_t& offset, std::string& value, std::size_t maxBytes = c_MaxStringBytes) {
		std::uint64_t length = 0;
		if (!ReadUnsigned(input, offset, sizeof(std::uint16_t), length) || length > maxBytes || offset > input.size() || length > input.size() - offset) {
			return false;
		}
		value.assign(reinterpret_cast<const char*>(input.data() + offset), static_cast<std::size_t>(length));
		offset += static_cast<std::size_t>(length);
		return UTF8::IsValid(value);
	}

	inline bool ValidHandDevice(const HandDeviceState& device) {
		for (const auto* value : {&device.ClassName, &device.ModuleName, &device.PresetName,
		                         &device.MagazineClassName, &device.MagazineModuleName, &device.MagazinePresetName}) {
			if (value->size() > c_MaxStringBytes || !UTF8::IsValid(*value)) return false;
		}
		if (!device.Present) return device == HandDeviceState{};
		if (device.ClassName.empty() || device.ModuleName.empty() || device.PresetName.empty()) return false;
		if (!device.HasMagazine) return device.MagazineClassName.empty() && device.MagazineModuleName.empty() &&
		    device.MagazinePresetName.empty() && device.MagazineRounds == 0;
		return device.ClassName == "HDFirearm" && device.MagazineClassName == "Magazine" &&
		    !device.MagazineModuleName.empty() && !device.MagazinePresetName.empty() && device.MagazineRounds >= -1;
	}

	inline bool ValidObject(const ObjectState& object) {
		if (!ValidDeviceVisual(object.DeviceVisual, object.ClassName)) return false;
		const bool equipmentIdentityValid = object.EquippedDeviceClassName.size() <= c_MaxStringBytes &&
		                                   object.EquippedDeviceModuleName.size() <= c_MaxStringBytes &&
		                                   object.EquippedDevicePresetName.size() <= c_MaxStringBytes &&
		                                   object.EquippedDeviceMagazineClassName.size() <= c_MaxStringBytes &&
		                                   object.EquippedDeviceMagazineModuleName.size() <= c_MaxStringBytes &&
		                                   object.EquippedDeviceMagazinePresetName.size() <= c_MaxStringBytes &&
		                                   UTF8::IsValid(object.EquippedDeviceClassName) && UTF8::IsValid(object.EquippedDeviceModuleName) &&
		                                   UTF8::IsValid(object.EquippedDevicePresetName) && UTF8::IsValid(object.EquippedDeviceMagazineClassName) &&
		                                   UTF8::IsValid(object.EquippedDeviceMagazineModuleName) && UTF8::IsValid(object.EquippedDeviceMagazinePresetName);
		const bool noEquipmentData = !object.HasEquippedDevice && !object.HasEquippedDeviceMagazine && object.EquippedDeviceClassName.empty() &&
		                            object.EquippedDeviceModuleName.empty() && object.EquippedDevicePresetName.empty() &&
		                            object.EquippedDeviceMagazineClassName.empty() && object.EquippedDeviceMagazineModuleName.empty() &&
		                            object.EquippedDeviceMagazinePresetName.empty() && object.EquippedDeviceMagazineRounds == 0;
		const bool noMagazineData = !object.HasEquippedDeviceMagazine && object.EquippedDeviceMagazineClassName.empty() &&
		                            object.EquippedDeviceMagazineModuleName.empty() && object.EquippedDeviceMagazinePresetName.empty() &&
		                            object.EquippedDeviceMagazineRounds == 0;
		const bool deviceDataValid = object.HasEquippedDevice && !object.EquippedDeviceClassName.empty() && !object.EquippedDevicePresetName.empty() &&
		                             (noMagazineData || (object.HasEquippedDeviceMagazine && object.EquippedDeviceClassName == "HDFirearm" &&
		                                                 !object.EquippedDeviceMagazineClassName.empty() && !object.EquippedDeviceMagazinePresetName.empty()));
		const bool equipmentFieldsValid = equipmentIdentityValid &&
		                                  (object.Flags == c_ObjectFlagActor ? (noEquipmentData || deviceDataValid) : noEquipmentData);
		return object.NetworkId != 0 && !object.ClassName.empty() && object.ClassName.size() <= c_MaxStringBytes &&
		       object.ModuleName.size() <= c_MaxStringBytes && object.PresetName.size() <= c_MaxStringBytes &&
		       UTF8::IsValid(object.ClassName) && UTF8::IsValid(object.ModuleName) && UTF8::IsValid(object.PresetName) &&
		       std::isfinite(object.PositionX) && std::isfinite(object.PositionY) && std::isfinite(object.VelocityX) &&
		       std::isfinite(object.VelocityY) && std::isfinite(object.Rotation) && std::isfinite(object.AngularVelocity) &&
		       std::isfinite(object.Health) && std::isfinite(object.AimAngle) && object.ActorStatus <= 4 &&
		       (object.HasEmitterState ? (object.ClassName == "AEmitter" || object.ClassName == "AEJetpack") : (!object.EmissionEnabled && !object.FlashVisible)) &&
		       (!object.FlashVisible || object.EmissionEnabled) &&
		       (object.Flags == c_ObjectFlagActor || (object.AimAngle == 0 && object.ActorStatus == 0)) &&
		       (object.HasAttachmentState ? ValidAttachmentTree(object.Attachments, c_MaxStringBytes) : object.Attachments.empty()) &&
		       (object.Flags != c_ObjectFlagTransientPixel || !object.HasAttachmentState) &&
		       object.Team >= c_NoTeam && object.Team < c_MaxTeamCount &&
		       (object.Flags == c_ObjectFlagActor || object.Flags == c_ObjectFlagItem || object.Flags == c_ObjectFlagParticle || object.Flags == c_ObjectFlagTransientPixel) &&
		       object.PixelColorIndex <= 255 && std::isfinite(object.PixelMass) && object.PixelMass >= 0.0F && std::isfinite(object.PixelSharpness) && equipmentFieldsValid &&
		       ValidHandDevice(object.BackgroundDevice) &&
		       ((object.ClassName == "AHuman" && object.Flags == c_ObjectFlagActor) || object.BackgroundDevice == HandDeviceState{});
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

namespace Detail {
inline void WriteDeviceVisual(std::vector<std::uint8_t>& packet, const DeviceVisualState& state) {
	WriteUnsigned(packet, (state.Present ? 1 : 0) | (state.Activated ? 2 : 0) | (state.Reloading ? 4 : 0) | (state.FireFrame ? 8 : 0) | (state.Recoiled ? 16 : 0), 1);
	if (state.Present) WriteFloat(packet, state.ReloadProgress);
	if (state.Recoiled) { WriteFloat(packet, state.RecoilOffsetX); WriteFloat(packet, state.RecoilOffsetY); }
	if (state.Present) { WriteUnsigned(packet, state.ShotSequence, 4); WriteUnsigned(packet, state.ShotAgeMS, 2); }
}
inline bool ReadDeviceVisual(std::span<const std::uint8_t> packet, std::size_t& offset, DeviceVisualState& state) {
	std::uint64_t flags = 0;
	if (!ReadUnsigned(packet, offset, 1, flags) || flags > 31 || (!(flags & 1) && flags)) return false;
	state = {};
	state.Present = (flags & 1) != 0; state.Activated = (flags & 2) != 0; state.Reloading = (flags & 4) != 0;
	state.FireFrame = (flags & 8) != 0; state.Recoiled = (flags & 16) != 0;
	if (state.Present && !ReadFloat(packet, offset, state.ReloadProgress)) return false;
	if (state.Recoiled && (!ReadFloat(packet, offset, state.RecoilOffsetX) || !ReadFloat(packet, offset, state.RecoilOffsetY))) return false;
	if (state.Present) {
		std::uint64_t sequence = 0, age = 0;
		if (!ReadUnsigned(packet, offset, 4, sequence) || !ReadUnsigned(packet, offset, 2, age)) return false;
		state.ShotSequence = static_cast<std::uint32_t>(sequence); state.ShotAgeMS = static_cast<std::uint16_t>(age);
	}
	return true;
}
inline bool WriteAttachments(std::vector<std::uint8_t>& packet, const ObjectState& object) {
	WriteUnsigned(packet, object.HasAttachmentState, 1);
	if (!object.HasAttachmentState) return true;
	WriteUnsigned(packet, object.Attachments.size(), 2);
	for (const auto& node : object.Attachments) {
		WriteUnsigned(packet, node.Parent, 2);
		WriteUnsigned(packet, node.Slot, 2);
		WriteUnsigned(packet, static_cast<std::uint8_t>(node.Role), 1);
		if (!WriteString(packet, node.ClassName) || !WriteString(packet, node.ModuleName) || !WriteString(packet, node.PresetName)) return false;
		for (float number : {node.ParentOffsetX, node.ParentOffsetY, node.JointOffsetX, node.JointOffsetY,
		                    node.RotationFromRoot, node.HandOffsetX, node.HandOffsetY, node.Scale}) WriteFloat(packet, number);
		WriteUnsigned(packet, node.SpriteFrame, 2);
		WriteUnsigned(packet, (node.HFlipped ? 1 : 0) | (node.DrawAfterParent ? 2 : 0) | (node.HasEmitterState ? 4 : 0) | (node.EmissionEnabled ? 8 : 0) | (node.FlashVisible ? 16 : 0), 1);
		WriteDeviceVisual(packet, node.DeviceVisual);
		WriteUnsigned(packet, std::bit_cast<std::uint32_t>(node.MagazineRounds), 4);
		if (packet.size() > c_MaxPacketSize) return false;
	}
	return true;
}
inline bool ReadAttachments(std::span<const std::uint8_t> packet, std::size_t& offset, ObjectState& object) {
	std::uint64_t value = 0;
	if (!ReadUnsigned(packet, offset, 1, value) || value > 1) return false;
	object.HasAttachmentState = value != 0;
	object.Attachments.clear();
	if (!object.HasAttachmentState) return true;
	if (!ReadUnsigned(packet, offset, 2, value) || value > c_MaxAttachments) return false;
	const auto count = static_cast<std::size_t>(value);
	object.Attachments.reserve(count);
	for (std::size_t index = 0; index < count; ++index) {
		AttachmentState node;
		if (!ReadUnsigned(packet, offset, 2, value)) return false;
		node.Parent = static_cast<std::uint16_t>(value);
		if (!ReadUnsigned(packet, offset, 2, value)) return false;
		node.Slot = static_cast<std::uint16_t>(value);
		if (!ReadUnsigned(packet, offset, 1, value)) return false;
		node.Role = static_cast<AttachmentRole>(value);
		if (!ReadString(packet, offset, node.ClassName) || !ReadString(packet, offset, node.ModuleName) || !ReadString(packet, offset, node.PresetName)) return false;
		for (float* number : {&node.ParentOffsetX, &node.ParentOffsetY, &node.JointOffsetX, &node.JointOffsetY,
		                     &node.RotationFromRoot, &node.HandOffsetX, &node.HandOffsetY, &node.Scale})
			if (!ReadFloat(packet, offset, *number)) return false;
		if (!ReadUnsigned(packet, offset, 2, value)) return false;
		node.SpriteFrame = static_cast<std::uint16_t>(value);
		if (!ReadUnsigned(packet, offset, 1, value) || value > 31) return false;
		node.HasEmitterState = (value & 4) != 0; node.EmissionEnabled = (value & 8) != 0; node.FlashVisible = (value & 16) != 0;
		node.HFlipped = (value & 1) != 0; node.DrawAfterParent = (value & 2) != 0;
		if (!ReadDeviceVisual(packet, offset, node.DeviceVisual)) return false;
		if (!ReadUnsigned(packet, offset, 4, value)) return false;
		node.MagazineRounds = std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(value));
		object.Attachments.push_back(std::move(node));
	}
	return ValidAttachmentTree(object.Attachments, c_MaxStringBytes);
}
inline bool WriteObject(std::vector<std::uint8_t>& packet, const ObjectState& object) {
	if (!ValidObject(object)) return false;
	if ((object.HasOwnMagazine && object.ClassName!="HDFirearm") || (!object.HasOwnMagazine &&
	    (!object.OwnMagazineClassName.empty() || !object.OwnMagazineModuleName.empty() || !object.OwnMagazinePresetName.empty() || object.OwnMagazineRounds!=0))) return false;
	Detail::WriteUnsigned(packet, object.NetworkId, sizeof(object.NetworkId));
	if (!Detail::WriteString(packet, object.ClassName) || !Detail::WriteString(packet, object.ModuleName) || !Detail::WriteString(packet, object.PresetName)) {
		return false;
	}
	Detail::WriteFloat(packet, object.PositionX);
	Detail::WriteFloat(packet, object.PositionY);
	Detail::WriteFloat(packet, object.VelocityX);
	Detail::WriteFloat(packet, object.VelocityY);
	Detail::WriteFloat(packet, object.Rotation);
	Detail::WriteFloat(packet, object.AngularVelocity);
	Detail::WriteFloat(packet, object.Health);
	Detail::WriteUnsigned(packet, object.SpriteFrame, sizeof(object.SpriteFrame));
	Detail::WriteUnsigned(packet, static_cast<std::uint16_t>(object.Team), sizeof(object.Team));
	Detail::WriteUnsigned(packet, object.Flags, sizeof(object.Flags));
	Detail::WriteUnsigned(packet, object.HFlipped ? 1 : 0, sizeof(std::uint8_t));
	if (object.Flags == c_ObjectFlagTransientPixel) {
		Detail::WriteUnsigned(packet, object.PixelMaterialId, sizeof(object.PixelMaterialId));
		Detail::WriteUnsigned(packet, object.PixelColorIndex, sizeof(object.PixelColorIndex));
		Detail::WriteFloat(packet, object.PixelMass);
		Detail::WriteUnsigned(packet, object.PixelLifetime, sizeof(object.PixelLifetime));
		Detail::WriteFloat(packet, object.PixelSharpness);
	}
	if (object.Flags == c_ObjectFlagActor) {
		Detail::WriteFloat(packet, object.AimAngle);
		Detail::WriteUnsigned(packet, object.ActorStatus, 1);
		Detail::WriteUnsigned(packet, object.HasEquippedDevice ? 1 : 0, sizeof(std::uint8_t));
		if (object.HasEquippedDevice) {
		if (!Detail::WriteString(packet, object.EquippedDeviceClassName) || !Detail::WriteString(packet, object.EquippedDeviceModuleName) ||
		    !Detail::WriteString(packet, object.EquippedDevicePresetName)) {
			return false;
		}
		Detail::WriteUnsigned(packet, object.HasEquippedDeviceMagazine ? 1 : 0, sizeof(std::uint8_t));
		if (object.HasEquippedDeviceMagazine) {
			if (!Detail::WriteString(packet, object.EquippedDeviceMagazineClassName) || !Detail::WriteString(packet, object.EquippedDeviceMagazineModuleName) ||
			    !Detail::WriteString(packet, object.EquippedDeviceMagazinePresetName)) {
			return false;
			}
			Detail::WriteUnsigned(packet, std::bit_cast<std::uint32_t>(object.EquippedDeviceMagazineRounds), sizeof(std::uint32_t));
		}
		}
	}
	if (object.ClassName == "AHuman") {
		const auto& device = object.BackgroundDevice;
		Detail::WriteUnsigned(packet, device.Present, 1);
		if (device.Present) {
			if (!Detail::WriteString(packet, device.ClassName) || !Detail::WriteString(packet, device.ModuleName) ||
			    !Detail::WriteString(packet, device.PresetName)) return false;
			Detail::WriteUnsigned(packet, device.HasMagazine, 1);
			if (device.HasMagazine) {
				if (!Detail::WriteString(packet, device.MagazineClassName) || !Detail::WriteString(packet, device.MagazineModuleName) ||
				    !Detail::WriteString(packet, device.MagazinePresetName)) return false;
				Detail::WriteUnsigned(packet, std::bit_cast<std::uint32_t>(device.MagazineRounds), 4);
			}
		}
	}
	if (object.ClassName=="HDFirearm") {
		Detail::WriteUnsigned(packet,object.HasOwnMagazine,1);
		if (object.HasOwnMagazine) {
			if (object.OwnMagazineClassName!="Magazine" || object.OwnMagazineModuleName.empty() || object.OwnMagazinePresetName.empty() || object.OwnMagazineRounds < -1 ||
			    !Detail::WriteString(packet,object.OwnMagazineClassName) || !Detail::WriteString(packet,object.OwnMagazineModuleName) || !Detail::WriteString(packet,object.OwnMagazinePresetName)) return false;
			Detail::WriteUnsigned(packet,std::bit_cast<std::uint32_t>(object.OwnMagazineRounds),4);
		}
	}
	WriteUnsigned(packet, (object.HasEmitterState ? 1 : 0) | (object.EmissionEnabled ? 2 : 0) | (object.FlashVisible ? 4 : 0), 1);
	WriteDeviceVisual(packet, object.DeviceVisual);
	if (!WriteAttachments(packet, object) || packet.size() > c_MaxPacketSize) {
		return false;
	}
	return true;
}
inline bool ReadObject(std::span<const std::uint8_t> packet, std::size_t& offset, ObjectState& object) {
	std::uint64_t value=0;
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
	if (object.Flags == c_ObjectFlagActor) {
		if (!Detail::ReadFloat(packet, offset, object.AimAngle) || !Detail::ReadUnsigned(packet, offset, 1, value)) return false;
		object.ActorStatus = static_cast<std::uint8_t>(value);
		if (!Detail::ReadUnsigned(packet, offset, sizeof(std::uint8_t), value) || value > 1) return false;
		object.HasEquippedDevice = value != 0;
		if (object.HasEquippedDevice) {
		if (!Detail::ReadString(packet, offset, object.EquippedDeviceClassName) || !Detail::ReadString(packet, offset, object.EquippedDeviceModuleName) ||
		    !Detail::ReadString(packet, offset, object.EquippedDevicePresetName) || !Detail::ReadUnsigned(packet, offset, sizeof(std::uint8_t), value) || value > 1) {
			return false;
		}
		object.HasEquippedDeviceMagazine = value != 0;
		if (object.HasEquippedDeviceMagazine) {
			if (!Detail::ReadString(packet, offset, object.EquippedDeviceMagazineClassName) || !Detail::ReadString(packet, offset, object.EquippedDeviceMagazineModuleName) ||
			    !Detail::ReadString(packet, offset, object.EquippedDeviceMagazinePresetName) || !Detail::ReadUnsigned(packet, offset, sizeof(std::uint32_t), value)) {
			return false;
			}
			object.EquippedDeviceMagazineRounds = std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(value));
		}
		}
	}
	if (object.ClassName == "AHuman") {
		auto& device = object.BackgroundDevice;
		if (!Detail::ReadUnsigned(packet, offset, 1, value) || value > 1) return false;
		device.Present = value != 0;
		if (device.Present) {
			if (!Detail::ReadString(packet, offset, device.ClassName) || !Detail::ReadString(packet, offset, device.ModuleName) ||
			    !Detail::ReadString(packet, offset, device.PresetName) || !Detail::ReadUnsigned(packet, offset, 1, value) || value > 1) return false;
			device.HasMagazine = value != 0;
			if (device.HasMagazine) {
				if (!Detail::ReadString(packet, offset, device.MagazineClassName) || !Detail::ReadString(packet, offset, device.MagazineModuleName) ||
				    !Detail::ReadString(packet, offset, device.MagazinePresetName) || !Detail::ReadUnsigned(packet, offset, 4, value)) return false;
				device.MagazineRounds = std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(value));
			}
		}
	}
	if (object.ClassName=="HDFirearm") {
		if (!Detail::ReadUnsigned(packet,offset,1,value) || value>1) return false; object.HasOwnMagazine=value!=0;
		if (object.HasOwnMagazine) {
			if (!Detail::ReadString(packet,offset,object.OwnMagazineClassName) || object.OwnMagazineClassName!="Magazine" ||
			    !Detail::ReadString(packet,offset,object.OwnMagazineModuleName) || object.OwnMagazineModuleName.empty() ||
			    !Detail::ReadString(packet,offset,object.OwnMagazinePresetName) || object.OwnMagazinePresetName.empty() || !Detail::ReadUnsigned(packet,offset,4,value)) return false;
			object.OwnMagazineRounds=std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(value)); if (object.OwnMagazineRounds < -1) return false;
		}
	}
	if (!ReadUnsigned(packet, offset, 1, value) || value > 7) return false;
	object.HasEmitterState = (value & 1) != 0; object.EmissionEnabled = (value & 2) != 0; object.FlashVisible = (value & 4) != 0;
	if (!ReadDeviceVisual(packet, offset, object.DeviceVisual)) return false;
	return ReadAttachments(packet, offset, object) && ValidObject(object);
}
inline bool ValidInventory(const Snapshot& snapshot) {
	if (!snapshot.InventoryComplete || snapshot.Inventory.size()>c_MaxInventoryItems || snapshot.Objects.size()>c_MaxObjects) return false;
	if (snapshot.Inventory.empty()) return true; // Root IDs are already checked by the ordinary object codec.
	std::unordered_set<std::uint64_t> ids;
	std::unordered_map<std::uint64_t,std::pair<std::size_t,std::size_t>> parents;
	for (const auto& object:snapshot.Objects) {
		if (!ids.insert(object.NetworkId).second) return false;
		if (object.Flags==c_ObjectFlagActor) parents.emplace(object.NetworkId,std::pair<std::size_t,std::size_t>{0,0});
	}
	for (const auto& item:snapshot.Inventory) {
		auto parent=parents.find(item.ParentNetworkId);
		if (!ValidObject(item.Object) || !ids.insert(item.Object.NetworkId).second || parent==parents.end() ||
		    parent->second.first>=c_MaxInventoryDepth || parent->second.second>=c_MaxActorInventory || item.Slot!=parent->second.second) return false;
		const auto depth=parent->second.first+1;
		++parent->second.second;
		if (item.Object.Flags==c_ObjectFlagActor) parents.emplace(item.Object.NetworkId,std::pair<std::size_t,std::size_t>{depth,0});
	}
	return true;
}
} // namespace Detail

inline bool EncodeSnapshot(const Snapshot& snapshot, std::uint32_t sequence, std::vector<std::uint8_t>& packet, std::vector<std::uint64_t>& networkIds) {
	packet.clear();
	// Transport callers retain this scratch vector so its storage can be reused between snapshots.
	networkIds.clear();
	if (!Detail::ValidInventory(snapshot)) return false;
	for (float funds : snapshot.TeamFunds) if (!std::isfinite(funds)) return false;
	if (snapshot.ActivityState < c_NoActivity || snapshot.ActivityState > c_MaxActivityState ||
	    snapshot.Objects.size() > c_MaxObjects || snapshot.ActivityClassName.size() > c_MaxStringBytes || snapshot.ActivityPreset.size() > c_MaxStringBytes ||
	    snapshot.ActivityModuleName.size() > c_MaxStringBytes || snapshot.SceneModuleName.size() > c_MaxStringBytes || snapshot.ScenePreset.size() > c_MaxStringBytes ||
	    !UTF8::IsValid(snapshot.ActivityClassName) || !UTF8::IsValid(snapshot.ActivityPreset) || !UTF8::IsValid(snapshot.ActivityModuleName) ||
	    !UTF8::IsValid(snapshot.SceneModuleName) || !UTF8::IsValid(snapshot.ScenePreset)) {
		return false;
	}

	networkIds.reserve(snapshot.Objects.size());
	packet.reserve(c_HeaderSize + 32 + snapshot.Objects.size() * 64);
	packet.resize(c_HeaderSize, 0);
	Detail::WriteUnsigned(packet, snapshot.Tick, sizeof(snapshot.Tick));
	Detail::WriteUnsigned(packet, snapshot.SceneRevision, sizeof(snapshot.SceneRevision));
	Detail::WriteUnsigned(packet, std::bit_cast<std::uint8_t>(snapshot.ActivityState), sizeof(snapshot.ActivityState));
	for (float funds : snapshot.TeamFunds) Detail::WriteFloat(packet, funds);
	if (!Detail::WriteString(packet, snapshot.ActivityClassName) || !Detail::WriteString(packet, snapshot.ActivityPreset) ||
	    !Detail::WriteString(packet, snapshot.ActivityModuleName) || !Detail::WriteString(packet, snapshot.SceneModuleName) ||
	    !Detail::WriteString(packet, snapshot.ScenePreset)) {
		return false;
	}
	Detail::WriteUnsigned(packet, snapshot.Objects.size(), sizeof(std::uint32_t));
	for (const ObjectState& object : snapshot.Objects) {
		if (!Detail::WriteObject(packet,object)) return false;
		networkIds.push_back(object.NetworkId);
	}
	std::sort(networkIds.begin(), networkIds.end());
	if (std::adjacent_find(networkIds.begin(), networkIds.end()) != networkIds.end()) {
		return false;
	}
	for (const PlayerState& player : snapshot.Players) {
		if (!std::isfinite(player.CameraX) || !std::isfinite(player.CameraY) || player.Team < c_NoTeam || player.Team >= c_MaxTeamCount ||
		    (!player.ActiveHuman && player.ControlledActorId != 0)) return false;
		if (player.ControlledActorId != 0 && std::none_of(snapshot.Objects.begin(), snapshot.Objects.end(), [&player](const ObjectState& object) {
			return object.NetworkId == player.ControlledActorId && object.Flags == c_ObjectFlagActor;
		})) return false;
		Detail::WriteUnsigned(packet, player.ControlledActorId, sizeof(player.ControlledActorId));
		Detail::WriteFloat(packet, player.CameraX);
		Detail::WriteFloat(packet, player.CameraY);
		Detail::WriteUnsigned(packet, static_cast<std::uint16_t>(player.Team), sizeof(player.Team));
		Detail::WriteUnsigned(packet, player.ActiveHuman ? 1 : 0, sizeof(std::uint8_t));
		if (player.TextBlinkInterval > static_cast<std::uint32_t>(std::numeric_limits<int>::max()) ||
		    !Detail::WriteString(packet, player.ScreenText, c_MaxScreenTextBytes)) return false;
		Detail::WriteUnsigned(packet, player.TextBlinkInterval, 4);
		Detail::WriteUnsigned(packet, player.TextCentered ? 1 : 0, 1);
	}
	Detail::WriteUnsigned(packet,snapshot.Inventory.size(),2);
	for (const auto& item:snapshot.Inventory) {
		Detail::WriteUnsigned(packet,item.ParentNetworkId,8); Detail::WriteUnsigned(packet,item.Slot,2);
		if (!Detail::WriteObject(packet,item.Object)) return false;
	}
	if (packet.size() > c_MaxPacketSize) return false;

	std::size_t headerOffset = 0;
	auto WriteHeaderUnsigned = [&](std::uint64_t value, std::size_t byteCount) {
		for (std::size_t byte = 0; byte < byteCount; ++byte) {
			packet[headerOffset++] = static_cast<std::uint8_t>((value >> (byte * 8)) & 0xFF);
		}
	};
	WriteHeaderUnsigned(c_Magic, sizeof(c_Magic));
	WriteHeaderUnsigned(c_Version, sizeof(c_Version));
	WriteHeaderUnsigned(static_cast<std::uint8_t>(MessageType::WorldSnapshot), sizeof(std::uint8_t));
	WriteHeaderUnsigned(0, sizeof(std::uint8_t));
	WriteHeaderUnsigned(sequence, sizeof(sequence));
	WriteHeaderUnsigned(packet.size() - c_HeaderSize, sizeof(std::uint32_t));
	return true;
}

inline bool EncodeSnapshot(const Snapshot& snapshot, std::uint32_t sequence, std::vector<std::uint8_t>& packet) {
	std::vector<std::uint64_t> networkIds;
	return EncodeSnapshot(snapshot, sequence, packet, networkIds);
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
	if (!Detail::ReadUnsigned(packet, offset, sizeof(decoded.ActivityState), value)) return false;
	decoded.ActivityState = std::bit_cast<std::int8_t>(static_cast<std::uint8_t>(value));
	if (decoded.ActivityState < c_NoActivity || decoded.ActivityState > c_MaxActivityState) return false;
	for (float& funds : decoded.TeamFunds) if (!Detail::ReadFloat(packet, offset, funds)) return false;
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
		if (!Detail::ReadObject(packet,offset,object) || !networkIds.insert(object.NetworkId).second) return false;
		decoded.Objects.push_back(std::move(object));
	}
	for (PlayerState& player : decoded.Players) {
		if (!Detail::ReadUnsigned(packet, offset, sizeof(player.ControlledActorId), value)) return false;
		player.ControlledActorId = value;
		if (!Detail::ReadFloat(packet, offset, player.CameraX) || !Detail::ReadFloat(packet, offset, player.CameraY) ||
		    !Detail::ReadUnsigned(packet, offset, sizeof(player.Team), value)) return false;
		player.Team = std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(value));
		if (player.Team < c_NoTeam || player.Team >= c_MaxTeamCount ||
		    !Detail::ReadUnsigned(packet, offset, sizeof(std::uint8_t), value) || value > 1) return false;
		player.ActiveHuman = value != 0;
		if (!Detail::ReadString(packet, offset, player.ScreenText, c_MaxScreenTextBytes) ||
		    !Detail::ReadUnsigned(packet, offset, 4, value) || value > static_cast<std::uint32_t>(std::numeric_limits<int>::max())) return false;
		player.TextBlinkInterval = static_cast<std::uint32_t>(value);
		if (!Detail::ReadUnsigned(packet, offset, 1, value) || value > 1) return false;
		player.TextCentered = value != 0;
		if (!player.ActiveHuman && player.ControlledActorId != 0) return false;
		if (player.ControlledActorId != 0 && std::none_of(decoded.Objects.begin(), decoded.Objects.end(), [&player](const ObjectState& object) {
			return object.NetworkId == player.ControlledActorId && object.Flags == c_ObjectFlagActor;
		})) return false;
	}
	if (!Detail::ReadUnsigned(packet,offset,2,value) || value>c_MaxInventoryItems) return false;
	const auto inventoryCount=static_cast<std::size_t>(value);
	decoded.Inventory.reserve(inventoryCount);
	for (std::size_t index=0;index<inventoryCount;++index) {
		InventoryItemState item;
		if (!Detail::ReadUnsigned(packet,offset,8,value)) return false; item.ParentNetworkId=value;
		if (!Detail::ReadUnsigned(packet,offset,2,value)) return false; item.Slot=static_cast<std::uint16_t>(value);
		if (!Detail::ReadObject(packet,offset,item.Object)) return false;
		decoded.Inventory.push_back(std::move(item));
	}
	if (!Detail::ValidInventory(decoded)) return false;
	if (offset != packet.size()) {
		return false;
	}
	snapshot = std::move(decoded);
	if (sequence) *sequence = static_cast<std::uint32_t>(sequenceValue);
	return true;
}

} // namespace RTE::WorldStateProtocol
