#pragma once

#include "Core/Types.h"

namespace Rendering {

struct ProjectionBasis {
    Core::Vec3 right{};
    Core::Vec3 up{};
    Core::Vec3 forward{};
    bool valid = false;
    // Project through camera.viewProj (exact game pipeline) instead of the
    // right/up/forward approximation.
    bool useViewProj = false;
};

bool worldToScreen(
    const Core::Vec3& world,
    const Core::CameraSnapshot& camera,
    const Core::Vec2& viewport,
    const ProjectionBasis& basis,
    Core::Vec2& out);

ProjectionBasis chooseProjectionBasis(
    const Core::GameSnapshot& snapshot,
    const Core::Vec2& viewport);

} // namespace Rendering
