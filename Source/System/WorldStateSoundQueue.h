#pragma once

#include "WorldStateSoundProtocol.h"
#include <deque>
#include <utility>

namespace RTE {

/// Single-threaded receive queue. Overflow must end the session rather than lose
/// lifecycle commands such as Stop and leave a voice playing indefinitely.
class WorldStateSoundQueue {
public:
	static constexpr std::size_t MaxBatches = 32;
	static constexpr std::size_t MaxQueuedEvents = 2048;

	bool Push(std::uint32_t revision, std::uint32_t sequence, std::vector<WorldStateSoundProtocol::Event> events) {
		if (!revision || !sequence || events.size() > WorldStateSoundProtocol::MaxEvents) return false;
		if (m_LastSequence && static_cast<std::int32_t>(sequence - m_LastSequence) <= 0) return true;
		if (!events.empty() && (m_Batches.size() >= MaxBatches || events.size() > MaxQueuedEvents - m_Count)) return false;
		m_LastSequence = sequence;
		if (!events.empty()) {
			m_Count += events.size();
			m_Batches.push_back({revision, std::move(events)});
		}
		return true;
	}

	std::vector<WorldStateSoundProtocol::Event> Drain(std::uint32_t revision) {
		std::vector<WorldStateSoundProtocol::Event> result;
		const auto batches = m_Batches.size();
		for (std::size_t index = 0; index < batches; ++index) {
			auto batch = std::move(m_Batches.front());
			m_Batches.pop_front();
			if (static_cast<std::int32_t>(revision - batch.Revision) < 0) {
				m_Batches.push_back(std::move(batch));
				continue;
			}
			m_Count -= batch.Events.size();
			if (batch.Revision == revision)
				for (auto& event : batch.Events) result.push_back(std::move(event));
		}
		return result;
	}

	void Clear() { m_Batches.clear(); m_Count = 0; m_LastSequence = 0; }
	std::size_t Size() const { return m_Count; }

private:
	struct Batch { std::uint32_t Revision; std::vector<WorldStateSoundProtocol::Event> Events; };
	std::deque<Batch> m_Batches;
	std::size_t m_Count = 0;
	std::uint32_t m_LastSequence = 0;
};

} // namespace RTE
