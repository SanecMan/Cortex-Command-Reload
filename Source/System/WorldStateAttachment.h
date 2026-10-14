#pragma once

#include "UTF8.h"
#include <cmath>
#include <cstdint>
#include <limits>
#include <string>
#include <unordered_set>
#include <vector>

namespace RTE::WorldStateProtocol {

// A role identifies native pointer slots even when an earlier limb was removed.
// Generic and TurretDevice use Slot to distinguish multiple children.
enum class AttachmentRole : std::uint8_t {
    Generic, HumanHead, HumanFGArm, HumanBGArm, HumanFGLeg, HumanBGLeg,
    HumanJetpack, CrabTurret, CrabJetpack, CrabLeftFGLeg, CrabLeftBGLeg,
    CrabRightFGLeg, CrabRightBGLeg, RocketLeftLeg, RocketRightLeg,
    RocketMainThruster, RocketLeftThruster, RocketRightThruster,
    RocketULeftThruster, RocketURightThruster, DropshipLeftThruster,
    DropshipRightThruster, DropshipULeftThruster, DropshipURightThruster,
    DropshipLeftHatch, DropshipRightHatch, LegFoot, ArmDevice,
    FirearmMagazine, FirearmFlash, TurretDevice, EmitterFlash, Wound
};

inline constexpr std::uint16_t c_AttachmentRoot = std::numeric_limits<std::uint16_t>::max();
inline constexpr std::size_t c_MaxAttachments = 256, c_MaxAttachmentDepth = 16;

struct DeviceVisualState {
    bool Present = false, Activated = false, Reloading = false;
    float ReloadProgress = 1;
    bool FireFrame = false, Recoiled = false;
    float RecoilOffsetX = 0, RecoilOffsetY = 0;
    bool operator==(const DeviceVisualState&) const = default;
};
inline bool ValidDeviceVisual(const DeviceVisualState& state, const std::string& className) {
    if (!std::isfinite(state.ReloadProgress) || state.ReloadProgress < 0 || state.ReloadProgress > 1) return false;
    if (!std::isfinite(state.RecoilOffsetX) || !std::isfinite(state.RecoilOffsetY)) return false;
    if (!state.Recoiled && (state.RecoilOffsetX != 0 || state.RecoilOffsetY != 0)) return false;
    if (state.FireFrame && className != "HDFirearm") return false;
    if (!state.Present) return !state.Activated && !state.Reloading && !state.FireFrame && !state.Recoiled && state.ReloadProgress == 1;
    if (state.Reloading) return className == "HDFirearm";
    return state.ReloadProgress == 1;
}

struct AttachmentState {
    std::uint16_t Parent = c_AttachmentRoot, Slot = 0;
    AttachmentRole Role = AttachmentRole::Generic;
    std::string ClassName, ModuleName, PresetName;
    float ParentOffsetX = 0, ParentOffsetY = 0;
    float JointOffsetX = 0, JointOffsetY = 0;
    float RotationFromRoot = 0;
    float Scale = 1;
    bool HasEmitterState = false, EmissionEnabled = false, FlashVisible = false;
    float HandOffsetX = 0, HandOffsetY = 0;
    std::uint16_t SpriteFrame = 0;
    bool HFlipped = false, DrawAfterParent = false;
    std::int32_t MagazineRounds = 0;
    DeviceVisualState DeviceVisual;
    bool operator==(const AttachmentState&) const = default;
};

inline bool ValidAttachmentTree(const std::vector<AttachmentState>& nodes, std::size_t maxStringBytes = 1024) {
    if (nodes.size() > c_MaxAttachments) return false;
    std::vector<std::size_t> depths;
    depths.reserve(nodes.size());
    std::vector<std::uint16_t> woundCounts(nodes.size() + 1, 0);
    std::unordered_set<std::uint64_t> slots;
    for (std::size_t index = 0; index < nodes.size(); ++index) {
        const auto& node = nodes[index];
        if (!ValidDeviceVisual(node.DeviceVisual, node.ClassName)) return false;
        if (node.Parent != c_AttachmentRoot && node.Parent >= index) return false;
        const std::size_t depth = node.Parent == c_AttachmentRoot ? 1 : depths[node.Parent] + 1;
        if (depth > c_MaxAttachmentDepth) return false;
        depths.push_back(depth);
        const auto role = static_cast<std::uint8_t>(node.Role);
        if (role > static_cast<std::uint8_t>(AttachmentRole::Wound) || node.Slot >= c_MaxAttachments ||
            (node.Role != AttachmentRole::Generic && node.Role != AttachmentRole::TurretDevice && node.Role != AttachmentRole::Wound && node.Slot != 0)) return false;
        const std::uint64_t key = (std::uint64_t(node.Parent) << 24) | (std::uint64_t(role) << 16) | node.Slot;
        if (!slots.insert(key).second) return false;
        if (node.ClassName.empty() || node.ModuleName.empty() || node.PresetName.empty()) return false;
        for (const auto* text : {&node.ClassName, &node.ModuleName, &node.PresetName})
            if (text->size() > maxStringBytes || !UTF8::IsValid(*text)) return false;
        for (float number : {node.ParentOffsetX, node.ParentOffsetY, node.JointOffsetX, node.JointOffsetY,
                             node.RotationFromRoot, node.HandOffsetX, node.HandOffsetY, node.Scale})
            if (!std::isfinite(number)) return false;
        if ((!node.HasEmitterState && (node.EmissionEnabled || node.FlashVisible)) ||
            (node.HasEmitterState && node.ClassName != "AEmitter" && node.ClassName != "AEJetpack") ||
            (node.FlashVisible && !node.EmissionEnabled)) return false;
        if (node.Role == AttachmentRole::Wound && (!node.HasEmitterState || (node.ClassName != "AEmitter" && node.ClassName != "AEJetpack"))) return false;
        if (node.Role == AttachmentRole::Wound && node.Slot != woundCounts[node.Parent == c_AttachmentRoot ? nodes.size() : node.Parent]++) return false;
        if (node.ClassName == "Magazine" ? node.MagazineRounds < -1 : node.MagazineRounds != 0) return false;
        if (node.ClassName != "Arm" && (node.HandOffsetX != 0 || node.HandOffsetY != 0)) return false;
    }
    return true;
}
} // namespace RTE::WorldStateProtocol
