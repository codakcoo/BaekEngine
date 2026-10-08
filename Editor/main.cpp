#include "Core/Window.h"
#include "Core/Paths.h"
#include "Renderer/Renderer.h"
#include "Renderer/SceneRenderer.h"
#include "Renderer\Tonemap.h"
#include "RHI/RenderTarget.h"
#include "UI/ImGuiLayer.h"
#include "Scene/Camera.h"
#include "Renderer/Mesh.h"
#include "Renderer\Texture.h"
#include "Scene/Scene.h"
#include "Asset\Model.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "ImGuizmo.h"

#include <exception>
#include <algorithm>
#include <string>
#include <vector>

baek::RenderTarget sceneRT;             // HDR: 씬을 그리는 곳
baek::RenderTarget viewportRT;          // LDR: 톤매핑 결과, ImGui가 표시 (기존 변수)


UINT vpReqW = 1200, vpReqH = 720;       // UI가 요청한 크기
static int gSelected = -1;                  // 선택된 엔티티 인덱스 (-1 = 없음)
static ImGuizmo::OPERATION gGizmoOp = ImGuizmo::TRANSLATE;
static float gExposure = 1.0f;

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
    ImGui::SeparatorText("Material");
    if (baek::Material* m = e.material)
    {
        ImGui::TextDisabled("%s", m->name.c_str());
        ImGui::ColorEdit3("Color", &m->baseColor.x);
        ImGui::SliderFloat("Metallic", &m->metallic, 0.0f, 1.0f);
        ImGui::SliderFloat("Roughness", &m->roughness, 0.0, 1.0f);
    }
    else
    {
        ImGui::TextDisabled("(default material)");
    }

    ImGui::End();
}

