#include "Core/Window.h"
#include "Renderer/Renderer.h"
#include "UI/ImGuiLayer.h"
#include "imgui.h"
#include <exception>

static void DrawEditorUI(bool& showDemo)
{
    // 메인 창 전체를 도킹 영역으로 (가운데는 비워서 씬이 보이게)
    ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode);

    ImGui::Begin("Stats");
    const ImGuiIO& io = ImGui::GetIO();
    ImGui::Text("FPS : %.1f", io.Framerate);
    ImGui::Text("Frame : %.3f", 1000.0f / io.Framerate);
    ImGui::Separator();
    ImGui::Checkbox("ImGui Demo", &showDemo);
    ImGui::End();

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

        const float clear[4] = { 0.10f, 0.10f, 0.15f, 1.0f };
        bool showDemo = false;

        while (window.PumpMessages())
        {
            if (window.IsMinimized()) { Sleep(16); continue; }    // 최소화 시 0x0 스왑체인 방지
            
            renderer.BeginFrame(clear);
            imgui.BeginFrame();
            DrawEditorUI(showDemo);
            imgui.EndFrame(renderer.CommandList());
            renderer.EndFrame();
        }

        renderer.WaitIdle();                        // GPU가 ImGui 리소스를 다 쓴 뒤에
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
