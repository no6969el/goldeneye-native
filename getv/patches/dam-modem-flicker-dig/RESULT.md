# RESULT — Dam convert-modem texture flicker (#70) (DIG ONLY)

**Status:** DIG. **Not APPLY READY.** No C landed.
**Ask:** After Bond attaches the covert modem on Dam 007, looking toward that modem makes its texture flicker — even from downstairs under the towers (view-direction / frustum, not standing next to the prop).
**Constraints:** Do not merge into #55. Not Dam crate pop (#29). KEEP-ON explosion/tex knobs stay ON unless chair proves otherwise. New chair knob (if any) defaults **OFF**.
**Date:** 2026-09-20.
**Evidence:** public `goldeneye-native` HEAD, public `no6969el/GEVR` packaging + #70/#55 comments, public `n64decomp/007` Dam setup + monitor draw. Workshop `gfx_pc.c` **bodies** (what TEXINVAL actually does past the getenv) are **not on any public remote**.

Director can green-light APPLY, reject, or send the chair A/B from this page alone.

---

## Verdict (one screen)

| Question | Answer |
|----------|--------|
| Root | **PROPDEF_MONITOR draw + `texSelect`, gated by frustum `PROPFLAG_ONSCREEN`.** Dam 007 attach sticks `ITEM_BUG` onto tagged `PROP_MODEMBOX` (the connection screen). That screen runs `monAnim05GreenTextUp` and mutates UVs **during draw**. |
| Same as #55? | **Separate.** Do not merge. #55 DIG is portal / `HEAD_TRANSLATE` room drop (full black). This is a monitor quad / tex load when that object is in the eye. Family resemblance only: “special prop in view after place.” |
| Same as #29? | **No.** Crates are `PROPFOGALPHA` + `OCCLSKIP`. |
| TEXINVAL / VFX KEEP the cause? | **Unproven.** Those knobs **are already ON** in public vr442-class boot / C-default. They are documented as **explosion / fire**, not monitors. Chair A/B them to `0` — do **not** flip C-default OFF (purple explosions). |
| Per-eye / stereo? | **Strongest source-proven.** `GETV_STEREO_REBUILD` KEEP ON. `MonitorRecord` is not per-eye; `process_monitor_animation_microcode` ticks scroll during each eye’s draw. |
| Overlay / objective HUD? | **Falsified.** After attach, AI only sets bit `0x00010000` and plays SFX. No `tv_change_screen_bank` on tag 5. The “screen” is the monitor mesh. |
| APPLY tonight? | **No.** Chair first. Smallest C (if chair confirms stereo tick) is **not** a KEEP flip. |

```
Dam 007 spawn  → ITEM_BUG in inv (renamed “covert modem”)
place/stick    → PROP_CHRBUG embeds on tag 5 PROP_MODEMBOX
look that way  → prop ONSCREEN → process_monitor_animation_microcode
                 → texSelect(IMGTEXT, FIXED_MONITOR mode 8)
                 → stereo eye 2 mutates the same MonitorRecord
flicker        → UV/tex cache mismatch and/or TMEM reload in frustum
```

---

## 1. Root finding (files + functions)

### 1.1 Evidence boundary

| Layer | Where | What it proves |
|-------|--------|----------------|
| Public `goldeneye-native` | this repo | KEEP getenv stubs (`getv/port/fast3d/gfx_pc.c`), `PmodemboxZ` host blob, vr441-class KEEP list. **Not** the playable GETV tree. |
| Public `no6969el/GEVR` | `packaging/`, `docs/ship-feature-checklist.md` | vr441 boot still the public template; TEXINVAL / TEXDLRETAG / VFXTMEM / VFXSHIFT / HEAD_TRANSLATE / STEREO_REBUILD / DRAWALL **ON**. vr442 notes do **not** claim a #55 or #70 wear. |
| Public decomp | `n64decomp/007` | Dam attach, monitor draw, texSelect. |
| Workshop (private) | `F:\Projects\GEVR\GoldenEyeVR\goldeneye-native` | Real `ge_vr_texinval()` **call sites**. **Do not push.** |