static void DrawGizmo(baek::Camera& camera, baek::Scene& scene, bool hovered)
{
    using namespace DirectX;
    ImGuiIO& io = ImGui::GetIO();

    // 단축키: 카메라 플라이 중(우클릭)이거나 텍스트 입력 중에는 무시
    if (hovered && !ImGui::IsMouseDown(ImGuiMouseButton_Right) && !io.WantTextInput)
    {
        if (ImGui::IsKeyPressed(ImGuiKey_W)) gGizmoOp = ImGuizmo::TRANSLATE;
        if (ImGui::IsKeyPressed(ImGuiKey_E)) gGizmoOp = ImGuizmo::ROTATE;
        if (ImGui::IsKeyPressed(ImGuiKey_R)) gGizmoOp = ImGuizmo::SCALE;
    }

    auto& ents = scene.Entities();
    if (gSelected < 0 || gSelected >= (int)ents.size()) return;
    baek::Transform& t = ents[gSelected].transform;

    // 기즈모를 Viewport 이미지 영역에 맞춤
    const ImVec2 min = ImGui::GetItemRectMin();
    const ImVec2 size = ImGui::GetItemRectSize();
    ImGuizmo::SetOrthographic(false);
    ImGuizmo::SetDrawlist();
    ImGuizmo::SetRect(min.x, min.y, size.x, size.y);

    XMFLOAT4X4 view, proj, world;
    XMStoreFloat4x4(&view, camera.View());
    XMStoreFloat4x4(&proj, camera.Proj());
    XMStoreFloat4x4(&world, t.Matrix());

    // 스케일은 로컬 축에서만 의미가 있음
    const ImGuizmo::MODE mode = (gGizmoOp == ImGuizmo::SCALE) ? ImGuizmo::LOCAL : ImGuizmo::WORLD;

    if (ImGuizmo::Manipulate(&view.m[0][0], &proj.m[0][0], gGizmoOp, mode, &world.m[0][0]))
    {
        // float* 이기때문에 주소로 넘겨주면 float의 크기만큼만 넘겨면 x -> y -> z 로 원소를 가져올 수 있음
        ImGuizmo::DecomposeMatrixToComponents(&world.m[0][0], &t.position.x, &t.rotation.x, &t.scale.x);
    }
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

    // Stats 그리기
    ImGui::Begin("Stats");
    const ImGuiIO& io = ImGui::GetIO();
    ImGui::Text("FPS : %.1f", io.Framerate);
    ImGui::Text("Frame : %.3f", 1000.0f / io.Framerate);
    ImGui::Text("Cam : %.2f, %.2f, %.2f", p.x, p.y, p.z);
    ImGui::Separator();
    ImGui::Checkbox("ImGui Demo", &showDemo);
    ImGui::SeparatorText("Directional Light");
    ImGui::DragFloat3("Direction", &scene.light.direction.x, 0.01f, -1.0f, 1.0f);
    ImGui::ColorEdit3("Light Color", &scene.light.color.x);
    ImGui::SliderFloat("Intensity", &scene.light.intensity, 0.0f, 5.0f);
    ImGui::SliderFloat("Ambient", &scene.light.ambient, 0.0f, 1.0f);
    ImGui::SliderFloat("Exposure", &gExposure, 0.1f, 5.0f);
    ImGui::End();

    // 계층, 도구 그리기
    DrawHierarchy(scene);
    DrawInspector(scene);

    // Viewport 그리기
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0,0));
	ImGui::Begin("Viewport");
	ImVec2 avail = ImGui::GetContentRegionAvail();
	vpReqW = (UINT)std::max(1.0f, avail.x);
	vpReqH = (UINT)std::max(1.0f, avail.y);
	ImGui::Image((ImTextureID)(viewportRT.Srv().ptr), avail);
    
    // 기즈모 그리기
    const bool hovered = ImGui::IsWindowHovered();
    DrawGizmo(camera, scene, hovered);

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

        auto* device = renderer.GetDevice().Get();

        const float sceneClear[4] = { 0.012f, 0.012f, 0.014f, 1.0f };   // 선형 값 (화면에서는 어두운 회색)
        sceneRT.Init(device, &renderer.RtvHeap(), &renderer.DsvHeap(), &renderer.SrvHeap(),
                     DXGI_FORMAT_R16G16B16A16_FLOAT, sceneClear, true);
        sceneRT.Resize(vpReqW, vpReqH);

        const float black[4] = { 0, 0, 0, 1 };
        viewportRT.Init(device, &renderer.RtvHeap(), &renderer.DsvHeap(), &renderer.SrvHeap(),
                        DXGI_FORMAT_R8G8B8A8_UNORM, black, false);          // 깊이 없음
		viewportRT.Resize(vpReqW, vpReqH);

        baek::TonemapPass tonemap;
        tonemap.Init(device, DXGI_FORMAT_R8G8B8A8_UNORM);

        baek::SceneRenderer sceneRenderer;
        sceneRenderer.Init(renderer, DXGI_FORMAT_R16G16B16A16_FLOAT, baek::RenderTarget::DepthFormat);

        baek::Texture checker;                  // 체커보드 텍스처 (256x256, 32픽셀 칸)
        {
            const UINT size = 256;
            std::vector<uint8_t> px(size * size * 4);
            for (UINT y = 0; y < size; ++y)
            {
                for (UINT x = 0; x < size; ++x)
                {
                    const uint8_t v = (((x/8) + (y/8))%2 == 0) ? 235 : 70;
                    uint8_t* p = &px[(y * size + x) * 4];
                    p[0] = p[1] = p[2] = v;
                    p[3] = 255;
                }
            }
            checker.CreateFromPixels(renderer, px.data(), size, size, true);
        }
        baek::Texture bumps;                    // 256x256, 32픽셀마다 돌기 하나
        {
            const UINT size = 256, cell = 32;
            std::vector<uint8_t> px(size * size * 4);
            for (UINT y = 0; y < size; ++y)
            {
                for (UINT x = 0; x < size; ++x)
                {
                    const float dx = ((x % cell) + 0.5f) / (cell * 0.5f) - 1.0f;        // 칸 중심 기준 -1 ~ 1
                    const float dy = ((y % cell) + 0.5f) / (cell * 0.5f) - 1.0f;       
                    const float r2 = dx * dx + dy * dy;

                    float nx = 0.0f, ny = 0.0f, nz = 1.0f;          // 돌기 밖은 평평
                    if (r2 < 0.64f) 
                    { 
                        nx = dx; 
                        ny = dy; 
                        nz = sqrtf(1.0f-r2); 
                    }

                    uint8_t* p = &px[(y * size + x) * 4];
                    p[0] = (uint8_t)((nx * 0.5f + 0.5f) * 255.0f);
                    p[1] = (uint8_t)((ny * 0.5f + 0.5f) * 255.0f);
                    p[2] = (uint8_t)((nz * 0.5f + 0.5f) * 255.0f);
                    p[3] = 255;
                }
            }
            bumps.CreateFromPixels(renderer, px.data(), size, size, false);             // 데이터 텍스처: srgb = false
        }
        baek::Texture brick;
        brick.LoadFromFile(renderer, baek::AssetPath("Textures/brick.png"), true);

        baek::Mesh cubeMesh = baek::Mesh::CreateCube(renderer.GetDevice().Get());
        baek::Scene scene;

        auto& brickMat = scene.CreateMaterial("Brick");
        brickMat.albedoMap = &brick;
        brickMat.roughness = 0.8f;

        auto& bumpMat = scene.CreateMaterial("Bumpy Blue");
        bumpMat.baseColor = { 0.30f, 0.55f, 0.90f };
        bumpMat.albedoMap = &bumps;
        bumpMat.roughness = 0.35f;

        baek::Model helmet;
        helmet.Load(renderer, scene, baek::AssetPath("Models/DamagedHelmet.glb"), { -3.0f, 1.5f, 3.0f });

        // 큐브
        {
            auto& e = scene.Create("Cube A");
            e.mesh = &cubeMesh;
            e.material = &brickMat;
            e.transform.position = { 0.0f, 0.5f, 0.0f };
        }
        {
            auto& e = scene.Create("Cube B");
            e.mesh = &cubeMesh;
            e.transform.position = { 3.0f, 1.0f, 2.0f };
            e.transform.rotation = { 0.0f, 30.0f, 0.0f };
            e.transform.scale = { 2.0f, 2.0f, 2.0f };
            e.material = &bumpMat;
        }
        {
            auto& e = scene.Create("Cube C");
            e.mesh = &cubeMesh;
            e.transform.position = { -3.0f, 0.25f, -1.0f };
            e.transform.scale = { 0.5f, 0.5f, 0.5f };
            e.material = &brickMat;                     // Cube A와 같은 머티리얼 공유
        }

        baek::Mesh sphereMesh = baek::Mesh::CreateSphere(device);

        for (int i = 0; i < 5; ++i)
        {
            const float rough = 0.1f + 0.2f * i;        // 0.1, 0.3, 0.5, 0.7, 0.9
            const float x = -4.0f + 2.0f * i;

            auto& gold = scene.CreateMaterial("Gold " + std::to_string(i));
            gold.baseColor = { 1.00f, 0.77f, 0.34f };
            gold.metallic = 1.0f;
            gold.roughness = rough;
            gold.normalMap = &bumps;

            auto& plastic = scene.CreateMaterial("Plastic " + std::to_string(i));
            plastic.baseColor = { 0.80f, 0.10f, 0.10f };
            plastic.roughness = rough;

            {
                auto& e = scene.Create("Metal" + std::to_string(i));
                e.mesh = &sphereMesh;
                e.material = &gold;
                e.transform.position = { x, 0.5f, -3.0f };
            }
            {
                auto& e = scene.Create("Plastic" + std::to_string(i));
                e.mesh = &sphereMesh;
                e.material = &plastic;
                e.transform.position = { x, 0.5f, -5.0f };
            }

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
                sceneRT.Resize(vpReqW, vpReqH);
                viewportRT.Resize(vpReqW, vpReqH);          // 크기 변경
			}

            imgui.BeginFrame();
            ImGuizmo::BeginFrame();
            DrawEditorUI(showDemo, camera, scene);

            renderer.BeginFrame(clear);
			auto* cmd = renderer.CommandList();
            
            camera.SetAspect((float)viewportRT.Width() / (float)viewportRT.Height());

            // 1) 씬 -> HDR
            sceneRT.Begin(cmd);
            sceneRenderer.Render(cmd, renderer.FrameIndex(), camera, scene);
            sceneRT.End(cmd);

            // 2) HDR -> 톤매핑 -> LDR
			viewportRT.Begin(cmd);
            tonemap.Render(cmd, renderer.SrvHeap().Get(), sceneRT.Srv(), gExposure);
			viewportRT.End(cmd);

			renderer.BindBackBuffer();                 // 스왑체인 RT로 전환
            imgui.EndFrame(cmd);
            renderer.EndFrame();
        }

        renderer.WaitIdle();                        // GPU가 ImGui 리소스를 다 쓴 뒤에
        cubeMesh.Shutdown();
        sphereMesh.Shutdown();
        checker.Shutdown();
        brick.Shutdown();
        helmet.Shutdown();
        sceneRenderer.Shutdown();
        sceneRT.Shutdown();
        viewportRT.Shutdown();
        tonemap.Shutdown();
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
