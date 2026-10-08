#include "WorldStateClientReplica.h"

#include "Actor.h"
#include "Activity.h"
#include "ActivityMan.h"
#include "AHuman.h"
#include "Arm.h"
#include "Atom.h"
#include "Constants.h"
#include "HeldDevice.h"
#include "HDFirearm.h"
#include "Magazine.h"
#include "MOPixel.h"
#include "MovableMan.h"
#include "MovableObject.h"
#include "MOSprite.h"
#include "MOSRotating.h"
#include "PresetMan.h"
#include "SceneObject.h"
#include "Scene.h"
#include "SceneMan.h"
#include "SLTerrain.h"
#include "Vector.h"

#include <array>
#include <algorithm>
#include <cmath>
#include <deque>
#include <list>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>
#include <memory>
#include <functional>

namespace {
	// Compare identities only: the replica can already have been removed externally.
	void ClearReplicaControlLinks(RTE::MovableObject* object) {
		if (!object) return;
		if (RTE::Activity* activity = RTE::g_ActivityMan.GetActivity()) {
			for (int player = RTE::Players::PlayerOne; player < RTE::Players::MaxPlayerCount; ++player) {
				if (activity->GetControlledActor(player) == object) activity->ClearReplicaControlledActor(player);
				if (activity->GetPlayerBrain(player) == object) activity->SetPlayerBrain(nullptr, player);
			}
		}
	}
}

void RTE::WorldStateClientReplica::Forget() {
	for (const auto& [id, replica] : m_Objects) ClearReplicaControlLinks(replica.Object);
	m_Objects.clear();
	m_InventoryObjects.clear();
	m_LastSnapshotTick = 0;
}

using namespace RTE;

namespace {
	using CandidateBuckets = std::unordered_map<std::string, std::deque<MovableObject*>>;

	std::string MakeIdentityKey(const MovableObject& object) {
		// GetModuleName returns an owning string; keep it alive while using its view.
		const std::string moduleName=object.GetModuleName();
		const std::array<std::string_view, 3> identity = {object.GetClassName(), moduleName, object.GetPresetName()};
		std::string key;
		for (const std::string_view part : identity) {
			key += std::to_string(part.size()) + ':';
			key.append(part);
		}
		return key;
	}

	std::string MakeIdentityKey(const WorldStateProtocol::ObjectState& object) {
		const std::array<std::string_view, 3> identity = {object.ClassName, object.ModuleName, object.PresetName};
		std::string key;
		for (const std::string_view part : identity) {
			key += std::to_string(part.size()) + ':';
			key.append(part);
		}
		return key;
	}

	MovableObject* CloneObject(const WorldStateProtocol::ObjectState& state, std::string& failureReason) {
		if (state.Flags == WorldStateProtocol::c_ObjectFlagTransientPixel) {
			const Material* material = g_SceneMan.GetMaterialFromID(state.PixelMaterialId);
			if (!material || state.PixelColorIndex > 255) {
				failureReason = "invalid-transient-pixel";
				return nullptr;
			}
			auto* pixel = new MOPixel(Color(state.PixelColorIndex), state.PixelMass, Vector(state.PositionX, state.PositionY),
			                          Vector(state.VelocityX, state.VelocityY),
			                          new Atom(Vector(), material, nullptr, Color(state.PixelColorIndex), 0), state.PixelLifetime);
			pixel->SetSharpness(state.PixelSharpness);
			return pixel;
		}
		const int moduleId = state.ModuleName.empty() ? -1 : g_PresetMan.GetModuleID(state.ModuleName);
		const Entity* preset = g_PresetMan.GetEntityPreset(state.ClassName, state.PresetName, moduleId);
		if (!preset) {
			failureReason = "preset-not-found";
			return nullptr;
		}
		Entity* clone = preset->Clone();
		MovableObject* object = dynamic_cast<MovableObject*>(clone);
		if (!object || ((state.Flags & WorldStateProtocol::c_ObjectFlagActor) && !dynamic_cast<Actor*>(object)) ||
		    ((state.Flags & WorldStateProtocol::c_ObjectFlagItem) && !dynamic_cast<HeldDevice*>(object))) {
			failureReason = "class-category-mismatch";
			delete clone;
			return nullptr;
		}
		return object;
	}

