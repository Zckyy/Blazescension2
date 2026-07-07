#pragma once

#include <d3d11.h>
#include <dcomp.h>
#include <dxgi1_2.h>
#include <wrl/client.h>
#include <windows.h>

namespace Overlay {

class OverlayWindow {
public:
    OverlayWindow() = default;
    ~OverlayWindow();

    OverlayWindow(const OverlayWindow&) = delete;
    OverlayWindow& operator=(const OverlayWindow&) = delete;

    bool initialize(HINSTANCE instance);
    void shutdown();
    bool processMessages();

    void beginFrame();
    void endFrame();
    void present(int syncInterval);

    void setMenuOpen(bool open);
    void setClickThrough(bool enabled);
    void setStreamProof(bool enabled);
    void moveTo(const RECT& rect);

    HWND hwnd() const { return m_hwnd; }
    int width() const { return m_width; }
    int height() const { return m_height; }
    ID3D11Device* device() const { return m_device.Get(); }
    ID3D11DeviceContext* context() const { return m_context.Get(); }

private:
    static LRESULT CALLBACK wndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    bool createDeviceObjects();
    void destroyDeviceObjects();
    bool createRenderTarget();
    void destroyRenderTarget();
    bool resizeBuffers(int width, int height);
    void updateInputMode();

    HWND m_hwnd = nullptr;
    HINSTANCE m_instance = nullptr;
    bool m_menuOpen = false;
    bool m_clickThrough = true;
    bool m_streamProof = false;
    HWND m_previousForeground = nullptr;
    int m_width = 1280;
    int m_height = 720;

    Microsoft::WRL::ComPtr<ID3D11Device> m_device;
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> m_context;
    Microsoft::WRL::ComPtr<IDXGISwapChain1> m_swapChain;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView> m_renderTarget;
    Microsoft::WRL::ComPtr<IDCompositionDevice> m_dcompDevice;
    Microsoft::WRL::ComPtr<IDCompositionTarget> m_dcompTarget;
    Microsoft::WRL::ComPtr<IDCompositionVisual> m_dcompVisual;
};

} // namespace Overlay

