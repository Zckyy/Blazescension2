// ============================================================================
//  Offsets.h  --  Ascension.exe reverse-engineered layout
//
//  All values below were recovered statically from Ascension.exe
//  (module base 0x400000, MD5 aae945760f18fd0db849aaf8cc9761ad) using IDA.
//  Each entry lists the function it was derived from so it can be re-verified.
//
//  Access chain (see sub_4D3790 / ClntObjMgrObjectPtr sub_4D4DB0/sub_4D4BB0):
//      P       = TlsArray[TlsIndex]          ; TlsArray = TEB->TlsSlots (TEB+0x2C)
//      curMgr  = *(P + 8)
//      guid    = *(curMgr + 0xC0)            ; local player GUID (64-bit)
//      object  = HashLookup(curMgr, guid)    ; ClntObjMgrObjectPtr
//      descBase= *(object + 0xD0)            ; descriptor / "values" array
// ============================================================================
#pragma once
#include <cstdint>

namespace rva {
    // Statics are relative to the runtime module base (0x400000 preferred base).
    constexpr uint32_t TlsIndex          = 0x9439BC; // 0xD439BC : loader-filled TLS index
    constexpr uint32_t HealthCacheCvar   = 0x7D0A04; // 0xBD0A04 : ptr to cvar; [+0x30]!=0 => use cached health
    constexpr uint32_t PowerCacheCvar    = 0x7D0A08; // 0xBD0A08 : ptr to cvar; [+0x30]!=0 => use cached power
    constexpr uint32_t PowerDivisorTable = 0x6F5220; // 0xAF5220 : uint32[8] power display divisors

    // Camera statics recovered from CommentatorGetCamera (sub_56A2A0) and
    // active camera resolver sub_4F5960.
    constexpr uint32_t ActiveCameraRoot = 0x77436C; // 0xB7436C : root ptr; active camera at [root+0x7E20]
    constexpr uint32_t CameraX          = 0x6CE4B4; // 0xACE4B4 : world camera X
    constexpr uint32_t CameraY          = 0x6CE4B8; // 0xACE4B8 : world camera Y
    constexpr uint32_t CameraZ          = 0x6CE4BC; // 0xACE4BC : world camera Z
    constexpr uint32_t CameraFov        = 0x6CE4E4; // 0xACE4E4 : radians, CommentatorGetCamera multiplies by 57.29578
}

namespace teb {
    constexpr uint32_t TlsSlots    = 0x2C;   // 32-bit TEB->ThreadLocalStoragePointer
    constexpr uint32_t Wow64Offset = 0x2000; // 32-bit TEB = TEB64 + 0x2000 (WoW64)
}

namespace cvar {
    constexpr uint32_t Value = 0x30;      // *(cvarStruct + 48)
}

namespace mgr {
    constexpr uint32_t CurMgrFromSlot = 0x08; // curMgr = *(TlsSlot + 8)
    constexpr uint32_t HashBase       = 0x1C; // sub_4D4BB0 this[7]
    constexpr uint32_t HashMask       = 0x24; // sub_4D4BB0 this[9]
    constexpr uint32_t LocalGuid      = 0xC0; // sub_4D3790 : curMgr+192
}

// Hash-table node fields (sub_4D4BB0)
namespace node {
    constexpr uint32_t KeyLow   = 0x18; // result[6]
    constexpr uint32_t GuidLow  = 0x30; // result[12]
    constexpr uint32_t GuidHigh = 0x34; // result[13]
}

namespace obj {
    constexpr uint32_t DescriptorPtr = 0xD0;   // *(obj+0xD0) = descBase
    constexpr uint32_t MovementPtr   = 0xD8;   // *(obj+0xD8) = movement component
    constexpr uint32_t CachedHealth  = 0xFB0;  // sub_71C2C0 alt path
    constexpr uint32_t CachedPower   = 0xFB4;  // sub_71C2E0 alt path (+4*type)
}

namespace desc {
    constexpr uint32_t Health    = 0x48; // UnitHealth
    constexpr uint32_t Power     = 0x4C; // UnitPower  (+4*type)
    constexpr uint32_t MaxHealth = 0x68; // UnitHealthMax
    constexpr uint32_t MaxPower  = 0x6C; // UnitPowerMax (+4*type)
    constexpr uint32_t PowerType = 0x47; // uint8, current power type
    constexpr uint32_t Level     = 0xC0; // UnitLevel
}

namespace move {
    constexpr uint32_t X     = 0x10; // Movement.cpp validates mover position [0x10..0x18] for NaN
    constexpr uint32_t Y     = 0x14;
    constexpr uint32_t Z     = 0x18;
    constexpr uint32_t Speed = 0x8C; // *(movement+0x8C) float, current speed
}

namespace camera {
    constexpr uint32_t ActiveFromRoot = 0x7E20; // sub_4F5960: return *(root + 0x7E20)
    constexpr uint32_t Yaw            = 0x11C;  // CommentatorGetCamera: camera yaw radians
    constexpr uint32_t Pitch          = 0x120;  // CommentatorGetCamera: camera pitch radians
}
