#pragma once

#include "SoundContainer.h"
#include "SoundSet.h"
#include "PresetMan.h"
#include "WorldStateSoundProtocol.h"
#include <memory>
#include <unordered_map>

namespace RTE {

/// Local playback of server-selected resources. Never runs activity Lua.
class WorldStateSoundPlayback {
public:
	void Clear() { for (auto& [id, voice] : m_Voices) voice.Sound->Stop(); m_Voices.clear(); m_Revision = 0; }
	std::size_t Apply(std::uint32_t revision, int player, std::span<const WorldStateSoundProtocol::Event> events) {
		if (m_Revision != revision) { Clear(); m_Revision = revision; }
		for (auto it = m_Voices.begin(); it != m_Voices.end();)
			if (!it->second.Sound->IsBeingPlayed()) it = m_Voices.erase(it); else ++it;
		std::size_t applied = 0;
		for (const auto& event : events) {
			if (event.Player != -1 && event.Player != player) continue;
			if (event.Type == WorldStateSoundProtocol::Action::Play) {
				if (m_Voices.contains(event.Voice) || m_Voices.size() >= 2048 || g_PresetMan.GetModuleID(event.Module) < 0) continue;
				const auto module = UTF8::PathFromString(g_PresetMan.GetFullModulePath(event.Module));
				std::error_code error;
				const auto root = std::filesystem::weakly_canonical(module, error);
				if (error) continue;
				auto file = module / UTF8::PathFromString(event.Resource);
				if (!std::filesystem::is_regular_file(file, error)) {
					for (const auto* extension : c_SupportedAudioFormats) {
						auto alternative = file; alternative.replace_extension(extension);
						error.clear();
						if (std::filesystem::is_regular_file(alternative, error)) { file = std::move(alternative); break; }
					}
				}
				error.clear();
				const auto canonical = std::filesystem::weakly_canonical(file, error);
				if (error || !std::filesystem::is_regular_file(canonical, error)) continue;
				const auto relative = canonical.lexically_relative(root);
				if (relative.empty() || !WorldStateSoundProtocol::Detail::RelativeResource(UTF8::PathToString(relative))) continue;
				auto sound = std::make_unique<SoundContainer>();
				const auto path = UTF8::PathToString(file);
				sound->GetTopLevelSoundSet().AddSound(path, Vector(), 0, event.Attenuation, false);
				if (!sound->HasAnySounds()) continue;
				sound->SetImmobile(event.Immobile); sound->SetAffectedByGlobalPitch(event.AffectedByGlobalPitch);
				sound->SetBusRouting(static_cast<SoundContainer::BusRouting>(event.Bus));
				sound->SetLoopSetting(event.Loops); sound->SetPriority(event.Priority);
				sound->SetAttenuationStartDistance(event.Attenuation);
				sound->SetCustomPanValue(event.Pan); sound->SetPanningStrengthMultiplier(event.Panning);
				sound->SetPosition(Vector(event.X + event.OffsetX, event.Y + event.OffsetY));
				sound->SetVolume(event.Gain); sound->SetNetworkPitch(event.Rate); sound->SetPaused(event.Paused);
				if (!sound->Play(Players::PlayerOne)) continue;
				m_Voices.emplace(event.Voice, Voice{event.Container, Vector(event.OffsetX, event.OffsetY), std::move(sound)});
				++applied;
				continue;
			}
			for (auto it = m_Voices.begin(); it != m_Voices.end();) {
				auto& voice = it->second;
				if (voice.Container != event.Container) { ++it; continue; }
				switch (event.Type) {
					case WorldStateSoundProtocol::Action::Stop: voice.Sound->Stop(); it = m_Voices.erase(it); ++applied; continue;
					case WorldStateSoundProtocol::Action::Position: voice.Sound->SetPosition(Vector(event.X, event.Y) + voice.Offset); break;
					case WorldStateSoundProtocol::Action::Volume: voice.Sound->SetVolume(event.Gain); break;
					case WorldStateSoundProtocol::Action::Pitch: voice.Sound->SetNetworkPitch(event.Rate); break;
					case WorldStateSoundProtocol::Action::Pause: voice.Sound->SetPaused(event.Paused); break;
					case WorldStateSoundProtocol::Action::Fade: voice.Sound->FadeOut(static_cast<int>(event.FadeMilliseconds)); break;
					case WorldStateSoundProtocol::Action::Play: break;
				}
				++applied; ++it;
			}
		}
		return applied;
	}
	bool HasVoice(std::uint64_t id) const { return m_Voices.contains(id); }
	bool HasContainer(std::uint64_t id) const { for (const auto& [voiceID, voice] : m_Voices) if (voice.Container == id) return true; return false; }
	std::size_t Size() const { return m_Voices.size(); }
private:
	struct Voice { std::uint64_t Container; Vector Offset; std::unique_ptr<SoundContainer> Sound; };
	std::unordered_map<std::uint64_t, Voice> m_Voices;
	std::uint32_t m_Revision = 0;
};

} // namespace RTE
