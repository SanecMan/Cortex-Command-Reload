#include "Reader.h"
#include "ConsoleMan.h"
#include "PresetMan.h"
#include "SettingsMan.h"
#include "System.h"
#include "UTF8.h"

#include <fstream>

using namespace RTE;

namespace {
	int PeekBufferedCharacter(std::istream& stream) {
		const auto character = stream.rdbuf()->sgetc();
		if (std::char_traits<char>::eq_int_type(character, std::char_traits<char>::eof())) {
			stream.setstate(std::ios::eofbit);
			return -1;
		}
		return std::char_traits<char>::to_int_type(std::char_traits<char>::to_char_type(character));
	}

	int GetBufferedCharacter(std::istream& stream) {
		const auto character = stream.rdbuf()->sbumpc();
		if (std::char_traits<char>::eq_int_type(character, std::char_traits<char>::eof())) {
			stream.setstate(std::ios::eofbit);
			return -1;
		}
		return std::char_traits<char>::to_int_type(std::char_traits<char>::to_char_type(character));
	}
}

void Reader::Clear() {
	m_Stream = nullptr;
	m_FilePath.clear();
	m_CurrentLine = 1;
	m_PreviousIndent = 0;
	m_IndentDifference = 0;
	m_ObjectEndings = 0;
	m_EndOfStreams = false;
	m_ReportProgress = nullptr;
	m_ReportTabs = "\t";
	m_FileName.clear();
	m_DataModuleName.clear();
	m_DataModuleID = -1;
	m_OverwriteExisting = false;
	m_SkipIncludes = false;
	m_CanFail = false;
	m_NonModulePath = false;
}

Reader::Reader(const std::string& fileName, bool overwrites, const ProgressCallback& progressCallback, bool failOK, bool nonModulePath) {
	Clear();
	m_NonModulePath = nonModulePath;
	Create(fileName, overwrites, progressCallback, failOK);
}

Reader::Reader(std::unique_ptr<std::istream>&& stream, const std::string& fileName, bool overwrites, const ProgressCallback& progressCallback, bool failOK) {
	Clear();
	Create(std::move(stream), fileName, overwrites, progressCallback, failOK);
}

int Reader::Create(const std::string& fileName, bool overwrites, const ProgressCallback& progressCallback, bool failOK) {
	if (fileName.empty()) {
		return -1;
	}
	
	if (m_NonModulePath) {
		m_FilePath = UTF8::PathToString(UTF8::PathFromString(fileName));
		// Associate non-module paths with Base to prevent implosions when dealing with creating Entities.
		m_DataModuleName = "Base.rte";
		m_DataModuleID = 0;
	} else {
		m_FilePath = g_PresetMan.GetFullModulePath(fileName);

		// Extract the file name and module name from the path
		m_FileName = m_FilePath.substr(m_FilePath.find_last_of("/\\") + 1);
		m_DataModuleName = g_PresetMan.GetModuleNameFromPath(m_FilePath);
		m_DataModuleID = g_PresetMan.GetModuleID(m_DataModuleName);
	}
	
	return Create(std::make_unique<std::ifstream>(UTF8::PathFromString(m_FilePath)), fileName, overwrites, progressCallback, failOK);
}

int Reader::Create(std::unique_ptr<std::istream>&& stream, const std::string& fileName, bool overwrites, const ProgressCallback& progressCallback, bool failOK) {
	// We redundantly do this following block of code in both constructors, which feels really ugly and lazy
	if (fileName.empty()) {
		return -1;
	}

	if (m_NonModulePath) {
		m_FilePath = UTF8::PathToString(UTF8::PathFromString(fileName));
		// Associate non-module paths with Base to prevent implosions when dealing with creating Entities.
		m_DataModuleName = "Base.rte";
		m_DataModuleID = 0;
	} else {
		m_FilePath = g_PresetMan.GetFullModulePath(fileName);

		// Extract the file name and module name from the path
		m_FileName = m_FilePath.substr(m_FilePath.find_last_of("/\\") + 1);
		m_DataModuleName = g_PresetMan.GetModuleNameFromPath(m_FilePath);
		m_DataModuleID = g_PresetMan.GetModuleID(m_DataModuleName);
	}

	m_CanFail = failOK;

	m_Stream = std::move(stream);
	if (m_Stream && m_Stream->good()) {
		UTF8::SkipByteOrderMark(*m_Stream);
	}

	if (!m_CanFail) {
		RTEAssert(m_Stream->good(), "Failed to open data file \"" + m_FilePath + "\"!");
	}

	m_OverwriteExisting = overwrites;

	// Report that we're starting a new file
	m_ReportProgress = progressCallback;
	if (m_ReportProgress && m_Stream->good()) {
		m_ReportProgress("\t" + m_FileName + " on line " + std::to_string(m_CurrentLine), true);
	}

	return m_Stream->good() ? 0 : -1;
}

