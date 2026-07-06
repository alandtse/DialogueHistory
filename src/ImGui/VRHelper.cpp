#include "ImGui/VRHelper.h"

#include "ImGuiVRHelperClientSDK.h"
#include "Version.h"

namespace ImGui::Renderer::VR
{
	namespace
	{
		ImGuiVRHelperPluginAPI::Client g_local;   // conversation overlay (pointer-focus)
		ImGuiVRHelperPluginAPI::Client g_global;  // standalone archive (exclusive focus)

		ImGuiVRHelperPluginAPI::Client& ClientFor(Panel a_panel)
		{
			return a_panel == Panel::Local ? g_local : g_global;
		}
	}

	void Connect()
	{
		if (!REL::Module::IsVR()) {
			return;  // flat screen renders ImGui directly; no helper needed
		}
		namespace API = ImGuiVRHelperPluginAPI;
		// PointerFocus: coexist with the game's dialogue menu during a conversation — the
		// helper captures the wand only while the laser is on the overlay's panel, so dialogue
		// options stay selectable.
		// Deliberately NOT setting kClientFlag_OwnCursor: unset (the default) is what makes the
		// HELPER draw the wand pointer -- composited by the same code that computes the hit UV,
		// so it can't diverge from clicks the way this mod's own hand-drawn dot could. Setting
		// that flag means the opposite of what it sounds like: "I'll draw my own cursor, helper
		// stay out of my panel" -- which combined with this mod having removed its own
		// cursor-drawing code (see RenderMenuFrame) would mean no cursor renders at all.
		const bool local = g_local.Connect("DialogueHistory.Conversation", Version::NAME.data(),
			API::kClientFlag_RendersOnFocus | API::kClientFlag_PointerFocus);
		const bool global = g_global.Connect("DialogueHistory", Version::NAME.data(),
			API::kClientFlag_RendersOnFocus);

		// Called every VR frame to retry a handshake that raced the helper's listener at
		// kPostPostLoad (Client::Connect is a cheap no-op once registered), so log on state
		// change only -- not once for a permanent failure and never again if it later
		// recovers, and not every frame once connected.
		static bool everLogged = false;
		static bool prevFullyConnected = false;
		const bool fullyConnected = local && global;
		if (!everLogged || fullyConnected != prevFullyConnected) {
			if (fullyConnected) {
				logger::info("Connected to ImGuiVRHelper (conversation + archive panels)"sv);
			} else {
				logger::warn("ImGuiVRHelper not found; the dialogue history UI will not render in-headset"sv);
			}
			everLogged = true;
			prevFullyConnected = fullyConnected;
		}
	}

	bool IsConnected() { return g_local.IsConnected() && g_global.IsConnected(); }
	bool HasFocus(Panel a_panel) { return ClientFor(a_panel).HasFocus(); }
	void RequestFocus(Panel a_panel) { ClientFor(a_panel).RequestFocus(); }
	void ReleaseFocus(Panel a_panel) { ClientFor(a_panel).ReleaseFocus(); }
	void RequestReposition(Panel a_panel) { ClientFor(a_panel).RequestReposition(); }
	void PumpInput(Panel a_panel, bool a_active) { ClientFor(a_panel).PumpInput(a_active); }
	void PumpKeyboard(Panel a_panel) { ClientFor(a_panel).PumpKeyboard(); }
	// When not connected (flat screen / helper absent) RenderFrame() falls back to the normal
	// ImGui_ImplDX11 present, so the desktop path is unchanged.
	void RenderFrame(Panel a_panel) { ClientFor(a_panel).RenderFrame(); }

	bool GetPointer(Panel a_panel, float& a_u, float& a_v)
	{
		a_u = a_v = -1.0f;
		auto& client = ClientFor(a_panel);
		if (!client.IsConnected() || !client.Helper()) {
			return false;
		}
		return client.Helper()->GetPointer(client.Id(), &a_u, &a_v, nullptr);
	}

	bool ApplyPanelDisplaySize(Panel a_panel) { return ClientFor(a_panel).ApplyPanelDisplaySize(); }
}
