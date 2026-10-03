#pragma once

#include <cstddef>
#include <cstdint>
#include <istream>
#include <string_view>

namespace RTE::UTF8 {

inline void SkipByteOrderMark(std::istream& stream) {
	const std::streampos beginning = stream.tellg();
	if (beginning == std::streampos(-1)) {
		return;
	}
	unsigned char prefix[3]{};
	stream.read(reinterpret_cast<char*>(prefix), 3);
	if (stream.gcount() == 3 && prefix[0] == 0xEF && prefix[1] == 0xBB && prefix[2] == 0xBF) {
		return;
	}
	stream.clear();
	stream.seekg(beginning);
}

// Decode one UTF-8 code point. Invalid sequences are reported as U+FFFD and
// consume one byte, so callers always make progress without reading past end.
inline bool Decode(std::string_view text, std::size_t offset, std::uint32_t& codePoint, std::size_t& byteCount) {
	if (offset >= text.size()) {
		codePoint = 0;
		byteCount = 0;
		return false;
	}

	const auto first = static_cast<unsigned char>(text[offset]);
	if (first < 0x80) {
		codePoint = first;
		byteCount = 1;
		return true;
	}

	std::size_t expectedBytes = 0;
	std::uint32_t value = 0;
	if (first >= 0xC2 && first <= 0xDF) {
		expectedBytes = 2;
		value = first & 0x1F;
	} else if (first >= 0xE0 && first <= 0xEF) {
		expectedBytes = 3;
		value = first & 0x0F;
	} else if (first >= 0xF0 && first <= 0xF4) {
		expectedBytes = 4;
		value = first & 0x07;
	} else {
		codePoint = 0xFFFD;
		byteCount = 1;
		return false;
	}

	if (offset + expectedBytes > text.size()) {
		codePoint = 0xFFFD;
		byteCount = 1;
		return false;
	}
	for (std::size_t i = 1; i < expectedBytes; ++i) {
		const auto next = static_cast<unsigned char>(text[offset + i]);
		if ((next & 0xC0) != 0x80) {
			codePoint = 0xFFFD;
			byteCount = 1;
			return false;
		}
		value = (value << 6) | (next & 0x3F);
	}

	const bool overlong = (expectedBytes == 2 && value < 0x80) || (expectedBytes == 3 && value < 0x800) || (expectedBytes == 4 && value < 0x10000);
	if (overlong || value > 0x10FFFF || (value >= 0xD800 && value <= 0xDFFF)) {
		codePoint = 0xFFFD;
		byteCount = 1;
		return false;
	}

	codePoint = value;
	byteCount = expectedBytes;
	return true;
}

inline std::size_t NextBoundary(std::string_view text, std::size_t offset) {
	if (offset >= text.size()) {
		return text.size();
	}
	std::uint32_t codePoint = 0;
	std::size_t byteCount = 1;
	Decode(text, offset, codePoint, byteCount);
	return offset + byteCount;
}

inline std::size_t PreviousBoundary(std::string_view text, std::size_t offset) {
	offset = offset > text.size() ? text.size() : offset;
	if (offset == 0) {
		return 0;
	}
	std::size_t previous = offset - 1;
	while (previous > 0 && (static_cast<unsigned char>(text[previous]) & 0xC0) == 0x80) {
		--previous;
	}
	return previous;
}

inline std::size_t CountCodepoints(std::string_view text) {
	std::size_t count = 0;
	for (std::size_t offset = 0; offset < text.size(); offset = NextBoundary(text, offset)) {
		++count;
	}
	return count;
}

inline std::size_t Advance(std::string_view text, std::size_t offset, std::size_t codepoints) {
	while (codepoints-- > 0 && offset < text.size()) {
		offset = NextBoundary(text, offset);
	}
	return offset;
}

inline bool IsValid(std::string_view text) {
	for (std::size_t offset = 0; offset < text.size();) {
		std::uint32_t codePoint = 0;
		std::size_t byteCount = 1;
		if (!Decode(text, offset, codePoint, byteCount)) {
			return false;
		}
		offset += byteCount;
	}
	return true;
}

inline void AppendCodepoint(std::string& output, std::uint32_t codePoint) {
	if (codePoint <= 0x7F) {
		output += static_cast<char>(codePoint);
	} else if (codePoint <= 0x7FF) {
		output += static_cast<char>(0xC0 | (codePoint >> 6));
		output += static_cast<char>(0x80 | (codePoint & 0x3F));
	} else if (codePoint <= 0xFFFF && !(codePoint >= 0xD800 && codePoint <= 0xDFFF)) {
		output += static_cast<char>(0xE0 | (codePoint >> 12));
		output += static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F));
		output += static_cast<char>(0x80 | (codePoint & 0x3F));
	} else if (codePoint <= 0x10FFFF && !(codePoint >= 0xD800 && codePoint <= 0xDFFF)) {
		output += static_cast<char>(0xF0 | (codePoint >> 18));
		output += static_cast<char>(0x80 | ((codePoint >> 12) & 0x3F));
		output += static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F));
		output += static_cast<char>(0x80 | (codePoint & 0x3F));
	} else {
		output += "\xEF\xBF\xBD";
	}
}

// Preserve old text-based .rte files when they contain Windows-1251 bytes.
// Valid UTF-8 remains byte-for-byte unchanged; malformed sequences trigger a
// whole-string Windows-1251 fallback, matching the legacy file encoding.
inline std::string PreserveLegacyWindows1251(std::string_view text) {
	bool hasNonAsciiByte = false;
	for (unsigned char byte : text) {
		if (byte >= 0x80) {
			hasNonAsciiByte = true;
			break;
		}
	}
	if (!hasNonAsciiByte || IsValid(text)) {
		return std::string(text);
	}
	static constexpr std::uint16_t c_Windows1251LowBytes[] = {
		0x0402, 0x0403, 0x201A, 0x0453, 0x201E, 0x2026, 0x2020, 0x2021,
		0x20AC, 0x2030, 0x0409, 0x2039, 0x040A, 0x040C, 0x040B, 0x040F,
		0x0452, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014,
		0xFFFD, 0x2122, 0x0459, 0x203A, 0x045A, 0x045C, 0x045B, 0x045F,
		0x00A0, 0x040E, 0x045E, 0x0408, 0x00A4, 0x0490, 0x00A6, 0x00A7,
		0x0401, 0x00A9, 0x0404, 0x00AB, 0x00AC, 0x00AD, 0x00AE, 0x0407,
		0x00B0, 0x00B1, 0x0406, 0x0456, 0x0491, 0x00B5, 0x00B6, 0x00B7,
		0x0451, 0x2116, 0x0454, 0x00BB, 0x0458, 0x0405, 0x0455, 0x0457
	};
	std::string converted;
	converted.reserve(text.size() * 2);
	for (unsigned char byte : text) {
		std::uint32_t codePoint = byte;
		if (byte >= 0xC0) {
			codePoint = byte <= 0xDF ? 0x0410 + byte - 0xC0 : 0x0430 + byte - 0xE0;
		} else if (byte >= 0x80) {
			codePoint = c_Windows1251LowBytes[byte - 0x80];
		}
		AppendCodepoint(converted, codePoint);
	}
	return converted;
}

} // namespace RTE::UTF8
