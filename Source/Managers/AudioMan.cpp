#include "WorldStateSoundCapture.h"
#include "AudioMan.h"

#include "CameraMan.h"
#include "ConsoleMan.h"
#include "FrameMan.h"
#include "SceneMan.h"
#include "ActivityMan.h"
#include "SoundContainer.h"
#include "WindowMan.h"
#include "SoundSet.h"

#include <array>
#include <cstring>

using namespace RTE;

AudioMan::AudioMan() {
	Clear();
}

AudioMan::~AudioMan() {
	Destroy();
}

void AudioMan::Clear() {
	SoundContainer::ResetHeadlessPlayback();
	m_AudioEnabled = false;
	{ std::scoped_lock lock(m_ChannelOwnersMutex); m_ChannelOwners.clear(); }
	{ std::scoped_lock lock(m_FinishedChannelsMutex); m_FinishedChannels.clear(); }
	m_CurrentActivityHumanPlayerPositions.clear();
	m_SoundChannelMinimumAudibleDistances.clear();

	m_MuteMaster = false;
	m_MuteMusic = false;
	m_MuteSounds = false;
	m_MasterVolume = 0.5F;
	m_MusicVolume = 1.0F;
	m_SoundsVolume = 1.0F;
	m_GlobalPitch = 1.0F;

	m_SoundPanningEffectStrength = 0.5F;

	//////////////////////////////////////////////////
	// TODO These need to be removed when our soundscape is sorted out. They're only here temporarily to allow for easier tweaking by pawnis.
	m_ListenerZOffset = 400;
	m_MinimumDistanceForPanning = 30.0F;
	//////////////////////////////////////////////////

	m_MusicMuffled = false;

	m_IsInMultiplayerMode = false;
	for (int i = 0; i < c_MaxClients; i++) {
		m_SoundEvents[i].clear();
	}
}

bool AudioMan::Initialize() {
	FMOD_RESULT audioSystemSetupResult = FMOD::System_Create(&m_AudioSystem);

	FMOD_ADVANCEDSETTINGS audioSystemAdvancedSettings;
	memset(&audioSystemAdvancedSettings, 0, sizeof(audioSystemAdvancedSettings));
	audioSystemAdvancedSettings.cbSize = sizeof(FMOD_ADVANCEDSETTINGS);
	audioSystemSetupResult = (audioSystemSetupResult == FMOD_OK) ? m_AudioSystem->getAdvancedSettings(&audioSystemAdvancedSettings) : audioSystemSetupResult;
	audioSystemAdvancedSettings.vol0virtualvol = 0.001F;
	audioSystemAdvancedSettings.randomSeed = RandomNum(0, INT_MAX);

	audioSystemSetupResult = (audioSystemSetupResult == FMOD_OK) ? m_AudioSystem->setAdvancedSettings(&audioSystemAdvancedSettings) : audioSystemSetupResult;
	audioSystemSetupResult = (audioSystemSetupResult == FMOD_OK) ? m_AudioSystem->set3DSettings(1, c_PPM, 1) : audioSystemSetupResult;
	audioSystemSetupResult = (audioSystemSetupResult == FMOD_OK) ? m_AudioSystem->setSoftwareChannels(c_MaxSoftwareChannels) : audioSystemSetupResult;
	FMOD_INITFLAGS flags = FMOD_INIT_VOL0_BECOMES_VIRTUAL;

#if !RELEASE_BUILD
	flags |= FMOD_INIT_PROFILE_ENABLE;
#endif

	audioSystemSetupResult = (audioSystemSetupResult == FMOD_OK) ? m_AudioSystem->init(c_MaxVirtualChannels, flags, 0) : audioSystemSetupResult;

	audioSystemSetupResult = (audioSystemSetupResult == FMOD_OK) ? m_AudioSystem->getMasterChannelGroup(&m_MasterChannelGroup) : audioSystemSetupResult;
	audioSystemSetupResult = (audioSystemSetupResult == FMOD_OK) ? m_AudioSystem->createChannelGroup("SFX", &m_SFXChannelGroup) : audioSystemSetupResult;
	audioSystemSetupResult = (audioSystemSetupResult == FMOD_OK) ? m_AudioSystem->createChannelGroup("UI", &m_UIChannelGroup) : audioSystemSetupResult;
	audioSystemSetupResult = (audioSystemSetupResult == FMOD_OK) ? m_AudioSystem->createChannelGroup("Music", &m_MusicChannelGroup) : audioSystemSetupResult;

	// Add a lowpass filter to the music channel group for pause menu usage
	FMOD::DSP* dsp_multibandeq;
	audioSystemSetupResult = (audioSystemSetupResult == FMOD_OK) ? m_AudioSystem->createDSPByType(FMOD_DSP_TYPE_MULTIBAND_EQ, &dsp_multibandeq) : audioSystemSetupResult;
	audioSystemSetupResult = (audioSystemSetupResult == FMOD_OK) ? dsp_multibandeq->setParameterFloat(1, 22000.0f) : audioSystemSetupResult; // Functionally inactive lowpass filter
	audioSystemSetupResult = (audioSystemSetupResult == FMOD_OK) ? m_MusicChannelGroup->addDSP(0, dsp_multibandeq) : audioSystemSetupResult;

	// Add a safety limiter to the master channel group, after fader
	FMOD::DSP* dsp_limiter;
	audioSystemSetupResult = (audioSystemSetupResult == FMOD_OK) ? m_AudioSystem->createDSPByType(FMOD_DSP_TYPE_LIMITER, &dsp_limiter) : audioSystemSetupResult;
	audioSystemSetupResult = (audioSystemSetupResult == FMOD_OK) ? m_MasterChannelGroup->addDSP(0, dsp_limiter) : audioSystemSetupResult;

	// Add a compressor to the SFX channel group, pre fader
	// This is pretty heavy-handed, but it sounds great. Might need to be changed once we have sidechaining and fancier things going on.
	FMOD::DSP* dsp_compressor;
	audioSystemSetupResult = (audioSystemSetupResult == FMOD_OK) ? m_AudioSystem->createDSPByType(FMOD_DSP_TYPE_COMPRESSOR, &dsp_compressor) : audioSystemSetupResult;
	audioSystemSetupResult = (audioSystemSetupResult == FMOD_OK) ? dsp_compressor->setParameterFloat(0, -10.0f) : audioSystemSetupResult; // Threshold
	audioSystemSetupResult = (audioSystemSetupResult == FMOD_OK) ? dsp_compressor->setParameterFloat(1, 3.0f) : audioSystemSetupResult; // Ratio
	audioSystemSetupResult = (audioSystemSetupResult == FMOD_OK) ? dsp_compressor->setParameterFloat(2, 180.0f) : audioSystemSetupResult; // Attack time
	audioSystemSetupResult = (audioSystemSetupResult == FMOD_OK) ? dsp_compressor->setParameterFloat(3, 250.0f) : audioSystemSetupResult; // Release time
	audioSystemSetupResult = (audioSystemSetupResult == FMOD_OK) ? dsp_compressor->setParameterFloat(4, 5.0f) : audioSystemSetupResult; // Make-up gain
	audioSystemSetupResult = (audioSystemSetupResult == FMOD_OK) ? m_SFXChannelGroup->addDSP(1, dsp_compressor) : audioSystemSetupResult;

	audioSystemSetupResult = (audioSystemSetupResult == FMOD_OK) ? m_MasterChannelGroup->addGroup(m_SFXChannelGroup) : audioSystemSetupResult;
	audioSystemSetupResult = (audioSystemSetupResult == FMOD_OK) ? m_MasterChannelGroup->addGroup(m_UIChannelGroup) : audioSystemSetupResult;
	audioSystemSetupResult = (audioSystemSetupResult == FMOD_OK) ? m_MasterChannelGroup->addGroup(m_MusicChannelGroup) : audioSystemSetupResult;

	m_AudioEnabled = audioSystemSetupResult == FMOD_OK;

	if (!m_AudioEnabled) {
		return false;
	}

	if (m_MuteSounds) {
		SetSoundsMuted();
	}
	if (m_MuteMusic) {
		SetMusicMuted();
	}
	if (m_MuteMaster) {
		SetMasterMuted();
	}

	SetGlobalPitch(m_GlobalPitch, false, false);
	SetSoundsVolume(m_SoundsVolume);
	SetMusicVolume(m_MusicVolume);
	SetMasterVolume(m_MasterVolume);

	return true;
}

