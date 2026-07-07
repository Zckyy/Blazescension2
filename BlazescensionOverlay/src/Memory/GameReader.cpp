#include "GameReader.h"

#include "Offsets.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>
#include <tlhelp32.h>
#include <winternl.h>

namespace Memory {

namespace {

constexpr const wchar_t* kProcessName = L"Ascension.exe";

typedef struct _CLIENT_ID_COMPAT {
    PVOID UniqueProcess;
    PVOID UniqueThread;
} CLIENT_ID_COMPAT;

typedef struct _THREAD_BASIC_INFORMATION_COMPAT {
    NTSTATUS ExitStatus;
    PVOID TebBaseAddress;
    CLIENT_ID_COMPAT ClientId;
    ULONG_PTR AffinityMask;
    LONG Priority;
    LONG BasePriority;
} THREAD_BASIC_INFORMATION_COMPAT;

using NtQueryInformationThreadFn = NTSTATUS(NTAPI*)(HANDLE, int, PVOID, ULONG, PULONG);

bool finite3(const Core::Vec3& v) {
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}

// Contiguous memory blocks so one ReadProcessMemory call replaces a dozen
// single-field reads. Layouts mirror the offsets in Offsets.h.

#pragma pack(push, 1)

struct DescriptorBlock { // descriptor + 0x40
    uint8_t pad0[7];
    uint8_t powerType;    // +0x47
    uint32_t health;      // +0x48
    uint32_t power[7];    // +0x4C
    uint32_t maxHealth;   // +0x68
    uint32_t maxPower[7]; // +0x6C
    uint8_t pad1[0x38];
    uint32_t level;       // +0xC0
};
static_assert(sizeof(DescriptorBlock) == 0x84);

struct ObjectPointers { // object + 0xD0
    uint32_t descriptor; // +0xD0
    uint32_t pad;
    uint32_t movement;   // +0xD8
};

struct MovementBlock { // movement + 0x10
    float x;             // +0x10
    float y;
    float z;
    uint8_t pad[0x70];
    float speed;         // +0x8C
};
static_assert(sizeof(MovementBlock) == 0x80);

struct CachedStats { // object + 0xFB0
    uint32_t health;   // +0xFB0
    uint32_t power[7]; // +0xFB4
};

struct CameraBlock { // camera + 0x00
    uint8_t pad0[8];
    float x;           // +0x08
    float y;
    float z;
    float matrix[9];   // +0x14
    float nearClip;    // +0x38
    uint8_t pad1[4];
    float fov;         // +0x40
    uint8_t pad2[0xD8];
    float yaw;         // +0x11C
    float pitch;       // +0x120
};
static_assert(sizeof(CameraBlock) == 0x124);

struct HashNodeKeys { // node + 0x18
    uint32_t keyLow;   // +0x18
    uint8_t pad[0x14];
    uint32_t guidLow;  // +0x30
    uint32_t guidHigh; // +0x34
};
static_assert(sizeof(HashNodeKeys) == 0x20);

#pragma pack(pop)

constexpr float kMaxScanDistanceSq = 4000.0f * 4000.0f; // sanity bound, not the UI radius

float distanceSq(const Core::Vec3& a, const Core::Vec3& b) {
    const float dx = a.x - b.x;
    const float dy = a.y - b.y;
    const float dz = a.z - b.z;
    return dx * dx + dy * dy + dz * dz;
}

} // namespace

bool GameReader::attach() {
    if (!m_memory.attach(kProcessName)) {
        return false;
    }
    return cacheDivisors();
}

void GameReader::detach() {
    m_memory.detach();
    m_curMgr = 0;
}

bool GameReader::cacheDivisors() {
    if (!m_memory.attached()) {
        return false;
    }

    for (uint32_t i = 0; i < m_divisors.size(); ++i) {
        const uint32_t value = m_memory.read<uint32_t>(
            m_memory.moduleBase() + Offsets::rva::PowerDivisorTable + i * sizeof(uint32_t));
        m_divisors[i] = value == 0 ? 1 : value;
    }
    return true;
}

Core::GameSnapshot GameReader::readSnapshot(const Core::AppConfig& config) {
    Core::GameSnapshot snapshot{};

    if (!m_memory.attached()) {
        attach();
    }

    if (!m_memory.attached()) {
        return snapshot;
    }

    if (!m_memory.alive()) {
        detach();
        return snapshot;
    }

    snapshot.attached = true;
    snapshot.pid = m_memory.pid();
    snapshot.moduleBase = m_memory.moduleBase();

    const uint32_t curMgr = currentCurMgr();
    if (curMgr) {
        updateCvarFlags();
        readLocalPlayer(curMgr, snapshot.player);
        readUnitByGuid(curMgr, readGuid(m_memory.moduleBase() + Offsets::rva::TargetGuid),
                       Core::UnitRelation::Target, snapshot.target);
        readUnitByGuid(curMgr, readGuid(m_memory.moduleBase() + Offsets::rva::FocusGuid),
                       Core::UnitRelation::Focus, snapshot.focus);
        readUnitByGuid(curMgr, readGuid(m_memory.moduleBase() + Offsets::rva::MouseoverGuid),
                       Core::UnitRelation::Mouseover, snapshot.mouseover);

        if ((config.showNpcBoxes || config.showOtherPlayerBoxes) &&
            snapshot.player.valid && snapshot.player.hasPosition) {
            const int pollHz = std::clamp(config.nearbyPollHz, 1, 20);
            const auto interval = std::chrono::duration<double>(1.0 / pollHz);
            const auto now = std::chrono::steady_clock::now();
            if (now - m_lastNearbyScan >= interval) {
                scanNearbyUnits(curMgr, snapshot.player.position, config, snapshot.player.guid);
                m_lastNearbyScan = now;
            }
        }
        if (config.showNpcBoxes) {
            snapshot.nearbyNpcs = m_cachedNpcs;
        }
        if (config.showOtherPlayerBoxes) {
            snapshot.nearbyPlayers = m_cachedPlayers;
        }
    }

    readCamera(snapshot.camera);
    return snapshot;
}

uint32_t GameReader::currentCurMgr() {
    if (m_curMgr) {
        uint32_t guidLow = 0;
        if (m_memory.read(m_curMgr + Offsets::mgr::LocalGuid, guidLow) && guidLow) {
            return m_curMgr;
        }
        m_curMgr = 0;
    }

    m_curMgr = resolveCurMgr();
    return m_curMgr;
}

void GameReader::updateCvarFlags() {
    const uint32_t healthCvar = m_memory.read<uint32_t>(m_memory.moduleBase() + Offsets::rva::HealthCacheCvar);
    m_useCachedHealth = healthCvar && m_memory.read<uint32_t>(healthCvar + Offsets::cvar::Value) != 0;

    const uint32_t powerCvar = m_memory.read<uint32_t>(m_memory.moduleBase() + Offsets::rva::PowerCacheCvar);
    m_useCachedPower = powerCvar && m_memory.read<uint32_t>(powerCvar + Offsets::cvar::Value) != 0;
}

uint32_t GameReader::resolveCurMgr() {
    static NtQueryInformationThreadFn ntQueryInformationThread =
        reinterpret_cast<NtQueryInformationThreadFn>(
            GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "NtQueryInformationThread"));

