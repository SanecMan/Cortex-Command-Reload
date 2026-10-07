#pragma once

#include "UTF8.h"
#include <charconv>
#include <fstream>
#include <string>
#include <string_view>

namespace RTE {

// Independent of client Settings.ini and of GUI/renderer initialization.
struct ServerConfiguration {
	std::string ListenAddress = "0.0.0.0";
	unsigned short Port = 8000;
	unsigned short Players = 8;
	std::string Name = "Cortex Command Reload";
	std::string ActivityClass = "GATutorial";
	std::string ActivityPreset = "Tutorial Mission";
	std::string ScenePreset = "Tutorial Bunker";
	bool IncludeMods = false;
	std::string LogDirectory = "ServerLogs";

	static std::string_view Trim(std::string_view text) {
		const auto first = text.find_first_not_of(" \t\r\n");
		if (first == std::string_view::npos) return {};
		return text.substr(first, text.find_last_not_of(" \t\r\n") - first + 1);
	}

	bool Set(std::string_view key, std::string_view value, std::string& error) {
		value = Trim(value);
		if (value.size() >= 2 && value.front() == '"' && value.back() == '"') value = value.substr(1, value.size() - 2);
		if (value.empty() || value.size() > 1024 || !UTF8::IsValid(value) || value.find_first_of("\r\n") != std::string_view::npos) {
			error = "Invalid UTF-8 or empty/oversized server value for " + std::string(key);
			return false;
		}
		if (key == "Port" || key == "Players") {
			unsigned int number = 0;
			const auto [end, result] = std::from_chars(value.data(), value.data() + value.size(), number);
			const unsigned int maximum = key == "Port" ? 65535 : 64;
			const unsigned int minimum = key == "Port" ? 0 : 1;
			if (result != std::errc{} || end != value.data() + value.size() || number < minimum || number > maximum) {
				error = "Out-of-range server value for " + std::string(key);
				return false;
			}
			(key == "Port" ? Port : Players) = static_cast<unsigned short>(number);
		} else if (key == "ListenAddress") ListenAddress = value;
		else if (key == "Name") Name = value;
		else if (key == "ActivityClass") ActivityClass = value;
		else if (key == "Activity") ActivityPreset = value;
		else if (key == "Scene") ScenePreset = value;
		else if (key == "LogDirectory") LogDirectory = value;
		else if (key == "IncludeMods") {
			if (value != "true" && value != "false" && value != "1" && value != "0") {
				error = "IncludeMods must be true or false";
				return false;
			}
			IncludeMods = value == "true" || value == "1";
		} else {
			error = "Unknown server configuration key: " + std::string(key);
			return false;
		}
		return true;
	}

	bool Load(const std::string& path, std::string& error) {
		std::ifstream input(UTF8::PathFromString(path), std::ios::binary);
		if (!input) { error = "Cannot open server config: " + path; return false; }
		UTF8::SkipByteOrderMark(input);
		ServerConfiguration candidate = *this;
		std::string line;
		int lineNumber = 0;
		while (std::getline(input, line)) {
			++lineNumber;
			const auto text = Trim(line);
			if (text.empty() || text.front() == '#' || text.front() == ';' || text == "[Server]") continue;
			const auto separator = text.find('=');
			if (separator == std::string_view::npos || !candidate.Set(Trim(text.substr(0, separator)), text.substr(separator + 1), error)) {
				if (separator == std::string_view::npos) error = "Expected Key = Value";
				error = path + ":" + std::to_string(lineNumber) + ": " + error;
				return false;
			}
		}
		if (input.bad()) { error = "Cannot read server config: " + path; return false; }
		*this = std::move(candidate);
		return true;
	}

	bool ParseArguments(int argc, char** argv, std::string& error) {
		// Config is applied first, so explicit CLI values always override it.
		ServerConfiguration candidate = *this;
		for (int i = 1; i < argc; ++i) {
			if (std::string_view(argv[i]) == "--config") {
				if (i + 1 >= argc) { error = "--config requires a path"; return false; }
				if (!candidate.Load(argv[++i], error)) return false;
			}
		}
		for (int i = 1; i < argc; ++i) {
			const std::string_view option(argv[i]);
			if (option == "--config") { ++i; continue; }
			std::string_view key;
			if (option == "--listen") key = "ListenAddress";
			else if (option == "--port") key = "Port";
			else if (option == "--players") key = "Players";
			else if (option == "--name") key = "Name";
			else if (option == "--activity-class") key = "ActivityClass";
			else if (option == "--activity") key = "Activity";
			else if (option == "--scene") key = "Scene";
			else if (option == "--mods") key = "IncludeMods";
			else if (option == "--log-directory") key = "LogDirectory";
			else if (option == "--server") continue;
			else if (option.starts_with("--")) { error = "Unknown server option: " + std::string(option); return false; }
			else continue; // Existing single-dash diagnostic options belong to Main.
			if (i + 1 >= argc) { error = std::string(option) + " requires a value"; return false; }
			if (!candidate.Set(key, argv[++i], error)) return false;
		}
		*this = std::move(candidate);
		return true;
	}
};

} // namespace RTE
