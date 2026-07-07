#include "Menu.h"

#include "Rendering/Projection.h"
#include "imgui.h"

#include <algorithm>
#include <cfloat>
#include <cstdio>

namespace UI {

namespace {

constexpr ImVec4 kAccent{ 0.38f, 0.45f, 0.90f, 1.0f };
constexpr ImVec4 kGood{ 0.35f, 0.95f, 0.55f, 1.0f };
constexpr ImVec4 kWarn{ 1.0f, 0.65f, 0.30f, 1.0f };
constexpr ImVec4 kBad{ 0.95f, 0.35f, 0.35f, 1.0f };

const char* relationName(Core::UnitRelation relation) {
    switch (relation) {
    case Core::UnitRelation::LocalPlayer: return "Player";
    case Core::UnitRelation::Target: return "Target";
    case Core::UnitRelation::Focus: return "Focus";
    case Core::UnitRelation::Mouseover: return "Mouseover";
    case Core::UnitRelation::Npc: return "NPC";
    case Core::UnitRelation::OtherPlayer: return "Other player";
    default: return "Unit";
    }
}

const char* powerName(uint8_t type) {
    switch (type) {
    case 0: return "Mana";
    case 1: return "Rage";
    case 2: return "Focus";
    case 3: return "Energy";
    case 4: return "Happiness";
    case 5: return "Runes";
    case 6: return "Runic Power";
    default: return "Power";
    }
}

ImVec4 powerColor(uint8_t type) {
    switch (type) {
    case 1: return ImVec4(0.85f, 0.25f, 0.25f, 1.0f);
    case 3: return ImVec4(0.90f, 0.82f, 0.25f, 1.0f);
    case 6: return ImVec4(0.25f, 0.65f, 0.95f, 1.0f);
    default: return ImVec4(0.25f, 0.45f, 0.95f, 1.0f);
    }
}

void glowRect(ImDrawList* draw, ImVec2 min, ImVec2 max, ImVec4 color, float rounding) {
    for (int i = 4; i >= 1; --i) {
        const float t = static_cast<float>(i) / 4.0f;
        const float spread = 7.0f * t;
        const float alpha = 0.18f * (1.0f - t) * (1.0f - t);
        draw->AddRectFilled(
            ImVec2(min.x - spread, min.y - spread),
            ImVec2(max.x + spread, max.y + spread),
            ImGui::ColorConvertFloat4ToU32(ImVec4(color.x, color.y, color.z, alpha)),
            rounding + spread);
    }
}

void statusDot(bool active) {
    const ImVec4 color = active ? kGood : kBad;
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    ImDrawList* draw = ImGui::GetWindowDrawList();
    draw->AddCircleFilled(ImVec2(pos.x + 5.0f, pos.y + 8.0f), 4.0f, ImGui::ColorConvertFloat4ToU32(color), 12);
    ImGui::Dummy(ImVec2(14.0f, 16.0f));
    ImGui::SameLine();
}

void unitSummary(const Core::UnitSnapshot& unit) {
    statusDot(unit.valid);
    ImGui::TextUnformatted(relationName(unit.relation));

    if (!unit.valid) {
        ImGui::SameLine();
        ImGui::TextDisabled("not resolved");
        return;
    }

    if (unit.hasName && unit.name[0] != '\0') {
        ImGui::SameLine();
        ImGui::TextColored(kAccent, "%s", unit.name);
    }

    ImGui::Text("Level %u", unit.level);

    const float healthFrac = unit.maxHealth ? std::clamp(static_cast<float>(unit.health) / unit.maxHealth, 0.0f, 1.0f) : 0.0f;
    char healthText[64];
    std::snprintf(healthText, sizeof(healthText), "%u / %u", unit.health, unit.maxHealth);
    ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.20f, 0.75f, 0.25f, 1.0f));
    ImGui::ProgressBar(healthFrac, ImVec2(-FLT_MIN, 18.0f), healthText);
    ImGui::PopStyleColor();

    const float powerFrac = unit.maxPower ? std::clamp(static_cast<float>(unit.power) / unit.maxPower, 0.0f, 1.0f) : 0.0f;
    char powerText[80];
    std::snprintf(powerText, sizeof(powerText), "%s %u / %u", powerName(unit.powerType), unit.power, unit.maxPower);
    ImGui::PushStyleColor(ImGuiCol_PlotHistogram, powerColor(unit.powerType));
    ImGui::ProgressBar(powerFrac, ImVec2(-FLT_MIN, 18.0f), powerText);
    ImGui::PopStyleColor();

