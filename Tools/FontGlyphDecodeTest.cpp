#include "../Source/GUI/FontGlyphDecode.h"
#include <iostream>

int main() {
	std::uint32_t glyph = 0;
	std::size_t count = 0;
	for (unsigned int icon : {200U, 207U, 217U, 219U, 229U}) {
		const std::string raw(1, static_cast<char>(icon));
		RTE::DecodeFontGlyph(raw, 0, glyph, count);
		if (glyph != icon || count != 1) return 1;
		if (RTE::UTF8::Decode(raw, 0, glyph, count) || glyph != 0xFFFDU) return 2;
	}
	const std::string text = "Ж🙂";
	RTE::DecodeFontGlyph(text, 0, glyph, count);
	if (glyph != 0x416U || count != 2) return 3;
	RTE::DecodeFontGlyph(text, 2, glyph, count);
	if (glyph != 0x1F642U || count != 4) return 4;
	const std::string reload = "\xDB\xCF\xC8";
	for (std::size_t i = 0; i < reload.size(); ++i) {
		RTE::DecodeFontGlyph(reload, i, glyph, count);
		if (glyph != static_cast<unsigned char>(reload[i]) || count != 1) return 5;
	}
	RTE::DecodeFontGlyph(text, text.size(), glyph, count);
	if (glyph != 0 || count != 0) return 6;
	std::cout << "font_glyph_decode=passed\n";
}
