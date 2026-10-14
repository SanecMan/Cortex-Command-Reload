#include "../Source/GUI/FontLineLayout.h"
#include "../Source/System/UTF8.h"
#include <iostream>
#include <string>
#include <vector>
int main() {
    auto lines = [](std::string_view text, int width) {
        std::vector<std::string> result;
        RTE::ForEachFontLine(text, width, [](std::string_view line) { return static_cast<int>(RTE::UTF8::CountCodepoints(line)); },
            [&](std::string_view line, int) { result.emplace_back(line); });
        return result;
    };
    if (lines("", 1) != std::vector<std::string>{}) return 1;
    if (lines("\n\n", 1) != std::vector<std::string>{"", "", ""}) return 2;
    if (lines("А ББ В", 3) != std::vector<std::string>{"А", "ББ", "В"}) return 3;
    if (lines("Длинно слово\nСледующая строка", 4) != std::vector<std::string>{"Длинно", "слово", "Следующая", "строка"}) return 4;
    if (lines("È 🙂\nЖ", 3) != std::vector<std::string>{"È 🙂", "Ж"}) return 5;
    if (lines("A B\nC D", 2) != std::vector<std::string>{"A", "B", "C", "D"}) return 6;
    if (lines("А ББ В", 0) != std::vector<std::string>{"А ББ В"}) return 7;
    if (lines("A   B", 1) != std::vector<std::string>{"A", "B"}) return 8;
    std::cout << "font_line_layout=passed\n";
}
