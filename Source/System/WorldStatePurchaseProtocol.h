#pragma once
#include "WorldStateProtocol.h"

namespace RTE::WorldStateProtocol {
inline constexpr std::size_t c_MaxPurchaseCargo = 128;
inline constexpr std::size_t c_MaxPurchasePacketBytes = 64 * 1024;
struct PurchasePreset {
	std::string ClassName, ModuleName, PresetName;
	bool operator==(const PurchasePreset&) const = default;
};
struct PurchaseRequest {
	std::uint32_t RequestId = 0, SceneRevision = 0;
	float LandingX = 0, LandingY = 0;
	std::uint8_t AIMode = 1;
	PurchasePreset Craft;
	std::vector<PurchasePreset> Cargo;
	bool operator==(const PurchaseRequest&) const = default;
};
struct PurchaseResult {
	std::uint32_t RequestId = 0, SceneRevision = 0;
	bool Accepted = false;
	float ChargedCost = 0;
	std::string Reason;
	bool operator==(const PurchaseResult&) const = default;
};
namespace PurchaseDetail {
	inline bool WritePreset(std::vector<std::uint8_t>& bytes, const PurchasePreset& preset) {
		return !preset.ClassName.empty() && !preset.ModuleName.empty() && !preset.PresetName.empty() &&
		    Detail::WriteString(bytes, preset.ClassName) && Detail::WriteString(bytes, preset.ModuleName) && Detail::WriteString(bytes, preset.PresetName);
	}
	inline bool ReadPreset(std::span<const std::uint8_t> bytes, std::size_t& offset, PurchasePreset& preset) {
		return Detail::ReadString(bytes, offset, preset.ClassName) && Detail::ReadString(bytes, offset, preset.ModuleName) &&
		    Detail::ReadString(bytes, offset, preset.PresetName) && !preset.ClassName.empty() && !preset.ModuleName.empty() && !preset.PresetName.empty();
	}
	inline bool Wrap(MessageType type, std::uint32_t id, const std::vector<std::uint8_t>& body, std::vector<std::uint8_t>& packet) {
		if (!id || body.size() > c_MaxPurchasePacketBytes - c_HeaderSize) return false;
		packet.clear();
		Detail::WriteUnsigned(packet, c_Magic, 4);
		Detail::WriteUnsigned(packet, c_Version, 2);
		Detail::WriteUnsigned(packet, static_cast<std::uint8_t>(type), 1);
		Detail::WriteUnsigned(packet, 0, 1);
		Detail::WriteUnsigned(packet, id, 4);
		Detail::WriteUnsigned(packet, body.size(), 4);
		packet.insert(packet.end(), body.begin(), body.end());
		return true;
	}
	inline bool Header(std::span<const std::uint8_t> bytes, MessageType type, std::size_t& offset, std::uint32_t& id) {
		if (bytes.size() < c_HeaderSize || bytes.size() > c_MaxPurchasePacketBytes) return false;
		std::uint64_t value = 0;
		if (!Detail::ReadUnsigned(bytes, offset, 4, value) || value != c_Magic ||
		    !Detail::ReadUnsigned(bytes, offset, 2, value) || value != c_Version ||
		    !Detail::ReadUnsigned(bytes, offset, 1, value) || value != static_cast<std::uint8_t>(type) ||
		    !Detail::ReadUnsigned(bytes, offset, 1, value) || value != 0 ||
		    !Detail::ReadUnsigned(bytes, offset, 4, value) || value == 0) return false;
		id = static_cast<std::uint32_t>(value);
		return Detail::ReadUnsigned(bytes, offset, 4, value) && value == bytes.size() - c_HeaderSize;
	}
}
inline bool EncodePurchaseRequest(const PurchaseRequest& request, std::vector<std::uint8_t>& packet) {
	if (!request.SceneRevision || request.Cargo.size() > c_MaxPurchaseCargo || !std::isfinite(request.LandingX) || !std::isfinite(request.LandingY)) return false;
	std::vector<std::uint8_t> body;
	Detail::WriteUnsigned(body, request.SceneRevision, 4);
	Detail::WriteFloat(body, request.LandingX); Detail::WriteFloat(body, request.LandingY);
	Detail::WriteUnsigned(body, request.AIMode, 1);
	if (!PurchaseDetail::WritePreset(body, request.Craft)) return false;
	Detail::WriteUnsigned(body, request.Cargo.size(), 2);
	for (const auto& preset : request.Cargo) if (!PurchaseDetail::WritePreset(body, preset)) return false;
	return PurchaseDetail::Wrap(MessageType::PurchaseRequest, request.RequestId, body, packet);
}
inline bool DecodePurchaseRequest(std::span<const std::uint8_t> packet, PurchaseRequest& request) {
	PurchaseRequest decoded;
	std::size_t offset = 0;
	std::uint64_t value = 0;
	if (!PurchaseDetail::Header(packet, MessageType::PurchaseRequest, offset, decoded.RequestId) ||
	    !Detail::ReadUnsigned(packet, offset, 4, value) || !value) return false;
	decoded.SceneRevision = static_cast<std::uint32_t>(value);
	if (!Detail::ReadFloat(packet, offset, decoded.LandingX) || !Detail::ReadFloat(packet, offset, decoded.LandingY) ||
	    !Detail::ReadUnsigned(packet, offset, 1, value)) return false;
	decoded.AIMode = static_cast<std::uint8_t>(value);
	if (!PurchaseDetail::ReadPreset(packet, offset, decoded.Craft) || !Detail::ReadUnsigned(packet, offset, 2, value) || value > c_MaxPurchaseCargo) return false;
	decoded.Cargo.resize(static_cast<std::size_t>(value));
	for (auto& preset : decoded.Cargo) if (!PurchaseDetail::ReadPreset(packet, offset, preset)) return false;
	if (offset != packet.size()) return false;
	request = std::move(decoded); return true;
}
inline bool EncodePurchaseResult(const PurchaseResult& result, std::vector<std::uint8_t>& packet) {
	if (!std::isfinite(result.ChargedCost) || result.ChargedCost < 0 ||
	    (result.Accepted ? !result.Reason.empty() : (result.Reason.empty() || result.ChargedCost != 0))) return false;
	std::vector<std::uint8_t> body;
	Detail::WriteUnsigned(body, result.SceneRevision, 4);
	Detail::WriteUnsigned(body, result.Accepted, 1);
	Detail::WriteFloat(body, result.ChargedCost);
	if (!Detail::WriteString(body, result.Reason)) return false;
	return PurchaseDetail::Wrap(MessageType::PurchaseResult, result.RequestId, body, packet);
}
inline bool DecodePurchaseResult(std::span<const std::uint8_t> packet, PurchaseResult& result) {
	PurchaseResult decoded;
	std::size_t offset = 0;
	std::uint64_t value = 0;
	if (!PurchaseDetail::Header(packet, MessageType::PurchaseResult, offset, decoded.RequestId) || !Detail::ReadUnsigned(packet, offset, 4, value)) return false;
	decoded.SceneRevision = static_cast<std::uint32_t>(value);
	if (!Detail::ReadUnsigned(packet, offset, 1, value) || value > 1) return false;
	decoded.Accepted = value != 0;
	if (!Detail::ReadFloat(packet, offset, decoded.ChargedCost) || !Detail::ReadString(packet, offset, decoded.Reason) || offset != packet.size()) return false;
	std::vector<std::uint8_t> checked;
	if (!EncodePurchaseResult(decoded, checked)) return false;
	result = std::move(decoded); return true;
}
}
