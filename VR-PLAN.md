# VR-PLAN.md

The 6DoF plan, revised after `PRIOR-ART.md` established that working GoldenEye
PC ports already exist and all of them are static recompilation.

Revised 2026-08-21.

---

## 1. What changed

The old plan was linear: finish the decomp-native port, give it a renderer, then
add VR. That plan assumed nothing else existed. Something else does, it renders
today, and its renderer is MIT.

The new plan separates two things the old one conflated:

|  | Old plan | New plan |
|---|---|---|
| **Host** — what runs the game | `goldeneye-native` only | either; prove it against the recomp first, because that one renders |
| **VR layer** — what makes it stereo | patched into the decomp C | a host-agnostic library the host calls |

The VR layer was already written that way, by accident of good hygiene:
`include/ge_vr/ge_vr.h` is plain C89 with no OpenXR, no C++, and no reference to
this port's host layer. It talks about **GoldenEye's** conventions — `theta` and
`verta`, `perspNorm`, 100 units per metre — and every GoldenEye host has those,
because it is the same game.

## 2. The finding that makes this concrete

A VR layer needs to override the camera. In GoldenEye the world camera goes
through **four one-line accessors** on `g_CurrentPlayer`:

```c
/* src/game/bondview.c */
Mtx  *currentPlayerGetProjectionMatrix(void)  { return g_CurrentPlayer->projmatrix;  }
Mtxf *currentPlayerGetProjectionMatrixF(void) { return g_CurrentPlayer->projmatrixf; }
Mtxf *camGetWorldToScreenMtxf(void)           { return g_CurrentPlayer->field_10CC;  }
void  currentPlayerSetMatrix10CC(Mtxf *m)     { ... }
```

`bg.c` draws the world through them (`:692`, `:729`, and six more sites),
`sky.c:811` composes with them, `bondview2.c:8244` reads them. They are named,
non-static, and one line each.

**That is the whole attachment surface for stereo.** Not a diffuse rewrite of the
renderer — four symbols. On a recompilation host those are exactly what a
symbol-level hook replaces; on `goldeneye-native` they are four functions to
edit. The same VR library serves both.

## 3. What carries over, honestly

| | Lines | Carries to a recomp host? |
|---|---|---|
| VR layer (`ge_vr.h`, `xr_math`, `xr_session`, `xr_input`, bridge, tests) | 1,913 | **Yes, whole.** Zero coupling to this port. |
| GBI interpreter + vertex pipeline | 967 | Partly — RT64 does this, *except* where RT64 is wrong (see §5). |
| libultra shim, cartridge byte-order and layout conversion, `hostcompat/` | 9,910 | **No.** A recomp keeps the N64's memory layout, so this entire class of work does not arise. |
| `patches/decomp-host-port.patch` | 11,217 | **No.** |

The honest headline: **the VR work carries over whole; most of the host-port work
does not.** That is worth stating plainly rather than softening. It is not the
same as wasted — see §5 — but nobody should plan on the basis that 20,000 lines
of host layer are transferable, because they are not.

## 4. The plan

Ordered so that the cheapest thing that can invalidate the plan comes first.

**Phase 0 — bind test. Days.**
Hook `currentPlayerGetProjectionMatrix()` on a recomp host and return a matrix
with a deliberately wrong field of view. If the world's FOV changes, the hook
model works and everything below is engineering. If it does not, the plan is
wrong and we have spent days, not months, finding out.

**Phase 1 — stereo.** Render the world pass twice, once per eye, with per-eye
projection from `geVrBuildProjectionF` and per-eye view offset from
`geVrGetEyeViewOffsetF`. `geVrCurrentEye()` is already in the ABI for this.

**Phase 2 — the OpenXR frame loop takes over pacing.** `xr_session.cpp` already
owns a frame loop; the host's own loop becomes a tick driven by it.

**Phase 3 — decouple aim from view.** Head looks, controller aims. This is the
part that needs game state rather than matrices: Bond's `theta`/`verta` and the
hitscan origin. `geVrGetAimRay` and `geVrGetWeaponDisplacement` exist for it.

**Phase 4 — comfort and input.** Snap turn, height calibration, physical crouch,
`OSContPad` synthesis. All already declared in `ge_vr.h`.

`goldeneye-native` continues in parallel, but as the reference implementation and
the place microcode knowledge is produced — not as the critical path to a
headset.

## 5. What we hold that the recomp path needs

Not a large list, but a real one, and it is the reason to keep both alive.

- **`G_RDPHALF_CONT` (`0xB2`).** Stock F3D; F3DEX2 reassigned it; RT64 targets
  F3DEX2; `sky.c` emits it constantly. A candidate explanation for the black
  skyboxes `cblock85/GoldenEye64Recomp` reports. See `MICROCODE-SPEC.md` §2.1.
