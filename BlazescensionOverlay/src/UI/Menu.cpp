#include "Menu.h"

#include "imgui.h"

#include <algorithm>
#include <cfloat>
#include <cstdio>

namespace UI {

namespace {

constexpr ImVec4 kAccent{ 0.93f, 0.43f, 0.56f, 1.0f };
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

bool navButton(const char* label, bool active, ImVec2 size) {
    ImGui::PushStyleColor(ImGuiCol_Button, active ? ImVec4(0.16f, 0.20f, 0.27f, 1.0f)
                                                  : ImVec4(0.10f, 0.13f, 0.18f, 0.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.18f, 0.22f, 0.29f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.20f, 0.24f, 0.31f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Text, active ? ImVec4(0.94f, 0.95f, 0.98f, 1.0f)
                                                : ImVec4(0.56f, 0.62f, 0.72f, 1.0f));
    const bool pressed = ImGui::Button(label, size);
    ImGui::PopStyleColor(4);
    return pressed;
}

void beginPanel(const char* id, const char* title, ImVec2 size) {
    ImGui::BeginChild(id, size, ImGuiChildFlags_Borders);
    ImGui::TextColored(ImVec4(0.63f, 0.69f, 0.78f, 1.0f), "%s", title);
    ImGui::Separator();
    ImGui::Spacing();
}

void endPanel() {
    ImGui::EndChild();
}

} // namespace

void applyBlazeStyle() {
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 3.0f;
    style.ChildRounding = 4.0f;
    style.FrameRounding = 4.0f;
    style.GrabRounding = 3.0f;
    style.PopupRounding = 4.0f;
    style.ScrollbarRounding = 3.0f;
    style.TabRounding = 3.0f;
    style.WindowBorderSize = 1.0f;
    style.ChildBorderSize = 1.0f;
    style.FrameBorderSize = 1.0f;
    style.WindowPadding = ImVec2(16.0f, 12.0f);
    style.FramePadding = ImVec2(9.0f, 6.0f);
    style.ItemSpacing = ImVec2(9.0f, 7.0f);
    style.ItemInnerSpacing = ImVec2(7.0f, 5.0f);
    style.ScrollbarSize = 10.0f;

    ImVec4* colors = style.Colors;
    colors[ImGuiCol_Text] = ImVec4(0.66f, 0.71f, 0.79f, 1.00f);
    colors[ImGuiCol_TextDisabled] = ImVec4(0.38f, 0.44f, 0.53f, 1.00f);
    colors[ImGuiCol_WindowBg] = ImVec4(0.105f, 0.135f, 0.18f, 0.98f);
    colors[ImGuiCol_ChildBg] = ImVec4(0.12f, 0.16f, 0.215f, 0.98f);
    colors[ImGuiCol_PopupBg] = ImVec4(0.12f, 0.16f, 0.215f, 0.99f);
    colors[ImGuiCol_Border] = ImVec4(0.08f, 0.105f, 0.145f, 1.00f);
    colors[ImGuiCol_FrameBg] = ImVec4(0.18f, 0.22f, 0.29f, 1.00f);
    colors[ImGuiCol_FrameBgHovered] = ImVec4(0.22f, 0.27f, 0.35f, 1.00f);
    colors[ImGuiCol_FrameBgActive] = ImVec4(0.25f, 0.30f, 0.39f, 1.00f);
    colors[ImGuiCol_CheckMark] = kAccent;
    colors[ImGuiCol_SliderGrab] = kAccent;
    colors[ImGuiCol_SliderGrabActive] = ImVec4(1.00f, 0.54f, 0.66f, 1.00f);
    colors[ImGuiCol_Button] = ImVec4(0.18f, 0.22f, 0.29f, 1.00f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.23f, 0.28f, 0.36f, 1.00f);
    colors[ImGuiCol_ButtonActive] = kAccent;
    colors[ImGuiCol_Header] = ImVec4(0.18f, 0.22f, 0.29f, 1.00f);
    colors[ImGuiCol_HeaderHovered] = ImVec4(0.23f, 0.28f, 0.36f, 1.00f);
    colors[ImGuiCol_HeaderActive] = kAccent;
    colors[ImGuiCol_Separator] = ImVec4(0.08f, 0.105f, 0.145f, 1.00f);
}

void drawMenu(Core::AppConfig& config, const Core::GameSnapshot& snapshot) {
    static int page = 0;
    ImGui::SetNextWindowSize(ImVec2(850.0f, 680.0f), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSizeConstraints(ImVec2(720.0f, 560.0f), ImVec2(FLT_MAX, FLT_MAX));
    ImGui::Begin("Blazescension Control", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse);

    ImDrawList* draw = ImGui::GetWindowDrawList();
    const ImVec2 windowPos = ImGui::GetWindowPos();
    const ImVec2 windowSize = ImGui::GetWindowSize();
    const ImU32 topBg = IM_COL32(31, 40, 54, 252);
    const ImU32 subBg = IM_COL32(25, 36, 51, 252);
    draw->AddRectFilled(windowPos, ImVec2(windowPos.x + windowSize.x, windowPos.y + 48.0f), topBg, 3.0f,
                        ImDrawFlags_RoundCornersTop);
    draw->AddRectFilled(ImVec2(windowPos.x, windowPos.y + 48.0f),
                        ImVec2(windowPos.x + windowSize.x, windowPos.y + 84.0f), subBg);

    ImGui::SetCursorScreenPos(ImVec2(windowPos.x + 16.0f, windowPos.y + 15.0f));
    ImGui::TextColored(ImVec4(0.82f, 0.85f, 0.90f, 1.0f), "BLAZESCENSION");

    const float navStart = std::max(windowPos.x + 250.0f, windowPos.x + windowSize.x - 390.0f);
    ImGui::SetCursorScreenPos(ImVec2(navStart, windowPos.y + 7.0f));
    if (navButton("Visuals", page == 0, ImVec2(105.0f, 34.0f))) page = 0;
    ImGui::SameLine(0.0f, 2.0f);
    if (navButton("Memory", page == 1, ImVec2(105.0f, 34.0f))) page = 1;
    ImGui::SameLine(0.0f, 2.0f);
    if (navButton("Diagnostics", page == 2, ImVec2(115.0f, 34.0f))) page = 2;
    ImGui::SameLine(0.0f, 6.0f);
    if (navButton("X##close", false, ImVec2(32.0f, 34.0f))) config.requestExit = true;

    ImGui::SetCursorScreenPos(ImVec2(windowPos.x + 16.0f, windowPos.y + 58.0f));
    const ImVec4 statusColor = snapshot.attached ? kGood : kWarn;
    ImGui::TextColored(statusColor, "%s", snapshot.attached ? "Attached to Ascension" : "Waiting for Ascension.exe");
    ImGui::SetCursorScreenPos(ImVec2(windowPos.x + windowSize.x - 275.0f, windowPos.y + 58.0f));
    ImGui::TextDisabled("INSERT toggle  |  DELETE exit");

    ImGui::SetCursorScreenPos(ImVec2(windowPos.x + 16.0f, windowPos.y + 98.0f));
    const ImVec2 contentSize(windowSize.x - 32.0f, windowSize.y - 114.0f);
    ImGui::BeginChild("##page", contentSize, ImGuiChildFlags_None);

    const float gap = 8.0f;
    const float columnWidth = (ImGui::GetContentRegionAvail().x - gap) * 0.5f;
    const float contentHeight = ImGui::GetContentRegionAvail().y;

    if (page == 0) {
        beginPanel("##playerTarget", "Player & Target", ImVec2(columnWidth, contentHeight));
        ImGui::TextDisabled("Box style");
        int boxMode = static_cast<int>(config.boxDrawMode);
        if (ImGui::RadioButton("2D", boxMode == static_cast<int>(Core::BoxDrawMode::TwoD)))
            config.boxDrawMode = Core::BoxDrawMode::TwoD;
        ImGui::SameLine();
        if (ImGui::RadioButton("3D", boxMode == static_cast<int>(Core::BoxDrawMode::ThreeD)))
            config.boxDrawMode = Core::BoxDrawMode::ThreeD;
        ImGui::Spacing();
        ImGui::TextColored(kAccent, "Player");
        ImGui::Checkbox("Local player box", &config.showLocalPlayerBox);
        ImGui::Checkbox("Local player name", &config.showLocalPlayerName);
        ImGui::Checkbox("Local player circle", &config.showLocalPlayerCircle);
        ImGui::Spacing();
        ImGui::TextColored(kAccent, "Target");
        ImGui::Checkbox("Target box", &config.showTargetBox);
        ImGui::Checkbox("Target name", &config.showTargetName);
        ImGui::Checkbox("Target snap line", &config.showTargetLine);
        ImGui::Checkbox("Target circle", &config.showTargetCircle);
        if (config.showLocalPlayerCircle || config.showTargetCircle)
            ImGui::SliderFloat("Circle radius (yd)", &config.circleRadius, 0.30f, 8.0f, "%.2f");
        endPanel();

        ImGui::SameLine(0.0f, gap);
        ImGui::BeginGroup();
        beginPanel("##tracked", "Tracked Units", ImVec2(columnWidth, 190.0f));
        ImGui::Checkbox("Focus box", &config.showFocusBox);
        ImGui::Checkbox("Focus name", &config.showFocusName);
        ImGui::Checkbox("Mouseover box", &config.showMouseoverBox);
        ImGui::Checkbox("Mouseover name", &config.showMouseoverName);
        endPanel();
        beginPanel("##nearby", "Nearby Units", ImVec2(columnWidth, contentHeight - 268.0f));
        ImGui::Checkbox("Nearby NPC boxes", &config.showNpcBoxes);
        ImGui::Checkbox("Nearby NPC names", &config.showNpcNames);
        ImGui::Checkbox("Nearby player boxes", &config.showOtherPlayerBoxes);
        ImGui::Checkbox("Nearby player names", &config.showOtherPlayerNames);
        if (config.showNpcBoxes || config.showNpcNames || config.showOtherPlayerBoxes || config.showOtherPlayerNames) {
            ImGui::SliderFloat("Radius (yd)", &config.nearbyRadius, 10.0f, 200.0f, "%.0f");
            ImGui::SliderInt("Max count", &config.nearbyMaxCount, 5, 150);
            ImGui::SliderInt("Scan Hz", &config.nearbyPollHz, 1, 20);
        }
        endPanel();
        beginPanel("##overlay", "Overlay", ImVec2(columnWidth, 62.0f));
        ImGui::Checkbox("Hide overlay from capture", &config.streamProof);
        endPanel();
        ImGui::EndGroup();
    } else if (page == 1) {
        beginPanel("##memoryLeft", "Local Units", ImVec2(columnWidth, contentHeight));
        unitSummary(snapshot.player);
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        unitSummary(snapshot.focus);
        endPanel();
        ImGui::SameLine(0.0f, gap);
        beginPanel("##memoryRight", "Selected Units", ImVec2(columnWidth, contentHeight));
        unitSummary(snapshot.target);
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
        unitSummary(snapshot.mouseover);
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::TextDisabled("Nearby: %zu NPC%s, %zu player%s",
                            snapshot.nearbyNpcs.size(), snapshot.nearbyNpcs.size() == 1 ? "" : "s",
                            snapshot.nearbyPlayers.size(), snapshot.nearbyPlayers.size() == 1 ? "" : "s");
        endPanel();
    } else {
        beginPanel("##runtime", "Runtime Diagnostics", ImVec2(-FLT_MIN, contentHeight));
        ImGui::Text("Process ID"); ImGui::SameLine(180.0f); ImGui::TextColored(kAccent, "%u", snapshot.pid);
        ImGui::Text("Module base"); ImGui::SameLine(180.0f); ImGui::TextColored(kAccent, "0x%08X", snapshot.moduleBase);
        ImGui::Text("Camera"); ImGui::SameLine(180.0f); ImGui::TextColored(snapshot.camera.valid ? kGood : kWarn,
                                                                            "%s", snapshot.camera.valid ? "ready" : "waiting");
        ImGui::Text("Camera matrix"); ImGui::SameLine(180.0f); ImGui::TextColored(snapshot.camera.hasMatrix ? kGood : kWarn,
                                                                                   "%s", snapshot.camera.hasMatrix ? "ready" : "missing");
        if (snapshot.camera.valid) {
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::TextDisabled("Position  %.2f  %.2f  %.2f", snapshot.camera.position.x,
                                snapshot.camera.position.y, snapshot.camera.position.z);
            ImGui::TextDisabled("Yaw %.3f  Pitch %.3f  FOV %.3f", snapshot.camera.yaw,
                                snapshot.camera.pitch, snapshot.camera.fov);
        }
        ImGui::Spacing();
        ImGui::TextDisabled("External read-only overlay. No game memory is written.");
        endPanel();
    }

    ImGui::EndChild();
    ImGui::End();
}

} // namespace UI
