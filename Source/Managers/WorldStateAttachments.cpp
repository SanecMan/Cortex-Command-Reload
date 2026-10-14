#include "WorldStateAttachments.h"
#include "AHuman.h"
#include "ACrab.h"
#include "ACRocket.h"
#include "ACDropShip.h"
#include "ADoor.h"
#include "Arm.h"
#include "Leg.h"
#include "Turret.h"
#include "AEJetpack.h"
#include "AEmitter.h"
#include "HDFirearm.h"
#include "Magazine.h"
#include "PresetMan.h"
#include "ObjectScriptExecution.h"
#include "WorldStateDeviceVisual.h"
#include <memory>
#include <algorithm>
#include <unordered_map>
#include <unordered_set>

namespace RTE::WorldStateAttachments {
namespace {
using namespace WorldStateProtocol;
struct Slot { AttachmentRole Role; std::uint16_t Index; Attachable* Object; };
AttachmentRole RoleOf(const MOSRotating& parent, const Attachable* child, std::uint16_t& slot) {
	using enum AttachmentRole;
#define MATCH_SLOT(Getter, Role) if (value->Getter() == child) return Role
	if (const auto* value = dynamic_cast<const AHuman*>(&parent)) {
		MATCH_SLOT(GetHead, HumanHead); MATCH_SLOT(GetFGArm, HumanFGArm); MATCH_SLOT(GetBGArm, HumanBGArm);
		MATCH_SLOT(GetFGLeg, HumanFGLeg); MATCH_SLOT(GetBGLeg, HumanBGLeg); MATCH_SLOT(GetJetpack, HumanJetpack);
	} else if (const auto* value = dynamic_cast<const ACrab*>(&parent)) {
		MATCH_SLOT(GetTurret, CrabTurret); MATCH_SLOT(GetJetpack, CrabJetpack);
		MATCH_SLOT(GetLeftFGLeg, CrabLeftFGLeg); MATCH_SLOT(GetLeftBGLeg, CrabLeftBGLeg);
		MATCH_SLOT(GetRightFGLeg, CrabRightFGLeg); MATCH_SLOT(GetRightBGLeg, CrabRightBGLeg);
	} else if (const auto* value = dynamic_cast<const ACRocket*>(&parent)) {
		MATCH_SLOT(GetLeftLeg, RocketLeftLeg); MATCH_SLOT(GetRightLeg, RocketRightLeg);
		MATCH_SLOT(GetMainThruster, RocketMainThruster); MATCH_SLOT(GetLeftThruster, RocketLeftThruster);
		MATCH_SLOT(GetRightThruster, RocketRightThruster); MATCH_SLOT(GetULeftThruster, RocketULeftThruster);
		MATCH_SLOT(GetURightThruster, RocketURightThruster);
	} else if (const auto* value = dynamic_cast<const ACDropShip*>(&parent)) {
		MATCH_SLOT(GetLeftThruster, DropshipLeftThruster); MATCH_SLOT(GetRightThruster, DropshipRightThruster);
		MATCH_SLOT(GetULeftThruster, DropshipULeftThruster); MATCH_SLOT(GetURightThruster, DropshipURightThruster);
		MATCH_SLOT(GetLeftHatch, DropshipLeftHatch); MATCH_SLOT(GetRightHatch, DropshipRightHatch);
	} else if (const auto* value = dynamic_cast<const Arm*>(&parent)) { MATCH_SLOT(GetHeldDevice, ArmDevice);
	} else if (const auto* value = dynamic_cast<const Leg*>(&parent)) { MATCH_SLOT(GetFoot, LegFoot);
	} else if (const auto* value = dynamic_cast<const HDFirearm*>(&parent)) {
		MATCH_SLOT(GetMagazine, FirearmMagazine); MATCH_SLOT(GetFlash, FirearmFlash);
	} else if (const auto* value = dynamic_cast<const Turret*>(&parent)) {
		const auto& devices = value->GetMountedDevices();
		const auto found = std::find(devices.begin(), devices.end(), child);
		if (found != devices.end()) { slot = static_cast<std::uint16_t>(found - devices.begin()); return TurretDevice; }
	}
#undef MATCH_SLOT
	if (const auto* value = dynamic_cast<const AEmitter*>(&parent)) { if (value->GetFlash() == child) return EmitterFlash; }
	return Generic;
}
std::vector<Slot> Slots(const MOSRotating& parent) {
	std::vector<Slot> result;
	result.reserve(parent.GetAttachables().size() + parent.GetWoundList().size());
	std::uint16_t generic = 0;
	for (Attachable* child : parent.GetAttachables()) {
		std::uint16_t index = 0;
		const auto role = RoleOf(parent, child, index);
		if (role == AttachmentRole::Generic) index = generic++;
		result.push_back({role, index, child});
	}
	for (std::size_t index = 0; index < parent.GetWoundList().size(); ++index)
		result.push_back({AttachmentRole::Wound, static_cast<std::uint16_t>(index), parent.GetWoundList()[index]});
	return result;
}
bool SameIdentity(const Attachable& child, const AttachmentState& state) {
	return child.GetClassName() == state.ClassName && child.GetModuleName() == state.ModuleName && child.GetPresetName() == state.PresetName;
}
void Retire(MOSRotating& parent, Attachable* child) {
	if (auto* wound = dynamic_cast<AEmitter*>(child); wound &&
	    std::find(parent.GetWoundList().begin(), parent.GetWoundList().end(), wound) != parent.GetWoundList().end()) {
		parent.RemoveReplicaWound(wound);
		return;
	}
	child->SetDeleteWhenRemovedFromParent(false); child->SetGibWhenRemovedFromParent(false); child->SetToDelete(false);
	std::unique_ptr<Attachable> owned(parent.RemoveAttachable(child, false, false));
	if (owned) owned->DestroyScriptState();
}
bool Adopt(MOSRotating& parent, Attachable* child, AttachmentRole role) {
	using enum AttachmentRole;
#define ADOPT(Role, ParentType, ChildType, Setter) case Role: { auto* owner = dynamic_cast<ParentType*>(&parent); auto* value = dynamic_cast<ChildType*>(child); if (!owner || !value) return false; owner->Setter(value); return true; }
	switch (role) {
		ADOPT(HumanHead, AHuman, Attachable, SetHead)
		ADOPT(HumanFGArm, AHuman, Arm, SetFGArm)
		ADOPT(HumanBGArm, AHuman, Arm, SetBGArm)
		ADOPT(HumanFGLeg, AHuman, Leg, SetFGLeg)
		ADOPT(HumanBGLeg, AHuman, Leg, SetBGLeg)
		ADOPT(HumanJetpack, AHuman, AEJetpack, SetJetpack)
		ADOPT(CrabTurret, ACrab, Turret, SetTurret)
		ADOPT(CrabJetpack, ACrab, AEJetpack, SetJetpack)
		ADOPT(CrabLeftFGLeg, ACrab, Leg, SetLeftFGLeg)
		ADOPT(CrabLeftBGLeg, ACrab, Leg, SetLeftBGLeg)
		ADOPT(CrabRightFGLeg, ACrab, Leg, SetRightFGLeg)
		ADOPT(CrabRightBGLeg, ACrab, Leg, SetRightBGLeg)
		ADOPT(RocketLeftLeg, ACRocket, Leg, SetLeftLeg)
		ADOPT(RocketRightLeg, ACRocket, Leg, SetRightLeg)
		ADOPT(RocketMainThruster, ACRocket, AEmitter, SetMainThruster)
		ADOPT(RocketLeftThruster, ACRocket, AEmitter, SetLeftThruster)
		ADOPT(RocketRightThruster, ACRocket, AEmitter, SetRightThruster)
		ADOPT(RocketULeftThruster, ACRocket, AEmitter, SetULeftThruster)
		ADOPT(RocketURightThruster, ACRocket, AEmitter, SetURightThruster)
		ADOPT(DropshipLeftThruster, ACDropShip, AEmitter, SetLeftThruster)
		ADOPT(DropshipRightThruster, ACDropShip, AEmitter, SetRightThruster)
		ADOPT(DropshipULeftThruster, ACDropShip, AEmitter, SetULeftThruster)
		ADOPT(DropshipURightThruster, ACDropShip, AEmitter, SetURightThruster)
		ADOPT(DropshipLeftHatch, ACDropShip, Attachable, SetLeftHatch)
		ADOPT(DropshipRightHatch, ACDropShip, Attachable, SetRightHatch)
		ADOPT(LegFoot, Leg, Attachable, SetFoot)
		ADOPT(ArmDevice, Arm, HeldDevice, SetHeldDevice)
		ADOPT(FirearmMagazine, HDFirearm, Magazine, SetMagazine)
		ADOPT(FirearmFlash, HDFirearm, Attachable, SetFlash)
		ADOPT(TurretDevice, Turret, HeldDevice, AddMountedDevice)
		ADOPT(EmitterFlash, AEmitter, Attachable, SetFlash)
		case Wound: {
			auto* wound = dynamic_cast<AEmitter*>(child); if (!wound) return false;
			parent.AddWound(wound, wound->GetParentOffset(), false);
			return std::find(parent.GetWoundList().begin(), parent.GetWoundList().end(), wound) != parent.GetWoundList().end();
		}
		case Generic: parent.AddAttachable(child); return true;
	}
#undef ADOPT
	return false;
}
}

bool Capture(const MOSRotating& object, WorldStateProtocol::ObjectState& state) {
	using namespace WorldStateProtocol;
	state.HasAttachmentState = true;
	state.DeviceVisual = WorldStateDeviceVisual::Capture(object);
	state.HasEmitterState = state.EmissionEnabled = state.FlashVisible = false;
	if (const auto* emitter = dynamic_cast<const AEmitter*>(&object)) {
		state.HasEmitterState = true; state.EmissionEnabled = emitter->IsEmitting(); state.FlashVisible = emitter->IsEmissionFlashVisible();
	}
	state.Attachments.clear();
	struct Parent { const MOSRotating* Object; std::uint16_t Index; std::size_t Depth; };
	std::vector<Parent> parents{{&object, c_AttachmentRoot, 0}};
	for (std::size_t parentIndex = 0; parentIndex < parents.size(); ++parentIndex) {
		const Parent parent = parents[parentIndex];
		for (const auto& slot : Slots(*parent.Object)) {
			const Attachable* child = slot.Object;
			if (!child || !child->IsAttachedTo(parent.Object) || parent.Depth >= c_MaxAttachmentDepth || state.Attachments.size() >= c_MaxAttachments) return false;
			AttachmentState node;
			node.Parent = parent.Index; node.Role = slot.Role; node.Slot = slot.Index;
			node.ClassName = child->GetClassName(); node.ModuleName = child->GetModuleName(); node.PresetName = child->GetPresetName();
			node.DeviceVisual = WorldStateDeviceVisual::Capture(*child);
			node.ParentOffsetX = child->GetParentOffset().GetX(); node.ParentOffsetY = child->GetParentOffset().GetY();
			node.JointOffsetX = child->GetJointOffset().GetX(); node.JointOffsetY = child->GetJointOffset().GetY();
			node.RotationFromRoot = child->GetRotAngle() - object.GetRotAngle();
			node.Scale = child->GetScale();
			if (const auto* emitter = dynamic_cast<const AEmitter*>(child)) {
				node.HasEmitterState = true; node.EmissionEnabled = emitter->IsEmitting(); node.FlashVisible = emitter->IsEmissionFlashVisible();
			}
			node.SpriteFrame = static_cast<std::uint16_t>(std::min<unsigned int>(child->GetFrame(), 65535));
			node.HFlipped = child->IsHFlipped(); node.DrawAfterParent = child->IsDrawnAfterParent();
			if (const auto* arm = dynamic_cast<const Arm*>(child)) {
				const Vector hand = arm->GetHandCurrentOffset() * Matrix(-object.GetRotAngle());
				node.HandOffsetX = hand.GetX(); node.HandOffsetY = hand.GetY();
			}
			if (const auto* magazine = dynamic_cast<const Magazine*>(child)) node.MagazineRounds = magazine->GetRoundCount();
			const auto index = static_cast<std::uint16_t>(state.Attachments.size());
			state.Attachments.push_back(std::move(node));
			parents.push_back({child, index, parent.Depth + 1});
		}
	}
	return ValidAttachmentTree(state.Attachments);
}

bool ApplyPose(MOSRotating& object, const std::vector<WorldStateProtocol::AttachmentState>& nodes) {
	using namespace WorldStateProtocol;
	if (!ValidAttachmentTree(nodes)) return false;
	std::vector<Attachable*> children;
	children.resize(nodes.size());
	// Classify each parent's ownership list once, rather than allocating and
	// performing the same native role casts again for every sibling each frame.
	for (std::size_t parentPass = 0; parentPass <= nodes.size(); ++parentPass) {
		const auto parentIndex = parentPass ? static_cast<std::uint16_t>(parentPass - 1) : c_AttachmentRoot;
		MOSRotating* parent = parentPass ? children[parentPass - 1] : &object;
		if (!parent) return false;
		const auto slots = Slots(*parent);
		for (std::size_t index = 0; index < nodes.size(); ++index) {
			const auto& node = nodes[index];
			if (node.Parent != parentIndex) continue;
			Attachable* child = nullptr;
			for (const auto& slot : slots)
				if (slot.Role == node.Role && slot.Index == node.Slot && SameIdentity(*slot.Object, node)) { child = slot.Object; break; }
			children[index] = child;
			if (!child) return false;
			if (!WorldStateDeviceVisual::Apply(*child, node.DeviceVisual)) return false;
			child->SetParentOffset(Vector(node.ParentOffsetX, node.ParentOffsetY));
			child->SetJointOffset(Vector(node.JointOffsetX, node.JointOffsetY));
			child->SetScale(node.Scale);
			if (node.HasEmitterState) {
				auto* emitter = dynamic_cast<AEmitter*>(child); if (!emitter) return false;
				emitter->SetReplicaEmissionVisual(node.EmissionEnabled, node.FlashVisible);
			}
			child->SetInheritsRotAngle(false); child->SetInheritsHFlipped(0);
			child->SetRotAngle(object.GetRotAngle() + node.RotationFromRoot);
			child->SetHFlipped(node.HFlipped); child->SetDrawnAfterParent(node.DrawAfterParent);
			child->SetFrame(std::min<unsigned int>(node.SpriteFrame, child->GetFrameCount() ? child->GetFrameCount() - 1 : 0));
			if (auto* arm = dynamic_cast<Arm*>(child)) arm->SetHandCurrentOffset(Vector(node.HandOffsetX, node.HandOffsetY) * Matrix(object.GetRotAngle()));
			if (auto* magazine = dynamic_cast<Magazine*>(child)) magazine->SetRoundCount(node.MagazineRounds);
		}
	}
	// Received offsets/angles are authoritative. ADoor's override resets its
	// moving panel to the preset's default open/closed pose.
	object.MOSRotating::CorrectAttachableAndWoundPositionsAndRotations();
	return true;
}

bool Apply(MOSRotating& object, const WorldStateProtocol::ObjectState& state) {
	using namespace WorldStateProtocol;
	if (state.HasEmitterState) {
		auto* emitter = dynamic_cast<AEmitter*>(&object); if (!emitter) return false;
		emitter->SetReplicaEmissionVisual(state.EmissionEnabled, state.FlashVisible);
	}
	if (!state.HasAttachmentState) return true;
	if (!ValidAttachmentTree(state.Attachments)) return false;
	ObjectScriptExecution::SuppressionScope suppress;
	struct Parent { MOSRotating* Object; std::uint16_t Index; };
	std::vector<Parent> parents{{&object, c_AttachmentRoot}};
	for (std::size_t index = 0; index < parents.size(); ++index) {
		const Parent parent = parents[index];
		auto current = Slots(*parent.Object);
		std::vector<Attachable*> order;
		std::vector<HeldDevice*> mountedOrder;
		std::vector<AEmitter*> woundOrder;
		std::unordered_set<Attachable*> retained;
		for (std::size_t childIndex = 0; childIndex < state.Attachments.size(); ++childIndex) {
			const auto& node = state.Attachments[childIndex];
			if (node.Parent != parent.Index) continue;
			Attachable* child = nullptr;
			const auto matching = std::find_if(current.begin(), current.end(), [&](const Slot& slot) { return slot.Role == node.Role && slot.Index == node.Slot; });
			if (matching != current.end() && SameIdentity(*matching->Object, node)) child = matching->Object;
			if (!child) {
				const Entity* preset = g_PresetMan.GetEntityPreset(node.ClassName, node.PresetName, node.ModuleName);
				std::unique_ptr<Entity> clone(preset ? preset->Clone() : nullptr);
				auto* replacement = dynamic_cast<Attachable*>(clone.get());
				if (!replacement || !SameIdentity(*replacement, node)) return false;
				if (matching != current.end()) { Retire(*parent.Object, matching->Object); matching->Object = nullptr; }
				if (!Adopt(*parent.Object, replacement, node.Role)) return false;
				clone.release(); child = replacement;
			}
			retained.insert(child);
			if (node.Role == AttachmentRole::Wound) woundOrder.push_back(dynamic_cast<AEmitter*>(child));
			else order.push_back(child);
			if (node.Role == AttachmentRole::TurretDevice) mountedOrder.push_back(dynamic_cast<HeldDevice*>(child));
			parents.push_back({child, static_cast<std::uint16_t>(childIndex)});
		}
		for (const auto& slot : current) if (slot.Object && !retained.contains(slot.Object)) Retire(*parent.Object, slot.Object);
		if (!parent.Object->SetReplicaAttachmentOrder(order)) return false;
		if (!parent.Object->SetReplicaWoundOrder(woundOrder)) return false;
		if (auto* turret = dynamic_cast<Turret*>(parent.Object)) if (!turret->SetReplicaMountedDeviceOrder(mountedOrder)) return false;
	}
	return ApplyPose(object, state.Attachments);
}

bool Matches(const MOSRotating& object, const std::vector<WorldStateProtocol::AttachmentState>& nodes) {
	WorldStateProtocol::ObjectState observed;
	if (!Capture(object, observed) || observed.Attachments.size() != nodes.size()) return false;
	const auto near = [](float left, float right) { return std::abs(left - right) < 0.0001F; };
	for (std::size_t index = 0; index < nodes.size(); ++index) {
		const auto& a = nodes[index]; const auto& b = observed.Attachments[index];
		if (a.Parent != b.Parent || a.Slot != b.Slot || a.Role != b.Role || a.ClassName != b.ClassName ||
		    a.ModuleName != b.ModuleName || a.PresetName != b.PresetName || a.SpriteFrame != b.SpriteFrame ||
		    a.HFlipped != b.HFlipped || a.DrawAfterParent != b.DrawAfterParent || a.MagazineRounds != b.MagazineRounds || a.DeviceVisual != b.DeviceVisual || a.HasEmitterState != b.HasEmitterState || a.EmissionEnabled != b.EmissionEnabled || a.FlashVisible != b.FlashVisible || !near(a.Scale, b.Scale) ||
		    !near(a.ParentOffsetX, b.ParentOffsetX) || !near(a.ParentOffsetY, b.ParentOffsetY) ||
		    !near(a.JointOffsetX, b.JointOffsetX) || !near(a.JointOffsetY, b.JointOffsetY) ||
		    !near(a.RotationFromRoot, b.RotationFromRoot) || !near(a.HandOffsetX, b.HandOffsetX) || !near(a.HandOffsetY, b.HandOffsetY)) return false;
	}
	return true;
}

bool TestEmitterVisual() {
	using namespace WorldStateProtocol;
	ObjectScriptExecution::SuppressionScope suppress;
	const auto* preset = dynamic_cast<const AEJetpack*>(g_PresetMan.GetEntityPreset("AEJetpack", "Jetpack", "Base.rte"));
	if (!preset || !preset->GetFlash()) return false;
	std::unique_ptr<AEJetpack> source(dynamic_cast<AEJetpack*>(preset->Clone()));
	std::unique_ptr<AEJetpack> target(dynamic_cast<AEJetpack*>(preset->Clone()));
	if (!source || !target) return false;
	source->SetPos(Vector(64, 64)); target->SetPos(source->GetPos());
	source->EnableEmission(true); source->GetFlash()->SetScale(0.75F);
	source->GetFlash()->SetParentOffset(Vector(8, 0));
	source->CorrectAttachableAndWoundPositionsAndRotations();
	ObjectState state;
	if (!Capture(*source, state) || !state.EmissionEnabled || !state.FlashVisible || !Apply(*target, state) ||
	    !target->IsEmitting() || !target->IsEmissionFlashVisible() || !target->GetFlash() ||
	    std::abs(target->GetFlash()->GetScale() - 0.75F) > 0.0001F) return false;
	BITMAP* on = create_bitmap_ex(32, 128, 128);
	BITMAP* copy = create_bitmap_ex(32, 128, 128);
	BITMAP* off = create_bitmap_ex(32, 128, 128);
	if (!on || !copy || !off) { if (on) destroy_bitmap(on); if (copy) destroy_bitmap(copy); if (off) destroy_bitmap(off); return false; }
	clear_to_color(on, 0); clear_to_color(copy, 0); clear_to_color(off, 0);
	source->Draw(on, Vector(), g_DrawColor); target->Draw(copy, Vector(), g_DrawColor);
	source->EnableEmission(false);
	const bool disabled = Capture(*source, state) && Apply(*target, state) && !target->IsEmissionFlashVisible();
	target->Draw(off, Vector(), g_DrawColor);
	bool equal = true, changed = false;
	for (int y = 0; y < 128; ++y) for (int x = 0; x < 128; ++x) {
		equal = equal && getpixel(on, x, y) == getpixel(copy, x, y);
		changed = changed || getpixel(on, x, y) != getpixel(off, x, y);
	}
	destroy_bitmap(on); destroy_bitmap(copy); destroy_bitmap(off);
	return disabled && equal && changed;
}

bool TestDoorPose() {
	const auto* preset = dynamic_cast<const ADoor*>(g_PresetMan.GetEntityPreset("ADoor", "Door Slide Long", "Base.rte"));
	if (!preset || !preset->GetDoor()) return false;
	std::unique_ptr<ADoor> source(dynamic_cast<ADoor*>(preset->Clone()));
	std::unique_ptr<ADoor> target(dynamic_cast<ADoor*>(preset->Clone()));
	if (!source || !target) return false;
	source->SetRotAngle(0.35F); target->SetRotAngle(source->GetRotAngle());
	source->GetDoor()->SetParentOffset(source->GetDoor()->GetParentOffset() + Vector(0, 17));
	source->GetDoor()->SetInheritsRotAngle(false);
	source->GetDoor()->SetRotAngle(0.75F);
	source->MOSRotating::CorrectAttachableAndWoundPositionsAndRotations();
	WorldStateProtocol::ObjectState state;
	if (!Capture(*source, state) || !Apply(*target, state) || !Matches(*target, state.Attachments)) return false;
	// The interpolation path also reapplies the received attachment pose.
	return ApplyPose(*target, state.Attachments) && Matches(*target, state.Attachments);
}

bool TestNative() {
	using namespace WorldStateProtocol;
	ObjectScriptExecution::SuppressionScope suppress;
	const auto* body = dynamic_cast<const AHuman*>(g_PresetMan.GetEntityPreset("AHuman", "Fat Culled Clone", "Base.rte"));
	const Entity* weapon = g_PresetMan.GetEntityPreset("HDFirearm", "Auto Pistol", "Coalition.rte");
	if (!body || !body->GetFGArm() || !weapon || !TestEmitterVisual() || !TestDoorPose()) return false;
	std::unique_ptr<AHuman> source(dynamic_cast<AHuman*>(body->Clone()));
	std::unique_ptr<AHuman> target(dynamic_cast<AHuman*>(body->Clone()));
	std::unique_ptr<HDFirearm> gun(dynamic_cast<HDFirearm*>(weapon->Clone()));
	if (!source || !target || !gun || !gun->GetMagazine()) return false;
	if (source->GetFGArm()->GetHeldDevice()) Retire(*source->GetFGArm(), source->GetFGArm()->GetHeldDevice());
	source->GetFGArm()->SetHeldDevice(gun.release());
	source->SetPos(Vector(200, 100)); source->SetRotAngle(0.37F);
	source->GetFGArm()->SetParentOffset(Vector(5, 2)); source->GetFGArm()->SetJointOffset(Vector(-1, 4));
	source->GetFGArm()->SetInheritsRotAngle(false); source->GetFGArm()->SetRotAngle(0.82F);
	source->GetFGArm()->SetHandCurrentOffset(Vector(8, -3));
	auto* sourceGun = dynamic_cast<HDFirearm*>(source->GetFGArm()->GetHeldDevice());
	sourceGun->SetInheritsRotAngle(false); sourceGun->SetRotAngle(1.1F); sourceGun->GetMagazine()->SetRoundCount(7);
	sourceGun->SetReplicaActivationVisual(true); sourceGun->SetReplicaReloadVisual(true, 0.35F);
	sourceGun->SetReplicaFireVisual(true); sourceGun->SetRecoil(Vector(), Vector(-0.75F, 0.125F), true);
	const auto near = [](float left, float right) { return std::abs(left - right) < 0.0001F; };
	auto matches = [&]() {
		source->CorrectAttachableAndWoundPositionsAndRotations();
		ObjectState expected, observed;
		if (!Capture(*source, expected)) return false;
		target->SetPos(source->GetPos()); target->SetRotAngle(source->GetRotAngle()); target->SetHFlipped(source->IsHFlipped());
		if (!Apply(*target, expected) || !Capture(*target, observed) || expected.Attachments.size() != observed.Attachments.size()) return false;
		for (std::size_t index = 0; index < expected.Attachments.size(); ++index) {
			const auto& a = expected.Attachments[index]; const auto& b = observed.Attachments[index];
			if (a.Parent != b.Parent || a.Slot != b.Slot || a.Role != b.Role || a.ClassName != b.ClassName ||
			    a.ModuleName != b.ModuleName || a.PresetName != b.PresetName || a.SpriteFrame != b.SpriteFrame ||
			    a.HFlipped != b.HFlipped || a.DrawAfterParent != b.DrawAfterParent || a.MagazineRounds != b.MagazineRounds || a.DeviceVisual != b.DeviceVisual || a.HasEmitterState != b.HasEmitterState || a.EmissionEnabled != b.EmissionEnabled || a.FlashVisible != b.FlashVisible || !near(a.Scale, b.Scale) ||
			    !near(a.ParentOffsetX, b.ParentOffsetX) || !near(a.ParentOffsetY, b.ParentOffsetY) ||
			    !near(a.JointOffsetX, b.JointOffsetX) || !near(a.JointOffsetY, b.JointOffsetY) ||
			    !near(a.RotationFromRoot, b.RotationFromRoot) || !near(a.HandOffsetX, b.HandOffsetX) || !near(a.HandOffsetY, b.HandOffsetY)) return false;
		}
		return true;
	};
	if (!matches()) return false;
	auto* targetGun = dynamic_cast<HDFirearm*>(target->GetFGArm()->GetHeldDevice());
	if (!targetGun || !targetGun->IsActivated() || !targetGun->IsReloading() || !near(targetGun->GetReloadProgress(), 0.35F)) return false;
	if (!targetGun->FiredFrame() || !targetGun->IsRecoiled() || !near(targetGun->GetRecoilOffset().m_X, -0.75F) || !near(targetGun->GetRecoilOffset().m_Y, 0.125F)) return false;
	Arm* retainedArm = target->GetFGArm();
	HeldDevice* retainedGun = retainedArm->GetHeldDevice();
	Magazine* retainedMagazine = dynamic_cast<HDFirearm*>(retainedGun)->GetMagazine();
	source->SetRotAngle(-0.5F); source->GetFGArm()->SetRotAngle(1.2F); sourceGun->GetMagazine()->SetRoundCount(-1);
	sourceGun->SetReplicaActivationVisual(false); sourceGun->SetReplicaReloadVisual(true, 0.75F);
	sourceGun->SetReplicaFireVisual(false); sourceGun->SetRecoil(Vector(), Vector(), false);
	if (!matches() || target->GetFGArm() != retainedArm || retainedArm->GetHeldDevice() != retainedGun ||
	    dynamic_cast<HDFirearm*>(retainedGun)->GetMagazine() != retainedMagazine) return false;
	if (targetGun->IsActivated() || !targetGun->IsReloading() || !near(targetGun->GetReloadProgress(), 0.75F)) return false;
	if (targetGun->FiredFrame() || targetGun->IsRecoiled() || targetGun->GetRecoilOffset() != Vector()) return false;
	sourceGun->SetReplicaReloadVisual(false, 1);
	if (!matches() || targetGun->IsReloading() || targetGun->GetReloadProgress() != 1) return false;
	// A shot occurred between snapshots: its native FireFrame has already cleared.
	sourceGun->SetReplicaFireVisual(false, 17, 20);
	if (!matches() || targetGun->FiredFrame() || !targetGun->MuzzleFlashVisible() || targetGun->GetMagazine() != retainedMagazine || targetGun->GetMagazine()->GetRoundCount() != -1) return false;
	sourceGun->SetReplicaFireVisual(false, 18, 150);
	if (!matches() || targetGun->MuzzleFlashVisible()) return false;
	// Preserve an expired received age even when the native frame flag is set.
	sourceGun->SetReplicaFireVisual(true, 19, 65535);
	if (!matches() || targetGun->GetNetworkShotAgeMS() != 65535 || targetGun->MuzzleFlashVisible()) return false;
	Retire(*source, source->GetFGArm());
	if (!matches() || target->GetFGArm()) return false;
	std::unique_ptr<Arm> replacement(dynamic_cast<Arm*>(body->GetFGArm()->Clone()));
	if (!replacement) return false;
	source->SetFGArm(replacement.release());
	if (!matches() || !target->GetFGArm()) return false;
	const auto* woundPreset = dynamic_cast<const AEmitter*>(g_PresetMan.GetEntityPreset("AEmitter", "Wound Flesh", "Base.rte"));
	if (!woundPreset) return false;
	auto addWound = [&](MOSRotating& owner, Vector offset) {
		std::unique_ptr<AEmitter> wound(dynamic_cast<AEmitter*>(woundPreset->Clone()));
		if (!wound) return false;
		wound->EnableEmission(false);
		owner.AddWound(wound.get(), offset, false);
		if (wound->GetParent() != &owner) return false;
		wound.release(); return true;
	};
	const float unwoundedMass = source->GetMass();
	if (!addWound(*source, Vector(2, 3)) || !addWound(*source, Vector(-3, 2)) ||
	    !addWound(*source->GetFGArm(), Vector(4, 0)) || !matches() ||
	    target->GetWoundList().size() != 2 || target->GetFGArm()->GetWoundList().size() != 1 ||
	    !near(source->GetMass(), target->GetMass())) return false;
	AEmitter* retainedWound = target->GetWoundList()[0];
	using BitmapOwner = std::unique_ptr<BITMAP, void(*)(BITMAP*)>;
	BitmapOwner wounded(create_bitmap_ex(32, 512, 256), destroy_bitmap);
	BitmapOwner replica(create_bitmap_ex(32, 512, 256), destroy_bitmap);
	if (!wounded || !replica) return false;
	clear_to_color(wounded.get(), 0); clear_to_color(replica.get(), 0);
	source->Draw(wounded.get(), Vector(), g_DrawColor); target->Draw(replica.get(), Vector(), g_DrawColor);
	for (int y = 0; y < 256; ++y) for (int x = 0; x < 512; ++x)
		if (getpixel(wounded.get(), x, y) != getpixel(replica.get(), x, y)) return false;
	if (!matches() || target->GetWoundList()[0] != retainedWound) return false;
	const auto& wounds = source->GetWoundList();
	if (source->SetReplicaWoundOrder({wounds[0], wounds[0]}) ||
	    !source->SetReplicaWoundOrder({wounds[1], wounds[0]}) || !matches()) return false;
	if (!source->RemoveReplicaWound(source->GetWoundList()[0]) || !matches() || target->GetWoundList().size() != 1) return false;
	if (!source->RemoveReplicaWound(source->GetWoundList()[0]) ||
	    !source->GetFGArm()->RemoveReplicaWound(source->GetFGArm()->GetWoundList()[0]) || !matches()) return false;
	clear_to_color(replica.get(), 0); target->Draw(replica.get(), Vector(), g_DrawColor);
	bool pixelsChanged = false;
	for (int y = 0; y < 256; ++y) for (int x = 0; x < 512; ++x)
		pixelsChanged = pixelsChanged || getpixel(wounded.get(), x, y) != getpixel(replica.get(), x, y);
	return pixelsChanged && target->GetWoundList().empty() && target->GetFGArm()->GetWoundList().empty() &&
	    near(source->GetMass(), unwoundedMass) && near(target->GetMass(), unwoundedMass);
}
}
