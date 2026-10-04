#include "PerformanceMan.h"
#include "MovableMan.h"
#include "FrameMan.h"
#include "AudioMan.h"
#include "MovableObject.h"
#include "SettingsMan.h"
#include "WindowMan.h"

#include "GUI.h"
#include "AllegroBitmap.h"

#include <array>
#include <chrono>
#include <fstream>
#ifdef _WIN32
#include <windows.h>
#include <psapi.h>
#elif defined(__linux__)
#include <unistd.h>
#endif

using namespace RTE;

const std::array<std::string, PerformanceMan::PerformanceCounters::PerfCounterCount> PerformanceMan::m_PerfCounterNames = {"Total", "Input", "Lua VM", "Act AI", "Act Travel", "Act Update", "Prt Travel", "Prt Update", "Activity", "Scripts", "Net Snapshot"};

thread_local std::array<uint64_t, PerformanceMan::PerformanceCounters::PerfCounterCount> s_PerfMeasureStart; //!< Current measurement start time in microseconds.
thread_local std::array<uint64_t, PerformanceMan::PerformanceCounters::PerfCounterCount> s_PerfMeasureStop; //!< Current measurement stop time in microseconds.

namespace {
	std::uint64_t ReadResidentMemoryBytes() {
#ifdef _WIN32
		PROCESS_MEMORY_COUNTERS_EX counters{};
		return GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&counters), sizeof(counters)) ? counters.WorkingSetSize : 0;
#elif defined(__linux__)
		std::ifstream statm("/proc/self/statm");
		std::uint64_t pages = 0;
		std::uint64_t resident = 0;
		const long pageSize = sysconf(_SC_PAGESIZE);
	return (statm >> pages >> resident) && pageSize > 0 ? resident * static_cast<std::uint64_t>(pageSize) : 0;
#else
		return 0;
#endif
	}
}

PerformanceMan::PerformanceMan() {
	Clear();
}

PerformanceMan::~PerformanceMan() {
	Destroy();
}

void PerformanceMan::Clear() {
	m_OverlayLevel = 0;
	m_ConnectedNetworkPlayers = 0;
	m_AdvancedPerfStats = true;
	m_Sample = 0;
	m_SimUpdateTimer = nullptr;
	m_MSPSUs.clear();
	m_MSPSUAverage = 0;
	m_MSPFs.clear();
	m_MSPFAverage = 0;
	m_ActualFrameTimes.clear();
	m_ActualFrameAverage = 0;
	m_MSPUs.clear();
	m_MSPUAverage = 0;
	m_MSPDs.clear();
	m_MSPDAverage = 0;
	m_CurrentPing = 0;
}

void PerformanceMan::Initialize() {
	m_SimUpdateTimer = std::make_unique<Timer>();

	for (int counter = 0; counter < PerformanceCounters::PerfCounterCount; ++counter) {
		for (int i = 0; i < c_MaxSamples; ++i) {
			m_PerfData[counter][i] = 0;
		}
		m_PerfPercentages[counter].fill(0);
	}
}

void PerformanceMan::StartPerformanceMeasurement(PerformanceCounters counter) {
	s_PerfMeasureStart[counter] = g_TimerMan.GetAbsoluteTime();
}

void PerformanceMan::StopPerformanceMeasurement(PerformanceCounters counter) {
	s_PerfMeasureStop[counter] = g_TimerMan.GetAbsoluteTime();
	AddPerformanceSample(counter, s_PerfMeasureStop[counter] - s_PerfMeasureStart[counter]);
}

void PerformanceMan::NewPerformanceSample() {
	m_Sample++;
	if (m_Sample >= c_MaxSamples) {
		m_Sample = 0;
	}

	for (int counter = 0; counter < PerformanceCounters::PerfCounterCount; ++counter) {
		m_PerfData[counter][m_Sample] = 0;
		m_PerfPercentages[counter][m_Sample] = 0;
	}
}

void PerformanceMan::CalculateSamplePercentages() {
	for (int counter = 0; counter < PerformanceCounters::PerfCounterCount; ++counter) {
		const auto total = m_PerfData[PerformanceCounters::SimTotal][m_Sample].load(std::memory_order_relaxed);
		int samplePercentage = total > 0 ? static_cast<int>(static_cast<float>(m_PerfData[counter][m_Sample]) / static_cast<float>(total) * 100) : 0;
		m_PerfPercentages[counter][m_Sample] = samplePercentage;
	}
}