    if (!ntQueryInformationThread || !m_memory.attached()) {
        return 0;
    }

    const uint32_t tlsIndex = m_memory.read<uint32_t>(m_memory.moduleBase() + Offsets::rva::TlsIndex);
    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (snapshot == INVALID_HANDLE_VALUE) {
        return 0;
    }

    uint32_t curMgr = 0;
    THREADENTRY32 entry{ sizeof(entry) };
    for (BOOL ok = Thread32First(snapshot, &entry); ok && !curMgr; ok = Thread32Next(snapshot, &entry)) {
        if (entry.th32OwnerProcessID != m_memory.pid()) {
            continue;
        }

        HANDLE thread = OpenThread(THREAD_QUERY_INFORMATION, FALSE, entry.th32ThreadID);
        if (!thread) {
            continue;
        }

        THREAD_BASIC_INFORMATION_COMPAT tbi{};
        if (ntQueryInformationThread(thread, 0, &tbi, sizeof(tbi), nullptr) == 0 && tbi.TebBaseAddress) {
            const uint32_t teb64 = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(tbi.TebBaseAddress));
            const uint32_t teb32 = teb64 + Offsets::teb::Wow64Offset;
            const uint32_t tlsArray = m_memory.read<uint32_t>(teb32 + Offsets::teb::TlsSlots);
            const uint32_t slot = tlsArray ? m_memory.read<uint32_t>(tlsArray + tlsIndex * 4) : 0;
            const uint32_t candidate = slot ? m_memory.read<uint32_t>(slot + Offsets::mgr::CurMgrFromSlot) : 0;
            const uint32_t guidLow = candidate ? m_memory.read<uint32_t>(candidate + Offsets::mgr::LocalGuid) : 0;

            if (candidate && guidLow) {
                curMgr = candidate;
            }
        }

