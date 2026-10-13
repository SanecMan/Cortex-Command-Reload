#pragma once

#include "SoundContainer.h"
#include "SoundSet.h"
#include "PresetMan.h"
#include "WorldStateSoundProtocol.h"
#include <memory>
#include <array>
#include <unordered_map>

namespace RTE {

/// Local playback of server-selected resources. Never runs activity Lua.
class WorldStateSoundPlayback {
public:
	void Clear() { for (auto& [id, voice] : m_Voices) voice.Sound->Stop(); m_Voices.clear(); m_Revision = 0; m_Pitch.fill(1); m_BusPaused.fill(false); }
	std::size_t Apply(std::uint32_t revision, int player, std::span<const WorldStateSoundProtocol::Event> events) {
		if (m_Revision != revision) { Clear(); m_Revision = revision; }
		for (auto it = m_Voices.begin(); it != m_Voices.end();)
			if (!it->second.Sound->IsBeingPlayed()) it = m_Voices.erase(it); else ++it;
		std::size_t applied = 0;
		for (const auto& event : events) {
			if (event.Player != -1 && event.Player != player) continue;
			if (event.Type == WorldStateSoundProtocol::Action::GlobalPitch) {
				for (std::size_t bus = 0; bus < m_Pitch.size(); ++bus) if (event.Bus & (1U << bus)) m_Pitch[bus] = event.Rate;
				for (auto& [id, voice] : m_Voices) { voice.Sound->SetNetworkPitch(voice.Rate * m_Pitch[voice.Bus]); ++applied; }
				continue;
			}
			if (event.Type == WorldStateSoundProtocol::Action::BusPause) {
				m_BusPaused[event.Bus] = event.Paused;
				for (auto& [id, voice] : m_Voices) if (voice.Bus == event.Bus) { voice.Sound->SetPaused(voice.Paused || event.Paused); ++applied; }
				continue;
			}
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
				sound->SetVolume(event.Gain); sound->SetNetworkPitch(event.Rate * m_Pitch[event.Bus]); sound->SetPaused(event.Paused || m_BusPaused[event.Bus]);
				if (!sound->Play(Players::PlayerOne)) continue;
				m_Voices.emplace(event.Voice, Voice{event.Container, Vector(event.OffsetX, event.OffsetY), event.Bus, event.Rate, event.Paused, std::move(sound)});
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
					case WorldStateSoundProtocol::Action::Pitch: voice.Rate = event.Rate; voice.Sound->SetNetworkPitch(event.Rate * m_Pitch[voice.Bus]); break;
					case WorldStateSoundProtocol::Action::Pause: voice.Paused = event.Paused; voice.Sound->SetPaused(event.Paused || m_BusPaused[voice.Bus]); break;
					case WorldStateSoundProtocol::Action::Fade: voice.Sound->FadeOut(static_cast<int>(event.FadeMilliseconds)); break;
					case WorldStateSoundProtocol::Action::Pan: voice.Sound->SetCustomPanValue(event.Pan); break;
					case WorldStateSoundProtocol::Action::Panning: voice.Sound->SetPanningStrengthMultiplier(event.Panning); break;
					case WorldStateSoundProtocol::Action::GlobalPitch: case WorldStateSoundProtocol::Action::BusPause:
					case WorldStateSoundProtocol::Action::Play: break;
				}
				++applied; ++it;
			}
		}
		return applied;
	}
	bool HasVoice(std::uint64_t id) const { return m_Voices.contains(id); }
	bool HasContainer(std::uint64_t id) const { for (const auto& [voiceID, voice] : m_Voices) if (voice.Container == id) return true; return false; }
	float GetVoicePitch(std::uint64_t id) const {
		const auto found = m_Voices.find(id); if (found == m_Voices.end()) return 0;
		if (!g_AudioMan.IsAudioEnabled()) return found->second.Sound->GetPitch();
		const auto* channels = found->second.Sound->GetPlayingChannels();
		if (channels->empty()) return 0;
		FMOD::Channel* channel = nullptr; float pitch = 0;
		if (g_AudioMan.GetAudioSystem()->getChannel(*channels->begin(), &channel) != FMOD_OK || !channel || channel->getPitch(&pitch) != FMOD_OK) return 0;
		return pitch;
	}
	bool IsVoicePaused(std::uint64_t id) const {
		const auto found = m_Voices.find(id); if (found == m_Voices.end()) return false;
		if (!g_AudioMan.IsAudioEnabled()) return found->second.Sound->IsPaused();
		const auto* channels = found->second.Sound->GetPlayingChannels();
		if (channels->empty()) return false;
		for (int index : *channels) {
			FMOD::Channel* channel = nullptr; bool paused = false;
			if (g_AudioMan.GetAudioSystem()->getChannel(index, &channel) != FMOD_OK || !channel || channel->getPaused(&paused) != FMOD_OK || !paused) return false;
		}
		return true;
	}
	std::size_t Size() const { return m_Voices.size(); }
private:
	struct Voice { std::uint64_t Container; Vector Offset; std::uint8_t Bus; float Rate; bool Paused; std::unique_ptr<SoundContainer> Sound; };
	std::array<float, 3> m_Pitch{1,1,1};
	std::array<bool, 3> m_BusPaused{};
	std::unordered_map<std::uint64_t, Voice> m_Voices;
	std::uint32_t m_Revision = 0;
};

} // namespace RTE
