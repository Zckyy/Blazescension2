#include "WindowTracker.h"

namespace Overlay {

namespace {

struct FindWindowData {
    uint32_t pid = 0;
    HWND hwnd = nullptr;
};

BOOL CALLBACK enumWindowsProc(HWND hwnd, LPARAM param) {
    auto* data = reinterpret_cast<FindWindowData*>(param);

    DWORD windowPid = 0;
    GetWindowThreadProcessId(hwnd, &windowPid);
    if (windowPid != data->pid || !IsWindowVisible(hwnd) || GetWindow(hwnd, GW_OWNER) != nullptr) {
        return TRUE;
    }

    RECT client{};
    GetClientRect(hwnd, &client);
    const int width = client.right - client.left;
    const int height = client.bottom - client.top;
    if (width > 100 && height > 100) {
        data->hwnd = hwnd;
        return FALSE;
    }

    return TRUE;
}

} // namespace

bool findMainWindowForPid(uint32_t pid, HWND& hwnd) {
    FindWindowData data{ pid, nullptr };
    EnumWindows(enumWindowsProc, reinterpret_cast<LPARAM>(&data));
    hwnd = data.hwnd;
    return hwnd != nullptr;
}

bool getClientScreenRect(HWND hwnd, RECT& rect) {
    if (!hwnd || IsIconic(hwnd)) {
        return false;
    }

    RECT client{};
    if (!GetClientRect(hwnd, &client)) {
        return false;
    }

    POINT topLeft{ 0, 0 };
    if (!ClientToScreen(hwnd, &topLeft)) {
        return false;
    }

    rect.left = topLeft.x;
    rect.top = topLeft.y;
    rect.right = topLeft.x + (client.right - client.left);
    rect.bottom = topLeft.y + (client.bottom - client.top);
    return true;
}

} // namespace Overlay