uint64_t PerformanceMan::GetPerformanceCounterAverage(PerformanceCounters counter) const {
	uint64_t totalPerformanceMeasurement = 0;
	int sample = m_Sample;
	for (int i = 0; i < c_Average; ++i) {
		totalPerformanceMeasurement += m_PerfData[counter][sample];
		sample--;
		if (sample < 0) {
			sample = c_MaxSamples - 1;
		}
	}
	return totalPerformanceMeasurement / c_Average;
}

void PerformanceMan::CalculateTimeAverage(std::deque<float>& timeMeasurements, float& avgResult, float newTimeMeasurement) const {
	timeMeasurements.emplace_back(newTimeMeasurement);
	while (timeMeasurements.size() > c_MSPAverageSampleSize) {
		timeMeasurements.pop_front();
	}
	avgResult = 0;
	for (const float& timeMeasurement: timeMeasurements) {
		avgResult += timeMeasurement;
	}
	avgResult /= static_cast<float>(timeMeasurements.size());
}

void PerformanceMan::UpdateMSPF(long long measuredUpdateTime, long long measuredDrawTime, long long actualFrameTime) {
	CalculateTimeAverage(m_MSPUs, m_MSPUAverage, static_cast<float>(measuredUpdateTime) / 1000.0F);
	CalculateTimeAverage(m_MSPDs, m_MSPDAverage, static_cast<float>(measuredDrawTime) / 1000.0F);
	CalculateTimeAverage(m_MSPFs, m_MSPFAverage, static_cast<float>(measuredUpdateTime + measuredDrawTime) / 1000.0F);
	CalculateTimeAverage(m_ActualFrameTimes, m_ActualFrameAverage, static_cast<float>(actualFrameTime) / 1000.0F);
}