	bool ApplyOwnMagazine(HDFirearm& firearm, const WorldStateProtocol::ObjectState& state) {
		if (!state.HasOwnMagazine) { firearm.SetMagazine(nullptr); return firearm.GetMagazine()==nullptr; }
		Magazine* magazine=firearm.GetMagazine();
		auto matches=[&]() { return magazine && magazine->GetClassName()==state.OwnMagazineClassName &&
		    magazine->GetModuleName()==state.OwnMagazineModuleName && magazine->GetPresetName()==state.OwnMagazinePresetName; };
		if (!matches()) {
			const int module=g_PresetMan.GetModuleID(state.OwnMagazineModuleName);
			const Entity* preset=module>=0 ? g_PresetMan.GetEntityPreset(state.OwnMagazineClassName,state.OwnMagazinePresetName,module) : nullptr;
			std::unique_ptr<Entity> clone(preset ? preset->Clone() : nullptr);
			if (auto* replacement=dynamic_cast<Magazine*>(clone.get())) { firearm.SetMagazine(replacement); clone.release(); magazine=replacement; }
		}
		if (!matches()) return false;
		magazine->SetRoundCount(state.OwnMagazineRounds); return magazine->GetRoundCount()==state.OwnMagazineRounds;
	}

	bool ApplyBackgroundDevice(AHuman& human, const WorldStateProtocol::HandDeviceState& state) {
		Arm* arm = human.GetBGArm();
		if (!arm) return !state.Present;
		HeldDevice* device = arm->GetHeldDevice();
		if (!state.Present) {
			if (device) arm->SetHeldDevice(nullptr);
			return arm->GetHeldDevice() == nullptr;
		}
		if (!device || device->GetClassName() != state.ClassName || device->GetModuleName() != state.ModuleName || device->GetPresetName() != state.PresetName) {
			const int module = g_PresetMan.GetModuleID(state.ModuleName);
			const Entity* preset = module >= 0 ? g_PresetMan.GetEntityPreset(state.ClassName, state.PresetName, module) : nullptr;
			std::unique_ptr<Entity> clone(preset ? preset->Clone() : nullptr);
			auto* replacement = dynamic_cast<HeldDevice*>(clone.get());
			if (!replacement || replacement->GetClassName() != state.ClassName || replacement->GetModuleName() != state.ModuleName ||
			    replacement->GetPresetName() != state.PresetName) return false;
			arm->SetHeldDevice(replacement);
			clone.release();
			device = replacement;
		}
		device->SetTeam(human.GetTeam());
		if (auto* firearm = dynamic_cast<HDFirearm*>(device)) {
			WorldStateProtocol::ObjectState magazine;
			magazine.HasOwnMagazine = state.HasMagazine;
			magazine.OwnMagazineClassName = state.MagazineClassName;
			magazine.OwnMagazineModuleName = state.MagazineModuleName;
			magazine.OwnMagazinePresetName = state.MagazinePresetName;
			magazine.OwnMagazineRounds = state.MagazineRounds;
			return ApplyOwnMagazine(*firearm, magazine);
		}
		return !state.HasMagazine;
	}

