#include "SettingsGUI.h"
#include "WindowMan.h"
#include "SettingsMan.h"
#include "PerformanceMan.h"

#include "GUI.h"
#include "AllegroScreen.h"
#include "GUIInputWrapper.h"
#include "GUICollectionBox.h"
#include "GUIButton.h"
#include "GUITab.h"
#include "GUIComboBox.h"

#include <algorithm>
#include <vector>

using namespace RTE;

SettingsGUI::SettingsGUI(AllegroScreen* guiScreen, GUIInputWrapper* guiInput, bool createForPauseMenu) {
	m_GUIControlManager = std::make_unique<GUIControlManager>();
	RTEAssert(m_GUIControlManager->Create(guiScreen, guiInput, "Base.rte/GUIs/Skins/Menus", "MainMenuSubMenuSkin.ini"), "Failed to create GUI Control Manager and load it from Base.rte/GUIs/Skins/Menus/MainMenuSubMenuSkin.ini");
	m_GUIControlManager->Load(createForPauseMenu ? "Base.rte/GUIs/SettingsPauseGUI.ini" : "Base.rte/GUIs/SettingsGUI.ini");
	RTEAssert(m_GUIControlManager->Load("Base.rte/GUIs/SettingsPerformanceGUI.ini", true), "Failed to load Performance settings controls");

	int rootBoxMaxWidth = g_WindowMan.FullyCoversAllDisplays() ? g_WindowMan.GetPrimaryWindowDisplayWidth() / g_WindowMan.GetResMultiplier() : g_WindowMan.GetResX();

	GUICollectionBox* rootBox = dynamic_cast<GUICollectionBox*>(m_GUIControlManager->GetControl("root"));
	rootBox->Resize(rootBoxMaxWidth, g_WindowMan.GetResY());

	m_SettingsTabberBox = dynamic_cast<GUICollectionBox*>(m_GUIControlManager->GetControl("CollectionBoxSettingsBase"));
	m_SettingsTabberBox->SetPositionAbs((rootBox->GetWidth() - m_SettingsTabberBox->GetWidth()) / 2, 140);
	if (rootBox->GetHeight() < 540) {
		m_SettingsTabberBox->CenterInParent(true, true);
	}

	m_BackToMainButton = dynamic_cast<GUIButton*>(m_GUIControlManager->GetControl("ButtonBackToMainMenu"));
	m_BackToMainButton->SetPositionAbs((rootBox->GetWidth() - m_BackToMainButton->GetWidth()) / 2, m_SettingsTabberBox->GetYPos() + m_SettingsTabberBox->GetHeight() + 10);

	m_SettingsMenuTabs[SettingsMenuScreen::VideoSettingsMenu] = dynamic_cast<GUITab*>(m_GUIControlManager->GetControl("TabVideoSettings"));
	m_SettingsMenuTabs[SettingsMenuScreen::AudioSettingsMenu] = dynamic_cast<GUITab*>(m_GUIControlManager->GetControl("TabAudioSettings"));
	m_SettingsMenuTabs[SettingsMenuScreen::InputSettingsMenu] = dynamic_cast<GUITab*>(m_GUIControlManager->GetControl("TabInputSettings"));
	m_SettingsMenuTabs[SettingsMenuScreen::GameplaySettingsMenu] = dynamic_cast<GUITab*>(m_GUIControlManager->GetControl("TabGameplaySettings"));
	m_SettingsMenuTabs[SettingsMenuScreen::MiscSettingsMenu] = dynamic_cast<GUITab*>(m_GUIControlManager->GetControl("TabMiscSettings"));
	m_SettingsMenuTabs[SettingsMenuScreen::PerformanceSettingsMenu] = dynamic_cast<GUITab*>(m_GUIControlManager->GetControl("TabPerformanceSettings"));
	m_PerformanceSettingsBox = dynamic_cast<GUICollectionBox*>(m_GUIControlManager->GetControl("CollectionBoxPerformanceSettings"));
	const std::array<const char*, PerformanceControlCount> performanceControlNames = {
	    "Preset", "Overlay", "FPS", "VSync", "Particles", "Gibs", "DebrisLife", "ParticleLife", "MaxMO", "Background", "Screen", "Gore"};
	const std::array<std::vector<std::string>, PerformanceControlCount> performanceChoices = {{
	    {"Quality", "Balanced", "Performance", "Potato", "Custom"},
	    {"Off", "Basic", "Detailed"},
	    {"Unlimited", "30", "45", "60", "75", "90", "120", "144"},
	    {"Off", "On"},
	    {"Unlimited", "High", "Medium", "Low", "Very Low"},
	    {"Unlimited", "High", "Medium", "Low"},
	    {"Unlimited", "Long", "Normal", "Short"},
	    {"100%", "75%", "50%", "25%"},
	    {"Unlimited", "4000", "8000", "10000", "15000", "20000"},
	    {"High", "Medium", "Low", "Off"},
	    {"On", "Reduced", "Off"},
	    {"100%", "75%", "50%", "25%", "Off"}
	}};
	for (int index = 0; index < PerformanceControlCount; ++index) {
		m_PerformanceCombos[index] = dynamic_cast<GUIComboBox*>(m_GUIControlManager->GetControl(std::string("ComboPerformance") + performanceControlNames[index]));
		RTEAssert(m_PerformanceCombos[index], "Missing Performance settings combo box");
		for (const std::string& choice : performanceChoices[index]) m_PerformanceCombos[index]->AddItem(choice);
	}
	RefreshPerformanceControls();

	m_VideoSettingsMenu = std::make_unique<SettingsVideoGUI>(m_GUIControlManager.get());
	m_AudioSettingsMenu = std::make_unique<SettingsAudioGUI>(m_GUIControlManager.get());
	m_InputSettingsMenu = std::make_unique<SettingsInputGUI>(m_GUIControlManager.get());
	m_GameplaySettingsMenu = std::make_unique<SettingsGameplayGUI>(m_GUIControlManager.get());
	m_MiscSettingsMenu = std::make_unique<SettingsMiscGUI>(m_GUIControlManager.get());

	if (createForPauseMenu) {
		m_SettingsTabberBox->SetPositionAbs((rootBox->GetWidth() - m_SettingsTabberBox->GetWidth()) / 2, (rootBox->GetHeight() - m_SettingsTabberBox->GetHeight() - 30) / 2);
		m_BackToMainButton->SetPositionAbs((rootBox->GetWidth() - m_BackToMainButton->GetWidth()) / 2, m_SettingsTabberBox->GetYPos() + m_SettingsTabberBox->GetHeight() + 10);

		SetActiveSettingsMenuScreen(SettingsMenuScreen::GameplaySettingsMenu, false);
	} else {
		SetActiveSettingsMenuScreen(SettingsMenuScreen::VideoSettingsMenu, false);
	}
	m_SettingsMenuTabs[m_ActiveSettingsMenuScreen]->SetCheck(true);
}