void AudioMan::Destroy() {
	if (m_AudioEnabled) {
		StopAll();
		const auto closed = m_AudioSystem->close();
		if (closed != FMOD_OK) g_ConsoleMan.PrintString("ERROR: FMOD close failed: " + std::string(FMOD_ErrorString(closed)));
		m_AudioSystem->release();
		m_AudioSystem = nullptr;
	}
	Clear();
}

void AudioMan::Update() {
	if (m_AudioEnabled) {
		DrainFinishedChannels();
		FMOD_RESULT status = FMOD_OK;

		if (m_MuteAudioOnFocusLoss && !g_WindowMan.AnyWindowHasFocus()) {
			m_MasterChannelGroup->setMute(true);
		} else {
			m_MasterChannelGroup->setMute(m_MuteMaster);
		}

		float globalPitch = 1.0F;

		float timeScale = g_TimerMan.GetTimeScale();
		// Soften the ratio of the pitch adjustment so it's not such an extreme effect on the audio.
		// TODO: This coefficient should probably move to SettingsMan and be loaded from ini. That way this effect can be lessened or even turned off entirely by users. 0.35 is a good default value though.
		globalPitch = timeScale + (1.0F - timeScale) * 0.35F;

		SetGlobalPitch(globalPitch);

		if (!g_ActivityMan.ActivityPaused()) {
			const Activity* currentActivity = g_ActivityMan.GetActivity();
			uint8_t currentActivityHumanCount = m_IsInMultiplayerMode ? 1 : currentActivity->GetHumanCount();

			if (m_CurrentActivityHumanPlayerPositions.size() != currentActivityHumanCount) {
				status = status == FMOD_OK ? m_AudioSystem->set3DNumListeners(currentActivityHumanCount) : status;
			}

			m_CurrentActivityHumanPlayerPositions.clear();
			for (int player = Players::PlayerOne; player < Players::MaxPlayerCount && m_CurrentActivityHumanPlayerPositions.size() < currentActivityHumanCount; player++) {
				if (currentActivity->PlayerActive(player) && currentActivity->PlayerHuman(player)) {
					int screen = currentActivity->ScreenOfPlayer(player);
					Vector humanPlayerPosition = g_CameraMan.GetScrollTarget(screen);
					if (IsInMultiplayerMode()) {
						humanPlayerPosition += (Vector(static_cast<float>(g_FrameMan.GetPlayerFrameBufferWidth(screen)), static_cast<float>(g_FrameMan.GetPlayerFrameBufferHeight(screen))) / 2);
					}
					m_CurrentActivityHumanPlayerPositions.push_back(std::make_unique<const Vector>(humanPlayerPosition));
				}
			}

			int listenerNumber = 0;
			for (const std::unique_ptr<const Vector>& humanPlayerPosition: m_CurrentActivityHumanPlayerPositions) {
				if (status == FMOD_OK) {
					FMOD_VECTOR playerPosition = GetAsFMODVector(*(humanPlayerPosition.get()), m_ListenerZOffset);
					status = m_AudioSystem->set3DListenerAttributes(listenerNumber, &playerPosition, nullptr, &c_FMODForward, &c_FMODUp);
				}
				listenerNumber++;
			}

			Update3DEffectsForSFXChannels();
		} else {
			if (!m_CurrentActivityHumanPlayerPositions.empty()) {
				m_CurrentActivityHumanPlayerPositions.clear();
				status = status == FMOD_OK ? m_AudioSystem->set3DNumListeners(1) : status;
			}
			if (status == FMOD_OK) {
				FMOD_VECTOR scrollTarget = GetAsFMODVector(g_CameraMan.GetScrollTarget(), m_ListenerZOffset);
				status = m_AudioSystem->set3DListenerAttributes(0, &scrollTarget, nullptr, &c_FMODForward, &c_FMODUp);
			}
		}

		status = status == FMOD_OK ? m_AudioSystem->update() : status;
		DrainFinishedChannels();
		if (status != FMOD_OK) {
			g_ConsoleMan.PrintString("ERROR: Could not update AudioMan due to FMOD error: " + std::string(FMOD_ErrorString(status)));
		}
	}
}

