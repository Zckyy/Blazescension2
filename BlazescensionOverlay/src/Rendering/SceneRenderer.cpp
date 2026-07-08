#include "SceneRenderer.h"

#include "Projection.h"
#include "imgui.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>

namespace Rendering {

namespace {

constexpr float kBoxWorldWidth = 0.90f;
constexpr float kBoxHeight = 2.35f;
constexpr float kScreenBoxWidthRatio = 0.42f;
constexpr float kLineThickness = 1.35f;

ImU32 relationColor(Core::UnitRelation relation) {
    switch (relation) {
    case Core::UnitRelation::LocalPlayer: return IM_COL32(70, 210, 255, 235);
    case Core::UnitRelation::Target: return IM_COL32(255, 90, 90, 235);
    case Core::UnitRelation::Focus: return IM_COL32(255, 210, 80, 235);
    case Core::UnitRelation::Mouseover: return IM_COL32(120, 255, 145, 235);
    case Core::UnitRelation::Npc: return IM_COL32(255, 140, 60, 235);
    case Core::UnitRelation::OtherPlayer: return IM_COL32(200, 110, 255, 235);
    default: return IM_COL32(210, 210, 220, 235);
    }
}

const char* relationLabel(Core::UnitRelation relation) {
    switch (relation) {
    case Core::UnitRelation::LocalPlayer: return "Player";
    case Core::UnitRelation::Target: return "Target";
    case Core::UnitRelation::Focus: return "Focus";
    case Core::UnitRelation::Mouseover: return "Mouseover";
    case Core::UnitRelation::Npc: return "NPC";
    case Core::UnitRelation::OtherPlayer: return "Player";
    default: return "Unit";
    }
}

void formatUnitLabel(const Core::UnitSnapshot& unit, char* out, size_t outSize) {
    const char* label = unit.hasName && unit.name[0] != '\0' ? unit.name : relationLabel(unit.relation);
    snprintf(out, outSize, "%s L%u", label, unit.level);
}

// Draws a horizontal ring in world space around the unit's feet by projecting
// N points around the circle and connecting them; perspective makes it read as
// a ground disc under the character.
void drawGroundCircle(
    const Core::UnitSnapshot& unit,
    const Core::CameraSnapshot& camera,
    const Core::AppConfig& config,
    const ProjectionBasis& basis,
    const Core::Vec2& viewport) {
    if (!unit.valid || !unit.hasPosition || !camera.valid) {
        return;
    }

    constexpr int kSegments = 40;
    constexpr float kTwoPi = 6.28318530718f;
    const float radius = std::clamp(config.circleRadius, 0.30f, 8.0f);

    std::array<ImVec2, kSegments> points{};
    std::array<bool, kSegments> ok{};
    for (int i = 0; i < kSegments; ++i) {
        const float angle = kTwoPi * static_cast<float>(i) / static_cast<float>(kSegments);
        const Core::Vec3 world{
            unit.position.x + radius * std::cos(angle),
            unit.position.y + radius * std::sin(angle),
            unit.position.z,
        };
        Core::Vec2 screen{};
        ok[i] = worldToScreen(world, camera, viewport, basis, screen);
        points[i] = ImVec2{ screen.x, screen.y };
    }

    ImDrawList* draw = ImGui::GetForegroundDrawList();
    const ImU32 line = relationColor(unit.relation);
    for (int i = 0; i < kSegments; ++i) {
        const int j = (i + 1) % kSegments;
        if (!ok[i] || !ok[j]) {
            continue;
        }
        draw->AddLine(points[i], points[j], line, kLineThickness);
    }
}

// Projects the 3D player->target segment and draws it as a single screen
// line. A straight world-space segment projects to a straight screen line
// under perspective, so connecting the two projected endpoints is exact.
// The line runs from the local player's feet to the target's feet.
void drawTargetLine(
    const Core::UnitSnapshot& player,
    const Core::UnitSnapshot& target,
    const Core::CameraSnapshot& camera,
    const ProjectionBasis& basis,
    const Core::Vec2& viewport) {
    if (!player.valid || !player.hasPosition ||
        !target.valid || !target.hasPosition || !camera.valid) {
        return;
    }

    const Core::Vec3 from{ player.position.x, player.position.y, player.position.z };
    const Core::Vec3 to{ target.position.x, target.position.y, target.position.z };

    Core::Vec2 a{};
    Core::Vec2 b{};
    if (!worldToScreen(from, camera, viewport, basis, a) ||
        !worldToScreen(to, camera, viewport, basis, b)) {
        return;
    }

    ImDrawList* draw = ImGui::GetForegroundDrawList();
    const ImVec2 pa{ a.x, a.y };
    const ImVec2 pb{ b.x, b.y };
    const ImU32 line = relationColor(Core::UnitRelation::Target);
    draw->AddLine(pa, pb, line, kLineThickness);
}

} // namespace

void SceneRenderer::draw(const Core::GameSnapshot& snapshot, const Core::AppConfig& config) {
    if (!snapshot.camera.valid) {
        return;
    }

    const ImVec2 display = ImGui::GetIO().DisplaySize;
    const Core::Vec2 viewport{ display.x, display.y };
    const ProjectionBasis basis = chooseProjectionBasis(snapshot, viewport);

    if (config.showLocalPlayerBox) {
        drawUnitBox(snapshot.player, snapshot.camera, config, basis);
    }
    if (config.showLocalPlayerName) {
        drawUnitLabel(snapshot.player, snapshot.camera, basis);
    }
    if (config.showTargetBox) {
        drawUnitBox(snapshot.target, snapshot.camera, config, basis);
    }
    if (config.showTargetName) {
        drawUnitLabel(snapshot.target, snapshot.camera, basis);
    }
    if (config.showTargetLine) {
        drawTargetLine(snapshot.player, snapshot.target, snapshot.camera, basis, viewport);
    }
    if (config.showLocalPlayerCircle) {
        drawGroundCircle(snapshot.player, snapshot.camera, config, basis, viewport);
    }
    if (config.showTargetCircle) {
        drawGroundCircle(snapshot.target, snapshot.camera, config, basis, viewport);
    }
    if (config.showFocusBox) {
        drawUnitBox(snapshot.focus, snapshot.camera, config, basis);
    }
    if (config.showFocusName) {
        drawUnitLabel(snapshot.focus, snapshot.camera, basis);
    }
    if (config.showMouseoverBox) {
        drawUnitBox(snapshot.mouseover, snapshot.camera, config, basis);
    }
    if (config.showMouseoverName) {
        drawUnitLabel(snapshot.mouseover, snapshot.camera, basis);
    }
    if (config.showNpcBoxes) {
        for (const Core::UnitSnapshot& npc : snapshot.nearbyNpcs) {
            drawUnitBox(npc, snapshot.camera, config, basis);
        }
    }
    if (config.showNpcNames) {
        for (const Core::UnitSnapshot& npc : snapshot.nearbyNpcs) {
            drawUnitLabel(npc, snapshot.camera, basis);
        }
    }
    if (config.showOtherPlayerBoxes) {
        for (const Core::UnitSnapshot& other : snapshot.nearbyPlayers) {
            drawUnitBox(other, snapshot.camera, config, basis);
        }
    }
    if (config.showOtherPlayerNames) {
        for (const Core::UnitSnapshot& other : snapshot.nearbyPlayers) {
            drawUnitLabel(other, snapshot.camera, basis);
        }
    }
}

void SceneRenderer::drawUnitBox(
    const Core::UnitSnapshot& unit,
    const Core::CameraSnapshot& camera,
    const Core::AppConfig& config,
    const ProjectionBasis& basis) {
    if (config.boxDrawMode == Core::BoxDrawMode::ThreeD) {
        drawWorldUnitBox(unit, camera, basis);
    } else {
        drawScreenUnitBox(unit, camera, basis);
    }
}

void SceneRenderer::drawScreenUnitBox(
    const Core::UnitSnapshot& unit,
    const Core::CameraSnapshot& camera,
    const ProjectionBasis& basis) {
    if (!unit.valid || !unit.hasPosition || !camera.valid) {
        return;
    }

    ImDrawList* draw = ImGui::GetForegroundDrawList();
    const ImVec2 display = ImGui::GetIO().DisplaySize;
    const Core::Vec2 viewport{ display.x, display.y };

    const float height = kBoxHeight;
    const Core::Vec3 c = unit.position;
    const Core::Vec3 feetWorld{ c.x, c.y, c.z };
    const Core::Vec3 headWorld{ c.x, c.y, c.z + height };
    const Core::Vec3 midWorld{ c.x, c.y, c.z + height * 0.52f };

    Core::Vec2 feet{};
    Core::Vec2 head{};
    Core::Vec2 mid{};
    if (!worldToScreen(feetWorld, camera, viewport, basis, feet) ||
        !worldToScreen(headWorld, camera, viewport, basis, head) ||
        !worldToScreen(midWorld, camera, viewport, basis, mid)) {
        return;
    }

    float top = std::min(feet.y, head.y);
    float bottom = std::max(feet.y, head.y);
    float boxHeightPx = bottom - top;
    if (boxHeightPx < 18.0f) {
        boxHeightPx = 18.0f;
        top = mid.y - boxHeightPx * 0.5f;
        bottom = mid.y + boxHeightPx * 0.5f;
    }

    const float boxWidthPx = std::max(12.0f, boxHeightPx * kScreenBoxWidthRatio);
    const float centerX = mid.x;
    const ImVec2 min{ centerX - boxWidthPx * 0.5f, top };
    const ImVec2 max{ centerX + boxWidthPx * 0.5f, bottom };

    const ImU32 line = relationColor(unit.relation);
    draw->AddRect(min, max, line, 2.0f, 0, kLineThickness);

}

void SceneRenderer::drawWorldUnitBox(
    const Core::UnitSnapshot& unit,
    const Core::CameraSnapshot& camera,
    const ProjectionBasis& basis) {
    if (!unit.valid || !unit.hasPosition || !camera.valid) {
        return;
    }

    ImDrawList* draw = ImGui::GetForegroundDrawList();
    const ImVec2 display = ImGui::GetIO().DisplaySize;
    const Core::Vec2 viewport{ display.x, display.y };

    const float halfWidth = kBoxWorldWidth * 0.5f;
    const float height = kBoxHeight;
    const Core::Vec3 c = unit.position;

    const std::array<Core::Vec3, 8> corners = {
        Core::Vec3{ c.x - halfWidth, c.y - halfWidth, c.z },
        Core::Vec3{ c.x + halfWidth, c.y - halfWidth, c.z },
        Core::Vec3{ c.x + halfWidth, c.y + halfWidth, c.z },
        Core::Vec3{ c.x - halfWidth, c.y + halfWidth, c.z },
        Core::Vec3{ c.x - halfWidth, c.y - halfWidth, c.z + height },
        Core::Vec3{ c.x + halfWidth, c.y - halfWidth, c.z + height },
        Core::Vec3{ c.x + halfWidth, c.y + halfWidth, c.z + height },
        Core::Vec3{ c.x - halfWidth, c.y + halfWidth, c.z + height },
    };

    std::array<Core::Vec2, 8> screen{};
    std::array<bool, 8> projected{};
    for (size_t i = 0; i < corners.size(); ++i) {
        projected[i] = worldToScreen(corners[i], camera, viewport, basis, screen[i]);
    }

    constexpr int edges[12][2] = {
        {0, 1}, {1, 2}, {2, 3}, {3, 0},
        {4, 5}, {5, 6}, {6, 7}, {7, 4},
        {0, 4}, {1, 5}, {2, 6}, {3, 7},
    };

    const ImU32 line = relationColor(unit.relation);
    for (const auto& edge : edges) {
        if (!projected[edge[0]] || !projected[edge[1]]) {
            continue;
        }

        const ImVec2 a{ screen[edge[0]].x, screen[edge[0]].y };
        const ImVec2 b{ screen[edge[1]].x, screen[edge[1]].y };
        draw->AddLine(a, b, line, kLineThickness);
    }

}

void SceneRenderer::drawUnitLabel(
    const Core::UnitSnapshot& unit,
    const Core::CameraSnapshot& camera,
    const ProjectionBasis& basis) {
    if (!unit.valid || !unit.hasPosition || !camera.valid) {
        return;
    }

    const ImVec2 display = ImGui::GetIO().DisplaySize;
    const Core::Vec2 viewport{ display.x, display.y };
    const Core::Vec3 c = unit.position;
    const Core::Vec3 labelWorld{ c.x, c.y, c.z + kBoxHeight + 0.15f };
    Core::Vec2 labelPos{};
    if (worldToScreen(labelWorld, camera, viewport, basis, labelPos)) {
        ImDrawList* draw = ImGui::GetForegroundDrawList();
        char text[96];
        formatUnitLabel(unit, text, sizeof(text));
        const ImVec2 textSize = ImGui::CalcTextSize(text);
        const ImVec2 min{ labelPos.x - textSize.x * 0.5f - 5.0f, labelPos.y - textSize.y - 4.0f };
        const ImVec2 max{ labelPos.x + textSize.x * 0.5f + 5.0f, labelPos.y + 3.0f };
        draw->AddRectFilled(min, max, IM_COL32(8, 10, 14, 180), 4.0f);
        draw->AddText(ImVec2(min.x + 5.0f, min.y + 2.0f), IM_COL32(245, 248, 255, 245), text);
    }
}

} // namespace Rendering