- The rest of the F3D + `G_TRI4` map, ROM-validated over 1,937 room lists.
- A catalogue of 33 bug families (`patches/HOST-PORT-PATCHES.md`) — most are
  decomp-specific, but the endianness and layout reasoning applies to anyone
  reading cartridge data.

## 6. Questions that gated real work — now settled

Settled 2026-08-21 against `n64decomp/007` @ HEAD. Every claim below cites the
call site it was read from, per the rule in §6.4.

### 6.1 World near/far planes — SETTLED, and the old note was wrong

The world projection is not built where the survey was looking. The chain is:

```
bgfog.c:301   viSetZRange(env->Visibility.BlendMultiplier, env->Visibility.FarFog)
fr.c:929      g_ViBackData->znear = near;  g_ViBackData->zfar = far;
fr.c:709      guPerspectiveF(g_viProjectionMatrixF, &g_viPerspNorm,
                             g_ViBackData->fovy, g_ViBackData->aspect,
                             g_ViBackData->znear, g_ViBackData->zfar, 1.0f)
fr.c:720-721  currentPlayerSetProjectionMatrix{,F}(g_viProjectionMatrix{,F})
bg.c:1258     gSPMatrix(..., g_viProjectionMatrix, G_MTX_LOAD | G_MTX_PROJECTION)
```

`fr.c` is the only caller of `currentPlayerSetProjectionMatrixF()`. So:

- **The near and far planes are per-level data, not constants.** They come from
  `fog_tables[]` (`bgfog.c:119`/`:174`), keyed by level id and player count.