int Reader::GetReadModuleID() const {
	return (m_DataModuleID < 0) ? g_PresetMan.GetModuleID(m_DataModuleName) : m_DataModuleID;
}

std::string Reader::WholeFileAsString() const {
	std::stringstream stringStream;
	stringStream << m_Stream->rdbuf();
	return stringStream.str();
}

std::string Reader::ReadLine() {
	DiscardEmptySpace();

	std::string retString;
	char temp;
	int peek = PeekBufferedCharacter(*m_Stream);

	while (peek != '\n' && peek != '\r' && peek != '\t') {
		temp = static_cast<char>(GetBufferedCharacter(*m_Stream));

		// Check for line comment "//"
		if (peek == '/' && PeekBufferedCharacter(*m_Stream) == '/') {
			m_Stream->unget();
			break;
		}

		if (m_Stream->eof()) {
			break;
		}
		if (!m_Stream->good()) {
			ReportError("Stream failed for some reason");
		}

		retString.append(1, temp);
		peek = PeekBufferedCharacter(*m_Stream);
	}
	return UTF8::PreserveLegacyWindows1251(TrimString(retString));
}

std::string Reader::ReadPropName() {
	DiscardEmptySpace();

	std::string retString;
	char temp;
	int peek;

	while (true) {
		peek = PeekBufferedCharacter(*m_Stream);
		if (peek == '=') {
			GetBufferedCharacter(*m_Stream);
			break;
		}
		if (peek == '\n' || peek == '\r' || peek == '\t') {
			ReportError("Property name wasn't followed by a value");
		}

		temp = static_cast<char>(GetBufferedCharacter(*m_Stream));
		if (m_Stream->eof()) {
			EndIncludeFile();
			break;
		}
		if (!m_Stream->good() || temp == -1) {
			ReportError("Stream failed for some reason");
			EndIncludeFile();
			break;
		}
		retString.append(1, temp);
	}
	// Trim the string of whitespace
	retString = UTF8::PreserveLegacyWindows1251(TrimString(retString));

	// If the property name turns out to be the special IncludeFile,and we're not skipping include files then open that file and read the first property from it instead.
	if (retString == "IncludeFile") {
		if (m_SkipIncludes) {
			// Discard IncludeFile value
			std::string val = ReadPropValue();
			DiscardEmptySpace();
			retString = ReadPropName();
		} else {
			StartIncludeFile();
			// Return the first property name in the new file, this is to make the file inclusion seamless.
			// Alternatively, if StartIncludeFile failed, this will just grab the next prop name and ignore the failed IncludeFile property.
			retString = ReadPropName();
		}
	}
	return retString;
}

std::string Reader::ReadPropValue() {
	std::string fullLine = ReadLine();
	size_t valuePos = fullLine.find_first_of('=');
	std::string propValue = (valuePos == std::string::npos) ? fullLine : fullLine.substr(valuePos + 1);
	return TrimString(propValue);
}

bool Reader::NextProperty() {
	if (!DiscardEmptySpace() || m_EndOfStreams) {
		return false;
	}
	// If there are fewer tabs on the last line eaten this time, that means there are no more properties to read on this object.
	if (m_ObjectEndings < -m_IndentDifference) {
		m_ObjectEndings++;
		return false;
	}
	m_ObjectEndings = 0;
	return true;
}

