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

## 6. Open questions that gate real work

**World scale is settled.** `bondview.c:1507` gives
`eyeheight = player_perspective_height * 185.0f - 10.0f`, so a standing Bond's
eye is ~175 units. A human eye is ~1.75 m. **100 units per metre**, which is what
`GE_VR_UNITS_PER_METRE` already says — derived independently, now corroborated by
the game's own constant. Getting this wrong is the classic VR-port failure and it
is one of the few things here that is *not* in doubt.

**The world's near and far planes are NOT settled, and a previous note about them
was probably wrong.** `tools/room_render.cpp` says the game clips at
`znear = 10 / zfar = 300`, citing `bondview2.c`. The `guPerspective` call at that
site is inside `bondviewRenderWatch` — the *watch menu*, which renders an object
held at arm's length, where a 3 m far plane is entirely reasonable. The **world**
projection is reached through `currentPlayerGetProjectionMatrix()` and is built
somewhere this survey has not yet located.

This matters twice over: a 3 m far plane would end the world well inside a 5.76 m
room, and the near and far planes are direct inputs to the stereo frustum. Settle
it by finding the caller of `currentPlayerSetProjectionMatrixF()` before Phase 1.

It is the same mistake as `0xB2` in a different costume — a value read from one
call site and generalised to a path it was never on. Twice in one day is a
pattern, and the rule that falls out is: **a constant is only established for the
code path it was read from.**

**Unverified:** N64ModernRuntime's licence. RT64 and N64Recomp are both confirmed
MIT; the third component of the stack has not been checked, and it sits under
every option in `PRIOR-ART.md` §4.3.
