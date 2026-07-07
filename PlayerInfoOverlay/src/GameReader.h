// ============================================================================
//  GameReader.h  --  attaches to a running Ascension.exe and reads the local
//  player's live stats via ReadProcessMemory. Read-only; never writes.
// ============================================================================
#pragma once
#include <cstdint>
#include <string>
#include <windows.h>

struct Vec3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

struct PlayerInfo {
    bool     valid       = false;
    uint32_t guidLow     = 0;
    uint32_t guidHigh    = 0;
    uint32_t level       = 0;
    uint32_t health      = 0;
    uint32_t maxHealth   = 0;
    uint32_t power       = 0;
    uint32_t maxPower    = 0;
    uint8_t  powerType   = 0;
    float    speed       = 0.0f;
    Vec3     position{};
    bool     hasPosition = false;
};

struct CameraInfo {
    bool  valid = false;
    Vec3  position{};
    float yaw   = 0.0f;
    float pitch = 0.0f;
    float fov   = 0.0f;
};

class GameReader {
public:
    ~GameReader();

    // Locate + open the Ascension.exe process. Returns true once attached.
    bool attach();
    // True while the handle is open and the process is alive.
    bool attached() const { return process_ != nullptr; }
    // Drop the handle (e.g. after the game exits).
    void detach();

    // Resolve the current local player and fill `out`. Returns false if the
    // object manager / player is not yet available (e.g. at character select).
    bool readPlayer(PlayerInfo& out);
    bool readCamera(CameraInfo& out);

    uint32_t pid() const        { return pid_; }
    uint32_t moduleBase() const { return moduleBase_; }
    const char* powerName(uint8_t type) const;

    // Diagnostic: exposes each stage of the resolution chain.
    struct Chain { uint32_t peSig, tlsIndex, curMgr, guidLow, guidHigh, object, descBase; };
    Chain diagnose();
    uint32_t raw(uint32_t addr) { return read<uint32_t>(addr); } // diagnostics only
    void debugDump();                                             // diagnostics only

private:
    template <typename T>
    bool read(uint32_t addr, T& value) const {
        SIZE_T got = 0;
        return process_ && addr &&
               ReadProcessMemory(process_, reinterpret_cast<LPCVOID>(static_cast<uintptr_t>(addr)),
                                 &value, sizeof(T), &got) && got == sizeof(T);
    }
    template <typename T>
    T read(uint32_t addr) const { T v{}; read(addr, v); return v; }

    bool     processAlive() const;
    uint32_t resolveCurMgr();                       // via TLS across threads
    uint32_t hashLookup(uint32_t curMgr,            // ClntObjMgrObjectPtr port
                        uint32_t guidLow, uint32_t guidHigh);

    HANDLE   process_    = nullptr;
    uint32_t pid_        = 0;
    uint32_t moduleBase_ = 0;
    uint32_t divisors_[8] = {1,1,1,1,1,1,1,1};
};
