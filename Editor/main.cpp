#include "Core/Window.h"
#include "Renderer/Renderer.h"
#include "Renderer/SceneRenderer.h"
#include "RHI/RenderTarget.h"
#include "UI/ImGuiLayer.h"
#include "Scene/Camera.h"
#include "Renderer/Mesh.h"
#include "Scene/Scene.h"
#include "imgui.h"
#include "imgui_internal.h"

#include <exception>
#include <algorithm>

baek::RenderTarget viewportRT;
UINT vpReqW = 1200, vpReqH = 720;       // UI가 요청한 크기
static int gSelected = -1;                  // 선택된 엔티티 인덱스 (-1 = 없음)

static void UpdateCameraInput(baek::Camera& cam, bool hovered)
{
    ImGuiIO& io = ImGui::GetIO();
    static bool flying = false, orbiting = false;

    // 시작은 Viewport 위에서만, 유지는 버튼을 누르고 있는 동안 (커서가 패널 밖으로 나가도 계속)
    if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right))                   flying = true;
    if (!ImGui::IsMouseDown(ImGuiMouseButton_Right))                                flying =false;
    if (hovered && io.KeyAlt && ImGui::IsMouseClicked(ImGuiMouseButton_Left))       orbiting = true;
    if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))                                 orbiting = false;

    const float rot = 0.004f;           // rad / pixel
    const float dYaw  = io.MouseDelta.x * rot;
    const float dPitch = -io.MouseDelta.y * rot;

    if (flying)
    {
        cam.Rotate(dYaw, dPitch);

        const float speed = 5.0f * (io.KeyShift ? 3.0f : 1.0f) * io.DeltaTime;
        const float f = (float)ImGui::IsKeyDown(ImGuiKey_W) - (float)ImGui::IsKeyDown(ImGuiKey_S);
        const float r = (float)ImGui::IsKeyDown(ImGuiKey_D) - (float)ImGui::IsKeyDown(ImGuiKey_A);
        const float u = (float)ImGui::IsKeyDown(ImGuiKey_E) - (float)ImGui::IsKeyDown(ImGuiKey_Q);
        cam.MoveLocal(r * speed, u * speed, f * speed);
    }
    else if (orbiting)
    {
        cam.Orbit(dYaw, dPitch);
    }
    
    if (hovered && io.MouseWheel != 0.0f)
        cam.Zoom(io.MouseWheel * 0.5f);
}

// 엔티티 목록에서 하나를 선택하고, Transform을 드래그로 편집하는 패널을 붙입니다.
static void DrawHierarchy(baek::Scene& scene)
{
    ImGui::Begin("Hierarchy");

    auto& ents = scene.Entities();
    for (int i = 0; i < (int)ents.size(); ++i)
    {
        ImGui::PushID(i);               // 같은 이름의 엔티티가 있어도 ID가 겹치지 않게
        const char* label = ents[i].name.empty() ? "(unnamed)" : ents[i].name.c_str();
        if(ImGui::Selectable(label, gSelected == i))
            gSelected = i;
        ImGui::PopID();
    }

    // 빈 공간을 클릭하면 선택 해제
    if(ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !ImGui::IsAnyItemHovered())
        gSelected = -1;

    ImGui::End();
}

static void DrawInspector(baek::Scene& scene)
{
    ImGui::Begin("Inspector");

    auto& ents = scene.Entities();
    if (gSelected < 0 || gSelected >= (int)ents.size())
    {
        ImGui::TextDisabled("No entity selected");
        ImGui::End();
        return;
    }

    baek::Entity& e = ents[gSelected];

    char buf[128];
    strncpy_s(buf, e.name.c_str(), _TRUNCATE);
    if(ImGui::InputText("Name", buf, sizeof(buf)))
        e.name = buf;

    ImGui::Checkbox("Visible", &e.visible);

    ImGui::SeparatorText("Transform");
    ImGui::DragFloat3("Position", &e.transform.position.x, 0.05f);
    ImGui::DragFloat3("Rotation", &e.transform.rotation.x, 0.5f);
    ImGui::DragFloat3("Scale", &e.transform.scale.x, 0.02f, 0.01f, 100.0f);

    ImGui::End();
}

