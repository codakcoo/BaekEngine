if(NOT EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/imgui/imgui.cpp)
    message(FATAL_ERROR "ImGui directory not found. Please ensure that the ImGui library is present in the ThirdParty directory.")
endif()

set(IMGUI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/imgui)
set(IMGUIZMO_DIR ${CMAKE_CURRENT_SOURCE_DIR}/ImGuizmo/src)
set(STB_DIR ${CMAKE_CURRENT_SOURCE_DIR}/stb)
set(CGLTF_DIR ${CMAKE_CURRENT_SOURCE_DIR}/cgltf)


add_library(imgui STATIC
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/imgui_demo.cpp
    ${IMGUI_DIR}/backends/imgui_impl_win32.cpp
    ${IMGUI_DIR}/backends/imgui_impl_dx12.cpp)
add_subdirectory(DirectX-Headers)

add_library(imguizmo STATIC ${IMGUIZMO_DIR}/ImGuizmo.cpp)
target_include_directories(imguizmo PUBLIC ${IMGUIZMO_DIR})
target_link_libraries(imguizmo PUBLIC imgui)
if(MSVC)
    target_compile_options(imguizmo PRIVATE /w)   # 서드파티 경고 끄기
endif()

add_library(stb INTERFACE)
target_include_directories(stb INTERFACE ${STB_DIR}/stb)

add_library(cgltf INTERFACE)
target_include_directories(cgltf INTERFACE ${CGLTF_DIR}/cgltf)

target_include_directories(imgui PUBLIC ${IMGUI_DIR} ${IMGUI_DIR}/backends)
target_link_libraries(imgui PUBLIC d3d12 dxgi d3dcompiler)
