# AGENTS.md - BlazescensionOverlay

This project is the new scalable rewrite of the original Ascension overlay. It
is a Windows-only C++17 Dear ImGui / DirectX 11 external overlay for
`Ascension.exe`.

## Hard Rules

- Read-only memory access only: `ReadProcessMemory`.
- Do not write game memory.
- Do not inject, hook, patch, or automate input.
- Keep offsets documented in `docs/MEMORY_MAP.md` and mirrored in
  `src/Memory/Offsets.h`.
- Build Release before reporting code changes done.

## Build

```powershell
cmake -S BlazescensionOverlay -B BlazescensionOverlay/build -A x64
cmake --build BlazescensionOverlay/build --config Release
```

Output:

```text
BlazescensionOverlay/build/Release/BlazescensionOverlay.exe
```

The executable requests administrator elevation because the game runs elevated.

## Architecture

```text
Memory::ProcessMemory
    Opens Ascension.exe and wraps typed ReadProcessMemory calls.

Memory::GameReader
    Resolves CurMgr through WoW64 TEB/TLS, performs GUID hash lookup, reads
    unit snapshots and camera snapshots.

Core::GameSnapshot
    Plain data only. Contains attached state, player, target, focus,
    mouseover, and camera.

Overlay::OverlayWindow
    DirectComposition + D3D11 transparent topmost window, ImGui frame lifecycle,
    click-through/menu input handling, optional stream-proof display affinity.

Rendering::SceneRenderer
    Draws overlay layers. Default unit boxes are screen-aligned ESP rectangles
    from projected feet/head points, so they keep stable proportions. The older
    world-space cuboid is still available as a debug/tuning mode.

UI::Menu
    BlazeAI-inspired ImGui styling, controls, status panel, and diagnostics.

App::Application
    Main loop, hotkeys, polling, game-window tracking, and frame pacing.
```

## Important Files

| File | Purpose |
|---|---|
| `docs/MEMORY_MAP.md` | Full memory map, addresses, offsets, RVA notes, and re-derivation steps |
| `src/Memory/Offsets.h` | Compile-time offset constants |
| `src/Memory/GameReader.cpp` | TLS object manager resolution, hash lookup, unit/camera reads |
| `src/Core/Types.h` | Snapshot structs passed from reader to renderer/UI |
| `src/Rendering/Projection.cpp` | Camera-based world-to-screen projection |
| `src/Rendering/SceneRenderer.cpp` | 3D unit boxes and future draw-layer home |
| `src/Overlay/OverlayWindow.cpp` | Transparent DirectComposition overlay shell |
| `src/UI/Menu.cpp` | ImGui menu and status panels |

## Memory Flow

The reader mirrors the client:

```text
TEB32      = TEB64 + 0x2000
TlsArray   = *(TEB32 + 0x2C)
TlsIndex   = *(moduleBase + 0x9439BC)
slot       = *(TlsArray + TlsIndex * 4)
CurMgr     = *(slot + 0x08)
localGUID  = *(CurMgr + 0xC0)
object     = hashLookup(CurMgr, GUID)
descBase   = *(object + 0xD0)
movement   = *(object + 0xD8)
```

The same hash lookup is used for local player, target, focus, and mouseover.
Future entity categories should be added by finding GUID lists or object-manager
iteration details, then publishing more `Core::UnitSnapshot` or future entity
snapshot structs.

Camera reads should use the active camera object:

```text
root       = *(moduleBase + 0x77436C)
camera     = *(root + 0x7E20)
position   = *(camera + 0x08/+0x0C/+0x10)
matrix3x3  = *(camera + 0x14) as 9 floats
fov        = *(camera + 0x40)
yaw/pitch  = *(camera + 0x11C/+0x120)
```

Do not use the `0xACE4B4..0xACE4E4` commentator globals for projection; IDA
shows those are controller/display copies, while `sub_6038A0` writes the active
camera object used by the camera update path.

Projection should prefer the matrix at `camera+0x14`. `sub_604490` applies the
follow-camera offsets after mouse-look release and writes the final rotation via
`sub_607BD0 -> sub_4C56D0`; using raw yaw/pitch can line up only while the mouse
button is held.

## Adding Future Draw Features

For future players, enemies, quest NPCs, quest items, or objects:

1. Add or re-derive offsets and document them in `docs/MEMORY_MAP.md`.
2. Read memory in `Memory::GameReader` or a new reader in `src/Memory`.
3. Add plain data fields to `Core::GameSnapshot` or a new snapshot struct.
4. Draw the layer in `Rendering::SceneRenderer` or a new renderer class.
5. Add concise toggles in the relevant `UI::Menu` visual subsection.

Keep UI, memory access, and drawing separate. That separation is the main reason
this rewrite is easier for future agents to expand safely.

## Box Rendering

`Core::AppConfig::boxDrawMode` defaults to `TwoD`. This projects the
unit's feet, head, and midpoint, then draws a 2D rectangle in screen space. Use
this for normal ESP-style unit boxes because it avoids perspective skew while
still sticking to the 3D unit position.

`ThreeD` draws the original world-space cuboid and will skew with camera angle
because it is a real perspective-projected box.