- **`znear` is `Visibility.BlendMultiplier`** — a fog parameter reused as the
  near plane. Observed range across the table: **2 to 30 units** (Surface and
  Surface 2 use 2; Statue uses 15; Dam's cinema entry uses 30).
- **`zfar` is `Visibility.FarFog`.** Observed range: **1000 to 20000 units**
  (Facility-alt 1000, Train 1500, Citadel and Egypt 20000).
- **Fallback for fogless levels is `viSetZRange(15.0f, 10000.0f)`**
  (`bgfog.c:480`).
- The menu path is separate and uses `viSetZRange(100.0f, 10000.0f)`
  (`front.c`, eight sites).

**`10 / 300` was never the world.** That pair is `bondview2.c:8423`, inside the
zoom/scope path, exactly as suspected. `tools/room_render.cpp` should be
corrected. The plausible world default for offline tooling is `15 / 10000`.

**Consequence for Phase 1.** `geVrBuildProjectionF(mf, &pn, znear, zfar, scale)`
already takes near and far as arguments, which turns out to be the right shape:
the host passes `g_ViBackData->znear` and `->zfar` straight through. Two caveats:

1. A 2-unit near plane is 2 cm. Harmless on the N64's W-buffer; in stereo it
   wastes precision and puts the near plane inside the headset's own comfort
   floor. **Clamp `znear` to a VR floor (~10 units / 10 cm) rather than passing
   the game's value verbatim.** This is a deliberate divergence and belongs in
   the VR layer, not the host.
2. `zfar` is per-level and changes on environment transitions
   (`fogLoadCurrentEnvironment`). The stereo frustum must be rebuilt when it
   does, not cached at level load.

### 6.2 A second finding the survey did not go looking for: per-level world scale

`bg.c:841-844`, on level load:

```c
mCurrentLevelVisibilityScale = levelinfotable[levelentry_index].visibility;
matrix_4x4_7F058C4C(mCurrentLevelVisibilityScale);   /* matrixmath.c:476 */
```

which sets `D_80032310[0] = 65536.0f * visibility`, the multiplier
`matrix_4x4_f32_to_s32()` (`matrixmath.c:495`) uses when converting every `Mtxf`
to the fixed-point `Mtx` the RSP consumes. It hits the 3x3 and the translation
row; only the fourth column keeps the unscaled 65536.

`visibility` is `1.0` for every level **except Dam, Surface and Surface 2, where
it is `0.2`** (`bg.c:196`, `:199`, `:206`). Those three render the world at
one-fifth scale so the far plane and depth range reach across an outdoor map.
`bgfog.c:304` corroborates: it divides the z-range by this scale to recover
game-unit distances for the fog maths.

This is a uniform scale on the modelview, so it does not change the image and it
does not change `GE_VR_UNITS_PER_METRE` — **100 units per metre remains correct
in game space.** What it changes is *where the VR hook may legally sit*:

> The per-eye view offset must be composed into the **`Mtxf`**, above
> `matrix_4x4_f32_to_s32()`. Compose it into the fixed-point `Mtx` and the eye
> separation is not scaled with the world, so Dam, Surface and Surface 2 get an
> IPD five times too large while every other level looks right.

That is a bug that would ship, reproduce on three levels only, and read as
"something's wrong with Dam". Writing it down now is the cheapest it will ever
be. It also argues for hooking `currentPlayerGetProjectionMatrixF()` and the
`Mtxf` accessors rather than their `Mtx` counterparts, wherever there is a
choice.

### 6.3 N64ModernRuntime licence — SETTLED, and it is the bad answer

**GPL-3.0** (`COPYING`, verbatim GPLv3, at `N64Recomp/N64ModernRuntime`).
All three components were checked directly, not taken second-hand:

| Component | Licence | Linked into shipped host? |
|---|---|---|
| `rt64/rt64` | MIT | yes |
| `N64Recomp/N64Recomp` | MIT | no — build-time tool |
| `N64ModernRuntime` (`librecomp`, `ultramodern`) | **GPL-3.0** | **yes** |

`N64ModernRuntime` has one top-level `COPYING` and no per-subtree grant, so the
whole runtime is covered. It is linked into the shipped host, so **any recomp
host built on this stack is GPL-3.0 as distributed** — and `N64Recomp` being MIT
does not change that, because a recompiler's licence does not travel into what it
recompiles. The consequences worth stating plainly:

- The MIT VR layer can be linked into a GPL-3.0 host without difficulty — MIT is
  GPL-compatible in that direction. `include/ge_vr/` stays MIT and stays
  reusable elsewhere.
- The *combined binary* must be distributed under GPL-3.0, with source. That is
  a decision about this project's licensing, not a technical blocker, and it
  wants a deliberate answer before Phase 0 rather than after Phase 3.
- It is one more reason `goldeneye-native` is worth keeping alive: it is the
  path that is not encumbered.

### 6.4 The rule, restated

**A constant is only established for the code path it was read from.** `0xB2`
and `10 / 300` were the same mistake twice. §6.1 above is the third instance
caught — `10 / 300` really was the scope path — and the near/far values are now
recorded *with the level table they come from*, not as a pair of numbers.

## 7. What to do next

1. **Correct `tools/room_render.cpp`** to `15 / 10000`, or better, to the
   `fog_tables[]` entry for the level being rendered. Cheap, and it makes the
   offline renderer agree with the game.
2. **Clamp `znear` in the VR layer** and document the divergence in
   `ge_vr.h`.
3. **Decide the licence question** in §6.3 before spending days on Phase 0.
4. **Then Phase 0.** The harness is written and self-testing: `phase0/`.
   Standalone C89, links no `ge_vr` code, builds without OpenXR or a ROM.

   Two design points that came out of writing it, both worth carrying forward:

   - **It mutates the matrix in place rather than returning one.** The
     accessors' return value reaches `gSPMatrix(gdl++, osVirtualToPhysical(m))`,
     so a pointer into host memory truncates — the fiber-stack bug in
     `RESUME-HERE.md`, which would present as a corrupt frame and read as "the
     hook does not work". In-place mutation sidesteps address translation and
     behaves identically on both host types.
   - **It scales `[0][0]` and `[1][1]` only**, recovering fovy from `[1][1]`.
     Near, far, the W row and `perspNorm` are never touched, so the test cannot
     render black — and, usefully, it never needs to know the per-level near and
     far that §6.1 just established.

   It also counts calls even when the perturbation is disabled, which splits
   "no change" into two verdicts: the hook never ran (attachment problem), or
   the hook ran and the world projection reaches the RSP by another path
   (**the plan is wrong**). Without the counter those are indistinguishable, and
   only the second one falsifies VR-PLAN §2.

   Run `phase0/ge_bind_test_selftest` before trusting any host result.

5. **The host is `cblock85/GoldenEye64Recomp`, and Phase 0 is largely already
   answered by it.** Verified against that repo directly:

   - All four accessors survive recompilation as **named** symbols with
     addresses and sizes in `dump.toml` — `currentPlayerGetProjectionMatrix` at
     `0x80478374`, and so on. The central bet of §2 holds at the symbol level.
   - So do both setters, which are the better seam for a bind test: they are
     called once per frame from `fr.c:720-721`, where the getters are called
     eight-plus times and would compound an in-place perturbation into a black
     frame.
   - `RECOMP_PATCH` is a working, shipped mechanism there (`patches/skybox.c`
     already replaces `skyRenderTri`).
   - **`patches/workbench_theboy.c` already alters the world projection**,
     patching `viSetFovY` and `viSetAspect` to add widescreen. Changing this
     game's world projection through this chain is not a hypothesis on that
     host; it ships.

   A drop-in patch is written and **compiles under that project's own MIPS
   toolchain**, emitting both functions into `.recomp_patch`:
   `phase0/adapters/ge64recomp/`. Its `patches/Makefile` globs `*.c`, so
   installing it is one `cp`.

   Note for the record: `SunJaycy/GoldenEye-Recomp` is **not** a candidate host.
   It is the Xbox 360 remaster under XenonRecomp — PowerPC, `default.xex`, no C
   symbols. PRIOR-ART §3.2 said so; this was re-confirmed the hard way.
