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

// Player name cache DB (unk_C5D938). GUID-keyed hash with the same bucket
// layout as the object manager; resolved by sub_67D770/sub_6792E0.
constexpr uint32_t PlayerNameStore = 0x85D938;
// Creature name cache DB (unk_C5DB58). Creature-display-id keyed hash;
// resolved by sub_67EA30/sub_6F6020 before the object-local fallback.
constexpr uint32_t CreatureNameStore = 0x85DB58;
} // namespace rva

namespace nameStore {
// Offsets into the name-cache DB struct (same shape as the object manager).
constexpr uint32_t LookupBase = 0x08; // sub_67D770: sub_6792E0(this + 8)
constexpr uint32_t HashBase = 0x1C; // this[7]
constexpr uint32_t HashMask = 0x24; // this[9]
// Offsets into a resolved name record (sub_6792E0 match + sub_67D770 read).
constexpr uint32_t RecordGuidLow = 0x18;
constexpr uint32_t RecordGuidHigh = 0x1C;
constexpr uint32_t RecordName = 0x20;  // inline null-terminated string
constexpr uint32_t RecordValid = 0x178; // byte: entry populated
} // namespace nameStore

namespace creatureNameStore {
constexpr uint32_t LookupBase = 0x08; // sub_67EA30: sub_6F6020(this + 8)
constexpr uint32_t HashBase = 0x1C; // this[7]
constexpr uint32_t HashMask = 0x24; // this[9]
constexpr uint32_t RecordKey = 0x00;
constexpr uint32_t RecordName = 0x18;  // sub_67EA30 returns record + 0x18
constexpr uint32_t RecordValid = 0x78; // byte: entry populated
} // namespace creatureNameStore

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
// Creature name chain: *(object + 0x964) -> +0x5C -> char* (sub_72A000
// creature path). Non-player units only.
constexpr uint32_t NpcNameCache = 0x964;
constexpr uint32_t NpcNamePtr = 0x5C;
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
constexpr uint32_t CreatureNameId = 0x114;
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
