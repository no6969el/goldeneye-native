# RESULT — ammo HUD picture distortion in VR (GEVR #34) (DIG ONLY)

**Status:** DIG. **Not APPLY READY on this public tree.** No C landed.
**Ask:** Combat ammo HUD **picture** (side stamp / icon) looks stretched / fat, or shows graphical errors, in VR. **Not** double image, **not** screen-edge clip, **not** overlap. Placement already moved when the HUD moved.
**Later (parked):** diegetic side-stamp on the gun — **Approach A quads**. Do **not** relocate until this picture renders clean.
**Constraints:** Do not merge into #33 (HUD text depth) or #58 (watch highlight one-eye). Do not sit Approach A. `GETV_STEREO_HUDGATE` / `GETV_TEX32BE` stay KEEP-ON unless chair A/B says otherwise.
**Date:** 2026-09-20.
**Evidence:** public `goldeneye-native` HEAD, public `no6969el/GEVR` #34 + packaging (vr441 boot), public `n64decomp/007` ammo draw, GEVR textbook `docs/231`–`232` / `312` (Fast3D texrect + aspect). Workshop `gfx_pc.c` **bodies** (`import_texture_rgba32`, `gfx_draw_rectangle` `kx`/`ky`) are **not on any public remote**.

Director can green-light a Fast3D picture fix, reject, or send the chair RECTPROBE from this page alone.

---

## Verdict (one screen)

| Question | Answer |
|----------|--------|
| What draws the picture? | Combat **`gSPTextureRectangle`**, not a 3D quad. `generate_ammo_total_microcode` → `microcode_generation_ammo_related` → `texSelect` + `draw_textured_rectangle`. |
| Why not glyphs / numbers? | Numbers are I8 font texrects (`dtdy=+1024`). The **icon** is tiny **RGBA32** (5×12 9mm, 5×28 rifle, …) with **`flipY` so `dtdy=0xFC00`**. GEVR `232` exonerated the I8 glyph path. This is the other texrect class. |
| Double image / clip / overlap? | **Out of scope.** #34 says those were stale after the HUD move. `GETV_VR_AMMOHUD_PAD` / `TEXTBAND_*` are **placement** (wiped in boot). Wrong layer for fat pixels. |
| Approach A gun quads? | **Later. Parked.** Picture first. |
| Root (fat) | **Dest X.** Fast3D `gfx_draw_rectangle` maps 10.2 X through window/eye aspect (`gfx_adjust_x_for_aspect_ratio` / GETV `kx`) while Y stays native 4:3. HMD eyes are ~1:1 → X stretches ~33%. A 5-px-wide stamp reads **fat**. |
| Root (speck / garbage) | **RGBA32 line treated as 16-bit.** Game `texSelect` 32b uses `line=(width+3)>>2`. Fast3D `import_texture_rgba32` uses `width=line_size_bytes/2`. 5-wide 32b becomes an 8-wide (or 4-wide) GL texture with sheared rows. |
| vr442 ship? | **#34 still OPEN.** Sep 19 docs teased a chair fix; vr442 RELEASE-NOTES do **not** list it. |
| APPLY tonight? | **No from this repo.** Picture fix is workshop Fast3D. Hide/PAD/relocate will not un-fat the stamp. |

```
combat HUD
  bondview2 maybe_mp_interface
    → generate_ammo_total_microcode          (gunfire.c)  viewport X/Y
      → microcode_generation_ammo_related    RGBA32 tile + black fill
        → texSelect 32b  line=(w+3)>>2       16-bit line formula
        → gSPTextureRectangle  dsdx=0x400, dtdy=0xFC00 (flipY)
          → gfx_dp_texture_rectangle
            → gfx_draw_rectangle  two tris + dest X aspect
              → import_texture_rgba32  width=line_bytes/2
fat / speck  → dest X stretched on ~square eye  AND/OR  5px 32b uploaded as 8
```

---

## 1. Root finding (files + functions)

### 1.1 Evidence boundary

| Layer | Where | What it proves |
|-------|--------|----------------|
| Public `goldeneye-native` | this repo | KEEP getenv stubs (`GETV_STEREO_HUDGATE`, `GETV_TEX32BE`). **Not** the playable GETV tree. |
| Public `no6969el/GEVR` | issue #34, `packaging/templates/gevr-vr441-boot.cmd`, `KEEP-DEFAULTS-INVENTORY-vr441.md`, `docs/231` `232` `312` | Symptom split (render vs clip), wiped `GETV_VR_AMMOHUD_PAD`, Fast3D texrect forensics, aspect term on X. |
| Public decomp | `n64decomp/007` | Exact icon draw math, formats, sizes. |
| Workshop (private) | `F:\Projects\GEVR\GoldenEyeVR\goldeneye-native` | Live `gfx_pc.c` `import_texture_rgba32`, `gfx_draw_rectangle` `kx`/`ky`, any `GETV_VR_AMMOHUD*` C. **Do not push.** |