    if (unit.hasPosition) {
        ImGui::TextDisabled("pos %.2f %.2f %.2f | speed %.2f",
                            unit.position.x, unit.position.y, unit.position.z, unit.speed);
    }
}

void sectionTitle(const char* title, const char* description) {
    ImGui::TextColored(kAccent, "%s", title);
    ImGui::TextDisabled("%s", description);
    ImGui::Separator();
}

} // namespace

void applyBlazeStyle() {
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 8.0f;
    style.ChildRounding = 7.0f;
    style.FrameRounding = 5.0f;
    style.GrabRounding = 5.0f;
    style.PopupRounding = 6.0f;
    style.ScrollbarRounding = 7.0f;
    style.TabRounding = 5.0f;
    style.WindowBorderSize = 0.0f;
    style.ChildBorderSize = 1.0f;
    style.FrameBorderSize = 0.0f;
    style.WindowPadding = ImVec2(14.0f, 12.0f);
    style.FramePadding = ImVec2(10.0f, 6.0f);
    style.ItemSpacing = ImVec2(10.0f, 8.0f);
    style.ItemInnerSpacing = ImVec2(8.0f, 5.0f);
    style.ScrollbarSize = 12.0f;

    ImVec4* colors = style.Colors;
    colors[ImGuiCol_Text] = ImVec4(0.95f, 0.96f, 0.98f, 1.00f);
    colors[ImGuiCol_TextDisabled] = ImVec4(0.50f, 0.53f, 0.60f, 1.00f);
    colors[ImGuiCol_WindowBg] = ImVec4(0.08f, 0.08f, 0.11f, 0.93f);
    colors[ImGuiCol_ChildBg] = ImVec4(0.10f, 0.10f, 0.14f, 0.55f);
    colors[ImGuiCol_PopupBg] = ImVec4(0.10f, 0.10f, 0.14f, 0.98f);
    colors[ImGuiCol_Border] = ImVec4(0.30f, 0.33f, 0.48f, 0.30f);
    colors[ImGuiCol_FrameBg] = ImVec4(0.16f, 0.16f, 0.21f, 0.78f);
    colors[ImGuiCol_FrameBgHovered] = ImVec4(0.22f, 0.23f, 0.32f, 0.90f);
    colors[ImGuiCol_FrameBgActive] = ImVec4(0.27f, 0.28f, 0.40f, 1.00f);
    colors[ImGuiCol_CheckMark] = kAccent;
    colors[ImGuiCol_SliderGrab] = ImVec4(0.38f, 0.45f, 0.90f, 1.00f);
    colors[ImGuiCol_SliderGrabActive] = ImVec4(0.50f, 0.58f, 1.00f, 1.00f);
    colors[ImGuiCol_Button] = ImVec4(0.20f, 0.22f, 0.35f, 0.75f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.28f, 0.30f, 0.50f, 0.90f);
    colors[ImGuiCol_ButtonActive] = kAccent;
    colors[ImGuiCol_Header] = ImVec4(0.20f, 0.20f, 0.30f, 0.70f);
    colors[ImGuiCol_HeaderHovered] = ImVec4(0.28f, 0.30f, 0.50f, 0.80f);
    colors[ImGuiCol_HeaderActive] = kAccent;
    colors[ImGuiCol_Separator] = ImVec4(0.30f, 0.33f, 0.48f, 0.30f);
}

