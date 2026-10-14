#pragma once

#include "../System/UTF8.h"

namespace RTE {

// Text is UTF-8. Historical HUD strings also contain standalone byte indices
// into the bitmap atlas. Preserve those only when strict UTF-8 decoding fails;
// never apply this compatibility rule to paths, parsers or network payloads.
// Return true only for an explicit legacy byte fallback. The numeric Unicode
// code point alone cannot distinguish a Latin letter from a HUD atlas index.
inline bool DecodeFontGlyph(std::string_view text, std::size_t offset, std::uint32_t& codePoint, std::size_t& byteCount) {
	if (!UTF8::Decode(text, offset, codePoint, byteCount) && offset < text.size()) {
		codePoint = static_cast<unsigned char>(text[offset]);
		byteCount = 1;
		return true;
	}
	return false;
}

inline bool UseBitmapFontGlyph(std::uint32_t codePoint, bool legacyByte, std::uint32_t atlasCapacity) {
	return codePoint < atlasCapacity && (codePoint < 128 || legacyByte);
}

} // namespace RTE
