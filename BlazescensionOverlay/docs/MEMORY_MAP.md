# Ascension Memory Map

This file is the agent-facing reference for the memory-reading side of
BlazescensionOverlay. Keep it updated whenever offsets or reader behavior change.

## Target Binary

| Field | Value |
|---|---|
| Module | `Ascension.exe` |
| Typical path | `C:\Ascension\Launcher\resources\ascension-live\Ascension.exe` |
| Architecture | x86, running under WoW64 |
| Preferred image base | `0x400000` |
| MD5 | `aae945760f18fd0db849aaf8cc9761ad` |
| SHA-256 | `5b26e33b2129737af3a0c3164459f4c9b109398dab921f76c6740a8746fbb929` |

Always read the runtime module base and use `moduleBase + RVA`.

## Read-Only Rule

The overlay only uses `ReadProcessMemory`. Do not add writes, injection, hooks,
or input automation to this project. New features should display information
only.

## Local Player Chain

The object manager is stored in thread-local storage. For a 64-bit reader of the
32-bit Ascension process, `NtQueryInformationThread(ThreadBasicInformation)`
returns the 64-bit TEB. The 32-bit TEB is at `TEB64 + 0x2000`.

```text
TlsArray  = *(TEB32 + 0x2C)
TlsIndex  = *(moduleBase + 0x9439BC)
P         = *(TlsArray + TlsIndex * 4)
CurMgr    = *(P + 0x08)
localGUID = *(CurMgr + 0xC0) as u64
object    = hashLookup(CurMgr, localGUID)
descBase  = *(object + 0xD0)
```

The reader enumerates target-process threads and accepts the first `CurMgr`
whose local GUID is non-zero.

## Object Manager Hash Lookup

Given `CurMgr`, `guidLow`, and `guidHigh`:

```text
hashBase = *(CurMgr + 0x1C)
mask     = *(CurMgr + 0x24)
bucket   = 12 * (guidLow & mask)
entry    = *(hashBase + bucket + 8)
delta    = *(hashBase + bucket)

while entry != 0 and (entry & 1) == 0:
    if *(entry + 0x18) == guidLow and
       *(entry + 0x30) == guidLow and
       *(entry + 0x34) == guidHigh:
        return entry
    entry = *(entry + delta + 4)
```

This lookup works for local player, target, focus, mouseover, and future GUID
sources.

## Static RVAs

| RVA | Meaning |
|---|---|
| `0x9439BC` | TLS index |
| `0x6F5220` | Power divisor table, `u32[8]` |
| `0x77436C` | Active camera/world root pointer |
| `0x6CE4B4` | Commentator camera/controller X copy |
| `0x6CE4B8` | Commentator camera/controller Y copy |
| `0x6CE4BC` | Commentator camera/controller Z copy |
| `0x6CE4E4` | Commentator camera/controller FOV copy in radians |
| `0x7D0A04` | Health display-cache cvar pointer |
| `0x7D0A08` | Power display-cache cvar pointer |
| `0x7D07B0` | Target GUID low, high at `+4` |
| `0x7D07D0` | Focus GUID low, high at `+4` |
| `0x7D07A0` | Mouseover GUID low, high at `+4` |
| `0x85D938` | Player name cache DB (`unk_C5D938`) |
| `0x85DB58` | Creature name cache DB (`unk_C5DB58`) |

## Object, Descriptor, Movement Offsets

| Offset | Meaning |
|---|---|
| Object `+0x08` | Type-info pointer (see Object Enumeration below) |
| Object `+0xD0` | Descriptor pointer |
| Object `+0xD8` | Movement pointer |
| Object `+0x964` | Creature name cache pointer (see Unit Names below) |
| Object `+0xFB0` | Cached/display health |
| Object `+0xFB4 + 4 * powerType` | Cached/display power |
| Descriptor `+0x47` | Current power type |
| Descriptor `+0x48` | Health |
| Descriptor `+0x4C + 4 * powerType` | Power |
| Descriptor `+0x68` | Max health |
| Descriptor `+0x6C + 4 * powerType` | Max power |
| Descriptor `+0xC0` | Level |
| Descriptor `+0x114` | Creature display/name id used by creature-name DB |
| Movement `+0x10` | World X |
| Movement `+0x14` | World Y |
| Movement `+0x18` | World Z |
| Movement `+0x8C` | Current speed |

Power divisor table:

```text
type:    0     1     2      3       4          5       6
power:   Mana  Rage  Focus  Energy  Happiness  Runes   Runic Power
divisor: 1     10    1      1       1000       1       10
```

## Object Enumeration (Nearby NPC/Player ESP)

