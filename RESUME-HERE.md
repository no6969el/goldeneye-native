# Resume here

State as of the pause, and what to pick up next.

---

## Where things stand

| Milestone | State |
|---|---|
| **M0** — verify the ROM, extract assets | **done** |
| **M1** — libultra shim | **substantially done**; `A_ADPCM` verified 47/47 bit-exact (see §1) |
| **M2** — RDP backend | rooms and levels render via `room_render`; **RT64 wired, compiles and links** — needs a display to run |
| **Host port** | **boots the whole intro sequence**: legal screen → Nintendo logo → Rare logo → gunbarrel, with music. ~1,300 graphics tasks and one per frame, sustained. Crashes reproducibly at frame 2520 in the gunbarrel model walk. No renderer attached yet |
| M3–M6 | not started |

### The port is DETERMINISTIC as of this pause

It was not, and that mattered more than any single crash: the same binary on the
same ROM died at frame 7490, 4960 and 4930 on three consecutive runs. Disabling
ASLR made it identical twice, which located the cause.

The fiber stacks came from `malloc`. The renderer takes `Mtxf` and
`ModelRenderData` off the stack and writes their addresses into display lists --
`gSPMatrix(gdl++, osVirtualToPhysical(mtx))` with `mtx` a local -- which works on
the N64 because the thread stacks ARE in RDRAM. On a host they were not, so a
64-bit stack address got truncated to whichever 32 bits the allocator happened to
give it.

`src/ultra/fiber.cpp` now allocates stacks **inside RDRAM**, from a four-megabyte
region mapped above the eight the game can see (`kStackRegionBase`,
src/ultra/rdram.h). The game's own allocator runs from 0x8008E360 to 0x807FE000 --
measured, not assumed -- so there was no room below it and the space is added on
top. `osVirtualToPhysical` now translates a stack matrix, and three runs give the
same frame and the same fault address.

### Where it gets to

Legal screen, Nintendo logo, Rare logo, gunbarrel intro with music, the cast
sequence, and then the attract-mode demo, which loads a real level. As of this
pause the whole of `load_bg_file` completes for Archives: the section table, the
59-room list, the portal graph, the environment data, the .stan collision file,
and every room's vertex run and display list, decompressed and converted. The
setup file's header and its waypoint, waygroup, ailist, path, pad and boundpad
lists convert too.

The propDef records and the intro section convert too, now, and with them
`lvlStageLoad` runs to completion: every prop and door placed, all 275 pads and
bound pads resolved to collision tiles, the path tables built, and the intro
list walked. **The game reaches its main loop** -- `bossMainloop` ->
`lvlViewMoveTick` -> `MoveBond` -- with Bond standing on a real collision tile
in Frigate at (531.8, 356.2, -1320.5), spawned from the setup file's own start
pad.

It dies there, in `bondviewTrySimpleMovePlayerCollision`, with `next_pos` NaN on
a tick where the stick is centred. Bond's position and tile are both sound, so
that NaN is made inside the movement code rather than carried in from the level
data -- the first bug in this port that is about the game's own arithmetic
rather than the shape of its cartridge data.

That last stretch needed a new kind of tool. The setup records are the first
cartridge structs the port cannot PIN -- `ObjectRecord` is 128 bytes on the
cartridge and 144 here, and `GE_N64PTR` does not compile for it because
`src/game/gobjdata.c` is a static initializer table of them. So a record has two
strides, one in the file and one in memory, and
`tools/gen_struct_expand.py` generates the conversion between them from two DWARF
builds of the same headers. `PRIORITIES.md` §P3p has the account, including the
four bugs that were hiding behind it -- one of which was in the port's own
conversion code and was caught by an invariant rather than a crash.

### The bug worth reading about

`GFX_CMD` was returning an unsigned opcode. PR/gbi.h declares it `int cmd : 8`
-- SIGNED -- so `G_ENDDL` is `(G_IMMFIRST-7)` = **-72**, and the port was
comparing it against 184. Every display-list scan in the game therefore ran
forever: `bgBuildRoomVtxBounds` looking for the end of a room's list,
`lightfixture.c` looking for the last `G_VTX`. It presented as the level loader
hanging on room 1 of Archives with a display list that had been verified correct
and a terminator sitting at command 356 of 358.

**The goal this all serves:** GoldenEye, playable, in VR, on PC. The ordering in
`PRIORITIES.md` is deliberate and has not changed — the flat port has to run
before stereo is worth touching, because debugging stereo on an unproven port is
how projects lose months. VR is P7 and the math for it is already done and
tested; what is between here and there is a game that renders a frame at all.