std::string Reader::TrimString(const std::string& stringToTrim) const {
	if (stringToTrim.empty()) {
		return "";
	}
	size_t start = stringToTrim.find_first_not_of(' ');
	size_t end = stringToTrim.find_last_not_of(' ');

	return stringToTrim.substr(start, (end - start + 1));
}

bool Reader::DiscardEmptySpace() {
	int peek;
	int indent = 0;
	int leadingSpaceCount = 0;
	bool discardedLine = false;

	while (true) {
		peek = PeekBufferedCharacter(*m_Stream);

		// If we have hit the end and don't have any files to resume, then quit and indicate that
		if (m_Stream->eof()) {
			return EndIncludeFile();
		}
		// Not end-of-file but still got junk back... something went to shit
		if (peek == -1) {
			ReportError("Something went wrong reading the line; make sure it is providing the expected type");
		}

		// Discard spaces
		if (peek == ' ') {
			leadingSpaceCount++;
			GetBufferedCharacter(*m_Stream);
			// Discard tabs, and count them
		} else if (peek == '\t') {
			indent++;
			GetBufferedCharacter(*m_Stream);
			// Discard newlines and reset the tab count for the new line, also count the lines
		} else if (peek == '\n' || peek == '\r') {
			// So we don't count lines twice when there are both newline and carriage return at the end of lines
			if (peek == '\n') {
				m_CurrentLine++;
				// Only report every few lines
				if (m_ReportProgress && (m_CurrentLine % g_SettingsMan.LoadingScreenProgressReportPrecision() == 0)) {
					m_ReportProgress(m_ReportTabs + m_FileName + " reading line " + std::to_string(m_CurrentLine), false);
				}
			}
			indent = 0;
			leadingSpaceCount = 0;
			discardedLine = true;
			GetBufferedCharacter(*m_Stream);

			// Comment line?
		} else if (PeekBufferedCharacter(*m_Stream) == '/') {
			char temp = static_cast<char>(GetBufferedCharacter(*m_Stream));

			// Confirm that it's a comment line, if so discard it and continue
			if (PeekBufferedCharacter(*m_Stream) == '/') {
				while (PeekBufferedCharacter(*m_Stream) != '\n' && PeekBufferedCharacter(*m_Stream) != '\r' && !m_Stream->eof()) {
					GetBufferedCharacter(*m_Stream);
				}
				// Block comment
			} else if (PeekBufferedCharacter(*m_Stream) == '*') {
				int openBlockComments = 1;
				m_BlockCommentOpenTagLines.emplace(m_CurrentLine);

				char temp2 = 0;
				while (openBlockComments > 0 && !m_Stream->eof()) {
					temp2 = static_cast<char>(GetBufferedCharacter(*m_Stream));
					if (temp2 == '\n') {
						++m_CurrentLine;
					}

					// Find the matching close tag.
					if (!(temp2 == '*' && PeekBufferedCharacter(*m_Stream) == '/')) {
						// Check if a nested block comment open tag.
						if (temp2 == '/' && PeekBufferedCharacter(*m_Stream) == '*') {
							openBlockComments++;
							m_BlockCommentOpenTagLines.emplace(m_CurrentLine);
						}
					} else {
						openBlockComments--;
						m_BlockCommentOpenTagLines.pop();
					}
				}
				// Discard that final '/'.
				if (!m_Stream->eof()) {
					GetBufferedCharacter(*m_Stream);
				} else if (openBlockComments > 0) {
					ReportError("File stream ended with an open block comment!\nCouldn't find closing tag for block comment opened on line " + std::to_string(m_BlockCommentOpenTagLines.top()) + ".\n");
				}

				// Not a comment, so it's data, so quit.
			} else {
				m_Stream->putback(temp);
				break;
			}
		} else {
			break;
		}
	}

	// This precaution enables us to use DiscardEmptySpace repeatedly without messing up the indentation tracking logic
	if (discardedLine) {
		if (leadingSpaceCount > 0) {
			ReportError("Encountered space characters used for indentation where a tab character was expected!\nPlease make sure the preset definition structure is correct.\n");
		}
		// Get indentation difference from the last line of the last call to DiscardEmptySpace(), and the last line of this call to DiscardEmptySpace().
		m_IndentDifference = indent - m_PreviousIndent;
		if (m_IndentDifference > 1) {
			ReportError("Over indentation detected!\nPlease make sure the preset definition structure is correct.\n");
		}
		// Save the last tab count
		m_PreviousIndent = indent;
	}
	return true;
}

