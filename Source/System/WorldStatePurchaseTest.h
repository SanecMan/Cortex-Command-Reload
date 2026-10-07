#pragma once
#include "WorldStateClientSession.h"
#include "SceneMan.h"
#include "Scene.h"
#include <chrono>
#include <ostream>

namespace RTE {
inline bool TestPurchaseCodec() {
	using namespace WorldStateProtocol;
	PurchaseRequest source{.RequestId=1, .SceneRevision=2, .LandingX=400, .LandingY=30,
	    .Craft={"ACDropShip", "Base.rte", "Dropship MK1"}, .Cargo={{"AHuman", "Base.rte", "Привет"}}};
	std::vector<std::uint8_t> bytes;
	PurchaseRequest decoded;
	if (!EncodePurchaseRequest(source, bytes) || !DecodePurchaseRequest(bytes, decoded) || decoded != source) return false;
	const auto valid = bytes;
	for (std::size_t length=0; length<valid.size(); ++length) {
		decoded = source;
		if (DecodePurchaseRequest(std::span(valid).first(length), decoded) || decoded != source) return false;
	}
	bytes.push_back(0);
	if (DecodePurchaseRequest(bytes, decoded)) return false;
	bytes=valid; bytes[7]=1;
	if (DecodePurchaseRequest(bytes, decoded)) return false;
	bytes=valid; for (int index=8; index<12; ++index) bytes[index]=0;
	if (DecodePurchaseRequest(bytes, decoded)) return false;
	bytes=valid; bytes[6]=static_cast<std::uint8_t>(MessageType::PurchaseResult);
	if (DecodePurchaseRequest(bytes, decoded)) return false;
	source.Cargo.resize(129, source.Cargo.front());
	if (EncodePurchaseRequest(source, bytes)) return false;
	source.Cargo.clear(); source.LandingX=std::numeric_limits<float>::infinity();
	if (EncodePurchaseRequest(source, bytes)) return false;
	source.LandingX=1; source.Craft.PresetName="\xFF";
	if (EncodePurchaseRequest(source, bytes)) return false;
	PurchaseResult result{.RequestId=3, .SceneRevision=2, .Accepted=true, .ChargedCost=500}, roundTrip;
	if (!EncodePurchaseResult(result, bytes) || !DecodePurchaseResult(bytes, roundTrip) || roundTrip != result) return false;
	for (std::size_t length=0; length<bytes.size(); ++length) if (DecodePurchaseResult(std::span(bytes).first(length), roundTrip)) return false;
	result.Accepted=false; result.Reason="insufficient_funds";
	if (EncodePurchaseResult(result, bytes)) return false;
	result.ChargedCost=0;
	return EncodePurchaseResult(result, bytes) && DecodePurchaseResult(bytes, roundTrip) && roundTrip == result;
}

struct WorldStatePurchaseTest {
	int Stage = 0;
	bool Passed = false, Failed = false, Spectator = false, SawCraft = false;
	float ExpectedFunds = 0;
	WorldStateProtocol::PurchaseRequest Request;
	WorldStateProtocol::PurchaseResult AcceptedResult;
	std::chrono::steady_clock::time_point LastSend{};
	void Update(WorldStateClientSession& client, std::ostream& log) {
		using namespace WorldStateProtocol;
		if (Passed || Failed || !client.HasSnapshot() || !client.HasPlayerAssignment()) return;
		const auto now=std::chrono::steady_clock::now();
		auto send=[&] { if (!client.SendPurchaseRequest(Request)) Failed=true; LastSend=now; };
		if (Stage==0) {
			Spectator=client.GetAssignedPlayerSlot()<0;
			Request={.RequestId=1, .SceneRevision=client.GetLatestSnapshot().SceneRevision+1, .LandingX=400, .LandingY=30,
			    .Craft={"ACDropShip", "Base.rte", "Dropship MK1"}};
			send(); Stage=1;
		}
		for (const auto& result : client.DrainPurchaseResults()) {
			log << "purchase_reply id=" << result.RequestId << " accepted=" << result.Accepted << " charged=" << result.ChargedCost << " reason=" << result.Reason << '\n' << std::flush;
			if (Stage==1) {
				if (result.RequestId!=1 || result.Accepted || result.Reason!=(Spectator ? "spectator" : "stale_scene")) Failed=true;
				else if (Spectator) Passed=true;
				else { Stage=2; LastSend=now; }
			} else if (Stage==3) {
				if (result.RequestId!=2 || result.Accepted || result.Reason!="invalid_preset") Failed=true; else { Stage=4; LastSend=now; }
			} else if (Stage==5) {
				if (result.RequestId!=3 || !result.Accepted || result.ChargedCost!=500) Failed=true;
				else { AcceptedResult=result; Request.Craft.PresetName="different_payload_same_id"; send(); Stage=6; }
			} else if (Stage==6) {
				if (result!=AcceptedResult) Failed=true;
				else { Request.RequestId=2; send(); Stage=7; }
			} else if (Stage==7) {
				if (result.RequestId!=2 || result.Accepted || result.Reason!="stale_request") Failed=true; else Stage=8;
			} else Failed=true;
		}
		if ((Stage==2 || Stage==4) && now-LastSend>=std::chrono::milliseconds(300)) {
			Request.SceneRevision=client.GetLatestSnapshot().SceneRevision;
			if (Stage==2) { Request.RequestId=2; Request.Craft.ModuleName="MissingPurchaseModule.rte"; send(); Stage=3; }
			else {
				Request.RequestId=3; Request.Craft.ModuleName="Base.rte";
				const int team=client.GetLatestSnapshot().Players[client.GetAssignedPlayerSlot()].Team;
				if (team<0 || team>=static_cast<int>(c_TeamCount)) { Failed=true; return; }
				// The Tutorial fixture uses the map's static team landing-zone area.
				Scene* scene=g_SceneMan.GetScene();
				const Scene::Area* area=scene ? scene->GetArea("LZ Team " + std::to_string(team+1)) : nullptr;
				if (!area && scene) area=scene->GetArea("Landing Zone");
				if (!area || !area->GetFirstBox()) { Failed=true; return; }
				const Vector center=area->GetFirstBox()->GetCenter();
				Request.LandingX=center.m_X;
				Request.LandingY=std::clamp(center.m_Y, 0.0F, static_cast<float>(g_SceneMan.GetSceneHeight()-1));
				ExpectedFunds=client.GetLatestSnapshot().TeamFunds[team]-500;
				send(); Stage=5;
			}
		}
		if (Stage==8) {
			int craftCount=0;
			for (const auto& object : client.GetLatestSnapshot().Objects) if (object.ClassName=="ACDropShip" && object.PresetName=="Dropship MK1") ++craftCount;
			if (craftCount>1) Failed=true;
			SawCraft=SawCraft || craftCount==1;
			const int team=client.GetLatestSnapshot().Players[client.GetAssignedPlayerSlot()].Team;
			if (SawCraft && team>=0 && team<static_cast<int>(c_TeamCount) && client.GetLatestSnapshot().TeamFunds[team]==ExpectedFunds) Passed=true;
		}
	}
};
}
