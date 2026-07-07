#pragma once

#include <cstdint>

namespace Memory::Offsets {

namespace rva {
constexpr uint32_t TlsIndex = 0x9439BC;
constexpr uint32_t HealthCacheCvar = 0x7D0A04;
constexpr uint32_t PowerCacheCvar = 0x7D0A08;
constexpr uint32_t PowerDivisorTable = 0x6F5220;

constexpr uint32_t ActiveCameraRoot = 0x77436C;
constexpr uint32_t CommentatorCameraX = 0x6CE4B4;
constexpr uint32_t CommentatorCameraY = 0x6CE4B8;
constexpr uint32_t CommentatorCameraZ = 0x6CE4BC;
constexpr uint32_t CommentatorCameraFov = 0x6CE4E4;

constexpr uint32_t TargetGuid = 0x7D07B0;
constexpr uint32_t FocusGuid = 0x7D07D0;
constexpr uint32_t MouseoverGuid = 0x7D07A0;
} // namespace rva

namespace teb {
constexpr uint32_t TlsSlots = 0x2C;
constexpr uint32_t Wow64Offset = 0x2000;
} // namespace teb

namespace cvar {
constexpr uint32_t Value = 0x30;
} // namespace cvar

namespace mgr {
constexpr uint32_t CurMgrFromSlot = 0x08;
constexpr uint32_t HashBase = 0x1C;
constexpr uint32_t HashMask = 0x24;
constexpr uint32_t LocalGuid = 0xC0;
} // namespace mgr

namespace node {
constexpr uint32_t KeyLow = 0x18;
constexpr uint32_t GuidLow = 0x30;
constexpr uint32_t GuidHigh = 0x34;
} // namespace node

namespace obj {
// CGObject_C+0x08 -> type-info struct; +0x08 of that struct is a cumulative
// type bitmask (Object=1, Item=2, Container=4, Unit=8, Player=16,
// GameObject=32, DynamicObject=64, Corpse=128), confirmed via sub_4D4DB0's
// callers (e.g. "Cursor Item" passes mask 2, "Local Player" passes mask 16).
constexpr uint32_t TypeInfoPtr = 0x08;
constexpr uint32_t DescriptorPtr = 0xD0;
constexpr uint32_t MovementPtr = 0xD8;
constexpr uint32_t CachedHealth = 0xFB0;
constexpr uint32_t CachedPower = 0xFB4;
} // namespace obj

namespace typeinfo {
constexpr uint32_t TypeMask = 0x08;
constexpr uint32_t MaskUnit = 0x08;
constexpr uint32_t MaskPlayer = 0x10;
} // namespace typeinfo

namespace desc {
constexpr uint32_t PowerType = 0x47;
constexpr uint32_t Health = 0x48;
constexpr uint32_t Power = 0x4C;
constexpr uint32_t MaxHealth = 0x68;
constexpr uint32_t MaxPower = 0x6C;
constexpr uint32_t Level = 0xC0;
} // namespace desc

namespace movement {
constexpr uint32_t X = 0x10;
constexpr uint32_t Y = 0x14;
constexpr uint32_t Z = 0x18;
constexpr uint32_t Speed = 0x8C;
} // namespace movement

namespace worldFrame {
// CGWorldFrame::GetScreenCoordinates (0x4F6D20) multiplies the camera-relative
// position by the 4x4 matrix at this+0x340.
constexpr uint32_t ViewProjMatrix = 0x340;
} // namespace worldFrame

namespace camera {
constexpr uint32_t ActiveFromRoot = 0x7E20;
constexpr uint32_t X = 0x08;
constexpr uint32_t Y = 0x0C;
constexpr uint32_t Z = 0x10;
constexpr uint32_t Matrix = 0x14;
constexpr uint32_t NearClip = 0x38;
constexpr uint32_t Fov = 0x40;
constexpr uint32_t DesiredPitch = 0x104;
constexpr uint32_t DesiredYaw = 0x108;
constexpr uint32_t Yaw = 0x11C;
constexpr uint32_t Pitch = 0x120;
} // namespace camera

} // namespace Memory::Offsets
