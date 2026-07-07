#pragma once

#include "Core/AppConfig.h"
#include "Core/Types.h"

namespace Rendering {

struct ProjectionBasis;

class SceneRenderer {
public:
    void draw(const Core::GameSnapshot& snapshot, const Core::AppConfig& config);

private:
    void drawUnitBox(
        const Core::UnitSnapshot& unit,
        const Core::CameraSnapshot& camera,
        const Core::AppConfig& config,
        const ProjectionBasis& basis);
    void drawScreenUnitBox(
        const Core::UnitSnapshot& unit,
        const Core::CameraSnapshot& camera,
        const Core::AppConfig& config,
        const ProjectionBasis& basis);
    void drawWorldUnitBox(
        const Core::UnitSnapshot& unit,
        const Core::CameraSnapshot& camera,
        const Core::AppConfig& config,
        const ProjectionBasis& basis);
};

} // namespace Rendering
