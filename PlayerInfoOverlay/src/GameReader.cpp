#include "GameReader.h"
#include "Offsets.h"

#include <cmath>
#include <tlhelp32.h>
#include <winternl.h>

// NtQueryInformationThread(ThreadBasicInformation) -> TebBaseAddress
typedef struct _CLIENT_ID_ { PVOID u1; PVOID u2; } CLIENT_ID_;
typedef struct _THREAD_BASIC_INFORMATION_ {
    NTSTATUS  ExitStatus;
    PVOID     TebBaseAddress;
    CLIENT_ID_ ClientId;
    ULONG_PTR AffinityMask;
    LONG      Priority;
    LONG      BasePriority;
} THREAD_BASIC_INFORMATION_;

using NtQIT_t = NTSTATUS(NTAPI*)(HANDLE, int /*ThreadInformationClass*/, PVOID, ULONG, PULONG);

static const wchar_t* kProcessName = L"Ascension.exe";

// Best-effort: grant this process SeDebugPrivilege so we can open a protected /
// elevated target. Requires the app itself to be running elevated.
static void enableDebugPrivilege() {
    HANDLE token;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &token))
        return;
    LUID luid;
    if (LookupPrivilegeValueA(nullptr, SE_DEBUG_NAME, &luid)) {
        TOKEN_PRIVILEGES tp{};
        tp.PrivilegeCount = 1;
        tp.Privileges[0].Luid = luid;
        tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
        AdjustTokenPrivileges(token, FALSE, &tp, sizeof(tp), nullptr, nullptr);
    }
    CloseHandle(token);
}

GameReader::~GameReader() { detach(); }

void GameReader::detach() {
    if (process_) CloseHandle(process_);
    process_ = nullptr;
    pid_ = 0;
    moduleBase_ = 0;
}

bool GameReader::processAlive() const {
    if (!process_) return false;
    DWORD code = 0;
    return GetExitCodeProcess(process_, &code) && code == STILL_ACTIVE;
}

bool GameReader::attach() {
    if (attached()) {
        if (processAlive()) return true;
        detach();
    }

    enableDebugPrivilege();

    // --- find the process by image name -------------------------------------
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return false;
    PROCESSENTRY32W pe{ sizeof(pe) };
    DWORD pid = 0;
    for (BOOL ok = Process32FirstW(snap, &pe); ok; ok = Process32NextW(snap, &pe)) {
        if (_wcsicmp(pe.szExeFile, kProcessName) == 0) { pid = pe.th32ProcessID; break; }
    }
    CloseHandle(snap);
    if (!pid) return false;

    // --- module base of Ascension.exe ---------------------------------------
    uint32_t base = 0;
    HANDLE msnap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
    if (msnap != INVALID_HANDLE_VALUE) {
        MODULEENTRY32W me{ sizeof(me) };
        for (BOOL ok = Module32FirstW(msnap, &me); ok; ok = Module32NextW(msnap, &me)) {
            if (_wcsicmp(me.szModule, kProcessName) == 0) {
                base = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(me.modBaseAddr));
                break;
            }
        }
        CloseHandle(msnap);
    }
    if (!base) return false;

    HANDLE h = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
    if (!h) return false;

    process_ = h;
    pid_ = pid;
    moduleBase_ = base;

    // cache the power display divisor table (uint32[8]) once
    for (int i = 0; i < 8; ++i) {
        uint32_t v = read<uint32_t>(moduleBase_ + rva::PowerDivisorTable + i * 4);
        divisors_[i] = (v == 0) ? 1 : v;
    }
    return true;
}

// Walk every thread's TEB, deref the game's TLS slot, and validate the
// resulting object-manager pointer (matches sub_4D3790's TLS access).
uint32_t GameReader::resolveCurMgr() {
    static NtQIT_t NtQIT = reinterpret_cast<NtQIT_t>(
        GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "NtQueryInformationThread"));
    if (!NtQIT) return 0;

    uint32_t tlsIndex = read<uint32_t>(moduleBase_ + rva::TlsIndex);

    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (snap == INVALID_HANDLE_VALUE) return 0;

    uint32_t curMgr = 0;
    THREADENTRY32 te{ sizeof(te) };
    for (BOOL ok = Thread32First(snap, &te); ok && !curMgr; ok = Thread32Next(snap, &te)) {
        if (te.th32OwnerProcessID != pid_) continue;

        HANDLE th = OpenThread(THREAD_QUERY_INFORMATION, FALSE, te.th32ThreadID);
        if (!th) continue;

        THREAD_BASIC_INFORMATION_ tbi{};
        if (NtQIT(th, 0 /*ThreadBasicInformation*/, &tbi, sizeof(tbi), nullptr) == 0 && tbi.TebBaseAddress) {
            uint32_t teb64 = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(tbi.TebBaseAddress));
            uint32_t teb = teb64 + teb::Wow64Offset; // 32-bit TEB in WoW64
            uint32_t tlsArray = read<uint32_t>(teb + teb::TlsSlots);
            if (tlsArray) {
                uint32_t slot = read<uint32_t>(tlsArray + tlsIndex * 4);
                if (slot) {
                    uint32_t mgr = read<uint32_t>(slot + mgr::CurMgrFromSlot);
                    // sanity: a real curMgr has a non-zero local GUID
                    if (mgr && read<uint32_t>(mgr + mgr::LocalGuid) != 0)
                        curMgr = mgr;
                }
            }
        }
        CloseHandle(th);
    }
    CloseHandle(snap);
    return curMgr;
}