	bool ApplyFields(MovableObject& target, const WorldStateProtocol::ObjectState& state) {
		target.SetPos(Vector(state.PositionX, state.PositionY));
		target.SetPrevPos(target.GetPos());
		target.SetVel(Vector(state.VelocityX, state.VelocityY));
		target.SetRotAngle(state.Rotation);
		target.SetAngularVel(state.AngularVelocity);
		if (auto* sprite = dynamic_cast<MOSprite*>(&target)) {
			const unsigned int lastFrame = sprite->GetFrameCount() > 0 ? sprite->GetFrameCount() - 1 : 0;
			sprite->SetFrame(std::min<unsigned int>(state.SpriteFrame, lastFrame));
			sprite->SetHFlipped(state.HFlipped);
		}
		if (target.GetTeam() != state.Team) {
			target.SetTeam(state.Team);
		}
		if (Actor* actor = dynamic_cast<Actor*>(&target)) {
			actor->SetHealth(state.Health);
		}
		if (auto* firearm=dynamic_cast<HDFirearm*>(&target)) return ApplyOwnMagazine(*firearm,state);
		if (auto* human = dynamic_cast<AHuman*>(&target)) {
			const bool foregroundApplied = [&]() {
			HeldDevice* equippedDevice = human->GetEquippedItem();
			const bool equippedDeviceMatches = equippedDevice && equippedDevice->GetClassName() == state.EquippedDeviceClassName &&
			                                  equippedDevice->GetModuleName() == state.EquippedDeviceModuleName &&
			                                  equippedDevice->GetPresetName() == state.EquippedDevicePresetName;
			if (!state.HasEquippedDevice) {
				human->UnequipFGArm();
				return human->GetEquippedItem() == nullptr;
			} else {
				if (!equippedDeviceMatches && !human->EquipNamedDevice(state.EquippedDeviceModuleName, state.EquippedDevicePresetName, false)) {
					const int deviceModuleId = state.EquippedDeviceModuleName.empty() ? -1 : g_PresetMan.GetModuleID(state.EquippedDeviceModuleName);
					const Entity* devicePreset = g_PresetMan.GetEntityPreset(state.EquippedDeviceClassName, state.EquippedDevicePresetName, deviceModuleId);
					if (devicePreset) {
						Entity* deviceClone = devicePreset->Clone();
						if (auto* device = dynamic_cast<HeldDevice*>(deviceClone)) {
							human->AddInventoryItem(device);
						} else {
							delete deviceClone;
						}
					}
				}
				human->EquipNamedDevice(state.EquippedDeviceModuleName, state.EquippedDevicePresetName, true);
				equippedDevice = human->GetEquippedItem();
				if (!equippedDevice || equippedDevice->GetClassName() != state.EquippedDeviceClassName ||
				    equippedDevice->GetModuleName() != state.EquippedDeviceModuleName || equippedDevice->GetPresetName() != state.EquippedDevicePresetName) {
					return false;
				}
				if (auto* firearm = dynamic_cast<HDFirearm*>(equippedDevice)) {
					Magazine* magazine = firearm->GetMagazine();
					const bool magazineMatches = magazine && state.HasEquippedDeviceMagazine &&
					                            magazine->GetClassName() == state.EquippedDeviceMagazineClassName &&
					                            magazine->GetModuleName() == state.EquippedDeviceMagazineModuleName &&
					                            magazine->GetPresetName() == state.EquippedDeviceMagazinePresetName;
					if (!state.HasEquippedDeviceMagazine) {
						firearm->SetMagazine(nullptr);
						return firearm->GetMagazine() == nullptr;
					} else {
						if (!magazineMatches) {
							const int magazineModuleId = state.EquippedDeviceMagazineModuleName.empty() ? -1 : g_PresetMan.GetModuleID(state.EquippedDeviceMagazineModuleName);
							const Entity* magazinePreset = g_PresetMan.GetEntityPreset(state.EquippedDeviceMagazineClassName,
							                                                           state.EquippedDeviceMagazinePresetName, magazineModuleId);
							if (magazinePreset) {
								Entity* magazineEntityClone = magazinePreset->Clone();
								if (auto* magazineClone = dynamic_cast<Magazine*>(magazineEntityClone)) {
									firearm->SetMagazine(magazineClone);
									magazine = magazineClone;
								} else {
									delete magazineEntityClone;
								}
							}
						}
						if (magazine && magazine->GetClassName() == state.EquippedDeviceMagazineClassName &&
						    magazine->GetModuleName() == state.EquippedDeviceMagazineModuleName &&
						    magazine->GetPresetName() == state.EquippedDeviceMagazinePresetName) {
							magazine->SetRoundCount(state.EquippedDeviceMagazineRounds);
							return magazine->GetRoundCount() == state.EquippedDeviceMagazineRounds;
						}
						return false;
					}
				}
				return !state.HasEquippedDeviceMagazine;
			}
			}();
			// Foreground equipment selection can automatically equip a shield.
			// Apply the authoritative background state after those side effects.
			const bool backgroundApplied = ApplyBackgroundDevice(*human, state.BackgroundDevice);
			return foregroundApplied && backgroundApplied;
		}
		return true;
	}

	void CollectLocalObjects(std::vector<MovableObject*>& objects) {
		std::list<SceneObject*> collected;
		g_MovableMan.GetAllActors(false, collected);
		g_MovableMan.GetAllItems(false, collected);
		g_MovableMan.GetAllParticles(false, collected);
		objects.reserve(collected.size());
		for (SceneObject* sceneObject : collected) {
			if (auto* object = dynamic_cast<MovableObject*>(sceneObject)) {
				objects.push_back(object);
			}
		}
	}
} // namespace

