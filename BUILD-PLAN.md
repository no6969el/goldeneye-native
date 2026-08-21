# From here to a working build

Answers the two practical questions: what do the assets block, and what is the
actual critical path.

---

## 1. Asset dependency: what's in the repo, what isn't

The repo's readme says it "does not include all assets necessary for compiling
the ROMs", which reads as though nothing is there. In fact **most of the game
data is already in the repo as C source** — about 31 MB of it.

### Already present (no ROM needed)

| Data | Path | Size | Form |
|---|---|---|---|
| Level geometry | `assets/obseg/bg/` | 11 MB | C source |
| Collision / tiles | `assets/obseg/stan/` | 7.4 MB | C source |
| Level setups (spawns, objectives, AI lists) | `assets/obseg/setup/` | 6.6 MB | C source |
| Props / world objects | `assets/obseg/prop/` | 4.2 MB | C source |
| Weapon models | `assets/obseg/gun/` | 1.3 MB | C source |
| Character models | `assets/obseg/chr/` | 996 KB | C source |
| Text / localisation | `assets/obseg/text/` | 528 KB | C source |
| Fonts | `assets/font/` | 84 KB | C source |
| Skeletons, gait objects | `assets/embedded/` | 68 KB | C source |

### Missing — extracted from your own ROM

| Data | Path | Size | Needed for |
|---|---|---|---|
| **Textures** (2,698 images) | `assets/images/split/` | 3,075,864 B | anything to look right |
| Audio sample banks | `assets/music/{instruments,sfx}.{ctl,tbl}` | — | sound |
| Music sequences | `assets/music/*.bin` | — | music |
| Attract-mode demos | `assets/ramrom/*.bin` (14 files) | — | title-screen demos only |
| Graphics microcode | `bin/gspboot.{text,data}.bin` | — | **the ROM build only — not us** |
| Audio microcode | `bin/aspboot.{text,data}.bin` | — | ROM build; us only if we HLE audio |
| One object segment | `assets/obseg/ob__ob_end.seg` | — | ROM build |

Extraction is already automated: place `baserom.u.z64` (NTSC-U) in the repo root
and run `./scripts/extract_baserom.u.sh`, or `make extractassets`. Offsets come
from `imagelist.u.csv` and `scripts/filelist.u.csv`.

### So: build now, drop assets in later

**Yes — assets can come later.** Concretely:

- **Port code needs zero assets to compile or test.** Everything in this repo so
  far — the GBI interpreter, the VR math, the OpenXR layer — has been built and
  tested with no ROM present, against synthetic display lists and a fake RDRAM
  (`tests/test_gbi.cpp`). That stays true through the libultra shim and most of
  the RDP backend.
- **Assets are a runtime dependency, not a build dependency.** The port loads
  them from a data directory at startup, the way any modern port does. Nothing
  needs recompiling when you drop them in.
- **We never need `bin/gspboot.*.bin`.** That's the RSP microcode blob, and we
  are replacing the RSP with the interpreter. This is worth noting because it's
  the one missing asset that would have been genuinely awkward — it's the source
  of every UNRESOLVED item in `MICROCODE-SPEC.md`. We needed to *understand* it;
  we never need to *run* it.

The one thing to decide early rather than late is the **asset container format**
(§4, decision 3), because it determines whether the extraction step is "run the
decomp's script" or "run our own converter".

---

## 2. The other good news: the C is essentially complete

Total hand-written MIPS assembly remaining in game code:

```
src/game/chrObjRandom.s          39 lines
src/game/math_sincos.s          128 lines
src/game/sub_GAME_7F01D1C0.s     93 lines
                                ---
                                260 lines
```

That's it. Everything else under `src/game/` — all 242 translation units — is C.
The other 36 `.s` files are boot code, TLB handling, and libultra internals,
every one of which we are deleting rather than porting.

260 lines of MIPS is an afternoon of hand-translation, and two of the three are
pure math with obvious semantics. **There is no decompilation work left to do
before porting.** That is unusual and it is the reason this project is viable at
all.

---

## 3. Critical path

Ordered by dependency. Each milestone has a definition of done you can actually
check, rather than "it works".

### M0 — Baseline: build the ROM  *(needs your ROM)*

Not because we need the ROM, but because it proves the toolchain and the asset
extraction are correct before we start changing things. If the port later
renders garbage, you want to already know the assets are good.

