#include "WorldStateSnapshotBuilder.h"

#include "Activity.h"
#include "ActivityMan.h"
#include "Actor.h"
#include "CameraMan.h"
#include "AHuman.h"
#include "HDFirearm.h"
#include "HeldDevice.h"
#include "Magazine.h"
#include "MovableMan.h"
#include "MovableObject.h"
#include "MOPixel.h"
#include "MOSprite.h"
#include "Scene.h"
#include "SceneMan.h"

#include <algorithm>
#include <limits>
#include <list>
#include <unordered_set>

using namespace RTE;
static_assert(Activity::NoActivity == WorldStateProtocol::c_NoActivity && Activity::Over == WorldStateProtocol::c_MaxActivityState);
static_assert(Activity::MaxTeamCount == WorldStateProtocol::c_TeamCount);

WorldStateProtocol::Snapshot WorldStateSnapshotBuilder::CaptureIdentity() {
	WorldStateProtocol::Snapshot snapshot;
	Activity* activity = g_ActivityMan.GetActivity();
	const Scene* scene = g_SceneMan.GetScene();
	if (activity) {
		snapshot.ActivityState = static_cast<std::int8_t>(activity->GetActivityState());
		for (int team = 0; team < Activity::MaxTeamCount; ++team) snapshot.TeamFunds[team] = activity->GetTeamFunds(team);
		snapshot.ActivityClassName = activity->GetClass().GetName();
		snapshot.ActivityPreset = activity->GetPresetName();
		snapshot.ActivityModuleName = activity->GetModuleName();
	}
	if (scene) {
		snapshot.SceneModuleName = scene->GetModuleName();
		snapshot.ScenePreset = scene->GetPresetName();
	}
	const std::uintptr_t activityAddress = reinterpret_cast<std::uintptr_t>(activity);
	const std::uintptr_t sceneAddress = reinterpret_cast<std::uintptr_t>(scene);

	if (sceneAddress != m_LastSceneAddress || activityAddress != m_LastActivityAddress || snapshot.ScenePreset != m_LastScenePreset ||
	    snapshot.ActivityClassName != m_LastActivityClassName || snapshot.ActivityPreset != m_LastActivityPreset ||
	    snapshot.ActivityModuleName != m_LastActivityModuleName || snapshot.SceneModuleName != m_LastSceneModuleName) {
		++m_SceneRevision;
		if (m_SceneRevision == 0) {
			m_SceneRevision = 1;
		}
		m_LastSceneAddress = sceneAddress;
		m_LastActivityAddress = activityAddress;
		m_LastScenePreset = snapshot.ScenePreset;
		m_LastActivityClassName = snapshot.ActivityClassName;
		m_LastActivityPreset = snapshot.ActivityPreset;
		m_LastActivityModuleName = snapshot.ActivityModuleName;
		m_LastSceneModuleName = snapshot.SceneModuleName;
	}
	snapshot.SceneRevision = m_SceneRevision;
	return snapshot;
}

