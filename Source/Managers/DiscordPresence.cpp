#include "DiscordPresence.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

namespace RTE {
namespace {
	constexpr char c_DiscordApplicationID[] = "1555721601100685342";

	std::string EscapeJSON(const std::string& value) {
		std::string escaped;
		escaped.reserve(value.size() + 8);
		for (unsigned char character : value) {
			switch (character) {
				case '"': escaped += "\\\""; break;
				case '\\': escaped += "\\\\"; break;
				case '\b': escaped += "\\b"; break;
				case '\f': escaped += "\\f"; break;
				case '\n': escaped += "\\n"; break;
				case '\r': escaped += "\\r"; break;
				case '\t': escaped += "\\t"; break;
				default:
					if (character < 0x20) {
						constexpr char hex[] = "0123456789abcdef";
						escaped += "\\u00";
						escaped += hex[(character >> 4) & 0xF];
						escaped += hex[character & 0xF];
					} else {
						escaped += static_cast<char>(character);
					}
			}
		}
		return escaped;
	}

#ifdef _WIN32
	void AppendUint32LE(std::vector<char>& output, uint32_t value) {
		output.push_back(static_cast<char>(value & 0xff));
		output.push_back(static_cast<char>((value >> 8) & 0xff));
		output.push_back(static_cast<char>((value >> 16) & 0xff));
		output.push_back(static_cast<char>((value >> 24) & 0xff));
	}

	bool WritePacket(HANDLE pipe, uint32_t opcode, const std::string& payload) {
		std::vector<char> packet;
		packet.reserve(payload.size() + 8);
		AppendUint32LE(packet, opcode);
		AppendUint32LE(packet, static_cast<uint32_t>(payload.size()));
		packet.insert(packet.end(), payload.begin(), payload.end());
		DWORD bytesWritten = 0;
		return WriteFile(pipe, packet.data(), static_cast<DWORD>(packet.size()), &bytesWritten, nullptr) && bytesWritten == packet.size();
	}

	bool ReadPacket(HANDLE pipe, std::string& payload, DWORD timeoutMilliseconds) {
		std::vector<char> received;
		ULONGLONG deadline = GetTickCount64() + timeoutMilliseconds;
		while (GetTickCount64() < deadline) {
			char buffer[4096];
			DWORD bytesRead = 0;
			if (ReadFile(pipe, buffer, sizeof(buffer), &bytesRead, nullptr) && bytesRead > 0) {
				received.insert(received.end(), buffer, buffer + bytesRead);
				if (received.size() >= 8) {
					uint32_t length = static_cast<unsigned char>(received[4]) |
						(static_cast<uint32_t>(static_cast<unsigned char>(received[5])) << 8) |
						(static_cast<uint32_t>(static_cast<unsigned char>(received[6])) << 16) |
						(static_cast<uint32_t>(static_cast<unsigned char>(received[7])) << 24);
					if (received.size() >= static_cast<size_t>(length) + 8) {
						payload.assign(received.data() + 8, length);
						return true;
					}
				}
			} else {
				const DWORD error = GetLastError();
				if (error != ERROR_NO_DATA && error != ERROR_PIPE_LISTENING) {
					return false;
				}
				Sleep(10);
			}
		}
		return false;
	}

