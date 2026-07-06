#pragma once

namespace ImGui
{
	void ExtendWindowPastBorder();

	void AlignForWidth(float width, float alignment = 0.5f);

	void CenteredText(const char* label, bool vertical);
	void TextColoredWrapped(const ImVec4& col, const char* fmt, ...);

	bool ToggleButton(const char* label, bool* v);

	bool IsItemSelected();

	// A real framed ImGui::Button (bounded background, needed in VR so the wand has a clear
	// click target -- unlike this mod's flat-only invisible-label buttons) needs
	// ImGuiCol_Button/Hovered/Active themed, or it inherits ImGui's stock blue on
	// hover/active and this mod's own Button color (opaque white, meant for icon tinting, not
	// a filled rect) at rest -- both clash with the dark theme. Reuses the same subtle overlay
	// already themed for hovered/selectable rows (Header/HeaderHovered/HeaderActive) so a
	// bounded VR button matches the rest of the mod's look. Pair with PopVRButtonStyle().
	void PushVRButtonStyle();
	void PopVRButtonStyle();

	void Spacing(std::uint32_t a_numSpaces);

	ImVec2 GetNativeViewportSize();
	ImVec2 GetNativeViewportPos();
	ImVec2 GetNativeViewportCenter();
}
