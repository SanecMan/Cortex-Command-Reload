#pragma once
#include "WorldStateClientSession.h"
#include "WorldStateAttachments.h"
#include "ObjectScriptExecution.h"
#include "Actor.h"
#include "Controller.h"
#include "PresetMan.h"
#include "SceneMan.h"
#include "CameraMan.h"
#include "FrameMan.h"
#include "GUIFont.h"
#include "AllegroBitmap.h"
#include "UInputMan.h"
#include "ObjectPickerGUI.h"
#include "MovableObject.h"
#include <chrono>
#include <memory>
#include <unordered_set>

namespace RTE {
class WorldStateConstructionMenu {
public:
	~WorldStateConstructionMenu() { Reset(); }
	bool IsActive() const { return m_Active; }
	bool IsPending() const { return m_Pending; }
	bool LastAccepted() const { return m_LastAccepted; }
	bool HasOwnPreview() const { return m_Slot >= 0 && m_Slot < WorldStateProtocol::c_PlayerSlotCount && m_Previews[m_Slot].Object != nullptr; }
	bool IsReady() const { return m_Slot >= 0 && m_Slot < WorldStateProtocol::c_PlayerSlotCount && m_State.Players[m_Slot].Ready; }
	const Vector& GetCursor() const { return m_Cursor; }
	float LastChargedCost() const { return m_LastCost; }
	std::uint64_t LastPlacedObjectId() const { return m_LastPlacedId; }
	bool RemoveLastObject(WorldStateClientSession& client) {
		return RemoveObject(client, m_LastPlacedId);
	}
	std::uint64_t FindOwnedObjectAt(const WorldStateClientSession& client, const Vector& position) const {
		if (!m_Active || m_Slot < 0 || !client.HasSnapshot() || client.GetLatestSnapshot().SceneRevision != m_Revision) return 0;
		std::uint64_t nearest = 0; float distanceSquared = 24.0F * 24.0F;
		for (const auto& object : client.GetLatestSnapshot().Objects) {
			if (!m_OwnedObjects.contains(object.NetworkId) || object.Team != m_State.Players[m_Slot].Team) continue;
			const auto distance = g_SceneMan.ShortestDistance(position, Vector(object.PositionX, object.PositionY), true);
			const float squared = distance.GetX() * distance.GetX() + distance.GetY() * distance.GetY();
			if (squared < distanceSquared || (squared == distanceSquared && (!nearest || object.NetworkId < nearest))) {
				nearest = object.NetworkId; distanceSquared = squared;
			}
		}
		return nearest;
	}
	bool RemoveObjectAt(WorldStateClientSession& client, const Vector& position) {
		const auto target = FindOwnedObjectAt(client, position);
		if (!target) { m_Status = "Move the cursor near an object you placed"; return false; }
		return RemoveObject(client, target);
	}
	bool RemoveObject(WorldStateClientSession& client, std::uint64_t objectId) {
		if (!m_Active || m_Slot < 0 || m_Pending || !m_State.SceneRevision || !objectId || !m_OwnedObjects.contains(objectId)) return false;
		WorldStateProtocol::ConstructionRequest request;
		request.Action = WorldStateProtocol::ConstructionAction::RemoveObject; request.ObjectId = objectId;
		return Send(client, request);
	}
	const std::string& LastRejection() const { return m_LastReason; }
	const SceneObject* GetSelectedObject() const { return m_Selected.get(); }
	ObjectPickerGUI* GetPicker() const { return m_Picker.get(); }
	bool HasAppliedCatalog() const { return m_CatalogSequence != 0 && m_CatalogReady; }
	float GetSelectedCost() const { return m_Selected ? m_Selected->GetTotalValue(m_CatalogNativeModule, m_Catalog.ForeignMultiplier) : 0; }
	bool OpenPicker() {
		if (!m_Active || m_Slot < 0 || m_Pending || !m_CatalogReady) return false;
		if (!m_Picker) {
			m_Picker = std::make_unique<ObjectPickerGUI>(); if (m_Picker->Create(&m_Controller) < 0) { m_Picker.reset(); return false; }
			if (m_CatalogSequence) m_Picker->ApplyNetworkCatalog(m_Catalog, m_CatalogNativeModule, m_CatalogModuleSpace);
		}
		m_Picker->SetEnabled(true); return true;
	}
	bool SelectObject(const SceneObject* preset) {
		const ObjectScriptExecution::SuppressionScope suppress;
		if (!m_Picker || !preset || !m_Picker->IsObjectAvailable(*preset)) return false;
		if (preset->IsInGroup("Brains")) { m_Status = "Press B to place your server-selected brain"; return false; }
		if (m_Selected && m_Selected->GetModuleAndPresetName() == preset->GetModuleAndPresetName() && m_Selected->GetClassName() == preset->GetClassName()) return true;
		std::unique_ptr<Entity> clone(preset->Clone());
		auto* object = dynamic_cast<SceneObject*>(clone.get()); if (!object) return false;
		ClearSelection(); m_Selected.reset(object); clone.release(); return true;
	}
	void SelectBrain() { const ObjectScriptExecution::SuppressionScope suppress; ClearSelection(); m_Picker.reset(); }
	bool PlaceObject(WorldStateClientSession& client, const Vector& position) {
		if (!m_Selected || !m_Active || m_Slot < 0 || m_Pending || !m_State.SceneRevision) return false;
		WorldStateProtocol::ConstructionRequest request;
		request.Action = WorldStateProtocol::ConstructionAction::PlaceObject; request.X = position.GetX(); request.Y = position.GetY();
		request.Rotation = m_Selected->GetRotAngle(); request.HFlipped = m_Selected->IsHFlipped();
		request.Preset = {m_Selected->GetClassName(), m_Selected->GetModuleName(), m_Selected->GetPresetName()};
		if (!Send(client, request)) return false;
		m_Cursor = position; return true;
	}
	void Reset() {
		const ObjectScriptExecution::SuppressionScope suppress;
		ClearSelection(); m_Picker.reset(); m_LastCost = 0; m_LastReason.clear(); m_LastPlacedId = 0; m_OwnedObjects.clear();
		m_Catalog = {}; m_CatalogSequence = 0; m_CatalogReady = true; m_CatalogNativeModule = 0; m_CatalogModuleSpace = -1;
		for (auto& preview : m_Previews) { if (preview.Object) preview.Object->DestroyScriptState(); preview = {}; }
		m_Active = m_Pending = m_LastAccepted = false; m_Revision = 0; m_Slot = -1; m_State = {}; m_Status.clear();
	}
	bool PlaceBrain(WorldStateClientSession& client, const Vector& position) {
		if (!m_Active || m_Slot < 0 || m_Pending || !m_State.SceneRevision) return false;
		WorldStateProtocol::ConstructionRequest request;
		request.Action = WorldStateProtocol::ConstructionAction::PlaceBrain; request.X = position.GetX(); request.Y = position.GetY();
		if (!Send(client, request)) return false;
		m_Cursor = position; return true;
	}
	bool SetReady(WorldStateClientSession& client, bool ready) {
		if (!m_Active || m_Slot < 0 || m_Pending || !m_State.SceneRevision) return false;
		WorldStateProtocol::ConstructionRequest request;
		request.Action = WorldStateProtocol::ConstructionAction::SetReady; request.Ready = ready;
		return Send(client, request);
	}
	void Update(WorldStateClientSession& client, bool physicalInput = true) {
		const ObjectScriptExecution::SuppressionScope suppress;
		if (!client.IsConnected() || !client.HasSnapshot()) { Reset(); return; }
		const auto& snapshot = client.GetLatestSnapshot();
		const int slot = client.GetAssignedPlayerSlot();
		if (m_Revision != snapshot.SceneRevision || m_Slot != slot) {
			Reset(); m_Revision = snapshot.SceneRevision; m_Slot = slot;
			if (slot >= 0 && slot < WorldStateProtocol::c_PlayerSlotCount) {
				m_Controller.Create(Controller::CIM_PLAYER, Players::PlayerOne);
				m_Controller.SetTeam(snapshot.Players[slot].Team);
				m_Cursor = Vector(snapshot.Players[slot].CameraX, snapshot.Players[slot].CameraY);
			}
		}
		m_Active = snapshot.ActivityState == 2;
		if (!m_Active) { if (m_State.SceneRevision || m_Pending) Reset(); return; }
		const auto& incoming = client.GetConstructionState();
		if (!client.HasConstructionState() || incoming.SceneRevision != m_Revision || incoming.ActivityState != 2) {
			m_State = {}; m_Status = "Waiting for construction controls..."; return;
		}
		m_State = incoming;
		const auto& catalog = client.GetConstructionCatalog();
		if (client.GetConstructionCatalogSequence() && catalog.SceneRevision == m_Revision && catalog.PlayerSlot == m_Slot &&
		    (m_CatalogSequence != client.GetConstructionCatalogSequence() || !m_CatalogReady)) {
			const int native = catalog.NativeModule.empty() ? 0 : g_PresetMan.GetModuleID(catalog.NativeModule);
			const int space = catalog.ModuleSpace.empty() ? -1 : g_PresetMan.GetModuleID(catalog.ModuleSpace);
			const bool reopen = m_Picker && m_Picker->IsEnabled();
			ClearSelection(); m_Picker.reset(); m_Catalog = catalog; m_CatalogSequence = client.GetConstructionCatalogSequence();
			m_CatalogReady = native >= 0 && (catalog.ModuleSpace.empty() || space >= 0);
			m_CatalogNativeModule = native; m_CatalogModuleSpace = space;
			if (!m_CatalogReady) m_Status = "Server construction catalog requires a missing module";
			else if (reopen) OpenPicker();
		}
		for (const auto& reply : client.DrainConstructionResults()) {
			if (!m_Pending || reply.RequestId != m_Request.RequestId || reply.SceneRevision != m_Revision) continue;
			m_Pending = false; m_LastAccepted = reply.Accepted;
			m_LastCost = reply.ChargedCost;
			m_LastReason = reply.Reason;
			if (reply.Accepted && m_Request.Action == WorldStateProtocol::ConstructionAction::PlaceObject && reply.ObjectId) {
				m_LastPlacedId = reply.ObjectId; m_OwnedObjects.insert(reply.ObjectId);
			}
			if (m_Request.Action == WorldStateProtocol::ConstructionAction::RemoveObject && (reply.Accepted || reply.Reason == "object_missing")) {
				m_OwnedObjects.erase(m_Request.ObjectId); if (m_LastPlacedId == m_Request.ObjectId) m_LastPlacedId = 0;
			}
			m_Status = reply.Accepted ? "Server confirmed" : RejectionText(reply.Reason);
			if (reply.Accepted && m_Request.Action == WorldStateProtocol::ConstructionAction::RemoveObject) m_Status = "Object removed; placement cost is not refunded";
		}
		for (int player = 0; player < WorldStateProtocol::c_PlayerSlotCount; ++player) {
			auto& preview = m_Previews[player]; const auto& brain = m_State.Players[player].Brain;
			if (!brain) { if (preview.Object) preview.Object->DestroyScriptState(); preview = {}; continue; }
			if (!preview.Object || preview.Id != brain->NetworkId || preview.Identity != brain->ClassName + "/" + brain->ModuleName + "/" + brain->PresetName) {
				if (preview.Object) preview.Object->DestroyScriptState(); preview = {};
				const int module = g_PresetMan.GetModuleID(brain->ModuleName);
				const auto* preset = module < 0 ? nullptr : g_PresetMan.GetEntityPreset(brain->ClassName, brain->PresetName, module);
				if (preset && preset->GetModuleName() == brain->ModuleName && preset->GetClassName() == brain->ClassName && preset->GetPresetName() == brain->PresetName) {
					std::unique_ptr<Entity> clone(preset->Clone());
					if (auto* actor = dynamic_cast<Actor*>(clone.get())) { preview.Object.reset(actor); clone.release(); }
				}
				preview.Id = brain->NetworkId; preview.Identity = brain->ClassName + "/" + brain->ModuleName + "/" + brain->PresetName;
			}
			if (auto* actor = preview.Object.get()) {
				actor->SetPos(Vector(brain->PositionX, brain->PositionY)); actor->SetRotAngle(brain->Rotation);
				actor->SetHFlipped(brain->HFlipped); actor->SetFrame(brain->SpriteFrame); actor->SetTeam(brain->Team);
				if (brain->HasAttachmentState) { WorldStateAttachments::Apply(*actor, *brain); WorldStateAttachments::ApplyPose(*actor, brain->Attachments); }
			}
		}
		if (m_Slot < 0 || m_Slot >= WorldStateProtocol::c_PlayerSlotCount) { m_Status = "Spectator: construction unavailable"; return; }
		if (physicalInput) {
			m_Controller.Update();
			if (!m_Pending && g_UInputMan.KeyPressed(SDLK_F7)) { if (m_Picker && m_Picker->IsEnabled()) m_Picker->SetEnabled(false); else OpenPicker(); }
			if (!m_Pending && g_UInputMan.KeyPressed(SDLK_B)) SelectBrain();
			const bool pickerVisible = m_Picker && m_Picker->IsVisible();
			if (m_Picker) { m_Picker->Update(); if (pickerVisible) { if (const auto* picked = m_Picker->ObjectPicked()) SelectObject(picked); } }
			if (!pickerVisible) {
				m_Controller.RelativeCursorMovement(m_Cursor); g_SceneMan.ForceBounds(m_Cursor);
				if (m_Selected && !m_Pending && g_UInputMan.KeyPressed(SDLK_F8)) m_Selected->SetRotAngle(m_Selected->GetRotAngle() + 0.2617994F);
				if (m_Selected && !m_Pending && g_UInputMan.KeyPressed(SDLK_F9)) m_Selected->SetHFlipped(!m_Selected->IsHFlipped());
				if (!m_Pending && g_UInputMan.KeyPressed(SDLK_DELETE)) RemoveObjectAt(client, m_Cursor);
				if (!m_Pending && (m_Controller.IsState(PRESS_PRIMARY) || g_UInputMan.KeyPressed(SDLK_RETURN))) {
					if (m_Selected) PlaceObject(client, m_Cursor); else PlaceBrain(client, m_Cursor);
				}
				if (!m_Pending && g_UInputMan.KeyPressed(SDLK_F6)) SetReady(client, !IsReady());
			}
		}
		if (!physicalInput && m_Picker) m_Picker->Update();
		if (m_Selected) { m_Selected->SetPos(m_Cursor); m_Selected->SetTeam(m_State.Players[m_Slot].Team); }
		g_CameraMan.SetScrollTarget(m_Cursor, 0.1F, Players::PlayerOne);
		if (m_Pending) {
			if (std::chrono::steady_clock::now() - m_Began >= std::chrono::seconds(8)) { m_Pending = false; m_Status = "Server confirmation timed out"; }
			else if (std::chrono::steady_clock::now() - m_LastSend >= std::chrono::seconds(1)) { client.SendConstructionRequest(m_Request); m_LastSend = std::chrono::steady_clock::now(); }
		}
	}
	void Draw(BITMAP* target, int screen, const Vector& offset) const {
		if (!m_Active || screen != Players::PlayerOne) return;
		for (const auto& preview : m_Previews) if (preview.Object) preview.Object->Draw(target, offset);
		if (m_Selected && (!m_Picker || !m_Picker->IsVisible())) m_Selected->Draw(target, offset);
		const Vector point = m_Cursor - offset;
		circle(target, point.GetFloorIntX(), point.GetFloorIntY(), 14, g_YellowGlowColor);
		line(target, point.GetFloorIntX() - 20, point.GetFloorIntY(), point.GetFloorIntX() + 20, point.GetFloorIntY(), g_YellowGlowColor);
		AllegroBitmap bitmap(target);
		g_FrameMan.GetSmallFont()->DrawAligned(&bitmap, target->w / 2, target->h - 56, m_Selected ? m_Selected->GetPresetName() : "Brain placement", GUIFont::Centre);
		g_FrameMan.GetSmallFont()->DrawAligned(&bitmap, target->w / 2, target->h - 40, "Click: place | F7: objects | B: brain | F8/F9: rotate/flip | Del: object under cursor | F6: ready", GUIFont::Centre);
		g_FrameMan.GetSmallFont()->DrawAligned(&bitmap, target->w / 2, target->h - 24, IsReady() ? "Ready - waiting for other players" : (m_Pending ? "Waiting for server..." : m_Status), GUIFont::Centre);
		if (m_Picker) m_Picker->Draw(target);
	}
private:
	void ClearSelection() { if (auto* movable = dynamic_cast<MovableObject*>(m_Selected.get())) movable->DestroyScriptState(); m_Selected.reset(); }
	static std::string RejectionText(const std::string& reason) {
		if (reason == "outside_scene") return "Choose a position inside the map";
		if (reason == "invalid_brain_placement") return "Your brain needs a clear path to orbit";
		if (reason == "brain_unavailable") return "No brain is available for this activity";
		if (reason == "rate_limit") return "Wait a moment, then try again";
		if (reason == "not_editing") return "Construction has ended";
		if (reason == "stale_scene") return "The map changed; choose a position again";
		if (reason == "spectator") return "Spectators cannot build";
		if (reason == "insufficient_funds") return "Your team cannot afford this object";
		if (reason == "prohibited_item") return "This object is not available for construction";
		if (reason == "terrain_obstructed") return "Choose a clear position for this object";
		if (reason == "invalid_preset") return "The server does not have this object";
		if (reason == "object_not_owned") return "You can only remove objects you placed";
		if (reason == "object_missing") return "This object no longer exists";
		if (reason == "object_attached") return "This object is held or attached; it cannot be removed here";
		if (reason == "construction_limit") return "The construction object limit has been reached";
		return "Server declined the construction request";
	}
	bool Send(WorldStateClientSession& client, WorldStateProtocol::ConstructionRequest request) {
		if (m_NextId == std::numeric_limits<std::uint32_t>::max()) return false;
		request.RequestId = ++m_NextId; request.SceneRevision = m_Revision;
		if (!client.SendConstructionRequest(request)) return false;
	m_Request = std::move(request); m_Pending = true; m_LastAccepted = false; m_LastCost = 0; m_LastReason.clear(); m_Began = m_LastSend = std::chrono::steady_clock::now(); return true;
	}
	struct Preview { std::unique_ptr<Actor> Object; std::uint64_t Id = 0; std::string Identity; };
	std::array<Preview, WorldStateProtocol::c_PlayerSlotCount> m_Previews;
	std::unique_ptr<ObjectPickerGUI> m_Picker;
	std::unique_ptr<SceneObject> m_Selected;
	float m_LastCost = 0;
	WorldStateProtocol::ConstructionCatalog m_Catalog;
	std::uint32_t m_CatalogSequence = 0;
	int m_CatalogNativeModule = 0, m_CatalogModuleSpace = -1;
	bool m_CatalogReady = true;
	std::uint64_t m_LastPlacedId = 0;
	std::unordered_set<std::uint64_t> m_OwnedObjects;
	std::string m_LastReason;
	WorldStateProtocol::ConstructionState m_State;
	WorldStateProtocol::ConstructionRequest m_Request;
	Controller m_Controller;
	Vector m_Cursor;
	std::uint32_t m_Revision = 0, m_NextId = 0;
	int m_Slot = -1;
	bool m_Active = false, m_Pending = false, m_LastAccepted = false;
	std::string m_Status;
	std::chrono::steady_clock::time_point m_LastSend{}, m_Began{};
};
}