	HANDLE ConnectToDiscord() {
		for (int index = 0; index < 10; ++index) {
			const std::string pipeName = "\\\\?\\pipe\\discord-ipc-" + std::to_string(index);
			HANDLE pipe = CreateFileA(pipeName.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
			if (pipe == INVALID_HANDLE_VALUE) {
				continue;
			}
			DWORD mode = PIPE_READMODE_BYTE | PIPE_NOWAIT;
			if (!SetNamedPipeHandleState(pipe, &mode, nullptr, nullptr)) {
				CloseHandle(pipe);
				continue;
			}
			return pipe;
		}
		return INVALID_HANDLE_VALUE;
	}

	class PresenceWorker {
	public:
		void Start() {
			std::lock_guard<std::mutex> lock(m_Mutex);
			if (m_Running) return;
			m_Stop = false;
			m_Running = true;
			m_Thread = std::thread(&PresenceWorker::Run, this);
		}

		void SetEnabled(bool enabled) {
			{
				std::lock_guard<std::mutex> lock(m_Mutex);
				m_Enabled = enabled;
				m_Changed = true;
			}
			m_Condition.notify_all();
		}

		void SetActivity(const std::string& details, const std::string& state) {
			{
				std::lock_guard<std::mutex> lock(m_Mutex);
				if (m_Details == details && m_State == state) return;
				m_Details = details;
				m_State = state;
				m_Changed = true;
			}
			m_Condition.notify_all();
		}

		void Stop() {
			{
				std::lock_guard<std::mutex> lock(m_Mutex);
				if (!m_Running) return;
				m_Stop = true;
			}
			m_Condition.notify_all();
			if (m_Thread.joinable()) m_Thread.join();
			std::lock_guard<std::mutex> lock(m_Mutex);
			m_Running = false;
		}

	private:
		void Run() {
			HANDLE pipe = INVALID_HANDLE_VALUE;
			std::string details;
			std::string state;
			bool wasEnabled = false;
			while (true) {
				bool enabled;
				bool changed;
				{
					std::unique_lock<std::mutex> lock(m_Mutex);
					m_Condition.wait_for(lock, std::chrono::milliseconds(100), [this] { return m_Stop || m_Changed; });
					if (m_Stop) break;
					enabled = m_Enabled;
					changed = m_Changed;
					m_Changed = false;
					details = m_Details;
					state = m_State;
				}

				if (!enabled) {
					if (pipe != INVALID_HANDLE_VALUE) {
						WritePacket(pipe, 1, std::string("{\"cmd\":\"SET_ACTIVITY\",\"args\":{\"pid\":") + std::to_string(GetCurrentProcessId()) + ",\"activity\":null},\"nonce\":\"ccr-clear\"}");
						CloseHandle(pipe);
						pipe = INVALID_HANDLE_VALUE;
					}
					wasEnabled = false;
					continue;
				}

				if (pipe == INVALID_HANDLE_VALUE) {
					pipe = ConnectToDiscord();
					if (pipe == INVALID_HANDLE_VALUE) continue;
					const std::string handshake = std::string("{\"v\":1,\"client_id\":\"") + c_DiscordApplicationID + "\"}";
					std::string response;
					const bool handshakeSent = WritePacket(pipe, 0, handshake);
					const bool handshakeAnswered = handshakeSent && ReadPacket(pipe, response, 2000);
					const bool handshakeAccepted = handshakeAnswered && !response.empty() &&
						response.find("\"cmd\":\"ERROR\"") == std::string::npos &&
						(response.find("\"cmd\":\"DISPATCH\"") != std::string::npos || response.find("READY") != std::string::npos);
					if (!handshakeAccepted) {
						CloseHandle(pipe);
						pipe = INVALID_HANDLE_VALUE;
						continue;
					}
					changed = true;
				}

				if (changed || !wasEnabled) {
					std::ostringstream request;
					request << "{\"cmd\":\"SET_ACTIVITY\",\"args\":{\"pid\":" << GetCurrentProcessId()
						<< ",\"activity\":{\"details\":\"" << EscapeJSON(details)
						<< "\",\"state\":\"" << EscapeJSON(state)
						<< "\",\"instance\":false}},\"nonce\":\"ccr-presence\"}";
					if (!WritePacket(pipe, 1, request.str())) {
						CloseHandle(pipe);
						pipe = INVALID_HANDLE_VALUE;
						continue;
					}
					wasEnabled = true;
				}

				// Drain Discord replies so its local IPC pipe cannot fill up over long sessions.
				std::string ignored;
				ReadPacket(pipe, ignored, 10);
				if (pipe != INVALID_HANDLE_VALUE && !PeekNamedPipe(pipe, nullptr, 0, nullptr, nullptr, nullptr)) {
					CloseHandle(pipe);
					pipe = INVALID_HANDLE_VALUE;
					wasEnabled = false;
				}
			}
			if (pipe != INVALID_HANDLE_VALUE) {
				WritePacket(pipe, 1, std::string("{\"cmd\":\"SET_ACTIVITY\",\"args\":{\"pid\":") + std::to_string(GetCurrentProcessId()) + ",\"activity\":null},\"nonce\":\"ccr-clear\"}");
				CloseHandle(pipe);
			}
		}

		std::mutex m_Mutex;
		std::condition_variable m_Condition;
		std::thread m_Thread;
		bool m_Running = false;
		bool m_Stop = false;
		bool m_Enabled = true;
		bool m_Changed = true;
		std::string m_Details = "At the main menu";
		std::string m_State = "Cortex Command Reload";
	};

	PresenceWorker g_PresenceWorker;
#endif
} // namespace

void DiscordPresence::Initialize() {
#ifdef _WIN32
	g_PresenceWorker.Start();
#endif
}

void DiscordPresence::SetEnabled(bool enabled) {
#ifdef _WIN32
	g_PresenceWorker.SetEnabled(enabled);
#else
	(void)enabled;
#endif
}

void DiscordPresence::SetActivity(const std::string& details, const std::string& state) {
#ifdef _WIN32
	g_PresenceWorker.SetActivity(details, state);
#else
	(void)details;
	(void)state;
#endif
}

void DiscordPresence::Shutdown() {
#ifdef _WIN32
	g_PresenceWorker.Stop();
#endif
}

} // namespace RTE
