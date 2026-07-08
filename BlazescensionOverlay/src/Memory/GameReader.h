#pragma once

#include "Core/AppConfig.h"
#include "Core/Types.h"
#include "ProcessMemory.h"

#include <array>
#include <chrono>
#include <vector>

namespace Memory {

class GameReader {
public:
    bool attach();
    void detach();
    bool attached() const { return m_memory.attached(); }

    Core::GameSnapshot readSnapshot(const Core::AppConfig& config);
    const char* powerName(uint8_t type) const;

private:
    bool cacheDivisors();
    uint32_t resolveCurMgr();
    uint32_t currentCurMgr();
    void updateCvarFlags();
    uint32_t hashLookup(uint32_t curMgr, Core::Guid64 guid);
    Core::Guid64 readGuid(uint32_t address) const;
    bool readUnitByGuid(uint32_t curMgr, Core::Guid64 guid, Core::UnitRelation relation, Core::UnitSnapshot& out);
    bool readLocalPlayer(uint32_t curMgr, Core::UnitSnapshot& out);
    bool readCamera(Core::CameraSnapshot& out);
    bool readUnitFromObject(uint32_t object, Core::UnitRelation relation, Core::UnitSnapshot& out);
    bool readCString(uint32_t address, char* out, size_t outSize);
    bool readPlayerName(Core::Guid64 guid, char* out, size_t outSize);
    bool readNpcName(uint32_t object, uint32_t descriptor, char* out, size_t outSize);
    void scanNearbyUnits(
        uint32_t curMgr,
        const Core::Vec3& origin,
        const Core::AppConfig& config,
        Core::Guid64 localGuid);

    ProcessMemory m_memory;
    std::array<uint32_t, 8> m_divisors{ 1, 1, 1, 1, 1, 1, 1, 1 };
    // Object manager pointer survives across frames; re-resolving it requires
    // an expensive system-wide thread walk, so cache and revalidate instead.
    uint32_t m_curMgr = 0;
    bool m_useCachedHealth = false;
    bool m_useCachedPower = false;

    // Nearby-unit enumeration walks the entire object-manager hash table, far
    // pricier than the fixed GUID lookups, so results are cached and only
    // refreshed at config.nearbyPollHz.
    std::chrono::steady_clock::time_point m_lastNearbyScan{};
    std::vector<Core::UnitSnapshot> m_cachedNpcs;
    std::vector<Core::UnitSnapshot> m_cachedPlayers;
};

} // namespace Memory
