#pragma once

#include <cstdint>
#include <vector>

namespace Core {

struct Vec2 {
    float x = 0.0f;
    float y = 0.0f;
};

struct Vec3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

struct Guid64 {
    uint32_t low = 0;
    uint32_t high = 0;

    bool valid() const { return low != 0 || high != 0; }
};

enum class UnitRelation {
    LocalPlayer,
    Target,
    Focus,
    Mouseover,
    Npc,
    OtherPlayer,
    Unknown
};

struct UnitSnapshot {
    bool valid = false;
    UnitRelation relation = UnitRelation::Unknown;
    Guid64 guid{};
    uint32_t objectAddress = 0;
    uint32_t descriptorAddress = 0;
    uint32_t level = 0;
    uint32_t health = 0;
    uint32_t maxHealth = 0;
    uint32_t power = 0;
    uint32_t maxPower = 0;
    uint8_t powerType = 0;
    float speed = 0.0f;
    Vec3 position{};
    bool hasPosition = false;
    char name[48] = {};
    bool hasName = false;
};

struct CameraSnapshot {
    bool valid = false;
    Vec3 position{};
    float matrix[9]{};
    bool hasMatrix = false;
    // Full view-projection matrix the game renders with (WorldFrame+0x340),
    // row-vector convention: clip = (world - position, 0) * viewProj.
    float viewProj[16]{};
    bool hasViewProj = false;
    float nearClip = 0.0f;
    float yaw = 0.0f;
    float pitch = 0.0f;
    float fov = 0.0f;
};

struct GameSnapshot {
    bool attached = false;
    uint32_t pid = 0;
    uint32_t moduleBase = 0;
    UnitSnapshot player{};
    UnitSnapshot target{};
    UnitSnapshot focus{};
    UnitSnapshot mouseover{};
    std::vector<UnitSnapshot> nearbyNpcs{};
    std::vector<UnitSnapshot> nearbyPlayers{};
    CameraSnapshot camera{};
};

} // namespace Core
