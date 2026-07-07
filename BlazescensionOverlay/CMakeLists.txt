cmake_minimum_required(VERSION 3.18)
project(BlazescensionOverlay LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")

include(FetchContent)
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        v1.91.6
)
FetchContent_MakeAvailable(imgui)

file(GLOB_RECURSE APP_SRC CONFIGURE_DEPENDS
    "src/*.h"
    "src/*.cpp"
)

set(IMGUI_SRC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_dx11.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_win32.cpp
)

add_executable(BlazescensionOverlay WIN32 ${APP_SRC} ${IMGUI_SRC})

target_include_directories(BlazescensionOverlay PRIVATE
    src
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)

target_compile_definitions(BlazescensionOverlay PRIVATE
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    _CRT_SECURE_NO_WARNINGS
)

target_link_libraries(BlazescensionOverlay PRIVATE
    d3d11
    dxgi
    dcomp
    dwmapi
    advapi32
    user32
    gdi32
    shell32
)

if (MSVC)
    target_compile_options(BlazescensionOverlay PRIVATE /W4 /permissive-)
    target_link_options(BlazescensionOverlay PRIVATE "/MANIFESTUAC:level='requireAdministrator'")
endif()

