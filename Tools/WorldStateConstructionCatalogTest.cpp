#include "WorldStateConstructionCatalog.h"
#include <iostream>
using namespace RTE::WorldStateProtocol;
int main() {
	ConstructionCatalog source;
	source.SceneRevision = 7; source.PlayerSlot = 3; source.NativeModule = "Фракция_Ёжик.rte";
	source.ModuleSpace = "Мод.rte"; source.ForeignMultiplier = 4; source.ShowForeign = false;
	source.VisibleAssemblyGroups = {"Assemblies", "Бункеры"};
	ConstructionCatalog decoded;
	std::uint32_t sequence = 0; std::vector<std::uint8_t> bytes;
	if (!EncodeConstructionCatalog(source, 17, bytes) || !DecodeConstructionCatalog(bytes, decoded, sequence) || source != decoded || sequence != 17) return 1;
	auto rejects = [&](const std::vector<std::uint8_t>& invalid) {
		const auto saved = decoded; const auto savedSequence = sequence;
		return !DecodeConstructionCatalog(invalid, decoded, sequence) && decoded == saved && sequence == savedSequence;
	};
	for (std::size_t size = 0; size < bytes.size(); ++size) if (!rejects(std::vector(bytes.begin(), bytes.begin() + size))) return 2;
	for (std::size_t offset : {std::size_t(0), std::size_t(4), std::size_t(6), std::size_t(7), std::size_t(12)}) {
		auto invalid = bytes; invalid[offset] ^= 0x80; if (!rejects(invalid)) return 3;
	}
	auto trailing = bytes; trailing.push_back(0); if (!rejects(trailing)) return 4;
	const std::size_t floatOffset = c_HeaderSize + 5 + 6 + source.NativeModule.size() + source.ModuleSpace.size() + source.TypeFilter.size();
	trailing = bytes; trailing[floatOffset + 4] = 2; if (!rejects(trailing)) return 15;
	std::vector<std::uint8_t> numberBytes;
	Detail::WriteFloat(numberBytes, std::numeric_limits<float>::quiet_NaN());
	trailing = bytes; std::copy(numberBytes.begin(), numberBytes.end(), trailing.begin() + floatOffset); if (!rejects(trailing)) return 16;
	trailing = bytes; std::fill(trailing.begin() + 8, trailing.begin() + 12, 0); if (!rejects(trailing)) return 17;
	trailing = bytes; trailing[20] = 4; if (!rejects(trailing)) return 18;
	if (!rejects(std::vector<std::uint8_t>(c_MaxConstructionCatalogBytes + 1))) return 19;
	trailing = bytes; trailing[23] = 0xFF; if (!rejects(trailing)) return 20;
	trailing = bytes; std::fill(trailing.begin() + 16, trailing.begin() + 20, 0); if (!rejects(trailing)) return 21;
	auto invalid = source;
	for (float number : {-1.0F, std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN()}) {
		invalid.ForeignMultiplier = number; if (EncodeConstructionCatalog(invalid, 1, trailing)) return 5;
	}
	invalid = source; invalid.PlayerSlot = 4; if (EncodeConstructionCatalog(invalid, 1, trailing)) return 6;
	invalid = source; invalid.SceneRevision = 0; if (EncodeConstructionCatalog(invalid, 1, trailing)) return 7;
	invalid = source; invalid.TypeFilter.clear(); if (EncodeConstructionCatalog(invalid, 1, trailing)) return 8;
	invalid = source; invalid.NativeModule = std::string("x\0y", 3); if (EncodeConstructionCatalog(invalid, 1, trailing)) return 9;
	invalid = source; invalid.ModuleSpace = std::string(1, char(0xFF)); if (EncodeConstructionCatalog(invalid, 1, trailing)) return 10;
	invalid = source; invalid.VisibleAssemblyGroups.resize(129, "group"); if (EncodeConstructionCatalog(invalid, 1, trailing)) return 11;
	invalid = {}; invalid.SceneRevision = 1; invalid.ForeignMultiplier = 0;
	invalid.VisibleAssemblyGroups.resize(128, std::string(1024, 'x'));
	if (!EncodeConstructionCatalog(invalid, 1, bytes) || !DecodeConstructionCatalog(bytes, decoded, sequence) || invalid != decoded) return 12;
	invalid.VisibleAssemblyGroups[0].push_back('x'); if (EncodeConstructionCatalog(invalid, 1, trailing)) return 13;
	if (EncodeConstructionCatalog(source, 0, trailing)) return 14;
	std::cout << "construction catalog codec passed\n";
}
