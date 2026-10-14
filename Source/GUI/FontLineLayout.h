#pragma once
#include <string_view>

namespace RTE {
// Break only at ASCII spaces/newlines, which cannot occur inside a UTF-8 glyph.
// Both rendering and height measurement use this exact sequence of lines.
template <class Measure, class Emit>
void ForEachFontLine(std::string_view text, int maxWidth, Measure measure, Emit emit) {
    if (text.empty()) return;
    std::size_t paragraphStart = 0;
    for (;;) {
        const auto newline = text.find('\n', paragraphStart);
        const auto paragraphEnd = newline == std::string_view::npos ? text.size() : newline;
        std::size_t start = paragraphStart;
        if (start == paragraphEnd) emit(text.substr(start, 0), 0);
        while (start < paragraphEnd) {
            std::size_t end = paragraphEnd;
            int width = measure(text.substr(start, end - start));
            if (maxWidth > 0 && width > maxWidth) {
                auto space = text.rfind(' ', paragraphEnd - 1);
                std::size_t firstWordEnd = paragraphEnd;
                while (space != std::string_view::npos && space > start) {
                    firstWordEnd = space;
                    const int candidateWidth = measure(text.substr(start, space - start));
                    if (candidateWidth <= maxWidth) { end = space; width = candidateWidth; break; }
                    space = text.rfind(' ', space - 1);
                }
                if (end == paragraphEnd && firstWordEnd < paragraphEnd) {
                    // An oversized first word overflows intact; following words
                    // and subsequent explicit paragraphs still get rendered.
                    end = firstWordEnd; width = measure(text.substr(start, end - start));
                }
            }
            emit(text.substr(start, end - start), width);
            start = end;
            while (start < paragraphEnd && text[start] == ' ') ++start;
        }
        if (newline == std::string_view::npos) break;
        paragraphStart = newline + 1;
    }
}
}
