#pragma once

#include "Core/Types.h"
#include "ProcessMemory.h"

#include <array>

namespace Memory {

class GameReader {
public:
    bool attach();
    void detach();
    bool attached() const { return m_memory.attached(); }

    Core::GameSnapshot readSnapshot();
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

    ProcessMemory m_memory;
    std::array<uint32_t, 8> m_divisors{ 1, 1, 1, 1, 1, 1, 1, 1 };
    // Object manager pointer survives across frames; re-resolving it requires
    // an expensive system-wide thread walk, so cache and revalidate instead.
    uint32_t m_curMgr = 0;
    bool m_useCachedHealth = false;
    bool m_useCachedPower = false;
};

} // namespace Memory

