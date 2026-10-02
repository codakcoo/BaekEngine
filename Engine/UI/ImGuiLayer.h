#pragma once
#include "RHI/DxUtil.h"

namespace baek
{
	class Window;
	class Renderer;

	class ImGuiLayer
	{
	public:
		void Init(Window& window, Renderer& renderer);
		void Shutdown();

		void BeginFrame();												// 백엔드 NewFrame + ImGui::NewFrame
		void EndFrame(ID3D12GraphicsCommandList* cmd);					// ImGui::Render + 드로우 기록

	private:
		Renderer* mRenderer = nullptr;
	};
}
