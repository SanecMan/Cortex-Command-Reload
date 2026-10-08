// This frontend uses Windows code-page definitions excluded by legacy NONLS flags.
#ifdef NONLS
#undef NONLS
#endif
#include "System/ApplicationEntry.h"
#include "System/UTF8.h"
#ifdef _WIN32
#include <Windows.h>
#include <winnls.h>
#endif
#include <string>
#include <vector>

namespace {
int RunServer(std::vector<std::string> arguments) {
	arguments.emplace_back("--server");
	std::vector<char*> pointers;
	for (std::string& argument : arguments) pointers.push_back(argument.data());
	pointers.push_back(nullptr);
	return RunCortexCommand(static_cast<int>(arguments.size()), pointers.data());
}
}

#ifdef _WIN32
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
	return RunServer(std::move(arguments));
}
#else
int main(int argc, char** argv) {
	std::vector<std::string> arguments;
	for (int index = 0; index < argc; ++index) arguments.emplace_back(argv[index]);
	return RunServer(std::move(arguments));
}
#endif