### 1.2 Attach (setup / prop / objective)

**Inventory:** Dam intro `StartWeapon` 47 is `ITEM_BUG`. Rename command index 320 maps item 47 to `LdamE` “covert modem” strings.

```
n64decomp/007 assets/obseg/setup/UsetupdamZ.c
  intro StartWeapon 47          → ITEM_BUG
  PROPDEF_RENAME index 320      → 47, strings 11292–11296 (covert modem)
```

**Target:** Tag 5 → SingleMonitor index 290.

| Field | Value |
|-------|--------|
| Type | `PROPDEF_MONITOR` (10) |
| Model | **335 `PROP_MODEMBOX`** — comment: “Covert Modem Connection Screen” |
| Pad | 10057 (PAD3D) **{3408, -10, 465}** tower, not downstairs |
| Flags | `0x10001002` = `PROPFLAG_FIXED_MONITOR` + Absolute Position + setup “in air” |
| `ImageNum` | **5** → `monAnim05GreenTextUp` |

**AI (`ai_20` in the same file):**

```
if_item_is_attached_to_object(0x2f, 0x05, 0x0a)
  0x2f = 47 = ITEM_BUG
  0x05 = tag 5 = PROP_MODEMBOX
→ objective_bitfield_set_on(0x00010000)
→ text “Covert modem installed.”
→ sfx_emit_from_object(..., tag 5)
```

Wrong place: `if_item_is_stationary_within_level(0x2f)` → “Covert modem incorrectly installed.”

**Place / stick (not a console “use” HUD):**

| Step | File | Function |
|------|------|----------|
| Fire gadget | `src/game/gunfire.c` | `ITEM_BUG` → `generate_player_thrown_object` |
| Spawn world prop | `src/game/gun.c` | `ITEM_BUG` → **`PROP_CHRBUG` (245)** “Covert Modem / Tracker Bug” |
| Stick | `src/game/propobj.c` ~4603 | `canEmbed = TRUE` for BUG / mines / plastique / microcamera |

Downstairs under the towers is **y ≈ -144**. The dish screen is **y ≈ -10**. Looking up puts tag 5 in the eye frustum. That matches the owner add on #70.

### 1.3 Draw / tex state when the modem is in view

`chrprop.c` `chraiUpdateOnscreenPropCount`: only `PROPFLAG_ENABLED | PROPFLAG_ONSCREEN`.

`propobj.c` `sub_GAME_7F04AC20` (draw, only if `PROPFLAG_ONSCREEN`):

- `PROPDEF_MONITOR` + `mrData->flags & 1`
- `PROPFLAG_FIXED_MONITOR` → `mN = 8` (else 1)
- **`process_monitor_animation_microcode(..., mN, 1)`**
- ends in **`texSelect(&gdl, tconfig, arg5, arg4, 2)`**

`monAnim05GreenTextUp`: `MONUSEIMAGE(IMGTEXT)` + looping `MONVERTSCROLL`. `MonitorRecord` (`offset`, `xmid`/`ymid`, colour) is **mutated in the draw function** with `MONITOR_TIMER_DELTA` (`g_GlobalTimerDelta`).

That is why it is **view-direction / frustum**: no ONSCREEN → no `texSelect` for that screen. Distance to the prop does not matter.

**Explosion path (different `texSelect` mode):** `explosion.c` uses `texSelect(..., genericimage, 4, 1, 2)` for scorches. KEEP inventory calls TEXINVAL “Explosion / fire texture path.” Monitor uses arg2 **1 or 2**, arg3 **8**. Same neighborhood (texSelect / gfx_pc), **not the same mode**.

### 1.4 Why “starts as soon as placed”

`PROP_MODEMBOX` **already exists at load** with GreenTextUp. Attach does **not** swap the anim (no `tv_change_screen_bank` on tag 5). What **is** new is **`PROP_CHRBUG` embedded on that monitor**.

