#pragma once
#include "WorldStateProtocol.h"
#include <optional>

namespace RTE::WorldStateProtocol {
inline constexpr std::size_t c_MaxConstructionStateBytes = 512 * 1024;
struct ConstructionPlayerState {
	bool Active = false, Ready = false;
	std::int16_t Team = c_NoTeam;
	std::optional<ObjectState> Brain;
	bool operator==(const ConstructionPlayerState&) const = default;
};
struct ConstructionState {
	std::uint32_t SceneRevision = 0;
	std::int8_t ActivityState = c_NoActivity;
	std::array<ConstructionPlayerState, c_PlayerSlotCount> Players;
	bool operator==(const ConstructionState&) const = default;
};
inline bool ValidConstructionState(const ConstructionState& state) {
	if (!state.SceneRevision || state.ActivityState < c_NoActivity || state.ActivityState > c_MaxActivityState) return false;
	std::unordered_set<std::uint64_t> ids;
	for (const auto& player : state.Players) {
		if (!player.Active) {
			if (player.Ready || player.Team != c_NoTeam || player.Brain) return false;
		} else if (player.Team < 0 || player.Team >= c_MaxTeamCount) return false;
		if (state.ActivityState != 2 && (player.Ready || player.Brain)) return false;
		if (player.Brain && (!Detail::ValidObject(*player.Brain) || !player.Brain->NetworkId || player.Brain->Flags != c_ObjectFlagActor ||
		    player.Brain->Team != player.Team || !ids.insert(player.Brain->NetworkId).second)) return false;
	}
	return true;
}
inline bool EncodeConstructionState(const ConstructionState& state, std::uint32_t sequence, std::vector<std::uint8_t>& packet) {
	if (!sequence || !ValidConstructionState(state)) return false;
	std::vector<std::uint8_t> body;
	Detail::WriteUnsigned(body, state.SceneRevision, 4);
	Detail::WriteUnsigned(body, std::bit_cast<std::uint8_t>(state.ActivityState), 1);
	for (const auto& player : state.Players) {
		Detail::WriteUnsigned(body, player.Active, 1); Detail::WriteUnsigned(body, player.Ready, 1);
		Detail::WriteUnsigned(body, std::bit_cast<std::uint16_t>(player.Team), 2);
		Detail::WriteUnsigned(body, player.Brain.has_value(), 1);
		if (player.Brain && !Detail::WriteObject(body, *player.Brain)) return false;
	}
	if (body.size() > c_MaxConstructionStateBytes - c_HeaderSize) return false;
	packet.clear();
	Detail::WriteUnsigned(packet, c_Magic, 4); Detail::WriteUnsigned(packet, c_Version, 2);
	Detail::WriteUnsigned(packet, static_cast<std::uint8_t>(MessageType::ConstructionState), 1); Detail::WriteUnsigned(packet, 0, 1);
	Detail::WriteUnsigned(packet, sequence, 4); Detail::WriteUnsigned(packet, body.size(), 4);
	packet.insert(packet.end(), body.begin(), body.end()); return true;
}
inline bool DecodeConstructionState(std::span<const std::uint8_t> packet, ConstructionState& state, std::uint32_t& sequence) {
	if (packet.size() < c_HeaderSize || packet.size() > c_MaxConstructionStateBytes) return false;
	ConstructionState decoded;
	std::uint64_t value = 0; std::size_t offset = 0;
	if (!Detail::ReadUnsigned(packet, offset, 4, value) || value != c_Magic || !Detail::ReadUnsigned(packet, offset, 2, value) || value != c_Version ||
	    !Detail::ReadUnsigned(packet, offset, 1, value) || value != static_cast<std::uint8_t>(MessageType::ConstructionState) ||
	    !Detail::ReadUnsigned(packet, offset, 1, value) || value != 0 || !Detail::ReadUnsigned(packet, offset, 4, value) || !value) return false;
	const auto receivedSequence = static_cast<std::uint32_t>(value);
	if (!Detail::ReadUnsigned(packet, offset, 4, value) || value != packet.size() - c_HeaderSize || !Detail::ReadUnsigned(packet, offset, 4, value)) return false;
	decoded.SceneRevision = static_cast<std::uint32_t>(value);
	if (!Detail::ReadUnsigned(packet, offset, 1, value)) return false;
	decoded.ActivityState = std::bit_cast<std::int8_t>(static_cast<std::uint8_t>(value));
	for (auto& player : decoded.Players) {
		if (!Detail::ReadUnsigned(packet, offset, 1, value) || value > 1) return false;
		player.Active = value != 0;
		if (!Detail::ReadUnsigned(packet, offset, 1, value) || value > 1) return false;
		player.Ready = value != 0;
		if (!Detail::ReadUnsigned(packet, offset, 2, value)) return false;
		player.Team = std::bit_cast<std::int16_t>(static_cast<std::uint16_t>(value));
		if (!Detail::ReadUnsigned(packet, offset, 1, value) || value > 1) return false;
		if (value) { player.Brain.emplace(); if (!Detail::ReadObject(packet, offset, *player.Brain)) return false; }
	}
	if (offset != packet.size() || !ValidConstructionState(decoded)) return false;
	state = std::move(decoded); sequence = receivedSequence; return true;
}
}
