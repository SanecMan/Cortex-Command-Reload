#pragma once
#include "WorldStateProtocol.h"

namespace RTE::WorldStateProtocol {
// Optional extension. Existing v29 snapshots and construction messages are unchanged.
inline constexpr std::uint8_t c_ConstructionCatalogMessage = 14;
inline constexpr std::size_t c_MaxConstructionCatalogBytes = 256 * 1024;
inline constexpr std::size_t c_MaxConstructionAssemblyGroups = 128;
struct ConstructionCatalog {
	std::uint32_t SceneRevision = 0;
	std::uint8_t PlayerSlot = 0;
	std::string NativeModule, ModuleSpace, TypeFilter = "All";
	float ForeignMultiplier = 1;
	bool ShowForeign = true;
	std::vector<std::string> VisibleAssemblyGroups;
	bool operator==(const ConstructionCatalog&) const = default;
};
inline bool ValidConstructionCatalog(const ConstructionCatalog& catalog) {
	auto validText = [](const std::string& text) { return text.size() <= c_MaxStringBytes && UTF8::IsValid(text) && text.find('\0') == std::string::npos; };
	if (!catalog.SceneRevision || catalog.PlayerSlot >= c_PlayerSlotCount || !validText(catalog.NativeModule) || !validText(catalog.ModuleSpace) ||
	    catalog.TypeFilter.empty() || !validText(catalog.TypeFilter) || !std::isfinite(catalog.ForeignMultiplier) || catalog.ForeignMultiplier < 0 ||
	    catalog.VisibleAssemblyGroups.size() > c_MaxConstructionAssemblyGroups) return false;
	for (const auto& group : catalog.VisibleAssemblyGroups) if (group.empty() || !validText(group)) return false;
	return true;
}
inline bool EncodeConstructionCatalog(const ConstructionCatalog& catalog, std::uint32_t sequence, std::vector<std::uint8_t>& packet) {
	if (!sequence || !ValidConstructionCatalog(catalog)) return false;
	std::vector<std::uint8_t> body;
	Detail::WriteUnsigned(body, catalog.SceneRevision, 4); Detail::WriteUnsigned(body, catalog.PlayerSlot, 1);
	if (!Detail::WriteString(body, catalog.NativeModule) || !Detail::WriteString(body, catalog.ModuleSpace) || !Detail::WriteString(body, catalog.TypeFilter)) return false;
	Detail::WriteFloat(body, catalog.ForeignMultiplier); Detail::WriteUnsigned(body, catalog.ShowForeign, 1);
	Detail::WriteUnsigned(body, catalog.VisibleAssemblyGroups.size(), 2);
	for (const auto& group : catalog.VisibleAssemblyGroups) if (!Detail::WriteString(body, group)) return false;
	if (body.size() > c_MaxConstructionCatalogBytes - c_HeaderSize) return false;
	std::vector<std::uint8_t> encoded;
	Detail::WriteUnsigned(encoded, c_Magic, 4); Detail::WriteUnsigned(encoded, c_Version, 2);
	Detail::WriteUnsigned(encoded, c_ConstructionCatalogMessage, 1); Detail::WriteUnsigned(encoded, 0, 1);
	Detail::WriteUnsigned(encoded, sequence, 4); Detail::WriteUnsigned(encoded, body.size(), 4);
	encoded.insert(encoded.end(), body.begin(), body.end()); packet = std::move(encoded); return true;
}
inline bool DecodeConstructionCatalog(std::span<const std::uint8_t> packet, ConstructionCatalog& catalog, std::uint32_t& sequence) {
	if (packet.size() < c_HeaderSize || packet.size() > c_MaxConstructionCatalogBytes) return false;
	std::size_t offset = 0; std::uint64_t value = 0;
	if (!Detail::ReadUnsigned(packet, offset, 4, value) || value != c_Magic || !Detail::ReadUnsigned(packet, offset, 2, value) || value != c_Version ||
	    !Detail::ReadUnsigned(packet, offset, 1, value) || value != c_ConstructionCatalogMessage ||
	    !Detail::ReadUnsigned(packet, offset, 1, value) || value || !Detail::ReadUnsigned(packet, offset, 4, value) || !value) return false;
	const auto receivedSequence = static_cast<std::uint32_t>(value);
	if (!Detail::ReadUnsigned(packet, offset, 4, value) || value != packet.size() - c_HeaderSize) return false;
	ConstructionCatalog decoded;
	if (!Detail::ReadUnsigned(packet, offset, 4, value)) return false;
	decoded.SceneRevision = static_cast<std::uint32_t>(value);
	if (!Detail::ReadUnsigned(packet, offset, 1, value)) return false;
	decoded.PlayerSlot = static_cast<std::uint8_t>(value);
	if (!Detail::ReadString(packet, offset, decoded.NativeModule) || !Detail::ReadString(packet, offset, decoded.ModuleSpace) ||
	    !Detail::ReadString(packet, offset, decoded.TypeFilter) || !Detail::ReadFloat(packet, offset, decoded.ForeignMultiplier) ||
	    !Detail::ReadUnsigned(packet, offset, 1, value) || value > 1) return false;
	decoded.ShowForeign = value != 0;
	if (!Detail::ReadUnsigned(packet, offset, 2, value) || value > c_MaxConstructionAssemblyGroups) return false;
	decoded.VisibleAssemblyGroups.resize(static_cast<std::size_t>(value));
	for (auto& group : decoded.VisibleAssemblyGroups) if (!Detail::ReadString(packet, offset, group)) return false;
	if (offset != packet.size() || !ValidConstructionCatalog(decoded)) return false;
	catalog = std::move(decoded); sequence = receivedSequence; return true;
}
}