void AudioMan::SetGlobalPitch(float pitch, bool includeImmobileSounds, bool includeMusic) {
	if (!m_AudioEnabled) {
		m_GlobalPitch = std::clamp(pitch, 0.125F, 8.0F);
		SoundContainer::SetHeadlessGlobalPitch(m_GlobalPitch, includeImmobileSounds, includeMusic);
		return;
	}
	if (m_IsInMultiplayerMode) {
		RegisterSoundEvent(-1, SOUND_SET_GLOBAL_PITCH, nullptr);
	}

	m_GlobalPitch = std::clamp(pitch, 0.125F, 8.0F);

	m_SFXChannelGroup->setPitch(m_GlobalPitch);

	if (includeImmobileSounds) {
		m_UIChannelGroup->setPitch(m_GlobalPitch);
	}

	if (includeMusic) {
		m_MusicChannelGroup->setPitch(m_GlobalPitch);
	}
}

bool AudioMan::SetMusicPitch(float pitch) {
	if (!m_AudioEnabled) {
		return false;
	}

	pitch = Limit(pitch, 8, 0.125); // Limit pitch change to 8 octaves up or down
	FMOD_RESULT result = m_MusicChannelGroup->setPitch(pitch);

	if (result != FMOD_OK) {
		g_ConsoleMan.PrintString("ERROR: Could not set music pitch: " + std::string(FMOD_ErrorString(result)));
	}

	return true;
}

void AudioMan::StopAll() {
	GetWorldStateSoundCapture().StopAll();
	if (m_AudioEnabled) {
		std::scoped_lock lock(m_ChannelOwnersMutex);
		for (const auto& [index, owner] : m_ChannelOwners) {
			FMOD::Channel* channel = nullptr;
			if (m_AudioSystem->getChannel(index, &channel) == FMOD_OK && channel) {
				channel->setCallback(nullptr); channel->setUserData(nullptr);
			}
			if (owner.Container) owner.Container->RemovePlayingChannel(index);
		}
		m_MasterChannelGroup->stop();
		m_ChannelOwners.clear();
		{ std::scoped_lock distanceLock(m_SoundChannelMinimumAudibleDistancesMutex); m_SoundChannelMinimumAudibleDistances.clear(); }
	}
	else SoundContainer::StopHeadlessPlayback();
}
void AudioMan::PauseIngameSounds(bool pause) {
	if (m_AudioEnabled) m_SFXChannelGroup->setPaused(pause);
	else SoundContainer::PauseHeadlessIngamePlayback(pause);
}
void AudioMan::FinishIngameLoopingSounds() {
	if (!m_AudioEnabled) { SoundContainer::FinishHeadlessIngameLoops(); return; }
	if (m_AudioEnabled) {
		int numberOfPlayingChannels;
		FMOD::Channel* soundChannel;

		FMOD_RESULT result = m_SFXChannelGroup->getNumChannels(&numberOfPlayingChannels);
		if (result != FMOD_OK) {
			g_ConsoleMan.PrintString("ERROR: Failed to get the number of playing SFX sound channels when finishing all looping sounds: " + std::string(FMOD_ErrorString(result)));
			return;
		}

		for (int i = 0; i < numberOfPlayingChannels; i++) {
			result = m_SFXChannelGroup->getChannel(i, &soundChannel);
			if (result != FMOD_OK) {
				g_ConsoleMan.PrintString("ERROR: Failed to get SFX sound channel when finishing all looping sounds: " + std::string(FMOD_ErrorString(result)));
				return;
			}
			soundChannel->setLoopCount(0);
		}
	}
}

SoundContainer* AudioMan::PlaySound(const std::string& filePath, const Vector& position, int player) {
	if (m_IsInMultiplayerMode) {
		return nullptr;
	}

	SoundContainer* newSoundContainer = new SoundContainer();
	newSoundContainer->SetPosition(position);
	newSoundContainer->GetTopLevelSoundSet().AddSound(filePath);
	if (newSoundContainer->HasAnySounds()) {
		PlaySoundContainer(newSoundContainer, player);
	}
	return newSoundContainer;
}

void AudioMan::GetSoundEvents(int player, std::list<NetworkSoundData>& list) {
	if (player < 0 || player >= c_MaxClients) {
		return;
	}
	list.clear();

	g_SoundEventsListMutex[player].lock();
	const NetworkSoundData* lastSetGlobalPitchEvent = nullptr;
	for (const NetworkSoundData& soundEvent: m_SoundEvents[player]) {
		if (soundEvent.State == SOUND_SET_GLOBAL_PITCH) {
			lastSetGlobalPitchEvent = &soundEvent;
		} else {
			list.push_back(soundEvent);
		}
	}
	if (lastSetGlobalPitchEvent) {
		list.push_back(*lastSetGlobalPitchEvent);
	}
	m_SoundEvents[player].clear();
	g_SoundEventsListMutex[player].unlock();
}

void AudioMan::RegisterSoundEvent(int player, NetworkSoundState state, const SoundContainer* soundContainer, int fadeoutTime) {
	if (player == -1) {
		for (int i = 0; i < c_MaxClients; i++) {
			RegisterSoundEvent(i, state, soundContainer, fadeoutTime);
		}
	} else {
		FMOD_RESULT result = FMOD_OK;
		std::vector<NetworkSoundData> soundDataVector;

		if (state == SOUND_SET_GLOBAL_PITCH) {
			NetworkSoundData soundData;
			soundData.State = state;
			soundData.Pitch = m_GlobalPitch;
			soundDataVector.push_back(soundData);
		} else {
			for (int playingChannel: *soundContainer->GetPlayingChannels()) {
				FMOD::Channel* soundChannel;
				result = m_AudioSystem->getChannel(playingChannel, &soundChannel);
				FMOD::Sound* sound;
				result = (result == FMOD_OK) ? soundChannel->getCurrentSound(&sound) : result;

				if (result != FMOD_OK) {
					continue;
				}
				NetworkSoundData soundData;
				soundData.State = state;
				soundData.SoundFileHash = soundContainer->GetSoundDataForSound(sound)->SoundFile.GetHash();
				soundData.Channel = playingChannel;
				soundData.Immobile = soundContainer->IsImmobile();
				soundData.AttenuationStartDistance = soundContainer->GetAttenuationStartDistance();
				soundData.Loops = soundContainer->GetLoopSetting();
				soundData.Priority = soundContainer->GetPriority();
				soundData.AffectedByGlobalPitch = soundContainer->IsAffectedByGlobalPitch();
				soundData.Position[0] = soundContainer->GetPosition().m_X;
				soundData.Position[1] = soundContainer->GetPosition().m_Y;
				soundData.Volume = soundContainer->GetVolume();
				soundData.Pitch = soundContainer->GetPitch();
				soundData.FadeOutTime = fadeoutTime;
				soundDataVector.push_back(soundData);
			}
		}

		g_SoundEventsListMutex[player].lock();
		m_SoundEvents[player].insert(m_SoundEvents[player].end(), soundDataVector.begin(), soundDataVector.end());
		g_SoundEventsListMutex[player].unlock();
	}
}