        CloseHandle(thread);
    }

    CloseHandle(snapshot);
    return curMgr;
}

uint32_t GameReader::hashLookup(uint32_t curMgr, Core::Guid64 guid) {
    if (!curMgr || !guid.valid()) {
        return 0;
    }

    const uint32_t hashBase = m_memory.read<uint32_t>(curMgr + Offsets::mgr::HashBase);
    const uint32_t mask = m_memory.read<uint32_t>(curMgr + Offsets::mgr::HashMask);
    if (!hashBase || mask == 0xFFFFFFFF) {
        return 0;
    }

    const uint32_t bucket = 12u * (guid.low & mask);
    uint32_t entry = m_memory.read<uint32_t>(hashBase + bucket + 8);
    const uint32_t delta = m_memory.read<uint32_t>(hashBase + bucket);

    for (int guard = 0; entry && !(entry & 1) && guard < 100000; ++guard) {
        HashNodeKeys keys{};
        if (m_memory.read(entry + Offsets::node::KeyLow, keys) &&
            keys.keyLow == guid.low && keys.guidLow == guid.low && keys.guidHigh == guid.high) {
            return entry;
        }
        entry = m_memory.read<uint32_t>(entry + delta + 4);
    }

    return 0;
}

Core::Guid64 GameReader::readGuid(uint32_t address) const {
    Core::Guid64 guid{};
    guid.low = m_memory.read<uint32_t>(address);
    guid.high = m_memory.read<uint32_t>(address + sizeof(uint32_t));
    return guid;
}

bool GameReader::readLocalPlayer(uint32_t curMgr, Core::UnitSnapshot& out) {
    Core::Guid64 guid{};
    guid.low = m_memory.read<uint32_t>(curMgr + Offsets::mgr::LocalGuid);
    guid.high = m_memory.read<uint32_t>(curMgr + Offsets::mgr::LocalGuid + sizeof(uint32_t));
    return readUnitByGuid(curMgr, guid, Core::UnitRelation::LocalPlayer, out);
}

bool GameReader::readUnitByGuid(uint32_t curMgr, Core::Guid64 guid, Core::UnitRelation relation, Core::UnitSnapshot& out) {
    out = Core::UnitSnapshot{};
    out.relation = relation;
    out.guid = guid;

    if (!guid.valid()) {
        return false;
    }

    const uint32_t object = hashLookup(curMgr, guid);
    if (!object) {
        return false;
    }

    return readUnitFromObject(object, relation, out);
}

