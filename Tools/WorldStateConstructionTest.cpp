#include "WorldStateConstructionProtocol.h"
#include <iostream>

using namespace RTE::WorldStateProtocol;
static bool Check(bool good, const char* name) {
	if (!good) std::cerr << "FAILED: " << name << '\n';
	return good;
}
int main() {
	ConstructionRequest request;
	request.RequestId = 7; request.SceneRevision = 2;
	std::vector<std::uint8_t> bytes;
	for (auto action : {ConstructionAction::PlaceBrain, ConstructionAction::PlaceObject,
	                    ConstructionAction::RemoveObject, ConstructionAction::SetReady}) {
		request.Action = action; request.Preset = {}; request.ObjectId = 0;
		request.X = request.Y = request.Rotation = 0; request.HFlipped = request.Ready = false;
		if (action == ConstructionAction::PlaceObject) request.Preset = {"AHuman", "Мод.rte", "Робот Ёжик"};
		if (action == ConstructionAction::PlaceBrain || action == ConstructionAction::PlaceObject) {
			request.X = 42.5F; request.Y = 210; request.Rotation = 0.5F; request.HFlipped = true;
		}
		if (action == ConstructionAction::RemoveObject) request.ObjectId = 0x100000001ULL;
		if (action == ConstructionAction::SetReady) request.Ready = true;
		ConstructionRequest decoded;
		if (!Check(EncodeConstructionRequest(request, bytes) && DecodeConstructionRequest(bytes, decoded) && decoded == request, "request round trip")) return 1;
		if (action == ConstructionAction::SetReady) {
			request.Ready = false;
			if (!Check(EncodeConstructionRequest(request, bytes) && DecodeConstructionRequest(bytes, decoded) && decoded == request, "ready cancellation")) return 1;
		}
		for (std::size_t length = 0; length < bytes.size(); ++length) {
			const auto original = decoded;
			if (!Check(!DecodeConstructionRequest(std::span(bytes).first(length), decoded) && decoded == original, "truncated request and atomic output")) return 2;
		}
		auto bad = bytes; bad[c_HeaderSize + 4] = 255;
		if (!Check(!DecodeConstructionRequest(bad, decoded), "unknown action")) return 3;
		bad = bytes; bad[7] = 1;
		if (!Check(!DecodeConstructionRequest(bad, decoded), "reserved header byte")) return 4;
		bad = bytes; bad.push_back(0); ++bad[12];
		if (!Check(!DecodeConstructionRequest(bad, decoded), "trailing data with matching body length")) return 5;
		bad = bytes;
		const std::size_t flag = c_HeaderSize + 5 + (action == ConstructionAction::SetReady ? 0 : 12);
		if (action != ConstructionAction::RemoveObject) {
			bad[flag] = 2;
			if (!Check(!DecodeConstructionRequest(bad, decoded), "nonboolean flag")) return 6;
		}
		if (action == ConstructionAction::PlaceBrain || action == ConstructionAction::PlaceObject) {
			bad = bytes;
			const std::uint32_t nanBits = 0x7FC00000;
			for (int i = 0; i < 4; ++i) bad[c_HeaderSize + 5 + i] = static_cast<std::uint8_t>(nanBits >> (8 * i));
			if (!Check(!DecodeConstructionRequest(bad, decoded), "nonfinite position on wire")) return 7;
		}
		if (action == ConstructionAction::PlaceObject) {
			bad = bytes;
			auto unicodeByte = std::find_if(bad.begin() + c_HeaderSize + 18, bad.end(), [](std::uint8_t byte) { return byte >= 128; });
			if (!Check(unicodeByte != bad.end(), "UTF-8 fixture")) return 7;
			*unicodeByte = 255;
			if (!Check(!DecodeConstructionRequest(bad, decoded), "invalid identity UTF-8 on wire")) return 7;
		}
	}
	request = {}; request.RequestId = 1; request.SceneRevision = 1;
	request.X = std::numeric_limits<float>::infinity();
	if (!Check(!EncodeConstructionRequest(request, bytes), "infinite position")) return 8;
	request.X = 0; request.Ready = true;
	if (!Check(!EncodeConstructionRequest(request, bytes), "unused placement field")) return 9;
	request.Ready = false; request.Action = ConstructionAction::PlaceObject;
	request.Preset = {"AHuman", "Base.rte", std::string(1, '\xFF')};
	if (!Check(!EncodeConstructionRequest(request, bytes), "legacy bytes are not UTF-8 identities")) return 10;
	request.Preset.PresetName.assign(c_MaxStringBytes + 1, 'x');
	if (!Check(!EncodeConstructionRequest(request, bytes), "oversized preset")) return 11;
	request.Preset.PresetName.assign(c_MaxStringBytes, 'x');
	ConstructionRequest decoded;
	if (!Check(EncodeConstructionRequest(request, bytes) && DecodeConstructionRequest(bytes, decoded), "maximum preset length")) return 12;
	auto oversized = bytes; oversized.resize(c_MaxConstructionPacketBytes + 1);
	if (!Check(!DecodeConstructionRequest(oversized, decoded), "oversized packet")) return 13;
	request.SceneRevision = 0;
	if (!Check(!EncodeConstructionRequest(request, bytes), "missing revision")) return 14;
	request.SceneRevision = 1; request.RequestId = 0;
	if (!Check(!EncodeConstructionRequest(request, bytes), "missing request ID")) return 14;
	request.RequestId = 1; request.Action = static_cast<ConstructionAction>(255);
	if (!Check(!EncodeConstructionRequest(request, bytes), "unknown action at encode")) return 14;
	ConstructionResult result;
	result.RequestId = 1; result.SceneRevision = 2; result.Reason = "Мозг нельзя разместить здесь";
	for (bool accepted : {false, true}) {
		result.Accepted = accepted;
		if (accepted) { result.Reason.clear(); result.ChargedCost = 123.5F; result.ObjectId = 0x100000002ULL; }
		ConstructionResult received;
		if (!Check(EncodeConstructionResult(result, bytes) && DecodeConstructionResult(bytes, received) && received == result, "result round trip")) return 15;
		for (std::size_t length = 0; length < bytes.size(); ++length) {
			const auto original = received;
			if (!Check(!DecodeConstructionResult(std::span(bytes).first(length), received) && received == original, "truncated result and atomic output")) return 16;
		}
		auto bad = bytes; bad[c_HeaderSize + 4] = 2;
		if (!Check(!DecodeConstructionResult(bad, received), "nonboolean acceptance")) return 17;
	}
	result.Accepted = false; result.Reason = "denied";
	if (!Check(!EncodeConstructionResult(result, bytes), "rejection cannot charge or create an object")) return 18;
	result.ChargedCost = 0; result.ObjectId = 0;
	result.Reason.assign(1, '\xFF');
	if (!Check(!EncodeConstructionResult(result, bytes), "invalid UTF-8 reason")) return 19;
	result.Reason.clear();
	if (!Check(!EncodeConstructionResult(result, bytes), "rejection requires a reason")) return 20;
	std::cout << "construction_codec=passed version=" << c_Version << '\n';
}