GUICollectionBox* SettingsGUI::GetActiveDialogBox() const {
	GUICollectionBox* activeDialogBox = nullptr;
	switch (m_ActiveSettingsMenuScreen) {
		case SettingsMenuScreen::VideoSettingsMenu:
			activeDialogBox = m_VideoSettingsMenu->GetActiveDialogBox();
			break;
		case SettingsMenuScreen::InputSettingsMenu:
			activeDialogBox = m_InputSettingsMenu->GetActiveDialogBox();
			break;
		default:
			break;
	}
	DisableSettingsMenuNavigation(activeDialogBox);

	return activeDialogBox;
}

void SettingsGUI::CloseActiveDialogBox() const {
	switch (m_ActiveSettingsMenuScreen) {
		case SettingsMenuScreen::VideoSettingsMenu:
			m_VideoSettingsMenu->CloseActiveDialogBox();
			g_GUISound.BackButtonPressSound()->Play();
			break;
		case SettingsMenuScreen::InputSettingsMenu:
			m_InputSettingsMenu->CloseActiveDialogBox();
			g_GUISound.BackButtonPressSound()->Play();
			break;
		default:
			break;
	}
}

void SettingsGUI::DisableSettingsMenuNavigation(bool disable) const {
	m_BackToMainButton->SetEnabled(!disable);
	for (GUITab* settingsTabberTab: m_SettingsMenuTabs) {
		settingsTabberTab->SetEnabled(!disable);
	}
}

