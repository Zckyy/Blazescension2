// ============================================================================
//  PlayerInfoOverlay -- transparent, click-through Dear ImGui overlay that
//  draws on top of Ascension.exe and shows the local player's live stats.
//
//  Toggle the menu with INSERT. When hidden it is fully click-through (the game
//  gets all input). When shown it only captures the mouse while the cursor is
//  over the panel, so gameplay is never blocked elsewhere.
//
//  Read-only: only ReadProcessMemory on the game. Never writes to it.
// ============================================================================
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"
#include <d3d11.h>
#include <dwmapi.h>
#include <algorithm>
#include <cmath>
#include <cstdio>

#include "GameReader.h"

// ---- D3D plumbing ----------------------------------------------------------
static ID3D11Device*            g_pd3dDevice = nullptr;
static ID3D11DeviceContext*     g_pd3dDeviceContext = nullptr;
static IDXGISwapChain*          g_pSwapChain = nullptr;
static ID3D11RenderTargetView*  g_mainRenderTargetView = nullptr;
static bool                     g_clickThrough = true;

void CreateRenderTarget() {
    ID3D11Texture2D* pBackBuffer = nullptr;
    g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    if (pBackBuffer) {
        g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_mainRenderTargetView);
        pBackBuffer->Release();
    }
}
void CleanupRenderTarget() {
    if (g_mainRenderTargetView) { g_mainRenderTargetView->Release(); g_mainRenderTargetView = nullptr; }
}
bool CreateDeviceD3D(HWND hWnd) {
    DXGI_SWAP_CHAIN_DESC sd{};
    sd.BufferCount = 2;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    D3D_FEATURE_LEVEL fl;
    const D3D_FEATURE_LEVEL levels[] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0 };
    if (D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
            levels, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &fl,
            &g_pd3dDeviceContext) != S_OK)
        return false;
    CreateRenderTarget();
    return true;
}
void CleanupDeviceD3D() {
    CleanupRenderTarget();
    if (g_pSwapChain) { g_pSwapChain->Release(); g_pSwapChain = nullptr; }
    if (g_pd3dDeviceContext) { g_pd3dDeviceContext->Release(); g_pd3dDeviceContext = nullptr; }
    if (g_pd3dDevice) { g_pd3dDevice->Release(); g_pd3dDevice = nullptr; }
}

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);
LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_NCHITTEST && g_clickThrough) return HTTRANSPARENT;
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam)) return true;
    switch (msg) {
    case WM_SIZE:
        if (g_pd3dDevice && wParam != SIZE_MINIMIZED) {
            CleanupRenderTarget();
            g_pSwapChain->ResizeBuffers(0, (UINT)LOWORD(lParam), (UINT)HIWORD(lParam), DXGI_FORMAT_UNKNOWN, 0);
            CreateRenderTarget();
        }
        return 0;
    case WM_DESTROY: ::PostQuitMessage(0); return 0;
    }
    return ::DefWindowProc(hWnd, msg, wParam, lParam);
}

// ---- find the game's main top-level window by PID --------------------------
struct FindWndData { DWORD pid; HWND hwnd; };
static BOOL CALLBACK enumWndProc(HWND h, LPARAM lp) {
    auto* d = reinterpret_cast<FindWndData*>(lp);
    DWORD wpid = 0; GetWindowThreadProcessId(h, &wpid);
    if (wpid == d->pid && IsWindowVisible(h) && GetWindow(h, GW_OWNER) == nullptr) {
        RECT r; GetClientRect(h, &r);
        if ((r.right - r.left) > 100 && (r.bottom - r.top) > 100) { d->hwnd = h; return FALSE; }
    }
    return TRUE;
}
static HWND findGameWindow(DWORD pid) {
    FindWndData d{ pid, nullptr };
    EnumWindows(enumWndProc, reinterpret_cast<LPARAM>(&d));
    return d.hwnd;
}

// ---- small colored stat bar ------------------------------------------------
static void StatBar(const char* label, float cur, float max, ImVec4 color) {
    float frac = (max > 0.0f) ? (cur / max) : 0.0f;
    if (frac < 0.0f) frac = 0.0f; if (frac > 1.0f) frac = 1.0f;
    char overlay[64];
    std::snprintf(overlay, sizeof(overlay), "%.0f / %.0f  (%.0f%%)", cur, max, frac * 100.0f);
    ImGui::TextUnformatted(label);
    ImGui::PushStyleColor(ImGuiCol_PlotHistogram, color);
    ImGui::ProgressBar(frac, ImVec2(-FLT_MIN, 20.0f), overlay);
    ImGui::PopStyleColor();
}

