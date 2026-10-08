#pragma once
#include "WorldStateProtocol.h"
#include "ACDropShip.h"
#include "AHuman.h"
#include "HDFirearm.h"
#include "Magazine.h"
#include "MovableMan.h"
#include "PresetMan.h"
#include "ThreadMan.h"
#include <memory>

namespace RTE {
inline bool TestWorldStateInventoryCodec() {
	using namespace WorldStateProtocol;
	Snapshot source; source.SceneRevision=1;
	ObjectState root; root.NetworkId=1; root.ClassName="ACDropShip"; root.ModuleName="Base.rte"; root.PresetName="Dropship MK1"; root.Flags=c_ObjectFlagActor;
	source.Objects.push_back(root);
	ObjectState passenger=root; passenger.NetworkId=2; passenger.ClassName="AHuman"; passenger.PresetName="Fat Culled Clone"; passenger.Health=42.25F;
	ObjectState gun; gun.NetworkId=3; gun.ClassName="HDFirearm"; gun.ModuleName="Coalition.rte"; gun.PresetName="Auto Pistol"; gun.Flags=c_ObjectFlagItem;
	gun.HasOwnMagazine=true; gun.OwnMagazineClassName="Magazine"; gun.OwnMagazineModuleName="Coalition.rte"; gun.OwnMagazinePresetName="Magazine"; gun.OwnMagazineRounds=7;
	source.Inventory={{1,0,passenger},{2,0,gun}};
	gun.NetworkId=4; gun.OwnMagazineRounds=-1; source.Inventory.push_back({2,1,gun});
	std::vector<std::uint8_t> bytes; Snapshot decoded;
	if (!EncodeSnapshot(source,1,bytes) || !DecodeSnapshot(bytes,decoded) || decoded.Inventory!=source.Inventory || decoded.Objects!=source.Objects) return false;
	const auto valid=bytes;
	for (std::size_t length=0;length<valid.size();++length) {
		decoded=source; if (DecodeSnapshot(std::span(valid).first(length),decoded) || decoded.Inventory!=source.Inventory) return false;
	}
	bytes.push_back(0); if (DecodeSnapshot(bytes,decoded)) return false;
	auto rejected=[&](const Snapshot& invalid) { return !EncodeSnapshot(invalid,1,bytes); };
	auto invalid=source; invalid.Inventory[0].ParentNetworkId=2; if (!rejected(invalid)) return false; // Self/cyclic parent.
	invalid=source; invalid.Inventory[1].ParentNetworkId=3; if (!rejected(invalid)) return false;
	invalid=source; invalid.Inventory[1].Object.NetworkId=1; if (!rejected(invalid)) return false;
	invalid=source; invalid.Inventory[2].Slot=4; if (!rejected(invalid)) return false;
	invalid=source; invalid.Inventory[1].Object.OwnMagazineRounds=-2; if (!rejected(invalid)) return false;
	invalid=source; invalid.Inventory[1].Object.PresetName="\xFF"; if (!rejected(invalid)) return false;
	invalid=source; invalid.InventoryComplete=false; if (!rejected(invalid)) return false;
	invalid=source; invalid.Inventory.clear();
	for (std::size_t depth=0;depth<c_MaxInventoryDepth;++depth) {
		passenger.NetworkId=depth+2; invalid.Inventory.push_back({depth+1,0,passenger});
	}
	if (!EncodeSnapshot(invalid,1,bytes) || !DecodeSnapshot(bytes,decoded)) return false;
	passenger.NetworkId=c_MaxInventoryDepth+2; invalid.Inventory.push_back({c_MaxInventoryDepth+1,0,passenger});
	return rejected(invalid);
}

// These native objects belong to a disposable diagnostic server session only.
inline ACDropShip* InventoryFixtureCarrier=nullptr;
inline HDFirearm* CloneInventoryFixtureGun(int rounds) {
	const Entity* preset=g_PresetMan.GetEntityPreset("HDFirearm","Auto Pistol","Coalition.rte");
	std::unique_ptr<Entity> clone(preset ? preset->Clone() : nullptr);
	auto* gun=dynamic_cast<HDFirearm*>(clone.get());
	if (!gun || !gun->GetMagazine()) return nullptr;
	gun->SetTeam(0); gun->GetMagazine()->SetRoundCount(rounds); clone.release(); return gun;
}
inline bool CreateWorldStateInventoryFixture() {
	if (InventoryFixtureCarrier && g_MovableMan.IsActor(InventoryFixtureCarrier)) return false;
	g_ThreadMan.GetPriorityThreadPool().wait_for_tasks();
	const Entity* preset=g_PresetMan.GetEntityPreset("ACDropShip","Dropship MK1","Base.rte");
	std::unique_ptr<Entity> clone(preset ? preset->Clone() : nullptr);
	auto* carrier=dynamic_cast<ACDropShip*>(clone.get());
	const Entity* body=g_PresetMan.GetEntityPreset("AHuman","Fat Culled Clone","Base.rte");
	if (!carrier || !body) return false;
	while (carrier->GetInventorySize()) delete carrier->RemoveInventoryItemAtIndex(0);
	carrier->SetTeam(0); carrier->SetHealth(73.375F); carrier->SetGlobalAccScalar(0); carrier->SetVel(Vector()); carrier->SetPos(Vector(250,30));
	carrier->SetPinStrength(1000000); carrier->SetHatchDelay(600000);
	carrier->GetController()->SetInputMode(Controller::CIM_DISABLED); carrier->GetController()->SetDisabled(true);
	carrier->SetAIMode(Actor::AIMODE_NONE); carrier->CloseHatch();
	for (int index=0;index<2;++index) {
		std::unique_ptr<Entity> passengerClone(body->Clone());
		auto* passenger=dynamic_cast<AHuman*>(passengerClone.get());
		std::unique_ptr<HDFirearm> gun(CloneInventoryFixtureGun(7));
		if (!passenger || !gun) return false;
		while (passenger->GetInventorySize()) delete passenger->RemoveInventoryItemAtIndex(0);
		passenger->SetTeam(0); passenger->SetHealth(42.25F+index);
		if (!passenger->AddToInventoryBack(gun.get())) return false;
		gun.release();
		if (!carrier->AddToInventoryBack(passenger)) return false;
		passengerClone.release();
	}
	if (!g_MovableMan.AddMO(carrier)) return false;
	InventoryFixtureCarrier=carrier; clone.release(); return true;
}
inline bool MutateWorldStateInventoryFixture() {
	if (!InventoryFixtureCarrier || !g_MovableMan.IsActor(InventoryFixtureCarrier) || InventoryFixtureCarrier->GetInventorySize()!=2) return false;
	g_ThreadMan.GetPriorityThreadPool().wait_for_tasks();
	auto* passenger=dynamic_cast<Actor*>(InventoryFixtureCarrier->GetInventory()->at(0));
	if (!passenger || passenger->GetInventorySize()!=1) return false;
	auto* gun=dynamic_cast<HDFirearm*>(passenger->GetInventory()->at(0));
	std::unique_ptr<HDFirearm> added(CloneInventoryFixtureGun(5));
	if (!gun || !gun->GetMagazine() || !added || !passenger->AddToInventoryBack(added.get())) return false;
	added.release(); gun->GetMagazine()->SetRoundCount(3);
	return InventoryFixtureCarrier->SwapInventoryItemsByIndex(0,1);
}
inline int ObserveWorldStateInventoryFixture(const WorldStateProtocol::Snapshot& snapshot) {
	for (const auto& root:snapshot.Objects) if (root.ClassName=="ACDropShip" && root.Health==73.375F) {
		std::uint64_t passenger=0; bool initialOrder=false, swappedOrder=false;
		for (const auto& item:snapshot.Inventory) if (item.ParentNetworkId==root.NetworkId) {
			if (item.Object.Health==42.25F) { passenger=item.Object.NetworkId; initialOrder=item.Slot==0; swappedOrder=item.Slot==1; }
		}
		bool seven=false, three=false, five=false; int guns=0;
		for (const auto& item:snapshot.Inventory) if (item.ParentNetworkId==passenger && item.Object.ClassName=="HDFirearm") {
			++guns; seven=seven || item.Object.OwnMagazineRounds==7; three=three || item.Object.OwnMagazineRounds==3; five=five || item.Object.OwnMagazineRounds==5;
		}
		if (initialOrder && guns==1 && seven) return 1;
		if (swappedOrder && guns==2 && three && five) return 2;
	}
	return 0;
}
}
