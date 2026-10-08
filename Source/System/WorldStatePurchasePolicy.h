#pragma once
#include "WorldStatePurchaseProtocol.h"
#include <map>

namespace RTE::WorldStateProtocol {
struct PurchaseLandingBox {
	float X=0, Y=0, Width=0, Height=0;
	bool operator==(const PurchaseLandingBox&) const = default;
};
struct PurchasePolicy {
	std::uint32_t SceneRevision=0;
	std::int8_t PlayerSlot=-1;
	std::int8_t Team=-1;
	bool OnlyOwned=false, EnforceMass=true, EnforcePassengers=true;
	std::string PricingModule="Base.rte";
	float ForeignMultiplier=1, NativeMultiplier=1;
	std::vector<std::string> Allowed, AlwaysAllowed, Prohibited;
	std::map<std::string, std::int32_t> Owned;
	std::vector<PurchaseLandingBox> LandingBoxes;
	bool operator==(const PurchasePolicy&) const = default;
};
inline constexpr std::size_t c_MaxPolicyEntries=4096, c_MaxLandingBoxes=1024, c_MaxPolicyBytes=1024*1024;
inline bool EncodePurchasePolicy(const PurchasePolicy& policy, std::uint32_t sequence, std::vector<std::uint8_t>& packet) {
	if (!sequence || !policy.SceneRevision || policy.PlayerSlot<0 || policy.PlayerSlot>=c_PlayerSlotCount || policy.Team<0 || policy.Team>=c_MaxTeamCount ||
	    policy.Owned.size()>c_MaxPolicyEntries || policy.LandingBoxes.size()>c_MaxLandingBoxes) return false;
	std::vector<std::uint8_t> body;
	Detail::WriteUnsigned(body,policy.SceneRevision,4); Detail::WriteUnsigned(body,static_cast<std::uint8_t>(policy.PlayerSlot),1);
	Detail::WriteUnsigned(body,static_cast<std::uint8_t>(policy.Team),1);
	Detail::WriteUnsigned(body,(policy.OnlyOwned?1:0)|(policy.EnforceMass?2:0)|(policy.EnforcePassengers?4:0),1);
	if (policy.PricingModule.empty() || !Detail::WriteString(body,policy.PricingModule) ||
	    !std::isfinite(policy.ForeignMultiplier) || policy.ForeignMultiplier<0 ||
	    !std::isfinite(policy.NativeMultiplier) || policy.NativeMultiplier<0) return false;
	Detail::WriteFloat(body,policy.ForeignMultiplier); Detail::WriteFloat(body,policy.NativeMultiplier);
	auto writeKeys=[&](const auto& keys) {
		if (keys.size()>c_MaxPolicyEntries) return false;
		Detail::WriteUnsigned(body,keys.size(),2);
		std::unordered_set<std::string> unique;
		for (const auto& key:keys) if (key.empty() || !unique.insert(key).second || !Detail::WriteString(body,key) || body.size()>c_MaxPolicyBytes-c_HeaderSize) return false;
		return true;
	};
	if (!writeKeys(policy.Allowed) || !writeKeys(policy.AlwaysAllowed) || !writeKeys(policy.Prohibited)) return false;
	Detail::WriteUnsigned(body,policy.Owned.size(),2);
	for (const auto& [key,count]:policy.Owned) {
		if (key.empty() || !Detail::WriteString(body,key)) return false;
		Detail::WriteUnsigned(body,std::bit_cast<std::uint32_t>(count),4);
		if (body.size()>c_MaxPolicyBytes-c_HeaderSize) return false;
	}
	Detail::WriteUnsigned(body,policy.LandingBoxes.size(),2);
	for (const auto& box:policy.LandingBoxes) {
		if (!std::isfinite(box.X)||!std::isfinite(box.Y)||!std::isfinite(box.Width)||!std::isfinite(box.Height)) return false;
		Detail::WriteFloat(body,box.X); Detail::WriteFloat(body,box.Y); Detail::WriteFloat(body,box.Width); Detail::WriteFloat(body,box.Height);
	}
	if (body.size()>c_MaxPolicyBytes-c_HeaderSize) return false;
	packet.clear(); Detail::WriteUnsigned(packet,c_Magic,4); Detail::WriteUnsigned(packet,c_Version,2);
	Detail::WriteUnsigned(packet,static_cast<std::uint8_t>(MessageType::PurchasePolicy),1); Detail::WriteUnsigned(packet,0,1);
	Detail::WriteUnsigned(packet,sequence,4); Detail::WriteUnsigned(packet,body.size(),4);
	packet.insert(packet.end(),body.begin(),body.end()); return true;
}
inline bool DecodePurchasePolicy(std::span<const std::uint8_t> packet, PurchasePolicy& policy, std::uint32_t* sequence=nullptr) {
	if (packet.size()<c_HeaderSize || packet.size()>c_MaxPolicyBytes) return false;
	std::size_t offset=0; std::uint64_t value=0;
	if (!Detail::ReadUnsigned(packet,offset,4,value)||value!=c_Magic || !Detail::ReadUnsigned(packet,offset,2,value)||value!=c_Version ||
	    !Detail::ReadUnsigned(packet,offset,1,value)||value!=static_cast<std::uint8_t>(MessageType::PurchasePolicy) ||
	    !Detail::ReadUnsigned(packet,offset,1,value)||value || !Detail::ReadUnsigned(packet,offset,4,value)||!value) return false;
	const std::uint32_t received=static_cast<std::uint32_t>(value);
	if (!Detail::ReadUnsigned(packet,offset,4,value)||value!=packet.size()-c_HeaderSize) return false;
	PurchasePolicy decoded;
	if (!Detail::ReadUnsigned(packet,offset,4,value)||!value) return false; decoded.SceneRevision=static_cast<std::uint32_t>(value);
	if (!Detail::ReadUnsigned(packet,offset,1,value)||value>=c_PlayerSlotCount) return false; decoded.PlayerSlot=static_cast<std::int8_t>(value);
	if (!Detail::ReadUnsigned(packet,offset,1,value)||value>=c_MaxTeamCount) return false; decoded.Team=static_cast<std::int8_t>(value);
	if (!Detail::ReadUnsigned(packet,offset,1,value)||value>7) return false;
	decoded.OnlyOwned=(value&1)!=0; decoded.EnforceMass=(value&2)!=0; decoded.EnforcePassengers=(value&4)!=0;
	if (!Detail::ReadString(packet,offset,decoded.PricingModule) || decoded.PricingModule.empty() ||
	    !Detail::ReadFloat(packet,offset,decoded.ForeignMultiplier) || decoded.ForeignMultiplier<0 ||
	    !Detail::ReadFloat(packet,offset,decoded.NativeMultiplier) || decoded.NativeMultiplier<0) return false;
	auto readKeys=[&](auto& keys) {
		if (!Detail::ReadUnsigned(packet,offset,2,value)||value>c_MaxPolicyEntries) return false;
		keys.resize(static_cast<std::size_t>(value)); std::unordered_set<std::string> unique;
		for (auto& key:keys) if (!Detail::ReadString(packet,offset,key)||key.empty()||!unique.insert(key).second) return false;
		return true;
	};
	if (!readKeys(decoded.Allowed)||!readKeys(decoded.AlwaysAllowed)||!readKeys(decoded.Prohibited)) return false;
	if (!Detail::ReadUnsigned(packet,offset,2,value)||value>c_MaxPolicyEntries) return false;
	const std::size_t count=static_cast<std::size_t>(value);
	for (std::size_t index=0; index<count; ++index) {
		std::string key;
		if (!Detail::ReadString(packet,offset,key)||key.empty()||!Detail::ReadUnsigned(packet,offset,4,value)||
		    !decoded.Owned.emplace(key,std::bit_cast<std::int32_t>(static_cast<std::uint32_t>(value))).second) return false;
	}
	if (!Detail::ReadUnsigned(packet,offset,2,value)||value>c_MaxLandingBoxes) return false;
	decoded.LandingBoxes.resize(static_cast<std::size_t>(value));
	for (auto& box:decoded.LandingBoxes) if (!Detail::ReadFloat(packet,offset,box.X)||!Detail::ReadFloat(packet,offset,box.Y)||
	    !Detail::ReadFloat(packet,offset,box.Width)||!Detail::ReadFloat(packet,offset,box.Height)) return false;
	if (offset!=packet.size()) return false;
	policy=std::move(decoded); if (sequence) *sequence=received; return true;
}
inline bool TestPurchasePolicyCodec() {
	PurchasePolicy source;
	source.SceneRevision=1; source.PlayerSlot=0; source.Team=0; source.OnlyOwned=true;
	source.PricingModule="Ronin.rte"; source.ForeignMultiplier=2; source.NativeMultiplier=0.5F;
	source.Allowed={"Base.rte/Привет"}; source.AlwaysAllowed={"Base.rte/Brain Case"}; source.Prohibited={"Base.rte/SMG"};
	source.Owned={{"Base.rte/Rocket MK2",2},{"Base.rte/negative",-7}}; source.LandingBoxes={{100,0,300,1000}};
	std::vector<std::uint8_t> bytes; PurchasePolicy decoded; std::uint32_t seq=0;
	if (!EncodePurchasePolicy(source,8,bytes)||!DecodePurchasePolicy(bytes,decoded,&seq)||decoded!=source||seq!=8) return false;
	const auto valid=bytes;
	for (std::size_t length=0;length<valid.size();++length) { decoded=source; if (DecodePurchasePolicy(std::span(valid).first(length),decoded)||decoded!=source) return false; }
	bytes.push_back(0); if (DecodePurchasePolicy(bytes,decoded)) return false;
	bytes=valid; bytes[c_HeaderSize+6]=255; if (DecodePurchasePolicy(bytes,decoded)) return false;
	const std::size_t nativeOffset=c_HeaderSize+7+2+source.PricingModule.size()+4;
	for (const float invalid : {-1.0F,std::numeric_limits<float>::infinity()}) {
		bytes=valid;
		const auto bits=std::bit_cast<std::uint32_t>(invalid);
		for (unsigned index=0;index<4;++index) bytes[nativeOffset+index]=static_cast<std::uint8_t>(bits>>(index*8));
		decoded=source; if (DecodePurchasePolicy(bytes,decoded) || decoded!=source) return false;
	}
	const auto pricing=source;
	source.NativeMultiplier=-1; if (EncodePurchasePolicy(source,1,bytes)) return false;
	source=pricing; source.ForeignMultiplier=std::numeric_limits<float>::quiet_NaN(); if (EncodePurchasePolicy(source,1,bytes)) return false;
	source=pricing; source.PricingModule.clear(); if (EncodePurchasePolicy(source,1,bytes)) return false;
	source=pricing;
	source.Allowed.push_back(source.Allowed.front()); if (EncodePurchasePolicy(source,1,bytes)) return false;
	source.Allowed.pop_back(); source.LandingBoxes[0].Width=std::numeric_limits<float>::infinity(); if (EncodePurchasePolicy(source,1,bytes)) return false;
	source.LandingBoxes.clear(); source.Allowed={"\xFF"}; if (EncodePurchasePolicy(source,1,bytes)) return false;
	source.Allowed.resize(c_MaxPolicyEntries+1,"Base.rte/Test"); return !EncodePurchasePolicy(source,1,bytes);
}
}
