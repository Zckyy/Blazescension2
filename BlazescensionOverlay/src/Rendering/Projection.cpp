#include "Projection.h"

#include "Core/Math.h"

#include <cmath>
#include <limits>

namespace Rendering {

namespace {

constexpr float kPi = 3.14159265358979323846f;

struct BasisSpec {
    float yawOffset = 0.0f;
    float pitchSign = 1.0f;
};

BasisSpec basisSpec(Core::ProjectionMode mode) {
    switch (mode) {
    case Core::ProjectionMode::YawX: return { 0.0f, 1.0f };
    case Core::ProjectionMode::YawXInvertPitch: return { 0.0f, -1.0f };
    case Core::ProjectionMode::YawY: return { kPi * 0.5f, 1.0f };
    case Core::ProjectionMode::YawYInvertPitch: return { kPi * 0.5f, -1.0f };
    case Core::ProjectionMode::YawNegX: return { kPi, 1.0f };
    case Core::ProjectionMode::YawNegY: return { -kPi * 0.5f, 1.0f };
    case Core::ProjectionMode::Auto:
    default: return { 0.0f, 1.0f };
    }
}

bool projectWithMode(
    const Core::Vec3& world,
    const Core::CameraSnapshot& camera,
    const Core::Vec2& viewport,
    Core::Vec2& out,
    Core::ProjectionMode mode) {
    const BasisSpec spec = basisSpec(mode);
    const float yaw = camera.yaw + spec.yawOffset;
    const float cy = std::cos(yaw);
    const float sy = std::sin(yaw);
    const float cp = std::cos(camera.pitch);
    const float sp = std::sin(camera.pitch) * spec.pitchSign;

    const Core::Vec3 forward{ cy * cp, sy * cp, sp };
    const Core::Vec3 right{ -sy, cy, 0.0f };
    const Core::Vec3 up{ -cy * sp, -sy * sp, cp };
    const Core::Vec3 relative{
        world.x - camera.position.x,
        world.y - camera.position.y,
        world.z - camera.position.z,
    };

    const float depth = Core::dot(relative, forward);
    if (depth <= 0.05f) {
        return false;
    }

    const float aspect = viewport.x / viewport.y;
    const float tanHalfFov = std::tan(camera.fov * 0.5f);
    if (tanHalfFov <= 0.001f) {
        return false;
    }

    const float x = Core::dot(relative, right) / (depth * tanHalfFov * aspect);
    const float y = Core::dot(relative, up) / (depth * tanHalfFov);

    out.x = viewport.x * 0.5f * (1.0f + x);
    out.y = viewport.y * 0.5f * (1.0f - y);

    return std::isfinite(out.x) && std::isfinite(out.y) &&
           out.x > -viewport.x && out.x < viewport.x * 2.0f &&
           out.y > -viewport.y && out.y < viewport.y * 2.0f;
}

Core::Vec3 row(const float* m, int i) {
    return Core::Vec3{ m[i * 3 + 0], m[i * 3 + 1], m[i * 3 + 2] };
}

Core::Vec3 col(const float* m, int i) {
    return Core::Vec3{ m[i + 0], m[i + 3], m[i + 6] };
}

Core::Vec3 scale(const Core::Vec3& v, float s) {
    return Core::Vec3{ v.x * s, v.y * s, v.z * s };
}

// Mirrors CGWorldFrame::GetScreenCoordinates: clip = (world - camPos, 0) * viewProj
// (row-vector convention), reject when clip.z is in front of the near plane,
// then perspective-divide into viewport pixels.
bool projectWithViewProj(
    const Core::Vec3& world,
    const Core::CameraSnapshot& camera,
    const Core::Vec2& viewport,
    Core::Vec2& out) {
    const float rx = world.x - camera.position.x;
    const float ry = world.y - camera.position.y;
    const float rz = world.z - camera.position.z;
    const float* m = camera.viewProj;

    const float clipX = rx * m[0] + ry * m[4] + rz * m[8];
    const float clipY = rx * m[1] + ry * m[5] + rz * m[9];
    const float clipZ = rx * m[2] + ry * m[6] + rz * m[10];
    const float clipW = rx * m[3] + ry * m[7] + rz * m[11];

    const float nearClip = (std::isfinite(camera.nearClip) && camera.nearClip > 0.0f)
        ? camera.nearClip
        : 0.05f;
    if (clipZ < nearClip || clipW <= 0.0001f) {
        return false;
    }

    out.x = viewport.x * 0.5f * (1.0f + clipX / clipW);
    out.y = viewport.y * 0.5f * (1.0f - clipY / clipW);

    return std::isfinite(out.x) && std::isfinite(out.y) &&
           out.x > -viewport.x && out.x < viewport.x * 2.0f &&
           out.y > -viewport.y && out.y < viewport.y * 2.0f;
}

bool projectWithBasis(
    const Core::Vec3& world,
    const Core::CameraSnapshot& camera,
    const Core::Vec2& viewport,
    const ProjectionBasis& basis,
    Core::Vec2& out) {
    if (!basis.valid || !camera.valid || viewport.x <= 1.0f || viewport.y <= 1.0f) {
        return false;
    }

    if (basis.useViewProj) {
        return camera.hasViewProj && projectWithViewProj(world, camera, viewport, out);
    }

    const Core::Vec3 relative{
        world.x - camera.position.x,
        world.y - camera.position.y,
        world.z - camera.position.z,
    };

    const float depth = Core::dot(relative, basis.forward);
    if (depth <= 0.05f) {
        return false;
    }

    const float aspect = viewport.x / viewport.y;
    const float tanHalfFov = std::tan(camera.fov * 0.5f);
    if (tanHalfFov <= 0.001f) {
        return false;
    }

    const float x = Core::dot(relative, basis.right) / (depth * tanHalfFov * aspect);
    const float y = Core::dot(relative, basis.up) / (depth * tanHalfFov);

    out.x = viewport.x * 0.5f * (1.0f + x);
    out.y = viewport.y * 0.5f * (1.0f - y);

    return std::isfinite(out.x) && std::isfinite(out.y) &&
           out.x > -viewport.x && out.x < viewport.x * 2.0f &&
           out.y > -viewport.y && out.y < viewport.y * 2.0f;
}

float pivotScore(const Core::Vec2& point, const Core::Vec2& viewport) {
    const float targetX = viewport.x * 0.5f;
    const float targetY = viewport.y * 0.58f;
    const float dx = (point.x - targetX) / (viewport.x * 0.5f);
    const float dy = (point.y - targetY) / (viewport.y * 0.5f);
    return dx * dx + dy * dy;
}

} // namespace

bool worldToScreen(
    const Core::Vec3& world,
    const Core::CameraSnapshot& camera,
    const Core::Vec2& viewport,
    Core::Vec2& out,
    Core::ProjectionMode mode) {
    if (!camera.valid || viewport.x <= 1.0f || viewport.y <= 1.0f) {
        return false;
    }

    if (mode == Core::ProjectionMode::Auto) {
        mode = chooseProjectionMode(Core::GameSnapshot{}, viewport, Core::ProjectionMode::YawX);
    }

    return projectWithMode(world, camera, viewport, out, mode);
}

bool worldToScreen(
    const Core::Vec3& world,
    const Core::CameraSnapshot& camera,
    const Core::Vec2& viewport,
    const ProjectionBasis& basis,
    Core::Vec2& out) {
    return projectWithBasis(world, camera, viewport, basis, out);
}

Core::ProjectionMode chooseProjectionMode(
    const Core::GameSnapshot& snapshot,
    const Core::Vec2& viewport,
    Core::ProjectionMode requestedMode) {
    if (requestedMode != Core::ProjectionMode::Auto || !snapshot.camera.valid ||
        !snapshot.player.valid || !snapshot.player.hasPosition) {
        return requestedMode == Core::ProjectionMode::Auto ? Core::ProjectionMode::YawX : requestedMode;
    }

    const Core::Vec3 pivot{
        snapshot.player.position.x,
        snapshot.player.position.y,
        snapshot.player.position.z + 1.15f,
    };
    const Core::ProjectionMode modes[] = {
        Core::ProjectionMode::YawX,
        Core::ProjectionMode::YawXInvertPitch,
        Core::ProjectionMode::YawY,
        Core::ProjectionMode::YawYInvertPitch,
        Core::ProjectionMode::YawNegX,
        Core::ProjectionMode::YawNegY,
    };

    Core::ProjectionMode bestMode = Core::ProjectionMode::YawX;
    float bestScore = std::numeric_limits<float>::max();
    for (Core::ProjectionMode mode : modes) {
        Core::Vec2 screen{};
        if (!projectWithMode(pivot, snapshot.camera, viewport, screen, mode)) {
            continue;
        }

        const float score = pivotScore(screen, viewport);
        if (score < bestScore) {
            bestScore = score;
            bestMode = mode;
        }
    }

    return bestMode;
}

const char* projectionModeName(Core::ProjectionMode mode) {
    switch (mode) {
    case Core::ProjectionMode::Auto: return "Auto";
    case Core::ProjectionMode::YawX: return "Yaw X";
    case Core::ProjectionMode::YawXInvertPitch: return "Yaw X, inv pitch";
    case Core::ProjectionMode::YawY: return "Yaw Y";
    case Core::ProjectionMode::YawYInvertPitch: return "Yaw Y, inv pitch";
    case Core::ProjectionMode::YawNegX: return "Yaw -X";
    case Core::ProjectionMode::YawNegY: return "Yaw -Y";
    default: return "Unknown";
    }
}

ProjectionBasis chooseProjectionBasis(
    const Core::GameSnapshot& snapshot,
    const Core::Vec2& viewport,
    Core::ProjectionMode requestedMode) {
    if (!snapshot.camera.valid) {
        return ProjectionBasis{};
    }

    if (requestedMode == Core::ProjectionMode::Auto && snapshot.camera.hasViewProj) {
        ProjectionBasis basis{};
        basis.valid = true;
        basis.useViewProj = true;
        basis.name = "view-proj";

        if (!snapshot.player.valid || !snapshot.player.hasPosition) {
            return basis;
        }

        const Core::Vec3 pivot{
            snapshot.player.position.x,
            snapshot.player.position.y,
            snapshot.player.position.z + 1.15f,
        };
        Core::Vec2 screen{};
        if (projectWithBasis(pivot, snapshot.camera, viewport, basis, screen)) {
            return basis;
        }
        // Player did not land on screen with the game's own matrix; fall back
        // to the heuristic search below.
    }

    if (requestedMode != Core::ProjectionMode::Auto || !snapshot.camera.hasMatrix ||
        !snapshot.player.valid || !snapshot.player.hasPosition) {
        const Core::ProjectionMode mode = chooseProjectionMode(snapshot, viewport, requestedMode);
        const BasisSpec spec = basisSpec(mode);
        const float yaw = snapshot.camera.yaw + spec.yawOffset;
        const float cy = std::cos(yaw);
        const float sy = std::sin(yaw);
        const float cp = std::cos(snapshot.camera.pitch);
        const float sp = std::sin(snapshot.camera.pitch) * spec.pitchSign;
        return ProjectionBasis{
            Core::Vec3{ -sy, cy, 0.0f },
            Core::Vec3{ -cy * sp, -sy * sp, cp },
            Core::Vec3{ cy * cp, sy * cp, sp },
            true,
            false,
            projectionModeName(mode),
        };
    }

    const float* m = snapshot.camera.matrix;
    const Core::Vec3 axes[6] = {
        row(m, 0), row(m, 1), row(m, 2),
        col(m, 0), col(m, 1), col(m, 2),
    };

    const Core::Vec3 pivot{
        snapshot.player.position.x,
        snapshot.player.position.y,
        snapshot.player.position.z + 1.15f,
    };

    ProjectionBasis best{};
    float bestScore = std::numeric_limits<float>::max();
    // Rows and columns of a rotation matrix are each orthonormal sets, but a
    // row is generally not perpendicular to a column. Mixing the two families
    // in one basis produces a sheared (skewed) projection, so each candidate
    // basis is drawn from a single family: indices 0-2 (rows) or 3-5 (cols).
    for (int family = 0; family < 2; ++family) {
        const int base = family * 3;
        for (int f = 0; f < 3; ++f) {
            const int forwardIndex = base + f;
            for (int r = 0; r < 3; ++r) {
                if (r == f) {
                    continue;
                }
                const int rightIndex = base + r;
                const int upIndex = base + (3 - f - r);
                const float signs[] = { -1.0f, 1.0f };
                for (float forwardSign : signs) {
                    for (float rightSign : signs) {
                        for (float upSign : signs) {
                            ProjectionBasis candidate{
                                scale(axes[rightIndex], rightSign),
                                scale(axes[upIndex], upSign),
                                scale(axes[forwardIndex], forwardSign),
                                true,
                                false,
                                "camera matrix",
                            };

                            Core::Vec2 screen{};
                            if (!projectWithBasis(pivot, snapshot.camera, viewport, candidate, screen)) {
                                continue;
                            }

                            const float score = pivotScore(screen, viewport);
                            if (score < bestScore) {
                                bestScore = score;
                                best = candidate;
                            }
                        }
                    }
                }
            }
        }
    }

    if (best.valid) {
        return best;
    }

    return chooseProjectionBasis(snapshot, viewport, Core::ProjectionMode::YawX);
}

} // namespace Rendering
