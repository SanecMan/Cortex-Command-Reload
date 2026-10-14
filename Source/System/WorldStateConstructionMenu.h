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
#include <chrono>
#include <memory>

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
	void Reset() {
		const ObjectScriptExecution::SuppressionScope suppress;
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
		for (const auto& reply : client.DrainConstructionResults()) {
			if (!m_Pending || reply.RequestId != m_Request.RequestId || reply.SceneRevision != m_Revision) continue;
			m_Pending = false; m_LastAccepted = reply.Accepted;
			m_Status = reply.Accepted ? "Server confirmed" : RejectionText(reply.Reason);
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
			m_Controller.Update(); m_Controller.RelativeCursorMovement(m_Cursor); g_SceneMan.ForceBounds(m_Cursor);
			if (!m_Pending && (m_Controller.IsState(PRESS_PRIMARY) || g_UInputMan.KeyPressed(SDLK_RETURN))) PlaceBrain(client, m_Cursor);
			if (!m_Pending && g_UInputMan.KeyPressed(SDLK_F6)) SetReady(client, !IsReady());
		}
		g_CameraMan.SetScrollTarget(m_Cursor, 0.1F, Players::PlayerOne);
		if (m_Pending) {
			if (std::chrono::steady_clock::now() - m_Began >= std::chrono::seconds(8)) { m_Pending = false; m_Status = "Server confirmation timed out"; }
			else if (std::chrono::steady_clock::now() - m_LastSend >= std::chrono::seconds(1)) { client.SendConstructionRequest(m_Request); m_LastSend = std::chrono::steady_clock::now(); }
		}
	}
	void Draw(BITMAP* target, int screen, const Vector& offset) const {
		if (!m_Active || screen != Players::PlayerOne) return;
		for (const auto& preview : m_Previews) if (preview.Object) preview.Object->Draw(target, offset);
		const Vector point = m_Cursor - offset;
		circle(target, point.GetFloorIntX(), point.GetFloorIntY(), 14, g_YellowGlowColor);
		line(target, point.GetFloorIntX() - 20, point.GetFloorIntY(), point.GetFloorIntX() + 20, point.GetFloorIntY(), g_YellowGlowColor);
		AllegroBitmap bitmap(target);
		g_FrameMan.GetSmallFont()->DrawAligned(&bitmap, target->w / 2, target->h - 40, "Mouse/arrows: position | Click/Enter: place brain | F6: ready/cancel", GUIFont::Centre);
		g_FrameMan.GetSmallFont()->DrawAligned(&bitmap, target->w / 2, target->h - 24, IsReady() ? "Ready - waiting for other players" : (m_Pending ? "Waiting for server..." : m_Status), GUIFont::Centre);
	}
private:
	static std::string RejectionText(const std::string& reason) {
		if (reason == "outside_scene") return "Choose a position inside the map";
		if (reason == "invalid_brain_placement") return "Your brain needs a clear path to orbit";
		if (reason == "brain_unavailable") return "No brain is available for this activity";
		if (reason == "rate_limit") return "Wait a moment, then try again";
		if (reason == "not_editing") return "Construction has ended";
		if (reason == "stale_scene") return "The map changed; choose a position again";
		if (reason == "spectator") return "Spectators cannot build";
		return "Server declined the construction request";
	}
	bool Send(WorldStateClientSession& client, WorldStateProtocol::ConstructionRequest request) {
		if (m_NextId == std::numeric_limits<std::uint32_t>::max()) return false;
		request.RequestId = ++m_NextId; request.SceneRevision = m_Revision;
		if (!client.SendConstructionRequest(request)) return false;
		m_Request = std::move(request); m_Pending = true; m_LastAccepted = false; m_Began = m_LastSend = std::chrono::steady_clock::now(); return true;
	}
	struct Preview { std::unique_ptr<Actor> Object; std::uint64_t Id = 0; std::string Identity; };
	std::array<Preview, WorldStateProtocol::c_PlayerSlotCount> m_Previews;
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
