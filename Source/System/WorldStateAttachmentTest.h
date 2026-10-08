#pragma once
#include "WorldStateProtocol.h"

namespace RTE {
inline bool TestWorldStateAttachmentCodec() {
	using namespace WorldStateProtocol;
	Snapshot source;
	source.SceneRevision = 1;
	ObjectState body;
	body.NetworkId = 1; body.Flags = c_ObjectFlagActor;
	body.ClassName = "AHuman"; body.ModuleName = "Base.rte"; body.PresetName = "Actor";
	body.HasAttachmentState = true; body.AimAngle = 0.375F; body.ActorStatus = 1;
	AttachmentState arm;
	arm.Role = AttachmentRole::HumanFGArm;
	arm.ClassName = "Arm"; arm.ModuleName = "Base.rte"; arm.PresetName = "Arm";
	arm.ParentOffsetX = 3; arm.JointOffsetY = -2; arm.HandOffsetX = 7;
	arm.RotationFromRoot = -0.25F; arm.SpriteFrame = 2; arm.HFlipped = true;
	AttachmentState gun = arm;
	gun.Parent = 0; gun.Role = AttachmentRole::ArmDevice; gun.ClassName = "HDFirearm";
	gun.PresetName = "Gun"; gun.HandOffsetX = 0; gun.DrawAfterParent = true;
	AttachmentState magazine = gun;
	magazine.Parent = 1; magazine.Role = AttachmentRole::FirearmMagazine;
	magazine.ClassName = "Magazine"; magazine.PresetName = "Magazine"; magazine.MagazineRounds = -1;
	body.Attachments = {arm, gun, magazine}; source.Objects.push_back(body);
	std::vector<std::uint8_t> packet;
	Snapshot decoded;
	if (!EncodeSnapshot(source, 1, packet) || !DecodeSnapshot(packet, decoded) || decoded.Objects != source.Objects) return false;
	const auto valid = packet;
	for (std::size_t length = 0; length < valid.size(); ++length) {
		decoded = source;
		if (DecodeSnapshot(std::span(valid).first(length), decoded) || decoded.Objects != source.Objects) return false;
	}
	auto rejects = [&](const Snapshot& invalid) { return !EncodeSnapshot(invalid, 1, packet); };
	auto invalid = source; invalid.Objects[0].Attachments[0].Parent = 0; if (!rejects(invalid)) return false;
	invalid = source; invalid.Objects[0].Attachments[1].Parent = 2; if (!rejects(invalid)) return false;
	invalid = source; invalid.Objects[0].Attachments.push_back(arm); if (!rejects(invalid)) return false;
	invalid = source; invalid.Objects[0].Attachments[0].Slot = 1; if (!rejects(invalid)) return false;
	invalid = source; invalid.Objects[0].Attachments[1].HandOffsetX = 2; if (!rejects(invalid)) return false;
	invalid = source; invalid.Objects[0].Attachments[2].MagazineRounds = -2; if (!rejects(invalid)) return false;
	invalid = source; invalid.Objects[0].Attachments[0].RotationFromRoot = std::numeric_limits<float>::quiet_NaN(); if (!rejects(invalid)) return false;
	invalid = source; invalid.Objects[0].Attachments[0].PresetName = "\xFF"; if (!rejects(invalid)) return false;
	invalid = source; invalid.Objects[0].Attachments[0].Role = static_cast<AttachmentRole>(255); if (!rejects(invalid)) return false;
	invalid = source; invalid.Objects[0].HasAttachmentState = false; if (!rejects(invalid)) return false;
	invalid = source; invalid.Objects[0].ActorStatus = 5; if (!rejects(invalid)) return false;
	invalid = source; invalid.Objects[0].AimAngle = std::numeric_limits<float>::infinity(); if (!rejects(invalid)) return false;
	invalid = source; invalid.Objects[0].Attachments.clear();
	for (std::size_t depth = 0; depth < c_MaxAttachmentDepth; ++depth) {
		auto node = gun; node.Role = AttachmentRole::Generic;
		node.Parent = depth ? static_cast<std::uint16_t>(depth - 1) : c_AttachmentRoot;
		invalid.Objects[0].Attachments.push_back(node);
	}
	if (!EncodeSnapshot(invalid, 1, packet) || !DecodeSnapshot(packet, decoded)) return false;
	auto node = gun; node.Role = AttachmentRole::Generic; node.Parent = c_MaxAttachmentDepth - 1;
	invalid.Objects[0].Attachments.push_back(node);
	return rejects(invalid);
}
} // namespace RTE
