#include "GameReader.h"

#include "Offsets.h"

#include <cmath>
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

} // namespace

bool GameReader::attach() {
    if (!m_memory.attach(kProcessName)) {
        return false;
    }
    return cacheDivisors();
}

void GameReader::detach() {
    m_memory.detach();
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

Core::GameSnapshot GameReader::readSnapshot() {
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

    const uint32_t curMgr = resolveCurMgr();
    if (curMgr) {
        readLocalPlayer(curMgr, snapshot.player);
        readUnitByGuid(curMgr, readGuid(m_memory.moduleBase() + Offsets::rva::TargetGuid),
                       Core::UnitRelation::Target, snapshot.target);
        readUnitByGuid(curMgr, readGuid(m_memory.moduleBase() + Offsets::rva::FocusGuid),
                       Core::UnitRelation::Focus, snapshot.focus);
        readUnitByGuid(curMgr, readGuid(m_memory.moduleBase() + Offsets::rva::MouseoverGuid),
                       Core::UnitRelation::Mouseover, snapshot.mouseover);
    }

    readCamera(snapshot.camera);
    return snapshot;
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
        if (m_memory.read<uint32_t>(entry + Offsets::node::KeyLow) == guid.low &&
            m_memory.read<uint32_t>(entry + Offsets::node::GuidLow) == guid.low &&
            m_memory.read<uint32_t>(entry + Offsets::node::GuidHigh) == guid.high) {
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
    const uint32_t descBase = object ? m_memory.read<uint32_t>(object + Offsets::obj::DescriptorPtr) : 0;
    if (!object || !descBase) {
        return false;
    }

    out.valid = true;
    out.objectAddress = object;
    out.descriptorAddress = descBase;
    out.level = m_memory.read<uint32_t>(descBase + Offsets::desc::Level);
    out.powerType = m_memory.read<uint8_t>(descBase + Offsets::desc::PowerType);

    const uint8_t powerType = out.powerType < m_divisors.size() ? out.powerType : 0;
    const uint32_t divisor = m_divisors[powerType] == 0 ? 1 : m_divisors[powerType];

    const uint32_t healthCvar = m_memory.read<uint32_t>(m_memory.moduleBase() + Offsets::rva::HealthCacheCvar);
    const bool useCachedHealth = healthCvar && m_memory.read<uint32_t>(healthCvar + Offsets::cvar::Value) != 0;
    out.health = useCachedHealth ? m_memory.read<uint32_t>(object + Offsets::obj::CachedHealth)
                                 : m_memory.read<uint32_t>(descBase + Offsets::desc::Health);
    out.maxHealth = m_memory.read<uint32_t>(descBase + Offsets::desc::MaxHealth);

    const uint32_t powerCvar = m_memory.read<uint32_t>(m_memory.moduleBase() + Offsets::rva::PowerCacheCvar);
    const bool useCachedPower = powerCvar && m_memory.read<uint32_t>(powerCvar + Offsets::cvar::Value) != 0;
    const uint32_t rawPower = useCachedPower
        ? m_memory.read<uint32_t>(object + Offsets::obj::CachedPower + powerType * sizeof(uint32_t))
        : m_memory.read<uint32_t>(descBase + Offsets::desc::Power + powerType * sizeof(uint32_t));
    const uint32_t rawMaxPower = m_memory.read<uint32_t>(descBase + Offsets::desc::MaxPower + powerType * sizeof(uint32_t));

    out.power = rawPower / divisor;
    out.maxPower = rawMaxPower / divisor;

    const uint32_t movement = m_memory.read<uint32_t>(object + Offsets::obj::MovementPtr);
    if (movement) {
        out.position.x = m_memory.read<float>(movement + Offsets::movement::X);
        out.position.y = m_memory.read<float>(movement + Offsets::movement::Y);
        out.position.z = m_memory.read<float>(movement + Offsets::movement::Z);
        out.speed = m_memory.read<float>(movement + Offsets::movement::Speed);
        out.hasPosition = finite3(out.position);
    }

    return true;
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

    out.position.x = m_memory.read<float>(camera + Offsets::camera::X);
    out.position.y = m_memory.read<float>(camera + Offsets::camera::Y);
    out.position.z = m_memory.read<float>(camera + Offsets::camera::Z);
    out.hasMatrix = m_memory.read(camera + Offsets::camera::Matrix, out.matrix);
    out.nearClip = m_memory.read<float>(camera + Offsets::camera::NearClip);
    out.hasViewProj = m_memory.read(root + Offsets::worldFrame::ViewProjMatrix, out.viewProj);
    out.yaw = m_memory.read<float>(camera + Offsets::camera::Yaw);
    out.pitch = m_memory.read<float>(camera + Offsets::camera::Pitch);
    out.fov = m_memory.read<float>(camera + Offsets::camera::Fov);

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
