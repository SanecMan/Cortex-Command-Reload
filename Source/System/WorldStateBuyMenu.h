#pragma once

#include "WorldStateClientSession.h"
#include "BuyMenuGUI.h"
#include "Controller.h"
#include "FrameMan.h"
#include "SceneMan.h"
#include "Scene.h"
#include "CameraMan.h"
#include "UInputMan.h"
#include "GUIFont.h"
#include "AllegroBitmap.h"
#include "ConsoleMan.h"
#include "RTETools.h"
#include "allegro.h"
#include <chrono>
#include <memory>

namespace RTE {
class WorldStateBuyMenu {
public:
	enum class Mode { Closed, Catalog, Landing, Pending };
	bool BlocksGameplayInput() const { return m_Mode != Mode::Closed || m_WaitForFireRelease || (m_Menu && m_Menu->IsVisible()); }
	bool IsCatalogVisible() const { return m_Menu && m_Mode == Mode::Catalog && m_Menu->IsVisible(); }
	bool LastOrderAccepted() const { return m_LastAccepted; }
	float LastCharge() const { return m_LastCharge; }
	const WorldStateProtocol::PurchaseRequest& LastRequest() const { return m_Request; }
	bool IsSpectator() const { return m_Slot < 0; }
	BuyMenuGUI* GetMenu() { return m_Menu.get(); }
	void Reset() {
		m_Menu.reset(); m_Mode=Mode::Closed; m_Revision=0; m_Slot=-1; m_Team=-1; m_LastAccepted=false; m_Status.clear(); m_WaitForFireRelease=false;
		g_UInputMan.TrapMousePos(true, Players::PlayerOne);
	}
	void Open() { if (m_Menu && m_Mode==Mode::Closed) { m_Menu->SetEnabled(true); m_Mode=Mode::Catalog; m_Status.clear(); } }
	void ConfirmOrder() {
		if (!m_Menu || m_Mode!=Mode::Catalog) return;
		m_Menu->ConfirmOrder();
		if (m_Menu->PurchaseMade()) BeginLandingSelection();
	}
	bool ConfirmLanding(WorldStateClientSession& client, const Vector& position) {
		if (m_Mode!=Mode::Landing || m_NextId==std::numeric_limits<std::uint32_t>::max()) return false;
		m_Request.RequestId=++m_NextId; m_Request.SceneRevision=m_Revision;
		m_Request.LandingX=position.m_X; m_Request.LandingY=position.m_Y;
		if (!client.SendPurchaseRequest(m_Request)) { m_Status="Order could not be sent"; m_Mode=Mode::Closed; return false; }
		m_Mode=Mode::Pending; m_LastSend=std::chrono::steady_clock::now(); m_Status="Waiting for server confirmation..."; m_LastAccepted=false;
		return true;
	}
	void Update(WorldStateClientSession& client, bool allowPhysicalInput=true) {
		if (!client.IsConnected() || !client.HasSnapshot() || !client.HasPlayerAssignment()) { if (m_Revision) Reset(); return; }
		const auto& snapshot=client.GetLatestSnapshot();
		const int slot=client.GetAssignedPlayerSlot();
		const int team=slot>=0 && slot<WorldStateProtocol::c_PlayerSlotCount ? snapshot.Players[slot].Team : -1;
		if (m_Revision!=snapshot.SceneRevision || m_Slot!=slot || m_Team!=team) {
			Reset(); m_Revision=snapshot.SceneRevision; m_Slot=slot; m_Team=team;
			if (slot>=0 && team>=0 && team<WorldStateProtocol::c_MaxTeamCount) {
				m_Controller.Create(Controller::CIM_PLAYER, Players::PlayerOne); m_Controller.SetTeam(static_cast<short>(team));
				m_Menu=std::make_unique<BuyMenuGUI>();
				m_Menu->Create(&m_Controller); m_Menu->SetNativeTechModule(0); m_Menu->SetForeignCostMultiplier(1);
			}
		}
		for (const auto& result : client.DrainPurchaseResults()) {
			if (m_Mode!=Mode::Pending || result.RequestId!=m_Request.RequestId || result.SceneRevision!=m_Revision) continue;
			m_LastAccepted=result.Accepted; m_LastCharge=result.ChargedCost; m_Mode=Mode::Closed;
			m_WaitForFireRelease=true;
			m_Status=result.Accepted ? "Delivery ordered: " + RoundFloatToPrecision(result.ChargedCost, 2) + " oz" : "Order rejected: " + result.Reason;
			g_ConsoleMan.PrintString(m_Status);
		}
		if (!m_Menu) return;
		if (m_WaitForFireRelease && !g_UInputMan.ElementHeld(Players::PlayerOne, InputElements::INPUT_FIRE) && !g_UInputMan.MouseButtonHeld(MouseButtons::MOUSE_LEFT, Players::PlayerOne))
			m_WaitForFireRelease=false;
		if (allowPhysicalInput) m_Controller.Update();
		if (allowPhysicalInput && g_UInputMan.KeyPressed(SDLK_F6)) {
			if (m_Mode==Mode::Closed) Open();
			else if (m_Mode!=Mode::Pending) { m_Menu->SetEnabled(false); m_Mode=Mode::Closed; m_Status.clear(); }
		}
		if (m_Mode==Mode::Catalog || m_Menu->IsVisible()) {
			m_Menu->Update();
			if (m_Mode==Mode::Catalog && m_Menu->PurchaseMade()) BeginLandingSelection();
			else if (m_Mode==Mode::Catalog && !m_Menu->IsEnabled()) m_Mode=Mode::Closed;
		}
		if (m_Mode==Mode::Landing) {
			if (allowPhysicalInput) m_Controller.RelativeCursorMovement(m_Landing);
			g_SceneMan.ForceBounds(m_Landing);
			g_CameraMan.SetScrollTarget(m_Landing, 0.1F, Players::PlayerOne);
			m_Status="Choose landing zone: click to order, F6 to cancel";
			if (allowPhysicalInput && m_Controller.IsState(PRESS_PRIMARY)) ConfirmLanding(client, m_Landing);
		}
		if (m_Mode==Mode::Pending && std::chrono::steady_clock::now()-m_LastSend>=std::chrono::seconds(1)) {
			client.SendPurchaseRequest(m_Request); m_LastSend=std::chrono::steady_clock::now();
		}
	}
	void Draw(BITMAP* target, int screen, const Vector& offset) const {
		if (screen!=Players::PlayerOne || !m_Revision) return;
		if (m_Menu && m_Menu->IsVisible()) m_Menu->Draw(target);
		if (m_Mode==Mode::Landing) {
			const Vector position=m_Landing-offset;
			circle(target, position.GetFloorIntX(), position.GetFloorIntY(), 16, g_YellowGlowColor);
			line(target, position.GetFloorIntX()-24, position.GetFloorIntY(), position.GetFloorIntX()+24, position.GetFloorIntY(), g_YellowGlowColor);
		}
		AllegroBitmap bitmap(target);
		const std::string text=m_Status.empty() ? (m_Mode==Mode::Catalog ? "F6: close catalog" : (m_Menu ? "F6: equipment order" : "Spectator: equipment orders unavailable")) : m_Status;
		g_FrameMan.GetSmallFont()->DrawAligned(&bitmap, target->w/2, target->h-24, text, GUIFont::Centre);
	}
private:
	void BeginLandingSelection() {
		const SceneObject* craft=m_Menu->GetDeliveryCraftPreset();
		if (!craft) return;
		auto identity=[](const SceneObject* item) { return WorldStateProtocol::PurchasePreset{item->GetClassName(), item->GetModuleName(), item->GetPresetName()}; };
		m_Request.Craft=identity(craft); m_Request.Cargo.clear();
		std::list<const SceneObject*> order; m_Menu->GetOrderList(order);
		for (const SceneObject* item : order) m_Request.Cargo.push_back(identity(item));
		m_Menu->SetEnabled(false); m_Mode=Mode::Landing;
		m_Landing=Vector(g_CameraMan.GetOffset(0).m_X+g_FrameMan.GetPlayerScreenWidth()/2.0F, g_CameraMan.GetOffset(0).m_Y+g_FrameMan.GetPlayerScreenHeight()/2.0F);
	}
	Controller m_Controller;
	std::unique_ptr<BuyMenuGUI> m_Menu;
	Mode m_Mode=Mode::Closed;
	std::uint32_t m_Revision=0, m_NextId=0;
	int m_Slot=-1, m_Team=-1;
	WorldStateProtocol::PurchaseRequest m_Request;
	std::chrono::steady_clock::time_point m_LastSend{};
	Vector m_Landing;
	std::string m_Status;
	bool m_LastAccepted=false;
	bool m_WaitForFireRelease=false;
	float m_LastCharge=0;
};
}
