#pragma once

namespace Core {

enum class ProjectionMode {
    Auto = 0,
    YawX = 1,
    YawXInvertPitch = 2,
    YawY = 3,
    YawYInvertPitch = 4,
    YawNegX = 5,
    YawNegY = 6,
};

enum class BoxDrawMode {
    ScreenAligned = 0,
    World3D = 1,
};

struct AppConfig {
    bool showMenu = false;
    bool showStatusPanel = true;
    bool showLocalPlayerBox = true;
    bool showTargetBox = true;
    bool showFocusBox = false;
    bool showMouseoverBox = false;
    bool showNpcBoxes = false;
    bool showOtherPlayerBoxes = false;
    bool showTargetLine = false;
    bool showLocalPlayerCircle = false;
    bool showTargetCircle = false;
    bool showUnitNames = true;
    bool showDebugPanel = false;
    bool showProjectionDebug = false;
    bool streamProof = false;
    bool requestExit = false;
    ProjectionMode projectionMode = ProjectionMode::Auto;
    BoxDrawMode boxDrawMode = BoxDrawMode::ScreenAligned;

    float boxWidth = 0.90f;
    float boxHeight = 2.35f;
    float screenBoxWidthRatio = 0.42f;
    float circleRadius = 1.20f;
    float lineThickness = 1.8f;
    int pollHz = 20;
    int overlayFps = 144;

    // Enumerating the whole object-manager hash table is far pricier than the
    // fixed GUID lookups, so it runs on its own slow poll independent of
    // pollHz, and is capped by range/count.
    float nearbyRadius = 60.0f;
    int nearbyMaxCount = 40;
    int nearbyPollHz = 4;
};

} // namespace Core
