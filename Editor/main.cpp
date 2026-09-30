#include "Core/Window.h"
#include "Renderer/Renderer.h"
#include <exception>

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int)
{
    try
    {
        baek::Window window;
        if (!window.Create(L"BaekEngine Editor", 1280, 720))
            return -1;

        baek::Renderer renderer;
        renderer.Init(window);

        const float clear[4] = { 0.10f, 0.10f, 0.15f, 1.0f };
        while (window.PumpMessages())
        {
            if (window.IsMinimized()) { Sleep(16); continue; }    // 최소화 시 0x0 스왑체인 방지
            renderer.Render(clear);
        }
        renderer.Shutdown();
    }
    catch (const std::exception& e)
    {
        MessageBoxA(nullptr, e.what(), "BaekEngine - Fatal", MB_OK | MB_ICONERROR);
        return -1;
    }
    return 0;
}