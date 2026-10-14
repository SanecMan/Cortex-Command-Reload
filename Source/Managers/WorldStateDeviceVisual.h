#pragma once
#include "WorldStateAttachment.h"
#include "HeldDevice.h"
#include "HDFirearm.h"
#include <algorithm>
namespace RTE::WorldStateDeviceVisual {
inline WorldStateProtocol::DeviceVisualState Capture(const MovableObject& object) {
    WorldStateProtocol::DeviceVisualState state;
    if (const auto* device = dynamic_cast<const HeldDevice*>(&object)) {
        state.Present = true; state.Activated = device->IsActivated();
        state.Recoiled = device->IsRecoiled();
        if (state.Recoiled) { state.RecoilOffsetX = device->GetRecoilOffset().m_X; state.RecoilOffsetY = device->GetRecoilOffset().m_Y; }
        if (const auto* firearm = dynamic_cast<const HDFirearm*>(device)) {
            state.Reloading = firearm->IsReloading();
            state.FireFrame = firearm->FiredFrame();
            state.ShotSequence = firearm->GetNetworkShotSequence(); state.ShotAgeMS = firearm->GetNetworkShotAgeMS();
            state.ReloadProgress = state.Reloading ? std::clamp(firearm->GetReloadProgress(), 0.0F, 1.0F) : 1;
        }
    }
    return state;
}
inline bool Apply(MovableObject& object, const WorldStateProtocol::DeviceVisualState& state) {
    if (!WorldStateProtocol::ValidDeviceVisual(state, object.GetClassName())) return false;
    if (!state.Present) return true;
    auto* device = dynamic_cast<HeldDevice*>(&object); if (!device) return false;
    device->SetReplicaActivationVisual(state.Activated);
    device->SetRecoil(Vector(), Vector(state.RecoilOffsetX, state.RecoilOffsetY), state.Recoiled);
    if (auto* firearm = dynamic_cast<HDFirearm*>(device)) {
        firearm->SetReplicaReloadVisual(state.Reloading, state.ReloadProgress);
        firearm->SetReplicaFireVisual(state.FireFrame, state.ShotSequence, state.ShotAgeMS);
    }
    return true;
}
}
