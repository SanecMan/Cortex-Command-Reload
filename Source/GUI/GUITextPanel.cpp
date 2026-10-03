#include "GUI.h"
#include "GUITextPanel.h"
#include "../System/UTF8.h"

#include <cassert>

using namespace RTE;

GUITextPanel::GUITextPanel(GUIManager* Manager) :
    GUIPanel(Manager) {
	m_Font = nullptr;
	m_CursorX = m_CursorY = 0;
	m_CursorIndex = 0;
	m_CursorColor = 0;
	m_BlinkTimer.Reset();

	m_FontColor = 0;
	m_FontSelectColor = 0;
	m_StartIndex = 0;
	m_GotSelection = false;
	m_SelectedColorIndex = 0;
	m_Locked = false;
	m_WidthMargin = 3;
	m_HeightMargin = 0;

	m_MaxTextLength = 0;
	m_NumericOnly = false;
	m_MaxNumericValue = 0;
}

// TODO: Both constructors use a common clear function?? Same with other panels

GUITextPanel::GUITextPanel() :
    GUIPanel(),
	m_FontSelectColor(0),
	m_Text(""),
	m_Locked(false),
	m_WidthMargin(3),
	m_HeightMargin(0),
	m_CursorX(m_CursorY = 0),
	m_CursorIndex(0),
	m_StartIndex(0),
	m_GotSelection(false),
	m_SelectedColorIndex(0),
	m_MaxTextLength(0),
	m_NumericOnly(false),
	m_MaxNumericValue(0) {

	m_Font = nullptr;
	m_CursorColor = 0;
	m_FontColor = 0;
	m_BlinkTimer.Reset();
}

void GUITextPanel::Create(int X, int Y, int Width, int Height) {
	m_X = X;
	m_Y = Y;
	m_Width = Width;
	m_Height = Height;

	assert(m_Manager);
}

void GUITextPanel::ChangeSkin(GUISkin* Skin) {
	// Load the font
	std::string Filename;
	Skin->GetValue("TextBox", "Font", &Filename);
	m_Font = Skin->GetFont(Filename);
	Skin->GetValue("TextBox", "FontColor", &m_FontColor);
	Skin->GetValue("TextBox", "FontShadow", &m_FontShadow);
	Skin->GetValue("TextBox", "FontKerning", &m_FontKerning);
	Skin->GetValue("TextBox", "FontSelectColor", &m_FontSelectColor);
	Skin->GetValue("TextBox", "SelectedColorIndex", &m_SelectedColorIndex);

	Skin->GetValue("TextBox", "WidthMargin", &m_WidthMargin);
	Skin->GetValue("TextBox", "HeightMargin", &m_HeightMargin);

	// Convert
	m_FontColor = Skin->ConvertColor(m_FontColor);
	m_FontSelectColor = Skin->ConvertColor(m_FontSelectColor);
	m_SelectedColorIndex = Skin->ConvertColor(m_SelectedColorIndex);

	// Pre-cache the font colors
	m_Font->CacheColor(m_FontColor);
	m_Font->CacheColor(m_FontSelectColor);

	// Get the cursor color
	Skin->GetValue("TextBox", "CursorColorIndex", &m_CursorColor);
	m_CursorColor = Skin->ConvertColor(m_CursorColor);
}