Chair must split that (section 4). Two live sources after place:

1. Parent monitor quad (was already there).
2. Child `PROP_CHRBUG` (new), drawn when the parent is ONSCREEN.

If downstairs `PROP_TV1` TVs (same skeleton + same ImageNum 5, pads ~y=-144) already flicker **before** place, attach is when the tester first stared at a monitor, not a new mechanism.

---

## 2. Same as #55? — **No. Separate ticket.**

| | #70 Dam modem | #55 Facility tanks / Bunker |
|--|----------------|------------------------------|
| Symptom | Object **texture** flicker / direction flash; **not** a solid blackout | Screen goes **black** |
| Prop | `PROP_MODEMBOX` monitor + `ITEM_BUG` | `PROP_GASTANK` (117) bottling tank — **not** a monitor. Bunker = rooms |
| DIG on file | This page | Issue #55: body vs `HEAD_TRANSLATE` eye; eye leans past portal; room dropped |
| Chair A/B named there | — | `GETV_XR_HEAD_TRANSLATE=0`; do not disable DRAWALL / PORTALWIDE |
| vr442 notes | Still open on Latest | Owner said tanks/Bunker “in the cut”; **vr442 RELEASE-NOTES do not list it** |

Do **not** APPLY the #55 EYEROOM / head-translate wear as the #70 fix unless chair Run 0 PASSes (flicker dies with `HEAD_TRANSLATE=0` on Dam). Even then, keep the issues split: Dam 007 reviewers will still need a modem stare.

#29 leftover: `GETV_VR_PROPFOGALPHA=0` + `GETV_VR_OCCLSKIP=1` (C-default after KEEP graduation). vr441 **boot still wipes** `OCCLSKIP=`. Unrelated to this flicker.

---

## 3. Hypothesis table (Director’s four)

| # | Hypothesis | Result |
|---|------------|--------|
| 1 | Special prop / VFX / TMEM after place (TEXINVAL / TEXDLRETAG / VFXTMEM / VFXSHIFT) | **Open, chair-cheap.** Path is `texSelect` on a **monitor**, not explosion mode 4. Those four knobs are **already KEEP ON** in vr442-class boot **and** C-unset. Flicker exists **with them armed**. Turning one to `0` is the A/B; **do not change C-default.** |
| 2 | Per-eye / stereo invalidation when object in frustum | **Best source fit.** `GETV_STEREO_REBUILD=1` KEEP. Shared `MonitorRecord` ticked in each eye’s `process_monitor_animation_microcode`. TEXDLRETAG can retag DLs if hashes move. |
| 3 | Screen-space overlay tied to modem objective | **Falsified** as HUD/overlay. Objective bit + SFX only. The flicker **is** the connection-screen quad (and/or stuck bug) in the frustum. |
| 4 | Same root as #55 | **Falsified** as same function. Chair Run 0 still required so we do not miss a Dam-specific portal on that tower sightline. |

---

## 4. KEEP knobs already armed (public vr442 interact)

Public GEVR still checks in `gevr-vr441-boot.cmd`. C-defaults in this repo match KEEP-ON for the tex/VFX stack (`getv/patches/keep-defaults-on/MANIFEST.json`). vr442 Latest is the chair zip; notes do not add a modem or #55 line.

