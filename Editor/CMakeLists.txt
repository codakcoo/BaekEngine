add_executable(Editor WIN32 main.cpp)
target_link_libraries(Editor PRIVATE Engine)

# 실행 시 작업 디렉터리 = Assets (Shaders\\... 상대 경로 그대로 사용)
set_property(TARGET Editor PROPERTY VS_DEBUGGER_WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/Assets)