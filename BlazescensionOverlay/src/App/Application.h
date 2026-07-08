#pragma once

#include "Core/AppConfig.h"
#include "Core/Types.h"
#include "Memory/GameReader.h"
#include "Overlay/OverlayWindow.h"
#include "Rendering/SceneRenderer.h"

#include <windows.h>

namespace App {

class Application {
public:
    int run(HINSTANCE instance);

private:
    bool handleHotkeys();
    void updateGameWindow();
    void capFrameRate();

    Core::AppConfig m_config{};
    Core::GameSnapshot m_snapshot{};
    Memory::GameReader m_reader;
    Overlay::OverlayWindow m_overlay;
    Rendering::SceneRenderer m_sceneRenderer;
    HWND m_gameWindow = nullptr;
    RECT m_lastWindowRect{};
    bool m_haveWindowRect = false;
    bool m_prevInsertDown = false;
    double m_lastFrameSeconds = 0.0;
    double m_lastWindowSearchSeconds = 0.0;
};

} // namespace App
