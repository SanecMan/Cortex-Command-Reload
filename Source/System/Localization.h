#pragma once

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <unordered_map>

namespace RTE {

/// Lightweight UI text localization. The legacy bitmap fonts address glyphs by
/// single-byte character index, so translated UTF-8 is converted to Windows-1251
/// at the final rendering boundary while English remains byte-for-byte intact.
class Localization {
public:
	static void Initialize(const std::filesystem::path& russianCatalog, std::string_view language) {
		Translations().clear();
		std::ifstream input(russianCatalog, std::ios::binary);
		std::string line;
		while (std::getline(input, line)) {
			if (line.size() >= 3 && static_cast<unsigned char>(line[0]) == 0xEF && static_cast<unsigned char>(line[1]) == 0xBB && static_cast<unsigned char>(line[2]) == 0xBF) {
				line.erase(0, 3);
			}
			const size_t separator = line.find('\t');
			if (separator == std::string::npos || separator == 0) {
				continue;
			}
			Translations().insert_or_assign(Unescape(line.substr(0, separator)), Unescape(line.substr(separator + 1)));
		}
		SetLanguage(language);
	}

	static void SetLanguage(std::string_view language) {
		CurrentLanguage() = (language == "ru" || language == "Russian") ? "ru" : "en";
	}

	static const std::string& GetLanguage() { return CurrentLanguage(); }

	/// Returns a localized string, or the original English text when no Russian
	/// translation is available. The English fallback is never an empty string.
	static std::string Get(std::string_view englishText) {
		if (CurrentLanguage() == "ru") {
			const auto translation = Translations().find(std::string(englishText));
			if (translation != Translations().end() && !translation->second.empty()) {
				return translation->second;
			}
			static constexpr std::string_view dynamicPrefixes[] = {"Volume: ", "Mouse Sensitivity: ", "Stick Deadzone: ", "Need ", "Player ", "Will save in ", "Need ", "Step "};
			for (const std::string_view prefix : dynamicPrefixes) {
				if (englishText.starts_with(prefix)) {
					const auto prefixTranslation = Translations().find(std::string(prefix));
					if (prefixTranslation != Translations().end()) {
						return prefixTranslation->second + std::string(englishText.substr(prefix.size()));
					}
				}
			}
		}
		return std::string(englishText);
	}

	/// Localizes a string and adapts UTF-8 Russian literals to the byte encoding
	/// expected by Cortex Command's legacy bitmap font.
	static std::string ForBitmapFont(std::string_view text) {
		std::string localized = Get(text);
		return CurrentLanguage() == "ru" ? Utf8ToWindows1251(localized) : localized;
	}

private:
	static std::string Unescape(std::string value) {
		for (size_t position = 0; (position = value.find("\\n", position)) != std::string::npos;) {
			value.replace(position, 2, "\n");
			++position;
		}
		return value;
	}

	static std::unordered_map<std::string, std::string>& Translations() {
		static std::unordered_map<std::string, std::string> translations;
		return translations;
	}

	static std::string& CurrentLanguage() {
		static std::string language = "en";
		return language;
	}

	static std::string Utf8ToWindows1251(std::string_view input) {
		std::string output;
		output.reserve(input.size());
		for (size_t i = 0; i < input.size();) {
			const uint8_t first = static_cast<uint8_t>(input[i++]);
			uint32_t codepoint = first;
			if ((first & 0xE0) == 0xC0 && i < input.size()) {
				codepoint = ((first & 0x1F) << 6) | (static_cast<uint8_t>(input[i++]) & 0x3F);
			} else if ((first & 0xF0) == 0xE0 && i + 1 < input.size()) {
				codepoint = ((first & 0x0F) << 12) | ((static_cast<uint8_t>(input[i++]) & 0x3F) << 6);
				codepoint |= static_cast<uint8_t>(input[i++]) & 0x3F;
			} else if (first >= 0x80) {
				// Preserve malformed or unsupported UTF-8 as a visible replacement.
				output.push_back('?');
				continue;
			}

			if (codepoint < 0x80 || (codepoint >= 0xA0 && codepoint <= 0xFF)) {
				output.push_back(static_cast<char>(codepoint));
			} else if (codepoint >= 0x0410 && codepoint <= 0x042F) {
				output.push_back(static_cast<char>(0xC0 + codepoint - 0x0410));
			} else if (codepoint >= 0x0430 && codepoint <= 0x044F) {
				output.push_back(static_cast<char>(0xE0 + codepoint - 0x0430));
			} else if (codepoint == 0x0401) {
				output.push_back(static_cast<char>(0xA8));
			} else if (codepoint == 0x0451) {
				output.push_back(static_cast<char>(0xB8));
			} else {
				output.push_back('?');
			}
		}
		return output;
	}
};

} // namespace RTE