```bash
docker compose run ge007 bash      # or the Linux deps in docs/SetupGuide.md
make                               # runs extractassets, then builds
make checksum
```

**Done when:** `ge007.u.z64` matches `abe01e4aeb033b6c0836819f549c791b26cfde83`.

*Skippable if you're impatient — M1 and M2 don't depend on it — but then an
asset problem and a code problem look identical later.*

---

### M1 — `ge-ultra`: the libultra shim  ← **the long pole** *(core done)*

> **Status.** The scheduler, message queues, timers, fibers, and flat RDRAM are
> written and tested (`src/ultra/`, `tests/test_ultra.cpp`). Semantics were taken
> from the reference implementations that ship in the decomp itself —
> `src/libultra/os/{startthread,sendmesg,recvmesg,thread}.c` — rather than from
> documentation, so the odd corners are reproduced rather than reinvented.
>
> PI/DMA, VI, SI, and the frame driver (`hostRunFrame`) are also done and
> tested, including an end-to-end scenario: a thread shaped like GoldenEye's
> main thread — block on retrace, read pads, DMA an asset, draw, swap — driven
> ten frames by the host loop.
>
> **Audio is now written too**: the 16-command audio ABI interpreter
> (`src/ultra/audio/`) and the AI shim (`src/ultra/os_ai.cpp`), tested in
> `tests/test_audio.cpp`. Same architecture as graphics — the game's own
> `alAudioFrame()` and sequence players are kept and only the RSP is replaced.
> GoldenEye uses the *stock* audio library with no ABI extensions, so unlike
> `G_TRI4` there was nothing bespoke to reverse-engineer.
>
> **`A_ADPCM` is known incorrect.** A bit-exact oracle was built from the
> instrument bank's loop-point state (`tools/extract_adpcm_vectors.py` +
> `tools/audio_validate.cpp`) and it fails 0/4. It found and fixed one real bug
> along the way — the inner convolution was feeding back decoded outputs instead
> of residuals, which made the recursion diverge to saturation while still
> *looking* smooth — but the decode is still wrong in a dimension not yet
> identified. `A_RESAMPLE`, `A_ENVMIXER` and `A_POLEF` remain unvalidated but
> are not known-wrong. See `RESUME-HERE.md` §1 for what has already been ruled
> out.
>
> Remaining in M1: fix `A_ADPCM`, validate the other three, stack high-water
> reporting.


Replace `src/libultra/` and `src/libultrare/` with a host implementation. This
is now the hardest part of the project, having overtaken the graphics work.

**Threading is the whole difficulty.** The game starts six threads (`src/init.c:170-227`,
`src/sched.c:183`, `src/audi.c:397`, `src/crash.c:326`) under N64 priority
scheduling, which was effectively deterministic — a thread ran until it blocked
or something higher-priority woke. Map that onto preemptive host threads and you
will surface races that never fired on hardware, intermittently, for weeks.

**Use fibers.** One host thread, cooperative switching at exactly the points
libultra would have switched: `osRecvMesg`, `osSendMesg` when it unblocks a
higher-priority thread, `osStopThread`, `osYieldThread`, timer expiry. This
reproduces the original scheduling rather than approximating it.

Surface to implement:

| Area | Calls | Notes |
|---|---|---|
| Threads | `osCreateThread`, `osStartThread`, `osStopThread`, `osSetThreadPri` | fibers |
| Messaging | `OSMesgQueue`, `osRecvMesg`, `osSendMesg`, `osJamMesg`, `osSetEventMesg` | the scheduling points |
| Time | `osGetTime`, `osSetTimer`, `OS_CYCLES_TO_*` | drive from the frame clock, not wall time |
| PI/DMA | `osPiStartDma`, `osPiRawReadIo` | ROM reads → file reads from the asset dir |
| VI | `osViSwapBuffer`, `osViSetMode`, retrace messages | becomes the frame signal |
| SI | `osContStartReadData`, `osContGetReadData` | fed by `synthesizePad()` (already written) |
| Memory | `osVirtualToPhysical`, `OS_K0_TO_PHYSICAL` | identity-ish over a flat RDRAM allocation |

**Delete, don't port:** `src/sched.c` (OpenXR paces us), `src/tlb_*.c`,
`src/crash.c`, `src/rmon.c`, `src/usb.c`, `src/boot.s`, `src/_start.s`.