void AudioMan::ClearSoundEvents(int player) {
	if (player == -1 || player >= c_MaxClients) {
		for (int i = 0; i < c_MaxClients; i++) {
			ClearSoundEvents(i);
		}
	} else {
		g_SoundEventsListMutex[player].lock();
		m_SoundEvents[player].clear();
		g_SoundEventsListMutex[player].unlock();
	}
}

bool AudioMan::PlaySoundContainer(SoundContainer* soundContainer, int player) {
	if (!m_AudioEnabled && soundContainer) return soundContainer->Play(player);
	if (!m_AudioEnabled || !soundContainer || soundContainer->GetPlayingChannels()->size() >= c_MaxPlayingSoundsPerContainer) {
		return false;
	}
	FMOD_RESULT result = FMOD_OK;

	if (!soundContainer->SoundPropertiesUpToDate()) {
		result = soundContainer->UpdateSoundProperties();
		soundContainer->GetTopLevelSoundSet().SelectNextSounds();
		if (result != FMOD_OK) {
			g_ConsoleMan.PrintString("ERROR: Could not update sound properties for SoundContainer " + soundContainer->GetPresetName() + ": " + std::string(FMOD_ErrorString(result)));
			return false;
		}
	}

	FMOD::ChannelGroup* channelGroupToPlayIn = m_SFXChannelGroup;

	switch (soundContainer->GetBusRouting()) {
		case SoundContainer::UI:
			channelGroupToPlayIn = m_UIChannelGroup;
			break;
		case SoundContainer::SFX:
			channelGroupToPlayIn = m_SFXChannelGroup;
			break;
		case SoundContainer::MUSIC:
			channelGroupToPlayIn = m_MusicChannelGroup;
			break;
	}

	FMOD::Channel* channel;
	int channelIndex;
	std::vector<const SoundData*> selectedSoundData;
	soundContainer->GetTopLevelSoundSet().GetFlattenedSoundData(selectedSoundData, true);
	float pitchVariationFactor = 1.0F + std::abs(soundContainer->GetPitchVariation());
	for (const SoundData* soundData: selectedSoundData) {
		result = (result == FMOD_OK) ? m_AudioSystem->playSound(soundData->SoundObject, channelGroupToPlayIn, true, &channel) : result;
		result = (result == FMOD_OK) ? channel->getIndex(&channelIndex) : result;

		result = (result == FMOD_OK) ? channel->setUserData(soundContainer) : result;
		result = (result == FMOD_OK) ? channel->setCallback(SoundChannelEndedCallback) : result;
		result = (result == FMOD_OK) ? channel->setPriority(soundContainer->GetPriority()) : result;
		float pitchVariationMultiplier = pitchVariationFactor == 1.0F ? 1.0F : RandomNum(1.0F / pitchVariationFactor, 1.0F * pitchVariationFactor);
		result = (result == FMOD_OK) ? channel->setPitch(soundContainer->GetPitch() * pitchVariationMultiplier) : result;

		if (soundContainer->GetCustomPanValue() != 0.0f) {
			result = (result == FMOD_OK) ? channel->setPan(soundContainer->GetCustomPanValue()) : result;
		}

		if (soundContainer->IsImmobile()) {
			result = (result == FMOD_OK) ? channel->setVolume(soundContainer->GetVolume()) : result;
		} else {

			FMOD::DSP* dsp_multibandeq;
			result = (result == FMOD_OK) ? m_AudioSystem->createDSPByType(FMOD_DSP_TYPE_MULTIBAND_EQ, &dsp_multibandeq) : result;
			result = (result == FMOD_OK) ? dsp_multibandeq->setParameterFloat(1, 22000.0f) : result; // Functionally inactive lowpass filter
			result = (result == FMOD_OK) ? channel->addDSP(0, dsp_multibandeq) : result;

			{
				std::scoped_lock<std::mutex> lock(m_SoundChannelMinimumAudibleDistancesMutex);
				m_SoundChannelMinimumAudibleDistances.insert({channelIndex, soundData->MinimumAudibleDistance});
			}

			result = (result == FMOD_OK) ? channel->set3DLevel(m_SoundPanningEffectStrength * soundContainer->GetPanningStrengthMultiplier()) : result;

			FMOD_VECTOR soundContainerPosition = GetAsFMODVector(soundContainer->GetPosition() + soundData->Offset);
			UpdatePositionalEffectsForSoundChannel(channel, &soundContainerPosition);
		}

		if (result != FMOD_OK) {
			g_ConsoleMan.PrintString("ERROR: Could not play sounds from SoundContainer " + soundContainer->GetPresetName() + ": " + std::string(FMOD_ErrorString(result)));
			return false;
		}

		// At this point the sound is ready to go, but if the SoundContainer is explicitly paused, it and this new channel will hopefully be unpaused
		// at some later point in time by whatever paused it
		if (!soundContainer->IsPaused()) {
			result = channel->setPaused(false);
			if (result != FMOD_OK) {
				g_ConsoleMan.PrintString("ERROR: Failed to start playing sounds from SoundContainer " + soundContainer->GetPresetName() + " after setting it up: " + std::string(FMOD_ErrorString(result)));
				return false;
			}
		}

		{ std::scoped_lock lock(m_ChannelOwnersMutex);
			if (const auto previous = m_ChannelOwners.find(channelIndex); previous != m_ChannelOwners.end() && previous->second.Container)
				previous->second.Container->RemovePlayingChannel(channelIndex);
			m_ChannelOwners[channelIndex] = {soundContainer, soundContainer->GetCustomPanValue(), soundContainer->GetPanningStrengthMultiplier(), soundContainer->GetVolume(), soundContainer->GetLoopSetting() == -1};
			soundContainer->AddPlayingChannel(channelIndex);
		}
		soundContainer->CaptureNetworkPlay(*soundData, pitchVariationMultiplier, player);
	}

	if (m_IsInMultiplayerMode) {
		RegisterSoundEvent(player, SOUND_PLAY, soundContainer);
	}

	// Choose the sounds for next time
	bool choseNext = soundContainer->GetTopLevelSoundSet().SelectNextSounds();
	RTEAssert(choseNext, "Unable to select new sounds to play for SoundContainer " + soundContainer->GetPresetName());

	return true;
}