void SettingsGUI::RefreshPerformanceControls() {
	const std::array<int, 8> fpsValues = {0, 30, 45, 60, 75, 90, 120, 144};
	const std::array<int, 6> objectValues = {0, 4000, 8000, 10000, 15000, 20000};
	const std::array<int, 4> particleLifetimeValues = {100, 75, 50, 25};
	const std::array<int, 5> goreValues = {100, 75, 50, 25, 0};
	const auto findIndex = [](const auto& values, int value) {
		const auto found = std::find(values.begin(), values.end(), value);
		return found == values.end() ? 0 : static_cast<int>(found - values.begin());
	};
	const std::array<int, PerformanceControlCount> selections = {
	    g_SettingsMan.GetPerformancePreset(), g_PerformanceMan.GetOverlayLevel(), findIndex(fpsValues, g_SettingsMan.GetFPSLimit()),
	    g_WindowMan.GetVSyncEnabled() ? 1 : 0, g_SettingsMan.GetParticleLimitLevel(), g_SettingsMan.GetGibLimitLevel(),
	    g_SettingsMan.GetDebrisLifetimeLevel(), findIndex(particleLifetimeValues, g_SettingsMan.GetParticleLifetimePercent()),
	    findIndex(objectValues, g_SettingsMan.GetMaximumMovableObjects()), g_SettingsMan.GetBackgroundEffectsLevel(),
	    g_SettingsMan.GetScreenEffectsLevel(), findIndex(goreValues, g_SettingsMan.GetGoreDensityPercent())};
	for (int index = 0; index < PerformanceControlCount; ++index) {
		m_PerformanceCombos[index]->SetSelectedIndex(selections[index]);
		m_LastPerformanceSelections[index] = selections[index];
	}
}

void SettingsGUI::HandlePerformanceSelection(PerformanceControl control) {
	const int selected = m_PerformanceCombos[control]->GetSelectedIndex();
	if (selected < 0 || selected == m_LastPerformanceSelections[control]) return;
	m_LastPerformanceSelections[control] = selected;
	switch (control) {
		case Preset: g_SettingsMan.SetPerformancePreset(selected); break;
		case Overlay: g_PerformanceMan.SetOverlayLevel(selected); break;
		case FPSLimit: {
			const std::array<int, 8> values = {0, 30, 45, 60, 75, 90, 120, 144};
			g_SettingsMan.SetFPSLimit(values.at(selected));
			break;
		}
		case VSync: g_WindowMan.SetVSyncEnabled(selected != 0); break;
		case Particles: g_SettingsMan.SetParticleLimitLevel(selected); break;
		case Gibs: g_SettingsMan.SetGibLimitLevel(selected); break;
		case DebrisLife: g_SettingsMan.SetDebrisLifetimeLevel(selected); break;
		case ParticleLife: g_SettingsMan.SetParticleLifetimePercent(100 - selected * 25); break;
		case MaxMO: {
			const std::array<int, 6> values = {0, 4000, 8000, 10000, 15000, 20000};
			g_SettingsMan.SetMaximumMovableObjects(values.at(selected));
			break;
		}
		case Background: g_SettingsMan.SetBackgroundEffectsLevel(selected); break;
		case Screen: g_SettingsMan.SetScreenEffectsLevel(selected); break;
		case Gore: g_SettingsMan.SetGoreDensityPercent(100 - selected * 25); break;
		default: break;
	}
	RefreshPerformanceControls();
	g_SettingsMan.UpdateSettingsFile();
}

void SettingsGUI::SetActiveSettingsMenuScreen(SettingsMenuScreen activeMenu, bool playButtonPressSound) {
	m_VideoSettingsMenu->SetEnabled(false);
	m_AudioSettingsMenu->SetEnabled(false);
	m_InputSettingsMenu->SetEnabled(false);
	m_GameplaySettingsMenu->SetEnabled(false);
	m_MiscSettingsMenu->SetEnabled(false);
	m_PerformanceSettingsBox->SetVisible(false);
	m_PerformanceSettingsBox->SetEnabled(false);

	switch (activeMenu) {
		case SettingsMenuScreen::VideoSettingsMenu:
			m_VideoSettingsMenu->SetEnabled(true);
			break;
		case SettingsMenuScreen::AudioSettingsMenu:
			m_AudioSettingsMenu->SetEnabled(true);
			break;
		case SettingsMenuScreen::InputSettingsMenu:
			m_InputSettingsMenu->SetEnabled(true);
			break;
		case SettingsMenuScreen::GameplaySettingsMenu:
			m_GameplaySettingsMenu->SetEnabled(true);
			break;
		case SettingsMenuScreen::MiscSettingsMenu:
			m_MiscSettingsMenu->SetEnabled(true);
			break;
		case SettingsMenuScreen::PerformanceSettingsMenu:
			RefreshPerformanceControls();
			m_PerformanceSettingsBox->SetVisible(true);
			m_PerformanceSettingsBox->SetEnabled(true);
			break;
		default:
			RTEAbort("Invalid settings menu passed to SettingsGUI::SetActiveSettingsMenuScreen!");
			break;
	}
	m_ActiveSettingsMenuScreen = activeMenu;
	// Remove focus so the tab hovered graphic is removed after being pressed, otherwise it remains stuck on the active tab.
	m_GUIControlManager->GetManager()->SetFocus(nullptr);

	if (playButtonPressSound) {
		g_GUISound.BackButtonPressSound()->Play();
	}
}

