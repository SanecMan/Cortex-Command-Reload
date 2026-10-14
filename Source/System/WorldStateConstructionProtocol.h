#pragma once
#include "WorldStatePurchaseProtocol.h"

namespace RTE::WorldStateProtocol {
inline constexpr std::size_t c_MaxConstructionPacketBytes = 4096;
enum class ConstructionAction : std::uint8_t { PlaceBrain, PlaceObject, RemoveObject, SetReady };

// Player identity, permissions and price come from the server, never this request.
struct ConstructionRequest {
	std::uint32_t RequestId = 0, SceneRevision = 0;
	ConstructionAction Action = ConstructionAction::PlaceBrain;
	float X = 0, Y = 0, Rotation = 0;
	bool HFlipped = false, Ready = false;
	std::uint64_t ObjectId = 0;
	PurchasePreset Preset;
	bool operator==(const ConstructionRequest&) const = default;
};
struct ConstructionResult {
	std::uint32_t RequestId = 0, SceneRevision = 0;
	bool Accepted = false;
	float ChargedCost = 0;
	std::uint64_t ObjectId = 0;
	std::string Reason;
	bool operator==(const ConstructionResult&) const = default;
};
namespace ConstructionDetail {
inline bool Valid(const ConstructionRequest& request) {
	if (!request.RequestId || !request.SceneRevision || !std::isfinite(request.X) ||
	    !std::isfinite(request.Y) || !std::isfinite(request.Rotation)) return false;
	const bool emptyPreset = request.Preset == PurchasePreset{};
	const bool emptyPose = request.X == 0 && request.Y == 0 && request.Rotation == 0 && !request.HFlipped;
	switch (request.Action) {
		case ConstructionAction::PlaceBrain:
			return emptyPreset && !request.ObjectId && !request.Ready;
		case ConstructionAction::PlaceObject:
			return !request.ObjectId && !request.Ready && !request.Preset.ClassName.empty() &&
			    !request.Preset.ModuleName.empty() && !request.Preset.PresetName.empty();
		case ConstructionAction::RemoveObject:
			return request.ObjectId && emptyPreset && emptyPose && !request.Ready;
		case ConstructionAction::SetReady:
			return !request.ObjectId && emptyPreset && emptyPose;
	}
	return false;
}
}
inline bool EncodeConstructionRequest(const ConstructionRequest& request, std::vector<std::uint8_t>& packet) {
	if (!ConstructionDetail::Valid(request)) return false;
	std::vector<std::uint8_t> body;
	Detail::WriteUnsigned(body, request.SceneRevision, 4);
	Detail::WriteUnsigned(body, static_cast<std::uint8_t>(request.Action), 1);
	if (request.Action == ConstructionAction::PlaceBrain || request.Action == ConstructionAction::PlaceObject) {
		Detail::WriteFloat(body, request.X); Detail::WriteFloat(body, request.Y); Detail::WriteFloat(body, request.Rotation);
		Detail::WriteUnsigned(body, request.HFlipped, 1);
		if (request.Action == ConstructionAction::PlaceObject && !PurchaseDetail::WritePreset(body, request.Preset)) return false;
	} else if (request.Action == ConstructionAction::RemoveObject) {
		Detail::WriteUnsigned(body, request.ObjectId, 8);
	} else {
		Detail::WriteUnsigned(body, request.Ready, 1);
	}
	return body.size() <= c_MaxConstructionPacketBytes - c_HeaderSize &&
	    PurchaseDetail::Wrap(MessageType::ConstructionRequest, request.RequestId, body, packet);
}
inline bool DecodeConstructionRequest(std::span<const std::uint8_t> packet, ConstructionRequest& request) {
	if (packet.size() > c_MaxConstructionPacketBytes) return false;
	ConstructionRequest decoded;
	std::size_t offset = 0;
	std::uint64_t value = 0;
	if (!PurchaseDetail::Header(packet, MessageType::ConstructionRequest, offset, decoded.RequestId) ||
	    !Detail::ReadUnsigned(packet, offset, 4, value)) return false;
	decoded.SceneRevision = static_cast<std::uint32_t>(value);
	if (!Detail::ReadUnsigned(packet, offset, 1, value) || value > static_cast<std::uint8_t>(ConstructionAction::SetReady)) return false;
	decoded.Action = static_cast<ConstructionAction>(value);
	if (decoded.Action == ConstructionAction::PlaceBrain || decoded.Action == ConstructionAction::PlaceObject) {
		if (!Detail::ReadFloat(packet, offset, decoded.X) || !Detail::ReadFloat(packet, offset, decoded.Y) ||
		    !Detail::ReadFloat(packet, offset, decoded.Rotation) || !Detail::ReadUnsigned(packet, offset, 1, value) || value > 1) return false;
		decoded.HFlipped = value != 0;
		if (decoded.Action == ConstructionAction::PlaceObject && !PurchaseDetail::ReadPreset(packet, offset, decoded.Preset)) return false;
	} else if (decoded.Action == ConstructionAction::RemoveObject) {
		if (!Detail::ReadUnsigned(packet, offset, 8, decoded.ObjectId)) return false;
	} else {
		if (!Detail::ReadUnsigned(packet, offset, 1, value) || value > 1) return false;
		decoded.Ready = value != 0;
	}
	if (offset != packet.size() || !ConstructionDetail::Valid(decoded)) return false;
	request = std::move(decoded);
	return true;
}
inline bool EncodeConstructionResult(const ConstructionResult& result, std::vector<std::uint8_t>& packet) {
	if (!result.RequestId || !result.SceneRevision || !std::isfinite(result.ChargedCost) || result.ChargedCost < 0 ||
	    (result.Accepted ? !result.Reason.empty() : (result.Reason.empty() || result.ChargedCost != 0 || result.ObjectId != 0))) return false;
	std::vector<std::uint8_t> body;
	Detail::WriteUnsigned(body, result.SceneRevision, 4);
	Detail::WriteUnsigned(body, result.Accepted, 1);
	Detail::WriteFloat(body, result.ChargedCost);
	Detail::WriteUnsigned(body, result.ObjectId, 8);
	if (!Detail::WriteString(body, result.Reason) || body.size() > c_MaxConstructionPacketBytes - c_HeaderSize) return false;
	return PurchaseDetail::Wrap(MessageType::ConstructionResult, result.RequestId, body, packet);
}
inline bool DecodeConstructionResult(std::span<const std::uint8_t> packet, ConstructionResult& result) {
	if (packet.size() > c_MaxConstructionPacketBytes) return false;
	ConstructionResult decoded;
	std::size_t offset = 0;
	std::uint64_t value = 0;
	if (!PurchaseDetail::Header(packet, MessageType::ConstructionResult, offset, decoded.RequestId) ||
	    !Detail::ReadUnsigned(packet, offset, 4, value)) return false;
	decoded.SceneRevision = static_cast<std::uint32_t>(value);
	if (!Detail::ReadUnsigned(packet, offset, 1, value) || value > 1) return false;
	decoded.Accepted = value != 0;
	if (!Detail::ReadFloat(packet, offset, decoded.ChargedCost) || !Detail::ReadUnsigned(packet, offset, 8, decoded.ObjectId) ||
	    !Detail::ReadString(packet, offset, decoded.Reason) || offset != packet.size()) return false;
	std::vector<std::uint8_t> checked;
	if (!EncodeConstructionResult(decoded, checked)) return false;
	result = std::move(decoded);
	return true;
}
}