void drawMenu(Core::AppConfig& config, const Core::GameSnapshot& snapshot) {
    ImGui::SetNextWindowSize(ImVec2(650.0f, 540.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSizeConstraints(ImVec2(560.0f, 420.0f), ImVec2(FLT_MAX, FLT_MAX));
    // The window's X button quits the app outright (matches user expectation
    // for an overlay close button); INSERT is what merely hides the menu.
    bool windowOpen = true;
    ImGui::Begin("Blazescension Control", &windowOpen, ImGuiWindowFlags_NoCollapse);
    if (!windowOpen) {
        config.requestExit = true;
    }

    ImDrawList* draw = ImGui::GetWindowDrawList();
    const ImVec2 p = ImGui::GetCursorScreenPos();
    const float width = ImGui::GetContentRegionAvail().x;
    const float headerHeight = 42.0f;

    ImVec2 logoCenter{ p.x + 16.0f, p.y + headerHeight * 0.5f };
    draw->AddCircleFilled(logoCenter, 10.0f, ImGui::ColorConvertFloat4ToU32(ImVec4(kAccent.x, kAccent.y, kAccent.z, 0.25f)), 24);
    draw->AddCircle(logoCenter, 10.0f, ImGui::ColorConvertFloat4ToU32(kAccent), 24, 1.5f);
    draw->AddCircleFilled(logoCenter, 3.8f, ImGui::ColorConvertFloat4ToU32(kAccent), 16);

    ImGui::SetCursorScreenPos(ImVec2(p.x + 38.0f, p.y + 4.0f));
    ImGui::TextColored(kAccent, "Blazescension");
    ImGui::SetCursorScreenPos(ImVec2(p.x + 38.0f, p.y + 22.0f));
    ImGui::TextDisabled("External read-only overlay");

    const char* status = snapshot.attached ? "ATTACHED" : "WAITING";
    const ImVec4 statusColor = snapshot.attached ? kGood : kWarn;
    const ImVec2 textSize = ImGui::CalcTextSize(status);
    const ImVec2 pillMin{ p.x + width - textSize.x - 34.0f, p.y + 9.0f };
    const ImVec2 pillMax{ p.x + width, p.y + 33.0f };
    glowRect(draw, pillMin, pillMax, statusColor, 12.0f);
    draw->AddRectFilled(pillMin, pillMax, ImGui::ColorConvertFloat4ToU32(ImVec4(statusColor.x, statusColor.y, statusColor.z, 0.14f)), 12.0f);
    draw->AddRect(pillMin, pillMax, ImGui::ColorConvertFloat4ToU32(ImVec4(statusColor.x, statusColor.y, statusColor.z, 0.55f)), 12.0f, 0, 1.2f);
    draw->AddCircleFilled(ImVec2(pillMin.x + 13.0f, (pillMin.y + pillMax.y) * 0.5f), 3.0f, ImGui::ColorConvertFloat4ToU32(statusColor), 12);
    draw->AddText(ImVec2(pillMin.x + 22.0f, pillMin.y + 4.0f), ImGui::ColorConvertFloat4ToU32(statusColor), status);

    ImGui::SetCursorScreenPos(ImVec2(p.x, p.y + headerHeight + 8.0f));
    ImGui::Separator();

    if (ImGui::BeginTabBar("##MainTabs")) {
        if (ImGui::BeginTabItem("Visuals")) {
            sectionTitle("Draw Layers", "Toggle independent overlay layers.");
            ImGui::Checkbox("Local player box", &config.showLocalPlayerBox);
            ImGui::Checkbox("Target box", &config.showTargetBox);
            ImGui::Checkbox("Target snap line", &config.showTargetLine);
            ImGui::Checkbox("Focus box", &config.showFocusBox);
            ImGui::Checkbox("Mouseover box", &config.showMouseoverBox);
            ImGui::Checkbox("Unit names", &config.showUnitNames);
            ImGui::Checkbox("Status panel", &config.showStatusPanel);
            ImGui::Checkbox("Debug panel", &config.showDebugPanel);
            ImGui::Spacing();
            ImGui::Checkbox("Local player circle", &config.showLocalPlayerCircle);
            ImGui::Checkbox("Target circle", &config.showTargetCircle);
            if (config.showLocalPlayerCircle || config.showTargetCircle) {
                ImGui::SliderFloat("Circle radius (yd)", &config.circleRadius, 0.30f, 8.0f, "%.2f");
            }
            ImGui::Spacing();
            ImGui::Checkbox("Nearby NPC boxes", &config.showNpcBoxes);
            ImGui::Checkbox("Nearby player boxes", &config.showOtherPlayerBoxes);
            if (config.showNpcBoxes || config.showOtherPlayerBoxes) {
                ImGui::SliderFloat("Nearby radius (yd)", &config.nearbyRadius, 10.0f, 200.0f, "%.0f");
                ImGui::SliderInt("Nearby max count", &config.nearbyMaxCount, 5, 150);
                ImGui::SliderInt("Nearby scan Hz", &config.nearbyPollHz, 1, 20);
                ImGui::TextDisabled("Scans the full object list; kept slower than the main poll rate.");
            }
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Tuning")) {
            sectionTitle("Box Geometry", "These settings apply to every unit box.");
            const char* boxModeItems[] = { "Screen aligned", "World 3D" };
            int boxModeIndex = static_cast<int>(config.boxDrawMode);
            if (ImGui::Combo("Box mode", &boxModeIndex, boxModeItems, IM_ARRAYSIZE(boxModeItems))) {
                config.boxDrawMode = static_cast<Core::BoxDrawMode>(boxModeIndex);
            }
            ImGui::SliderFloat("Height", &config.boxHeight, 0.80f, 4.50f, "%.2f");
            if (config.boxDrawMode == Core::BoxDrawMode::ScreenAligned) {
                ImGui::SliderFloat("Width ratio", &config.screenBoxWidthRatio, 0.20f, 1.20f, "%.2f");
            } else {
                ImGui::SliderFloat("World width", &config.boxWidth, 0.30f, 2.50f, "%.2f");
            }
            ImGui::SliderFloat("Line thickness", &config.lineThickness, 1.0f, 5.0f, "%.1f");
            ImGui::Separator();
            const char* projectionItems[] = {
                "Auto",
                "Yaw X",
                "Yaw X, inv pitch",
                "Yaw Y",
                "Yaw Y, inv pitch",
                "Yaw -X",
                "Yaw -Y",
            };
            int projectionIndex = static_cast<int>(config.projectionMode);
            if (ImGui::Combo("Projection", &projectionIndex, projectionItems, IM_ARRAYSIZE(projectionItems))) {
                config.projectionMode = static_cast<Core::ProjectionMode>(projectionIndex);
            }
            ImGui::Checkbox("Projection debug marker", &config.showProjectionDebug);
            ImGui::Separator();
            ImGui::SliderInt("Memory poll Hz", &config.pollHz, 5, 60);
            ImGui::SliderInt("Overlay FPS cap", &config.overlayFps, 30, 240);
            ImGui::Checkbox("Hide overlay from capture", &config.streamProof);
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Memory")) {
            sectionTitle("Resolved Units", "GUID-backed unit snapshots read from Ascension.");
            unitSummary(snapshot.player);
            ImGui::Spacing();
            unitSummary(snapshot.target);
            ImGui::Spacing();
            unitSummary(snapshot.focus);
            ImGui::Spacing();
            unitSummary(snapshot.mouseover);
            if (config.showNpcBoxes || config.showOtherPlayerBoxes) {
                ImGui::Spacing();
                ImGui::Separator();
                ImGui::TextDisabled("Nearby: %zu NPC%s, %zu player%s",
                                    snapshot.nearbyNpcs.size(), snapshot.nearbyNpcs.size() == 1 ? "" : "s",
                                    snapshot.nearbyPlayers.size(), snapshot.nearbyPlayers.size() == 1 ? "" : "s");
            }
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Diagnostics")) {
            sectionTitle("Runtime", "Addresses are runtime module-base adjusted.");
            ImGui::Text("PID: %u", snapshot.pid);
            ImGui::Text("Module base: 0x%08X", snapshot.moduleBase);
            ImGui::Text("Camera: %s", snapshot.camera.valid ? "ready" : "waiting");
            if (snapshot.camera.valid) {
                const ImVec2 display = ImGui::GetIO().DisplaySize;
                const Rendering::ProjectionBasis basis =
                    Rendering::chooseProjectionBasis(snapshot, Core::Vec2{ display.x, display.y }, config.projectionMode);
                ImGui::Text("Projection: %s", basis.name);
                ImGui::Text("Camera matrix: %s", snapshot.camera.hasMatrix ? "ready" : "missing");
            }
            if (snapshot.camera.valid) {
                ImGui::TextDisabled("cam %.2f %.2f %.2f | yaw %.3f pitch %.3f fov %.3f",
                                    snapshot.camera.position.x,
                                    snapshot.camera.position.y,
                                    snapshot.camera.position.z,
                                    snapshot.camera.yaw,
                                    snapshot.camera.pitch,
                                    snapshot.camera.fov);
            }
            ImGui::TextDisabled("INSERT toggles menu | DELETE or Ctrl+Shift+End exits");
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }

    ImGui::End();
}

void drawStatusPanel(const Core::AppConfig& config, const Core::GameSnapshot& snapshot) {
    if (!config.showStatusPanel) {
        return;
    }

    ImGui::SetNextWindowPos(ImVec2(18.0f, 18.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(310.0f, 0.0f), ImGuiCond_FirstUseEver);
    ImGui::Begin("Ascension Status", nullptr,
                 ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize |
                     ImGuiWindowFlags_NoSavedSettings);

    if (!snapshot.attached) {
        ImGui::TextColored(kWarn, "Waiting for Ascension.exe");
    } else if (!snapshot.player.valid) {
        ImGui::TextColored(kWarn, "Attached. Waiting for player in world.");
    } else {
        unitSummary(snapshot.player);
    }

    if (config.showDebugPanel) {
        ImGui::Separator();
        ImGui::TextDisabled("PID %u | base 0x%08X | %.0f FPS", snapshot.pid, snapshot.moduleBase, ImGui::GetIO().Framerate);
        if (snapshot.target.valid) {
            ImGui::TextDisabled("target 0x%08X%08X", snapshot.target.guid.high, snapshot.target.guid.low);
        }
    }

    ImGui::End();
}

} // namespace UI
