#include "LocalHistory.h"

#include "GlobalHistory.h"
#include "Hotkeys.h"
#include "ImGui/Renderer.h"
#include "ImGui/Styles.h"
#include "ImGui/VRHelper.h"

namespace LocalHistory
{
	void Manager::Register()
	{
		RE::UI::GetSingleton()->AddEventSink<RE::MenuOpenCloseEvent>(GetSingleton());
	}

	void Manager::LoadMCMSettings(const CSimpleIniA& a_ini)
	{
		unpauseMenu = a_ini.GetBoolValue("Settings", "bUnpauseLocalHistory", unpauseMenu);
		blurMenu = a_ini.GetBoolValue("Settings", "bBlurLocalHistory", blurMenu);
		hideButton = a_ini.GetBoolValue("Settings", "bHideButtonLocalHistory", hideButton);
	}

	void Manager::Draw()
	{
		if (!ShouldDraw()) {
			return;
		}

		ImGui::SetNextWindowPos(ImGui::GetNativeViewportPos());
		ImGui::SetNextWindowSize(ImGui::GetNativeViewportSize());

		// VR: no explicit open toggle -- there's room to just show the full history by default;
		// vrHistoryVisible tracks whether the player minimized it instead.
		auto localHistoryOpen = stl::IsVR() ? vrHistoryVisible : IsLocalHistoryOpen();

		ImGui::Begin("##Main", nullptr, ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBringToFrontOnFocus);
		{
			auto [localHistoryFont, localHistoryFontSize] = MANAGER(IconFont)->GetLocalHistoryFont();
			ImGui::PushFont(localHistoryFont, localHistoryFontSize);

			if (localHistoryOpen) {
				ImGui::SetNextWindowPos(ImGui::GetNativeViewportCenter(), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
				ImGui::SetNextWindowSize(ImGui::GetNativeViewportSize() * 0.8f);

				ImGui::Begin("##LocalHistory", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);
				{
					ImGui::ExtendWindowPastBorder();

					if (stl::IsVR()) {
						auto [buttonFont, buttonFontSize] = MANAGER(IconFont)->GetButtonFont();
						ImGui::PushFont(buttonFont, buttonFontSize);
						ImGui::PushVRButtonStyle();

						// The dialogue menu's own grip binding closes it, so the helper's normal
						// off-panel grip-drag can't be used to reposition this panel during a live
						// conversation -- hold trigger on this button instead (IsItemActive spans
						// the whole hold, even once the wand ray drifts off it as the user moves
						// their hand to drag).
						ImGui::Button("$DH_Move_Button"_T);
						if (ImGui::IsItemActive()) {
							ImGui::Renderer::VR::RequestReposition(ImGui::Renderer::VR::Panel::Local);
						}

						// Flat screen can jump from a live conversation into the full searchable
						// archive via the Global hotkey alone (GlobalHistory::IsValid() doesn't
						// block the dialogue menu) -- this is the wand-clickable equivalent, since
						// VR has no keyboard-driven path.
						ImGui::SameLine();
						if (ImGui::Button("$DH_Title_Conversation"_T)) {
							MANAGER(GlobalHistory)->SetDrawConversation(true);
							MANAGER(GlobalHistory)->SetGlobalHistoryOpen(true);
						}

						// Minimize: a small corner glyph (like a desktop window's minimize
						// button) rather than a labeled button competing with Move/Search --
						// top-right of the panel it hides, not a separate floating element
						// elsewhere on screen the player has to go hunting for.
						if (!hideButton) {
							const float glyphWidth = ImGui::CalcTextSize("_").x + ImGui::GetStyle().FramePadding.x * 2;
							ImGui::SameLine();
							ImGui::AlignForWidth(glyphWidth, 1.0f);
							if (ImGui::Button("_##Minimize")) {
								vrHistoryVisible = false;
							}
						}

						ImGui::PopVRButtonStyle();
						ImGui::PopFont();
					}

					auto [headerFont, headerFontSize] = MANAGER(IconFont)->GetHeaderFont();
					ImGui::PushFont(headerFont, headerFontSize);
					{
						ImGui::CenteredText(gameTimeString.c_str(), false);
					}
					ImGui::PopFont();

					ImGui::Spacing(2);
					ImGui::SeparatorEx(ImGuiSeparatorFlags_Horizontal, ImGui::GetUserStyleVar(ImGui::USER_STYLE::kSeparatorThickness));
					ImGui::Spacing(2);

					localDialogue.Draw();
				}
				ImGui::End();
			}

			ImGui::PopFont();

			if (!hideButton) {
				auto [buttonFont, buttonFontSize] = MANAGER(IconFont)->GetButtonFont();
				ImGui::PushFont(buttonFont, buttonFontSize);
				if (stl::IsVR()) {
					// Reopen: only needed when minimized -- the giant panel (and its own
					// minimize glyph) is gone, so this is the sole way back in. Centered like
					// the rest of this mod's VR buttons, not the flat branch's desktop-corner
					// coordinate below (tuned for an invitation button beside the game's own
					// dialogue UI, not a giant centered panel).
					if (!localHistoryOpen) {
						const float width = ImGui::CalcTextSize("$DH_Title"_T).x + ImGui::GetStyle().FramePadding.x * 2;
						ImGui::PushVRButtonStyle();
						ImGui::AlignForWidth(width);
						ImGui::SetCursorPosY(ImGui::GetNativeViewportSize().y * 0.9f);
						if (ImGui::Button("$DH_Title"_T)) {
							vrHistoryVisible = true;
						}
						ImGui::PopVRButtonStyle();
					}
				} else {
					const auto& icons = MANAGER(Hotkeys)->LocalHistoryIcons();

					// exit button position (1784,1015) + offset (32) at 1080p. Uses the current ImGui
					// viewport (tracks io.DisplaySize), not the game's raw screen size.
					const auto windowSize = ImGui::GetNativeViewportSize();
					float      posY = 0.93981481481f * windowSize.y;
					float      posX;
					if (!localHistoryOpen) {
						// calculate position backwards
						float        posX_exitButtonPos = 0.9125f * windowSize.x;
						static float textSize = ImGui::CalcTextSize("$DH_Title"_T).x;
						static float innerSpacing = ImGui::GetStyle().ItemInnerSpacing.x * 0.40f;

						posX = posX_exitButtonPos;
						posX -= textSize;
						posX -= innerSpacing;
						for (auto& icon : icons) {
							posX -= icon->size.x;
						}

					} else {
						posX = 0.92916666666f * windowSize.x;
						if (icons.size() > 1) {
							posX -= (*icons.begin())->size.x;
						}
					}

					ImGui::SetCursorScreenPos({ posX, posY });
					ImGui::ButtonIconWithLabel(localHistoryOpen ? "$DH_Exit_Button"_T : "$DH_Title"_T, icons);
					if (ImGui::IsItemSelected() && localHistoryOpen) {
						// no dialogue menu click because the real menu registers as a click too
						SetLocalHistoryOpen(false);
					}
				}
				ImGui::PopFont();
			}
		}
		ImGui::End();
	}

	bool Manager::ShouldDraw()
	{
		return IsDialogueMenuOpen() && !ShouldHide();
	}

	bool Manager::IsDialogueMenuOpen() const
	{
		return dialogueMenuOpen;
	}

	bool Manager::ShouldHide()
	{
		static constexpr std::array badMenus{
			// if hitting esc
			RE::JournalMenu::MENU_NAME,
			// accessible via dialogue
			RE::BarterMenu::MENU_NAME,
			RE::GiftMenu::MENU_NAME,
			RE::ContainerMenu::MENU_NAME,
			RE::TrainingMenu::MENU_NAME,
			// console
			RE::Console::MENU_NAME
		};

		const auto UI = RE::UI::GetSingleton();
		if (!UI) {
			return true;
		}
		const auto badMenu = std::ranges::find_if(badMenus, [&](const auto& menuName) { return UI->IsMenuOpen(menuName); });
		const bool hasBadMenu = badMenu != badMenus.end();
		// IsShowingMenus() reflects the flat 2D HUD/menu layer's visibility flag. In VR it goes
		// false during ordinary dialogue (VR's DialogueMenu isn't part of that 2D layer), so this
		// check only makes sense on flat screen; skip it in VR rather than mistake it for a
		// "HUD hidden" state (screenshot mode etc).
		const bool notShowingMenus = !stl::IsVR() && !UI->IsShowingMenus();
		if (hasBadMenu || notShowingMenus) {
			return true;
		}

		return false;
	}

	bool Manager::IsLocalHistoryOpen() const
	{
		return localHistoryMenuOpen;
	}

	void Manager::ToggleActive()
	{
		if (!IsDialogueMenuOpen()) {
			return;
		}

		// VR: mirror the wand-clickable minimize/reopen button (flip vrHistoryVisible) rather
		// than SetLocalHistoryOpen, which pokes flat-only DialogueMenu Scaleform variables.
		if (stl::IsVR()) {
			vrHistoryVisible = !vrHistoryVisible;
		} else {
			SetLocalHistoryOpen(!IsLocalHistoryOpen());
		}
	}

	void Manager::SetDialogueMenuOpen(bool a_opened)
	{
		dialogueMenuOpen = a_opened;

		if (!dialogueMenuOpen) {
			if (ShouldHide()) {  // if dialogue menu is temporarily interrupted by inventory, console etc
				tempClosed = true;
				return;
			}
			if (IsLocalHistoryOpen()) {
				SetLocalHistoryOpen(false);
			}
			SaveDialogueHistory();
			localDialogue.Clear();
			tempClosed = false;
		} else {
			UpdateDialogue();
			ImGui::Styles::GetSingleton()->RefreshStyle();
			vrHistoryVisible = true;  // show expanded by default for each new conversation (VR only)
		}

		ImGui::Renderer::RenderMenus(a_opened);
	}

	void Manager::SetLocalHistoryOpen(bool a_opened)
	{
		localHistoryMenuOpen = a_opened;
		SetupLocalHistoryMenu(localHistoryMenuOpen);
	}

	void Manager::SetupLocalHistoryMenu(bool a_opened, bool a_blurBG)
	{
		if (a_blurBG) {
			a_blurBG = blurMenu;
		}

		if (a_opened) {
			RE::PlaySound("UIMenuOK");
			if (a_blurBG) {
				RE::UIBlurManager::GetSingleton()->IncrementBlurCount();
			}
		} else {
			if (a_blurBG) {
				RE::UIBlurManager::GetSingleton()->DecrementBlurCount();
			}
			RE::PlaySound("UIMenuCancel");
		}

		if (auto dialogueMenu = RE::UI::GetSingleton()->GetMenu<RE::DialogueMenu>()) {
			if (auto& movie = dialogueMenu->uiMovie) {
				// hiding TopicListHolder causes dialogue menu to show defaults even when reset
				movie->SetVariable("_root.DialogueMenu_mc.TopicListHolder.PanelCopy_mc._visible", !a_opened);
				movie->SetVariable("_root.DialogueMenu_mc.TopicListHolder.ListPanel._visible", !a_opened);
				movie->SetVariable("_root.DialogueMenu_mc.TopicListHolder.TextCopy_mc._visible", !a_opened);
				movie->SetVariable("_root.DialogueMenu_mc.TopicListHolder.List_mc._visible", !a_opened);
				movie->SetVariable("_root.DialogueMenu_mc.TopicListHolder.ScrollIndicators._visible", !a_opened);

				movie->SetVariable("_root.DialogueMenu_mc.ExitButton._visible", !a_opened);
				movie->SetVariable("_root.DialogueMenu_mc.SubtitleText._visible", !a_opened);
				movie->SetVariable("_root.DialogueMenu_mc.SpeakerName._visible", !a_opened);
			}
		}

		if (!unpauseMenu) {
			MAIN_DATA(RE::Main::GetSingleton()).freezeTime = a_opened;
		}
	}

	void Manager::AddDialogue(RE::TESObjectREFR* a_speaker, const std::string& a_response, const std::string& a_voice)
	{
		if (!a_speaker->IsPlayerRef()) {
			if (!currentSpeaker) {
				currentSpeaker = a_speaker;
			}
			if (currentSpeaker != a_speaker) {
				return;
			}
		}

		localDialogue.AddDialogue(a_speaker, a_response, a_voice);

		// erase duplicate opening lines
		if (auto& dialogue = localDialogue.dialogue; dialogue.size() == 2 &&
													 dialogue[0].line == dialogue[1].line &&
													 dialogue[0].voice == dialogue[1].voice) {
			dialogue.erase(dialogue.begin());
		}
	}

	void Manager::SaveDialogueHistory()
	{
		if (localDialogue.empty()) {
			return;
		}

		MANAGER(GlobalHistory)->SaveDialogueHistory(gameTime, localDialogue);
	}

	void Manager::UpdateDialogue()
	{
		auto calendar = RE::Calendar::GetSingleton();

		auto target = RE::MenuTopicManager::GetSingleton()->speaker.get();
		if (target) {
			currentSpeaker = target.get();
		} else {
			currentSpeaker = nullptr;
		}
		localDialogue.Initialize(currentSpeaker);

		char timeStamp[MAX_PATH]{};
		calendar->GetTimeDateString(timeStamp, MAX_PATH, false);
		gameTimeString = timeStamp;

		gameTime = calendar->GetTime();
		localDialogue.Initialize(gameTime);

		localDialogue.RefreshContents();
	}

	RE::BSEventNotifyControl Manager::ProcessEvent(const RE::MenuOpenCloseEvent* a_evn, RE::BSTEventSource<RE::MenuOpenCloseEvent>*)
	{
		if (!a_evn) {
			return EventResult::kContinue;
		}

		if (a_evn->menuName == RE::DialogueMenu::MENU_NAME) {
			SetDialogueMenuOpen(a_evn->opening);
		} else if (a_evn->menuName == RE::JournalMenu::MENU_NAME) {
			if (IsLocalHistoryOpen()) {
				SetupLocalHistoryMenu(!a_evn->opening, false);
			}
		} else if (a_evn->opening) {
			switch (string::const_hash(a_evn->menuName)) {
			case string::const_hash(RE::MainMenu::MENU_NAME):
			case string::const_hash(RE::LoadingMenu::MENU_NAME):
			case "CustomMenu"_h:
				{
					if (IsDialogueMenuOpen()) {
						SetDialogueMenuOpen(false);
					}
				}
				break;
			default:
				break;
			}
		}

		return EventResult::kContinue;
	}
}
