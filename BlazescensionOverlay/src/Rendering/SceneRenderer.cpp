#include "SceneRenderer.h"

#include "Projection.h"
#include "imgui.h"

#include <algorithm>
#include <array>
#include <cstdio>

namespace Rendering {

namespace {

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

void drawProjectionDebug(
    const Core::GameSnapshot& snapshot,
    const ProjectionBasis& basis,
    const Core::Vec2& viewport) {
    if (!snapshot.player.valid || !snapshot.player.hasPosition || !snapshot.camera.valid) {
        return;
    }

    Core::Vec2 feet{};
    Core::Vec2 chest{};
    const Core::Vec3 chestWorld{
        snapshot.player.position.x,
        snapshot.player.position.y,
        snapshot.player.position.z + 1.15f,
    };
    if (!worldToScreen(snapshot.player.position, snapshot.camera, viewport, basis, feet) ||
        !worldToScreen(chestWorld, snapshot.camera, viewport, basis, chest)) {
        return;
    }

    ImDrawList* draw = ImGui::GetForegroundDrawList();
    const ImVec2 feetPoint{ feet.x, feet.y };
    const ImVec2 chestPoint{ chest.x, chest.y };
    const ImU32 color = IM_COL32(255, 220, 80, 230);
    draw->AddCircleFilled(feetPoint, 4.0f, color, 16);
    draw->AddCircle(chestPoint, 7.0f, color, 16, 1.5f);
    draw->AddLine(ImVec2(chest.x - 10.0f, chest.y), ImVec2(chest.x + 10.0f, chest.y), color, 1.5f);
    draw->AddLine(ImVec2(chest.x, chest.y - 10.0f), ImVec2(chest.x, chest.y + 10.0f), color, 1.5f);
    draw->AddText(ImVec2(chest.x + 9.0f, chest.y - 11.0f), color, basis.name);
}

// Projects the 3D player->target segment and draws it as a single screen
// line. A straight world-space segment projects to a straight screen line
// under perspective, so connecting the two projected endpoints is exact.
// The line runs from the top of the local player to the feet of the target.
void drawTargetLine(
    const Core::UnitSnapshot& player,
    const Core::UnitSnapshot& target,
    const Core::CameraSnapshot& camera,
    const Core::AppConfig& config,
    const ProjectionBasis& basis,
    const Core::Vec2& viewport) {
    if (!player.valid || !player.hasPosition ||
        !target.valid || !target.hasPosition || !camera.valid) {
        return;
    }

    const Core::Vec3 from{ player.position.x, player.position.y, player.position.z + config.boxHeight };
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
    const ImU32 shadow = IM_COL32(0, 0, 0, 190);
    const ImU32 line = relationColor(Core::UnitRelation::Target);
    draw->AddLine(pa, pb, shadow, config.lineThickness + 2.2f);
    draw->AddLine(pa, pb, line, config.lineThickness);
}

} // namespace

void SceneRenderer::draw(const Core::GameSnapshot& snapshot, const Core::AppConfig& config) {
    if (!snapshot.camera.valid) {
        return;
    }

    const ImVec2 display = ImGui::GetIO().DisplaySize;
    const Core::Vec2 viewport{ display.x, display.y };
    const ProjectionBasis basis = chooseProjectionBasis(snapshot, viewport, config.projectionMode);

    if (config.showLocalPlayerBox) {
        drawUnitBox(snapshot.player, snapshot.camera, config, basis);
    }
    if (config.showTargetBox) {
        drawUnitBox(snapshot.target, snapshot.camera, config, basis);
    }
    if (config.showTargetLine) {
        drawTargetLine(snapshot.player, snapshot.target, snapshot.camera, config, basis, viewport);
    }
    if (config.showFocusBox) {
        drawUnitBox(snapshot.focus, snapshot.camera, config, basis);
    }
    if (config.showMouseoverBox) {
        drawUnitBox(snapshot.mouseover, snapshot.camera, config, basis);
    }
    if (config.showNpcBoxes) {
        for (const Core::UnitSnapshot& npc : snapshot.nearbyNpcs) {
            drawUnitBox(npc, snapshot.camera, config, basis);
        }
    }
    if (config.showOtherPlayerBoxes) {
        for (const Core::UnitSnapshot& other : snapshot.nearbyPlayers) {
            drawUnitBox(other, snapshot.camera, config, basis);
        }
    }
    if (config.showProjectionDebug) {
        drawProjectionDebug(snapshot, basis, viewport);
    }
}

void SceneRenderer::drawUnitBox(
    const Core::UnitSnapshot& unit,
    const Core::CameraSnapshot& camera,
    const Core::AppConfig& config,
    const ProjectionBasis& basis) {
    if (config.boxDrawMode == Core::BoxDrawMode::World3D) {
        drawWorldUnitBox(unit, camera, config, basis);
    } else {
        drawScreenUnitBox(unit, camera, config, basis);
    }
}

void SceneRenderer::drawScreenUnitBox(
    const Core::UnitSnapshot& unit,
    const Core::CameraSnapshot& camera,
    const Core::AppConfig& config,
    const ProjectionBasis& basis) {
    if (!unit.valid || !unit.hasPosition || !camera.valid) {
        return;
    }

    ImDrawList* draw = ImGui::GetForegroundDrawList();
    const ImVec2 display = ImGui::GetIO().DisplaySize;
    const Core::Vec2 viewport{ display.x, display.y };

    const float height = config.boxHeight;
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

    const float widthRatio = std::clamp(config.screenBoxWidthRatio, 0.20f, 1.20f);
    const float boxWidthPx = std::max(12.0f, boxHeightPx * widthRatio);
    const float centerX = mid.x;
    const ImVec2 min{ centerX - boxWidthPx * 0.5f, top };
    const ImVec2 max{ centerX + boxWidthPx * 0.5f, bottom };

    const ImU32 shadow = IM_COL32(0, 0, 0, 190);
    const ImU32 line = relationColor(unit.relation);
    draw->AddRect(ImVec2(min.x - 1.0f, min.y - 1.0f), ImVec2(max.x + 1.0f, max.y + 1.0f), shadow, 3.0f, 0, config.lineThickness + 2.2f);
    draw->AddRect(min, max, line, 3.0f, 0, config.lineThickness);

    char text[96];
    snprintf(text, sizeof(text), "%s L%u", relationLabel(unit.relation), unit.level);
    const ImVec2 textSize = ImGui::CalcTextSize(text);
    const ImVec2 labelMin{ centerX - textSize.x * 0.5f - 5.0f, min.y - textSize.y - 6.0f };
    const ImVec2 labelMax{ centerX + textSize.x * 0.5f + 5.0f, min.y - 2.0f };
    draw->AddRectFilled(labelMin, labelMax, IM_COL32(8, 10, 14, 185), 4.0f);
    draw->AddRect(labelMin, labelMax, line, 4.0f, 0, 1.0f);
    draw->AddText(ImVec2(labelMin.x + 5.0f, labelMin.y + 2.0f), IM_COL32(245, 248, 255, 245), text);
}

void SceneRenderer::drawWorldUnitBox(
    const Core::UnitSnapshot& unit,
    const Core::CameraSnapshot& camera,
    const Core::AppConfig& config,
    const ProjectionBasis& basis) {
    if (!unit.valid || !unit.hasPosition || !camera.valid) {
        return;
    }

    ImDrawList* draw = ImGui::GetForegroundDrawList();
    const ImVec2 display = ImGui::GetIO().DisplaySize;
    const Core::Vec2 viewport{ display.x, display.y };

    const float halfWidth = config.boxWidth * 0.5f;
    const float height = config.boxHeight;
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

    const ImU32 shadow = IM_COL32(0, 0, 0, 190);
    const ImU32 line = relationColor(unit.relation);
    for (const auto& edge : edges) {
        if (!projected[edge[0]] || !projected[edge[1]]) {
            continue;
        }

        const ImVec2 a{ screen[edge[0]].x, screen[edge[0]].y };
        const ImVec2 b{ screen[edge[1]].x, screen[edge[1]].y };
        draw->AddLine(a, b, shadow, config.lineThickness + 2.2f);
        draw->AddLine(a, b, line, config.lineThickness);
    }

    Core::Vec2 labelPos{};
    const Core::Vec3 labelWorld{ c.x, c.y, c.z + height + 0.15f };
    if (worldToScreen(labelWorld, camera, viewport, basis, labelPos)) {
        char text[96];
        snprintf(text, sizeof(text), "%s L%u", relationLabel(unit.relation), unit.level);
        const ImVec2 textSize = ImGui::CalcTextSize(text);
        const ImVec2 min{ labelPos.x - textSize.x * 0.5f - 5.0f, labelPos.y - textSize.y - 4.0f };
        const ImVec2 max{ labelPos.x + textSize.x * 0.5f + 5.0f, labelPos.y + 3.0f };
        draw->AddRectFilled(min, max, IM_COL32(8, 10, 14, 180), 4.0f);
        draw->AddRect(min, max, line, 4.0f, 0, 1.0f);
        draw->AddText(ImVec2(min.x + 5.0f, min.y + 2.0f), IM_COL32(245, 248, 255, 245), text);
    }
}

} // namespace Rendering