void GUITextPanel::Draw(GUIScreen* Screen) {
	if (!m_Font)
		return;

	int FontHeight = m_Font->GetFontHeight();
	int wSpacer = m_WidthMargin;
	int hSpacer = m_HeightMargin;

	// Clamp the cursor
	m_CursorX = std::max(m_CursorX, 0);

	// Setup the clipping
	Screen->GetBitmap()->SetClipRect(GetRect());

	std::string Text = m_Text.substr(m_StartIndex);

	// Draw the text
	m_Font->SetColor(m_FontColor);
	m_Font->SetKerning(m_FontKerning);
	m_Font->Draw(Screen->GetBitmap(), m_X + wSpacer, m_Y + hSpacer, Text, m_FontShadow);

	// Draw the right-justified extra text in the background
	m_Font->DrawAligned(Screen->GetBitmap(), m_X + m_Width - wSpacer, m_Y + hSpacer, m_RightText, GUIFont::Right, GUIFont::Top, m_Width, m_FontShadow);

	// Draw the selected text
	if (m_GotSelection && m_GotFocus && !m_Text.empty()) {
		// Draw selection mark
		Screen->GetBitmap()->DrawRectangle(m_X + wSpacer + m_SelectionX, m_Y + hSpacer + 2, m_SelectionWidth, FontHeight - 3, m_SelectedColorIndex, true);
		// Draw text with selection regions in different color
		m_Font->SetColor(m_FontSelectColor);
		int Start = std::min(m_StartSelection, m_EndSelection);
		int End = std::max(m_StartSelection, m_EndSelection);

		// Selection
		if (m_StartIndex > Start) {
			Start = m_StartIndex;
		}
		m_Font->Draw(Screen->GetBitmap(), m_X + wSpacer + m_SelectionX, m_Y + hSpacer, Text.substr(Start - m_StartIndex, End - Start));
	}

	// If we have focus, draw the blinking cursor
	const int blinkInterval = 250;
	bool shouldBlink = static_cast<int>(m_BlinkTimer.GetElapsedRealTimeMS()) % (blinkInterval * 2) > blinkInterval;
	if (m_GotFocus && shouldBlink) {
		Screen->GetBitmap()->DrawRectangle(m_X + m_CursorX + 2, m_Y + hSpacer + m_CursorY + 2, 1, FontHeight - 3, m_CursorColor, true);
	}

	// Restore normal clipping
	Screen->GetBitmap()->SetClipRect(nullptr);
}

void GUITextPanel::OnGainFocus() {
	GUIPanel::OnGainFocus();
	m_Manager->GetInputController()->StartTextInput();
}

void GUITextPanel::OnLoseFocus() {
	GUIPanel::OnLoseFocus();
	m_Manager->GetInputController()->StopTextInput();
}

