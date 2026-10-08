#include "System/ApplicationEntry.h"

#ifdef _WIN32
#include "System/UTF8.h"
#include <Windows.h>
#include <shellapi.h>
#include <cstdlib>
#include <string>
#include <vector>

int APIENTRY WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
	int count = 0;
	LPWSTR* wideArguments = CommandLineToArgvW(GetCommandLineW(), &count);
	if (!wideArguments) return RunCortexCommand(__argc, __argv);
	std::vector<std::string> arguments;
	arguments.reserve(static_cast<std::size_t>(count));
	for (int index = 0; index < count; ++index) {
		const std::wstring_view wide(wideArguments[index]);
		std::u16string utf16;
		for (const wchar_t character : wide) utf16.push_back(static_cast<char16_t>(character));
		arguments.push_back(RTE::UTF8::EncodeUTF16(utf16));
	}
	LocalFree(wideArguments);
	std::vector<char*> pointers;
	for (std::string& argument : arguments) pointers.push_back(argument.data());
	pointers.push_back(nullptr);
	return RunCortexCommand(count, pointers.data());
}
#else
int main(int argc, char** argv) { return RunCortexCommand(argc, argv); }
#endif
