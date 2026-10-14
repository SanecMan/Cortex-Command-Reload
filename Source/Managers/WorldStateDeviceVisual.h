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
        if (const auto* firearm = dynamic_cast<const HDFirearm*>(device)) {
            state.Reloading = firearm->IsReloading();
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
    if (auto* firearm = dynamic_cast<HDFirearm*>(device)) firearm->SetReplicaReloadVisual(state.Reloading, state.ReloadProgress);
    return true;
}
}