bool WorldStateClientReplica::TestBackgroundEquipment() {
	const Entity* body = g_PresetMan.GetEntityPreset("AHuman", "Fat Culled Clone", "Base.rte");
	const Entity* gunPreset = g_PresetMan.GetEntityPreset("HDFirearm", "Auto Pistol", "Coalition.rte");
	const auto* gun = dynamic_cast<const HDFirearm*>(gunPreset);
	if (!body || !gun || !gun->GetMagazine()) return false;
	WorldStateProtocol::Snapshot snapshot;
	WorldStateProtocol::ObjectState state;
	state.NetworkId = std::numeric_limits<std::uint64_t>::max() - 10;
	state.ClassName = body->GetClassName(); state.ModuleName = body->GetModuleName(); state.PresetName = body->GetPresetName();
	state.Flags = WorldStateProtocol::c_ObjectFlagActor; state.Team = 0; state.Health = 42;
	state.PositionX = 100000; state.PositionY = 100000;
	state.BackgroundDevice = {true, gun->GetClassName(), gun->GetModuleName(), gun->GetPresetName(), true,
	    gun->GetMagazine()->GetClassName(), gun->GetMagazine()->GetModuleName(), gun->GetMagazine()->GetPresetName(), 17};
	WorldStateClientReplica replica;
	auto apply = [&]() -> AHuman* {
		snapshot.Objects = {state}; ++snapshot.Tick;
		std::vector<std::uint8_t> packet; WorldStateProtocol::Snapshot decoded;
		if (!WorldStateProtocol::EncodeSnapshot(snapshot, snapshot.Tick, packet) || !WorldStateProtocol::DecodeSnapshot(packet, decoded)) return nullptr;
		const auto result = replica.Apply(decoded);
		const auto found = replica.m_Objects.find(state.NetworkId);
		if (result.MissingPresets || found == replica.m_Objects.end()) return nullptr;
		return dynamic_cast<AHuman*>(found->second.Object);
	};
	bool passed = true;
	AHuman* human = apply();
	auto* original = human ? dynamic_cast<HDFirearm*>(human->GetEquippedBGItem()) : nullptr;
	passed = original && original->GetMagazine() && original->GetMagazine()->GetRoundCount() == 17;
	for (const int rounds : {3, -1}) {
		state.BackgroundDevice.MagazineRounds = rounds;
		human = apply();
		auto* current = human ? dynamic_cast<HDFirearm*>(human->GetEquippedBGItem()) : nullptr;
		passed = passed && current == original && current && current->GetMagazine() && current->GetMagazine()->GetRoundCount() == rounds;
	}
	state.BackgroundDevice.HasMagazine = false;
	state.BackgroundDevice.MagazineClassName.clear(); state.BackgroundDevice.MagazineModuleName.clear(); state.BackgroundDevice.MagazinePresetName.clear();
	state.BackgroundDevice.MagazineRounds = 0;
	human = apply();
	auto* unloaded = human ? dynamic_cast<HDFirearm*>(human->GetEquippedBGItem()) : nullptr;
	passed = passed && unloaded == original && unloaded && !unloaded->GetMagazine();
	state.BackgroundDevice = {true, "HeldDevice", "Coalition.rte", "Combat Shield"};
	human = apply();
	passed = passed && human && human->GetEquippedBGItem() && human->GetEquippedBGItem()->GetPresetName() == "Combat Shield";
	// PresetMan can fall back to another official module. Reject that substitute.
	state.BackgroundDevice.ModuleName = "Base.rte";
	snapshot.Objects = {state}; ++snapshot.Tick;
	const auto incompatible = replica.Apply(snapshot);
	passed = passed && incompatible.MissingPresets == 1 && human && human->GetEquippedBGItem() &&
	    human->GetEquippedBGItem()->GetModuleName() == "Coalition.rte";
	state.BackgroundDevice = {};
	human = apply();
	passed = passed && human && !human->GetEquippedBGItem();
	snapshot.Objects.clear();
	const auto removed = replica.Apply(snapshot);
	replica.Forget();
	return passed && removed.Removed == 1;
}