void Reader::ReportError(const std::string& errorDesc) const {
	if (!m_CanFail) {
		RTEAbort(errorDesc + "\nError happened in " + m_FilePath + " at line " + std::to_string(m_CurrentLine) + "!");
	} else {
		if (m_ReportProgress) {
			m_ReportProgress(errorDesc + ", skipping!", true);
		}
	}
}

bool Reader::StartIncludeFile() {
	// Report that we're including a file
	if (m_ReportProgress) {
		m_ReportProgress(m_ReportTabs + m_FileName + " on line " + std::to_string(m_CurrentLine) + " includes:", false);
	}

	// Get the file path from the current stream before pushing it into the StreamStack, otherwise we can't open a new stream after releasing it because we can't read.
	std::string includeFilePath = g_PresetMan.GetFullModulePath(ReadPropValue());

	// Push the current stream onto the StreamStack for future retrieval when the new include file has run out of data.
	m_StreamStack.push(StreamInfo(m_Stream.release(), m_FilePath, m_CurrentLine, m_PreviousIndent));

	m_FilePath = includeFilePath;
	m_Stream = std::make_unique<std::ifstream>(std::filesystem::u8path(m_FilePath));

	if (m_Stream->fail() || !System::PathExistsCaseSensitive(includeFilePath)) {
		// Backpedal and set up to read the next property in the old stream
		m_Stream.reset(m_StreamStack.top().Stream); // Destructs the current m_Stream and takes back ownership and management of the raw StreamInfo std::istream pointer.
		m_FilePath = m_StreamStack.top().FilePath;
		m_CurrentLine = m_StreamStack.top().CurrentLine;
		m_PreviousIndent = m_StreamStack.top().PreviousIndent;
		m_StreamStack.pop();

		ReportError((!m_CanFail ? "" : m_ReportTabs + "\t") + "Failed to open included data file \"" + includeFilePath + "\"");

		DiscardEmptySpace();
		return false;
	}
	UTF8::SkipByteOrderMark(*m_Stream);

	// Line counting starts with 1, not 0
	m_CurrentLine = 1;
	// This is set to 0, because locally in the included file, all properties start at that count
	m_PreviousIndent = 0;

	m_FileName = m_FilePath.substr(m_FilePath.find_first_of("/\\") + 1);

	// Report that we're starting a new file
	if (m_ReportProgress) {
		m_ReportTabs = "\t";
		for (int i = 0; i < m_StreamStack.size(); ++i) {
			m_ReportTabs.append("\t");
		}
		m_ReportProgress(m_ReportTabs + m_FileName + " on line " + std::to_string(m_CurrentLine), true);
	}
	DiscardEmptySpace();
	return true;
}

bool Reader::EndIncludeFile() {
	if (m_ReportProgress) {
		m_ReportProgress(m_ReportTabs + m_FileName + " - done! " + static_cast<char>(-42), false);
	}

	if (m_StreamStack.empty()) {
		m_EndOfStreams = true;
		return false;
	}

	// Replace the current included stream with the parent one
	m_Stream.reset(m_StreamStack.top().Stream);
	m_FilePath = m_StreamStack.top().FilePath;
	m_CurrentLine = m_StreamStack.top().CurrentLine;

	// Observe it's being added, not just replaced. This is to keep proper track when exiting out of a file.
	m_PreviousIndent += m_StreamStack.top().PreviousIndent;

	m_StreamStack.pop();

	m_FileName = m_FilePath.substr(m_FilePath.find_first_of("/\\") + 1);

	// Report that we're going back a file
	if (m_ReportProgress) {
		m_ReportTabs = "\t";
		for (int i = 0; i < m_StreamStack.size(); ++i) {
			m_ReportTabs.append("\t");
		}
		m_ReportProgress(m_ReportTabs + m_FileName + " on line " + std::to_string(m_CurrentLine), true);
	}
	DiscardEmptySpace();
	return true;
}
