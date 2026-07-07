#include "Renderer.h"

#include "GlobalHistory.h"
#include "IconsFonts.h"
#include "Input.h"
#include "LocalHistory.h"
#include "Styles.h"
#include "VRHelper.h"

namespace ImGui::Renderer
{
	struct WndProc
	{
		static LRESULT thunk(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
		{
			auto& io = ImGui::GetIO();
			if (uMsg == WM_KILLFOCUS) {
				io.ClearInputKeys();
			}

			return func(hWnd, uMsg, wParam, lParam);
		}
		static inline WNDPROC func;
	};

	void Connect()
	{
		VR::Connect();  // ImGuiVRHelper handshake; no-op on flat builds
	}

	struct CreateD3DAndSwapChain
	{
		static void thunk()
		{
			func();

			if (const auto renderer = RE::BSGraphics::Renderer::GetSingleton()) {
				const auto swapChain = (IDXGISwapChain*)RENDERER_DATA(renderer).renderWindows[0].swapChain;
				if (!swapChain) {
					logger::error("couldn't find swapChain");
					return;
				}

				DXGI_SWAP_CHAIN_DESC desc{};
				if (FAILED(swapChain->GetDesc(std::addressof(desc)))) {
					logger::error("IDXGISwapChain::GetDesc failed.");
					return;
				}

				const auto device = (ID3D11Device*)RENDERER_DATA(renderer).forwarder;
				const auto context = (ID3D11DeviceContext*)RENDERER_DATA(renderer).context;

				logger::info("Initializing ImGui..."sv);

				ImGui::CreateContext();

				auto& io = ImGui::GetIO();
				io.ConfigFlags = ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_NavEnableGamepad | ImGuiConfigFlags_NoMouseCursorChange;
				io.IniFilename = nullptr;

				if (!SKSE::ImGui_ImplWin32_Init(desc.OutputWindow)) {
					logger::error("ImGui initialization failed (Win32)");
					return;
				}
				if (!ImGui_ImplDX11_Init(device, context)) {
					logger::error("ImGui initialization failed (DX11)"sv);
					return;
				}

				logger::info("ImGui initialized.");

				MANAGER(IconFont)->LoadIcons();

				initialized.store(true);

				WndProc::func = reinterpret_cast<WNDPROC>(
					SetWindowLongPtrA(
						desc.OutputWindow,
						GWLP_WNDPROC,
						reinterpret_cast<LONG_PTR>(WndProc::thunk)));
				if (!WndProc::func) {
					logger::error("SetWindowLongPtrA failed!");
				}
			}
		}
		static inline REL::Relocation<decltype(thunk)> func;
	};

	// Build one ImGui frame for the history surfaces and present it. On flat (and when the VR
	// helper is absent) VR::RenderFrame() falls back to the normal in-game DX11 present; when
	// connected it blits to the headset panel.
	void RenderMenuFrame(VR::Panel a_panel, bool a_pumpWand = false)
	{
		// refresh style
		ImGui::Styles::GetSingleton()->OnStyleRefresh();

		ImGui_ImplDX11_NewFrame();
		SKSE::ImGui_ImplWin32_NewFrame();
		{
			//trick imgui into rendering at game's real resolution (ie. if upscaled with Display Tweaks)
			static const auto screenSize = RE::BSGraphics::Renderer::GetScreenSize();

			auto& io = ImGui::GetIO();
			io.DisplaySize.x = (float)screenSize.width;
			io.DisplaySize.y = (float)screenSize.height;

			// VR: the canvas must equal the panel's EXACT pixel size, not merely its aspect --
			// the stock DX11 backend renders draw data into a DisplaySize-sized viewport
			// anchored at the panel's top-left (it overrides the blit's panel viewport), while
			// the helper's wand hit-test UV and cursor marker span the full panel. Delegates to
			// the client SDK's own ApplyPanelDisplaySize() rather than hand-deriving this from
			// GetPanelSize, so this can't drift out of sync with how the helper computes it;
			// leaves io.DisplaySize at the flat value above if not yet connected/allocated.
			if (stl::IsVR()) {
				VR::ApplyPanelDisplaySize(a_panel);
			}
		}
		// Inject the VR wand AFTER the Win32 backend, or the desktop cursor (off-screen in VR)
		// would be queued last and win, leaving our cursor off-panel. Must run after
		// io.DisplaySize is finalized above, since PumpInput maps the wand's panel UV through
		// it. Force the software cursor so the user can see where the laser is hovering once
		// it's on the panel.
		if (a_pumpWand) {
			VR::PumpInput(a_panel, true);
			ImGui::GetIO().MouseDrawCursor = true;
		}
		ImGui::NewFrame();
		{
			// disable windowing
			GImGui->NavWindowingTarget = nullptr;

			// VR: draw only the manager that owns a_panel -- PresentVR already decided which
			// one owns this frame. Flat has one screen (not two panel textures), so both draw.
			if (!stl::IsVR() || a_panel == VR::Panel::Local) {
				MANAGER(LocalHistory)->Draw();
			}
			if (!stl::IsVR() || a_panel == VR::Panel::Global) {
				MANAGER(GlobalHistory)->Draw();
			}
		}
		// The helper composites its own wand marker over the panel — drawn by the same
		// code that computes the hit UV, so it can't diverge from clicks now that the
		// canvas is 1:1 panel pixels (the old marker-vs-content disagreement was the
		// canvas/panel size mismatch). Suppress ImGui's software cursor so a second,
		// tiny pointer isn't drawn under it.
		if (stl::IsVR()) {
			ImGui::GetIO().MouseDrawCursor = false;
		}
		ImGui::EndFrame();
		ImGui::Render();
		VR::RenderFrame(a_panel);
	}

