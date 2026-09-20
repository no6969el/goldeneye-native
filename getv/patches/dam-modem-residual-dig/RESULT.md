# RESULT — #70 residual Dam modem flash after HT0 (DIG ONLY)

**Status:** DIG. **Not APPLY READY.** No C landed.
**Ask:** After `GETV_XR_HEAD_TRANSLATE=0` shipped, owner still sees a **quick white flash on attach**, then mostly gone; **residual when on/at the modem**. Continuous under-towers flicker is the solved half.
**Ship:** **PARK on HT0.** That is the best ship for the original #70 report.
**Do not** re-propose `GETV_VR_MONFRAME` or global `GETV_STEREO_MTXGUARD=2`.
**Do not** merge [#72](https://github.com/no6969el/GEVR/issues/72) Dam bridge blue.
**Date:** 2026-09-20.
**Evidence:** GEVR #70 chair (Run 0 PARTIAL PASS, MONFRAME REJECT), public `n64decomp/007` throw/embed/monitor draw, vr442 zip boot `HEAD_TRANSLATE=0`. Workshop `gfx_pc.c` bodies still private.

Director can PARK, or green-light one residual chair, from this page alone.

Prior DIG: `getv/patches/dam-modem-flicker-dig/RESULT.md` (PR #5).
MONFRAME APPLY (rejected in chair): `getv/patches/dam-modem-monframe-apply/` (PR #6). Leave that patch **OFF**.
**Left-eye leftover** (standing near, after SKYMESH=0): `getv/patches/dam-modem-lefteye-dig/RESULT.md` — PR27 CHRBUG attach path does **not** explain left-only.

---

## Verdict (one screen)

| Question | Answer |
|----------|--------|
| Best ship tonight | **PARK `GETV_XR_HEAD_TRANSLATE=0`.** Already vr442 `gevr-vr442-boot.cmd`. Original ticket (look-from-downstairs flicker) is that path. |
| Residual | **Near-field `PROP_CHRBUG` spawn + embed**, not the far monitor-UV tick. Attach white = first frames of the thrown gadget in front of the HMD. On-dish leftover = child stuck on the screen, always `ONSCREEN`, baked with `viewToWorld`. |
| Same as 5.3b MONFRAME? | **No. REJECT stands.** Skipping `process_monitor_animation_microcode` mutation made the **near** picture worse (see §2). |
| Same as #72 / MTXGUARD=2? | **No. Do not arm.** Global `=2` is Dam tunnel blue. This residual is a child prop, not sky fill. |
| Same as #55? | **Split kept.** HT0 helped Dam *and* Facility/Bunker, but #70 leftover is white/object, not full-screen black. |
| APPLY tonight? | **No.** Residual C is a sit, default OFF, and only if Director wants the last white pop. |

```
far / under towers     HEAD_TRANSLATE=0     PARK (shipped) — portal/eye offset
attach white (on dish) generate_player_thrown_object → PROP_CHRBUG at gun
                       → 1–2 frames in HMD near field → tex/shade pop
on-modem leftover      objEmbed uses currentPlayerGetViewToWorldMtxf()
                       child forced ONSCREEN, drawn before parent mtx convert
MONFRAME=1             skip cmdlist → tconfig stays 0 (IMGBOND) + verts 0xFF
                       → white quad  REJECT
MTXGUARD=2             skip f32→s16.16 globally → Dam tunnel blue  #72, not here
```

---

## 1. What chair already proved

| Sit | Result | Keep |
|-----|--------|------|
| Run 0 `HEAD_TRANSLATE=0` | Continuous under-towers flicker **dramatically decreased**. Brief dish flash; **big white on throw/place**. | **Ship.** vr442 boot already `set GETV_XR_HEAD_TRANSLATE=0`. |
| MONFRAME=1 | **REJECT — worse than Run 0.** | Never boot, never KEEP. |
| TEXINVAL / TEXDLRETAG / VFX `=0` | **Not run after HT0.** Still chair-cheap; not the first residual C. | KEEP stays ON. |
| MTXGUARD=2 | Not a #70 sit. #72: global `=2` → Dam tunnel blue. | Do not re-arm on Dam. |

Owner residual (this dig): *quick white flash on attach then mostly gone; leftover when on the modem.* That matches Run 0 leftovers, **not** the downstairs continuous case HT0 already cut.

C-unset in this repo’s KEEP harness is still `HEAD_TRANSLATE` **ON**. Product zip **overrides to 0**. PARK is the bat, not a C flip in `port_render.c`.

---

## 2. Why MONFRAME is not the residual path

`process_monitor_animation_microcode` (`propobj.c` ~6768):

1. Walks `MonitorRecord.cmdlist` (`TVCMD_SETTEXTURE` / colour / scroll).
2. Then copies 4 verts, `texSelect`, triangles.

PR #6 wrap skipped **(1)** on the non-sim eye. Vertex setup + `texSelect` stayed outside.

Init (`initobjects.c`): `g_InitialMonitorAnimController` has **`tconfig = 0`** and **RGBA 0xFF,0xFF,0xFF**. `monitorSetImageByNum` only swaps `cmdlist` (Dam tag 5 → `monAnim05GreenTextUp`). `tconfig` stays **0** until the first `TVCMD_SETTEXTURE`.

`tconfig == 0` is **not** NULL. `(u32)tconfig < 100` → `monitorimages[0]` = **`IMGBOND`** (I8 bond logo). Vertex colour still white. I8 × 0xFF = a **bright white/grey quad**.

`texSelect(NULL)` is the other white: `G_CC_SHADE` (shade-only). MONFRAME does not need NULL to look white; skipped SETTEXTURE + skipped `MONRGBA(COLOR_BARELYGREENOPAQUE)` is enough.

If `gePortSimShouldTick()` is missing or always 0, **both** eyes skip the cmdlist. The dish becomes a frozen white logo. That is “worse than Run 0.”

On the dish the quad is huge, so the fail is obvious. Under the towers HT0 already removed the portal flicker, so MONFRAME had nothing left to fix and only this failure mode.

**Do not reland, retune, or KEEP-ON MONFRAME.**

---

## 3. Residual root (attach white + on-modem)

### 3.1 Attach / throw is a world `PROP_CHRBUG`, not a monitor swap

| Step | File | What happens |
|------|------|----------------|
| Fire gadget | `gunfire.c` | `ITEM_BUG` → `generate_player_thrown_object` |
| Spawn | `gun.c` ~2014 | `PROP_CHRBUG` (245). `wor->timer = 1` (place, not a long toss) |
| Pose | `gunfire.c` ~574 | `throw_item_pos_related = viewToWorld * gunmtx_camspace` (`GETV_VR_THROWAIM=1` ships) |
| Stick | `propobj.c` ~4603 / 4646 | `canEmbed`; `objEmbed` if parent `ONSCREEN` |
| SFX | same | `ATTACH_MINE_SFX` — no `tv_change_screen_bank` on tag 5 |

You are **on the dish** looking at pad 10057 `{3408, -10, 465}`. The gadget spawns at the **gun**, ~arm’s length from the HMD, `znear=10`. One or two frames of a new model in that volume is a **full-view pop**.

`PchrbugZ` (3504) is larger than `PmodemboxZ` (832). The stuck device is not a speck.

Parent `PROP_MODEMBOX` already existed with `monAnim05GreenTextUp`. Attach does **not** reset that anim. What is **new** is the child.

### 3.2 Embed matrix is the current eye’s `viewToWorld`

`objEmbed` (`propobj.c` ~3229):

```c
nodemtx = modelFindNodeMtx(model, node, 0);
matrix_4x4_multiply_homogeneous(currentPlayerGetViewToWorldMtxf(), nodemtx, &mtx2);
matrix_4x4_invert_affine(mtx2, mtx3);
matrix_4x4_multiply_homogeneous(&mtx3, &mtx1, &obj->embedment->matrix);
```

Correct on a **single** camera if `nodemtx` and `viewToWorld` are the same eye. Stereo: whichever eye is current at stick **bakes** the child. The other eye then draws `parentNode * embedment`.

Far away (under towers) the child is tiny — HT0 already solved what you could see. **On the modem** the child fills the view, so IPD / stale-eye bake is a residual flash. Attach is the frame that bake happens.

### 3.3 Child is forced on-screen; drawn before parent mtx convert

`sub_GAME_7F0442DC` (~3622): if embedded, **`prop->flags |= PROPFLAG_ONSCREEN`** — no frustum test — `render_pos = parentNode * embedment`.

`sub_GAME_7F04AC20` (~7348): draw children, **then** if `arg2`, `bondviewTransformManyPosToViewMatrix` on the **parent** (in-place f32→s16.16). That convert is the MTXGUARD family. **Do not skip it globally** (#72 Dam blue). A monitor-only skip is *named* in §5.3d only as a last resort, not a recommendation.

### 3.4 White, specifically

| Source | When it reads white |
|--------|---------------------|
| New `PROP_CHRBUG` first draw | Model tex not bound yet / shade; **in HMD near field at attach** |
| Monitor verts 0xFF + `tconfig=0` | Only if cmdlist has not run (`IMGBOND` × white) — **MONFRAME**, not HT0 residual |
| `texSelect(NULL)` → `G_CC_SHADE` | Shade-only white quad — not the HT0 leftover if cmdlist still runs |
| Sky / water RGB | **#72**, blue, bridge/jump-strip. Not this ticket |

---

## 4. Hypothesis table (residual only)

| # | Hypothesis | Result |
|---|------------|--------|
| R1 | Far UV / per-eye monitor tick still the leftover | **Falsified as the ship problem.** HT0 cut the far case. MONFRAME (the UV tick) **worsened** the near case. |
| R2 | Thrown/embedded `PROP_CHRBUG` in near field | **Best source fit** for attach white + on-dish leftover. |
| R3 | TEXINVAL KEEP poisoning first `texSelect` | **Open, chair-cheap.** Already ON during the bug. A/B `=0` once; **do not C-default OFF**. |
| R4 | Global MTXGUARD=2 | **Rejected for Dam.** #72. Do not chair on this ticket. |
| R5 | #72 sky fill | **Separate.** Bridge blue ≠ dish white. |

---

## 5. APPLY sketches — **NOT LANDED**

**APPLY READY? No.** Need one split sit (flat vs HMD) before any C.

### 5.1 Do not land

- `GETV_VR_MONFRAME=1` / retune / KEEP.
- Global `GETV_STEREO_MTXGUARD=2`.
- Merge into #55 or #72.
- C-default OFF `TEXINVAL` / `TEXDLRETAG` / `VFXTMEM` / `VFXSHIFT`.
- Flip `HEAD_TRANSLATE` C-unset in this repo (zip already ships 0).

### 5.2 Chair first (no rebuild) — residual only

vr442 / Latest. `HEAD_TRANSLATE=0` already in boot. Dam **007**. Recenter. **Do not** set MONFRAME. **Do not** set MTXGUARD=2. **Do not** set FOVMATCH.

| Run | What | If white **dies** | If white **stays** |
|-----|------|-------------------|--------------------|
| **R-flat** | `Play-on-monitor.bat` (no stereo). Place on the dish. | Leftover is **stereo embed / IPD** → sketch 5.3b. | Leftover is **projectile first frame / tex** → 5.3a, then optional TEX `=0`. |
| **R-tex** | Headset, `GETV_VR_TEXINVAL=0` only. | Explosion-path inval on first bug `texSelect`. **Do not** ship OFF. 5.3a scoped skip. Check explosions still coloured. | Restore. Not TEXINVAL. |
| **R-throw** | Headset, `GETV_VR_THROWAIM=0`. | Spawn pose was in the HMD. Wear-only; 5.3a still the C if you want hide-in-flight. | Restore THROWAIM (KEEP). |

### 5.3 Smallest C (workshop, default OFF, after that sit)

**5.3a — if R-flat still white (projectile):** skip draw of `PROP_CHRBUG` while `RUNTIMEBITFLAG_HASPROJECTILE` (in-flight / pre-embed). After `objEmbed`, draw as now.

```
GETV_VR_BUGHIDE   unset / empty / 0 = OFF   (retail: see the gadget fly)
                  1 = hide PROP_CHRBUG until EMBEDDED
Banner once: [getv][bughide] GETV_VR_BUGHIDE=1
```

Site: `chrobjRenderProp` / `sub_GAME_7F04AC20` child walk. Gate `weaponnum == ITEM_BUG` (and only that — not mines). **Not KEEP-ON.**

**5.3b — if R-flat clean (stereo embed):** bake `objEmbed` with sim-owner / first-eye `viewToWorld` only (`gePortSimShouldTick`). Do **not** wrap monitor cmdlist (that is MONFRAME).

Same files: `propobj.c` `objEmbed` + `sub_GAME_7F0442DC`. Optional `GETV_VR_BUGEMBED=1` default OFF.

**5.3c — hide stuck mesh when too close:** skip embedded `PROP_CHRBUG` if parent is `PROP_MODEMBOX` and the node is inside ~`znear` slack. Loses the stuck-device mesh on the dish. Only if 5.3a/b PASS and on-dish leftover remains.

**5.3d — not recommended:** scoped skip of `bondviewTransformManyPosToViewMatrix` for `PROPDEF_MONITOR` only. Same *family* as MTXGUARD, **not** global `=2`. High Dam-blue risk on that object. Do not start here.

### 5.4 Out of scope

- Relanding MONFRAME
- Global MTXGUARD=2 / Bunker-only bat on Dam
- #72 sky fill / SKYMESH A/B
- Boot allowlist until a residual sit PASS (HT0 is already shipped)

---

## 6. Chair stare — residual (plain tester sentences)

**Setup:** vr442 Latest. `Start-GEVR.bat`. Boot already `HEAD_TRANSLATE=0`. Dam 007. Headset name + OpenXR. No MONFRAME. No MTXGUARD=2.

**Ship check (HT0 still holding the original ticket):**

| | Tester sentence |
|--|-----------------|
| **S-PASS** | “From downstairs under the towers, looking up at the dish, it stays **steady**. No every-frame flicker.” |
| **S-FAIL** | “It still strobes from downstairs.” → HT0 not in this zip / boot not 0. Stop residual C. |

**Residual:**

| | Tester sentence |
|--|-----------------|
| **A-NOTE** | “When I **stick** the covert modem, I get a **quick white flash**, then it is gone.” (known leftover; this is the residual.) |
| **N-NOTE** | “Standing **on the dish**, looking at the screen, I still get a **little** flash. Walking away, it is gone.” |
| **A-PASS** | “Stick is **clean**. No white pop. On the dish the picture stays green text, both eyes.” |
| **F-PASS** | `Play-on-monitor.bat`: “Stick is **clean** on the monitor.” → residual was stereo; 5.3b. |
| **F-FAIL** | Flat still **white on stick** → 5.3a (hide in-flight), not embed. |

**Regression:** explosions still coloured. Dam crates / water as vr442. Bridge **blue** is #72 — write it, do not fail this sit on it. Unset any scratch knob restores vr442.

---

## 7. Director decision

| If you say… | Then… |
|-------------|--------|
| **PARK** | **Recommended.** Ship HT0. Close #70 as residual-known or leave open with this page. No more C for the far flicker. |
| **Green residual chair only** | Sit S + A/N + R-flat. No C. |
| **Green 5.3a if R-flat still white** | Workshop `BUGHIDE` default OFF. Sit attach. |
| **Green 5.3b if R-flat clean** | Workshop sim-owner `objEmbed` default OFF. Sit on-dish. |
| **Green MONFRAME again** | **Rejected** by chair and by this dig. |
| **Green MTXGUARD=2** | **Rejected** for Dam (#72). |
| **Merge #72** | **Rejected.** Blue bridge ≠ white dish. |
| **Flip TEXINVAL C-default OFF** | **Rejected.** |

---

## 8. Attribution / legal

- No personal credit paths edited.
- No GoldenEye ROM, assets, or dumps.
- Decomp citations are public `n64decomp/007` (`gun.c` / `gunfire.c` / `propobj.c` `objEmbed` / `process_monitor_animation_microcode` / `texSelect` / `initobjects.c` / `oddtextures.c`).
- Workshop C stays private until release policy flips.
