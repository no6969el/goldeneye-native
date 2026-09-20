# RESULT — HMD render resolution independent of the desktop window (DIG ONLY)

**Status:** DIG. **Not APPLY READY on this public tree.** No C landed.
**Ask ([GEVR #46](https://github.com/no6969el/GEVR/issues/46)):** raise what the **headset** draws without forcing the same huge size on the **desktop mirror**. Most VR titles already split those two. Wear stays **vr441**.
**Not [GEVR #39](https://github.com/no6969el/GEVR/issues/39).** Gun scale / slight cross-eye is a viewmodel / stereo error. This ticket is fill-rate and present size. Do not touch `GETV_VR_GUN*`, `GETV_XR_UNITS_PER_M`, or `GE_VR_VIEWMODEL_*`.
**Date:** 2026-09-20.
**Evidence:** public `goldeneye-native` HEAD, public `no6969el/GEVR` textbook + packaging (vr441 boot), public probes under `GEVR/xr`. Workshop C (`gfx_pc.c` write sites, `gevr_xr.c` swapchains, `ge_postfx.c`) is **not on any public remote** (`GEVR` `docs/RELEASE-POLICY.md`). Brief names are the workshop symbols from public docs.

Director can green-light the split, park it, or reject from this page alone.

---

## Verdict (one screen)

| Question | Answer |
|----------|--------|
| Wear-only tonight? | **No.** vr441 KEEP already chairs sharpness (`GETV_SUPERSAMPLE=3` + `GETV_XR_PLAY_SRCFBO=1`). That path still sizes **3D + HMD copy + desktop** off **one** `gfx_current_dimensions`, which is the **SDL window**. |
| Where is the tie? | **`gfx_pc.c:6062`** writes `gfx_current_dimensions` from `gfx_wapi->get_dimensions()` (the window). **`:6069-6076`** then overwrites it for supersample. XR SrcFbo copies **that** FBO. Desktop presents **that** size. |
| Split? | **Yes.** One render (F4 forbids a second `gfx_run`). Two **present** sizes: HMD swapchain from `recommendedImageRect*`, desktop blit from the same FBO down to `GETV_WINDOW` / SDL. |
| Knob | **`GETV_XR_HMDSCALE`** default **unset = 1.0** (runtime recommended × scale). **PLAYER_PREF, not KEEP-ON.** `GETV_WINDOW` stays the desktop SDL size. `GETV_SUPERSAMPLE=3` stays KEEP and applies to the **HMD FBO only** once the split lands. |
| APPLY tonight? | **No from this repo.** Bodies are private. The overwrite site is one assignment, but X4 presentation + H19 aspect + SrcFbo copy-rect are not a one-liner. |

```
tonight  ->  SDL window  ->  gfx_current_dimensions  ->  x SS3  ->  one FBO
             ->  SrcFbo copy to XR   AND   SDL present   (same pixels)

split    ->  HMD  = recommended x HMDSCALE  [x SS3 on that FBO]
             desk = GETV_WINDOW / SDL (small)
             gfx_current_dimensions during 3D = HMD FBO size
             desktop = downsample blit of that FBO (not a second render)
```

---

## 1. Root finding (files + functions)

### 1.1 Evidence boundary

| Layer | Where | What it proves |
|-------|--------|----------------|
| Public `goldeneye-native` | this repo | Scaffold ABI already sizes XR swapchains from **`recommendedImageRectWidth/Height`**, not a window (`src/xr_session.cpp`). GETV workshop **did not ship that split** for the playable present. |
| Public `no6969el/GEVR` | docs + `packaging/` | vr441 KEEP: SS3 + SrcFbo. `GETV_WINDOW` **wiped**. Owner already named the trap: *2560x1369 is a desktop window, not the headset bar*. Runtime recommended on the chair rig: **4140x3292 per eye**. |
| Workshop (private) | workshop `goldeneye-native` | `gfx_pc.c` F1/F2, `gevr_xr.c` swapchains, `ge_postfx.c` / `gfx_opengl.c` `pp_fbo`, `port_support.c` `GETV_WINDOW`. **Do not push.** |
| Public probes | `GEVR/xr/xr_stereo.cpp`, `xr_probe.cpp` | Same recommended-rect create as the scaffold. Not the game present. |

Line numbers below are **workshop / textbook citations** (`GEVR` `docs/319`, `00-STATE`, `28`). Confirm live lines before typing.

### 1.2 The one size that everything else reads

**Workshop F1 (measured, `319` section 2):** `gfx_current_dimensions` has **exactly one write site**, once per frame:

- `gfx_start_frame()` at **`gfx_pc.c:6062`**
- source: `gfx_wapi->get_dimensions()` -> **the SDL window**
- downstream: `ge_scale()`, `ge_offset_x()`, `RATIO_X/Y`, viewport calc, and **`gfx_adjust_x_for_aspect_ratio()`** (`:2362` / `:2369`, H19)

So the 3D scene, the scissors, and the aspect squeeze all believe the **desktop window** is the render target.

**Workshop F2:** a shipped path **already overwrites the same struct after that read** -- `gfx_pc.c:6069-6076`, supersample. Comment (textbook): *everything downstream is derived from `gfx_current_dimensions`, so scaling it here makes the whole scene render at the higher resolution with no other changes.*

That overwrite is the **split hook**. Tonight it multiplies the **window**. The ask is to point it at the **HMD rect** instead, and leave the window alone.

### 1.3 How vr441 sharpness actually chairs

Public boot (`packaging/templates/gevr-vr441-boot.cmd`):

| Knob | Ship | Class | Role |
|------|------|-------|------|
| `GETV_SUPERSAMPLE` | `3` | KEEP_SHIP | Scale `gfx_current_dimensions` after the window read. |
| `GETV_XR_PLAY_SRCFBO` | `1` | KEEP_SHIP | Real SrcFbo name. **Never SS>1 without SrcFbo.** Dead name `GETV_SRCFBO` fails smoke. |
| `GETV_XR_PLAY` | `1` | KEEP_SHIP | XR present path. |
| `GETV_XR_PLAY_SCREEN` | `2` | KEEP_SHIP | Mirror / which-surface present. **Do not flip for this ticket.** |
| `GETV_XR_PLAY_SRCRECT` | `full` | KEEP_SHIP | Copy rect into XR. |
| `GETV_XR_PLAY_EYERECT` | `1` | KEEP_SHIP | Per-eye rect on submit. |
| `GETV_WINDOW` | **wiped** | DIG_OFF wipe | Live fill-rate / SDL size when set (`port_support.c`). Boot clears it so a polluted shell cannot pin a huge window. |

`GETV_SUPERSAMPLE` was **inert on Windows** in `273` section 7.2 (both `getenv` sites behind `TVOS_SUPERSAMPLE` / `GE_POSTFX`). vr441 KEEP proves that later **landed**. The live reader is F2 on `gfx_current_dimensions`, plus SrcFbo so SS does not have to grow the SDL window **by itself**.

**The remaining hitch:** SrcFbo stops SS from *being* the window, but the **XR copy still takes that FBO**. To get the headset near the runtime bar (4140x3292 per eye on the chair Pimax; Quest will differ), the only live lever is **grow `GETV_WINDOW` (or the default SDL size) then x SS3**. Desktop DWM / Present then hitch on the same huge buffer. That is #46.

### 1.4 Stereo is one framebuffer (F4 / X4)

**F4:** one display list, one `gfx_run`, one swap (`port_render.c` frame bracket; `stereo.h`: *`gfx_run()` is never called twice*). `geStereoEyeViewport()` (`stereo.c:233`) makes each eye a **half-width viewport of the one target**.

**X4 (still the presentation debt):** *today stereo is ONE framebuffer split in halves; a runtime wants swapchain images per eye plus a mirror window. What changes is where the halves go, not that there are two.*

Consequence for this dig:

- **Do not** render the world twice (once HMD, once desktop). Arena pairing already forbids a cheap second `gfx_run`.
- Desktop **must** be a **blit / downsample** of the HMD (or SBS) FBO.
- Per-eye XR swapchains can still be **created** at recommended size; the copy into them is X4, already KEEP via SrcFbo + `EYERECT`.

### 1.5 Historical RT64 coupling (same bug, older host)

`GEVR` `docs/28` (step 3f-3), when the host was RT64 + `CopyTextureRegion`:

> Create each eye's swapchain at `(windowWidth / 2, windowHeight)` and let the runtime scale to the panel.

`CopyTextureRegion` does not rescale, so HMD size **was** half the window. Stated costs: resolution capped by the window; monitor shows SBS. **Design A** (the follow-up that never became GETV present): render each eye through the real path **into the XR swapchain image at the headset's own resolution**, no dependence on the window.

Public scaffold already does Design A's **size** (`src/xr_session.cpp`):

```c
ci.width  = vc.recommendedImageRectWidth;
ci.height = vc.recommendedImageRectHeight;
```

Public `GEVR/xr/xr_stereo.cpp` does the same for the probe. GETV play still sizes 3D from the SDL window.

Old RT64 knob `GE_VR_RES_SCALE` (`docs/80`) scaled **recommended eye size**. **O22: smaller made it SLOWER, unexplained.** Do **not** revive that name. A new GETV knob must sit; naive downscale is not free (compositor upscale / reprojection).

### 1.6 Owner measurement that #46 restates

`00-STATE` / `319`: owner asked whether the resolution bar was the **headset** or *this arbitrary monitor resolution -- I believe we're at like five k by five k when we're in the headset.*

Textbook answer: **`2560x1369` is `GETV_WINDOW` (desktop, standing since `286`) and was being quoted as if it constrained the headset.** Measured recommended per-eye: **4140x3292** (max **8192x8192**). Chair recollection was the right order of magnitude.

#46 is that sentence as a feature: HMD bar without the desktop hitch.

### 1.7 What this is not

| Ticket / knob | Why out |
|---------------|---------|
| [#39](https://github.com/no6969el/GEVR/issues/39) gun huge / cross-eye | Viewmodel scale + stereo depth. `GETV_VR_GUNMOUNT` / `GUNAIM` / `GUNARM` stay. |
| `GETV_XR_UNITS_PER_M` | World scale. |
| `GE_VR_VIEWMODEL_SCALE` / `-ViewmodelScale` | RT64-era gun size. |
| [#49](https://github.com/no6969el/GEVR/issues/49) Hz | Timebase, not pixels. |
| [#60](https://github.com/no6969el/GEVR/issues/60) slowdown | May **share GPU budget** with a huge FBO; do not treat this split as that bug's fix until a sit. |
| `GETV_XR_FOVMATCH` | Dig falsifier. Stay wiped. |
| H19 aspect squeeze | Related trap if HMD aspect is not the window aspect. Named below; not this ticket's patch unless the split forces it. |

---

## 2. How the two sizes sit together

Not "SS vs window." One 3D target, two presents.

| Surface | Tonight | After split |
|---------|---------|-------------|
| 3D / Fast3D | `gfx_current_dimensions` = SDL x SS3 | **HMD FBO** = `recommended x HMDSCALE` (SBS width = 2x eye if still one target) |
| XR swapchain | Copy of that FBO (SrcFbo) | **Same HMD size** (create at recommended x scale; SrcFbo / EYERECT still copy) |
| SDL window | Same pixels (or the window grown to match) | **`GETV_WINDOW` / default SDL.** Downsample blit. Soft on the monitor is OK. |

**SS3 KEEP:** once split, F2 multiplies the **HMD** rect, not the window. Boot comment *Never SS>1 without SrcFbo* stays. Do not apply SS to both HMD and desktop.

**SBS width trap:** recommended is **per eye**. One-target GETV is **two halves**. HMD FBO width = `2 * recW * scale` if X4 has not yet split to two colour targets. Creating each XR swapchain at `recW x recH` and copying halves is already the X4 shape. Do not create one swapchain at SBS size and call it per-eye.

**Aspect trap (H19):** `gfx_adjust_x_for_aspect_ratio` divides by **`gfx_current_dimensions` aspect**. If 3D runs at HMD size, `a` uses **HMD aspect**, which is what the lenses want. If 3D stayed at window aspect while XR submitted a different rect, H19 would move. **F2 must set the struct to the HMD FBO size before any vertex / viewport work.** Window size is only the blit dest.

---

## 3. APPLY sketches -- **NOT LANDED**

### 3.1 Shared sizes (write once)

Workshop: next to F2 in `gfx_start_frame` (`gfx_pc.c`), reading rects from `gevr_xr.c` (swapchain owner, `D3`).

```c
/* NOT APPLY READY -- workshop sketch.
 * deskW/H = SDL / GETV_WINDOW          -- blit dest only
 * eyeW/H  = recommendedImageRect * HMDSCALE
 *          clamp to maxImageRect
 * fboW/H  = still-SBS ? (2*eyeW, eyeH) : (eyeW, eyeH)
 * gfx_current_dimensions = fboW/H      -- THEN x SS3 if KEEP
 * SDL window stays deskW/H
 */
```

`gevr_xr.c` already enumerated `recommendedImageRect*` at session create (`321`: two swapchains at 4140x3292 on the chair rig). Reuse those statics. Do not re-query every frame (`319` X1-3: rects unchanged across 1h).

### 3.2 Knob -- `GETV_XR_HMDSCALE` default unset = 1.0

```
GETV_XR_HMDSCALE    unset / empty = 1.0   (recommended, chair A/B)
                    0.5 / 1.5 / 2.0       wear; clamp to maxImageRect
GETV_WINDOW         unchanged: desktop SDL only (already wiped in vr441)
GETV_SUPERSAMPLE    3 KEEP -- multiply HMD FBO only
GETV_XR_PLAY_SRCFBO 1 KEEP -- required
GETV_XR_HMDRES_TRACE 0  DIG_OFF
```

```c
static float ge_xr_hmdscale(void)
{
    static float s = -1.f;
    if (s < 0.f) {
        const char *e = getenv("GETV_XR_HMDSCALE");
        s = (e != NULL && *e != '\0') ? (float)atof(e) : 1.f; /* default rec; not KEEP-ON */
        if (s < 0.1f) s = 0.1f;
    }
    return s;
}
```

Do **not** add `HMDSCALE` to `gevr-vr441-boot.cmd` / `$requiredBootKnobs` until a sit passes. Scratch: `set GETV_XR_HMDSCALE=1.5` with window left wiped.

**Do not** reuse `GE_VR_RES_SCALE` or invent `GETV_SRCFBO`.

Optional later: `GETV_XR_HMDRES=rec|WxH` if a fixed pixel bar beats a scale. Scale is enough for the first sit.

### 3.3 Desktop blit (F3 precedent)

**F3:** desktop offscreen target already exists. `ge_postfx.c` is tracked params; `glBindFramebuffer` / resolve stay in `gfx_opengl.c` (`pp_fbo` desktop, `ss_fbo` tvOS). `D3` put XR rects in `gevr_xr.c` and the bind beside those two FBO paths.

Split present:

1. Bind HMD / SBS FBO (or acquired swapchain if X4 has landed per-eye targets).
2. `gfx_run` once at `gfx_current_dimensions` = that FBO.
3. SrcFbo / EYERECT copy halves into XR swapchains (KEEP).
4. **Blit / downsample** that FBO into the SDL backbuffer at `deskW x deskH`. Soft mirror is success.

Do not `SDL_SetWindowSize` to the HMD rect. Do not `Present` the HMD-sized buffer to DWM.

### 3.4 Out of scope

- #39 gun scale / cross-eye
- KEEP-ON graduation of `HMDSCALE`
- Boot allowlist / pack smoke until sit PASS
- Second `gfx_run` / dual-res world
- Flipping `GETV_XR_PLAY_SCREEN`, `SRCRECT`, `EYERECT`
- `GETV_XR_FOVMATCH`
- Personal credit paths. ROM dumps

---

## 4. Chair stare (plain tester sentences)

**Setup:** vr441-class zip. `Start-GEVR.bat`. Recenter both sticks. Leave `GETV_WINDOW` **unset**. `GETV_SUPERSAMPLE=3` and `GETV_XR_PLAY_SRCFBO=1` stay.

**Run T -- tonight (control):** no `HMDSCALE`.

| | Tester sentence |
|--|-----------------|
| **T-PASS** | "The headset is vr441 sharp. The **desktop window is not a 4k+ slab** that hitchs the PC when I look at the monitor." If the window **is** already a hitch-slab, T **FAILS** and is the #46 repro. |
| **T-FAIL** | "Raising quality means I have to grow the window / the mirror stutters the desktop." |

**Run S -- split on:** same boot plus `GETV_XR_HMDSCALE=1` first (should match recommended, window still small), then `1.5` if T was already rec-sized.

| | Tester sentence |
|--|-----------------|
| **S-PASS 1** | "The **headset** got sharper (or stayed vr441 sharp at scale 1) and the **desktop window stayed small**. The mirror is a bit soft. That is fine." |
| **S-PASS 2** | "I can move my head. No extra hitch on the **monitor** when I raise `HMDSCALE`. Headset cost is GPU in the lenses, not DWM." |
| **S-PASS 3** | "Stereo still fuses. HUD / gun size did **not** jump. This is not a #39 gun-scale run." |
| **S-PASS 4** | "Unset `HMDSCALE` (or scale 1) + wiped `GETV_WINDOW` restores tonight." |
| **S-FAIL** | "The window grew with the headset." / "Headset stayed soft while the window exploded." / "World went skinny/fat" (H19 aspect). / "Gun looks huge or cross-eyed" (#39 -- **stop, revert, different ticket**). / "Scale 0.5 hitchs more than 1.0" (O22 -- write it down, do not ship that value). |

**Regression (every run):** recenter, trigger, stick-turn, squeeze ADS on the **gun ray**, casings. Do not move `GUNAIM` / `GUNMOUNT` / `GUNARM`.

**Log grep (workshop):** print `desk WxH`, `hmd eye WxH`, `fbo WxH`, `ss`, `hmdscale` once at first frame. If `desk == fbo` after the split, the blit did not land.

---

## 5. Director decision

| If you say... | Then... |
|-------------|---------|
| **Green split, HMDSCALE unset=1** | Workshop 3.1-3.3. SS3 stays on HMD FBO. Sit T then S. Window stays wiped. |
| **Green, but pin a ship scale** | Sit first. Only then boot-assign. Not KEEP-ON until PASS. |
| **Just grow `GETV_WINDOW`** | Rejected by this dig. That **is** the hitch. |
| **Render twice (HMD + desk)** | Rejected. F4 / arena pairing. |
| **Also fix #39** | No. Separate sit. |
| **Reject** | Stop. Leave SS3+SrcFbo coupled to the window. |

---

## 6. Attribution / legal

- No personal credit paths edited.
- No GoldenEye ROM, assets, or dumps.
- RT64-era `GE_VR_RES_SCALE` / Design A are **map-only** (public GEVR textbook). Do not copy workshop bodies.
- Workshop C stays private until release policy flips.
