cmake_minimum_required(VERSION 3.16)
project(PlayerInfoOverlay LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# deadcell-gui-2 carries the shadow-enabled Dear ImGui revision that its
# renderer is built around.  We only consume its vendored ImGui sources here;
# the upstream GUI controls depend on the original project's game-specific
# framework and are not a standalone CMake target.
include(FetchContent)
FetchContent_Declare(
    deadcell_gui
    GIT_REPOSITORY https://github.com/EternityX/deadcell-gui-2.git
    GIT_TAG        590f18d
)
FetchContent_GetProperties(deadcell_gui)
if (NOT deadcell_gui_POPULATED)
    FetchContent_Populate(deadcell_gui)
endif()

set(DEADCELL_IMGUI_DIR ${deadcell_gui_SOURCE_DIR}/vendor/imgui)

add_executable(PlayerInfoOverlay WIN32
    src/main.cpp
    src/GameReader.cpp
    ${DEADCELL_IMGUI_DIR}/imgui.cpp
    ${DEADCELL_IMGUI_DIR}/imgui_draw.cpp
    ${DEADCELL_IMGUI_DIR}/imgui_tables.cpp
    ${DEADCELL_IMGUI_DIR}/imgui_widgets.cpp
    ${DEADCELL_IMGUI_DIR}/imgui_impl_win32.cpp
    ${DEADCELL_IMGUI_DIR}/imgui_impl_dx11.cpp
)

target_include_directories(PlayerInfoOverlay PRIVATE
    src
    ${DEADCELL_IMGUI_DIR}
)

target_link_libraries(PlayerInfoOverlay PRIVATE d3d11 dxgi d3dcompiler advapi32 dwmapi)

# Request elevation on launch (game runs as admin -> we need PROCESS_VM_READ).
if (MSVC)
    target_link_options(PlayerInfoOverlay PRIVATE "/MANIFESTUAC:level='requireAdministrator'")
endif()

# A 64-bit reader can read the 32-bit game fine, but building 32-bit also works.