| Knob | Public boot / C unset | Role vs #70 |
|------|------------------------|-------------|
| `GETV_VR_TEXINVAL` | **ON** | Explosion/fire tex path. **Already on** during the bug. Dig `=0`. |
| `GETV_VR_TEXDLRETAG` | **ON** | DL retag after tex inval. Dig `=0`. |
| `GETV_VR_VFXTMEM` | **ON** | VFX TMEM. Dig `=0`. |
| `GETV_VR_VFXSHIFT` | **ON** | VFX shift. Dig `=0`. |
| `GETV_STEREO_REBUILD` | **ON** | Per-eye world rebuild. **Do not** turn off as first test (breaks stereo). |
| `GETV_XR_HEAD_TRANSLATE` | **ON** | #55 chair. Run 0 only. |
| `GETV_VR_DRAWALL` | **ON** | #55 said leave on. |
| `GETV_VR_PORTALWIDE` | **ON** (boot) | Same. |
| `GETV_TMEMMAP` | **0** (boot wipe) | Already off. |
| `GETV_VR_BLOODINVAL` | wiped empty | Not ship. Leave. |
| `GETV_VR_EYEROOM` | **not in public boot** | #55 phase 2. Not a #70 first knob. |
| `GETV_VR_PROPFOGALPHA` / `OCCLSKIP` | crate #29 | Do not touch for this stare. |

---

## 5. APPLY sketch (DIG ONLY — default OFF for chair)

**APPLY READY? No.** Need one Dam 007 sit. Workshop bodies are private.

### 5.1 Do not land

- Do **not** C-default `TEXINVAL` / `TEXDLRETAG` / `VFXTMEM` / `VFXSHIFT` to OFF (vr440 purple-explosion lesson).
- Do **not** merge with #55 EYEROOM.
- Do **not** wear FOGSKIP / DISTSKIP / PROPFOGW / FOVMATCH.

### 5.2 Chair A/B (no rebuild) — one change per run

Unset / empty = ship. Explicit `0` = dig.

| Run | Env | If flicker **dies** | If flicker **stays** |
|-----|-----|---------------------|----------------------|
| **0** | `GETV_XR_HEAD_TRANSLATE=0` | Treat as #55-class on that sightline. Do not start tex APPLY. | Portals not the Dam-modem root. Restore HEAD_TRANSLATE. |
| **1** | `GETV_VR_TEXINVAL=0` | Explosion-path inval is poisoning the monitor/bug. **Do not** ship that as default OFF. APPLY 5.3a. Check explosions still coloured. | TEXINVAL not sufficient. Restore. |
| **2** | `GETV_VR_TEXDLRETAG=0` | Stereo DL retag. APPLY 5.3a scoped to monitor `texSelect`. | Restore. |
| **3** | `GETV_VR_VFXTMEM=0` then `VFXSHIFT=0` | VFX TMEM. Same: scoped skip, KEEP stays ON. | Restore. Explosion/fire must still look like vr442. |

If 0–3 all **FAIL** (flicker remains, Dam picture intact): stereo **monitor tick**, not KEEP-OFF.

### 5.3 Smallest C (workshop only, after chair)

**5.3a — if Run 1 or 2 PASS:** In workshop `gfx_pc.c` (TEXINVAL / TEXDLRETAG call site), skip inval/retag when the `texSelect` is the monitor path (`arg2` 1/2 + `arg3` 8), not explosion `arg2==4`. **No new KEEP.** Optional chair pin `GETV_VR_MONINVAL=0` default OFF = current; `=1` = skip. Only if the sit needs a bat toggle.

**5.3b — if 0–3 FAIL (likely):** Tick `MonitorRecord` **once per sim frame**, not per eye.

```
/* vendor/ge-decomp/src/game/propobj.c */

process_monitor_animation_microcode(...)  /* mutates screen + emits gdl */

/* APPLY sketch — GETV_VR_MONFRAME default OFF (chair):
 * unset/0 = today (tick during each eye draw).
 * 1 = if this eye is not the first of the frame, skip the while(cmdlist)
 *     mutation; still texSelect + triangles with the already-ticked UVs.
 * Same gate in sub_GAME_7F04AC20 before the call.
 */
```

```c
static int ge_vr_monframe(void)
{
    static int on = -1;
    if (on < 0) {
        const char *e = getenv("GETV_VR_MONFRAME");
        on = (e != NULL && *e != '\0') ? (atoi(e) != 0) : 0; /* chair A/B; not KEEP-ON */
    }
    return on;
}
```

