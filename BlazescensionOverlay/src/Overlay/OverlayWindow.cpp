#include "OverlayWindow.h"

#include "imgui.h"
#include "imgui_impl_dx11.h"
#include "imgui_impl_win32.h"

#include <algorithm>
#include <iterator>

#pragma comment(lib, "dcomp.lib")

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace Overlay {

namespace {
constexpr const wchar_t* kWindowClass = L"BlazescensionOverlayWindow";
}

OverlayWindow::~OverlayWindow() {
    shutdown();
}

bool OverlayWindow::initialize(HINSTANCE instance) {
    m_instance = instance;

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.style = CS_CLASSDC;
    wc.lpfnWndProc = OverlayWindow::wndProc;
    wc.hInstance = instance;
    wc.lpszClassName = kWindowClass;
    RegisterClassExW(&wc);

    m_width = std::max(640, GetSystemMetrics(SM_CXSCREEN));
    m_height = std::max(480, GetSystemMetrics(SM_CYSCREEN));

    m_hwnd = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TRANSPARENT | WS_EX_LAYERED | WS_EX_NOACTIVATE |
            WS_EX_TOOLWINDOW | WS_EX_NOREDIRECTIONBITMAP,
        kWindowClass,
        L"BlazescensionOverlay",
        WS_POPUP,
        0,
        0,
        m_width,
        m_height,
        nullptr,
        nullptr,
        instance,
        this);

    if (!m_hwnd) {
        return false;
    }

    SetLayeredWindowAttributes(m_hwnd, 0, 255, LWA_ALPHA);

    if (!createDeviceObjects()) {
        shutdown();
        return false;
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.IniFilename = nullptr;

    ImGui_ImplWin32_Init(m_hwnd);
    ImGui_ImplDX11_Init(m_device.Get(), m_context.Get());

    ShowWindow(m_hwnd, SW_SHOWNOACTIVATE);
    UpdateWindow(m_hwnd);
    setClickThrough(true);
    return true;
}

void OverlayWindow::shutdown() {
    if (ImGui::GetCurrentContext()) {
        ImGui_ImplDX11_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
    }

    destroyDeviceObjects();

    if (m_hwnd) {
        DestroyWindow(m_hwnd);
        m_hwnd = nullptr;
    }

    if (m_instance) {
        UnregisterClassW(kWindowClass, m_instance);
        m_instance = nullptr;
    }
}

bool OverlayWindow::processMessages() {
    MSG msg{};
    while (PeekMessageW(&msg, nullptr, 0U, 0U, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
        if (msg.message == WM_QUIT) {
            return false;
        }
    }

    updateInputMode();
    return true;
}

void OverlayWindow::beginFrame() {
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();

    ImGuiIO& io = ImGui::GetIO();
    POINT cursor{};
    if (GetCursorPos(&cursor)) {
        ScreenToClient(m_hwnd, &cursor);
        io.AddMousePosEvent(static_cast<float>(cursor.x), static_cast<float>(cursor.y));
    }

    ImGui::NewFrame();
}

void OverlayWindow::endFrame() {
    ImGui::Render();

    const float transparent[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    m_context->OMSetRenderTargets(1, m_renderTarget.GetAddressOf(), nullptr);
    m_context->ClearRenderTargetView(m_renderTarget.Get(), transparent);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}

void OverlayWindow::present(int syncInterval) {
    if (m_swapChain) {
        m_swapChain->Present(static_cast<UINT>(syncInterval), 0);
    }
}

void OverlayWindow::setMenuOpen(bool open) {
    m_menuOpen = open;
    updateInputMode();
}

void OverlayWindow::setClickThrough(bool enabled) {
    if (!m_hwnd) {
        return;
    }

    m_clickThrough = enabled;
    LONG_PTR exStyle = GetWindowLongPtrW(m_hwnd, GWL_EXSTYLE);
    LONG_PTR wanted = enabled ? (exStyle | WS_EX_TRANSPARENT) : (exStyle & ~WS_EX_TRANSPARENT);
    if (wanted != exStyle) {
        SetWindowLongPtrW(m_hwnd, GWL_EXSTYLE, wanted);
        SetWindowPos(m_hwnd, HWND_TOPMOST, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_FRAMECHANGED);
    }
}

void OverlayWindow::setStreamProof(bool enabled) {
    if (!m_hwnd || m_streamProof == enabled) {
        return;
    }

    m_streamProof = enabled;
    SetWindowDisplayAffinity(m_hwnd, enabled ? WDA_EXCLUDEFROMCAPTURE : WDA_NONE);
}

void OverlayWindow::moveTo(const RECT& rect) {
    if (!m_hwnd) {
        return;
    }

    const int width = std::max(1L, rect.right - rect.left);
    const int height = std::max(1L, rect.bottom - rect.top);
    if (width != m_width || height != m_height) {
        resizeBuffers(width, height);
    }

    SetWindowPos(m_hwnd, HWND_TOPMOST, rect.left, rect.top, width, height, SWP_NOACTIVATE);
}

LRESULT CALLBACK OverlayWindow::wndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam)) {
        return TRUE;
    }

    if (msg == WM_NCCREATE) {
        const auto* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(create->lpCreateParams));
    }

    auto* window = reinterpret_cast<OverlayWindow*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (msg == WM_NCHITTEST && window && window->m_clickThrough) {
        return HTTRANSPARENT;
    }

    switch (msg) {
    case WM_SYSCOMMAND:
        if ((wParam & 0xFFF0) == SC_KEYMENU) {
            return 0;
        }
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    default:
        break;
    }

    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