void GUITextPanel::OnKeyPress(int KeyCode, int Modifier) {
	// TODO: Figure out what the "performance bitching" is.
	// Condition here to stop the compiler bitching about performance
	bool Shift = ((Modifier & MODI_SHIFT) != 0);
	bool ModKey = ((Modifier & MODI_CTRL) != 0);

	if (m_Locked) {
		return;
	}

	// Backspace
	if (KeyCode == GUIInput::Key_Backspace) {
		if (m_GotSelection) {
			RemoveSelectionText();
		} else {
			if (m_CursorIndex > 0) {
				int newCursorIndex = ModKey ? GetStartOfPreviousCharacterGroup(m_Text, m_CursorIndex) : static_cast<int>(UTF8::PreviousBoundary(m_Text, m_CursorIndex));
				m_Text.erase(newCursorIndex, m_CursorIndex - newCursorIndex);
				m_CursorIndex = newCursorIndex;
			}
		}
		UpdateText();
		SendSignal(Changed, 0);
		return;
	}

	// Delete
	if (KeyCode == GUIInput::Key_Delete) {
		if (m_GotSelection) {
			RemoveSelectionText();
		} else {
			if (m_CursorIndex < m_Text.size()) {
				int nextCursorIndex = ModKey ? GetStartOfNextCharacterGroup(m_Text, m_CursorIndex) : static_cast<int>(UTF8::NextBoundary(m_Text, m_CursorIndex));
				m_Text.erase(m_CursorIndex, nextCursorIndex - m_CursorIndex);
			}
		}
		UpdateText();
		SendSignal(Changed, 0);
		return;
	}

	// Left Arrow
	if (KeyCode == GUIInput::Key_LeftArrow) {
		if (m_CursorIndex > 0) {
			int newCursorIndex = ModKey ? GetStartOfPreviousCharacterGroup(m_Text, m_CursorIndex) : static_cast<int>(UTF8::PreviousBoundary(m_Text, m_CursorIndex));
			if (Shift) {
				DoSelection(m_CursorIndex, newCursorIndex);
			} else {
				m_GotSelection = false;
			}
			m_CursorIndex = newCursorIndex;
			UpdateText();
		}
		return;
	}

	// Right Arrow
	if (KeyCode == GUIInput::Key_RightArrow) {
		int newCursorIndex = ModKey ? GetStartOfNextCharacterGroup(m_Text, m_CursorIndex) : static_cast<int>(UTF8::NextBoundary(m_Text, m_CursorIndex));
		if (m_CursorIndex < m_Text.size()) {
			if (Shift) {
				DoSelection(m_CursorIndex, newCursorIndex);
			} else {
				m_GotSelection = false;
			}
			m_CursorIndex = newCursorIndex;
			UpdateText();
		}
		return;
	}

	// Home
	if (KeyCode == GUIInput::Key_Home) {
		if (Shift) {
			DoSelection(m_CursorIndex, 0);
		} else {
			m_GotSelection = false;
		}
		m_CursorIndex = 0;
		UpdateText();
		return;
	}

	// End
	if (KeyCode == GUIInput::Key_End) {
		if (Shift) {
			DoSelection(m_CursorIndex, m_Text.size());
		} else {
			m_GotSelection = false;
		}
		m_CursorIndex = m_Text.size();
		UpdateText();
		return;
	}

	// ModKey-X (Cut)
	if (KeyCode == 'x' && ModKey) {
		if (m_GotSelection) {
			GUIUtil::SetClipboardText(GetSelectionText());
			RemoveSelectionText();
			SendSignal(Changed, 0);
		}
		return;
	}

	// ModKey-C (Copy)
	if (KeyCode == 'c' && ModKey) {
		if (m_GotSelection) {
			GUIUtil::SetClipboardText(GetSelectionText());
		}
		return;
	}

	// ModKey-V (Paste)
	if (KeyCode == 'v' && ModKey) {
		RemoveSelectionText();
		std::string Text = "";
		GUIUtil::GetClipboardText(&Text);
		m_Text.insert(m_CursorIndex, Text);
		m_CursorIndex += Text.size();
		UpdateText(true, true);
		SendSignal(Changed, 0);
		return;
	}

	// ModKey-A (Select All)
	if (KeyCode == 'a' && ModKey) {
		DoSelection(0, m_Text.size());
		UpdateText();
		return;
	}

	// Enter key
	if (KeyCode == '\n' || KeyCode == '\r') {
		SendSignal(Enter, 0);
		return;
	}
}

void GUITextPanel::OnTextInput(std::string_view inputText) {
	for (std::size_t offset = 0; offset < inputText.size();) {
		std::uint32_t codePoint = 0;
		std::size_t byteCount = 1;
		if (!UTF8::Decode(inputText, offset, codePoint, byteCount)) {
			offset += byteCount;
			continue;
		}
		const bool accepted = m_NumericOnly ? (codePoint >= '0' && codePoint <= '9') : (codePoint >= 32 && codePoint != 127);
		if (accepted) {
			RemoveSelectionText();
			if (m_MaxTextLength > 0 && UTF8::CountCodepoints(m_Text) >= static_cast<std::size_t>(m_MaxTextLength)) {
				return;
			}
			m_Text.insert(static_cast<std::size_t>(m_CursorIndex), inputText.substr(offset, byteCount));
			m_CursorIndex += static_cast<int>(byteCount);

			if (m_NumericOnly && m_MaxNumericValue > 0 && std::stoi(m_Text) > m_MaxNumericValue) {
				m_Text = std::to_string(m_MaxNumericValue);
				m_CursorIndex = static_cast<int>(m_Text.size());
			}

			SendSignal(Changed, 0);
			UpdateText(true);
		}
		offset += byteCount;
	}
}