`hashLookup` (see Object Manager Hash Lookup above) resolves one GUID at a
time, but the hash table it walks holds every currently loaded object, not
just the local player/target/focus/mouseover. Confirmed via `sub_4D4BB0`
(the raw hash-bucket walker) and `sub_4D4DB0` (its type-filtered wrapper,
called ~150 places): the hash table is a
`TSExplicitList<CGObject_C, N>` (RTTI string `.?AV?$TSExplicitList@VCGObject_C@@...`),
and each `entry` returned by the walk **is** the object pointer itself — the
hash node fields (`node::KeyLow/GuidLow/GuidHigh`) are embedded directly in
`CGObject_C`.

To enumerate everything instead of one GUID, walk every bucket
(`0 .. HashMask`) and follow each bucket's chain with the same delta-linked
traversal `hashLookup` already uses per-bucket, just without the GUID
comparison — every non-null, non-tagged (`!(entry & 1)`) node encountered is
a live object.

### Object type filtering

```text
typeInfoPtr = *(object + 0x08)
typeMask    = *(typeInfoPtr + 0x08)
```

`typeMask` is a cumulative bitmask, standard WoW-client ordering, confirmed
by decompiling a debug-info dump function (`sub_404B80`) that labels each
`sub_4D4DB0` call by the mask it passes: `"Cursor Item"` → mask `2`,
`"Local Player"` → mask `16`.

```text
1   Object          (base type, always set)
2   Item
4   Container
8   Unit             <- creatures / NPCs
16  Player
32  GameObject
64  DynamicObject
128 Corpse
```

A plain NPC has `typeMask == 9` (`Object | Unit`, no `Player` bit); a player
character has `typeMask == 25` (`Object | Unit | Player`). Filter with
`typeMask & 8` for "is a unit" and `typeMask & 16` for "is a player" — do not
compare for exact equality, since the mask is inherited/cumulative.

### Cost

A full hash-table walk is far more expensive than the four fixed GUID
lookups (thousands of small reads instead of a handful), so
`GameReader::scanNearbyUnits` runs on its own slow poll (`AppConfig::nearbyPollHz`,
default 4 Hz), and results are cached between scans. The core player, target,
focus, mouseover, and camera snapshot is read once per rendered overlay frame.
Distance filtering happens during the walk itself (skip before the descriptor
read, not after) to avoid the cost of resolving stats for units that will be
discarded.

## Unit Names

Reversed from the Lua `UnitName` handler (`sub_60E740` → `sub_4FD0E0` →
`sub_72A000`). Names are resolved differently for players and creatures.

### Players

Player names live in a GUID-keyed name-cache DB at the static struct
`unk_C5D938` (RVA `0x85D938`). The DB uses the **same bucket layout as the
object manager** (`sub_6792E0` is structurally identical to `sub_4D4BB0`):

Important: `UnitName` does not feed this lookup from the object-manager hash
node GUID. In `sub_72A000`, the player path reads the name-cache GUID from
`data = *(object + 0x08)`, then `guidLow = *(data + 0x00)` and
`guidHigh = *(data + 0x04)`. The type mask still lives at `data + 0x08`.

```text
db       = moduleBase + 0x85D938 + 0x08
hashBase = *(db + 0x1C)
mask     = *(db + 0x24)
bucket   = 12 * (guidLow & mask)
entry    = *(hashBase + bucket + 8)
delta    = *(hashBase + bucket)

while entry != 0 and (entry & 1) == 0:
    if *(entry + 0x00) == guidLow and
       *(entry + 0x18) == guidLow and
       *(entry + 0x1C) == guidHigh:
        if *(byte)(entry + 0x178) != 0:      # record populated
            name = cstring at (entry + 0x20) # inline, null-terminated
        break
    entry = *(entry + delta + 4)
```

`sub_67D770` uses `unk_C5D938 + 0x08` as the hash lookup object, returns
`record + 0x20` (the inline name string) once the `+0x178` valid byte is set,
and `UnitName` pushes exactly that pointer to Lua. It may also provide a
second string at `name + 0x34`.

### Creatures / NPCs

`sub_72A000` first tries a creature-name DB at `unk_C5DB58` (RVA `0x85DB58`),
keyed by `*(descriptor + 0x114)`. `sub_67EA30` uses `unk_C5DB58 + 0x08` as
the hash lookup object. The DB bucket layout is the same as the object manager,
but the record match is only `*(record + 0x00) == key`. `sub_67EA30` returns
`record + 0x18` when `*(byte *)(record + 0x78)` is set.

If the descriptor key is zero or the DB record is not available yet,
`sub_72A000`'s creature fallback path reads the name straight off the object:

