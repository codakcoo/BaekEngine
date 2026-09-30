#include "UI/ImGuiLayer.h"
#include "Core/WIndow.h"
#include "Renderer/Renderer.h"

#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx12.h"

// imgui_impl_win32.h에 주석 처리돼 있어서 직접 선언
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace baek
{
	void ImGuiLayer::Init(Window& window, Renderer& renderer)
	{
		mRenderer = &renderer;

		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGuiIO& io = ImGui::GetIO();
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
		io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
		ImGui::StyleColorsDark();

		ImGui_ImplWin32_Init(window.Handle());
		window.SetMessageHook([](HWND h, UINT m, WPARAM w, LPARAM l)
		{
			return ImGui_ImplWin32_WndProcHandler(h, m, w, l);
		});

		ImGui_ImplDX12_InitInfo info = {};
		info.Device = renderer.GetDevice().Get();
		info.CommandQueue = renderer.GetDevice().Queue();
		info.NumFramesInFlight = Renderer::FrameCount;
		info.RTVFormat = SwapChain::Format;
		info.DSVFormat = DXGI_FORMAT_UNKNOWN;
		info.SrvDescriptorHeap = renderer.SrvHeap().Get();
		info.UserData = &renderer.SrvHeap();
		// ImGui가 폰트/텍스처 SRV가 필요할 때 우리 할당자에서 받아감
		info.SrvDescriptorAllocFn = [](ImGui_ImplDX12_InitInfo* i, D3D12_CPU_DESCRIPTOR_HANDLE* cpu, D3D12_GPU_DESCRIPTOR_HANDLE* gpu)
		{
			DescriptorHandle h = static_cast<DescriptorHeap*>(i->UserData)->Allocate();
			*cpu = h.cpu;
			*gpu = h.gpu;
		};
		info.SrvDescriptorFreeFn = [](ImGui_ImplDX12_InitInfo* i, D3D12_CPU_DESCRIPTOR_HANDLE cpu, D3D12_GPU_DESCRIPTOR_HANDLE)
		{
			static_cast<DescriptorHeap*>(i->UserData)->Free(cpu);
		};
		ImGui_ImplDX12_Init(&info);
	}
	void ImGuiLayer::Shutdown()
	{
		ImGui_ImplDX12_Shutdown();
		ImGui_ImplWin32_Shutdown();
		ImGui::DestroyContext();
	}

	void ImGuiLayer::BeginFrame()
	{
		ImGui_ImplDX12_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();
	}
	void ImGuiLayer::EndFrame(ID3D12GraphicsCommandList* cmd)
	{
		ImGui::Render();
		ID3D12DescriptorHeap* heaps[] = { mRenderer->SrvHeap().Get() };
		cmd->SetDescriptorHeaps(1, heaps);
		ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), cmd);
	}
}
