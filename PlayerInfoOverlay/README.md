# Ascension Player Info Overlay

A transparent, click-through **deadcell-gui-2 / Dear ImGui (Win32 + DirectX 11)** overlay that
draws on top of a running `Ascension.exe` and displays the local player's live
stats. Toggle the menu with **INSERT**.

Displays:

- **Level**
- **Health / Max Health** (bar)
- **Power / Max Power** (bar, colored by power type: Mana/Rage/Energy/…)
- **Speed**
- Player GUID

It is **read-only** — it only uses `ReadProcessMemory`. It never writes to the
game's memory.

## UI layer

The overlay uses the shadow-enabled Dear ImGui revision vendored by
[EternityX/deadcell-gui-2](https://github.com/EternityX/deadcell-gui-2). Its
rounded panel, window chrome, and drop shadow are rendered with the fork's
shadow draw-list primitives. The memory reader and transparent Win32/DX11
window remain external and read-only; the upstream repository's larger
event/layout framework is coupled to its original host application, so this
standalone port uses its renderer primitives directly.

## How it works

All addresses were reverse-engineered from `Ascension.exe`
(module base `0x400000`) and are documented in [`src/Offsets.h`](src/Offsets.h),
each tagged with the function it came from. The resolution chain mirrors the
client's own `sub_4D3790` / `ClntObjMgrObjectPtr`:

```
TlsArray = TEB->ThreadLocalStoragePointer   (fs:[0x2C])
P        = TlsArray[ TlsIndex ]             (TlsIndex @ module+0x9439BC)
curMgr   = *(P + 8)
guid     = *(curMgr + 0xC0)                 (local player GUID)
object   = HashLookup(curMgr, guid)         (ClntObjMgrObjectPtr port)
descBase = *(object + 0xD0)                  (descriptor / "values" array)
```

Stats are then read from the descriptor array (health `+0x48`, maxhealth `+0x68`,
power `+0x4C+4*type`, maxpower `+0x6C+4*type`, level `+0xC0`, power-type byte
`+0x47`) and speed from the movement component (`*(object+0xD8)+0x8C`).
Power values are scaled by the client's divisor table (`module+0x6F5220`), and
the client's display-cache cvars (`+0x7D0A04` / `+0x7D0A08`) are honored exactly
as the game does.

## Build

Requires CMake ≥ 3.16 and Visual Studio (MSVC). deadcell-gui-2 is fetched
automatically via CMake `FetchContent`.

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release
```

Output: `build/Release/PlayerInfoOverlay.exe`

## Run

The exe ships with a `requireAdministrator` manifest, so launching it triggers
a UAC prompt and runs elevated automatically — the Ascension client runs as
admin, so the overlay needs admin rights to open a `PROCESS_VM_READ` handle.
(If you launch from an already-elevated shell, no prompt appears.)

Launch the game, log a character **into the world**, then run the overlay.
At login / character-select there is no object manager yet, so it will show
"No player in world" — this is expected.

### Overlay behaviour
- The window is borderless, topmost, and **per-pixel transparent** (via DWM
  `DwmExtendFrameIntoClientArea`), and it tracks the game window's position/size
  each frame so it stays aligned.
- **INSERT** shows/hides the panel (polled with `GetAsyncKeyState`, so it works
  while the game has focus).
- It is **click-through** by default — the game receives all mouse input. The
  overlay only becomes solid (capturing the mouse) while the cursor is actually
  over the panel, so you can drag/interact with it without blocking gameplay.
- Run the game in **Windowed** or **Windowed (Fullscreen)** mode. True exclusive
  fullscreen bypasses the DWM compositor and the overlay won't be visible.

### Notes on Ascension specifics
- The target is a **32-bit (WoW64)** process. Its object manager is reached via
  the **32-bit TEB** (`TEB64 + 0x2000`), which is why a 64-bit reader must add
  the WoW64 offset before reading `fs:[0x2C]`'s TLS array.
- `TlsIndex` is legitimately **0** for this build.
- Health/level are read from the descriptor array; the client's display-cache
  cvar for **power is enabled**, so power comes from the cached array
  (`object+0xFB4+4*type`). Values verified live: Level 1, HP 82/82,
  Energy 100/100.
- "Speed" is the *current* movement speed (0 while standing still).

## Files

| File | Purpose |
|---|---|
| `src/Offsets.h`    | All reverse-engineered offsets, with provenance |
| `src/GameReader.*` | Process attach + memory reads + player resolution |
| `src/main.cpp`     | ImGui/DX11 window and rendering |
| `src/probe.cpp`    | Optional console verifier (not built by CMake) |
