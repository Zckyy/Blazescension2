// Standalone verification probe: reads the live player once and prints values.
#include "GameReader.h"
#include <cstdio>

int main() {
    GameReader g;
    if (!g.attach()) { std::printf("attach failed (game not running?)\n"); return 1; }
    std::printf("attached PID %u base 0x%08X\n", g.pid(), g.moduleBase());
    uint32_t code = g.raw(g.moduleBase() + (0x4D3790 - 0x400000)); // expect 0x0D8B64.. (64 8B 0D 2C)
    uint32_t div1 = g.raw(g.moduleBase() + 0x6F5220 + 4);          // expect 10 (rage divisor)
    std::printf("code@4D3790=0x%08X (expect 0x0D8B64xx) divisor[1]=%u (expect 10)\n", code, div1);
    g.debugDump();
    GameReader::Chain c = g.diagnose();
    std::printf("peSig=0x%04X (expect 0x5A4D) tlsIndex=%u curMgr=0x%08X guid=0x%08X%08X object=0x%08X descBase=0x%08X\n",
                c.peSig, c.tlsIndex, c.curMgr, c.guidHigh, c.guidLow, c.object, c.descBase);
    PlayerInfo p;
    for (int i = 0; i < 20 && !g.readPlayer(p); ++i) Sleep(200);
    if (!p.valid) { std::printf("no player resolved (character select?)\n"); return 2; }
    // --- raw descriptor dump for offset validation --------------------------
    GameReader::Chain d = g.diagnose();
    std::printf("descBase=0x%08X  [+0]=0x%08X [+4]=0x%08X (should be guid low/high)\n",
                d.descBase, g.raw(d.descBase), g.raw(d.descBase + 4));
    for (uint32_t off = 0; off <= 0x130; off += 4) {
        uint32_t v = g.raw(d.descBase + off);
        if (v == 7 || v == 82 || (v > 0 && v < 200))
            std::printf("  descBase+0x%03X = %-10u (0x%08X)\n", off, v, v);
    }
    // --- cached (display) region + cvar flags -------------------------------
    uint32_t obj = d.object;
    uint32_t hCv = g.raw(g.moduleBase() + 0x7D0A04);
    uint32_t pCv = g.raw(g.moduleBase() + 0x7D0A08);
    std::printf("healthCvar=0x%08X flag[+0x30]=%u | powerCvar=0x%08X flag[+0x30]=%u\n",
                hCv, hCv ? g.raw(hCv + 0x30) : 0, pCv, pCv ? g.raw(pCv + 0x30) : 0);
    std::printf("cachedHealth obj+0xFB0 = %u\n", g.raw(obj + 0xFB0));
    for (uint32_t off = 0xFB4; off <= 0xFD4; off += 4)
        std::printf("  cachedPower obj+0x%03X = %u\n", off, g.raw(obj + off));
    std::printf("wide scan descBase[0..0x600] for dword==7:\n");
    for (uint32_t off = 0; off <= 0x600; off += 4)
        if (g.raw(d.descBase + off) == 7) std::printf("  descBase+0x%03X = 7\n", off);
    std::printf("wide scan object[0..0x1400] for dword==7:\n");
    for (uint32_t off = 0; off <= 0x1400; off += 4)
        if (g.raw(obj + off) == 7) std::printf("  obj+0x%03X = 7\n", off);
    std::printf("Level      : %u\n", p.level);
    std::printf("Health     : %u / %u\n", p.health, p.maxHealth);
    std::printf("Power (%s): %u / %u  [type %u]\n", g.powerName(p.powerType), p.power, p.maxPower, p.powerType);
    std::printf("Speed      : %.2f\n", p.speed);
    std::printf("GUID       : 0x%08X%08X\n", p.guidHigh, p.guidLow);
    return 0;
}
