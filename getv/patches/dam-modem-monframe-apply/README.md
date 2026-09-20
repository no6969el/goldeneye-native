# APPLY — GETV_VR_MONFRAME (GEVR #70 residual Dam modem flicker)

**Status:** APPLY LANDED (public tree = patch + notes only).
**Ask:** Tick `MonitorRecord` scroll/timer **once per sim frame** so both eyes reuse the same UVs.
**Default:** `GETV_VR_MONFRAME` unset / empty / `0` = **OFF** (retail per-eye tick).
**Not KEEP-ON.** Do not add to `gevr-*-boot.cmd` until a sit PASS.
**Do not** flip `TEXINVAL` / `TEXDLRETAG` / `VFXTMEM` / `VFXSHIFT` OFF.
**Do not** merge #55 (`HEAD_TRANSLATE` / EYEROOM).
**Do not** ship unlockalls.

Chair Run 0 (`HEAD_TRANSLATE=0`) was **PARTIAL PASS**: continuous under-towers flicker dropped; throw/place white flash and rare dish flash remain. This APPLY is 5.3b for that residual.

DIG: `getv/patches/dam-modem-flicker-dig/RESULT.md` (PR #5).

---

## Why this public repo is a patch, not a C edit

Playable monitor draw is **not** in public `goldeneye-native` (`vendor/ge-decomp` here is `file2.h` only). Workshop bodies stay on SimRig.

Verified: `001-propobj-monframe.patch` applies `--fuzz=0` to public `n64decomp/007` `src/game/propobj.c` (`process_monitor_animation_microcode` at ~6768).

---

## SimRig file list

Land **one file**. From workshop root
`F:\Projects\GEVR\GoldenEyeVR\goldeneye-native`:

| Path | Change |
|------|--------|
| `vendor/ge-decomp/src/game/propobj.c` | getenv + once-per-frame tick wrap |

Do **not** touch:

| Path | Why |
|------|-----|
| `getv/port/fast3d/gfx_pc.c` | TEXINVAL / TEXDLRETAG / VFXTMEM / VFXSHIFT stay KEEP-ON |
| `packaging/templates/gevr-*-boot.cmd` | no allowlist until sit PASS |
| #55 EYEROOM / `GETV_XR_HEAD_TRANSLATE` | separate ticket |

First chair grep if the helper name drifted:

```
gePortSimShouldTick
GETV_VR_ONESHOT
process_monitor_animation_microcode
```

`gePortSimShouldTick` is the existing GEVR stereo sim-owner (ONESHOT / first eye). If the live spelling differs, retarget the `extern` only. Typical home: `vendor/ge-decomp/src/game/stereo.c`.

---

## Apply

```bat
cd /d F:\Projects\GEVR\GoldenEyeVR\goldeneye-native
patch -p1 -d vendor/ge-decomp < path\to\goldeneye-native\getv\patches\dam-modem-monframe-apply\001-propobj-monframe.patch
```

Unix:

```bash
patch -p1 -d vendor/ge-decomp < getv/patches/dam-modem-monframe-apply/001-propobj-monframe.patch
```

If hunk 1 (includes) fails because workshop already has `GE_PORT_NATIVE` / `stdlib.h`, keep that include block and hand-insert the `extern int gePortSimShouldTick(void);` next to the other port decls. Hunks 2–4 are the real change (`ge_vr_monframe` + mutation wrap). Snippet: `propobj_monframe.snippet.c`.

Rebuild the VR Windows target (`getv/build_windows.ps1 -Target all -Vr` per product docs).

---

## What the C does

`process_monitor_animation_microcode` mutates `MonitorRecord` (cmdlist `offset`, scroll/scale/colour via `MONITOR_TIMER_DELTA`) **during draw**. Stereo draws twice → UV mismatch / flicker.

When `GETV_VR_MONFRAME=1`:

1. Banner **once**: `[getv][monframe] GETV_VR_MONFRAME=1`
2. First eye / `gePortSimShouldTick()` → retail tick
3. Second eye → skip `while (cmdlist)` + increment blocks; still `texSelect` + triangles with the already-ticked UVs

`PROPDEF_MONITOR` and `PROPDEF_MULTI_MONITOR` both call this function (`sub_GAME_7F04AC20`). One wrap covers Dam `PROP_MODEMBOX` and downstairs TVs.

Unset / `0` = today (per-eye tick).

---

## How to chair (after boot)

vr442 / Latest. `Start-GEVR.bat`. Recenter both sticks. Headset (say which). **Do not** set `FOVMATCH`. **Do not** wipe TEX* / VFX for this sit.

After the boot cmd has exported PLAY0 / KEEP (scratch line, not the ship allowlist):

```bat
set GETV_VR_MONFRAME=1
```

Then Dam, difficulty **007**:

1. Place the covert modem on the dish connection screen.
2. Go downstairs under the towers and look up at that dish.
3. Note throw / place white flash (Run 0 leftover — may remain).
4. Confirm console once: `[getv][monframe] GETV_VR_MONFRAME=1`

| | Tester sentence |
|--|-----------------|
| **PASS** | “After attach, from under the towers the modem picture stays **steady**. Both eyes match. Scrolling green text does **not** flash.” |
| **NOTE** | “Throw / place still flashed white once.” (known Run 0 leftover; write it, do not fail the sit on that alone.) |
| **FAIL** | “Banner is there and it still flickers every frame while I stare.” → not a stereo tick; stop this APPLY. |

Regression: explosions still coloured (TEXINVAL still ON). Dam crates / water as vr442. No full-screen black when **not** looking at the dish. Unset `GETV_VR_MONFRAME` restores retail per-eye tick.

---

## Files in this folder

| File | Role |
|------|------|
| `001-propobj-monframe.patch` | Unified diff vs n64decomp-style `src/game/propobj.c` |
| `propobj_monframe.snippet.c` | Hand-apply if workshop drift rejects hunks |
| `README.md` | This page |
