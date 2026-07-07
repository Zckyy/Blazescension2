#pragma once

#include <cstdint>
#include <windows.h>

namespace Overlay {

bool findMainWindowForPid(uint32_t pid, HWND& hwnd);
bool getClientScreenRect(HWND hwnd, RECT& rect);

} // namespace Overlay

