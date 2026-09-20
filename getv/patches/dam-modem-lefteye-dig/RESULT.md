# RESULT — #70 leftover left-eye modem flicker (DIG ONLY)

**Status:** DIG. **Not APPLY READY.** No C landed.
**Ask:** After HT0 + `SKYMESH=0` sits, covert-modem flicker is **less overall**; residual is **very slight LEFT-EYE only** texture flicker while **standing near the modem**. Mechanism + smallest default-OFF falsifier. Rank park vs chair vs later APPLY.
**Ship stays:** `GETV_XR_HEAD_TRANSLATE=0` (vr442 boot). `SKYMESH=0` is the **#72** KEEP (do not merge).
**Do not** re-sit `GETV_VR_MONFRAME`. **Do not** Dam `GETV_STEREO_MTXGUARD=2`. **Do not** restore HT=1.
**Date:** 2026-09-20.
**Evidence:** GEVR #70 night sit, public `n64decomp/007` `propobj.c` monitor/embed/draw, GEVR `docs/292` eye loop, this repo KEEP stubs + prior DIG PRs #5 / #6 / #27 / #8. Workshop `gfx_pc.c` TEXINVAL **body** and `stereo.c` eye loop stay private.

Director can **park this leftover**, green one cheap chair, or demand a later scoped APPLY from this page alone.

Prior:

| PR | Page | What it closed |
|----|------|----------------|
| #5 | `getv/patches/dam-modem-flicker-dig/RESULT.md` | Downstairs frustum + per-eye `MonitorRecord` tick |
| #6 | `getv/patches/dam-modem-monframe-apply/` | `MONFRAME` APPLY — **chair REJECT** (white verts). Leave **OFF**. |
| #27 | `getv/patches/dam-modem-residual-dig/RESULT.md` | PARK HT0. Named attach-white / `PROP_CHRBUG` near-field. **That path does not explain left-only while standing near.** |
| #8 | `getv/patches/dam-bridge-blue-flash-dig/RESULT.md` | #72 jump-strip blue. Chair **PASS** `SKYMESH=0`. Separate. |

---

## Verdict (one screen)

