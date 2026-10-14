#include "WorldStateProtocol.h"
#include "WorldStateAttachmentTest.h"
#include <iostream>
int main() {
    using namespace RTE::WorldStateProtocol;
    if (!RTE::TestWorldStateAttachmentCodec()) return 1;
    Snapshot snapshot; snapshot.SceneRevision = 1;
    ObjectState gun; gun.NetworkId = 1; gun.Flags = c_ObjectFlagItem;
    gun.ClassName = "HDFirearm"; gun.ModuleName = "Coalition.rte"; gun.PresetName = "Auto Pistol";
    gun.DeviceVisual = {true, true, true, 0.35F};
    gun.DeviceVisual.FireFrame = true; gun.DeviceVisual.Recoiled = true;
    gun.DeviceVisual.RecoilOffsetX = -0.75F; gun.DeviceVisual.RecoilOffsetY = 0.125F;
    snapshot.Objects.push_back(gun);
    std::vector<std::uint8_t> packet; Snapshot decoded;
    if (!EncodeSnapshot(snapshot, 1, packet) || !DecodeSnapshot(packet, decoded) || decoded.Objects != snapshot.Objects) return 1;
    const auto validPacket = packet;
    for (float invalid : {-0.1F, 1.1F, std::numeric_limits<float>::quiet_NaN()}) {
        auto bad = snapshot; bad.Objects[0].DeviceVisual.ReloadProgress = invalid;
        if (EncodeSnapshot(bad, 1, packet)) return 1;
    }
    auto bad = snapshot; bad.Objects[0].DeviceVisual.Present = false;
    if (EncodeSnapshot(bad, 1, packet)) return 1;
    bad = snapshot; bad.Objects[0].ClassName = "HeldDevice";
    if (EncodeSnapshot(bad, 1, packet)) return 1;
    bad = snapshot; bad.Objects[0].DeviceVisual.Recoiled = false;
    if (EncodeSnapshot(bad, 1, packet)) return 1;
    bad = snapshot; bad.Objects[0].DeviceVisual.RecoilOffsetY = std::numeric_limits<float>::infinity();
    if (EncodeSnapshot(bad, 1, packet)) return 1;
    for (std::size_t length = 0; length < validPacket.size(); ++length) {
        Snapshot truncated;
        if (DecodeSnapshot(std::span<const std::uint8_t>(validPacket.data(), length), truncated)) return 1;
    }
    std::vector<std::uint8_t> malformed{2}; std::size_t offset = 0; DeviceVisualState state;
    if (Detail::ReadDeviceVisual(malformed, offset, state)) return 1;
    std::cout << "device_visual_codec=passed\n";
}