bool OverlayWindow::createDeviceObjects() {
    UINT flags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
    D3D_FEATURE_LEVEL featureLevel{};
    const D3D_FEATURE_LEVEL levels[] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0 };

    HRESULT hr = D3D11CreateDevice(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        flags,
        levels,
        static_cast<UINT>(std::size(levels)),
        D3D11_SDK_VERSION,
        &m_device,
        &featureLevel,
        &m_context);

    if (FAILED(hr)) {
        return false;
    }

    Microsoft::WRL::ComPtr<IDXGIDevice> dxgiDevice;
    Microsoft::WRL::ComPtr<IDXGIAdapter> adapter;
    Microsoft::WRL::ComPtr<IDXGIFactory2> factory;
    hr = m_device.As(&dxgiDevice);
    if (SUCCEEDED(hr)) hr = dxgiDevice->GetAdapter(&adapter);
    if (SUCCEEDED(hr)) hr = adapter->GetParent(IID_PPV_ARGS(&factory));
    if (FAILED(hr)) {
        return false;
    }

    DXGI_SWAP_CHAIN_DESC1 desc{};
    desc.Width = static_cast<UINT>(m_width);
    desc.Height = static_cast<UINT>(m_height);
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.BufferCount = 2;
    desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    desc.AlphaMode = DXGI_ALPHA_MODE_PREMULTIPLIED;

    hr = factory->CreateSwapChainForComposition(m_device.Get(), &desc, nullptr, &m_swapChain);
    if (FAILED(hr)) {
        return false;
    }

    hr = DCompositionCreateDevice(dxgiDevice.Get(), IID_PPV_ARGS(&m_dcompDevice));
    if (SUCCEEDED(hr)) hr = m_dcompDevice->CreateTargetForHwnd(m_hwnd, TRUE, &m_dcompTarget);
    if (SUCCEEDED(hr)) hr = m_dcompDevice->CreateVisual(&m_dcompVisual);
    if (SUCCEEDED(hr)) hr = m_dcompVisual->SetContent(m_swapChain.Get());
    if (SUCCEEDED(hr)) hr = m_dcompTarget->SetRoot(m_dcompVisual.Get());
    if (SUCCEEDED(hr)) hr = m_dcompDevice->Commit();
    if (FAILED(hr)) {
        return false;
    }

    return createRenderTarget();
}

void OverlayWindow::destroyDeviceObjects() {
    destroyRenderTarget();
    m_dcompVisual.Reset();
    m_dcompTarget.Reset();
    m_dcompDevice.Reset();
    m_swapChain.Reset();
    m_context.Reset();
    m_device.Reset();
}

bool OverlayWindow::createRenderTarget() {
    Microsoft::WRL::ComPtr<ID3D11Texture2D> backBuffer;
    HRESULT hr = m_swapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer));
    if (FAILED(hr)) {
        return false;
    }

    hr = m_device->CreateRenderTargetView(backBuffer.Get(), nullptr, &m_renderTarget);
    return SUCCEEDED(hr);
}

void OverlayWindow::destroyRenderTarget() {
    m_renderTarget.Reset();
}

bool OverlayWindow::resizeBuffers(int width, int height) {
    if (!m_swapChain || width <= 0 || height <= 0) {
        return false;
    }

    destroyRenderTarget();
    HRESULT hr = m_swapChain->ResizeBuffers(0, static_cast<UINT>(width), static_cast<UINT>(height),
                                            DXGI_FORMAT_UNKNOWN, 0);
    if (FAILED(hr)) {
        return false;
    }

    m_width = width;
    m_height = height;
    return createRenderTarget();
}

void OverlayWindow::updateInputMode() {
    if (!m_hwnd || !ImGui::GetCurrentContext()) {
        return;
    }

    ImGuiIO& io = ImGui::GetIO();
    const bool wantsInput = m_menuOpen || io.WantCaptureMouse || io.WantTextInput;
    setClickThrough(!wantsInput);

    static bool hadFocus = false;
    LONG_PTR exStyle = GetWindowLongPtrW(m_hwnd, GWL_EXSTYLE);
    if (wantsInput && !hadFocus) {
        m_previousForeground = GetForegroundWindow();
        SetWindowLongPtrW(m_hwnd, GWL_EXSTYLE, exStyle & ~WS_EX_NOACTIVATE);
        SetForegroundWindow(m_hwnd);
        SetFocus(m_hwnd);
        ClipCursor(nullptr);
        hadFocus = true;
    } else if (!wantsInput && hadFocus) {
        SetWindowLongPtrW(m_hwnd, GWL_EXSTYLE, exStyle | WS_EX_NOACTIVATE);
        if (m_previousForeground && IsWindow(m_previousForeground) && m_previousForeground != m_hwnd) {
            SetForegroundWindow(m_previousForeground);
        }
        m_previousForeground = nullptr;
        hadFocus = false;
    }
}

} // namespace Overlay