bool AudioMan::ChangeSoundContainerPlayingChannelsPosition(const SoundContainer* soundContainer) {
	if (!m_AudioEnabled || !soundContainer) {
		return false;
	}
	if (m_IsInMultiplayerMode) {
		RegisterSoundEvent(-1, SOUND_SET_POSITION, soundContainer);
	}

	FMOD_RESULT result = FMOD_OK;
	FMOD::Channel* soundChannel;
	FMOD::Sound* sound;

	const std::unordered_set<int>* playingChannels = soundContainer->GetPlayingChannels();
	for (int channelIndex: *playingChannels) {
		result = m_AudioSystem->getChannel(channelIndex, &soundChannel);
		result = (result == FMOD_OK) ? soundChannel->getCurrentSound(&sound) : result;
		const SoundData* soundData = soundContainer->GetSoundDataForSound(sound);

		FMOD_VECTOR soundPosition = GetAsFMODVector(soundContainer->GetPosition() + ((soundData == nullptr) ? Vector() : soundData->Offset));
		result = (result == FMOD_OK) ? UpdatePositionalEffectsForSoundChannel(soundChannel, &soundPosition) : result;
		if (result != FMOD_OK) {
			g_ConsoleMan.PrintString("ERROR: Could not set sound position for the sound being played on channel " + std::to_string(channelIndex) + " for SoundContainer " + soundContainer->GetPresetName() + ": " + std::string(FMOD_ErrorString(result)));
		}
	}
	return result == FMOD_OK;
}

float AudioMan::GetSoundContainerAudibleVolume(const SoundContainer* soundContainer) {
	if (!m_AudioEnabled || !soundContainer || !soundContainer->IsBeingPlayed()) {
		return 0.0F;
	}

	FMOD_RESULT result;
	FMOD::Channel* soundChannel;
	float audibleVolume;

	const std::unordered_set<int> channels = *soundContainer->GetPlayingChannels();
	for (int channel: channels) {
		result = m_AudioSystem->getChannel(channel, &soundChannel);
		result = (result == FMOD_OK) ? soundChannel->getAudibility(&audibleVolume) : result;

		if (result != FMOD_OK) {
			g_ConsoleMan.PrintString("ERROR: Could not get sound audible volume in SoundContainer " + soundContainer->GetPresetName() + ": " + std::string(FMOD_ErrorString(result)));
		} else {
			// Simply return the first one, they are all the same
			return audibleVolume;
		}
	}
	return 0.0F;
}

bool AudioMan::ChangeSoundContainerPlayingChannelsVolume(const SoundContainer* soundContainer, float newVolume) {
	if (!m_AudioEnabled || !soundContainer || !soundContainer->IsBeingPlayed()) {
		return false;
	}
	if (m_IsInMultiplayerMode) {
		RegisterSoundEvent(-1, SOUND_SET_VOLUME, soundContainer);
	}

	FMOD_RESULT result = FMOD_OK;
	FMOD::Channel* soundChannel;
	float soundContainerOldVolume = soundContainer->GetVolume() == 0 ? 1.0F : soundContainer->GetVolume();
	float soundChannelCurrentVolume;

	const std::unordered_set<int>* playingChannels = soundContainer->GetPlayingChannels();
	for (int channelIndex: *playingChannels) {
		result = m_AudioSystem->getChannel(channelIndex, &soundChannel);
		result = result == FMOD_OK ? soundChannel->getVolume(&soundChannelCurrentVolume) : result;

		if (newVolume == 0.0F) {
			result = result == FMOD_OK ? soundChannel->setMute(true) : result;
			result = result == FMOD_OK ? soundChannel->setVolume(soundChannelCurrentVolume / soundContainerOldVolume) : result;
		} else {
			result = result == FMOD_OK ? soundChannel->setMute(false) : result;
			result = result == FMOD_OK ? soundChannel->setVolume(newVolume / soundContainerOldVolume * soundChannelCurrentVolume) : result;
		}
		if (result != FMOD_OK) {
			g_ConsoleMan.PrintString("ERROR: Could not update sound volume for the sound being played on channel " + std::to_string(channelIndex) + " for SoundContainer " + soundContainer->GetPresetName() + ": " + std::string(FMOD_ErrorString(result)));
		}
	}
	return result == FMOD_OK;
}

bool AudioMan::ChangeSoundContainerPlayingChannelsPitch(const SoundContainer* soundContainer) {
	if (!m_AudioEnabled || !soundContainer || !soundContainer->IsBeingPlayed()) {
		return false;
	}
	if (m_IsInMultiplayerMode) {
		RegisterSoundEvent(-1, SOUND_SET_PITCH, soundContainer);
	}

	FMOD_RESULT result = FMOD_OK;
	FMOD::Channel* soundChannel;

	const std::unordered_set<int>* playingChannels = soundContainer->GetPlayingChannels();
	for (int channelIndex: *playingChannels) {
		result = m_AudioSystem->getChannel(channelIndex, &soundChannel);
		result = result == FMOD_OK ? soundChannel->setPitch(soundContainer->GetPitch()) : result;
		if (result != FMOD_OK) {
			g_ConsoleMan.PrintString("ERROR: Could not update sound pitch for the sound being played on channel " + std::to_string(channelIndex) + " for SoundContainer " + soundContainer->GetPresetName() + ": " + std::string(FMOD_ErrorString(result)));
		}
	}
	return result == FMOD_OK;
}

bool AudioMan::ChangeSoundContainerPlayingChannelsCustomPanValue(const SoundContainer* soundContainer) {
	if (!m_AudioEnabled || !soundContainer || !soundContainer->IsBeingPlayed()) {
		return false;
	}
	if (m_IsInMultiplayerMode) {
		RegisterSoundEvent(-1, SOUND_SET_PITCH, soundContainer);
	}

	FMOD_RESULT result = FMOD_OK;
	FMOD::Channel* soundChannel;

	const std::unordered_set<int>* playingChannels = soundContainer->GetPlayingChannels();
	for (int channelIndex: *playingChannels) {
		result = m_AudioSystem->getChannel(channelIndex, &soundChannel);
		result = result == FMOD_OK ? soundChannel->setPan(soundContainer->GetCustomPanValue()) : result;
		if (result != FMOD_OK) {
			g_ConsoleMan.PrintString("ERROR: Could not update sound custom pan value for the sound being played on channel " + std::to_string(channelIndex) + " for SoundContainer " + soundContainer->GetPresetName() + ": " + std::string(FMOD_ErrorString(result)));
		}
	}
	return result == FMOD_OK;
}

