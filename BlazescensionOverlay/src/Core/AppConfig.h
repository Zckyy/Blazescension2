#pragma once

namespace Core {

enum class BoxDrawMode {
    TwoD = 0,
    ThreeD = 1,
};

struct AppConfig {
    bool showMenu = false;
    bool showLocalPlayerBox = true;
    bool showTargetBox = true;
    bool showFocusBox = false;
    bool showMouseoverBox = false;
    bool showNpcBoxes = false;
    bool showOtherPlayerBoxes = false;
    bool showTargetLine = false;
    bool showLocalPlayerCircle = false;
    bool showTargetCircle = false;
    bool showLocalPlayerName = true;
    bool showTargetName = true;
    bool showFocusName = false;
    bool showMouseoverName = false;
    bool showNpcNames = false;
    bool showOtherPlayerNames = false;
    bool streamProof = false;
    bool requestExit = false;
    BoxDrawMode boxDrawMode = BoxDrawMode::TwoD;

    float circleRadius = 1.20f;

    // Enumerating the whole object-manager hash table is far pricier than the
    // fixed GUID lookups, so it runs on its own slow poll and is capped by
    // range/count.
    float nearbyRadius = 60.0f;
    int nearbyMaxCount = 40;
    int nearbyPollHz = 4;
};

} // namespace Core