void GUITextPanel::OnMouseDown(int X, int Y, int Buttons, int Modifier) {
	SendSignal(MouseDown, Buttons);

	if (m_Locked) {
		return;
	}

	// Give me focus
	SetFocus();
	CaptureMouse();

	if (!(Buttons & MOUSE_LEFT)) {
		return;
	}

	int OldIndex = m_CursorIndex;

	// Set the cursor
	std::string Text = m_Text.substr(m_StartIndex, m_Text.size() - m_StartIndex);
	m_CursorIndex = m_Text.size();

	if (!(Modifier & MODI_SHIFT)) {
		m_GotSelection = false;
	}

	// Go through each character until we to the mouse point
	int TX = m_X;
	for (std::size_t i = 0; i < Text.size();) {
		const std::size_t next = UTF8::NextBoundary(Text, i);
		TX += m_Font->CalculateWidth(Text.substr(i, next - i));
		if (TX > X) {
			m_CursorIndex = static_cast<int>(i) + m_StartIndex;
			break;
		}
		i = next;
	}

	// Do a selection if holding the shift button
	if (Modifier & MODI_SHIFT)
		DoSelection(OldIndex, m_CursorIndex);

	// Update the text
	UpdateText(false, false);
}

void GUITextPanel::OnMouseMove(int X, int Y, int Buttons, int Modifier) {
	if (!(Buttons & MOUSE_LEFT) || !IsCaptured()) {
		return;
	}

	// Select from the mouse down point to where the mouse is currently
	std::string Text = m_Text.substr(m_StartIndex, m_Text.size() - m_StartIndex);
	int TX = m_X;
	for (std::size_t i = 0; i < Text.size();) {
		const std::size_t next = UTF8::NextBoundary(Text, i);
		TX += m_Font->CalculateWidth(Text.substr(i, next - i));
		if (TX >= X) {
			DoSelection(m_CursorIndex, static_cast<int>(i) + m_StartIndex);
			m_CursorIndex = static_cast<int>(i) + m_StartIndex;
			UpdateText(false, true);
			break;
		}
		i = next;
	}

	// Double check for the mouse at the end of the text
	if (X > TX) {
		DoSelection(m_CursorIndex, m_Text.size());
		m_CursorIndex = m_Text.size();
		UpdateText(false, true);
	}
}

void GUITextPanel::OnMouseUp(int X, int Y, int Buttons, int Modifier) {
	ReleaseMouse();
	SendSignal(Clicked, Buttons);
}

void GUITextPanel::UpdateText(bool Typing, bool DoIncrement) {
	if (!m_Font) {
		return;
	}

	// Using increments of four characters to show a little extra of the text when
	// moving and typing
	// Only do this when NOT typing
	int Increment = 4;
	int Spacer = 2;

	if (Typing) {
		Increment = 1;
	}

	// Make sure the cursor is greater or equal to the start index
	if (m_CursorIndex <= m_StartIndex && DoIncrement) {
		m_StartIndex = static_cast<int>(UTF8::PreviousBoundary(m_Text, m_CursorIndex));
		for (int i = 1; i < Increment && m_StartIndex > 0; ++i) {
			m_StartIndex = static_cast<int>(UTF8::PreviousBoundary(m_Text, m_StartIndex));
		}
	}

	// Clamp it
	m_StartIndex = std::max(m_StartIndex, 0);

	// If the cursor is greater than the length of text panel, adjust the start index
	std::string Sub = m_Text.substr(m_StartIndex, m_CursorIndex - m_StartIndex);
	while (m_Font->CalculateWidth(Sub) > m_Width - Spacer * 2 && DoIncrement) {
		m_StartIndex = static_cast<int>(UTF8::Advance(m_Text, m_StartIndex, Increment));
		Sub = m_Text.substr(m_StartIndex, m_CursorIndex - m_StartIndex);
	}

	// Clamp it
	m_StartIndex = std::max(0, std::min(m_StartIndex, static_cast<int>(m_Text.size())));
	while (m_StartIndex > 0 && m_StartIndex < static_cast<int>(m_Text.size()) &&
	       (static_cast<unsigned char>(m_Text[m_StartIndex]) & 0xC0) == 0x80) {
		--m_StartIndex;
	}

	// Adjust the cursor position
	m_CursorX = m_Font->CalculateWidth(m_Text.substr(m_StartIndex, m_CursorIndex - m_StartIndex));

	// Update the selection
	if (m_GotSelection) {
		DoSelection(m_StartSelection, m_EndSelection);
	}
}

