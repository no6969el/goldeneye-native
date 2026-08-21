# goldeneye-native

**The GoldenEye 007 decompilation, compiled and run natively.** Not recompiled,
not emulated — the decompiled C from [`n64decomp/007`](https://github.com/n64decomp/007)
built with a host toolchain, against a reimplementation of the hardware it
expects.

As far as [`PRIOR-ART.md`](PRIOR-ART.md) can establish, this is the only one.
Every other working GoldenEye-on-PC effort is static recompilation of the
original MIPS. Both approaches are legitimate and the recomps are much further
along; what this gets you instead is **readable, hackable game code** — the
engine as C you can set a breakpoint in, not as translated machine code.

The eventual target is 6DoF VR, which is why the camera and stereo-projection
work is already here. That is a roadmap, not a claim: see *Status*.

## What this actually is

- A **libultra reimplementation** — cooperative fiber scheduler, message queues,
  PI/DMA, VI, SI, a flat 8 MB RDRAM with real physical addressing.
- **Cartridge-data conversion, generated rather than hand-written.** Byte order,
  pointer width and struct layout all differ from the N64's. The swappers and
  layout-converters are derived from DWARF, from two builds of the game's own
  headers, so a struct and its converter cannot drift apart.
- An independently derived, ROM-validated map of GoldenEye's display-list
  dialect — **F3D plus `G_TRI4`** — in [`MICROCODE-SPEC.md`](MICROCODE-SPEC.md),
  with a matching interpreter and a software rasteriser that needs no GPU.
- A **host-patch discipline** that keeps the matching ROM build byte-identical.
  Every change to the decomp lives in `patches/`, guarded by `GE_HOST_PORT`,
  with the original preserved verbatim. `make` still produces `abe01e4a…`.
- Audit tooling for the bug classes this kind of port actually hits:
  sign-extended pointers, unpinnable structs, bitfield allocation order, enum
  signedness, out-of-bounds matching hacks.

## What this is not

**It has never drawn a frame.** It boots, runs the game's own scheduler, loads a
level completely — geometry, collision, props, doors, pads, path tables — and
reaches the main game loop with Bond standing on a real collision tile. Then it
hits a NaN in the movement code. There is no window, no input, no audio output.

If you want to *play* GoldenEye on a PC today, use one of the recompilation
projects in [`PRIOR-ART.md`](PRIOR-ART.md). If you want to understand how the
game works, or build something on top of its actual source, this is for you.

## Ship no assets

You supply your own ROM. Nothing here contains Nintendo, Rare or MGM material —
no textures, no audio, no sequences, no ROM data. The decompiled game source is
**not vendored either**: it is cloned from upstream and modified through
`patches/`, because `n64decomp/007` carries no licence file. That is what keeps
this repository MIT and distributable. See [`VENDORING.md`](VENDORING.md).

## Status

| Component | State |
|---|---|
| **The game's own C, compiled on a host** | **done — 162/162 files build to objects** |
| **Matching ROM build preserved** | **done — `make` still produces `abe01e4a…`, verified per object** |
| Microcode archaeology (`MICROCODE-SPEC.md`) | **done, verified against a real ROM** |
| GBI display-list walker (F3D + `G_TRI4`) | **done — validated over all 1,937 room display lists** |
| Fiber context switching (ucontext / Win32) | **done** |
| Scheduler: threads, priorities, message queues, timers | **done, tested against exact interleavings** |
| Flat 8 MB RDRAM + physical addressing | **done, tested** |
| PI/DMA + ROM mounting (.z64/.v64/.n64) | **done, tested** |
| VI framebuffer latching | **done, tested** |
| SI controllers + rumble | **done, tested** |
| Frame driver (`hostRunFrame`) | **done, tested end-to-end** |
| Audio command-list interpreter (16 ABI commands) | 12 exact and tested; `A_ADPCM` **known incorrect** (oracle in place); 3 unvalidated |
| AI shim + drain model | **done, tested** |
| Vertex pipeline (clip outcodes, viewport, perspNorm) | **done** — validated on all 1,296 rooms |
| `G_CULLDL` real culling | **done** — was a never-cull stub |
| Software rasteriser (verification) | **done** — renders real rooms and whole levels |
| Setup-file conversion (propDefs, intro, pads, ailists) | **done** — two-stride relayout, generated |
| A level loads end to end | **done** — Frigate: props, doors, 275 pads, path tables |
| Reaches the main game loop | **done** — dies on a NaN in `MoveBond`, the current frontier |
| RT64 integration | **wired — compiles and links**; needs a display to run (see `RT64-INTEGRATION.md`) |
| Stereo projection math (asymmetric frustum, libultra layout) | **done, tested** |
| Pose → game angle decomposition | **done, tested** |
| View/model matrix construction | **done, tested** |
| OpenXR session, swapchains, frame loop | **implemented**, needs a graphics backend |
| Action sets + bindings (Simple, Touch) | **implemented**; Index/Vive/WMR TODO |
| Bridge C ABI (`ge_vr.h`) | **implemented** |

## Build

```bash
# Without the OpenXR SDK — builds the null backend, runs the math tests.
cmake -S . -B build -DGE_VR_WITH_OPENXR=OFF
cmake --build build -j
./build/ge_vr_tests

# With OpenXR (fetches the Khronos loader).
cmake -S . -B build-xr -DGE_VR_WITH_OPENXR=ON
cmake --build build-xr -j
```

Both configurations are verified building clean, and the test suite passes in
both.

### Validating against a real ROM

Unit tests use synthetic display lists. To run the interpreter over every room
display list in the actual game (no ROM needed — the room data is C source in
the decomp):

```bash
python3 tools/extract_display_lists.py /path/to/n64decomp-007 /tmp/corpus
./build/dl_validate /tmp/corpus
```

### The two measurements that matter most

Apply `patches/decomp-host-port.patch` to a decomp checkout, then:

```bash
tools/compile_census.sh /path/to/n64decomp-007   # 134/134 files, 0 errors
tools/link_census.sh    /path/to/n64decomp-007   # 162 objects; 149 symbols left
cd /path/to/n64decomp-007 && make                # still abe01e4a... byte-identical
```

The third is not optional. This is a *matching* decompilation, and a host patch
that changes the IDO token stream breaks the ROM while still compiling
perfectly — `patches/HOST-PORT-PATCHES.md` section 5 is a worked example that
cost one instruction and a full bisection to find.

Current result: 1,937 lists, 192,714 triangles, 13 distinct opcodes, **0
unknown**, max vertex index 15 of a 16-entry cache, all 1,937 terminating at
`G_ENDDL`. See `MICROCODE-SPEC.md` for what that establishes.

Geometry — does the transform put things in the right *place*, not just read the
right opcodes:

```bash
python3 tools/extract_room_geometry.py /path/to/n64decomp-007 /tmp/geom.bin
./build/geom_validate /tmp/geom.bin
```

23 levels, 1,296 rooms, 342,551 vertices. Checks room-local centring, level
assembly, finite screen coordinates, and that culling *discriminates*. Includes
a deliberate control — a transposed translate — to prove the assembly check can
actually fail.

Render a real room, or a whole level, end-to-end through the interpreter,
vertex pipeline and a software rasteriser:

```bash
python3 tools/extract_room_pair.py /path/to/n64decomp-007 bg_ame /tmp/pair.bin
./build/room_render /tmp/pair.bin  0 /tmp/room.ppm   800 600   # one room
./build/room_render /tmp/pair.bin -1 /tmp/level.ppm 1000 750   # whole level
```

Untextured by design — flat-shaded geometry is what exposes errors in the
display list walk, vertex cache, segment resolution, matrix convention and
viewport transform. A texture would hide them.

Audio — currently **fails**, see `RESUME-HERE.md` §1:

```bash
python3 tools/extract_adpcm_vectors.py /path/to/n64decomp-007 /tmp/vec.bin
./build/audio_validate /tmp/vec.bin
```

## Layout

```
MICROCODE-SPEC.md        The gsp3D dialect, with evidence and confidence levels.
BUILD-PLAN.md            Asset matrix and the ordered critical path.
src/ultra/os.h           libultra-compatible API. Replaces <PR/os.h> for the port.
src/ultra/fiber.*        Cooperative context switching.
src/ultra/os_thread.cpp  The scheduler. Semantics copied from src/libultra/os/.
src/ultra/rdram.*        The flat 8 MB buffer everything is addressed against.
src/gbi/gbi.{h,cpp}      Opcode map, G_TRI4 decode, N64 fixed-point matrices.
src/gbi/gbi_interp.*     Display-list walker: segments, matrix stack, vertex cache.
src/host/ge_swap*.c      Generated cartridge byte-order conversion + union policy.
src/host/ge_expand.*     Generated cartridge -> host LAYOUT conversion (two strides).
tools/gen_struct_swap.py   Derives the swappers from DWARF. Ambiguity -> link error.
tools/gen_struct_expand.py Derives the layout converters from two DWARF builds.
src/rhi/vertex_pipeline.*  RSP vertex transform: clip outcodes, viewport, perspNorm.
src/rhi/rt64_backend.*     Hands our RDRAM and display lists to RT64.
include/ge_vr/ge_vr.h    C ABI the decomp calls. Plain C89, no C++ or OpenXR leaks.
src/xr_math.{h,cpp}      Projection + pose math. libultra row-vector layout.
src/xr_session.{h,cpp}   OpenXR lifetime + frame loop. Owns frame pacing.
src/xr_input.{h,cpp}     Action sets, bindings, OSContPad synthesis.
src/ge_vr_bridge.cpp     Implements ge_vr.h. The only place XR and GE concepts meet.
tests/test_math.cpp      Parity + convention tests.
patches/DECOMP-PATCHES.md  Exactly what changes in n64decomp/007, and why.
patches/HOST-PORT-PATCHES.md  The 33 bug families this port has hit, with fixes.
PRIOR-ART.md             Every other GoldenEye PC-port effort, and where we differ.
VENDORING.md             What may be reused from other projects, and on what terms.
RESUME-HERE.md           Current frontier, and what bites next.
```

## The three things most likely to bite

**1. `G_TRI4` termination.** Not "skip all-zero triangles" — the microcode tests
the *entire remaining `w1`* before extracting nibbles, and never looks at the `z`
nibbles. Trailing empty slots terminate; an interior zero slot followed by a
nonzero one does not, and is drawn as a degenerate triangle. Get this wrong and
you render permuted windings, which looks like a backface-culling problem and
gets misdiagnosed for days. `test_gbi.cpp` checks the decoder against the game's
own CPU-side decoder over 16,368 exhaustive cases for exactly this reason.

**2. Row-vector matrices.** libultra uses `v * M` with translation in row 3 and
`m[2][3] == -1`. The frustum shear goes in **row** 2, not column 2. Getting this
backwards produces an image that looks nearly right and is subtly sheared —
which readers will blame on tracking, not on math. `test_math.cpp` asserts the
placement explicitly for exactly this reason.

**3. Simulate once, render twice.** Running the game tick per eye desynchronises
the eyes by a simulation step. It's the most reliably sickening bug available
here and it doesn't show up in screenshots.

## Legal

Source only. No ROM, no extracted assets, no textures, no audio. Users supply
their own cartridge dump and the build extracts locally. Do not bundle, mirror,
or auto-download game data.
