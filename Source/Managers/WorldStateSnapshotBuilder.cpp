#include "WorldStateSnapshotBuilder.h"

#include "Activity.h"
#include "ActivityMan.h"
#include "Actor.h"
#include "HeldDevice.h"
#include "MovableMan.h"
#include "MovableObject.h"
#include "Scene.h"
#include "SceneMan.h"

#include <list>
#include <unordered_set>

using namespace RTE;

WorldStateProtocol::Snapshot WorldStateSnapshotBuilder::Capture(std::uint32_t tick) {
	WorldStateProtocol::Snapshot snapshot;
	snapshot.Tick = tick;
	if (const Activity* activity = g_ActivityMan.GetActivity()) {
		snapshot.ActivityPreset = activity->GetPresetName();
	}
	if (const Scene* scene = g_SceneMan.GetScene()) {
		snapshot.ScenePreset = scene->GetPresetName();
	}

	if (snapshot.ScenePreset != m_LastScenePreset || snapshot.ActivityPreset != m_LastActivityPreset) {
		++m_SceneRevision;
		if (m_SceneRevision == 0) {
			m_SceneRevision = 1;
		}
		m_LastScenePreset = snapshot.ScenePreset;
		m_LastActivityPreset = snapshot.ActivityPreset;
	}
	snapshot.SceneRevision = m_SceneRevision;

	std::list<SceneObject*> objects;
	g_MovableMan.GetAllActors(false, objects);
	g_MovableMan.GetAllItems(false, objects);
	g_MovableMan.GetAllParticles(false, objects);
	std::unordered_set<std::uint64_t> includedObjectIds;
	includedObjectIds.reserve(objects.size());
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
		const std::uint64_t networkId = static_cast<std::uint64_t>(uniqueId);
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
		if (const auto* actor = dynamic_cast<const Actor*>(movableObject)) {
			object.Health = actor->GetHealth();
			object.Flags = WorldStateProtocol::c_ObjectFlagActor;
		} else if (dynamic_cast<const HeldDevice*>(movableObject)) {
			object.Flags = WorldStateProtocol::c_ObjectFlagItem;
		} else {
			object.Flags = WorldStateProtocol::c_ObjectFlagParticle;
		}
		object.Team = static_cast<std::int16_t>(movableObject->GetTeam());
		snapshot.Objects.push_back(std::move(object));
	}
	return snapshot;
}