void PerformanceMan::Draw(BITMAP* bitmapToDrawTo) {
	if (m_OverlayLevel != 0) {
		AllegroBitmap drawBitmap(bitmapToDrawTo);

		GUIFont* guiFont = g_FrameMan.GetLargeFont(true);
		char str[128];
		static auto lastMemoryRead = std::chrono::steady_clock::time_point{};
		static std::uint64_t residentMemory = 0;
		const auto now = std::chrono::steady_clock::now();
		if (now - lastMemoryRead >= std::chrono::seconds(1)) {
			residentMemory = ReadResidentMemoryBytes();
			lastMemoryRead = now;
		}
		if (m_OverlayLevel == 1) {
			rectfill(bitmapToDrawTo, 8, 8, 285, 66, makecol(12, 16, 24));
			const auto draw = [&](int row, const char* value) { guiFont->DrawAligned(&drawBitmap, c_StatsOffsetX, c_StatsHeight + row * 10, value, GUIFont::Left); };
			draw(0, "PERFORMANCE [F8: details]");
			std::snprintf(str, sizeof(str), "FPS: %.0f | Frame: %.1f ms", m_ActualFrameAverage > 0 ? 1000.0F / m_ActualFrameAverage : 0.0F, m_ActualFrameAverage);
			draw(1, str);
			std::snprintf(str, sizeof(str), "Simulation: %.1f ms | Render: %.1f ms", m_MSPUAverage, m_MSPDAverage);
			draw(2, str);
			std::snprintf(str, sizeof(str), "Actors: %li | MovableObjects: %li", g_MovableMan.GetActorCount(), g_MovableMan.GetMovableObjectCount());
			draw(3, str);
			std::snprintf(str, sizeof(str), "RAM: %.0f MiB", static_cast<double>(residentMemory) / (1024.0 * 1024.0));
			draw(4, str);
			return;
		}

		const float fps = m_ActualFrameAverage > 0 ? 1000.0F / m_ActualFrameAverage : 0.0F;
		static auto lastSceneRead = std::chrono::steady_clock::time_point{};
		static MovableMan::SceneStats sceneStats;
		static std::uint64_t previousSpawns = 0;
		static std::uint64_t previousDeletes = 0;
		static std::uint64_t spawnRate = 0;
		static std::uint64_t deleteRate = 0;
		if (now - lastSceneRead >= std::chrono::seconds(1)) {
			sceneStats = g_MovableMan.CollectSceneStats();
			const std::uint64_t spawns = MovableObject::GetSceneSpawnEvents();
			const std::uint64_t deletes = MovableObject::GetSceneDeleteEvents();
			const double seconds = lastSceneRead == std::chrono::steady_clock::time_point{} ? 1.0 : std::chrono::duration<double>(now - lastSceneRead).count();
			spawnRate = static_cast<std::uint64_t>((spawns - previousSpawns) / seconds);
			deleteRate = static_cast<std::uint64_t>((deletes - previousDeletes) / seconds);
			previousSpawns = spawns;
			previousDeletes = deletes;
			lastSceneRead = now;
		}
		rectfill(bitmapToDrawTo, 8, 8, 625, 172, makecol(12, 16, 24));
		const auto drawSummary = [&](int row, const char* value) { guiFont->DrawAligned(&drawBitmap, c_StatsOffsetX, c_StatsHeight + row * 10, value, GUIFont::Left); };
		drawSummary(0, "PERFORMANCE [F8: off]");
		std::snprintf(str, sizeof(str), "FPS %.0f | Frame %.1f ms", fps, m_ActualFrameAverage);
		drawSummary(1, str);
		std::snprintf(str, sizeof(str), "Simulation %.1f | Render %.1f ms", m_MSPUAverage, m_MSPDAverage);
		drawSummary(2, str);
		std::snprintf(str, sizeof(str), "Input/upd %.2f | Lua/upd %.2f ms", GetPerformanceCounterAverage(InputUpdate) / 1000.0, (GetPerformanceCounterAverage(LuaManagerUpdate) + GetPerformanceCounterAverage(ScriptsUpdate)) / 1000.0);
		drawSummary(3, str);
		std::snprintf(str, sizeof(str), "Physics/upd %.2f | AI/upd %.2f ms", (GetPerformanceCounterAverage(ActorsTravel) + GetPerformanceCounterAverage(ParticlesTravel)) / 1000.0, GetPerformanceCounterAverage(ActorsAI) / 1000.0);
		drawSummary(4, str);
		std::snprintf(str, sizeof(str), "Actors %li | MOs %li | Items %li", g_MovableMan.GetActorCount(), g_MovableMan.GetMovableObjectCount(), g_MovableMan.GetItemCount());
		drawSummary(5, str);
		std::snprintf(str, sizeof(str), "RAM %.0f MiB | Players %d", static_cast<double>(residentMemory) / (1024.0 * 1024.0), m_ConnectedNetworkPlayers);
		drawSummary(6, str);
		std::snprintf(str, sizeof(str), "%dx%d | VSync %s | Cap %d", g_WindowMan.GetResX(), g_WindowMan.GetResY(), g_WindowMan.GetVSyncEnabled() ? "on" : "off", g_SettingsMan.GetFPSLimit());
		drawSummary(7, str);
		int luaCallbackCount = 0;
		for (const auto& [script, timing] : m_SortedScriptTimings) luaCallbackCount += timing.m_CallCount;
		std::snprintf(str, sizeof(str), "Tracked Lua calls/update %d", luaCallbackCount);
		drawSummary(8, str);
		std::snprintf(str, sizeof(str), "Network snapshot capture %.2f ms", GetPerformanceCounterAverage(WorldStateSnapshot) / 1000.0);
		drawSummary(9, str);
		const int detailsX = 310;
		const auto drawDetails = [&](int row, const char* value) { guiFont->DrawAligned(&drawBitmap, detailsX, c_StatsHeight + row * 10, value, GUIFont::Left); };
		drawDetails(0, "SCENE (top-level objects)");
		std::snprintf(str, sizeof(str), "MO %li | Items %li | MOIDs %i", g_MovableMan.GetMovableObjectCount(), g_MovableMan.GetItemCount(), g_MovableMan.GetMOIDCount());
		drawDetails(1, str);
		std::snprintf(str, sizeof(str), "Actors %zu | MOSRotating %zu", sceneStats.Actors, sceneStats.MOSRotating);
		drawDetails(2, str);
		std::snprintf(str, sizeof(str), "MOSParticle %zu | MOPixel %zu", sceneStats.MOSParticles, sceneStats.MOPixels);
		drawDetails(3, str);
		std::snprintf(str, sizeof(str), "Gibs %zu | Projectile-like %zu", sceneStats.Gibs, sceneStats.ProjectileLike);
		drawDetails(4, str);
		std::snprintf(str, sizeof(str), "Collision-enabled %zu | Particles %li", sceneStats.CollisionEnabled, g_MovableMan.GetParticleCount());
		drawDetails(5, str);
		std::snprintf(str, sizeof(str), "Spawn/s %llu | Delete/s %llu", static_cast<unsigned long long>(spawnRate), static_cast<unsigned long long>(deleteRate));
		drawDetails(6, str);
		if (m_AdvancedPerfStats) DrawPeformanceGraphs(drawBitmap);
	}
}

