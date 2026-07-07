# AGENTS.md — Reading Ascension WoW player memory

This document lets another coding agent/model understand **how to read the
Ascension (WoW 3.3.5-based) client's memory** and **how the `PlayerInfoOverlay`
app in this workspace works**. It records the exact addresses/offsets recovered
by reverse-engineering the client, plus the method used to find them so they can
be re-derived if the binary changes.

> ⚠️ **All numeric addresses below are specific to this exact build.** They are
> valid only for the `Ascension.exe` fingerprinted in the next section. If the
> client updates, re-derive them with the method in
> [§7 Re-deriving offsets](#7-re-deriving-offsets-if-the-binary-changes). The
> *structure offsets* (descriptor layout, object fields) tend to be stable across
> 3.3.5 rebuilds; the *static module addresses* (TLS index, cvars, tables) move.

---

## 1. Target binary

| | |
|---|---|
| Module | `Ascension.exe` |
| Path | `C:\Ascension\Launcher\resources\ascension-live\Ascension.exe` |
| Architecture | **x86 (32-bit)**, runs under **WoW64** |
| Preferred image base | `0x400000` (loads there; no ASLR observed) |
| MD5 | `aae945760f18fd0db849aaf8cc9761ad` |
| SHA-256 | `5b26e33b2129737af3a0c3164459f4c9b109398dab921f76c6740a8746fbb929` |
| Engine | Custom classless client based on WoW 3.3.5a (WotLK, build 12340 lineage) |

**RVA convention:** `RVA = VA - 0x400000`. At runtime, read the real module base
of `Ascension.exe` in the target and use `moduleBase + RVA` (do not assume
`0x400000`, even though it currently loads there).

**The client runs elevated (as Administrator).** A reader process must also be
elevated (embed a `requireAdministrator` manifest and/or enable
`SeDebugPrivilege`) or `OpenProcess(PROCESS_VM_READ)` fails with
`ERROR_ACCESS_DENIED (5)`.

---

## 2. The pointer chain (player object → stats)

The client stores its object manager (`CurMgr`) in **thread-local storage**, and
resolves game objects by GUID through a hash table. To read the local player:

```
                 (32-bit TEB, see §3 for WoW64 caveat)
TlsArray  = *(TEB + 0x2C)                 ; TEB->ThreadLocalStoragePointer
TlsIndex  = *(moduleBase + 0x9439BC)      ; loader-filled; == 0 for this build
P         = *(TlsArray + TlsIndex*4)       ; per-thread slot pointer
CurMgr    = *(P + 8)                        ; object manager
localGUID = *(CurMgr + 0xC0)  (64-bit)      ; local player's GUID
object    = ClntObjMgrObjectPtr(CurMgr, localGUID)   ; hash lookup, see §4
descBase  = *(object + 0xD0)                ; descriptor ("values") array
; then read stats from descBase (see §5)
```

Derived from `sub_4D3790` (get-local-GUID). Its disassembly:

```asm
mov ecx, large fs:2Ch      ; TEB->ThreadLocalStoragePointer
mov eax, TlsIndex          ; [0x00D439BC]
mov edx, [ecx+eax*4]       ; P = TlsArray[TlsIndex]
mov ecx, [edx+8]           ; CurMgr = *(P+8)
mov eax, [ecx+0C0h]        ; localGUID low  = *(CurMgr+0xC0)
mov edx, [ecx+0C4h]        ; localGUID high = *(CurMgr+0xC4)
```

---

## 3. WoW64 TEB caveat (critical for 64-bit readers)

The game reads `fs:[0x2C]`, i.e. the **32-bit TEB**. A **64-bit** reader that
gets a thread's TEB via `NtQueryInformationThread(ThreadBasicInformation)`
receives the **64-bit TEB**. In a WoW64 process the 32-bit TEB is located at:

```
TEB32 = TEB64 + 0x2000
TlsArray = *(TEB32 + 0x2C)
```

Reading `TEB64 + 0x2C` yields `0` (wrong field) and the whole chain fails. This
was the #1 bug during development. If your reader is itself 32-bit, the TEB you
get is already the 32-bit one — no `+0x2000`.

**Finding the right thread:** TLS is per-thread; only the main game thread's slot
holds a valid `CurMgr`. Enumerate all process threads (`CreateToolhelp32Snapshot`
+ `THREADENTRY32`), and for each, walk the chain and accept the first `CurMgr`
whose `*(CurMgr+0xC0)` (localGUID) is non-zero. A robust discovery scan: for the
main thread, iterate `TlsArray[0..127]`; a slot `s` is the object manager when
`mask = *( *(s+8) + 0x24 )` is a power-of-two-minus-one and
`*( *(s+8) + 0xC0 )` (localGUID) is non-zero.

---

## 4. Object manager hash lookup (`ClntObjMgrObjectPtr`)

Ported from `sub_4D4BB0` (inner) / `sub_4D4DB0` (wrapper). Given `CurMgr` and a
64-bit GUID `(guidLow, guidHigh)`:

```
hashBase = *(CurMgr + 0x1C)
mask     = *(CurMgr + 0x24)
if hashBase == 0 or mask == 0xFFFFFFFF: return 0

bucket = 12 * (guidLow & mask)          ; buckets are 12 (0xC) bytes each
entry  = *(hashBase + bucket + 8)       ; first node pointer
delta  = *(hashBase + bucket)           ; per-bucket link offset (constant in loop)

while entry != 0 and (entry & 1) == 0:
    if *(entry + 0x18) == guidLow   and    ; node.KeyLow
       *(entry + 0x30) == guidLow   and    ; node.GuidLow
       *(entry + 0x34) == guidHigh:        ; node.GuidHigh
        return entry
    entry = *(entry + delta + 4)           ; next node
return 0
```

This works for **any** unit GUID (target, party, nameplates…), not just the
local player.

---

## 5. Descriptor & object offsets (the actual stats)

Every unit getter reads `descBase = *(object + 0xD0)` then indexes it. Confirmed
by decompiling the client's own Lua C-functions (see §6).

### Object-relative

| Offset | Meaning | Source |
|---|---|---|
| `+0xD0` | pointer to descriptor ("values") array | all getters |
| `+0xD8` | pointer to movement component | `GetUnitSpeed` |
| `+0xFB0` | cached/display **health** (see cvar in §5.2) | `CGUnit::GetHealth` |
| `+0xFB4 + 4*type` | cached/display **power[type]** | `CGUnit::GetPower` |

### Descriptor-relative (`descBase + …`)

| Offset | Field | Type | Source func |
|---|---|---|---|
| `+0x47` | current power **type** (0-6) | `u8` | `UnitPower`/`UnitPowerType` |
| `+0x48` | **Health** | `u32` | `UnitHealth` |
| `+0x4C + 4*type` | **Power[type]** | `u32` | `CGUnit::GetPower` |
| `+0x68` | **MaxHealth** | `u32` | `UnitHealthMax` |
| `+0x6C + 4*type` | **MaxPower[type]** | `u32` | `UnitPowerMax` |
| `+0xC0` | **Level** | `u32` | `UnitLevel` |

> Note: unlike stock 3.3.5, `descBase + 0x00` is **not** the object GUID here
> (it read as 0 live). Don't rely on descriptor[0] to validate the object; the
> hash lookup already matches the GUID.

### Movement-relative

| Offset | Field | Type |
|---|---|---|
| `*(object+0xD8) + 0x10` | world **X** (`float`) | player/unit position |
| `*(object+0xD8) + 0x14` | world **Y** (`float`) | player/unit position |
| `*(object+0xD8) + 0x18` | world **Z** (`float`) | player/unit position |
| `*(object+0xD8) + 0x8C` | current **speed** (`float`) | 0.0 while standing still |

The position offsets were confirmed from `Movement.cpp`: `sub_6F09F0` validates
`movement+0x10`, `movement+0x14`, and `movement+0x18` with `__isnan` before
logging `"Mover at invalid position"`. `GetPlayerMapPosition` also obtains XYZ
by resolving the unit and calling the object's vtable slot `+0x2C`, but the
direct movement fields are easier and cleaner for an external reader.

### 5.1 Camera / projection data for external 3D drawing

The overlay's first external 3D drawing pass uses the player movement position
above plus the active camera data recovered from `CommentatorGetCamera` and
`sub_4F5960`.

```
root       = *(moduleBase + 0x77436C)     ; VA 0xB7436C, camera/world root
camera     = *(root + 0x7E20)             ; active camera object
cameraPosX = *(moduleBase + 0x6CE4B4)     ; VA 0xACE4B4
cameraPosY = *(moduleBase + 0x6CE4B8)     ; VA 0xACE4B8
cameraPosZ = *(moduleBase + 0x6CE4BC)     ; VA 0xACE4BC
yaw        = *(camera + 0x11C)            ; radians
pitch      = *(camera + 0x120)            ; radians
fov        = *(moduleBase + 0x6CE4E4)     ; radians
```

`CommentatorGetCamera` (`sub_56A2A0`) returns
`cameraPosX/Y/Z`, `camera+0x11C`, `camera+0x120`, and `0xACE4E4`, multiplying
the angular values by `57.29578` for Lua display. Keep the reader values in
radians for projection math.

### 5.2 Power types & the divisor table

`type` (the byte at `descBase+0x47`) selects the power slot:

| type | power | | type | power |
|---|---|---|---|---|
| 0 | Mana | | 4 | Happiness |
| 1 | Rage | | 5 | Runes |
| 2 | Focus | | 6 | Runic Power |
| 3 | Energy | | | |

Some powers are stored **scaled** and must be divided by a table before display
(`sub_7FDE00` → `dword_AF5220`, RVA `0x6F5220`, `u32[8]`):

```
divisor[0..6] = { 1, 10, 1, 1, 1000, 1, 10 }   ; index by power type
displayedPower = rawPower / divisor[type]
```

(Rage and Runic Power ×10, Happiness ×1000 — the classic WotLK scaling.)

### 5.3 Display-cache cvars (base vs. displayed values)

Ascension keeps **base** values in the descriptor and **display** (scaled) values
in the cached object region. The client chooses per a cvar flag:

```
healthCvar = *(moduleBase + 0x7D0A04)          ; RVA of dword_BD0A04
if healthCvar && *(healthCvar + 0x30) != 0:
    health = *(object + 0xFB0)                  ; cached/display
else:
    health = *(descBase + 0x48)                 ; descriptor/base

powerCvar  = *(moduleBase + 0x7D0A08)          ; RVA of dword_BD0A08
if powerCvar && *(powerCvar + 0x30) != 0:
    power = *(object + 0xFB4 + 4*type)          ; cached/display
else:
    power = *(descBase + 0x4C + 4*type)         ; descriptor/base
```

Observed live: **health flag = 0** (use descriptor), **power flag = 1** (use
cached). Replicate this branch to match exactly what the player sees.

---

## 6. Key functions & statics (for further RE)

Addresses are VAs (image base `0x400000`).

### Core resolution
| VA | Name / meaning |
|---|---|
| `0x4D3790` | `GetLocalPlayerGuid` — TLS → CurMgr → `+0xC0` |
| `0x4D4DB0` | `ClntObjMgrObjectPtr(guid, typeMask)` wrapper (TLS null-check + type filter via `*(*(obj+8)+8) & mask`) |
| `0x4D4BB0` | hash-table lookup (ported in §4) |
| `0x60ABF0` | `GetGuidByUnitName("player"/"target"/…)` — string → GUID |
| `0x4F5960` | active camera resolver — `*(dword_B7436C + 0x7E20)` |
| `0x71C2C0` | `CGUnit::GetHealth` (cached vs descriptor branch) |
| `0x71C2E0` | `CGUnit::GetPower(type)` (cached vs descriptor branch) |
| `0x7FDE00` | power divisor lookup → `dword_AF5220` |

### Lua getter C-functions (each takes a Lua state; good decomp entry points)
| VA | Lua name |
|---|---|
| `0x60EB60` | `UnitHealth` |
| `0x60EC60` | `UnitHealthMax` |
| `0x60ED40` | `UnitPower` |
| `0x60EF40` | `UnitPowerMax` |
| `0x60F100` | `UnitPowerType` |
| `0x60F9E0` | `UnitLevel` |
| `0x613290` | `GetUnitSpeed` |
| `0x613330` | `GetUnitPitch` |
| `0x60A490` | `GetPlayerFacing` |
| `0x56A2A0` | `CommentatorGetCamera` |

### Useful statics (VA)
| VA | RVA | Meaning |
|---|---|---|
| `0xD439BC` | `0x9439BC` | `TlsIndex` (u32; `0` for this build) |
| `0xAF5220` | `0x6F5220` | power divisor table `u32[8]` |
| `0xB7436C` | `0x77436C` | active camera/world root pointer |
| `0xACE4B4` / `0xACE4B8` / `0xACE4BC` | `0x6CE4B4` / `0x6CE4B8` / `0x6CE4BC` | camera world XYZ |
| `0xACE4E4` | `0x6CE4E4` | active camera FOV in radians |
| `0xBD0A04` | `0x7D0A04` | health display-cache cvar pointer |
| `0xBD0A08` | `0x7D0A08` | power display-cache cvar pointer |
| `0xBD07B0` / `0xBD07B4` | — | **target** GUID (low/high) |
| `0xBD07D0` / `0xBD07D4` | — | **focus** GUID (low/high) |
| `0xBD07A0` / `0xBD07A4` | — | **mouseover** GUID (low/high) |

These target/focus/mouseover statics let you resolve those units directly (feed
into the §4 hash lookup) without walking names.

### Sample live values (for sanity-checking a port)
Character "Zck", level 1: `CurMgr=0x3D61BBB8`, `localGUID=0x0000000000051219`,
`object=0x803F3008`, `descBase=0x803F4978`, Health `82/82`, Energy `100/100`
(power type 3), hash mask `0x1FF`.

---

## 7. Re-deriving offsets if the binary changes

The client is a goldmine because it registers its **Lua API** as
`{const char* name, lua_CFunction fn}` pairs (8 bytes each on x86). This makes
every getter trivially findable:

1. **Find the name string** (e.g. `"UnitHealth"`, `"GetUnitSpeed"`) in `.rdata`.
2. **Xref the string** → a `.data` table entry. The **function pointer is the
   next dword** after the name pointer (`entry+4`).
3. **Decompile the function.** They all follow the same shape: resolve the unit
   name → GUID (`sub_60ABF0`), `ClntObjMgrObjectPtr` → object, then read
   `*(*(obj + 0xD0) + fieldOffset)`. Read off the field offset directly.
4. For the **object manager**, decompile `GetLocalPlayerGuid`
   (`ClntObjMgrObjectPtr`'s TLS access) to recover the current `TlsIndex`
   address, the `CurMgr` field offsets (hashBase/mask/localGUID), and the node
   layout.
5. For **movement position**, search for `"Mover at invalid position"` or
   decompile `Movement.cpp` function `sub_6F09F0`; the NaN checks identify the
   raw mover position fields at `movement+0x10/+0x14/+0x18`.
6. For **camera/projection**, decompile `CommentatorGetCamera`
   (`sub_56A2A0`) and `sub_4F5960`. The former exposes camera XYZ, yaw, pitch,
   and FOV; the latter exposes the active camera pointer chain.

Tooling used originally: **IDA Pro** (static decompilation to find offsets) +
**x64dbg** (live inspection). Both were driven via their MCP servers.

---

## 8. The `PlayerInfoOverlay` app

Located in [`PlayerInfoOverlay/`](PlayerInfoOverlay/). A read-only, transparent,
click-through **Dear ImGui (Win32 + DirectX 11)** overlay that shows the local
player's HP / power / level / speed and an external 3D player bounding box on
top of the game.

### Layout
| File | Purpose |
|---|---|
| `src/Offsets.h` | Every offset from this doc, as named constants with provenance |
| `src/GameReader.{h,cpp}` | Attach (elevation + `SeDebugPrivilege`), WoW64 TEB → CurMgr, hash lookup, stat/player-position/camera reads |
| `src/main.cpp` | Transparent DWM overlay, window tracking, INSERT toggle, ImGui panel, world-to-screen and 3D box drawing |
| `src/probe.cpp` | Standalone console verifier (not built by CMake) — prints the chain + values |
| `CMakeLists.txt` | Fetches Dear ImGui via `FetchContent`; embeds UAC manifest |

### How it reads memory
1. `attach()` — enable `SeDebugPrivilege`; find `Ascension.exe` via a process
   snapshot; get its module base via a module snapshot; `OpenProcess` with
   `PROCESS_QUERY_INFORMATION | PROCESS_VM_READ`; cache the divisor table.
2. `resolveCurMgr()` — per thread: `NtQueryInformationThread` → TEB64, add
   `0x2000` → TEB32, read TLS array, index by `TlsIndex`, `*(slot+8)` = CurMgr,
   validate localGUID ≠ 0.
3. `readPlayer()` — localGUID from `CurMgr+0xC0`; `hashLookup` → object;
   `descBase = *(object+0xD0)`; read level/health/maxhealth, power/maxpower
   (honoring the cvar cache flags and dividing by the divisor), movement
   position (`movement+0x10/+0x14/+0x18`), and speed.
4. `readCamera()` — `root = *(moduleBase+0x77436C)`,
   `camera = *(root+0x7E20)`, then read camera XYZ globals, yaw/pitch, and FOV.
5. All reads go through `ReadProcessMemory` (`GameReader::read<T>`). **Nothing is
   ever written to the game.**

### Overlay specifics (`main.cpp`)
- Borderless topmost window, per-pixel alpha via `DwmExtendFrameIntoClientArea`
  (margins `-1`) + clearing the RT to `(0,0,0,0)`.
- Tracks the game window (found by PID) each frame and matches its client rect.
- **INSERT** toggles the panel, polled with `GetAsyncKeyState` (works while the
  game has focus).
- Click-through by default; becomes solid only while the cursor is over the
  panel (`io.WantCaptureMouse`), so gameplay input is never blocked. Absolute
  mouse pos is fed every frame so hover works even while click-through.
- `drawPlayer3DBox()` builds a simple 0.9 x 0.9 x 2.35 world-space box around
  the local player's movement position, projects corners using active camera
  yaw/pitch/FOV, and draws shadowed ImGui foreground lines. This is still
  external rendering only; it does not hook or write to the game.

### Build & run
```powershell
cmake -S PlayerInfoOverlay -B PlayerInfoOverlay/build -A x64
cmake --build PlayerInfoOverlay/build --config Release
# then run build/Release/PlayerInfoOverlay.exe (accept the UAC prompt)
```
A 64-bit reader is fine for the 32-bit target (RPM with explicit addresses).
Play in **Windowed** / **Windowed-Fullscreen** — true exclusive fullscreen
bypasses DWM and hides any external overlay.

---

## 9. Scope / ethics

This is a **read-only** memory-reading tool for a private-server client, built
for reverse-engineering/learning. It does not write game memory, inject code, or
automate input. Keep additions read-only; if you extend it, prefer displaying
information over modifying game state.