// Port of ClntObjMgrObjectPtr's inner hash walk (sub_4D4BB0).
uint32_t GameReader::hashLookup(uint32_t curMgr, uint32_t guidLow, uint32_t guidHigh) {
    uint32_t hashBase = read<uint32_t>(curMgr + mgr::HashBase);
    uint32_t mask     = read<uint32_t>(curMgr + mgr::HashMask);
    if (!hashBase || mask == 0xFFFFFFFF) return 0;

    uint32_t bucket = 12u * (guidLow & mask);
    uint32_t entry  = read<uint32_t>(hashBase + bucket + 8);
    uint32_t delta  = read<uint32_t>(hashBase + bucket); // per-bucket link offset

    for (int guard = 0; entry && !(entry & 1) && guard < 100000; ++guard) {
        if (read<uint32_t>(entry + node::KeyLow)  == guidLow &&
            read<uint32_t>(entry + node::GuidLow) == guidLow &&
            read<uint32_t>(entry + node::GuidHigh) == guidHigh) {
            return entry;
        }
        entry = read<uint32_t>(entry + delta + 4);
    }
    return 0;
}

#include <cstdio>
// Diagnostic: dump the TLS index and, per thread, scan the TLS slot array for a
// pointer whose [+8] looks like a valid object manager (hash mask = 2^n-1,
// non-zero local GUID). Prints candidates so we can find the true TLS index.
void GameReader::debugDump() {
    if (!attached()) { std::printf("not attached\n"); return; }
    uint32_t tlsIndex = read<uint32_t>(moduleBase_ + rva::TlsIndex);
    std::printf("TlsIndex raw @0x%08X = %u\n", moduleBase_ + rva::TlsIndex, tlsIndex);

    static NtQIT_t NtQIT = reinterpret_cast<NtQIT_t>(
        GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "NtQueryInformationThread"));

    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    THREADENTRY32 te{ sizeof(te) };
    int tcount = 0;
    for (BOOL ok = Thread32First(snap, &te); ok; ok = Thread32Next(snap, &te)) {
        if (te.th32OwnerProcessID != pid_) continue;
        HANDLE th = OpenThread(THREAD_QUERY_INFORMATION, FALSE, te.th32ThreadID);
        if (!th) continue;
        THREAD_BASIC_INFORMATION_ tbi{};
        if (NtQIT(th, 0, &tbi, sizeof(tbi), nullptr) == 0 && tbi.TebBaseAddress) {
            uint32_t teb64 = (uint32_t)(uintptr_t)tbi.TebBaseAddress;
            uint32_t teb = teb64 + teb::Wow64Offset; // 32-bit TEB in WoW64
            uint32_t tlsArray = read<uint32_t>(teb + teb::TlsSlots);
            if (tlsArray) {
                // scan first 128 TLS slots for an object-manager-looking pointer
                for (int i = 0; i < 128; ++i) {
                    uint32_t slot = read<uint32_t>(tlsArray + i * 4);
                    if (!slot) continue;
                    uint32_t cand = read<uint32_t>(slot + 8);
                    if (cand < 0x10000) continue;
                    uint32_t mask = read<uint32_t>(cand + mgr::HashMask);
                    uint32_t glow = read<uint32_t>(cand + mgr::LocalGuid);
                    // mask should be 2^n - 1 (bucket count - 1)
                    bool maskOk = mask != 0 && mask != 0xFFFFFFFF && ((mask & (mask + 1)) == 0);
                    if (maskOk && glow != 0) {
                        std::printf("  tid %-6u tlsArray=0x%08X slot[%d]=0x%08X curMgr=0x%08X mask=0x%X guidLow=0x%08X\n",
                                    te.th32ThreadID, tlsArray, i, slot, cand, mask, glow);
                    }
                }
            }
            if (++tcount <= 4)
                std::printf("thread tid %u teb64=0x%08X teb32=0x%08X tlsArray=0x%08X\n",
                            te.th32ThreadID, teb64, teb, tlsArray);
        }
        CloseHandle(th);
    }
    CloseHandle(snap);
}