static void DrawEditorUI(bool& showDemo, baek::Camera& camera, baek::Scene& scene)
{
	ImGuiID dockId = ImGui::GetID("MainDockSpace");

	// 저장된 레이아웃(imgui.ini)이 없으면 기본 레이아웃 생성
    if(ImGui::DockBuilderGetNode(dockId) == nullptr)
    {
        ImGui::DockBuilderAddNode(dockId, ImGuiDockNodeFlags_DockSpace);   // 새 노드 생성
        ImGui::DockBuilderSetNodeSize(dockId, ImGui::GetMainViewport()->Size);

        // 왼쪽에 Stats, 가운데 Viewport
        ImGuiID center = dockId;
        ImGuiID left = ImGui::DockBuilderSplitNode(center, ImGuiDir_Left, 0.18f, nullptr, &center);
        ImGuiID right = ImGui::DockBuilderSplitNode(center, ImGuiDir_Right, 0.22f, nullptr, &center);
        ImGuiID rightBottom = ImGui::DockBuilderSplitNode(right, ImGuiDir_Down, 0.30f, nullptr, &right);

        ImGui::DockBuilderDockWindow("Hierarchy", left);
        ImGui::DockBuilderDockWindow("Viewport", center);
        ImGui::DockBuilderDockWindow("Inspector", right);
        ImGui::DockBuilderDockWindow("Stats", rightBottom);
        ImGui::DockBuilderFinish(dockId);
	}

    // 메인 창 전체를 도킹 영역으로 (가운데는 비워서 씬이 보이게)
    ImGui::DockSpaceOverViewport(dockId, ImGui::GetMainViewport());

    const auto& p = camera.Position();

    ImGui::Begin("Stats");
    const ImGuiIO& io = ImGui::GetIO();
    ImGui::Text("FPS : %.1f", io.Framerate);
    ImGui::Text("Frame : %.3f", 1000.0f / io.Framerate);
    ImGui::Text("Cam : %.2f, %.2f, %.2f", p.x, p.y, p.z);
    ImGui::Separator();
    ImGui::Checkbox("ImGui Demo", &showDemo);
    ImGui::End();

    DrawHierarchy(scene);
    DrawInspector(scene);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0,0));
	ImGui::Begin("Viewport");
	ImVec2 avail = ImGui::GetContentRegionAvail();
	vpReqW = (UINT)std::max(1.0f, avail.x);
	vpReqH = (UINT)std::max(1.0f, avail.y);
	ImGui::Image((ImTextureID)(viewportRT.Srv().ptr), avail);
    UpdateCameraInput(camera, ImGui::IsWindowHovered());
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

		const float sceneClear[4] = { 0.12f, 0.12f, 0.13f, 1.0f };          // 백버퍼와 구분되는 색
		viewportRT.Init(renderer.GetDevice().Get(), &renderer.RtvHeap(), &renderer.DsvHeap(), &renderer.SrvHeap(),
            DXGI_FORMAT_R8G8B8A8_UNORM, sceneClear);
		viewportRT.Resize(vpReqW, vpReqH);

        baek::SceneRenderer sceneRenderer;
        sceneRenderer.Init(renderer.GetDevice().Get(), DXGI_FORMAT_R8G8B8A8_UNORM, baek::RenderTarget::DepthFormat);

        baek::Mesh cubeMesh = baek::Mesh::CreateCube(renderer.GetDevice().Get());

        baek::Scene scene;
        {
            auto& e = scene.Create("Cube A");
            e.mesh = &cubeMesh;
            e.transform.position = { 0.0f, 0.5f, 0.0f };
        }
        {
            auto& e = scene.Create("Cube B");
            e.mesh = &cubeMesh;
            e.transform.position = { 3.0f, 1.0f, 2.0f };
            e.transform.rotation = { 0.0f, 30.0f, 0.0f };
            e.transform.scale = { 2.0f, 2.0f, 2.0f };
        }
        {
            auto& e = scene.Create("Cube C");
            e.mesh = &cubeMesh;
            e.transform.position = { -3.0f, 0.25f, -1.0f };
            e.transform.scale = { 0.5f, 0.5f, 0.5f };
        }

        const float clear[4] = { 0.10f, 0.10f, 0.15f, 1.0f };
        bool showDemo = false;
        baek::Camera camera;
        camera.SetLens(DirectX::XM_PIDIV4, 1280.0f / 720.0f, 0.1f, 1000.0f);

        while (window.PumpMessages())
        {
            if (window.IsMinimized()) { Sleep(16); continue; }    // 최소화 시 0x0 스왑체인 방지
            if(vpReqW != viewportRT.Width() || vpReqH != viewportRT.Height())
            {
                renderer.WaitIdle();                        // 아직 GPU가 쓰는 텍스처를 지우지 않도록 (지연 해제는 나중에)
                viewportRT.Resize(vpReqW, vpReqH);          // 크기 변경
			}

            imgui.BeginFrame();
            DrawEditorUI(showDemo, camera, scene);

            renderer.BeginFrame(clear);
			auto* cmd = renderer.CommandList();
            
            camera.SetAspect((float)viewportRT.Width() / (float)viewportRT.Height());

			viewportRT.Begin(cmd);
            sceneRenderer.Render(cmd, camera.ViewProj(), scene);
			viewportRT.End(cmd);

			renderer.BindBackBuffer();                 // 스왑체인 RT로 전환
            imgui.EndFrame(cmd);
            renderer.EndFrame();
        }

        renderer.WaitIdle();                        // GPU가 ImGui 리소스를 다 쓴 뒤에
        cubeMesh.Shutdown();
        sceneRenderer.Shutdown();
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
