#pragma once
#include "WorldStateProtocol.h"

namespace RTE {
class MOSRotating;
namespace WorldStateAttachments {
bool Capture(const MOSRotating& object, WorldStateProtocol::ObjectState& state);
bool Apply(MOSRotating& object, const WorldStateProtocol::ObjectState& state);
bool ApplyPose(MOSRotating& object, const std::vector<WorldStateProtocol::AttachmentState>& nodes);
bool Matches(const MOSRotating& object, const std::vector<WorldStateProtocol::AttachmentState>& nodes);
bool TestNative(); // Runs only in an explicit disposable diagnostic session.
}
}