GameReader::Chain GameReader::diagnose() {
    Chain c{};
    if (!attached()) return c;
    c.peSig    = read<uint16_t>(moduleBase_);            // expect 0x5A4D 'MZ'
    c.tlsIndex = read<uint32_t>(moduleBase_ + rva::TlsIndex);
    c.curMgr   = resolveCurMgr();
    if (c.curMgr) {
        c.guidLow  = read<uint32_t>(c.curMgr + mgr::LocalGuid);
        c.guidHigh = read<uint32_t>(c.curMgr + mgr::LocalGuid + 4);
        c.object   = hashLookup(c.curMgr, c.guidLow, c.guidHigh);
        if (c.object) c.descBase = read<uint32_t>(c.object + obj::DescriptorPtr);
    }
    return c;
}

const char* GameReader::powerName(uint8_t type) const {
    switch (type) {
        case 0: return "Mana";
        case 1: return "Rage";
        case 2: return "Focus";
        case 3: return "Energy";
        case 4: return "Happiness";
        case 5: return "Runes";
        case 6: return "Runic Power";
        default: return "Power";
    }
}

bool GameReader::readCamera(CameraInfo& out) {
    out = CameraInfo{};
    if (!attached()) return false;
    if (!processAlive()) { detach(); return false; }

    uint32_t root = read<uint32_t>(moduleBase_ + rva::ActiveCameraRoot);
    if (!root) return false;
    uint32_t camera = read<uint32_t>(root + camera::ActiveFromRoot);
    if (!camera) return false;

    out.position.x = read<float>(moduleBase_ + rva::CameraX);
    out.position.y = read<float>(moduleBase_ + rva::CameraY);
    out.position.z = read<float>(moduleBase_ + rva::CameraZ);
    out.yaw        = read<float>(camera + camera::Yaw);
    out.pitch      = read<float>(camera + camera::Pitch);
    out.fov        = read<float>(moduleBase_ + rva::CameraFov);

    if (!std::isfinite(out.position.x) || !std::isfinite(out.position.y) ||
        !std::isfinite(out.position.z) || !std::isfinite(out.yaw) ||
        !std::isfinite(out.pitch) || !std::isfinite(out.fov) ||
        out.fov < 0.1f || out.fov > 3.2f) {
        return false;
    }

    out.valid = true;
    return true;
}

bool GameReader::readPlayer(PlayerInfo& out) {
    out = PlayerInfo{};
    if (!attached()) return false;
    if (!processAlive()) { detach(); return false; }

    uint32_t curMgr = resolveCurMgr();
    if (!curMgr) return false;

    uint32_t guidLow  = read<uint32_t>(curMgr + mgr::LocalGuid);
    uint32_t guidHigh = read<uint32_t>(curMgr + mgr::LocalGuid + 4);
    if (!guidLow && !guidHigh) return false;

    uint32_t object = hashLookup(curMgr, guidLow, guidHigh);
    if (!object) return false;

    uint32_t descBase = read<uint32_t>(object + obj::DescriptorPtr);
    if (!descBase) return false;

    out.guidLow   = guidLow;
    out.guidHigh  = guidHigh;
    out.level     = read<uint32_t>(descBase + desc::Level);
    out.powerType = read<uint8_t>(descBase + desc::PowerType);

    uint8_t  pt   = out.powerType < 8 ? out.powerType : 0;
    uint32_t div  = divisors_[pt] ? divisors_[pt] : 1;

    // --- health: cached display value if the client's cvar flag is set ------
    uint32_t hCvar = read<uint32_t>(moduleBase_ + rva::HealthCacheCvar);
    bool useCachedH = hCvar && read<uint32_t>(hCvar + cvar::Value) != 0;
    out.health    = useCachedH ? read<uint32_t>(object + obj::CachedHealth)
                               : read<uint32_t>(descBase + desc::Health);
    out.maxHealth = read<uint32_t>(descBase + desc::MaxHealth);

    // --- power / max power (scaled by the divisor table) --------------------
    uint32_t pCvar = read<uint32_t>(moduleBase_ + rva::PowerCacheCvar);
    bool useCachedP = pCvar && read<uint32_t>(pCvar + cvar::Value) != 0;
    uint32_t rawPower = useCachedP ? read<uint32_t>(object + obj::CachedPower + pt * 4)
                                   : read<uint32_t>(descBase + desc::Power + pt * 4);
    uint32_t rawMax   = read<uint32_t>(descBase + desc::MaxPower + pt * 4);
    out.power    = rawPower / div;
    out.maxPower = rawMax / div;

    // --- speed --------------------------------------------------------------
    uint32_t movement = read<uint32_t>(object + obj::MovementPtr);
    if (movement) {
        out.position.x = read<float>(movement + move::X);
        out.position.y = read<float>(movement + move::Y);
        out.position.z = read<float>(movement + move::Z);
        out.hasPosition = std::isfinite(out.position.x) && std::isfinite(out.position.y) &&
                          std::isfinite(out.position.z);
        out.speed = read<float>(movement + move::Speed);
    }

    out.valid = true;
    return true;
}