bool AudioMan::StopSoundContainerPlayingChannels(SoundContainer* soundContainer, int player) {
	WorldStateSoundProtocol::Event capturedStop; capturedStop.Player = static_cast<std::int8_t>(player);
	GetWorldStateSoundCapture().Command(soundContainer, capturedStop);
	if (!m_AudioEnabled || !soundContainer || !soundContainer->IsBeingPlayed()) {
		return false;
	}
	if (m_IsInMultiplayerMode) {
		RegisterSoundEvent(player, SOUND_STOP, soundContainer);
	}

	FMOD_RESULT result = FMOD_OK;
	FMOD::Channel* soundChannel;

	std::scoped_lock ownerLock(m_ChannelOwnersMutex);
	const auto* channels = soundContainer->GetPlayingChannels();
	const std::vector<int> indices(channels->begin(), channels->end());
	for (int index : indices) {
		const auto status = m_AudioSystem->getChannel(index, &soundChannel);
		if (status == FMOD_OK && soundChannel) {
			soundChannel->setCallback(nullptr); soundChannel->setUserData(nullptr);
			const auto stopped = soundChannel->stop();
			if (stopped != FMOD_OK && stopped != FMOD_ERR_INVALID_HANDLE) result = stopped;
		}
		soundContainer->RemovePlayingChannel(index); m_ChannelOwners.erase(index);
		std::scoped_lock distanceLock(m_SoundChannelMinimumAudibleDistancesMutex);
		m_SoundChannelMinimumAudibleDistances.erase(index);
	}
	return result == FMOD_OK;
}

void AudioMan::FadeOutSoundContainerPlayingChannels(SoundContainer* soundContainer, int fadeOutTime) {
	if (!m_AudioEnabled || !soundContainer || !soundContainer->IsBeingPlayed()) {
		return;
	}
	if (m_IsInMultiplayerMode) {
		RegisterSoundEvent(-1, SOUND_FADE_OUT, soundContainer, fadeOutTime);
	}

	int sampleRate;
	m_AudioSystem->getSoftwareFormat(&sampleRate, nullptr, nullptr);
	int fadeOutTimeAsSamples = fadeOutTime * sampleRate / 1000;

	FMOD_RESULT result;
	FMOD::Channel* soundChannel;
	unsigned long long parentClock;
	float currentVolume;

	const std::unordered_set<int> channels = *soundContainer->GetPlayingChannels();
	for (int channel: channels) {
		result = m_AudioSystem->getChannel(channel, &soundChannel);
		result = (result == FMOD_OK) ? soundChannel->getDSPClock(nullptr, &parentClock) : result;
		result = (result == FMOD_OK) ? soundChannel->getVolume(&currentVolume) : result;
		result = (result == FMOD_OK) ? soundChannel->addFadePoint(parentClock, currentVolume) : result;
		result = (result == FMOD_OK) ? soundChannel->addFadePoint(parentClock + fadeOutTimeAsSamples, 0) : result;

		if (result != FMOD_OK) {
			g_ConsoleMan.PrintString("ERROR: Could not fade out sounds in SoundContainer " + soundContainer->GetPresetName() + ": " + std::string(FMOD_ErrorString(result)));
		}
	}
}

void AudioMan::SetPausedSoundContainerPlayingChannels(SoundContainer* soundContainer, bool paused) const {
	FMOD_RESULT result = FMOD_OK;
	FMOD::Channel* soundChannel;

	const std::unordered_set<int>* playingChannels = soundContainer->GetPlayingChannels();
	for (int channelIndex: *playingChannels) {
		result = m_AudioSystem->getChannel(channelIndex, &soundChannel);
		result = (result == FMOD_OK) ? soundChannel->setPaused(paused) : result;
		if (result != FMOD_OK) {
			g_ConsoleMan.PrintString("ERROR: Could not set pausedness for SoundContainer " + soundContainer->GetPresetName() + ": " + std::string(FMOD_ErrorString(result)));
		}
	}
}

void AudioMan::Update3DEffectsForSFXChannels() {
	int numberOfPlayingChannels;
	FMOD::Channel* soundChannel;

	FMOD_RESULT result = m_SFXChannelGroup->getNumChannels(&numberOfPlayingChannels);
	if (result != FMOD_OK) {
		g_ConsoleMan.PrintString("ERROR: Failed to get the number of playing channels when updating calculated sound effects for all playing channels: " + std::string(FMOD_ErrorString(result)));
		return;
	}

	for (int i = 0; i < numberOfPlayingChannels; i++) {
		result = m_SFXChannelGroup->getChannel(i, &soundChannel);
		FMOD_MODE mode;
		result = (result == FMOD_OK) ? soundChannel->getMode(&mode) : result;
		if (result == FMOD_OK && (mode & FMOD_2D) == 0) {
			FMOD_VECTOR channelPosition;
			result = result == FMOD_OK ? soundChannel->get3DAttributes(&channelPosition, nullptr) : result;
			result = result == FMOD_OK ? UpdatePositionalEffectsForSoundChannel(soundChannel, &channelPosition) : result;
			float channel3dLevel;
			result = (result == FMOD_OK) ? soundChannel->get3DLevel(&channel3dLevel) : result;
			if (result == FMOD_OK && m_CurrentActivityHumanPlayerPositions.size() == 1) {
				float sqrDistanceToPlayer = (*(m_CurrentActivityHumanPlayerPositions[0].get()) - GetAsVector(channelPosition)).GetSqrMagnitude();
				float doubleMinimumDistanceForPanning = m_MinimumDistanceForPanning * 2.0F;
				void* userData;
				result = result == FMOD_OK ? soundChannel->getUserData(&userData) : result;
				if (result == FMOD_OK) {
					const SoundContainer* soundContainer = static_cast<SoundContainer*>(userData);
					float pan = 0, panning = 1;
					if (soundContainer) { pan = soundContainer->GetCustomPanValue(); panning = soundContainer->GetPanningStrengthMultiplier(); }
					else {
						int index = 0;
						if (soundChannel->getIndex(&index) == FMOD_OK) {
							std::scoped_lock lock(m_ChannelOwnersMutex);
							if (const auto found = m_ChannelOwners.find(index); found != m_ChannelOwners.end()) { pan = found->second.Pan; panning = found->second.Panning; }
						}
					}
					if (sqrDistanceToPlayer < (m_MinimumDistanceForPanning * m_MinimumDistanceForPanning) || pan != 0.0f) {
						result = soundChannel->set3DLevel(0);
					} else if (sqrDistanceToPlayer < (doubleMinimumDistanceForPanning * doubleMinimumDistanceForPanning)) {
						result = soundChannel->set3DLevel(Lerp(0, 1, 0, m_SoundPanningEffectStrength * panning, channel3dLevel));
					} else {
						result = soundChannel->set3DLevel(m_SoundPanningEffectStrength * panning);
					}
				}
			}
		}

		if (result != FMOD_OK) {
			g_ConsoleMan.PrintString("ERROR: An error occurred updating calculated sound effects for playing channel with index " + std::to_string(i) + ": " + std::string(FMOD_ErrorString(result)));
			continue;
		}
	}
}