bool GameReader::readUnitFromObject(uint32_t object, Core::UnitRelation relation, Core::UnitSnapshot& out) {
    out.relation = relation;

    HashNodeKeys keys{};
    if (m_memory.read(object + Offsets::node::KeyLow, keys)) {
        out.guid.low = keys.guidLow;
        out.guid.high = keys.guidHigh;
    }

    ObjectPointers pointers{};
    if (!object || !m_memory.read(object + Offsets::obj::DescriptorPtr, pointers) || !pointers.descriptor) {
        return false;
    }
    const uint32_t descBase = pointers.descriptor;

    DescriptorBlock desc{};
    if (!m_memory.read(descBase + 0x40, desc)) {
        return false;
    }

    out.valid = true;
    out.objectAddress = object;
    out.descriptorAddress = descBase;
    out.level = desc.level;
    out.powerType = desc.powerType;

    const uint8_t powerType = out.powerType < m_divisors.size() ? out.powerType : 0;
    const uint32_t divisor = m_divisors[powerType] == 0 ? 1 : m_divisors[powerType];
    const uint8_t powerIndex = powerType < 7 ? powerType : 0;

    uint32_t rawHealth = desc.health;
    uint32_t rawPower = desc.power[powerIndex];
    if (m_useCachedHealth || m_useCachedPower) {
        CachedStats cached{};
        if (m_memory.read(object + Offsets::obj::CachedHealth, cached)) {
            if (m_useCachedHealth) {
                rawHealth = cached.health;
            }
            if (m_useCachedPower) {
                rawPower = cached.power[powerIndex];
            }
        }
    }

    out.health = rawHealth;
    out.maxHealth = desc.maxHealth;
    out.power = rawPower / divisor;
    out.maxPower = desc.maxPower[powerIndex] / divisor;

    MovementBlock movement{};
    if (pointers.movement && m_memory.read(pointers.movement + Offsets::movement::X, movement)) {
        out.position.x = movement.x;
        out.position.y = movement.y;
        out.position.z = movement.z;
        out.speed = movement.speed;
        out.hasPosition = finite3(out.position);
    }

    return true;
}

void GameReader::scanNearbyUnits(
    uint32_t curMgr,
    const Core::Vec3& origin,
    const Core::AppConfig& config,
    Core::Guid64 localGuid) {
    m_cachedNpcs.clear();
    m_cachedPlayers.clear();

    if (!curMgr) {
        return;
    }

    const uint32_t hashBase = m_memory.read<uint32_t>(curMgr + Offsets::mgr::HashBase);
    const uint32_t mask = m_memory.read<uint32_t>(curMgr + Offsets::mgr::HashMask);
    if (!hashBase || mask == 0xFFFFFFFF) {
        return;
    }

    const float radius = std::clamp(config.nearbyRadius, 5.0f, 300.0f);
    const float radiusSq = radius * radius;

    struct Candidate {
        uint32_t object;
        Core::UnitRelation relation;
        float distSq;
    };
    std::vector<Candidate> npcCandidates;
    std::vector<Candidate> playerCandidates;

    // mask+1 buckets; cap the walk defensively in case a bad read during a
    // loading screen produces a huge mask.
    const uint64_t bucketCount = std::min<uint64_t>(static_cast<uint64_t>(mask) + 1u, 65536u);

    for (uint64_t i = 0; i < bucketCount; ++i) {
        const uint32_t bucket = static_cast<uint32_t>(12u * i);
        uint32_t entry = m_memory.read<uint32_t>(hashBase + bucket + 8);
        const uint32_t delta = m_memory.read<uint32_t>(hashBase + bucket);

        for (int guard = 0; entry && !(entry & 1) && guard < 64; ++guard) {
            // Type filter first: cheapest way to skip items, containers,
            // corpses, and game objects before touching movement/descriptor.
            const uint32_t typeInfo = m_memory.read<uint32_t>(entry + Offsets::obj::TypeInfoPtr);
            const uint32_t typeMask = typeInfo ? m_memory.read<uint32_t>(typeInfo + Offsets::typeinfo::TypeMask) : 0;

            const bool isUnit = (typeMask & Offsets::typeinfo::MaskUnit) != 0;
            const bool isPlayer = (typeMask & Offsets::typeinfo::MaskPlayer) != 0;
            const bool wantNpc = config.showNpcBoxes && isUnit && !isPlayer;
            const bool wantPlayer = config.showOtherPlayerBoxes && isPlayer;

            if (wantNpc || wantPlayer) {
                const uint32_t movement = m_memory.read<uint32_t>(entry + Offsets::obj::MovementPtr);
                Core::Vec3 pos{};
                bool hasPos = false;
                if (movement) {
                    pos.x = m_memory.read<float>(movement + Offsets::movement::X);
                    pos.y = m_memory.read<float>(movement + Offsets::movement::Y);
                    pos.z = m_memory.read<float>(movement + Offsets::movement::Z);
                    hasPos = finite3(pos);
                }

                if (hasPos) {
                    const float distSq = distanceSq(pos, origin);
                    if (distSq <= radiusSq && distSq <= kMaxScanDistanceSq) {
                        if (wantNpc) {
                            npcCandidates.push_back({ entry, Core::UnitRelation::Npc, distSq });
                        } else if (wantPlayer) {
                            HashNodeKeys keys{};
                            bool isSelf = false;
                            if (m_memory.read(entry + Offsets::node::KeyLow, keys)) {
                                isSelf = keys.guidLow == localGuid.low && keys.guidHigh == localGuid.high;
                            }
                            if (!isSelf) {
                                playerCandidates.push_back({ entry, Core::UnitRelation::OtherPlayer, distSq });
                            }
                        }
                    }
                }
            }

            entry = m_memory.read<uint32_t>(entry + delta + 4);
        }
    }

    const int maxCount = std::clamp(config.nearbyMaxCount, 1, 200);
    auto byDistance = [](const Candidate& a, const Candidate& b) { return a.distSq < b.distSq; };

    if (npcCandidates.size() > static_cast<size_t>(maxCount)) {
        std::partial_sort(npcCandidates.begin(), npcCandidates.begin() + maxCount, npcCandidates.end(), byDistance);
        npcCandidates.resize(maxCount);
    } else {
        std::sort(npcCandidates.begin(), npcCandidates.end(), byDistance);
    }
    if (playerCandidates.size() > static_cast<size_t>(maxCount)) {
        std::partial_sort(playerCandidates.begin(), playerCandidates.begin() + maxCount, playerCandidates.end(), byDistance);
        playerCandidates.resize(maxCount);
    } else {
        std::sort(playerCandidates.begin(), playerCandidates.end(), byDistance);
    }

    m_cachedNpcs.reserve(npcCandidates.size());
    for (const Candidate& c : npcCandidates) {
        Core::UnitSnapshot snap{};
        if (readUnitFromObject(c.object, c.relation, snap)) {
            m_cachedNpcs.push_back(std::move(snap));
        }
    }
    m_cachedPlayers.reserve(playerCandidates.size());
    for (const Candidate& c : playerCandidates) {
        Core::UnitSnapshot snap{};
        if (readUnitFromObject(c.object, c.relation, snap)) {
            m_cachedPlayers.push_back(std::move(snap));
        }
    }
}