Files: `propobj.c` (`process_monitor_animation_microcode`, `sub_GAME_7F04AC20`). Optional getenv in workshop `stereo.c` / `gfx_pc.c` only if the eye-index helper already lives there.

**5.3c — if only the stuck bug flickers (Run B PASS, dish-before-place clean):** child `PROP_CHRBUG` draw when parent ONSCREEN. Smaller than 5.3b: skip far / second-eye child tex — still a sit, still default OFF.

### 5.4 Out of scope

- Turning off `GETV_STEREO_REBUILD` / `GETV_VR_DRAWALL`
- Dam crate fog, water, glass holes
- Facility tanks / Bunker (#55)
- Boot allowlist / pack smoke until a sit PASS

---

## 6. Chair stare — plain tester sentences

**Setup:** public **vr442** / Latest. `Start-GEVR.bat`. Dam, difficulty **007**. Recenter both sticks. Headset (say which). Do **not** set FOVMATCH.

**Control (before placing):**

| | Tester sentence |
|--|-----------------|
| **C-PASS 1** | “Downstairs TVs under the towers look **steady**. Scrolling green text does **not** flash the picture.” |
| **C-PASS 2** | “I look **up at the dish / connection screen** from downstairs **before** I place anything. **No** flicker.” |
| **C-FAIL** | “Those TVs or the dish already flash before I attach the modem.” → treat as **all Dam monitors**, attach is not the trigger. Still this ticket, still 5.3b. |

**Main (place, then look):**

| | Tester sentence |
|--|-----------------|
| **M-PASS** | “After I attach the covert modem, I go downstairs under the towers and look toward that dish. The picture stays **steady**. No flash on that screen.” |
| **M-FAIL** | “As soon as it is stuck, looking that way **flickers** the modem / flashes the view — even from downstairs.” (this is tonight’s bug) |

**Knob runs** (only if M-FAIL). Restore the previous knob before the next.

| Run | Sentence if it **fixed** it |
|-----|------------------------------|
| **0** `HEAD_TRANSLATE=0` | “Flicker gone, like leaning stopped a black room.” → #55-class; stop tex APPLY. |
| **1** `TEXINVAL=0` | “Flicker gone. Explosions still look like vr442 (not purple).” |
| **1 FAIL-cost** | “Flicker gone but explosions went purple / died.” → 5.3a, do not ship TEXINVAL=0. |
| **2** `TEXDLRETAG=0` | “Flicker gone. Rest of Dam looks like vr442.” |
| **3** VFX pair `=0` | “Flicker gone. Fire / sparks still ok.” |
| **None of 0–3** | “Still flickers with all of those at 0.” → green-light **5.3b `MONFRAME`** (default OFF). |

**Regression every run:** Dam crates still as vr442, water as now, gun aim, no full-screen black when **not** looking at the dish.

Headset / OpenXR / SteamVR on or off / HMD vs monitor — write them on the sit. No ROM.

---

## 7. Director decision

| If you say… | Then… |
|-------------|--------|
| **Green chair only** | Sit C + M on vr442. Then Runs 0–3 if M-FAIL. No C. |
| **Green 5.3b if 0–3 FAIL** | Workshop `MONFRAME` default OFF. Sit M with `=1`. |
| **Green 5.3a if TEXINVAL=0 PASS without purple** | Scoped skip, KEEP TEXINVAL ON. |
| **Merge into #55** | Rejected by this dig unless Run 0 PASS. Keep issues split anyway. |
| **Flip TEXINVAL C-default OFF** | Rejected. |
| **APPLY tonight from this repo** | No. Public tree has getenv stubs only. |

---

## 8. Attribution / legal

- No personal credit paths edited.
- No GoldenEye ROM, assets, or dumps.
- Decomp citations are public `n64decomp/007` (setup + `propobj.c` / `gun.c` / `gunfire.c` / `chrprop.c` / `othermodemicrocode.c` `texSelect`).
- Workshop C stays private until release policy flips.
