#pragma once

// Bridges DialogueHistory's ImGui output to the ImGuiVRHelper plugin so the menus render
// inside the headset and receive controller input on the VR build. On SE/AE these are
// no-ops / a plain DX11 present, leaving the flat-screen path unchanged.
//
// Two helper clients, one per surface — they're never shown at the same time, so a single
// ImGui context is rendered to whichever panel is active:
//   - Local  : the in-conversation history overlay. Pointer-focus — it coexists with the
//              game's dialogue menu, grabbing the wand only while the laser is on its panel,
//              so dialogue options stay selectable.
//   - Global : the standalone history archive. Ordinary exclusive focus, like any menu, so it
//              isn't blocked by the game when the laser points off its panel.
namespace ImGui::Renderer::VR
{
	enum class Panel
	{
		Local,
		Global
	};

	// kPostPostLoad handshake — registers both clients.
	void Connect();
	bool IsConnected();

	// True when the helper has routed in-scene focus to this panel's client this frame.
	bool HasFocus(Panel a_panel);

	// The helper's current wand-pointer position (0..1 panel UV) for this panel; false when
	// the laser isn't on it.
	bool GetPointer(Panel a_panel, float& a_u, float& a_v);

	// Sizes the current ImGui context's canvas (io.DisplaySize/DisplayFramebufferScale) to this
	// panel's EXACT pixel dimensions -- not merely its aspect. Must be called before NewFrame.
	// Delegates to the client SDK's own ApplyPanelDisplaySize(), so this mod can't drift out of
	// sync with how the helper computes it. False when not connected / panel not yet allocated
	// (caller's existing io.DisplaySize is left untouched).
	bool ApplyPanelDisplaySize(Panel a_panel);

	// Take/release the helper's interactive overlay for this panel.
	void RequestFocus(Panel a_panel);
	void ReleaseFocus(Panel a_panel);

	// Call every frame a reposition drag should stay active for this panel — e.g. while a
	// wand-clickable "Move" button is held (ImGui::IsItemActive() after drawing it). A heartbeat,
	// not a toggle: stop calling to end the drag. No-op against a helper older than this (v1.5.4
	// baseline predates it; needs the client-driven-reposition follow-up), or when this panel
	// isn't the one currently focused.
	void RequestReposition(Panel a_panel);

	// Pump VR controller input (wand cursor, buttons, scroll) into ImGui for this panel.
	void PumpInput(Panel a_panel, bool a_active);
	// Drive the VR keyboard for this panel's focused text field (the search box).
	void PumpKeyboard(Panel a_panel);
	// Per-frame output: blit to this panel's VR surface when connected, else the normal
	// in-game ImGui_ImplDX11 present. Call after ImGui::Render().
	void RenderFrame(Panel a_panel);
}
