#pragma once
#include <atomic>
#include <cstddef>
#include <utility>
#include <csignal>
#include <string>
#include <vector>
#ifdef _WIN32
#include <Windows.h>
#ifdef GetClassName
#undef GetClassName
#endif
#else
#include <poll.h>
#include <unistd.h>
#endif

namespace RTE {
class ServerConsole {
public:
	inline static std::atomic<bool> StopRequested{false};
	ServerConsole() {
		StopRequested.store(false);
#ifdef _WIN32
		SetConsoleCtrlHandler(HandleControl, TRUE);
#else
		m_OldInterrupt = std::signal(SIGINT, HandleSignal);
		m_OldTerminate = std::signal(SIGTERM, HandleSignal);
#endif
	}
	~ServerConsole() {
#ifdef _WIN32
		SetConsoleCtrlHandler(HandleControl, FALSE);
#else
		std::signal(SIGINT, m_OldInterrupt);
		std::signal(SIGTERM, m_OldTerminate);
#endif
	}
	std::vector<std::string> Poll() {
		std::vector<std::string> commands;
#ifdef _WIN32
		HANDLE input = GetStdHandle(STD_INPUT_HANDLE);
		if (input == INVALID_HANDLE_VALUE || !input) return commands;
		if (GetFileType(input) == FILE_TYPE_CHAR) {
			DWORD count = 0;
			while (GetNumberOfConsoleInputEvents(input, &count) && count > 0) {
				INPUT_RECORD record{};
				DWORD read = 0;
				if (!ReadConsoleInputW(input, &record, 1, &read) || !read) break;
				if (record.EventType != KEY_EVENT || !record.Event.KeyEvent.bKeyDown) continue;
				const wchar_t character = record.Event.KeyEvent.uChar.UnicodeChar;
				if (character > 0 && character < 128) Append(static_cast<char>(character), commands);
			}
		} else if (GetFileType(input) == FILE_TYPE_PIPE) {
			DWORD available = 0;
			if (PeekNamedPipe(input, nullptr, 0, nullptr, &available, nullptr) && available > 0) {
				char buffer[256];
				DWORD read = 0;
				if (ReadFile(input, buffer, available < sizeof(buffer) ? available : sizeof(buffer), &read, nullptr)) {
					for (DWORD i = 0; i < read; ++i) Append(buffer[i], commands);
				}
			}
		}
#else
		pollfd input{STDIN_FILENO, POLLIN, 0};
		if (::poll(&input, 1, 0) > 0 && (input.revents & POLLIN)) {
			char buffer[256];
			const auto count = ::read(STDIN_FILENO, buffer, sizeof(buffer));
			for (std::ptrdiff_t i = 0; i < count; ++i) Append(buffer[i], commands);
		}
#endif
		return commands;
	}
private:
	std::string m_Pending;
	void Append(char character, std::vector<std::string>& commands) {
		if (character == '\r' || character == '\n') {
			if (!m_Pending.empty()) commands.push_back(std::move(m_Pending));
			m_Pending.clear();
		} else if (character == '\b' || character == 127) {
			if (!m_Pending.empty()) m_Pending.pop_back();
		} else if (m_Pending.size() < 256) m_Pending.push_back(character);
	}
#ifdef _WIN32
	static BOOL WINAPI HandleControl(DWORD event) {
		if (event == CTRL_C_EVENT || event == CTRL_BREAK_EVENT || event == CTRL_CLOSE_EVENT || event == CTRL_SHUTDOWN_EVENT) {
			StopRequested.store(true);
			return TRUE;
		}
		return FALSE;
	}
#else
	static_assert(std::atomic<bool>::is_always_lock_free);
	static void HandleSignal(int) { StopRequested.store(true); }
	using SignalHandler = void (*)(int);
	SignalHandler m_OldInterrupt;
	SignalHandler m_OldTerminate;
#endif
};
} // namespace RTE