void GUITextPanel::DoSelection(int Start, int End) {
	// Start a selection
	if (!m_GotSelection) {
		m_GotSelection = true;
		m_StartSelection = Start;
		m_EndSelection = End;
	} else {
		// Update the selection
		m_EndSelection = End;
	}

	// Avoid zero char selections
	if (m_GotSelection && m_StartSelection == m_EndSelection) {
		m_GotSelection = false;
		return;
	}

	// Update the selection coordinates
	int StartSel = std::min(m_StartSelection, m_EndSelection);
	int EndSel = std::max(m_StartSelection, m_EndSelection);

	m_SelectionX = StartSel - m_StartIndex;
	m_SelectionX = std::max(m_SelectionX, 0);
	int temp = m_SelectionX;

	m_SelectionWidth = (EndSel - m_StartIndex) - m_SelectionX;

	m_SelectionX = m_Font->CalculateWidth(m_Text.substr(m_StartIndex, m_SelectionX));
	m_SelectionWidth = m_Font->CalculateWidth(m_Text.substr(m_StartIndex + temp, m_SelectionWidth));

	m_SelectionX = std::max(m_SelectionX, 0);
	m_SelectionWidth = std::min(m_SelectionWidth, m_Width);
}

int GUITextPanel::GetStartOfNextCharacterGroup(const std::string_view& stringToCheck, int currentIndex) const {
	currentIndex = std::clamp(currentIndex, 0, static_cast<int>(stringToCheck.size()));
	auto classify = [&](std::size_t offset, std::uint32_t& codePoint) {
		std::size_t bytes = 1;
		UTF8::Decode(stringToCheck, offset, codePoint, bytes);
		const bool space = codePoint == ' ' || codePoint == '\t' || codePoint == '\n' || codePoint == '\r';
		const bool word = codePoint >= 0x80 || codePoint == '_' || (codePoint < 0x80 && std::isalnum(static_cast<unsigned char>(codePoint)));
		return std::pair<bool, bool>{word, space};
	};
	std::size_t cursor = static_cast<std::size_t>(currentIndex);
	if (cursor >= stringToCheck.size()) return static_cast<int>(cursor);
	std::uint32_t cp = 0;
	auto [word, space] = classify(cursor, cp);
	while (cursor < stringToCheck.size()) {
		std::uint32_t nextCp = 0;
		auto [nextWord, nextSpace] = classify(cursor, nextCp);
		if (cursor != static_cast<std::size_t>(currentIndex) && (nextSpace || nextWord != word)) break;
		cursor = UTF8::NextBoundary(stringToCheck, cursor);
	}
	while (cursor < stringToCheck.size()) {
		std::uint32_t nextCp = 0;
		std::size_t bytes = 1;
		UTF8::Decode(stringToCheck, cursor, nextCp, bytes);
		if (nextCp != ' ' && nextCp != '\t' && nextCp != '\n' && nextCp != '\r') break;
		cursor = UTF8::NextBoundary(stringToCheck, cursor);
	}
	return static_cast<int>(cursor);
}