void PerformanceMan::DrawPeformanceGraphs(AllegroBitmap& bitmapToDrawTo) {
	CalculateSamplePercentages();

	GUIFont* guiFont = g_FrameMan.GetLargeFont(true);
	char str[128];

	for (int pc = 0; pc < PerformanceCounters::PerfCounterCount; ++pc) {
		int blockStart = c_GraphsStartOffsetY + pc * c_GraphBlockHeight;

		guiFont->DrawAligned(&bitmapToDrawTo, c_StatsOffsetX, blockStart, m_PerfCounterNames[pc], GUIFont::Left);

		const auto total = GetPerformanceCounterAverage(PerformanceCounters::SimTotal);
		int perc = total > 0 ? static_cast<int>((static_cast<float>(GetPerformanceCounterAverage(static_cast<PerformanceCounters>(pc))) / static_cast<float>(total) * 100)) : 0;
		std::snprintf(str, sizeof(str), "%%: %d", perc);
		guiFont->DrawAligned(&bitmapToDrawTo, c_StatsOffsetX + 60, blockStart, str, GUIFont::Left);

		// Print average processing time in milliseconds.
		std::snprintf(str, sizeof(str), "T: %llu", GetPerformanceCounterAverage(static_cast<PerformanceCounters>(pc)) / 1000);
		guiFont->DrawAligned(&bitmapToDrawTo, c_StatsOffsetX + 96, blockStart, str, GUIFont::Left);

		int graphStart = blockStart + c_GraphsOffsetX;

		// Draw graph backgrounds.
		bitmapToDrawTo.DrawRectangle(c_StatsOffsetX, graphStart, c_MaxSamples, c_GraphHeight, makecol32(96, 19, 32), true); // Palette index 240.
		bitmapToDrawTo.DrawLine(c_StatsOffsetX, graphStart + c_GraphHeight / 2, c_StatsOffsetX - 1 + c_MaxSamples, graphStart + c_GraphHeight / 2, makecol32(200, 206, 140)); // Palette index 96.

		// Draw sample dots.
		int peak = 0;
		int sample = m_Sample;
		for (int i = 0; i < c_MaxSamples; ++i) {
			// Show microseconds in graphs, assume that the RealToSimCap is the highest value on the graph. The graph will scale with the RealToSimCap if it is changed.
			int value = std::clamp(static_cast<int>(static_cast<float>(m_PerfData[pc][sample]) / (g_TimerMan.GetRealToSimCap() * 1000000.0F) * 100.0F), 0, 100);
			int dotHeight = static_cast<int>(static_cast<float>(c_GraphHeight) / 100.0F * static_cast<float>(value));

			bitmapToDrawTo.SetPixel(c_StatsOffsetX - 1 + c_MaxSamples - i, graphStart + c_GraphHeight - dotHeight, makecol32(234, 21, 7)); // Palette index 13.

			if (peak < m_PerfData[pc][sample]) {
				peak = static_cast<int>(m_PerfData[pc][sample]);
			}

			if (sample == 0) {
				sample = c_MaxSamples;
			}
			sample--;
		}

		// Print peak values
		guiFont->DrawAligned(&bitmapToDrawTo, c_StatsOffsetX + 130, blockStart, "Peak: " + std::to_string(peak / 1000), GUIFont::Left);
	}
}

void PerformanceMan::DrawCurrentPing() const {
	AllegroBitmap allegroBitmap(g_FrameMan.GetBackBuffer8());
	g_FrameMan.GetLargeFont()->DrawAligned(&allegroBitmap, g_FrameMan.GetBackBuffer8()->w - 25, g_FrameMan.GetBackBuffer8()->h - 14, "PING: " + std::to_string(m_CurrentPing), GUIFont::Right);
}

void PerformanceMan::UpdateSortedScriptTimings(const std::unordered_map<std::string, ScriptTiming>& scriptTimings) {
	std::vector<std::pair<std::string, ScriptTiming>> sortedScriptTimings;
	for (auto it = scriptTimings.begin(); it != scriptTimings.end(); it++) {
		sortedScriptTimings.push_back(*it);
	}

	std::sort(sortedScriptTimings.begin(), sortedScriptTimings.end(), [](const auto& l, const auto& r) { return l.second.m_Time > r.second.m_Time; });

	g_PerformanceMan.m_SortedScriptTimings = sortedScriptTimings;
}