Six test suites, all green in Release, Debug, and under ASan + UBSan:

```
ge_vr_tests     stereo projection math, pose decomposition
ge_gbi_tests    G_TRI4 decode (differential vs the game's own decoder)
ge_ultra_tests  scheduler, message queues, timers, RDRAM
ge_io_tests     PI/VI/SI, frame driver
ge_audio_tests  audio command list, AI drain model
ge_addr_tests   asset addresses vs the ROM offset space (link-time property)
```

`ge_io_tests` had in fact been RED since P3e and this file said otherwise: it
still asserted that a DMA to a non-RDRAM address is refused, which P3e
deliberately stopped doing because the game's statics live in host BSS. The
assertion was the stale half, not the code. Fixed, and all six pass.

Plus three corpus validators, which are not unit tests — they run against real
ROM data: `dl_validate` (every room display list), `geom_validate` (every room's
geometry), `audio_validate` (VADPCM vs ROM loop state — currently failing).

---

## Verified against the real ROM

ROM: NTSC-U, SHA1 `abe01e4aeb033b6c0836819f549c791b26cfde83`. Confirmed.

- **Assets extract byte-exact.** 2,698 textures; `combined.bin` hashes to
  `044fca472bf6ef6691fa02ff1b65ff474d86a9fa`, matching the documented value.
- **The microcode inferences were right.** `bin/gspboot.data.bin` — extracted
  from the ROM — confirmed the `0x2D0` lookup table is `{0, 10, … 150}` exactly
  as inferred, and the dispatch tables confirmed the DMA-group opcode map.
- **The interpreter survives the whole game.** 1,937 room display lists across
  23 levels, 192,714 triangles: 13 distinct opcodes, **zero unknown**, max vertex
  index 15 of a 16-entry cache, all 1,937 terminating at `G_ENDDL`.

Reproduce with:

```bash
python3 tools/extract_display_lists.py /path/to/n64decomp-007 /tmp/corpus
./build/dl_validate /tmp/corpus
```

---

## Pick up here

### 0. A level loads and the main loop runs — pick up at the NaN  *(current frontier)*

`lvlStageLoad` completes and `bossMainloop` starts ticking the level. It stops
in `bondviewTrySimpleMovePlayerCollision` (`bondview2.c:2267`) with `next_pos`
NaN. Deterministic.

Worth knowing before you start on it: `g_CurrentPlayer->field_488` holds a sane
position and a valid `current_tile_ptr`, and `g_Startpad[0]` is a real pad with
sane `pos`/`up`/`look`. So the level data underneath is right and the NaN is
produced by the movement computation. Locals in the caller frame come up as
`-inf` and `0xff800000` as well, which points at something uninitialised rather
than at a bad conversion.

Two stand-ins are in place from the stretch before it, both marked in the code
and both reporting once rather than passing quietly: one pad in Frigate matches a
collision tile by name but tests as geometrically outside it and is anchored to
the named tile, and a tile walk that starts from a null answers "crossed
nothing". The second exists only so the first is not fatal. Neither is a fix.

The class with no audit yet: **matching hacks that are out-of-bounds accesses.**
`init_path_table_links` spells its loop cursor `validationGroupCursors[-3]`, a
negative index into a one-element array. On the N64 that slot is real; here it
was whatever the host compiler put three slots down, and the load ran for five
minutes with no fault to point at. Expect more of these, and expect them to
present as hangs with plausible backtraces rather than as crashes.


### 1. It renders — how it got here

```
$ ge007 --rom ge007.u.z64 --frames 1200
assets : 729 loaded from ROM
tasks  : 3 graphics, 586 audio, 0 unrecognised
frames : 1200        (clean exit, no crash)
```

The port boots, loads and decompresses its assets, initialises every subsystem,
runs the game's own scheduler, loads a stage, **builds display lists and submits
them as RSP graphics tasks**, and streams audio continuously. Submit → RSP done
→ RDP done → `OS_SC_DONE_MSG` all work: 3 graphics tasks in, 3 completions back.

It read `"TWYCROSS BOARD OF GAME CLASSIFICATION"` out of the cartridge and laid
it out with the real font metrics on the way.

Full account in `PRIORITIES.md` §P3m. The two that unblocked it:

- **`os_sp.c` never signalled task completion.** A task is synchronous here, so
  it was already done — but the scheduler was never told, kept `curRSPTask` set
  forever, and dispatched exactly ONE task for the life of the process.
- **`__scHandleRetrace` read past the end of `OSScClient`** — deliberately, into
  `gfxClient[1].next`, which is zero on the N64. On the host the 16-byte element
  put that read inside `msgQ`. 15 client notifications in 120 frames became 212.