void WorldStateClientReplica::ApplyInventory(const WorldStateProtocol::Snapshot& snapshot, ApplyResult& result) {
	// Validate borrowed pointers against the live ownership tree before consulting cached IDs.
	std::unordered_set<MovableObject*> live;
	std::function<void(Actor&,std::size_t)> collect=[&](Actor& actor,std::size_t depth) {
		if (depth>WorldStateProtocol::c_MaxInventoryDepth) return;
		for (MovableObject* item:*actor.GetInventory()) if (item && live.insert(item).second)
			if (auto* child=dynamic_cast<Actor*>(item)) collect(*child,depth+1);
	};
	std::unordered_map<std::uint64_t,Actor*> parents;
	for (const auto& state:snapshot.Objects) {
		const auto root=m_Objects.find(state.NetworkId);
		if (root!=m_Objects.end()) if (auto* actor=dynamic_cast<Actor*>(root->second.Object)) {
			parents.emplace(state.NetworkId,actor); collect(*actor,0);
		}
	}
	std::unordered_map<MovableObject*,std::uint64_t> cachedIds;
	for (const auto& [id,item]:m_InventoryObjects) if (live.contains(item)) cachedIds.emplace(item,id);
	result.InventoryLiveObjects=static_cast<std::uint32_t>(live.size());
	result.InventoryCachedObjects=static_cast<std::uint32_t>(cachedIds.size());
	m_InventoryObjects.clear();
	// Drain children before their parent. This keeps every reusable item independently owned
	// while transferring between inventories, including cargo actors with their own inventory.
	std::unordered_map<std::uint64_t,std::unique_ptr<MovableObject>> pool;
	std::vector<std::unique_ptr<MovableObject>> unmatched;
	std::function<void(Actor&,std::size_t,bool)> drain=[&](Actor& actor,std::size_t depth,bool reuse) {
		if (depth>WorldStateProtocol::c_MaxInventoryDepth) return;
		while (actor.GetInventorySize()>0) {
			MovableObject* item=actor.RemoveInventoryItemAtIndex(actor.GetInventorySize()-1);
			if (!item) continue;
			if (auto* child=dynamic_cast<Actor*>(item)) drain(*child,depth+1,reuse);
			const auto cached=cachedIds.find(item);
			if (reuse && cached!=cachedIds.end()) pool.emplace(cached->second,std::unique_ptr<MovableObject>(item));
			else unmatched.emplace_back(item);
		}
	};
	for (const auto& [id,parent]:parents) drain(*parent,0,true);
	for (const auto& state:snapshot.Inventory) {
		const auto parent=parents.find(state.ParentNetworkId);
		if (parent==parents.end()) { ++result.MissingPresets; continue; }
		std::unique_ptr<MovableObject> item;
		if (auto existing=pool.find(state.Object.NetworkId); existing!=pool.end()) item=std::move(existing->second);
		if (item && MakeIdentityKey(*item)!=MakeIdentityKey(state.Object)) { item->DestroyScriptState(); item.reset(); }
		if (!item) {
			std::string reason;
			item.reset(CloneObject(state.Object,reason));
			if (!item) { ++result.MissingPresets; continue; }
			if (auto* actor=dynamic_cast<Actor*>(item.get())) drain(*actor,0,false);
			++result.InventoryObjectsSpawned;
		}
		if (!ApplyFields(*item,state.Object)) ++result.MissingPresets;
		if (!parent->second->AddToInventoryBack(item.get())) { item->DestroyScriptState(); ++result.MissingPresets; continue; }
		MovableObject* adopted=item.release();
		m_InventoryObjects.emplace(state.Object.NetworkId,adopted);
		if (auto* actor=dynamic_cast<Actor*>(adopted)) parents.emplace(state.Object.NetworkId,actor);
		++result.InventoryStatesApplied;
	}
	for (auto& [id,item]:pool) if (item) item->DestroyScriptState();
	for (auto& item:unmatched) item->DestroyScriptState();
	result.InventoryMatches=result.InventoryStatesApplied==snapshot.Inventory.size();
	auto mismatch=[&](std::string detail) { result.InventoryMatches=false; if (result.InventoryMismatchDetails.size()<8) result.InventoryMismatchDetails.push_back(std::move(detail)); };
	std::unordered_map<std::uint64_t,std::size_t> expectedSizes;
	for (const auto& state:snapshot.Inventory) {
		++expectedSizes[state.ParentNetworkId];
		const auto parent=parents.find(state.ParentNetworkId);
		const auto item=m_InventoryObjects.find(state.Object.NetworkId);
		if (parent==parents.end() || item==m_InventoryObjects.end() || state.Slot>=parent->second->GetInventory()->size() ||
		    parent->second->GetInventory()->at(state.Slot)!=item->second) mismatch("slot id="+std::to_string(state.Object.NetworkId)+" parent="+std::to_string(state.ParentNetworkId)+" slot="+std::to_string(state.Slot));
		if (item!=m_InventoryObjects.end()) {
			if (MakeIdentityKey(*item->second)!=MakeIdentityKey(state.Object) || item->second->GetTeam()!=state.Object.Team) mismatch("identity id="+std::to_string(state.Object.NetworkId)+" actual="+MakeIdentityKey(*item->second)+" expected="+MakeIdentityKey(state.Object)+" team="+std::to_string(item->second->GetTeam())+" expected_team="+std::to_string(state.Object.Team));
			if (auto* actor=dynamic_cast<Actor*>(item->second); actor && actor->GetHealth()!=state.Object.Health) mismatch("health id="+std::to_string(state.Object.NetworkId)+" actual="+std::to_string(actor->GetHealth())+" expected="+std::to_string(state.Object.Health));
			if (auto* firearm=dynamic_cast<HDFirearm*>(item->second)) {
				const Magazine* magazine=firearm->GetMagazine();
				if (state.Object.HasOwnMagazine ? (!magazine || magazine->GetRoundCount()!=state.Object.OwnMagazineRounds ||
				    magazine->GetModuleName()!=state.Object.OwnMagazineModuleName || magazine->GetPresetName()!=state.Object.OwnMagazinePresetName) : magazine!=nullptr) mismatch("magazine id="+std::to_string(state.Object.NetworkId)+" actual_rounds="+std::to_string(magazine ? magazine->GetRoundCount() : -2)+" expected_rounds="+std::to_string(state.Object.OwnMagazineRounds));
			}
		}
	}
	for (const auto& [id,parent]:parents) if (parent->GetInventory()->size()!=expectedSizes[id]) mismatch("size parent="+std::to_string(id)+" actual="+std::to_string(parent->GetInventorySize())+" expected="+std::to_string(expectedSizes[id]));
}