FMOD_RESULT AudioMan::UpdatePositionalEffectsForSoundChannel(FMOD::Channel* soundChannel, const FMOD_VECTOR* positionOverride) const {
	void* userData;
	FMOD_RESULT result = soundChannel->getUserData(&userData);

	if (result != FMOD_OK) {
		return result;
	}

	const SoundContainer* channelSoundContainer = static_cast<SoundContainer*>(userData);
	float sourcePan = 0, sourceVolume = 1;
	if (channelSoundContainer) { sourcePan = channelSoundContainer->GetCustomPanValue(); sourceVolume = channelSoundContainer->GetVolume(); }
	else {
		int index = 0;
		if (soundChannel->getIndex(&index) == FMOD_OK) {
			std::scoped_lock lock(m_ChannelOwnersMutex);
			if (const auto found = m_ChannelOwners.find(index); found != m_ChannelOwners.end()) { sourcePan = found->second.Pan; sourceVolume = found->second.Volume; }
		}
	}

	bool sceneWraps = g_SceneMan.SceneWrapsX();

	FMOD_VECTOR channelPosition;
	if (positionOverride) {
		channelPosition = *positionOverride;
	} else if (sceneWraps) {
		// NOTE If the scene doesn't wrap and the position hasn't changed, this method doesn't set the channel position below, so there's no need to get it here.
		result = soundChannel->get3DAttributes(&channelPosition, nullptr);
		if (result != FMOD_OK) {
			return result;
		}
	}

	float halfSceneWidth = static_cast<float>(g_SceneMan.GetSceneWidth()) / 2.0F;
	std::array<FMOD_VECTOR, 2> wrappedChannelPositions;
	if (!sceneWraps) {
		wrappedChannelPositions = {channelPosition};
	} else {
		if (channelPosition.x <= halfSceneWidth) {
			wrappedChannelPositions = {channelPosition, {channelPosition.x + g_SceneMan.GetSceneWidth(), channelPosition.y, 0.0}};
		} else {
			wrappedChannelPositions = {FMOD_VECTOR({channelPosition.x - g_SceneMan.GetSceneWidth(), channelPosition.y, 0.0}), channelPosition};
		}
	}

	float sqrShortestDistance = c_SoundMaxAudibleDistance * c_SoundMaxAudibleDistance;
	float sqrLongestDistance = 0.0F;
	for (const std::unique_ptr<const Vector>& humanPlayerPosition: m_CurrentActivityHumanPlayerPositions) {
		for (const FMOD_VECTOR& wrappedChannelPosition: wrappedChannelPositions) {
			float sqrDistanceToChannelPosition = (*(humanPlayerPosition.get()) - GetAsVector(wrappedChannelPosition)).GetSqrMagnitude();
			if (sqrDistanceToChannelPosition < sqrShortestDistance) {
				sqrShortestDistance = sqrDistanceToChannelPosition;
				channelPosition = wrappedChannelPosition;
			}
			if (sqrDistanceToChannelPosition > sqrLongestDistance) {
				sqrLongestDistance = sqrDistanceToChannelPosition;
			}
			if (!sceneWraps) {
				break;
			}
		}
	}
	float shortestDistance = std::sqrt(sqrShortestDistance);

	int soundChannelIndex;
	result = result == FMOD_OK ? soundChannel->getIndex(&soundChannelIndex) : result;
	if (result != FMOD_OK) {
		return result;
	}

	float attenuationStartDistance = c_DefaultAttenuationStartDistance;
	float soundMaxDistance = 0.0F;
	result = result == FMOD_OK ? soundChannel->get3DMinMaxDistance(&attenuationStartDistance, &soundMaxDistance) : result;

	float attenuatedVolume = (shortestDistance <= attenuationStartDistance) ? 1.0F : attenuationStartDistance / shortestDistance;

	// Lowpass as distance increases
	FMOD::DSP* dsp_multibandeq;
	result = (result == FMOD_OK) ? soundChannel->getDSP(0, &dsp_multibandeq) : result;
	float factor = 1 - pow(1 - attenuatedVolume, 3);
	float lowpassFrequency = 22000.0f * factor;
	lowpassFrequency = std::clamp(lowpassFrequency, 350.0f, 22000.0f);
	result = (result == FMOD_OK) ? dsp_multibandeq->setParameterFloat(1, lowpassFrequency) : result;

	if (sourcePan != 0.0f) {
		result = (result == FMOD_OK) ? soundChannel->setPan(sourcePan) : result;
	}

	float minimumAudibleDistance = 0;
	{
		std::scoped_lock lock(m_SoundChannelMinimumAudibleDistancesMutex);
		if (const auto found = m_SoundChannelMinimumAudibleDistances.find(soundChannelIndex); found != m_SoundChannelMinimumAudibleDistances.end())
			minimumAudibleDistance = found->second;
	}
	if (shortestDistance >= soundMaxDistance || sqrLongestDistance < minimumAudibleDistance * minimumAudibleDistance)
		attenuatedVolume = 0.0F;

	float panLevel;
	result = result == FMOD_OK ? soundChannel->get3DLevel(&panLevel) : result;
	if (result == FMOD_OK && (panLevel < 1.0F || attenuatedVolume == 0.0F)) {
		result = soundChannel->setVolume(attenuatedVolume * sourceVolume);
	}

	result = (result == FMOD_OK && (sceneWraps || positionOverride)) ? soundChannel->set3DAttributes(&channelPosition, nullptr) : result;

	return result;
}

