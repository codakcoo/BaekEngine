file(GLOB_RECURSE ENGINE_SRC CONFIGURE_DEPENDS *.h *.cpp)

add_library(Engine STATIC ${ENGINE_SRC} "RHI/DescriptorHeap.h" "RHI/DescriptorHeap.cpp" "UI/ImGuiLayer.h" "UI/ImGuiLayer.cpp" "RHI/RenderTarget.h" "RHI/RenderTarget.cpp" "Renderer/SceneRenderer.cpp")
target_include_directories(Engine PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/ThirdParty)
target_link_libraries(Engine PUBLIC DirectX-Headers DirectX-Guids d3d12 dxgi d3dcompiler imgui)  # dxguid → DirectX-Guids
target_compile_definitions(Engine PUBLIC BAEK_SHADER_DIR="${CMAKE_SOURCE_DIR}/Shaders/")

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${ENGINE_SRC})   # VS 필터 = 폴더 구조
