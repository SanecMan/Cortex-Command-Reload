#include "WorldStateClientReplica.h"

#include "Actor.h"
#include "HeldDevice.h"
#include "MovableMan.h"
#include "MovableObject.h"
#include "PresetMan.h"
#include "SceneObject.h"
#include "Vector.h"

#include <array>
#include <deque>
#include <list>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

using namespace RTE;

namespace {
	using CandidateBuckets = std::unordered_map<std::string, std::deque<MovableObject*>>;

	std::string MakeIdentityKey(const MovableObject& object) {
		const std::array<std::string_view, 3> identity = {object.GetClassName(), object.GetModuleName(), object.GetPresetName()};
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

	void ApplyFields(MovableObject& target, const WorldStateProtocol::ObjectState& state) {
		target.SetPos(Vector(state.PositionX, state.PositionY));
		target.SetPrevPos(target.GetPos());
		target.SetVel(Vector(state.VelocityX, state.VelocityY));
		target.SetRotAngle(state.Rotation);
		target.SetAngularVel(state.AngularVelocity);
		if (target.GetTeam() != state.Team) {
			target.SetTeam(state.Team);
		}
		if (Actor* actor = dynamic_cast<Actor*>(&target)) {
			actor->SetHealth(state.Health);
		}
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

WorldStateClientReplica::ApplyResult WorldStateClientReplica::Apply(const WorldStateProtocol::Snapshot& snapshot) {
	ApplyResult result;

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
			replica = m_Objects.erase(replica);
			continue;
		}
		if (!incomingIds.contains(replica->first)) {
			if (replica->second.ClientOwned && replica->second.Object && g_MovableMan.RemoveMO(replica->second.Object)) {
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
		if (auto existing = m_Objects.find(state.NetworkId); existing != m_Objects.end() && existing->second.Object && activeObjects.contains(existing->second.Object)) {
			object = existing->second.Object;
			clientOwned = existing->second.ClientOwned;
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
			ApplyFields(*object, state);
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
			ApplyFields(*object, state);
			++result.Updated;
		}
		mappedObjects.insert(object);
		m_Objects[state.NetworkId] = {object, clientOwned};
	}
	return result;
}
