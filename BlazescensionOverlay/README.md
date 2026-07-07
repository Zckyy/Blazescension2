# BlazescensionOverlay

BlazescensionOverlay is a new read-only C++ external overlay for Ascension WoW.
It is built around a scalable split between memory reading, game-state snapshots,
projection, drawing layers, and ImGui UI.

The app never writes game memory, injects code, hooks the client, or automates
input. It uses `ReadProcessMemory` and a transparent DirectComposition /
DirectX 11 Dear ImGui overlay.

Unit boxes default to a screen-aligned ESP style: the player feet/head are
projected from 3D, then a clean 2D rectangle is drawn in screen space so it
sticks to the character without cuboid skew.

3D world-space boxes project through the game's own view-projection matrix
(`WorldFrame+0x340`, see `docs/MEMORY_MAP.md`) rather than a reconstructed
camera basis, which previously caused visible cuboid shear at non-centered
screen positions.

## Build

```powershell
cmake -S BlazescensionOverlay -B BlazescensionOverlay/build -A x64
cmake --build BlazescensionOverlay/build --config Release
```

Output:

```text
BlazescensionOverlay/build/Release/BlazescensionOverlay.exe
```

The target has a `requireAdministrator` manifest because Ascension runs
elevated and `OpenProcess(PROCESS_VM_READ)` otherwise fails.

## Run

1. Start Ascension and enter the world on a character.
2. Run `BlazescensionOverlay.exe`.
3. Press `INSERT` to show or hide the menu.

Use Windowed or Windowed-Fullscreen mode. True exclusive fullscreen bypasses the
DWM compositor, so external overlays are not visible.

## Project Layout

| Path | Responsibility |
|---|---|
| `src/App` | Main app loop, attach retry, snapshot polling, window tracking |
| `src/Core` | Common math, config, and snapshot types |
| `src/Memory` | Process attach, WoW64 TEB/TLS object-manager resolution, unit/camera reads |
| `src/Overlay` | DirectComposition overlay window and ImGui frame lifecycle |
| `src/Rendering` | World-to-screen projection and draw-layer rendering |
| `src/UI` | BlazeAI-inspired menu style and controls |
| `docs/MEMORY_MAP.md` | Agent-facing memory map, addresses, offsets, and re-derivation notes |

## Expansion Plan

Future work should add new game readers as snapshot producers and new overlay
drawings as render layers:

1. Add offsets to `src/Memory/Offsets.h` and document them in
   `docs/MEMORY_MAP.md`.
2. Read raw memory in `Memory::GameReader`.
3. Publish plain structs in `Core::GameSnapshot`.
4. Draw using `Rendering::SceneRenderer` or a new render layer.
5. Add UI toggles in `UI::Menu`.

This keeps memory access isolated from drawing and keeps UI state separate from
game state, which matters once player, target, enemy, NPC, object, and quest
layers are added.