WorldStateProtocol::Snapshot WorldStateSnapshotBuilder::Capture(std::uint32_t tick) {
	WorldStateProtocol::Snapshot snapshot = CaptureIdentity();
	snapshot.Tick = tick;
	Activity* activity = g_ActivityMan.GetActivity();

	std::list<SceneObject*> objects;
	g_MovableMan.GetAllActors(false, objects);
	g_MovableMan.GetAllItems(false, objects);
	g_MovableMan.GetAllParticles(false, objects);
	std::unordered_set<std::uint64_t> includedObjectIds;
	includedObjectIds.reserve(objects.size());
	std::unordered_set<long> liveRuntimeIds;
	std::unordered_map<const Actor*, std::uint64_t> actorNetworkIds;
	liveRuntimeIds.reserve(objects.size());
	snapshot.Objects.reserve(objects.size());
	for (const SceneObject* sceneObject : objects) {
		const auto* movableObject = dynamic_cast<const MovableObject*>(sceneObject);
		if (!movableObject) {
			continue;
		}
		const long uniqueId = movableObject->GetUniqueID();
		if (uniqueId <= 0) {
			continue;
		}
		liveRuntimeIds.insert(uniqueId);
		auto [networkIdEntry, inserted] = m_NetworkIdsByRuntimeId.try_emplace(uniqueId, m_NextNetworkId);
		if (inserted) {
			++m_NextNetworkId;
			if (m_NextNetworkId == 0) {
				m_NextNetworkId = 1;
			}
		}
		const std::uint64_t networkId = networkIdEntry->second;
		if (!includedObjectIds.insert(networkId).second) {
			continue;
		}

		WorldStateProtocol::ObjectState object;
		object.NetworkId = networkId;
		object.ClassName = movableObject->GetClass().GetName();
		object.ModuleName = movableObject->GetModuleName();
		object.PresetName = movableObject->GetPresetName();
		object.PositionX = movableObject->GetPos().GetX();
		object.PositionY = movableObject->GetPos().GetY();
		object.VelocityX = movableObject->GetVel().GetX();
		object.VelocityY = movableObject->GetVel().GetY();
		object.Rotation = movableObject->GetRotAngle();
		object.AngularVelocity = movableObject->GetAngularVel();
		if (const auto* sprite = dynamic_cast<const MOSprite*>(movableObject)) {
			object.SpriteFrame = static_cast<std::uint16_t>(std::min(sprite->GetFrame(), static_cast<unsigned int>(std::numeric_limits<std::uint16_t>::max())));
			object.HFlipped = sprite->IsHFlipped();
		}
		if (const auto* actor = dynamic_cast<const Actor*>(movableObject)) {
			actorNetworkIds.emplace(actor, networkId);
			object.Health = actor->GetHealth();
			object.Flags = WorldStateProtocol::c_ObjectFlagActor;
			if (const auto* human = dynamic_cast<const AHuman*>(actor)) {
				if (const HeldDevice* device = human->GetEquippedItem()) {
					object.HasEquippedDevice = true;
					object.EquippedDeviceClassName = device->GetClassName();
					object.EquippedDeviceModuleName = device->GetModuleName();
					object.EquippedDevicePresetName = device->GetPresetName();
					if (const auto* firearm = dynamic_cast<const HDFirearm*>(device)) {
						if (const Magazine* magazine = firearm->GetMagazine()) {
							object.HasEquippedDeviceMagazine = true;
							object.EquippedDeviceMagazineClassName = magazine->GetClassName();
							object.EquippedDeviceMagazineModuleName = magazine->GetModuleName();
							object.EquippedDeviceMagazinePresetName = magazine->GetPresetName();
							object.EquippedDeviceMagazineRounds = magazine->GetRoundCount();
						}
					}
				}
			}
		} else if (dynamic_cast<const HeldDevice*>(movableObject)) {
			object.Flags = WorldStateProtocol::c_ObjectFlagItem;
		} else if (const auto* pixel = dynamic_cast<const MOPixel*>(movableObject); pixel && (object.PresetName.empty() || object.PresetName == "None")) {
			object.Flags = WorldStateProtocol::c_ObjectFlagTransientPixel;
			object.PixelMaterialId = pixel->GetMaterial()->GetIndex();
			object.PixelColorIndex = static_cast<std::uint16_t>(pixel->GetColorIndex());
			object.PixelMass = pixel->GetMass();
			object.PixelLifetime = static_cast<std::uint32_t>(pixel->GetLifetime());
			object.PixelSharpness = pixel->GetSharpness();
		} else {
			object.Flags = WorldStateProtocol::c_ObjectFlagParticle;
		}
		object.Team = static_cast<std::int16_t>(movableObject->GetTeam());
		snapshot.Objects.push_back(std::move(object));
	}
	if (activity) {
		for (int slot = Players::PlayerOne; slot < Players::MaxPlayerCount; ++slot) {
			auto& player = snapshot.Players[slot];
			player.ActiveHuman = activity->PlayerActive(slot) && activity->PlayerHuman(slot);
			if (!player.ActiveHuman) continue;
			player.Team = static_cast<std::int16_t>(activity->GetTeamOfPlayer(slot));
			// Resolve through the actors captured above instead of dereferencing an
			// Activity pointer that may outlive its removed actor during a restart.
			const auto controlled = actorNetworkIds.find(activity->GetControlledActor(slot));
			Vector camera;
			if (controlled != actorNetworkIds.end()) {
				player.ControlledActorId = controlled->second;
				camera = controlled->first->GetViewPoint();
			} else {
				const int screen = activity->ScreenOfPlayer(slot);
				if (screen >= 0 && screen < Players::MaxPlayerCount) camera = g_CameraMan.GetScrollTarget(screen);
			}
			player.CameraX = camera.GetX();
			player.CameraY = camera.GetY();
		}
	}
	for (auto networkIdEntry = m_NetworkIdsByRuntimeId.begin(); networkIdEntry != m_NetworkIdsByRuntimeId.end();) {
		if (!liveRuntimeIds.contains(networkIdEntry->first)) {
			networkIdEntry = m_NetworkIdsByRuntimeId.erase(networkIdEntry);
		} else {
			++networkIdEntry;
		}
	}
	return snapshot;
}
