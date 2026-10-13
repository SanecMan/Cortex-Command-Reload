#pragma once

#include "WorldStateSoundProtocol.h"
#include <atomic>
#include <array>
#include <deque>
#include <mutex>
#include <unordered_map>

namespace RTE {

/// Captures simulation decisions, never pointers or FMOD handles on the wire.
/// Source addresses are local lookup keys only and are released on container reset.
class WorldStateSoundCapture {
public:
	static constexpr std::size_t MaxPending = 8192;
	void Begin() { std::scoped_lock lock(m_Mutex); Reset(); m_Enabled = true; }
	void End() { std::scoped_lock lock(m_Mutex); m_Enabled = false; Reset(); }
	bool IsEnabled() const { return m_Enabled.load(std::memory_order_relaxed); }
	void Play(const void* source, WorldStateSoundProtocol::Event event) {
		if (!IsEnabled() || !source) return;
		std::scoped_lock lock(m_Mutex);
		if (!m_Enabled) return;
		event.Type = WorldStateSoundProtocol::Action::Play;
		event.Container = 1; event.Voice = 1;
		if (!WorldStateSoundProtocol::Detail::Valid(event)) return;
		auto& state = m_Containers[source];
		if (!state.ID) state.ID = NextID();
		state.Looping = state.Looping || event.Loops == -1;
		event.Container = state.ID; event.Voice = NextID();
		Append(std::move(event));
	}
	void Command(const void* source, WorldStateSoundProtocol::Event event) {
		if (!IsEnabled()) return;
		std::scoped_lock lock(m_Mutex);
		if (!m_Enabled) return;
		const auto found = m_Containers.find(source);
		if (found == m_Containers.end() || event.Type == WorldStateSoundProtocol::Action::Play) return;
		event.Container = found->second.ID;
		if (WorldStateSoundProtocol::Detail::Valid(event)) Append(std::move(event));
	}
	void Global(WorldStateSoundProtocol::Event event) {
		if (!IsEnabled() || !WorldStateSoundProtocol::Detail::Valid(event)) return;
		std::scoped_lock lock(m_Mutex);
		if (!m_Enabled) return;
		if (event.Type == WorldStateSoundProtocol::Action::GlobalPitch) {
			std::uint8_t changed = 0;
			for (std::size_t bus = 0; bus < m_Pitch.size(); ++bus) if ((event.Bus & (1U << bus)) && m_Pitch[bus] != event.Rate) {
				m_Pitch[bus] = event.Rate; changed |= static_cast<std::uint8_t>(1U << bus);
			}
			if (!changed) return;
			event.Bus = changed;
		} else if (event.Type == WorldStateSoundProtocol::Action::BusPause) {
			if (m_BusPaused[event.Bus] == event.Paused) return;
			m_BusPaused[event.Bus] = event.Paused;
		} else return;
		Append(std::move(event));
	}
	std::vector<WorldStateSoundProtocol::Event> GetMixState() {
		std::scoped_lock lock(m_Mutex);
		std::vector<WorldStateSoundProtocol::Event> result;
		for (std::size_t bus = 0; bus < 3; ++bus) {
			WorldStateSoundProtocol::Event pitch; pitch.Type = WorldStateSoundProtocol::Action::GlobalPitch;
			pitch.Bus = static_cast<std::uint8_t>(1U << bus); pitch.Rate = m_Pitch[bus]; result.push_back(pitch);
			WorldStateSoundProtocol::Event pause; pause.Type = WorldStateSoundProtocol::Action::BusPause;
			pause.Bus = static_cast<std::uint8_t>(bus); pause.Paused = m_BusPaused[bus]; result.push_back(pause);
		}
		return result;
	}
	void StopAll() {
		if (!IsEnabled()) return;
		std::scoped_lock lock(m_Mutex);
		if (!m_Enabled) return;
		for (const auto& [source, id] : m_Containers) {
			WorldStateSoundProtocol::Event stop; stop.Container = id.ID; Append(std::move(stop));
		}
		m_Containers.clear();
	}
	void Release(const void* source) {
		if (!IsEnabled()) return;
		std::scoped_lock lock(m_Mutex);
		if (!m_Enabled) return;
		if (const auto found = m_Containers.find(source); found != m_Containers.end()) {
			WorldStateSoundProtocol::Event stop; stop.Container = found->second.ID;
			if (found->second.Looping) Append(std::move(stop));
			m_Containers.erase(found);
		}
	}
	std::vector<WorldStateSoundProtocol::Event> Drain(bool& overflow) {
		std::scoped_lock lock(m_Mutex);
		overflow = m_Overflow;
		std::vector<WorldStateSoundProtocol::Event> result;
		result.reserve(std::min(m_Events.size(), std::size_t{100}));
		while (!m_Events.empty() && result.size() < std::size_t{100}) {
			result.push_back(std::move(m_Events.front())); m_Events.pop_front();
		}
		return result;
	}
private:
	void Reset() { m_Events.clear(); m_Containers.clear(); m_NextID = 0; m_Overflow = false; m_Pitch.fill(1); m_BusPaused.fill(false); }
	std::uint64_t NextID() { if (++m_NextID == 0) ++m_NextID; return m_NextID; }
	void Append(WorldStateSoundProtocol::Event event) {
		if (m_Events.size() >= MaxPending) { m_Overflow = true; return; }
		m_Events.push_back(std::move(event));
	}
	std::array<float, 3> m_Pitch{1,1,1};
	std::array<bool, 3> m_BusPaused{};
	std::atomic<bool> m_Enabled{false};
	std::mutex m_Mutex;
	struct ContainerState { std::uint64_t ID = 0; bool Looping = false; };
	std::unordered_map<const void*, ContainerState> m_Containers;
	std::deque<WorldStateSoundProtocol::Event> m_Events;
	std::uint64_t m_NextID = 0;
	bool m_Overflow = false;
};

inline WorldStateSoundCapture& GetWorldStateSoundCapture() {
	// Process-lifetime service: SoundContainer static destructors may run after
	// normal manager teardown. End() releases all session allocations explicitly.
	static auto* capture = new WorldStateSoundCapture;
	return *capture;
}

} // namespace RTE