static void applyDeadcellStyle() {
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 6.0f;
    style.ChildRounding = 4.0f;
    style.FrameRounding = 4.0f;
    style.PopupRounding = 4.0f;
    style.ScrollbarRounding = 4.0f;
    style.WindowBorderSize = 1.0f;
    style.FrameBorderSize = 1.0f;
    style.WindowPadding = ImVec2(16.0f, 14.0f);
    style.ItemSpacing = ImVec2(8.0f, 8.0f);

    // These fields and the shadow primitive are provided by deadcell-gui-2's
    // shadows branch of Dear ImGui.
    style.WindowShadowSize = 18.0f;
    style.WindowShadowOffsetDist = 2.0f;
    style.WindowShadowOffsetAngle = 3.14159265358979323846f * 0.5f;

    ImVec4* colors = style.Colors;
    colors[ImGuiCol_WindowBg] = ImVec4(0.055f, 0.065f, 0.085f, 0.97f);
    colors[ImGuiCol_Border] = ImVec4(0.26f, 0.31f, 0.37f, 0.95f);
    colors[ImGuiCol_FrameBg] = ImVec4(0.10f, 0.12f, 0.16f, 0.98f);
    colors[ImGuiCol_FrameBgHovered] = ImVec4(0.14f, 0.17f, 0.22f, 0.98f);
    colors[ImGuiCol_PlotHistogram] = ImVec4(0.82f, 0.45f, 0.55f, 1.0f);
    colors[ImGuiCol_Button] = ImVec4(0.12f, 0.14f, 0.19f, 1.0f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.17f, 0.20f, 0.27f, 1.0f);
    colors[ImGuiCol_ButtonActive] = ImVec4(0.22f, 0.25f, 0.32f, 1.0f);
}

static void drawDeadcellWindowChrome() {
    ImDrawList* draw = ImGui::GetWindowDrawList();
    const ImVec2 pos = ImGui::GetWindowPos();
    const ImVec2 size = ImGui::GetWindowSize();
    const ImVec2 max = ImVec2(pos.x + size.x, pos.y + size.y);

    // This is the shadow primitive exposed by the deadcell-gui-2 ImGui fork.
    // The cut-out flag leaves the actual panel contents crisp above the glow.
    draw->AddShadowRect(pos, max, IM_COL32(0, 0, 0, 185), 18.0f,
                        ImVec2(0.0f, 2.0f),
                        ImDrawFlags_ShadowCutOutShapeBackground,
                        ImGui::GetStyle().WindowRounding);

    const float headerHeight = 34.0f;
    draw->AddRectFilled(pos, ImVec2(max.x, pos.y + headerHeight),
                        IM_COL32(56, 35, 51, 230), ImGui::GetStyle().WindowRounding,
                        ImDrawFlags_RoundCornersTop);
    draw->AddLine(ImVec2(pos.x, pos.y + headerHeight),
                  ImVec2(max.x, pos.y + headerHeight),
                  IM_COL32(190, 102, 130, 160), 1.0f);
}

static float dot(const Vec3& a, const Vec3& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

static bool worldToScreen(const Vec3& world, const CameraInfo& cam, const ImVec2& viewport, ImVec2& out) {
    if (!cam.valid || viewport.x <= 1.0f || viewport.y <= 1.0f) return false;

    float cy = std::cos(cam.yaw);
    float sy = std::sin(cam.yaw);
    float cp = std::cos(cam.pitch);
    float sp = std::sin(cam.pitch);

    Vec3 forward{ cy * cp, sy * cp, sp };
    Vec3 right{ -sy, cy, 0.0f };
    Vec3 up{ -cy * sp, -sy * sp, cp };
    Vec3 rel{ world.x - cam.position.x, world.y - cam.position.y, world.z - cam.position.z };

    float depth = dot(rel, forward);
    if (depth <= 0.05f) return false;

    float aspect = viewport.x / viewport.y;
    float tanHalfFov = std::tan(cam.fov * 0.5f);
    if (tanHalfFov <= 0.001f) return false;

    float screenX = dot(rel, right) / (depth * tanHalfFov * aspect);
    float screenY = dot(rel, up) / (depth * tanHalfFov);
    out.x = viewport.x * 0.5f * (1.0f + screenX);
    out.y = viewport.y * 0.5f * (1.0f - screenY);

    return std::isfinite(out.x) && std::isfinite(out.y) &&
           out.x > -viewport.x && out.x < viewport.x * 2.0f &&
           out.y > -viewport.y && out.y < viewport.y * 2.0f;
}

static void drawPlayer3DBox(const PlayerInfo& player, const CameraInfo& cam) {
    if (!player.valid || !player.hasPosition || !cam.valid) return;

    ImDrawList* draw = ImGui::GetForegroundDrawList();
    ImVec2 viewport = ImGui::GetIO().DisplaySize;

    constexpr float halfWidth = 0.45f;
    constexpr float height = 2.35f;
    Vec3 c = player.position;
    Vec3 corners[8] = {
        { c.x - halfWidth, c.y - halfWidth, c.z },
        { c.x + halfWidth, c.y - halfWidth, c.z },
        { c.x + halfWidth, c.y + halfWidth, c.z },
        { c.x - halfWidth, c.y + halfWidth, c.z },
        { c.x - halfWidth, c.y - halfWidth, c.z + height },
        { c.x + halfWidth, c.y - halfWidth, c.z + height },
        { c.x + halfWidth, c.y + halfWidth, c.z + height },
        { c.x - halfWidth, c.y + halfWidth, c.z + height },
    };

    ImVec2 screen[8];
    for (int i = 0; i < 8; ++i) {
        if (!worldToScreen(corners[i], cam, viewport, screen[i])) return;
    }

    const int edges[12][2] = {
        {0,1}, {1,2}, {2,3}, {3,0},
        {4,5}, {5,6}, {6,7}, {7,4},
        {0,4}, {1,5}, {2,6}, {3,7},
    };
    const ImU32 shadow = IM_COL32(0, 0, 0, 190);
    const ImU32 line = IM_COL32(70, 210, 255, 230);

    for (const auto& e : edges) {
        draw->AddLine(screen[e[0]], screen[e[1]], shadow, 4.0f);
        draw->AddLine(screen[e[0]], screen[e[1]], line, 1.8f);
    }
}

static void setClickThrough(HWND hwnd, bool on) {
    g_clickThrough = on;
    LONG_PTR ex = GetWindowLongPtr(hwnd, GWL_EXSTYLE);
    LONG_PTR want = on ? (ex | WS_EX_TRANSPARENT) : (ex & ~WS_EX_TRANSPARENT);
    if (want != ex) {
        SetWindowLongPtr(hwnd, GWL_EXSTYLE, want);
        SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_FRAMECHANGED);
    }
}