**Next fault**, and it is the only thing between here and continuous frames:

```
[ge-ultra] PI DMA reads past end of ROM (offset 0x10FF528 + 4290846944) -- refused
```

4290846944 is −4120352 as s32 — a negative length gone huge. Same family as the
bitfield pun in §P3m: something read from cartridge data with the wrong width or
order.

**Then attach a renderer.** `tools/room_render.cpp` already rasterises real
geometry with no GPU, so wiring it to the graphics handler in `os_sp.c` would
turn those display lists into an actual image without waiting for RT64.

**New bug class to know about:** bitfield allocation order. Big-endian MIPS
allocates MSB-first, x86 LSB-first, so a type-pun over a bitfield struct reads a
different field on each. `check_struct_layout.py` cannot detect it — the struct
is the same SIZE on both.

### 1. `A_ADPCM` is CORRECT — the oracle was wrong  *(resolved)*

This section used to say the VADPCM decoder was known-incorrect and rank it the
single most important open item. **It was never broken.** The test was.

`ALADPCMloop::state` was assumed to be the sixteen samples immediately BEFORE
the loop point. It is not: it is the frame CONTAINING the loop point, which is
what a resume seed has to be, since the decoder can only restart on a frame
boundary. Measured rather than argued — decode all 48 looped ADPCM waves in the
instrument bank in full and search the output for each state sequence:

```
48 looped ADPCM waves, 0 where the state appears nowhere
found_index - loop_start  ==  -(loop_start % 16)     in every single case
```

Two things had to agree for the old result to look real, and both encoded the
same wrong assumption: `extract_adpcm_vectors.py` decoded `loop_start // 16`
frames, and `audio_validate.cpp` compared at `pcm + loop_start - 16`.

```
before:   0 /  4 vectors bit-exact
after:   47 / 47 vectors bit-exact
```

47 rather than 4 because a non-frame-aligned `loop_start` is perfectly checkable
under the correct reading, so the old alignment skip is gone.

**How it was found, since the method generalises:** the docs listed "compare
against a third-party VADPCM decoder" as untried. Writing one independently, from
the published algorithm, produced output *identical* to the port's on all four
vectors — two implementations agreeing with each other and disagreeing with the
oracle. That points at the oracle, not the decoder.

The previous note in this file said "The oracle is sound." That was an argument.

### 1b. The other three audio commands

`A_RESAMPLE`, `A_ENVMIXER`, `A_POLEF` remain unvalidated (not known-wrong):

- `A_RESAMPLE` interpolates **linearly**; hardware uses a 4-tap polyphase
  filter. Duller than hardware, not wrong. Pitch and length are exact, which is
  what keeps the sequence player in sync.
- `A_ENVMIXER`'s ramp is applied per sample; hardware may apply it per 8-sample
  group. If wrong, fades run at 8× or ⅛× speed — audible, not silent.
- `A_POLEF` is a one-pole filter on the aux path; least consequential.

### 2. Run RT64 on a machine with a display  *(wiring done)*

`tools/room_render.cpp` now renders a real room — and all 92 rooms of a level
placed at their origins — through our interpreter, vertex pipeline and a
software rasteriser: 1,605 triangles, zero walks bailed, zero failed address
resolutions. So the chain that feeds RT64 is verified; what remains is the
handoff, not the geometry.

The software rasteriser is a **verification tool, not the shipping renderer**.
Keep it: it runs headless with no GPU, which makes it viable in CI, and it is
the fastest way to see whether a change broke geometry.

RT64 is now **wired**: `src/rhi/rt64_backend.{h,cpp}` builds `Application::Core`
from our RDRAM, and `tools/rt64_probe.cpp` drives setup → display list →
present. It compiles clean against RT64's headers and links into an 8.3 MB
executable.

The probe runs here and stops at `SDL_Vulkan_CreateSurface failed` — this
container is headless and RT64 has no offscreen path (`SDL_VIDEODRIVER=offscreen`
and `=dummy` both fail). Environment limit, not a code one.

**On a machine with a display, this is the next command to run:**

```bash
cmake -S . -B build -DRT64_DIR=/path/to/rt64
cmake --build build --target rt64_probe -j
./build/rt64_probe /tmp/room_pair.bin /path/to/decomp/bin
```

Apply `patches/rt64-goldeneye-hashes.patch` first, or detection will report the
game as unsupported despite the handler being present and correct.

#### RT64 findings

See `RT64-INTEGRATION.md`. Headlines:

- RT64 builds on Linux (Vulkan + SDL2). Needs `libgtk-3-dev`, which the README
  does not mention — configure fails on a bare `gtk+-3.0` message.