bool GameReader::readCamera(Core::CameraSnapshot& out) {
    out = Core::CameraSnapshot{};
    if (!m_memory.attached()) {
        return false;
    }

    const uint32_t root = m_memory.read<uint32_t>(m_memory.moduleBase() + Offsets::rva::ActiveCameraRoot);
    const uint32_t camera = root ? m_memory.read<uint32_t>(root + Offsets::camera::ActiveFromRoot) : 0;
    if (!camera) {
        return false;
    }

    CameraBlock block{};
    if (!m_memory.read(camera, block)) {
        return false;
    }

    out.position.x = block.x;
    out.position.y = block.y;
    out.position.z = block.z;
    for (int i = 0; i < 9; ++i) {
        out.matrix[i] = block.matrix[i];
    }
    out.hasMatrix = true;
    out.nearClip = block.nearClip;
    out.hasViewProj = m_memory.read(root + Offsets::worldFrame::ViewProjMatrix, out.viewProj);
    out.yaw = block.yaw;
    out.pitch = block.pitch;
    out.fov = block.fov;

    if (!finite3(out.position) || !std::isfinite(out.yaw) || !std::isfinite(out.pitch) ||
        !std::isfinite(out.fov) || out.fov < 0.1f || out.fov > 3.2f) {
        return false;
    }

    if (out.hasMatrix) {
        for (float value : out.matrix) {
            if (!std::isfinite(value)) {
                out.hasMatrix = false;
                break;
            }
        }
    }

    if (out.hasViewProj) {
        for (float value : out.viewProj) {
            if (!std::isfinite(value)) {
                out.hasViewProj = false;
                break;
            }
        }
    }

    out.valid = true;
    return true;
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

} // namespace Memory