| Question | Answer |
|----------|--------|
| Why left-eye only? | **First-eye (LEFT = `GE_VR_EYE_LEFT` = 0) is the cold / mutated pass** of a shared monitor `texSelect` + `MonitorRecord`. Right eye (1) draws second, TMEM/cache already warm, UV already advanced. MTXGUARD-class convert hits **eye 1 (right)** — the wrong eye for this leftover. |
| Same as PR27 CHRBUG attach flash? | **No as the standing-near leftover.** Embed bake is a **stable** IPD offset; if baked on sim/first eye, the **right** eye is the mismatch. Attach white can stay a known pop. This sit is **texture crawl on the screen** after you are already there. |
| Why did `SKYMESH=0` make modem flicker *less*? | Sky remesh is extra vtx/TMEM on the same `gfx_pc` path as `texSelect`. Turning it off (for #72) cut contention. Residual is the **monitor's own** first-eye load of `IMGTEXT`. Do **not** treat SKYMESH as a #70 knob. |
| Smallest falsifier, default OFF | **Existing** `GETV_VR_TEXINVAL=0` (KEEP stays ON). Then `TEXDLRETAG=0`. Optional later C: `GETV_VR_MONINVAL=1` — skip inval **only** on monitor `texSelect` (`arg2` 1/2, `arg3` 8). |
| Park vs chair vs APPLY | **Park forever is honest** for a very slight leftover. One cheap chair is worth it if Director wants #70 closed. APPLY only after that sit PASSes without purple explosions. |
| APPLY tonight? | **No.** |

```
sim once, draw twice          geVrCurrentEye LEFT=0 then RIGHT=1
PROP_MODEMBOX ONSCREEN        process_monitor_animation_microcode  (opaque, flags&1)
  shared MonitorRecord        UV += MONITOR_TIMER_DELTA per EYE DRAW
  texSelect(IMGTEXT, 1, 8, 2) TEXINVAL KEEP on first (left) load
  dynAllocate 4 verts         first-eye payload is the one a second pass can stomp
SKYMESH=0 (#72 KEEP)          less TMEM/vtx → leftover shrinks, does not vanish
CHRBUG child                  forced ONSCREEN; embed bake ≠ left-only flicker
MONFRAME=1                    skip whole cmdlist incl. SETTEXTURE on “sim tick”
                              gePortSimShouldTick is 0 during DRAW → WHITE  REJECT
MTXGUARD=2                    skip 2nd f32→s16.16 — hits eye=1 RIGHT; Dam BLUE  FORBIDDEN
```

---

## 1. What chair already proved (do not re-break)

| Sit | Result | Keep |
|-----|--------|------|
| `HEAD_TRANSLATE=0` | Downstairs under-towers strobe **mostly gone**. Best ship for original #70. | **PARK / vr442 boot.** Do not restore `=1`. |
| `MONFRAME=1` | **REJECT** — worse (white verts / IMGBOND × 0xFF). | Never boot, never KEEP, **do not re-sit.** |
| `MTXGUARD=2` on Dam | Tunnel **blue**. Facility/Bunker bats only. | **Forbidden** on Dam. |
| `SKYMESH=0` | #72 jump-strip blue **PASS**. Owner: modem flickers **less overall**. | **#72 KEEP.** Do not merge into this leftover. Do not restore `=1` to “test modem.” |
| TEXINVAL / TEXDLRETAG / VFX `=0` | **Still unrun after HT0** (PR #5 named it; PR #27 deferred). | KEEP stays ON. This leftover’s first A/B. |

Owner (2026-09-19 night, during #72 sit): *covert modem flickers less; residual is very slight **left-eye** flicker only — parked as minor.*

---

## 2. Why left-eye only (source)

### 2.1 Eye order is left first

Public ABI (`include/ge_vr/ge_vr.h`): `GE_VR_EYE_LEFT = 0`, `RIGHT = 1`. Frame contract (`src/xr_session.h`): **simulate once, render twice.** GEVR `docs/292`: `geStereoIsFirstEye() && gePortSimShouldTick()` gates **`propsTick` / sim**, not prop **draw**. Watch-highlight #58 used the same fact: draw runs per eye; first eye is left.

So “left-only” = **something that is different on the first pass**, or something the **second pass does to shared state the first pass already recorded**.

### 2.2 Monitor draw mutates shared state during each eye’s opaque pass

`propobj.c` `chrobjRenderProp` → `sub_GAME_7F04AC20`:

- Full-alpha: `mrData.flags = (arg2==0) ? 1 : 2`.
- `process_monitor_animation_microcode` runs only if `mrData->flags & 1` → **opaque only**, once per eye, not on xlu.
- Dam tag 5 `PROP_MODEMBOX` is `PROPFLAG_FIXED_MONITOR` → `mN = 8`.
- Ends in `texSelect(&gdl, tconfig, arg5=1, arg4=8, ulst=2)` plus `gSPMatrix(model->render_pos)` and four verts.

Inside the microcode, **before** verts/`texSelect`:

- Walk `MonitorRecord.cmdlist` (`TVCMD_SETTEXTURE` / `MONVERTSCROLL` / colour).
- Advance `xmid`/`ymid`/scale/colour by `MONITOR_TIMER_DELTA` (`g_GlobalTimerDelta`).

`MonitorRecord` is **not** per-eye. Stereo therefore:

| Eye | What it does |
|-----|----------------|
| 0 LEFT | Tick UV, `texSelect` (cold), record DL + 4 verts |
| 1 RIGHT | Tick UV **again**, `texSelect` (warm), record DL + 4 verts |

That is **double scroll** and **one scroll-step of stereo mismatch** every frame. Both eyes have a picture. It reads as **texture crawl**. Owners often notice it in the left because that is the first (and, with TEXINVAL, the **reload**) pass.

`monAnim05GreenTextUp` is `MONUSEIMAGE(IMGTEXT)` + looping `MONVERTSCROLL` — a large near quad makes a one-step UV / one-frame TMEM miss **obvious**.

### 2.3 Why first-eye TEXINVAL fits left-only

KEEP `GETV_VR_TEXINVAL` / `TEXDLRETAG` / `VFXTMEM` are **already ON** during the leftover. Inventory labels them explosion/fire. Monitor is a **different** `texSelect` mode (`arg2` 1/2, `arg3` 8 vs explosion `arg2==4`), same `gfx_pc` neighborhood.

Workshop body is private. The **shape** that produces left-only:

1. Left `texSelect(IMGTEXT)` → inval / retag / TMEM reload → one-frame wrong or hitch on the quad.
2. Right `texSelect` same image → cache already rebuilt → **steady**.

`SKYMESH=0` helping the **modem** is the same neighborhood: sky remesh (`gfx_sky_rdp_tri`) is extra first-eye TMEM/vtx. Less of that → less left reload noise. Residual = monitor load itself.

`GETV_XR_PLAY_SRCFBO=1` + `GETV_SUPERSAMPLE=3` KEEP: if a **shared** src FBO is overwritten by eye 1 before eye 0 is presented, that is also **left** corruption. Chair-cheap diagnostic only (`SS=1`, restore 3). Do not ship SS off.

### 2.4 One-view pools (292) — first eye is the one that gets stomped

`docs/292` §1: Gfx + vertex pools are sized by `getPlayerCount()` (one view). Eye is not a player. Second pass reuses the same `dynAllocate` heaps. `process_monitor_animation_microcode` **`dynAllocateVertices(4)` every eye**. Sky remesh is a big consumer.

If the second pass reuses the heap **before** the first eye’s GPU work is done, **left** UVs/verts are the ones overwritten. `SKYMESH=0` = less heap traffic = leftover shrinks. `GETV_VR_VTXGUARD=64` KEEP already ships; do **not** sit `=0`.

### 2.5 Forced `ONSCREEN` / embed bake — not this leftover

`sub_GAME_7F0442DC`: embedded child **`prop->flags |= PROPFLAG_ONSCREEN`** (no frustum). `render_pos = parentNode * embedment`.

`objEmbed` bakes with `currentPlayerGetViewToWorldMtxf()` at stick time (sim / first eye under HT0 = body translation + left rotation). The **other** eye then draws a stable offset. That is **right-eye** mismatch if it is visible at all — **not** left-only flicker while standing there.

Standing near: both eyes have the dish in frustum anyway. Parent `PROPFLAG_ONSCREEN` is not toggling per-eye at this range.

PR27 `BUGHIDE` / `BUGEMBED` target **attach white** / in-flight mesh. Do not APPLY them for this sit.

### 2.6 In-place matrix convert — wrong eye, forbidden on Dam

`sub_GAME_7F04AC20` **after** children, **if `arg2` (xlu)**: `bondviewTransformManyPosToViewMatrix` f32→s16.16 **in place**.

`docs/292` S2d: **every** `already-converted` hit was **`eye=1`**. Mode 2 skip “both eyes showed better” and later **Dam tunnel blue**. Facility/Bunker may wear `MTXGUARD=2`. **Not Dam. Not left-only.**

Monitor `gSPMatrix(render_pos)` is on the **opaque** pass, **before** that convert. A second-eye saturated parent mtx would ugliness the **right** eye / xlu child, not a slight left texture crawl.

### 2.7 Hypothesis table (this leftover only)

| # | Hypothesis | Result |
|---|------------|--------|
| L1 | First-eye `texSelect` + TEXINVAL/TEXDLRETAG/TMEM; right is warm | **Best fit** for left-only + SKYMESH=0 shrink. Chair-cheap. |
| L2 | Shared `MonitorRecord` UV tick per eye (stereo crawl) | **Source-proven**, both eyes have a picture. Can be *called* left-only. MONFRAME tried this class with the **wrong gate** and went white. Do not re-sit MONFRAME. A *later* scroll-only wrap would use `geVrCurrentEye()==LEFT`, not `gePortSimShouldTick()`. |
| L3 | One-view dyn pool / SrcFbo stomp of first-eye verts | **Open, same left-eye sign.** SKYMESH=0 is circumstantial support. SS=1 diagnostic. |
| L4 | PR27 `PROP_CHRBUG` embed / near-field first frames | **Falsified as this leftover.** Stable bake; wrong eye; attach-timed, not standing-near texture. |
| L5 | Per-eye `ONSCREEN` drop | **Weak** at dish range; child forced on. |
| L6 | MTXGUARD / second convert | **Falsified for left-only.** Hits eye 1. Dam `=2` forbidden. |
| L7 | #72 sky fill in the left eye | **Separate.** Residual is **texture** on the modem, not flat RGB(16,48,96). SKYMESH=0 already PASS for blue. |

---

## 3. Smallest falsifier (default OFF)

**Do not land, do not KEEP, do not Dam-boot.**

### 3.1 Do not run

| Knob | Why |
|------|-----|
| `GETV_VR_MONFRAME=1` | Chair REJECT. Wrapped **whole cmdlist** including `TVCMD_SETTEXTURE`. Gated `gePortSimShouldTick()` **during draw** (sim already finished) → both eyes skip → `tconfig=0` IMGBOND × white verts. **Not a retune.** |
| `GETV_STEREO_MTXGUARD=2` | Dam blue. Bunker/Facility bats only. |
| `GETV_XR_HEAD_TRANSLATE=1` | Reopens downstairs #70 / #55 lean. |
| `GETV_VR_SKYMESH=1` | Restores #72 blue. Not a #70 probe. |
| `GETV_STEREO_REBUILD=0` | Breaks fusion. Last-resort only, not this leftover. |
| C-default OFF TEXINVAL / VFX / TEX16BE | Purple explosions (vr440 / #51). |
| `GETV_STEREO=0` | Owner F0 FAIL on watch #58; unreadable. |

### 3.2 Chair first (no rebuild) — one change, restore after

vr442 / Latest. Boot already `HEAD_TRANSLATE=0` and `SKYMESH=0`. Dam **007**. Recenter. Stand **on the dish**, look at the green-text screen. No MONFRAME. No MTXGUARD. No FOVMATCH.

| Run | Env | If **left** flicker **dies** | If it **stays** |
|-----|-----|------------------------------|-----------------|
| **L0** | unset (ship) | — | Confirm: close **right** eye, left still crawls; close **left**, right is **steady**. If both crawl equally, this is L2 (UV tick), not L1. If already gone, **park / close leftover**. |
| **T** | `GETV_VR_TEXINVAL=0` | First-eye inval. **Do not** ship OFF. Check explosions still vr442-coloured. Then APPLY sketch `MONINVAL`. | Restore. Not TEXINVAL. |
| **R** | `GETV_VR_TEXDLRETAG=0` | DL retag. Same: scoped skip, KEEP ON. | Restore. |
| **V** | `GETV_VR_VFXTMEM=0` then `VFXSHIFT=0` | VFX TMEM. Scoped skip. Fire/sparks must stay. | Restore. |
| **S** | `GETV_SUPERSAMPLE=1` (restore **3**) | Shared SrcFbo / first-eye present. **Do not** ship SS1. Note it; no C tonight. | Restore 3. Not FBO. |

If **T/R/V/S all FAIL** and L0 was truly left-only: leftover is L2 UV tick. **Park** unless Director later greens a **new** scroll-only knob (`GETV_VR_MONSCROLL`, §4.2). That is **not** MONFRAME.

### 3.3 Smallest later C (workshop, after T PASS, still default OFF)

**`GETV_VR_MONINVAL`** — skip TEXINVAL / TEXDLRETAG when `texSelect` is the monitor path (`arg2` 1 or 2, `arg3` 8). Explosion mode 4 unchanged. KEEP TEXINVAL **ON**.

```
GETV_VR_MONINVAL   unset / empty / 0 = OFF   (retail: inval every texSelect)
                   1 = skip inval/retag on FIXED_MONITOR texSelect only
Banner once: [getv][moninval] GETV_VR_MONINVAL=1
```

Site: workshop `getv/port/fast3d/gfx_pc.c` next to `ge_vr_texinval()`. **Not** `propobj.c`. **Not** cmdlist wrap.

### 3.4 Not this leftover (named so they stay parked)

| Sketch (PR27) | Why not now |
|---------------|-------------|
| `GETV_VR_BUGHIDE` | In-flight `PROP_CHRBUG`. Attach white, both-eyes pop. |
| `GETV_VR_BUGEMBED` | Sim-owner embed bake. Stable stereo, wrong eye. |
| Monitor-only MTXGUARD skip | Dam-blue family on that object. Do not start here. |

### 3.5 If Director later wants UV-once (not MONFRAME)

Different wrap than PR #6:

- Gate **`geVrCurrentEye() == GE_VR_EYE_LEFT`** (draw-time), **not** `gePortSimShouldTick()`.
- Skip **scroll/scale/colour increments** on eye 1 only.
- **Always** run `TVCMD_SETTEXTURE` and `texSelect` (that is what MONFRAME dropped).

New name `GETV_VR_MONSCROLL`, default OFF. **Do not** reuse `GETV_VR_MONFRAME`. **Do not** chair until T/R FAIL and L0 shows **both** eyes crawling.

---

## 4. Chair stare — plain tester sentences

**Setup:** vr442 Latest. `Start-GEVR.bat`. Boot already HT=0 and SKYMESH=0. Dam 007. Headset name + OpenXR. No MONFRAME. No MTXGUARD=2. No FOVMATCH.

**Ship still holding:**

| | Tester sentence |
|--|-----------------|
| **S-PASS** | “From downstairs under the towers, looking up at the dish, it stays **steady**. No every-frame strobe.” |
| **B-NOTE** | “On the jump-strip, turning around, **no blue flash**.” (#72; write it, do not fail this leftover on it.) |

**This leftover (on the dish, after attach):**

| | Tester sentence |
|--|-----------------|
| **L0-LEFT** | “I cover my **right** eye. The green text **crawls / flickers a little**.” |
| **L0-RIGHT** | “I cover my **left** eye. The green text stays **steady**.” |
| **L0-BOTH** | “Both eyes crawl the same.” → L2; do not start TEXINVAL; park or later MONSCROLL. |
| **L0-GONE** | “Standing on the dish, **no** flicker either eye.” → leftover already gone; park / close. |
| **T-PASS** | `TEXINVAL=0`: “Left eye is now **steady**. Explosions still look like Latest (not purple).” |
| **T-COST** | “Left steady but explosions went purple / died.” → do **not** ship TEXINVAL=0; greens `MONINVAL` after APPLY. |
| **T-FAIL** | “Left still crawls with TEXINVAL=0.” Restore. Try R, then V, then S. |

**Regression:** downstairs S-PASS still holds. Explosions coloured. Dam crates / water as vr442. Unset scratch knobs restore Latest.

---

## 5. Rank (the ask)

| Rank | Move | When |
|------|------|------|
| **1. Park forever** | **Recommended default.** Leftover is very slight, owner already parked for focus. HT0 + SKYMESH=0 are the ship wears. No new Dam boot line. Leave #70 **open** with this page, or close as residual-known. | Always legal. |
| **2. Chair knob only** | One sitting: L0 split, then `TEXINVAL=0`. No C. | If Director wants #70 closed rather than parked. |
| **3. Later APPLY** | Workshop `GETV_VR_MONINVAL=1` default OFF, **after** T-PASS (or T-COST). | Only if chair proves first-eye inval and KEEP TEXINVAL must stay ON. |
| **4. Later UV-once** | New `MONSCROLL`, not MONFRAME. | Only if L0-BOTH / T–S FAIL and leftover is still worth C. |

Park does **not** unblock MONFRAME or Dam MTXGUARD=2.

---

## 6. Director decision

| If you say… | Then… |
|-------------|--------|
| **Park leftover** | **Recommended.** Ship HT0 + SKYMESH=0. No C. #70 can stay open as left-residual-known. |
| **Green chair only** | Sit L0 then T (then R/V/S if T-FAIL). No C. |
| **Green `MONINVAL` if T-PASS / T-COST** | Workshop skip inval on monitor `texSelect` only. Default OFF. KEEP TEXINVAL ON. |
| **Green MONFRAME again / retune** | **Rejected.** Wrong gate, white verts. |
| **Green MTXGUARD=2 on Dam** | **Rejected.** #72 / tunnel blue. |
| **Green BUGHIDE / BUGEMBED for this leftover** | **Rejected.** Wrong leftover (PR27 attach). |
| **Merge #72 / restore SKYMESH=1** | **Rejected.** |
| **Flip TEXINVAL C-default OFF** | **Rejected.** |
| **APPLY tonight from this repo** | **No.** DIG only. Public tree has no workshop `gfx_pc.c` body. |

---

## 7. Attribution / legal

- No personal credit paths edited.
- No GoldenEye ROM, assets, or dumps.
- Decomp citations are public `n64decomp/007` (`propobj.c` `process_monitor_animation_microcode` / `objEmbed` / `sub_GAME_7F0442DC` / `sub_GAME_7F04AC20` / `chrobjRenderProp` / `texSelect` in `othermodemicrocode.c` / `initobjects.c` `g_InitialMonitorAnimController`).
- GEVR textbook: `docs/292-THE-EYE-LOOP-IS-WRITTEN-AND-BOTH-POOLS-ARE-SIZED-FOR-ONE-VIEW.md` (eye 0 first, `already-converted` on eye 1, one-view pools).
- Workshop C stays private until release policy flips.