bool WorldStateClientReplica::AdjustCameraForInterpolation(std::uint64_t networkId, Vector& camera) const {
	const auto replica = m_Objects.find(networkId);
	if (replica == m_Objects.end() || !dynamic_cast<Actor*>(replica->second.Object)) return false;
	camera += g_SceneMan.ShortestDistance(Vector(replica->second.AuthoritativeX, replica->second.AuthoritativeY), replica->second.Object->GetPos());
	return true;
}

bool WorldStateClientReplica::BindPlayerView(std::uint64_t networkId, int team, int localPlayer) const {
	Activity* activity = g_ActivityMan.GetActivity();
	if (!activity) return false;
	Actor* actor = nullptr;
	if (networkId != 0) {
		const auto replica = m_Objects.find(networkId);
		if (replica != m_Objects.end() && g_MovableMan.IsActor(replica->second.Object))
			actor = static_cast<Actor*>(replica->second.Object);
	}
	const bool bound = activity->SetReplicaControlledActor(actor, localPlayer, team);
	return bound && (networkId == 0 || actor != nullptr);
}

WorldStateClientReplica::ApplyResult WorldStateClientReplica::Apply(const WorldStateProtocol::Snapshot& snapshot, bool authoritativeWorld) {
	ApplyResult result;
	std::uint32_t interpolationDurationTicks = 3;
	if (m_LastSnapshotTick != 0) {
		const std::uint32_t elapsedServerTicks = snapshot.Tick - m_LastSnapshotTick;
		if (elapsedServerTicks > 0) {
			interpolationDurationTicks = std::clamp(elapsedServerTicks, 1U, 12U);
		}
	}

	std::vector<MovableObject*> localObjects;
	CollectLocalObjects(localObjects);
	std::unordered_set<MovableObject*> activeObjects(localObjects.begin(), localObjects.end());
	std::unordered_set<std::uint64_t> incomingIds;
	incomingIds.reserve(snapshot.Objects.size());
	for (const WorldStateProtocol::ObjectState& state : snapshot.Objects) {
		incomingIds.insert(state.NetworkId);
	}

	for (auto replica = m_Objects.begin(); replica != m_Objects.end();) {
		if (replica->second.Object && !activeObjects.contains(replica->second.Object)) {
			ClearReplicaControlLinks(replica->second.Object);
			replica = m_Objects.erase(replica);
			continue;
		}
		if (!incomingIds.contains(replica->first)) {
			ClearReplicaControlLinks(replica->second.Object);
			if (replica->second.ClientOwned && replica->second.Object && g_MovableMan.RemoveMO(replica->second.Object)) {
				activeObjects.erase(replica->second.Object);
				replica->second.Object->DestroyScriptState();
				delete replica->second.Object;
				++result.Removed;
			}
			replica = m_Objects.erase(replica);
			continue;
		}
		++replica;
	}

	CandidateBuckets candidates;
	bool candidatesBuilt = false;
	auto buildCandidates = [&]() {
		if (candidatesBuilt) return;
		localObjects.clear();
		CollectLocalObjects(localObjects);
		activeObjects.clear();
		activeObjects.insert(localObjects.begin(), localObjects.end());
		for (MovableObject* localObject : localObjects) {
			candidates[MakeIdentityKey(*localObject)].push_back(localObject);
		}
		candidatesBuilt = true;
	};
	std::unordered_set<MovableObject*> mappedObjects;
	mappedObjects.reserve(snapshot.Objects.size());
	for (const auto& [networkId, replica] : m_Objects) {
		if (replica.Object && activeObjects.contains(replica.Object)) {
			mappedObjects.insert(replica.Object);
		}
	}

	for (const WorldStateProtocol::ObjectState& state : snapshot.Objects) {
		MovableObject* object = nullptr;
		bool clientOwned = false;
		bool wasTrackedReplica = false;
		float interpolationStartRotation = 0.0F;
		if (auto existing = m_Objects.find(state.NetworkId); existing != m_Objects.end() && existing->second.Object && activeObjects.contains(existing->second.Object)) {
			object = existing->second.Object;
			clientOwned = existing->second.ClientOwned;
			wasTrackedReplica = true;
		}
		if (!object) {
			buildCandidates();
			auto bucket = candidates.find(MakeIdentityKey(state));
			if (bucket != candidates.end()) {
				while (!bucket->second.empty() && mappedObjects.contains(bucket->second.front())) {
					bucket->second.pop_front();
				}
				if (!bucket->second.empty()) {
					MovableObject* candidate = bucket->second.front();
					bucket->second.pop_front();
					const float dx = candidate->GetPos().GetX() - state.PositionX;
					const float dy = candidate->GetPos().GetY() - state.PositionY;
					if (dx * dx + dy * dy <= 80.0F * 80.0F) {
						object = candidate;
					}
				}
			}
		}
		if (!object) {
			std::string failureReason;
			object = CloneObject(state, failureReason);
			if (!object) {
				++result.MissingPresets;
				if (result.MissingPresetDetails.size() < 8) {
					result.MissingPresetDetails.push_back(failureReason + "|" + state.ClassName + "|" + state.ModuleName + "|" + state.PresetName);
				}
				continue;
			}
			const bool fieldsApplied = ApplyFields(*object, state);
			if (state.ClassName == "AHuman") {
				if (fieldsApplied) ++result.HumanEquipmentStatesApplied;
				else ++result.MissingPresets;
			} else if (!fieldsApplied) ++result.MissingPresets;
			if (!g_MovableMan.AddMO(object)) {
				object->DestroyScriptState();
				delete object;
				++result.MissingPresets;
				continue;
			}
			clientOwned = true;
			++result.Spawned;
			activeObjects.insert(object);
		} else {
			const Vector interpolationStartPosition = object->GetPos();
			interpolationStartRotation = object->GetRotAngle();
			const double dx = static_cast<double>(object->GetPos().GetX()) - state.PositionX;
			const double dy = static_cast<double>(object->GetPos().GetY()) - state.PositionY;
			const double positionError = std::sqrt(dx * dx + dy * dy);
			result.TotalPositionErrorBeforeCorrection += positionError;
			result.MaxPositionErrorBeforeCorrection = std::max(result.MaxPositionErrorBeforeCorrection, positionError);
			++result.PositionErrorsMeasured;
			const bool fieldsApplied = ApplyFields(*object, state);
			if (state.ClassName == "AHuman") {
				if (fieldsApplied) ++result.HumanEquipmentStatesApplied;
				else ++result.MissingPresets;
			} else if (!fieldsApplied) ++result.MissingPresets;
			if (wasTrackedReplica) {
				object->SetPos(interpolationStartPosition);
				object->SetPrevPos(interpolationStartPosition);
				object->SetRotAngle(interpolationStartRotation);
			}
			++result.Updated;
		}
		mappedObjects.insert(object);
		if (auto* rotating = dynamic_cast<MOSRotating*>(object)) rotating->CorrectAttachableAndWoundPositionsAndRotations();
		Replica& replica = m_Objects[state.NetworkId];
		replica.Object = object;
		replica.ClientOwned = clientOwned || authoritativeWorld;
		replica.AuthoritativeX = state.PositionX;
		replica.AuthoritativeY = state.PositionY;
		if (wasTrackedReplica) {
			replica.InterpolationStartX = object->GetPos().GetX();
			replica.InterpolationStartY = object->GetPos().GetY();
			const Vector interpolationDelta = g_SceneMan.ShortestDistance(object->GetPos(), Vector(state.PositionX, state.PositionY));
			replica.InterpolationDeltaX = interpolationDelta.GetX();
			replica.InterpolationDeltaY = interpolationDelta.GetY();
			replica.InterpolationStartRotation = interpolationStartRotation;
			replica.InterpolationDeltaRotation = std::remainder(state.Rotation - interpolationStartRotation, c_TwoPI);
			++result.RotationInterpolationsScheduled;
			result.MaxRotationInterpolationDeltaRadians = std::max(result.MaxRotationInterpolationDeltaRadians,
			                                                        std::abs(static_cast<double>(replica.InterpolationDeltaRotation)));
			replica.InterpolationElapsedTicks = 0;
			replica.InterpolationDurationTicks = interpolationDurationTicks;
		} else {
			replica.InterpolationElapsedTicks = 0;
			replica.InterpolationDurationTicks = 0;
		}
	}
	if (authoritativeWorld) {
		ApplyInventory(snapshot,result);
		if (Activity* activity = g_ActivityMan.GetActivity()) {
			activity->SetActivityState(static_cast<Activity::ActivityState>(snapshot.ActivityState));
			for (int team = 0; team < Activity::MaxTeamCount; ++team) activity->SetTeamFunds(snapshot.TeamFunds[team], team);
		}
		// Local Activity startup can spawn objects independently. A full server
		// snapshot is the only world authority for this client, including absence.
		for (MovableObject* object : activeObjects) {
			if (!mappedObjects.contains(object) && g_MovableMan.RemoveMO(object)) {
				ClearReplicaControlLinks(object);
				object->DestroyScriptState();
				delete object;
				++result.Removed;
			}
		}
	}
	m_LastSnapshotTick = snapshot.Tick;
	if (result.PositionErrorsMeasured > 0) {
		result.MeanPositionErrorBeforeCorrection = result.TotalPositionErrorBeforeCorrection / result.PositionErrorsMeasured;
	}
	return result;
}