**Critical constraint:** allocate RDRAM as one flat 8 MB block and make
`osVirtualToPhysical` return offsets into it. Game code does pointer arithmetic
on physical addresses and writes them into display lists; if physical addresses
aren't real offsets into a real buffer, the interpreter can't resolve them.

**Done when:** the game boots headless, reaches `mainproc`, ticks, and produces
non-empty display lists. Dump one and confirm it contains `G_VTX` and `0xB1`
commands. No rendering yet.

---

### M2 — `ge-rhi`: the RDP backend  *(can run in parallel with M1)*

The display-list walker (`src/gbi/`) already emits a command stream. This
consumes it.

Fork [RT64](https://github.com/rt64/rt64) and feed it from `IDrawSink` rather
than from its own GBI front end. What you get for free: combiner→shader
generation, N64 texture decode (RGBA16/32, IA4/8/16, I4/8, CI4/8 + TLUT), tile
clamp/mirror/mask/shift, blender and render-mode emulation, D3D12/Vulkan
backends.

What you still write: the `IDrawSink` → RT64 adapter, and the vertex transform
path (we deliberately don't transform on the CPU — see `gbi_interp.cpp`).

**Done when:** a captured display list from M1 renders to a window and looks
like GoldenEye. This is the moment the project becomes real.

---

### M3 — Flat-screen port

Wire M1 + M2 together, add audio and save data, translate the 260 lines of MIPS.

Audio: keep the game's synthesis structures (`src/audi.c`,
`src/libultra/audio/`) and run the audio command list on the CPU into a float
mixer. Well-trodden in other N64 ports.

**Done when:** you can play through Dam on a monitor with a gamepad.

**Ship this.** It's valuable on its own, it's the only honest way to validate
everything below the VR layer, and debugging stereo on top of an unproven port
is how people lose months.

---

### M4 — Stereo VR

Apply patches §1–§2 from `patches/DECOMP-PATCHES.md`. Wire `Session::pumpFrame`
to the game loop. Implement `IGraphicsBackend` against the M2 renderer. HUD as a
head-locked quad. Gamepad controls, head-look only.

The projection math is done and tested — `geVrBuildProjectionF` is proven to
reduce exactly to `guPerspectiveF` for a symmetric frustum, so any visual
difference from M3 is a real bug rather than a math discrepancy.

**Done when:** comfortable seated stereo, and rendering both eyes with identical
poses produces pixel-identical images (that test catches per-eye state leaks).

---

### M5 — 6DoF

Patches §3–§7: head-driven view angles, controller-driven hitscan, world-space
weapon models, recoil to haptics.

**Done when:** you can aim independently of where you're looking, and shoot a
guard by pointing at him.

---

### M6 — Polish

Diegetic HUD on the watch, dual-wield, comfort options, sniper-scope rework,
cutscene handling.

---

## 4. Decisions needed before M1

Four, and they're all cheap to make now and expensive to change later.

1. **Fibers or threads?** Recommendation: fibers. See M1.
2. **Language boundary.** The game is C compiled by a 1990s compiler. Build it
   as C with a modern compiler at `-fno-strict-aliasing` (non-negotiable —
   this code type-puns constantly) and keep the C++ confined to the shim, the
   interpreter, and the renderer.
3. **Asset container.** Loose files matching the decomp's layout (simplest, and
   the extraction script already produces it) versus a packed archive built by
   our own converter. Loose files unless there's a reason.
4. **Endianness.** The game is big-endian throughout — display lists, `Vtx`,
   asset data. Either byte-swap everything at load, or keep RDRAM big-endian and
   swap at access. **Swap at load.** Swapping at access means every single
   memory read in ported game code needs a wrapper, and you will miss some.

---

## 5. What can start right now, with no ROM

- **M1 entirely.** The shim needs no assets. Boot the game headless against
  stubbed asset loads and watch it reach the main loop.
- **M2's adapter layer.** `IDrawSink` → RT64, tested against synthetic display
  lists exactly as `tests/test_gbi.cpp` already does.
- **The 260 lines of MIPS translation.**
- **`G_CULLDL`** — currently a safe no-op (never cull). Computing outcodes for
  the bounding vertices restores it cheaply.
- **Interpreter gaps** — fog params, light colours, and viewport forwarding are
  all marked `TODO(phase2)` in `gbi_interp.cpp`.

The ROM becomes necessary at exactly one point: the first time you want to see a
real level instead of a synthetic triangle.
