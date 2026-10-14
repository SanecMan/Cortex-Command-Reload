#include "WorldStateConstructionState.h"
#include <iostream>
using namespace RTE::WorldStateProtocol;
int main() {
	ConstructionState state, decoded;
	state.SceneRevision = 3; state.ActivityState = 2;
	state.Players[0].Active = state.Players[0].Ready = true; state.Players[0].Team = 0;
	auto& brain = state.Players[0].Brain.emplace();
	brain.NetworkId = 5; brain.ClassName = "Actor"; brain.ModuleName = "Base.rte"; brain.PresetName = "Мозг Ёжик";
	brain.Flags = c_ObjectFlagActor; brain.Team = 0; brain.Health = 100; brain.PositionX = 123;
	state.Players[1].Active = true; state.Players[1].Team = 1;
	std::vector<std::uint8_t> bytes; std::uint32_t sequence = 0;
	if (!EncodeConstructionState(state, 9, bytes) || !DecodeConstructionState(bytes, decoded, sequence) || decoded != state || sequence != 9) return 1;
	for (std::size_t length = 0; length < bytes.size(); ++length) {
		if (DecodeConstructionState(std::span(bytes).first(length), decoded, sequence) || decoded != state || sequence != 9) return 2;
	}
	auto invalid = bytes; invalid[c_HeaderSize + 5] = 2;
	if (DecodeConstructionState(invalid, decoded, sequence)) return 3;
	invalid = bytes; invalid.push_back(0); ++invalid[12];
	if (DecodeConstructionState(invalid, decoded, sequence)) return 4;
	auto bad = state; bad.Players[2].Ready = true;
	if (EncodeConstructionState(bad, 9, bytes)) return 5;
	bad = state; bad.Players[0].Brain->Team = 1;
	if (EncodeConstructionState(bad, 9, bytes)) return 6;
	bad = state; bad.Players[1].Brain = brain; bad.Players[1].Brain->Team = 1;
	if (EncodeConstructionState(bad, 9, bytes)) return 7;
	bad = state; bad.Players[0].Brain->PositionX = std::numeric_limits<float>::infinity();
	if (EncodeConstructionState(bad, 9, bytes)) return 8;
	bad = state; bad.Players[0].Brain->PresetName.assign(1, '\xFF');
	if (EncodeConstructionState(bad, 9, bytes)) return 9;
	bad = state; bad.ActivityState = 4;
	if (EncodeConstructionState(bad, 9, bytes)) return 10;
	bad.Players[0].Brain.reset(); bad.Players[0].Ready = false;
	if (!EncodeConstructionState(bad, 10, bytes) || !DecodeConstructionState(bytes, decoded, sequence) || decoded != bad || sequence != 10) return 11;
	if (EncodeConstructionState(bad, 0, bytes)) return 12;
	bytes.resize(c_MaxConstructionStateBytes + 1);
	if (DecodeConstructionState(bytes, decoded, sequence)) return 13;
	std::cout << "construction_state_codec=passed\n";
}