std::uint32_t WorldStateClientReplica::AdvanceInterpolation() {
	std::uint32_t advancedObjects = 0;
	for (auto& [networkId, replica] : m_Objects) {
		(void)networkId;
		if (!replica.Object || replica.InterpolationDurationTicks == 0) {
			continue;
		}
		replica.InterpolationElapsedTicks = std::min(replica.InterpolationElapsedTicks + 1, replica.InterpolationDurationTicks);
		const float progress = static_cast<float>(replica.InterpolationElapsedTicks) / static_cast<float>(replica.InterpolationDurationTicks);
		const Vector currentPosition = replica.Object->GetPos();
		replica.Object->SetPrevPos(currentPosition);
		Vector interpolatedPosition(replica.InterpolationStartX + replica.InterpolationDeltaX * progress,
		                            replica.InterpolationStartY + replica.InterpolationDeltaY * progress);
		g_SceneMan.WrapPosition(interpolatedPosition);
		replica.Object->SetPos(interpolatedPosition);
		replica.Object->SetRotAngle(replica.InterpolationStartRotation + replica.InterpolationDeltaRotation * progress);
		if (auto* rotating = dynamic_cast<MOSRotating*>(replica.Object)) rotating->CorrectAttachableAndWoundPositionsAndRotations();
		++advancedObjects;
		if (replica.InterpolationElapsedTicks == replica.InterpolationDurationTicks) {
			replica.InterpolationDurationTicks = 0;
		}
	}
	return advancedObjects;
}

