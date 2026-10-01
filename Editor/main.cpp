#include "Core/Window.h"
#include "Renderer/Renderer.h"
#include "Renderer/SceneRenderer.h"
#include "RHI/RenderTarget.h"
#include "UI/ImGuiLayer.h"
#include "imgui.h"
#include "imgui_internal.h"
#include <exception>
#include <algorithm>
#include <chrono>

baek::RenderTarget viewportRT;
UINT vpReqW = 1200, vpReqH = 720;       // UI가 요청한 크기

static void DrawEditorUI(bool& showDemo)
{
	ImGuiID dockId = ImGui::GetID("MainDockSpace");

	// 저장된 레이아웃(imgui.ini)이 없으면 기본 레이아웃 생성
    if(ImGui::DockBuilderGetNode(dockId) == nullptr)
    {
        ImGui::DockBuilderAddNode(dockId, ImGuiDockNodeFlags_DockSpace);   // 새 노드 생성
        ImGui::DockBuilderSetNodeSize(dockId, ImGui::GetMainViewport()->Size);

        // 왼쪽에 Stats, 가운데 Viewport
        ImGuiID center = dockId;
        ImGuiID right = ImGui::DockBuilderSplitNode(dockId, ImGuiDir_Right, 0.22f, nullptr, &center);

        ImGui::DockBuilderDockWindow("Viewport", center);
        ImGui::DockBuilderDockWindow("Stats", right);
        ImGui::DockBuilderFinish(dockId);
	}

    // 메인 창 전체를 도킹 영역으로 (가운데는 비워서 씬이 보이게)
    ImGui::DockSpaceOverViewport(dockId, ImGui::GetMainViewport());

    ImGui::Begin("Stats");
    const ImGuiIO& io = ImGui::GetIO();
    ImGui::Text("FPS : %.1f", io.Framerate);
    ImGui::Text("Frame : %.3f", 1000.0f / io.Framerate);
    ImGui::Separator();
    ImGui::Checkbox("ImGui Demo", &showDemo);
    ImGui::End();

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0,0));
	ImGui::Begin("Viewport");
	ImVec2 avail = ImGui::GetContentRegionAvail();
	vpReqW = (UINT)std::max(1.0f, avail.x);
	vpReqH = (UINT)std::max(1.0f, avail.y);
	ImGui::Image((ImTextureID)(viewportRT.Srv().ptr), avail);
	ImGui::End();
	ImGui::PopStyleVar();
        
    if(showDemo) ImGui::ShowDemoWindow(&showDemo);
}

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int)
{
    try
    {
        baek::Window window;
        if (!window.Create(L"BaekEngine Editor", 1280, 720))
        {
            MessageBoxW(nullptr, L"Window 생성 실패", L"BaekEngine - Fatal", MB_OK | MB_ICONERROR | MB_TOPMOST);
            return -1;
        }

        baek::Renderer renderer;
        renderer.Init(window);

        baek::ImGuiLayer imgui;
        imgui.Init(window, renderer);

		const float sceneClear[4] = { 0.25f, 0.15f, 0.10f, 1.0f };          // 백버퍼와 구분되는 색
		viewportRT.Init(renderer.GetDevice().Get(), &renderer.RtvHeap(), &renderer.DsvHeap(), &renderer.SrvHeap(),
            DXGI_FORMAT_R8G8B8A8_UNORM, sceneClear);
		viewportRT.Resize(vpReqW, vpReqH);

        baek::SceneRenderer scene;
        scene.Init(renderer.GetDevice().Get(), DXGI_FORMAT_R8G8B8A8_UNORM, baek::RenderTarget::DepthFormat);

        const auto startTime = std::chrono::steady_clock::now();

        const float clear[4] = { 0.10f, 0.10f, 0.15f, 1.0f };
        bool showDemo = false;

        while (window.PumpMessages())
        {
            if (window.IsMinimized()) { Sleep(16); continue; }    // 최소화 시 0x0 스왑체인 방지
            if(vpReqW != viewportRT.Width() || vpReqH != viewportRT.Height())
            {
                renderer.WaitIdle();                        // 아직 GPU가 쓰는 텍스처를 지우지 않도록 (지연 해제는 나중에)
                viewportRT.Resize(vpReqW, vpReqH);          // 크기 변경
			}

            float t = std::chrono::duration<float>(std::chrono::steady_clock::now() - startTime).count();

            imgui.BeginFrame();
            DrawEditorUI(showDemo);

            renderer.BeginFrame(clear);
			auto* cmd = renderer.CommandList();
            
			viewportRT.Begin(cmd);
            scene.Render(cmd, (float)viewportRT.Width() / (float)viewportRT.Height(), t);
			viewportRT.End(cmd);

			renderer.BindBackBuffer();                 // 스왑체인 RT로 전환
            imgui.EndFrame(cmd);
            renderer.EndFrame();
        }

        renderer.WaitIdle();                        // GPU가 ImGui 리소스를 다 쓴 뒤에
        scene.Shutdown();
        viewportRT.Shutdown();
        imgui.Shutdown();                           // ImGui 해제
        renderer.Shutdown();
    }
    catch (const std::exception& e)
    {
        MessageBoxA(nullptr, e.what(), "BaekEngine - Fatal", MB_OK | MB_ICONERROR);
        return -1;
    }
    return 0;
}
