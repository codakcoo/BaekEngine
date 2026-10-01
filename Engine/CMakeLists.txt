file(GLOB_RECURSE ENGINE_SRC CONFIGURE_DEPENDS *.h *.cpp)

add_library(Engine STATIC ${ENGINE_SRC} "RHI/DescriptorHeap.h" "RHI/DescriptorHeap.cpp" "UI/ImGuiLayer.h" "UI/ImGuiLayer.cpp" "RHI/RenderTarget.h" "RHI/RenderTarget.cpp")
target_include_directories(Engine PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/ThirdParty)
target_link_libraries(Engine PUBLIC d3d12 dxgi dxguid d3dcompiler imgui)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${ENGINE_SRC})   # VS 필터 = 폴더 구조