int GUITextPanel::GetStartOfPreviousCharacterGroup(const std::string_view& stringToCheck, int currentIndex) const {
	currentIndex = std::clamp(currentIndex, 0, static_cast<int>(stringToCheck.size()));
	std::size_t cursor = static_cast<std::size_t>(currentIndex);
	if (cursor == 0) return 0;
	auto decodePrevious = [&](std::size_t end, std::uint32_t& cp) {
		const std::size_t start = UTF8::PreviousBoundary(stringToCheck, end);
		std::size_t bytes = 1;
		UTF8::Decode(stringToCheck, start, cp, bytes);
		return start;
	};
	std::uint32_t cp = 0;
	cursor = decodePrevious(cursor, cp);
	auto isSpace = [](std::uint32_t value) { return value == ' ' || value == '\t' || value == '\n' || value == '\r'; };
	auto isWord = [](std::uint32_t value) { return value >= 0x80 || value == '_' || (value < 0x80 && std::isalnum(static_cast<unsigned char>(value))); };
	while (isSpace(cp) && cursor > 0) cursor = decodePrevious(cursor, cp);
	const bool word = isWord(cp);
	while (cursor > 0) {
		const std::size_t previous = UTF8::PreviousBoundary(stringToCheck, cursor);
		std::uint32_t previousCp = 0;
		std::size_t bytes = 1;
		UTF8::Decode(stringToCheck, previous, previousCp, bytes);
		if (isSpace(previousCp) || isWord(previousCp) != word) break;
		cursor = previous;
	}
	return static_cast<int>(cursor);
}

void GUITextPanel::RemoveSelectionText() {
	if (!m_GotSelection) {
		return;
	}

	int Start = std::min(m_StartSelection, m_EndSelection);
	int End = std::max(m_StartSelection, m_EndSelection);

	if (Start == End) {
		return;
	}

	m_Text.erase(Start, End - Start);

	m_CursorIndex = Start;
	UpdateText(false);

	m_GotSelection = false;
}

void GUITextPanel::SetCursorPos(int cursorPos) {
	m_GotSelection = false;

	if (cursorPos <= 0) {
		cursorPos = 0;
	}
	if (cursorPos > static_cast<int>(m_Text.size())) {
		cursorPos = static_cast<int>(m_Text.size());
	}
	while (cursorPos > 0 && cursorPos < static_cast<int>(m_Text.size()) &&
	       (static_cast<unsigned char>(m_Text[cursorPos]) & 0xC0) == 0x80) {
		--cursorPos;
	}

	m_CursorIndex = cursorPos;

	UpdateText();
}

std::string GUITextPanel::GetSelectionText() const {
	if (!m_GotSelection) {
		return "";
	}
	int Start = std::min(m_StartSelection, m_EndSelection);
	int End = std::max(m_StartSelection, m_EndSelection);

	if (Start == End) {
		return "";
	}
	return m_Text.substr(Start, End - Start);
}

void GUITextPanel::SetText(const std::string& Text) {
	m_Text = Text;

	// Clear the selection
	ClearSelection();

	// Clear the cursor position
	m_CursorIndex = 0;
	m_StartIndex = 0;
	m_CursorX = 0;

	UpdateText(false, false);

	SendSignal(Changed, 0);
}

void GUITextPanel::SetRightText(const std::string& rightText) {
	m_RightText = rightText;
	SendSignal(Changed, 0);
}

void GUITextPanel::SetSelection(int Start, int End) {
	if (m_Locked) {
		return;
	}
	// Reset the selection
	m_GotSelection = false;

	DoSelection(Start, End);

	UpdateText(false, false);
}

int GUITextPanel::GetSelectionStart() const {
	// No selection?
	if (!m_GotSelection) {
		return -1;
	}

	return m_StartSelection;
}

int GUITextPanel::GetSelectionEnd() const {
	if (!m_GotSelection) {
		return -1;
	}
	return m_EndSelection;
}

void GUITextPanel::ClearSelection() {
	m_GotSelection = false;
}

void GUITextPanel::SetLocked(bool Locked) {
	m_Locked = Locked;

	// Clear the selection if we are now locked
	if (m_Locked) {
		ClearSelection();
	}
}

bool GUITextPanel::GetLocked() const {
	return m_Locked;
}