bool WorldStateClientReplica::ApplyTerrainPatch(const WorldStateProtocol::TerrainPatch& patch) {
	Scene* scene = g_SceneMan.GetScene();
	if (!scene || !scene->GetTerrain() || patch.SceneRevision == 0 || patch.Width == 0 || patch.Height == 0 ||
	    patch.Width > WorldStateProtocol::c_MaxTerrainPatchBytes / patch.Height ||
	    patch.Pixels.size() != static_cast<std::size_t>(patch.Width) * patch.Height ||
	    static_cast<std::uint64_t>(patch.X) + patch.Width > static_cast<std::uint64_t>(g_SceneMan.GetSceneWidth()) ||
	    static_cast<std::uint64_t>(patch.Y) + patch.Height > static_cast<std::uint64_t>(g_SceneMan.GetSceneHeight())) {
		return false;
	}
	SLTerrain* terrain = scene->GetTerrain();
	for (std::uint32_t row = 0; row < patch.Height; ++row) {
		for (std::uint32_t column = 0; column < patch.Width; ++column) {
			const int x = static_cast<int>(patch.X + column);
			const int y = static_cast<int>(patch.Y + row);
			const int value = patch.Pixels[static_cast<std::size_t>(row) * patch.Width + column];
			switch (patch.Layer) {
			case WorldStateProtocol::TerrainLayer::Material: terrain->SetMaterialPixel(x, y, value); break;
			case WorldStateProtocol::TerrainLayer::Foreground: terrain->SetFGColorPixel(x, y, value); break;
			case WorldStateProtocol::TerrainLayer::Background: terrain->SetBGColorPixel(x, y, value); break;
			default: return false;
			}
		}
	}
	if (patch.Layer == WorldStateProtocol::TerrainLayer::Material) {
		terrain->AddUpdatedMaterialArea(Box(Vector(static_cast<float>(patch.X), static_cast<float>(patch.Y)), static_cast<float>(patch.Width), static_cast<float>(patch.Height)));
	}
	return true;
}
