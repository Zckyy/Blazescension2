cmake_minimum_required(VERSION 3.16)
project(PlayerInfoOverlay LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Fetch Dear ImGui (no separate install needed).
include(FetchContent)
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        v1.91.6
)
FetchContent_MakeAvailable(imgui)

add_executable(PlayerInfoOverlay WIN32
    src/main.cpp
    src/GameReader.cpp
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_win32.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_dx11.cpp
)

target_include_directories(PlayerInfoOverlay PRIVATE
    src
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)

target_link_libraries(PlayerInfoOverlay PRIVATE d3d11 dxgi d3dcompiler advapi32 dwmapi)

# Request elevation on launch (game runs as admin -> we need PROCESS_VM_READ).
if (MSVC)
    target_link_options(PlayerInfoOverlay PRIVATE "/MANIFESTUAC:level='requireAdministrator'")
endif()

# A 64-bit reader can read the 32-bit game fine, but building 32-bit also works.
