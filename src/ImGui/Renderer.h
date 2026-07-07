#pragma once

namespace ImGui::Renderer
{
	void Install();
	void Connect();  // ImGuiVRHelper handshake (no-op on flat builds)

	void RenderMenus(bool a_render);

	// members
	inline std::atomic initialized{ false };
	inline std::atomic renderMenus{ false };
}
