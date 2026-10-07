// Console entry point linked with the same engine objects as the client.
#include "System/UTF8.h"
#include <Windows.h>
#include <string>
#include <vector>

int main(int argc, char** argv);

int wmain(int argc, wchar_t** argv) {
	SetConsoleOutputCP(CP_UTF8);
	std::vector<std::string> arguments;
	arguments.reserve(static_cast<std::size_t>(argc) + 1);
	for (int index = 0; index < argc; ++index) {
		const std::wstring_view wide(argv[index]);
		std::u16string utf16;
		for (const wchar_t character : wide) utf16.push_back(static_cast<char16_t>(character));
		arguments.push_back(RTE::UTF8::EncodeUTF16(utf16));
	}
	arguments.emplace_back("--server");
	std::vector<char*> pointers;
	for (std::string& argument : arguments) pointers.push_back(argument.data());
	return main(static_cast<int>(pointers.size()), pointers.data());
}
