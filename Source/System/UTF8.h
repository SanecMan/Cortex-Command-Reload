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

} // namespace RTE::UTF8