First chair grep: `import_texture_rgba32`, `gfx_dp_texture_rectangle`, `GETV_VR_AMMOHUD`, `generate_ammo_total_microcode`.

### 1.2 Game emit — combat picture (not watch, not numbers)

**Call:** `bondview2.c` `maybe_mp_interface` → `generate_ammo_total_microcode`.

**Icon:** `gunfire.c` `microcode_generation_ammo_related` (~5787). Point filter, no LUT, **`gDPFillRectangle` black backing in whole pixels**, then `texSelect` + `display_image_at_position` → `bondwalk2.c` `draw_textured_rectangle` → **`gSPTextureRectangle`**.

Combat dest (NTSC, right hand):

- X = `(c_screenleft + c_screenwidth) - rightx` (`rightx` 59 / 43 / 127 / 109).
- Y mode `-1` → bottom of view `- 20` + `IconYOffset`.
- `halfed = (width/2, height/2)` so **1 texel = 1 dest pixel**, `dsdx=0x400`.
- **`flipY=1`** → `t=((height-1)<<5)`, **`dtdy=0x10000-0x400=0xFC00`**.

Watch `gunDrawWatchAmmoDisplay` uses the **same** icon helper at a fixed (200,180). #34 is **combat HUD**, not the watch page.

**Do not confuse with numbers:** `gunDrawHudInteger` → gothic I8 glyphs. Those can look fine while the stamp is fat.

### 1.3 The assets (why this picture, not all HUD)

`assets/oddtextures.c` + `gun.c` `ammo_related[].IconImage`. Almost every stamp is **`G_IM_FMT_RGBA` + `G_IM_SIZ_32b`**, clamp, **odd / tiny widths**:

| Stamp | W×H | Format | Worst for |
|-------|-----|--------|-----------|
| 9mm / golden | **5×12** | RGBA32 | fat + line error |
| rifle | **5×28** | RGBA32 | same, taller |
| shotgun / knife | 6×20 / 6×24 | RGBA32 | line still 16b |
| magnum | 5×15 | RGBA32 | |
| rocket / tank | 7×22 | RGBA32 / **IA8** | tank is the format A/B |
| mines / grenade | 14×14 / 14×18 | RGBA32 | closer to even; less line-skew |
| GL | 8×21 | RGBA32 **S wrap** | wrap + wrong width → edge specks |

`texSelect` (`othermodemicrocode.c`) **32b and 16b share** `line = (width + 3) >> 2`.

N64 `SetTile.line` is 64-bit words per row. RGBA32 needs `ceil(width/2)`. Width 5 → **3**, game emits **2**. Row stride 16 bytes vs 20 bytes of texels.

### 1.4 Fast3D — where the picture is actually drawn

GEVR `231`–`232` (menu glyphs, **I8**): `gSPTextureRectangle` → `gfx_dp_texture_rectangle` → `gfx_draw_rectangle` → two `gfx_sp_tri1`. Glyph sample: `dsdx=1024 dtdy=1024 flip=0`, `fmt=I siz=8b`. **That path is clean.** Ammo is the other tile class.

Public Fast3D (Emill / sm64-port; GETV is this family, bodies private):

**Dest (`gfx_draw_rectangle`):** 10.2 → NDC with native 320×240 (GETV: `kx`/`ky`, native can be 440×330), **then X × `gfx_adjust_x_for_aspect_ratio`**. GETV `312`:

```c
x * (ge_effective_native_width() / gfx_native_height)
  / (gfx_current_dimensions.width / gfx_current_dimensions.height)
```

Widescreen off → `(4/3) / (W/H)`. In XR, `gfx_current_dimensions` is the **eye swapchain** (~1:1), not a 4:3 window → **`a ≈ 1.33` → fat**. 3D gets the same X term (`312` H19); a 5×12 stamp has no perspective to hide it.

**Upload (`import_texture_rgba32`):** `width = line_size_bytes / 2` (16-bit bytes/pixel). With game `line=2` → 16-byte stride → GL width **8**. Dest rect is **5** px. UVs from `dsdx=1024` span 5 of those 8, or the uploader walks 8×4 bytes per row through 5×4 packed data → **shear / white specks**.