int WINAPI wWinMain(HINSTANCE, HINSTANCE, LPWSTR, int) {
    WNDCLASSEXW wc = { sizeof(wc), CS_CLASSDC, WndProc, 0L, 0L, GetModuleHandle(nullptr),
                       nullptr, nullptr, (HBRUSH)GetStockObject(BLACK_BRUSH), nullptr,
                       L"AscensionOverlay", nullptr };
    ::RegisterClassExW(&wc);

    // Borderless, topmost, tool window, starts click-through & non-activating.
    HWND hwnd = ::CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | WS_EX_TRANSPARENT | WS_EX_LAYERED,
        wc.lpszClassName, L"AscensionOverlay", WS_POPUP,
        0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN),
        nullptr, nullptr, wc.hInstance, nullptr);
    SetLayeredWindowAttributes(hwnd, 0, 255, LWA_ALPHA);
    setClickThrough(hwnd, true);

    // Per-pixel alpha via DWM: extend the "glass" frame across the whole client.
    MARGINS margins = { -1, -1, -1, -1 };
    ::DwmExtendFrameIntoClientArea(hwnd, &margins);

    if (!CreateDeviceD3D(hwnd)) {
        CleanupDeviceD3D();
        ::UnregisterClassW(wc.lpszClassName, wc.hInstance);
        return 1;
    }
    ::ShowWindow(hwnd, SW_SHOWNOACTIVATE);
    ::UpdateWindow(hwnd);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    ImGui::StyleColorsDark();
    applyDeadcellStyle();
    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);

    GameReader game;
    PlayerInfo player;
    CameraInfo camera;
    bool havePlayer = false;
    bool haveCamera = false;
    double retryTimer = 0.0;

    bool menuVisible = false;  // toggled with INSERT
    bool prevInsert = false;
    bool clickThrough = true;  // current window state

    bool done = false;
    while (!done) {
        MSG msg;
        while (::PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE)) {
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
            if (msg.message == WM_QUIT) done = true;
        }
        if (done) break;

        // --- INSERT toggle (global, works while the game has focus) ---------
        bool insDown = (GetAsyncKeyState(VK_INSERT) & 0x8000) != 0;
        if (insDown && !prevInsert) menuVisible = !menuVisible;
        prevInsert = insDown;

        // Emergency keyboard-only exit if input ever feels wrong.
        if ((GetAsyncKeyState(VK_CONTROL) & 0x8000) &&
            (GetAsyncKeyState(VK_SHIFT) & 0x8000) &&
            (GetAsyncKeyState(VK_END) & 0x8000)) {
            done = true;
            break;
        }

        // --- keep the overlay glued on top of the game window ---------------
        if (game.attached()) {
            HWND gw = findGameWindow(game.pid());
            if (gw && !IsIconic(gw)) {
                RECT rc; POINT tl{ 0,0 };
                GetClientRect(gw, &rc);
                ClientToScreen(gw, &tl);
                int w = rc.right - rc.left, h = rc.bottom - rc.top;
                RECT cur; GetWindowRect(hwnd, &cur);
                if (cur.left != tl.x || cur.top != tl.y ||
                    (cur.right - cur.left) != w || (cur.bottom - cur.top) != h) {
                    SetWindowPos(hwnd, HWND_TOPMOST, tl.x, tl.y, w, h, SWP_NOACTIVATE);
                } else {
                    SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0,
                                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
                }
            }
        }

        // --- poll the game (attach lazily, stats ~10x/sec) ------------------
        retryTimer += io.DeltaTime;
        if (!game.attached()) {
            if (retryTimer > 1.0) { game.attach(); retryTimer = 0.0; }
        } else if (retryTimer > 0.1) {
            havePlayer = game.readPlayer(player);
            retryTimer = 0.0;
        }
        haveCamera = game.attached() && game.readCamera(camera);

        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();

        // Feed absolute cursor pos every frame so hover/WantCaptureMouse works
        // even while the window is click-through.
        POINT cp; ::GetCursorPos(&cp); ::ScreenToClient(hwnd, &cp);
        io.AddMousePosEvent((float)cp.x, (float)cp.y);

        ImGui::NewFrame();
        if (havePlayer && haveCamera) drawPlayer3DBox(player, camera);

        if (menuVisible) {
            ImGui::SetNextWindowPos(ImVec2(20, 20), ImGuiCond_FirstUseEver);
            ImGui::SetNextWindowSize(ImVec2(300, 0), ImGuiCond_FirstUseEver);
            ImGui::Begin("Ascension - Player Info", &menuVisible,
                         ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize);
            drawDeadcellWindowChrome();

            if (!game.attached()) {
                ImGui::TextColored(ImVec4(1, 0.5f, 0.3f, 1), "Waiting for Ascension.exe ...");
            } else if (!havePlayer || !player.valid) {
                ImGui::Text("Attached (PID %u)", game.pid());
                ImGui::TextColored(ImVec4(1, 0.8f, 0.3f, 1), "No player in world");
            } else {
                ImGui::Text("Level ");
                ImGui::SameLine();
                ImGui::TextColored(ImVec4(1, 0.9f, 0.4f, 1), "%u", player.level);

                ImGui::Spacing();
                StatBar("Health", (float)player.health, (float)player.maxHealth,
                        ImVec4(0.20f, 0.75f, 0.25f, 1.0f));
                ImGui::Spacing();

                ImVec4 pcol = ImVec4(0.25f, 0.45f, 0.95f, 1.0f);
                if (player.powerType == 1) pcol = ImVec4(0.85f, 0.25f, 0.25f, 1.0f);
                else if (player.powerType == 3) pcol = ImVec4(0.90f, 0.85f, 0.25f, 1.0f);
                StatBar(game.powerName(player.powerType), (float)player.power,
                        (float)player.maxPower, pcol);

                ImGui::Spacing();
                ImGui::Text("Speed ");
                ImGui::SameLine();
                ImGui::TextColored(ImVec4(0.7f, 0.9f, 1.0f, 1), "%.2f", player.speed);
                if (player.hasPosition) {
                    ImGui::Text("Position ");
                    ImGui::SameLine();
                    ImGui::TextColored(ImVec4(0.7f, 0.9f, 1.0f, 1), "%.2f %.2f %.2f",
                                       player.position.x, player.position.y, player.position.z);
                }
                ImGui::Text("3D box ");
                ImGui::SameLine();
                ImGui::TextColored(haveCamera ? ImVec4(0.35f, 0.9f, 0.55f, 1.0f)
                                              : ImVec4(1.0f, 0.65f, 0.3f, 1.0f),
                                   haveCamera ? "active" : "waiting for camera");
                ImGui::Separator();
                ImGui::TextDisabled("GUID 0x%08X%08X", player.guidHigh, player.guidLow);
            }
            ImGui::Separator();
            ImGui::TextDisabled("INSERT to toggle | %.0f FPS", io.Framerate);
            ImGui::End();
        }

        // Keep the overlay hard click-through. The current panel is read-only,
        // so mouse input should never be captured from the game or desktop.
        if (!clickThrough) { setClickThrough(hwnd, true); clickThrough = true; }

        ImGui::Render();
        g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, nullptr);
        const float transparent[4] = { 0.0f, 0.0f, 0.0f, 0.0f }; // fully see-through
        g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, transparent);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        g_pSwapChain->Present(1, 0);
    }

    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    CleanupDeviceD3D();
    setClickThrough(hwnd, true);
    ::ShowWindow(hwnd, SW_HIDE);
    ::DestroyWindow(hwnd);
    ::UnregisterClassW(wc.lpszClassName, wc.hInstance);
    return 0;
}