bool SettingsGUI::HandleInputEvents() {
	m_GUIControlManager->Update();

	GUIEvent guiEvent;
	while (m_GUIControlManager->GetEvent(&guiEvent)) {
		if (guiEvent.GetType() == GUIEvent::Command) {
			if (guiEvent.GetControl() == m_BackToMainButton) {
				RefreshActiveSettingsMenuScreen();
				return true;
			}
		} else if (guiEvent.GetType() == GUIEvent::Notification) {
			if ((guiEvent.GetMsg() == GUIButton::Focused) && dynamic_cast<GUIButton*>(guiEvent.GetControl()) || (guiEvent.GetMsg() == GUITab::Hovered && dynamic_cast<GUITab*>(guiEvent.GetControl()))) {
				g_GUISound.SelectionChangeSound()->Play();
			}

			if (guiEvent.GetMsg() == GUITab::UnPushed) {
				if (guiEvent.GetControl() == m_SettingsMenuTabs[SettingsMenuScreen::VideoSettingsMenu]) {
					SetActiveSettingsMenuScreen(SettingsMenuScreen::VideoSettingsMenu);
				} else if (guiEvent.GetControl() == m_SettingsMenuTabs[SettingsMenuScreen::AudioSettingsMenu]) {
					SetActiveSettingsMenuScreen(SettingsMenuScreen::AudioSettingsMenu);
				} else if (guiEvent.GetControl() == m_SettingsMenuTabs[SettingsMenuScreen::InputSettingsMenu]) {
					SetActiveSettingsMenuScreen(SettingsMenuScreen::InputSettingsMenu);
				} else if (guiEvent.GetControl() == m_SettingsMenuTabs[SettingsMenuScreen::GameplaySettingsMenu]) {
					SetActiveSettingsMenuScreen(SettingsMenuScreen::GameplaySettingsMenu);
				} else if (guiEvent.GetControl() == m_SettingsMenuTabs[SettingsMenuScreen::MiscSettingsMenu]) {
					SetActiveSettingsMenuScreen(SettingsMenuScreen::MiscSettingsMenu);
				} else if (guiEvent.GetControl() == m_SettingsMenuTabs[SettingsMenuScreen::PerformanceSettingsMenu]) {
					SetActiveSettingsMenuScreen(SettingsMenuScreen::PerformanceSettingsMenu);
				}
			}
		}
		switch (m_ActiveSettingsMenuScreen) {
			case SettingsMenuScreen::VideoSettingsMenu:
				m_VideoSettingsMenu->HandleInputEvents(guiEvent);
				break;
			case SettingsMenuScreen::AudioSettingsMenu:
				m_AudioSettingsMenu->HandleInputEvents(guiEvent);
				break;
			case SettingsMenuScreen::InputSettingsMenu:
				m_InputSettingsMenu->HandleInputEvents(guiEvent);
				break;
			case SettingsMenuScreen::GameplaySettingsMenu:
				m_GameplaySettingsMenu->HandleInputEvents(guiEvent);
				break;
			case SettingsMenuScreen::MiscSettingsMenu:
				m_MiscSettingsMenu->HandleInputEvents(guiEvent);
				break;
			case SettingsMenuScreen::PerformanceSettingsMenu:
				if (guiEvent.GetType() == GUIEvent::Notification && guiEvent.GetMsg() == GUIComboBox::Closed) {
					for (int index = 0; index < PerformanceControlCount; ++index) {
						if (guiEvent.GetControl() == m_PerformanceCombos[index]) HandlePerformanceSelection(static_cast<PerformanceControl>(index));
					}
				}
				break;
			default:
				RTEAbort("Trying to handle input events for an invalid settings menu in SettingsGUI::HandleInputEvents!");
				break;
		}
	}
	// Manual input config sequence has to be updated outside the GUI event handling loop otherwise input capture doesn't work (loop only runs if event queue isn't empty).
	if (m_ActiveSettingsMenuScreen == SettingsMenuScreen::InputSettingsMenu) {
		if (m_InputSettingsMenu->InputMappingConfigIsConfiguringManually()) {
			m_InputSettingsMenu->HandleMappingConfigManualConfiguration();
		} else if (m_InputSettingsMenu->InputConfigWizardIsConfiguringManually()) {
			m_InputSettingsMenu->HandleConfigWizardManualConfiguration();
		} else if (m_InputSettingsMenu->InputConfigIsConfiguringDevice()) {
			m_InputSettingsMenu->HandleConfigDeviceMapping();
		}
	}
	return false;
}

void SettingsGUI::Draw() const {
	m_GUIControlManager->Draw();
	m_GUIControlManager->DrawMouse();
}