**`dtdy` sign:** ammo `0xFC00` is **s16 −1024**. If workshop `gfx_dp_texture_rectangle` keeps `dtdy` as `uint32` from `C1(0,16)`, V runs ~63 texels/pixel → wild smear. Glyphs never hit this (`dtdy=+1024`). RECTPROBE on **combat** (not the legal screen) is the discriminator.

### 1.5 What is already *not* the bug

| Item | Why it is the wrong layer |
|------|---------------------------|
| `GETV_VR_AMMOHUD_PAD` | Wiped DIG_OFF with `TEXTBAND_PAD` / `TEXTBAND_X`. **Inset / move.** #34: placement already fixed. |
| `GETV_VR_MSGSCALE` | Message scale (C default ~50). Not the stamp texel. |
| `GETV_STEREO_HUDGATE=1` KEEP | Per-eye HUD gate. **Double-image / one-eye** class (#58). A/B it; do not C-default OFF for fat pixels. |
| `GETV_XR_PLAY_EYERECT` / `SRCRECT=full` / SS3 | Picture KEEP. Whole-frame blit would fatten **numbers too**. Testers call out the **icon**. |
| `hudShiftPixels` / `GE_VR_HUD_DEPTH` | Recomp-era **duplication** (`170`/`173`/`192`). #34 excludes double image. |
| Approach A quads on the gun | **Later.** Same RGBA32 sampled on a gun quad will still be fat/specked if B/C still fire. |

---

## 2. Same as #33 / #58? — **No.**

| | #34 ammo picture | #33 HUD text depth | #58 watch highlight |
|--|------------------|--------------------|---------------------|
| Symptom | Stamp **fat / speck / garbage** | Text too close / hard to fuse | Highlight **one eye** |
| Primitive | RGBA32 **texrect** | I8 glyphs / textband | Menu highlight draw |
| Knob family | (none for picture) | `TEXTBAND_*`, depth | HUDGATE / scissor |
| Chair | RECTPROBE combat icon | Depth / band | Per-eye highlight |

Do **not** APPLY a text-depth or HUDGATE wear as the #34 fix unless RECTPROBE says dest X is fine and the tile upload is fine.

---

## 3. Hypothesis table

| # | Hypothesis | Result |
|---|------------|--------|
| A | Dest X aspect on texrect tris (`gfx_draw_rectangle` / `kx` / `gfx_adjust_x_for_aspect_ratio`) vs ~square eye | **Best fit for “fat”.** VR-only. 5-px stamps show it; 14-px mines less so. |
| B | RGBA32 `line` + `import_texture_rgba32` 16-bit width | **Best fit for speck / shear.** Unique to these icons vs I8 glyphs (`232`). |
| C | Unsigned `dtdy=0xFC00` on flipY | **Chair-cheap.** Only ammo (and other flipY HUD). Glyphs `dtdy=+1024`. |
| D | `GETV_VR_AMMOHUD_PAD` / TEXTBAND | **Falsified as picture.** Placement family; wiped in vr441 boot. |
| E | HUDGATE double image | **Excluded by #34.** Still A/B once so we do not miss a blit stretch. |
| F | `GETV_TEX32BE` byte-swap | **Colors**, not aspect. Already KEEP ON. Dig `=0` only as A/B. |
| G | Relocate to gun (Approach A) | **Parked.** Does not un-fat the sample. |

A and B can **both** be live. RECTPROBE one 9mm rect decides the first APPLY line.

---

## 4. KEEP / boot (do not flip for luck)

Public GEVR still checks in `gevr-vr441-boot.cmd`. vr442 Latest notes do **not** close #34.

| Knob | Public boot / C unset | Role vs #34 |
|------|------------------------|-------------|
| `GETV_STEREO_HUDGATE` | **ON** | Per-eye HUD. Dig `=0` A/B only. |
| `GETV_TEX32BE` | **ON** | 32-bit swizzle. Dig `=0` A/B only. Do not C-default OFF (VFX). |
| `GETV_XR_PLAY_EYERECT` | **ON** | Picture KEEP. Leave on for first sit. |
| `GETV_XR_PLAY_SRCRECT` | `full` | Picture KEEP. |
| `GETV_SUPERSAMPLE` | `3` | Makes 5-px bilinear fat/soft. A/B `1` after RECTPROBE. |
| `GETV_VR_AMMOHUD_PAD` | **wiped** | Placement. Not the picture. |
| `GETV_VR_TEXTBAND_PAD` / `_X` | **wiped** | Text band, not the stamp. |
| `GETV_VR_MSGSCALE` | wiped; C ~50 | Messages. |

---

## 5. APPLY sketches — **NOT LANDED**

Workshop only (`getv/port/fast3d/gfx_pc.c`). Confirm live names before typing.

### 5.1 Instrument first (no behaviour change)

`GETV_RECTPROBE=1` already exists (`231` §4). Sit it on **combat HUD**, not the front end.

Need one 9mm (or KF7) sample:

- dest `px` W×H vs table 5×12 / 5×28
- `ndc` width/height vs 5:12 (fat if X/Y ≫ 5/12 after aspect)
- tile `fmt siz line` — expect RGBA32; `line=2` for width 5
- upload W×H — **PASS B if upload W ≠ 5**
- `dtdy` — **PASS C if 64512 / 0xFC00 used unsigned**

### 5.2 Picture fix (smallest C, after the probe)

**If B:** `import_texture_rgba32` width from **tile size** (`uls`/`lrs`), not `line_size_bytes/2`. Do not “fix” game `texSelect` 32b `line` first — that is N64-authored; Fast3D must interpret GE’s line.

**If A:** skip window/eye aspect on **2D rectangles** when the target is an XR eye (or map texrect X with the same pixel aspect as Y). Do **not** patch `gfx_adjust_x_for_aspect_ratio` for 3D as the #34 fix (`312` / `314` forbade starting H19 at session-end).

**If C:** pass `dtdy` as **`int16_t`** into the UV math (Emill already does).

**If A+B:** import first (speck), then dest X (fat). One sit each.

No new KEEP knob until a sit PASSes. Optional wipe-only: `GETV_VR_AMMOHUD_ASPECT=0` only if the chair must A/B dest X without a rebuild.

### 5.3 Out of scope (this dig)

- Approach A / B diegetic quads on the gun or watch
- `GETV_VR_AMMOHUD_PAD` as the fix
- HUDGATE / HUD_DEPTH / hudShift as the fix
- C-default OFF for TEX32BE or HUDGATE
- Boot allowlist / pack smoke until picture sit PASS

---

## 6. Chair stare (plain tester sentences)

**Setup:** vr441-class or Latest zip. Combat, ammo display ON. **PP7 / KF7** (5-wide RGBA32). Recenter. Compare the **icon**, not the numbers.

**Run 0 — RECTPROBE (dev):** `GETV_RECTPROBE=1`. One combat frame with a gun out. Capture the ammo texrect line (fmt/siz/line, px, ndc, dtdy, upload size).

| | Tester sentence |
|--|-----------------|
| **P-PASS 1** | “The 9mm / rifle **picture** is the same proportion as on a CRT / the monitor bat: a **skinny** stamp, not a fat blob.” |
| **P-PASS 2** | “No white specks, shear, or wrap junk on the stamp. The black backing matches the picture.” |
| **P-PASS 3** | “The **numbers** beside it were already readable; they did not change. This was the **icon**.” |
| **P-PASS 4** | “Dual-wield: left stamp is the same picture, mirrored, not a second stretched copy.” |
| **P-FAIL** | “Still fat.” / “Specks.” / “Numbers fat too” (then it is whole-HUD blit, not this texrect). / “Double image” (#34 excluded — file stereo, don’t call it this). |

**A/B (only after Run 0, one at a time):**

| Run | Knob | If PASS | If FAIL |
|-----|------|---------|---------|
| T | tank cannon (IA8 7×22) vs PP7 | B is RGBA32-specific | A (both fat) |
| H | `GETV_STEREO_HUDGATE=0` | blit/gate stretch | leave KEEP ON |
| 32 | `GETV_TEX32BE=0` | swizzle/speck | leave KEEP ON |
| M | `Play-on-monitor.bat` | dest is XR eye aspect | import/dtdy (both fat) |
| S | `GETV_SUPERSAMPLE=1` | bilinear fat/soft only | dest/import still wrong |

**Do not sit:** gun-side quads, PAD values, TEXTBAND, `#33` depth.

---

## 7. Director decision

| If you say… | Then… |
|-------------|--------|
| **Green picture after RECTPROBE** | Workshop 5.2 on the arm the probe named (B import, A dest X, C `int16` dtdy). Sit P-PASS 1–4. |
| **Green both A and B** | Import first, then dest X. Two sits. |
| **Want Approach A on the gun** | **No until P-PASS.** Relocating a bad sample does not close #34. |
| **Want PAD / TEXTBAND** | Rejected as picture fix. Placement is done. |
| **Reject** | Stop. Leave vr441/vr442 fat stamps. #34 stays open. |

---

## 8. Attribution / legal

- No personal credit paths edited.
- No GoldenEye ROM, assets, or dumps.
- Decomp paths are map-only (`n64decomp/007`). Fast3D line citations are public Emill/sm64-port + GEVR textbook; workshop bodies stay private.
- GEVR: https://github.com/no6969el/GEVR/issues/34