void AudioMan::DetachSoundContainerPlayingChannels(SoundContainer* source) {
	if (!m_AudioEnabled || !source) return;
	std::scoped_lock ownerLock(m_ChannelOwnersMutex);
	const auto* channels = source->GetPlayingChannels();
	const std::vector<int> indices(channels->begin(), channels->end());
	for (int index : indices) {
		FMOD::Channel* channel = nullptr;
		const auto found = m_ChannelOwners.find(index);
		if (m_AudioSystem->getChannel(index, &channel) == FMOD_OK && channel) {
			channel->setUserData(nullptr);
			if (found != m_ChannelOwners.end()) {
				found->second.Container = nullptr;
				found->second.Pan = source->GetCustomPanValue(); found->second.Panning = source->GetPanningStrengthMultiplier(); found->second.Volume = source->GetVolume();
				// One-shot explosions retain their tail after their device disappears.
				// Infinite loops must not remain orphaned indefinitely.
				if (found->second.Infinite) channel->stop();
			}
		} else m_ChannelOwners.erase(index);
		source->RemovePlayingChannel(index);
	}
}

bool AudioMan::VerifySoundContainerLifetime() {
	if (!m_AudioEnabled) return false;
	auto create = [this](bool infinite, int& index) {
		auto sound = std::make_unique<SoundContainer>();
		sound->GetTopLevelSoundSet().AddSound("Base.rte/Sounds/GUIs/SlicePicked.flac", Vector(), 0, -1, false);
		sound->SetImmobile(true); sound->SetPaused(true); sound->SetLoopSetting(infinite ? -1 : 0);
		if (!sound->Play() || sound->GetPlayingChannels()->empty()) return false;
		index = *sound->GetPlayingChannels()->begin();
		sound.reset();
		return true;
	};
	int finiteIndex = -1, loopIndex = -1;
	if (!create(false, finiteIndex)) return false;
	FMOD::Channel* channel = nullptr; bool playing = false; void* data = reinterpret_cast<void*>(1);
	bool passed = m_AudioSystem->getChannel(finiteIndex, &channel) == FMOD_OK && channel &&
	    channel->isPlaying(&playing) == FMOD_OK && playing && channel->getUserData(&data) == FMOD_OK && data == nullptr;
	if (channel) channel->stop();
	passed = create(true, loopIndex) && passed;
	channel = nullptr; playing = false;
	const auto status = m_AudioSystem->getChannel(loopIndex, &channel);
	if (status == FMOD_OK && channel) {
		const auto queried = channel->isPlaying(&playing);
		passed = (queried == FMOD_ERR_INVALID_HANDLE || (queried == FMOD_OK && !playing)) && passed;
	} else passed = status == FMOD_ERR_INVALID_HANDLE && passed;
	m_AudioSystem->update(); DrainFinishedChannels();
	return passed;
}

void AudioMan::DrainFinishedChannels() {
	std::vector<int> finished;
	{ std::scoped_lock lock(m_FinishedChannelsMutex); finished.swap(m_FinishedChannels); }
	std::scoped_lock ownerLock(m_ChannelOwnersMutex);
	for (int index : finished) {
		const auto found = m_ChannelOwners.find(index);
		if (found == m_ChannelOwners.end()) continue;
		FMOD::Channel* channel = nullptr; bool playing = false;
		if (m_AudioSystem->getChannel(index, &channel) == FMOD_OK && channel &&
		    channel->isPlaying(&playing) == FMOD_OK && playing) continue; // Reused channel: ignore an older end callback.
		if (found->second.Container) found->second.Container->RemovePlayingChannel(index);
		m_ChannelOwners.erase(found);
		std::scoped_lock distanceLock(m_SoundChannelMinimumAudibleDistancesMutex);
		m_SoundChannelMinimumAudibleDistances.erase(index);
	}
}

FMOD_RESULT F_CALLBACK AudioMan::SoundChannelEndedCallback(FMOD_CHANNELCONTROL* channelControl, FMOD_CHANNELCONTROL_TYPE channelControlType, FMOD_CHANNELCONTROL_CALLBACK_TYPE callbackType, void*, void*) {
	if (channelControlType == FMOD_CHANNELCONTROL_CHANNEL && callbackType == FMOD_CHANNELCONTROL_CALLBACK_END) {
		auto* channel = reinterpret_cast<FMOD::Channel*>(channelControl);
		int index = 0;
		if (channel->getIndex(&index) == FMOD_OK) {
			// Never dereference a container or mutate its channels on an FMOD thread.
			std::scoped_lock lock(g_AudioMan.m_FinishedChannelsMutex);
			g_AudioMan.m_FinishedChannels.push_back(index);
		}
	}
	return FMOD_OK;
}

FMOD_RESULT AudioMan::SetMusicMuffledState(bool musicMuffledState) {
	FMOD_RESULT status = FMOD_OK;
	if (musicMuffledState != m_MusicMuffled) {
		FMOD::DSP* dsp_multibandeq;
		status = (status == FMOD_OK) ? m_MusicChannelGroup->getDSP(0, &dsp_multibandeq) : status;
		float frequency = 22000.0F;

		if (musicMuffledState) {
			frequency = 1000.0F;
		}

		status = (status == FMOD_OK) ? dsp_multibandeq->setParameterFloat(1, frequency) : status;
		m_MusicMuffled = musicMuffledState;
	}
	return status;
}

FMOD_VECTOR AudioMan::GetAsFMODVector(const Vector& vector, float zValue) const {
	Vector sceneDimensions = g_SceneMan.GetScene() ? g_SceneMan.GetSceneDim() : Vector();
	return sceneDimensions.IsZero() ? FMOD_VECTOR{0, 0, zValue} : FMOD_VECTOR{vector.m_X, sceneDimensions.m_Y - vector.m_Y, zValue};
}

Vector AudioMan::GetAsVector(FMOD_VECTOR fmodVector) const {
	Vector sceneDimensions = g_SceneMan.GetScene() ? g_SceneMan.GetSceneDim() : Vector();
	return sceneDimensions.IsZero() ? Vector() : Vector(fmodVector.x, sceneDimensions.m_Y - fmodVector.y);
}