	// Flat-screen render driver. IMenu::PostDisplay fires while the HUD draws, which is enough
	// on desktop. On VR this hook is an unreliable per-frame driver (it stalls once a menu /
	// the helper overlay owns the screen and while time is frozen), so VR renders from the
	// always-on PresentVR hook below instead.
	struct PostDisplay
	{
		static void thunk(RE::IMenu* a_menu)
		{
			if (initialized.load() && !stl::IsVR() && renderMenus.load()) {
				RenderMenuFrame(VR::Panel::Global);  // flat: panel arg unused (DX11 present)
			}

			func(a_menu);
		}
		static inline REL::Relocation<decltype(thunk)> func;
		static inline std::size_t                      idx{ 0x6 };
	};

	// VR render/input driver — BSGraphics::Renderer::End runs every frame regardless of menu
	// state or frozen time, so the interactive overlay keeps rendering and receiving wand input
	// while the archive is open. No-op on flat / when the helper isn't connected.
	struct PresentVR
	{
		static void thunk(std::uint32_t a_timer)
		{
			func(a_timer);

			if (!initialized.load() || !stl::IsVR()) {
				return;
			}
			// Retryable, near-free once both clients are registered (see Client::Connect) --
			// covers a handshake that raced the helper's listener at kPostPostLoad, so one
			// panel's client can still come up instead of leaving VR permanently half-connected.
			VR::Connect();
			if (!VR::IsConnected()) {
				return;
			}

			const auto global = MANAGER(GlobalHistory);
			const auto local = MANAGER(LocalHistory);

			// Mirror the helper shell's focus to Global's open-state both ways, but only adopt a
			// shell-granted focus when nothing is actually shown (so Local's own focus can't pop
			// Global) -- a minimized Local panel doesn't count as "shown", so cycling to Global
			// during a backgrounded conversation still opens the archive instead of snapping back
			// to Local's reopen button.
			static bool prevGlobalFocus = false;
			const bool  globalFocus = VR::HasFocus(VR::Panel::Global);
			const bool  localShown = local->IsDialogueMenuOpen() && local->IsVRHistoryVisible();
			if (globalFocus && !prevGlobalFocus && !global->IsGlobalHistoryOpen() && !localShown) {
				// Opened via the helper's own cycle/quick-select (no in-game hotkey involved) —
				// default to the Conversation History tab rather than flat's Dialogue History
				// default, since that's the more useful quick-glance view in VR.
				global->SetDrawConversation(true);
				global->SetGlobalHistoryOpen(true);
			} else if (!globalFocus && prevGlobalFocus && global->IsGlobalHistoryOpen()) {
				global->SetGlobalHistoryOpen(false);
			}

			prevGlobalFocus = globalFocus;

			if (global->IsGlobalHistoryOpen()) {
				// Exclusive: hold focus and take all wand input, like any standalone menu, so it
				// isn't blocked by the game when the laser points off the panel.
				if (!VR::HasFocus(VR::Panel::Global)) {
					VR::RequestFocus(VR::Panel::Global);
				}
				VR::PumpKeyboard(VR::Panel::Global);
				RenderMenuFrame(VR::Panel::Global, true);
			} else if (local->IsDialogueMenuOpen()) {
				// Pointer-focus: coexist with the dialogue menu — only grab the wand while the
				// laser is on our panel (otherwise the game keeps it so dialogue options work).
				if (!VR::HasFocus(VR::Panel::Local)) {
					VR::RequestFocus(VR::Panel::Local);
				}
				float      u = 0.0f, v = 0.0f;
				const bool onPanel = VR::GetPointer(VR::Panel::Local, u, v);
				VR::PumpKeyboard(VR::Panel::Local);
				if (!onPanel) {
					VR::PumpInput(VR::Panel::Local, false);  // keep edges; let the game keep the wand
				}
				RenderMenuFrame(VR::Panel::Local, onPanel);
			} else {
				// Nothing shown: drop focus and keep both clients' button edges current.
				if (VR::HasFocus(VR::Panel::Global)) {
					VR::ReleaseFocus(VR::Panel::Global);
				}
				if (VR::HasFocus(VR::Panel::Local)) {
					VR::ReleaseFocus(VR::Panel::Local);
				}
				VR::PumpInput(VR::Panel::Global, false);
				VR::PumpInput(VR::Panel::Local, false);
			}
		}
		static inline REL::Relocation<decltype(thunk)> func;
	};

	void Install()
	{
		REL::Relocation<std::uintptr_t> target{ RELOCATION_ID(75595, 77226), OFFSET(0x9, 0x275) };  // BSGraphics::InitD3D
		stl::write_thunk_call<CreateD3DAndSwapChain>(target.address());

		REL::Relocation<std::uintptr_t> present{ RELOCATION_ID(75461, 77246), OFFSET_3(0x9, 0x9, 0x15) };  // BSGraphics::Renderer::End
		stl::write_thunk_call<PresentVR>(present.address());

		stl::write_vfunc<RE::HUDMenu, PostDisplay>();
	}

	void RenderMenus(bool a_render)
	{
		// Drives the flat-screen PostDisplay render gate. In VR, focus and rendering are driven
		// per-surface from PresentVR off the real Local/Global open-state, so nothing else here.
		renderMenus = a_render;
	}
}