- `src/gbi/rt64_gbi_f3dgolden.cpp` already implements `G_TRI4` at `0xB1`, and
  its decode matches ours exactly. Three independent implementations now agree.
- **Ucode auto-detection will not fire on this ROM.** RT64's F3D_GOLDEN hashes
  are the right lengths (0x1420 / 0x800, matching our blobs byte for byte) but
  different values — a different build. Either add a `GBISegment` for our
  hashes or force `GBIUCode::F3DGOLDEN`. Both listed in the doc.
- No adapter to write: RT64 takes raw RDRAM + display list addresses and runs
  its own interpreter (deferred RSP in compute). `IDrawSink` stays the reference
  model for culling and read-back, not a feed path.

### 2b. Vertex pipeline groundwork

Groundwork now done: `src/rhi/vertex_pipeline.*` implements the RSP's transform
in libultra conventions (row-vector, perspNorm, 2.2-fixed-point viewport) and is
validated against every room in the game. The renderer still does the drawing
transform on the GPU — the CPU path exists for culling and for the screen-space
read-back that `bondview` and the HUD depend on.

**Scale fact worth knowing before you tune anything:** the game's projection is
`znear=10, zfar=300` (`src/game/bondview2.c:8423`) while the mean room is ~576
units across. You cannot see a whole room at once. That short draw distance is
why GoldenEye is so foggy, and any camera test placed outside a room will
wrongly appear to cull everything.


### 3. Loose ends in the shim

- `stackHighWater()` is a stub. The fiber layer poisons stacks with `0xCD`;
  scanning for it would report real usage. The game's stack sizes were tuned for
  a 1997 compiler and are worth checking before trusting.
- ~~`G_CULLDL` is a deliberate never-cull.~~ **Done.** `src/rhi/vertex_pipeline.*`
  computes real clip outcodes and `cmdCullDl` applies the hardware rule.
  Validated on all 1,296 rooms by `tools/geom_validate.cpp`. Never-cull is
  retained only as the fallback when no vertex in the range is loaded.
- Interpreter gaps marked `TODO(phase2)`: fog params, light colours, viewport
  forwarding.
- `osMotorStart` applies rumble to port 0 unconditionally — the channel is not
  threaded through yet.

---

## Things that will bite you

Collected because each one cost time here, and none of them announces itself.

1. **`OS_K0_TO_PHYSICAL` must be redefined as a call, not a mask.** Every
   translation unit must see it. One file picking up the old bit-masking macro
   silently truncates 64-bit pointers into display lists that address nothing.
   Grep for raw `& 0x1FFFFFFF` too — not every site went through the macro.

2. **PI DMA does not byte-swap, deliberately.** A DMA does not know what it
   carries: a `Vtx` needs a 16-bit swap, a `Gfx` a 32-bit swap, an I8 texture
   none. Swapping belongs in the asset loaders where the structure is known. Full
   reasoning in `os_io.h`.

3. **1172 compression omits trailing zero bytes.** The game inflates into an
   already-zeroed buffer, so a final `G_ENDDL` is stored as the single byte
   `0xB8`. ~1% of room lists inflate to a non-multiple of 8 and must be
   zero-padded. Skip it and the walker runs off the end — and it looks like an
   interpreter bug.

4. **Segment 0 must stay 0.** A physical address is itself resolved through the
   segment table. Remapping segment 0 relocates a display list's own base and the
   walker reads zeroes, which look exactly like a list full of `G_SPNOOP`.

5. **Equal priority does not preempt.** `osStartThread` compares with a strict
   `<`. A "fairer" scheduler breaks the game.

6. **Simulate once, render twice.** Running the game tick per eye desynchronises
   the eyes by a simulation step. Most reliably sickening bug available, and it
   does not show up in screenshots.

7. **`osAiGetLength()` cannot be a stub.** `src/audi.c:517` sizes every audio
   frame from it. Return a constant and the game over- or under-produces forever.

8. **`osPiGetStatus()` must always report idle.** `src/init.c:133` spins on it;
   on a cooperative scheduler, reporting busy hangs the boot forever.

9. **The host image must be linked above the ROM address space, and below 4 GB.**
   The game hands the PI the *address* of an asset symbol as a cartridge offset,
   so the port translates host addresses back to ROM offsets. At a default load
   address the two spaces are the same numbers and a third of the ROM is
   shadowed — silently. Above 4 GB, `osPiStartDma`'s `u32 devAddr` truncates the
   pointer back down into the range you were avoiding. 0x20000000 is the window;
   `geAssetsCheckAddressSpace()` refuses to start if it is lost.
