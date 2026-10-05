#include "WorldStateClientReplica.h"

#include "Actor.h"
#include "Atom.h"
#include "Constants.h"
#include "HeldDevice.h"
#include "MOPixel.h"
#include "MovableMan.h"
#include "MovableObject.h"
#include "MOSprite.h"
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

	void ApplyFields(MovableObject& target, const WorldStateProtocol::ObjectState& state) {
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
			const Vector interpolationStartPosition = object->GetPos();
			interpolationStartRotation = object->GetRotAngle();
			const double dx = static_cast<double>(object->GetPos().GetX()) - state.PositionX;
			const double dy = static_cast<double>(object->GetPos().GetY()) - state.PositionY;
			const double positionError = std::sqrt(dx * dx + dy * dy);
			result.TotalPositionErrorBeforeCorrection += positionError;
			result.MaxPositionErrorBeforeCorrection = std::max(result.MaxPositionErrorBeforeCorrection, positionError);
			++result.PositionErrorsMeasured;
			ApplyFields(*object, state);
			if (wasTrackedReplica) {
				object->SetPos(interpolationStartPosition);
				object->SetPrevPos(interpolationStartPosition);
				object->SetRotAngle(interpolationStartRotation);
			}
			++result.Updated;
		}
		mappedObjects.insert(object);
		Replica& replica = m_Objects[state.NetworkId];
		replica.Object = object;
		replica.ClientOwned = clientOwned;
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
