# RESULT — Dam water flat / murky in VR (#30) (DIG ONLY)

**Status:** DIG. **Not APPLY READY on this public tree.** No C landed.
**Ask:** Dam water can look **flat**, **murky**, or otherwise wrong in VR vs the rest of the scene (and vs N64 / `Play-on-monitor.bat`).
**Constraints:** Parked falsifier `GETV_VR_WATERTILE` stays **default OFF** (wipe / env-only; do not KEEP-ON). Not Dam bridge / jump-strip **blue flash** ([#72](https://github.com/no6969el/GEVR/issues/72)). Not Dam crate pop ([#29](https://github.com/no6969el/GEVR/issues/29)). Do not re-arm `GETV_STEREO_MTXGUARD=2`.
**Date:** 2026-09-20.
**Evidence:** public `goldeneye-native` HEAD, public `no6969el/GEVR` packaging + #30/#72, public `n64decomp/007` `sky.c` / `bgfog.c`. Workshop `skyRender` / `SKYMESH` **bodies** are **not on any public remote** (`GEVR` `docs/RELEASE-POLICY.md`). Brief names are the decomp + boot symbols.

Director can green-light a WATERTILE sit, reject, or send the chair A/B from this page alone.

---

## Verdict (one screen)

| Question | Answer |
|----------|--------|
| What is “Dam water” in source? | **Not** `IsWater` textured water. Dam `fog_tables[]` has **`IsWater=0`**, `WaterRepeat=-1000`, `WaterImageId=0`, `WaterRGB=0`. The lake look is **sky below-horizon + room mesh**, not `skywaterimages[WaterImageId]`. |
| N64 “reflection”? | **Not a mirrored scene.** Sky uses `skywaterimages[SkyImageId]` + combiner `SHADE/ENV/TEXEL0`. Below-horizon on Dam is **`G_CYC_FILL` of sky RGB `0x10,0x30,0x60`**. Textured water (`IsWater`) is a **different** branch (Runway / Surface family). |
| Why VR looks flat? | Water / horizon is still **screen-space RDP** (`skyRenderTri` / `skyRenderFull` via `G_RDPHALF_1`+`G_RDPHALF_CONT`, or a fill rect). **`GETV_VR_SKYMESH=1` remeshes sky only.** No stereo disparity on the lake sheet. |
| Why VR looks murky? | Dam fill / fog base is **`0x10,0x30,0x60`**. Vertex tint is `skyChooseWaterVtxColour` (fog RGB + WaterRGB × angle). One-eye leftover viewport + Bond-pos (not IPD eye) makes that blend a **flat blue-grey**. |
| Monitor vs headset? | `Play-on-monitor.bat` is **`FLAT_FORCE`**: `GETV_STEREO=0`, no sky KEEP assigns. One 320-class viewport — N64-shaped. Headset: stereo + `SKYMESH`/`SKYSCISSOR`/`SKYFILL*` + `HEAD_TRANSLATE`. |
| Same as #72? | **No.** #72 is a **blue flash when turning** on the Dam bridge (MTXGUARD=2 history). This is the **standing look** of the reservoir. |
| Falsifier | **`GETV_VR_WATERTILE` default OFF** (already wiped). `1` = chair: world-space water / horizon tiles. Do not add to `$requiredBootKnobs`. |
| APPLY tonight? | **No from this repo.** Bodies are workshop `sky.c` / stereo sky path. |

```
N64 / monitor (STEREO=0)
  skyRender → screen corners → skyIsCornerInWater / InSky
  Dam IsWater=0 → G_CYC_FILL (0x10,0x30,0x60) below horizon
  Clouds=1     → skyRenderFull/Tri G_RDPHALF + SkyImageId
  room Dam mesh draws on top

VR headset (STEREO=1, SKYMESH=1 KEEP)
  sky  → remeshed world tris (per-eye isolation KEEP)
  water/horizon → leftover N64 screen-space fill / RDP spans
               → flat sheet + murky Dam blue
  WATERTILE=0  → tonight (ship)
  WATERTILE=1  → dig: tiles on the water/horizon plane, per eye
```

---

## 1. Root finding (files + functions)

### 1.1 Evidence boundary

| Layer | Where | What it proves |
|-------|--------|----------------|
| Public `goldeneye-native` | this repo | `G_RDPHALF_CONT` is the sky/water opcode (`src/gbi/gbi.h`, `MICROCODE-SPEC.md` §2.1). `GETV_VR_SKYMESH` getenv stub. `GETV_VR_WATERTILE` is **do_not_touch** in `getv/patches/keep-defaults-on/MANIFEST.json`. |
| Public `no6969el/GEVR` | packaging + #30 | vr441 boot **wipes** `GETV_VR_WATERTILE` (section 0). Sky KEEP: `SKYMESH=1`, `SKYSCISSOR=1`, `SKYFILL=1`, `SKYFILL2=0`, `SKYFILL3=1`. FEATURES still call out Dam water. |
| Public decomp | `n64decomp/007` `sky.c`, `sky.h`, `bgfog.c` | Per-frame sky/water builder. Dam env row. |
| Workshop (private) | `F:\Projects\GEVR\GoldenEyeVR\goldeneye-native` | Live `ge_vr_skymesh()` / `ge_vr_watertile()` call sites. **Do not push.** |

`ge_vr_watertile` **does not appear in any public file**. First chair grep: `getenv("GETV_VR_WATERTILE")` and `skyRender`.

### 1.2 N64 path (one camera, one viewport)

Sky and water are **not** in room display lists. `skyRender` builds them every frame (`sky.c:258`).

| Step | Function | Job |
|------|----------|-----|
| Screen → world ray | `skyGetWorldPosFromScreenPos` | `c_screenleft/top` + **`WaterConcavity`** on Y; `currentPlayerGetViewToWorldMtxf()` |
| Horizon test | `skyIsScreenCornerInSky` / `skyIsCornerInWater` | Intersect **CloudRepeat** (skyheight) / **WaterRepeat** (seaheight) from **`bondviewGetCurrentPlayersPosition()`** |
| Sky tint | `skyChooseCloudVtxColour` | fog RGB + CloudRGB × (1 − angle frac) |
| Water tint | `skyChooseWaterVtxColour` | fog RGB + WaterRGB × (1 − angle frac). Alpha **0xFF** (opaque) |
| Project to screen | `sub_GAME_7F097388` then clamp to `c_screen*` × 4 | Screen-space verts, not RSP `G_VTX` |
| Draw | `skyRenderTri` / `skyRenderFull` | Raw RDP spans: `G_RDPHALF_1` + `G_RDPHALF_CONT` (`G_TRI_SHADE_TXTR` or fill) |

`G_RDPHALF_CONT` (`0xB2`) is stock F3D. F3DEX2 reassigned the slot. Room-DL corpora never see it. That is why a sky/water-only bug can hide behind a 100% room walk.

**Texture / combiner (the “reflection”):**

```
sky:   texSelect(&skywaterimages[SkyImageId])
       gDPSetEnvColor(fog Red/Green/Blue)
       COMBINE: SHADE, ENVIRONMENT, TEXEL0, ENVIRONMENT   /* lerp(shade, env, texel) */

water: if (IsWater)
         texSelect(&skywaterimages[WaterImageId])
         G_RM_OPA_SURF
         skyRenderTri(..., textured=TRUE)
       else
         G_CYC_FILL rectangle of the water-vert screen bounds
```

There is **no** second camera, **no** framebuffer reflection, **no** env-map of the SKYMESH.

### 1.3 Dam env row (why this stage is special)

`bgfog.c` `fog_tables[]` `LEVELID_DAM` (NTSC):

| Field | Dam | Meaning |
|-------|-----|---------|
| Visibility.BlendMultiplier | `5` | Near plane / fog (tight) |
| FarFog | `15000` | Long Dam vista |
| Sky RGB | **`0x10, 0x30, 0x60`** | Dark blue fill / fog |
| Clouds | `1` | Sky path runs (not early full-screen fill) |
| CloudRepeat | `5000` | Skyheight |
| SkyImageId | `0` | `skywaterimages[0]` |
| CloudRGB | `255, 255, 255` | |
| **IsWater** | **`0`** | **Textured water branch OFF** |
| WaterRepeat | `-1000` | Docs default “no water image”; still used as a **plane** in `skyIsCornerInWater` |
| WaterImageId / WaterRGB / Concavity | `0` | |

So on Dam, `sky.c:829` takes the **`!IsWater` fill**, not the `texSelect(WaterImageId)` tris. The reservoir **picture** is that **blue fill behind / under the Dam room mesh**. Flat + murky is the expected failure mode when that fill stays screen-space while the sky is remeshed.

Facility has `Clouds=0` (early fill) and no lake. Do not chair Facility as a water control.

### 1.4 Monitor vs VR (same `skyRender`, different viewport)

| | N64 | `Play-on-monitor.bat` | Headset `Start-GEVR.bat` |
|--|-----|------------------------|---------------------------|
| Eyes | 1 | 1 (`GETV_STEREO=0`) | 2 (`GETV_STEREO=1`, `STEREO_SRC=xr`) |
| Viewport | `c_screen*` 320×240-class | Same shape | XR swapchain / SrcFbo; `c_screen*` still what `sky.c` clamps to unless workshop remaps |
| Sky | RDP spans | RDP spans (bat does **not** assign sky KEEP; C-default `SKYMESH` may still be ON after graduation) | **`SKYMESH=1` KEEP** world mesh |
| Horizon / water | Screen fill or RDP spans | Same | **Not remeshed.** `SKYSCISSOR` / `SKYFILL` / `SKYFILL3` isolate **sky fill**, not water tiles |
| Eye origin | Bond ≈ camera | Bond ≈ camera | **`HEAD_TRANSLATE=1`**: eye ≠ `bondviewGetCurrentPlayersPosition()` |
| Fog / z | `viSetZRange(5, 15000)` Dam | Same | Stereo rebuild per eye (`STEREO_REBUILD=1`) |

`skyGetWorldPosFromScreenPos` and the water clamp (`sky.c:819–826`) are **N64 screen maths**. `skyIsCornerInWater` aims the plane from **Bond’s body**, not the IPD eye. On a monitor that matches Rare. In VR the horizon sheet locks to the first eye / Bond and reads as a painted quad.

`getPlayerCount()==1` (`sky.c:322`) is the stereo trap: VR is still one Bond. Do not use player-count to mean “one eye.”

### 1.5 What SKYMESH already did (and did not)

Public boot §8 “Sky / eye isolation”:

| Knob | Ship | Class | Role |
|------|------|-------|------|
| `GETV_VR_SKYMESH` | `1` | KEEP_SHIP | World-space **sky** |
| `GETV_VR_SKYSCISSOR` | `1` | KEEP_SHIP | Per-eye sky scissor |
| `GETV_VR_SKYFILL` | `1` | KEEP_SHIP | Sky fill isolation |
| `GETV_VR_SKYFILL2` | `0` | DIG_OFF | Leave off |
| `GETV_VR_SKYFILL3` | `1` | KEEP_SHIP | Sky fill isolation |
| `GETV_SKYTRACE` | wipe | forbidden in public boot | Census |

Water has **no** KEEP twin. `GETV_VR_WATERTILE` is the parked name. `FOGSKIP` / `DISTSKIP` are other wipes (#29 family). `PROPFOGW` is **prop** fog (crates); do not arm it for the lake.

---

## 2. Not #72 (Dam bridge blue flash)

| | #30 (this dig) | #72 |
|--|----------------|-----|
| Symptom | Reservoir **looks** flat or murky while standing / looking at water | **Blue flash** when **turning** on the bridge / jump-strip |
| Seen with | vr441-class ship | vr442 / Latest; also with `HEAD_TRANSLATE=0` during #70 chair |
| History | WATERTILE parked OFF | Global `GETV_STEREO_MTXGUARD=2` fixed Bunker #55 and **caused Dam tunnel blue** |
| Chair rule | Do **not** set `MTXGUARD=2` | Needs its own Dam-safe dig |

A sit that “fixes” #30 by flashing blue on the bridge is a **FAIL**, not a #30 PASS.

---

## 3. Proposed falsifier — `GETV_VR_WATERTILE` default OFF

Already classified:

- Boot section 0: `set GETV_VR_WATERTILE=` (wipe)
- `MANIFEST.json` `do_not_touch`
- `GRADUATED-KNOBS.md`: wiped dig falsifier; **do not ship ON**

**Do not invent a second knob.** Chair A/B this name.

```
GETV_VR_WATERTILE    unset / empty / 0 = OFF   (ship tonight)
                     1                 = ON    (dig: world-space water / horizon tiles)
GETV_VR_WATERTRACE   unset / 0         = OFF   (optional census; never public boot)
```

```c
/* NOT APPLY READY — workshop sketch.
 * Default OFF. Same ternary shape as GETV_VR_TWOHAND, opposite of KEEP-ON. */
static int ge_vr_watertile(void)
{
    static int on = -1;
    if (on < 0) {
        const char *e = getenv("GETV_VR_WATERTILE");
        on = (e != NULL && *e != '\0') ? (atoi(e) != 0) : 0; /* DIG_OFF; dig sets 1 */
    }
    return on;
}
```

**When ON (workshop `skyRender` after the water-vert build, `sky.c:803+`):**

1. Skip `G_CYC_FILL` (`!IsWater`, Dam) and skip `skyRenderTri` RDP spans (`IsWater`).
2. Emit **world-space** tris on the water / horizon plane (`WaterRepeat` as seaheight; Dam’s `-1000` is the table value — wear it, do not invent a Dam Y).
3. Rebuild **per eye** from the stereo view / projection (`STEREO_REBUILD` already updates those). Do **not** clamp to `getPlayer_c_screen*`.
4. Keep Rare’s colour: fill uses sky RGB; if `IsWater`, `texSelect(WaterImageId)` + `skyChooseWaterVtxColour`.
5. Ray origin for the plane: **this eye**, not Bond-only. (Bond-only is tonight’s murky blend.)
6. Leave `SKYMESH` / `SKYFILL*` / `GUNARM` / `OCCLSKIP` / `PROPFOGALPHA` alone.
7. Tick on sim-owner / first-eye only for any static latch (`lvframe60` trap). Draw still runs both eyes.

**Layer 1 (recommend first):** Dam `!IsWater` fill → world tiles. Smallest C. Matches #30’s stage.

**Layer 2 (only if H-PASS still fails):** Dam **room** water mesh (bg tiles in `bg_dam`). Separate sit. Do not fold into SKYMESH. Do not use `PROPFOGW`.

**APPLY READY?** **No** here. Confirm live workshop names (`ge_vr_skymesh` site) before typing. Offset / per-eye / Dam `-1000` need a sit.

---

## 4. APPLY sketch — **NOT LANDED**

Workshop: next to the `SKYMESH` branch that already replaces `skyRenderFull` / cloud tris. Water is the sibling block at `IsWater` / `G_CYC_FILL`.

```c
/* AFTER water verts exist (sp274[]), BEFORE G_CYC_FILL / skyRenderTri.
 *
 * if (ge_vr_watertile()) {
 *     draw world tiles from destpos[] / WaterRepeat plane, this eye's mtx;
 * } else if (!env->IsWater) {
 *     G_CYC_FILL ...          -- tonight Dam
 * } else {
 *     texSelect(WaterImageId);
 *     skyRenderTri(...);      -- tonight IsWater stages
 * }
 */
```

**Out of scope**

- `GETV_STEREO_MTXGUARD=2` / #72
- `FOGSKIP` / `DISTSKIP` / `OCCLSKIP` / `PROPFOGW` / `PROPFOGALPHA` (#29)
- `SKYFILL2=1`
- KEEP-ON graduation of `WATERTILE`
- Boot allowlist / pack smoke until a sit PASSes
- Personal credit paths. ROM dumps

---

## 5. Chair stare (plain tester sentences)

**Setup:** vr441-class zip. `Start-GEVR.bat`. Recenter both sticks. Dam, walk to the reservoir (not the bridge jump-strip first). `GETV_VR_WATERTILE` **unset**. Keep `SKYMESH=1`.

**Run N — tonight (control):**

| | Tester sentence |
|--|-----------------|
| **N-PASS** | “I can see the lake. It may look **flat or murky**. Sky above is the usual remesh. I am **not** reporting a blue **flash** when I turn.” |
| **N-NOTE** | Write: headset, OpenXR, SteamVR on/off, HMD vs monitor, bat, `gevr-*-boot.cmd`. |

**Run M — monitor control:** `Play-on-monitor.bat`, same save / same spot.

| | Tester sentence |
|--|-----------------|
| **M-PASS** | “On the monitor the lake looks **closer to N64** — not a VR cardboard sheet. If monitor is also murky, say so (then SKYMESH C-default / fill, not stereo, is in play).” |
| **M-FAIL** | “Monitor matches the headset cardboard.” (then WATERTILE must also be proven on flat, or SKYMESH is the common fault.) |

**Run W — falsifier ON:** same headset boot plus `set GETV_VR_WATERTILE=1` (scratch; **not** in ship boot).

| | Tester sentence |
|--|-----------------|
| **W-PASS 1** | “The lake has **depth**. Left and right eye do not share one painted quad. It is not a murky blue billboard.” |
| **W-PASS 2** | “Sky still looks like tonight’s SKYMESH. I did **not** grow a second sky under the dam.” |
| **W-PASS 3** | “Walking the bank, the horizon **meets the water**. It does not peel off as a screen rectangle.” |
| **W-PASS 4** | “Unset `WATERTILE` (or `=0`) **restores tonight**.” |
| **W-FAIL** | “Still flat.” / “Blue flash on the bridge (#72).” / “Sky died.” / “A blue plane floats at the wrong height.” / “Crates pop more (#29).” |

**Run T — #72 regression (every W run):** stand on the Dam **bridge / jump-strip**, turn around.

| | Tester sentence |
|--|-----------------|
| **T-PASS** | “No new blue **flash**. Same as tonight for #72. This knob did not become MTXGUARD.” |
| **T-FAIL** | “Turning flashes blue.” **Stop.** Do not KEEP-ON. File against #72, not as a #30 fix. |

**Other stages (one look, not a ship gate):** Surface / Runway (`IsWater` likely ON) with `WATERTILE=1` must not lose the **textured** water image.

---

## 6. Director decision

| If you say… | Then… |
|-------------|--------|
| **Green WATERTILE sit** | Workshop APPLY sketch §4. Keep default OFF. Sit N, M, W, T. |
| **Green layer 1 only** | Dam `!IsWater` fill → tiles. Leave `IsWater` stages on RDP until W-PASS. |
| **Want KEEP-ON** | Reject until W+T PASS on headset **and** M still looks like N64. Then — and only then — talk graduation. |
| **Monitor already matches N64, VR does not** | Stereo / eye-origin is the fault. WATERTILE per-eye is the right dig. |
| **Monitor also murky** | Sit `SKYMESH=0` on headset **without** WATERTILE (KEEP dig `=0`). If lake returns, SKYMESH leaked into horizon; fix that seam before tiling. |
| **It’s the room mesh, not the fill** | Layer 2. New sit. Do not KEEP `PROPFOGW`. |
| **Fix #72 in this PR** | Rejected. Separate issue. |
| **Reject** | Stop. Leave wipe. Zip stays honest: Dam water can look flat or murky. |

---

## 7. Attribution / legal

- No personal credit paths edited.
- No GoldenEye ROM, assets, or dumps.
- Decomp cites are map-only (`n64decomp/007` `sky.c` / `bgfog.c`).
- Workshop C stays private until release policy flips.