```asm
mov esi, [esi+964h]   ; object + 0x964 -> creature name cache
mov eax, [esi+5Ch]    ; +0x5C -> char*
```

so `name = cstring at *(*(object + 0x964) + 0x5C)`. This is the widely-used
external creature-name chain.

`GameReader::readUnitFromObject` picks the source by object type: units with
the `Player` type bit (`0x10`) use the player name cache with the
`*(object+0x08)` GUID source, everything else tries the creature DB and then
falls back to the `0x964` chain.

## Camera

```text
root       = *(moduleBase + 0x77436C)
camera     = *(root + 0x7E20)
cameraPosX = *(camera + 0x08)
cameraPosY = *(camera + 0x0C)
cameraPosZ = *(camera + 0x10)
matrix3x3  = *(camera + 0x14)             ; 9 floats, written by sub_607BD0/sub_4C56D0
fov        = *(camera + 0x40)
yaw        = *(camera + 0x11C)
pitch      = *(camera + 0x120)
```

Yaw, pitch, and FOV are radians. `CommentatorGetCamera` exposes the global
controller copies at `0xACE4B4..0xACE4E4`, but `CommentatorSetCamera` and
`sub_6038A0` show the active render camera stores position at `camera+0x08`,
`+0x0C`, `+0x10` and active FOV at `camera+0x40`. Projection code reads the
active camera object directly.

For projection, prefer the camera matrix at `camera+0x14`. `sub_604490` builds
the effective render yaw/pitch/roll by combining `camera+0x11C/+0x120/+0x124`
with follow-camera offsets such as `camera+0x12C` and `camera+0x134`, then calls
`sub_607BD0`, which writes a 3x3 rotation matrix through `sub_4C56D0`. Raw
yaw/pitch can be correct while dragging mouse-look but wrong after release
because follow-camera offsets are then applied.

### World-to-Screen: use the game's own view-projection matrix

`CGWorldFrame::GetScreenCoordinates` (`sub_4F6D20`) is the function the client
itself calls to project world points to screen. It reads a full 4x4
view-projection matrix off the `WorldFrame` object (the same object
`ActiveCameraRoot` points to) and applies it with a row-vector multiply
(`sub_4C2270`):

```text
root         = *(moduleBase + 0x77436C)   ; WorldFrame
viewProj4x4  = root + 0x340               ; 16 floats, row-vector convention
nearClip     = *(camera + 0x38)

relative     = world - cameraPos          ; camera+0x08/0x0C/0x10
clip.x       = relative . column0(viewProj)
clip.y       = relative . column1(viewProj)
clip.z       = relative . column2(viewProj)
clip.w       = relative . column3(viewProj)

reject if clip.z < nearClip or clip.w <= 0

screenX = viewportW * 0.5 * (1 + clip.x / clip.w)
screenY = viewportH * 0.5 * (1 - clip.y / clip.w)
```

This replaced an earlier approach that reconstructed a right/up/forward basis
by brute-forcing combinations of the rows and columns of the 3x3 rotation
matrix at `camera+0x14`, scored by how close the local player's pivot point
landed to screen center. That heuristic was the root cause of skewed 3D ESP
boxes: rows and columns of a rotation matrix are each orthonormal within their
own family, but a row is not generally perpendicular to a column, so a
candidate built by mixing the two families (e.g. forward from a row, right
from a column) is a non-orthogonal, sheared basis. The single-point pivot
score could not detect this because shear is smallest exactly at the point
being scored (the player's chest, near screen center) and grows toward the
edges of the box.

`Projection.cpp` now reads `WorldFrame+0x340` every snapshot and, in `Auto`
mode, projects directly through it (`ProjectionBasis::useViewProj`) whenever
the read is finite and the local player projects on-screen with it. The old
row/column search still exists as a fallback for when the view-proj matrix is
unavailable, but it was also fixed to only combine axes from a single family
(all rows, or all columns) per candidate, since that was a latent skew bug in
the fallback path too.

## Re-Deriving Offsets

When the binary changes:

1. Find Lua API string names such as `UnitHealth`, `UnitPower`, `UnitLevel`,
   `GetUnitSpeed`, and `CommentatorGetCamera`.
2. Xref each string into the registration table. The function pointer is the
   next dword after the name pointer.
3. Decompile the function and recover descriptor or object offsets.
4. Decompile local GUID / object lookup functions to recover TLS and hash table
   layout.
5. Search for `Mover at invalid position` for movement XYZ fields.
6. Decompile `CommentatorGetCamera` and the active-camera resolver for camera
   statics.
