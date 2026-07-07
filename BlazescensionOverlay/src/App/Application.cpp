#include "Application.h"

#include "Overlay/WindowTracker.h"
#include "UI/Menu.h"

#include <algorithm>
#include <chrono>
#include <thread>

#include <timeapi.h>

#pragma comment(lib, "winmm.lib")

namespace App {

namespace {

double nowSeconds() {
    using clock = std::chrono::steady_clock;
    static const auto start = clock::now();
    const auto elapsed = clock::now() - start;
    return std::chrono::duration<double>(elapsed).count();
}

} // namespace

int Application::run(HINSTANCE instance) {
    if (!m_overlay.initialize(instance)) {
        return 1;
    }

    UI::applyBlazeStyle();
    // Default Windows timer resolution (~15.6 ms) makes sleep_for overshoot
    // badly; 1 ms resolution keeps the frame cap accurate.
    timeBeginPeriod(1);
    m_lastFrameSeconds = nowSeconds();
    m_lastSnapshotSeconds = 0.0;

    bool running = true;
    while (running) {
        running = m_overlay.processMessages();
        if (!running || !handleHotkeys() || m_config.requestExit) {
            break;
        }

        updateSnapshot();
        updateGameWindow();
        m_overlay.setMenuOpen(m_config.showMenu);
        m_overlay.setStreamProof(m_config.streamProof);

        m_overlay.beginFrame();
        m_sceneRenderer.draw(m_snapshot, m_config);
        UI::drawStatusPanel(m_config, m_snapshot);
        if (m_config.showMenu) {
            UI::drawMenu(m_config, m_snapshot);
        }
        m_overlay.endFrame();
        m_overlay.present(0);
        capFrameRate();
    }

    timeEndPeriod(1);
    m_overlay.shutdown();
    return 0;
}

bool Application::handleHotkeys() {
    const bool insertDown = (GetAsyncKeyState(VK_INSERT) & 0x8000) != 0;
    if (insertDown && !m_prevInsertDown) {
        m_config.showMenu = !m_config.showMenu;
    }
    m_prevInsertDown = insertDown;

    const bool exitChord =
        (GetAsyncKeyState(VK_CONTROL) & 0x8000) &&
        (GetAsyncKeyState(VK_SHIFT) & 0x8000) &&
        (GetAsyncKeyState(VK_END) & 0x8000);

    // DELETE is a bare panic key: no modifiers required, so it works even if
    // the overlay window has stolen focus and is eating other chords.
    const bool panicKey = (GetAsyncKeyState(VK_DELETE) & 0x8000) != 0;

    return !exitChord && !panicKey;
}

void Application::updateSnapshot() {
    const double now = nowSeconds();
    const int pollHz = std::clamp(m_config.pollHz, 1, 120);
    const double interval = 1.0 / static_cast<double>(pollHz);

    if (now - m_lastSnapshotSeconds >= interval) {
        m_snapshot = m_reader.readSnapshot();
        m_lastSnapshotSeconds = now;
    }
}

void Application::updateGameWindow() {
    if (!m_snapshot.attached || !m_snapshot.pid) {
        return;
    }

    if (!m_gameWindow || !IsWindow(m_gameWindow)) {
        // EnumWindows every frame is wasteful while the game window is
        // missing (minimized to tray, still loading); retry at 2 Hz.
        const double now = nowSeconds();
        if (now - m_lastWindowSearchSeconds < 0.5) {
            return;
        }
        m_lastWindowSearchSeconds = now;
        m_gameWindow = nullptr;
        if (!Overlay::findMainWindowForPid(m_snapshot.pid, m_gameWindow)) {
            return;
        }
        m_haveWindowRect = false;
    }

    RECT rect{};
    if (Overlay::getClientScreenRect(m_gameWindow, rect)) {
        const bool changed = !m_haveWindowRect ||
            rect.left != m_lastWindowRect.left || rect.top != m_lastWindowRect.top ||
            rect.right != m_lastWindowRect.right || rect.bottom != m_lastWindowRect.bottom;
        if (changed) {
            m_overlay.moveTo(rect);
            m_lastWindowRect = rect;
            m_haveWindowRect = true;
        }
    }
}

void Application::capFrameRate() {
    const int fps = std::clamp(m_config.overlayFps, 30, 360);
    const double target = 1.0 / static_cast<double>(fps);
    const double now = nowSeconds();
    const double elapsed = now - m_lastFrameSeconds;
    if (elapsed < target) {
        const auto sleepFor = std::chrono::duration<double>(target - elapsed);
        std::this_thread::sleep_for(sleepFor);
    }
    m_lastFrameSeconds = nowSeconds();
}

} // namespace App

