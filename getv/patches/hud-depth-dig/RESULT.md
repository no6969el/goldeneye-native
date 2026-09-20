# RESULT — HUD text too close / wrong depth in VR (GEVR #33) (DIG ONLY)

**Status:** DIG. **Not APPLY READY on this public tree.** No C landed.
**Ask:** In-headset HUD / on-screen messages feel glued to the face or sit at the wrong depth, so they are hard to read. Parked polish knob `GETV_VR_HUD_DEPTH_PX` (default off in ship boot).
**Constraints:** Do not merge into #34 (ammo **picture** distortion) or #57 (arm watch panel). Do not flip KEEP `GETV_STEREO_HUDGATE` / `GETV_STEREO_AIMRECT` as a ship default. Do not KEEP-ON a pixel count. TEXTBAND / MSGSCALE stay wipe / C-default (XY + size, not depth).
**Date:** 2026-09-20.
**Evidence:** public `goldeneye-native` HEAD, public `no6969el/GEVR` textbook + vr441 boot / KEEP inventory + #33/#34, public `n64decomp/007` HUD draw, GEVR docs `170`/`173`/`191`/`192`/`231`. Workshop Fast3D **bodies** (what HUDGATE actually does past the getenv) are **not on any public remote** (`GEVR` `docs/RELEASE-POLICY.md`).

Director can green-light a metres sit, reject PX as the ship lever, or park from this page alone.

---

## Verdict (one screen)

| Question | Answer |
|----------|--------|
| Where is HUD depth? | **Still 2D TEXRECT / FILLRECT**, not a 2 m world quad. Game emits screen-space rects; Fast3D `gfx_draw_rectangle` turns them into two triangles. |
| What ships tonight? | **`GETV_STEREO_HUDGATE=1` KEEP** — per-eye **canting** so the HUD sits at **infinity** (both eyes agree). Rim pull-in is TEXTBAND (wiped → C default). Size is `GETV_VR_MSGSCALE` (C ~50). |
| Is `GETV_VR_HUD_DEPTH_PX` the right knob? | **Weak sit probe, bad ship falsifier.** Pixels do not travel across HMD / FOV / 320 vs 440 native. Public boot does **not even name it**. |
| Better falsifier | **`GETV_VR_HUD_DEPTH_M` (metres).** Unset / 0 = infinity (tonight). Convert to per-eye **native** px *inside* HUDGATE using live `eyeX` + `tanHalfWidth`. Sweep **2.0 → 1.0 → 0.5**. |
| Do **not** ship `0.2` | That number fused the **crosshair** (`192`, `-HudDepth 0.2`). It is nearer than the gun (~0.128 NDC, ~6× PD’s clamp). HUD **text** wants reading distance, not gun vergence. |
| Same as reticle / cinema? | **No.** `GETV_VR_RETICLE_M=5` is the 3D aim mark. `GETV_XR_PLAY_SCREEN=2` is the hub cinema quad. Do not retune those for #33. |
| Same as #34? | **No.** #34 is ammo **icon** stretch. #34 comment: placement was already fixed “when the HUD moved” (TEXTBAND). This ticket is **depth / vergence**. |
| APPLY tonight? | **No from this repo.** Path + formula are named. Workshop `gfx_pc.c` body is private. Sit first. |

```
game HUD (2D)
  maybe_mp_interface
    gunDrawSight              → sight / RETICLE_M  (not this ticket)
    generate_ammo_total_microcode → ammo stamp + numbers
    countdownTimerRender
    hudmsgBottomRender        → status / messages (textRenderOutlined)
  textRender / textRenderOutlined / draw_blackbox_to_screen
    gSPTextureRectangle / gDPFillRectangle

Fast3D
  gfx_dp_texture_rectangle / gfx_dp_fill_rectangle
    gfx_draw_rectangle        → NDC via kx/ky  (docs 231)
      HUDGATE=1  → per-eye canting  (infinity)
      DEPTH unset/0 → no extra convergence  (tonight = “too close or double while verged on the gun”)
      DEPTH_M>0  → extra −eyeX/D  (finite plane)
      DEPTH_PX   → raw extra pixels  (parked; HMD-fragile)
```

---

## 1. Root finding (files + functions)

### 1.1 Evidence boundary

| Layer | Where | What it proves |
|-------|--------|----------------|
| Public `goldeneye-native` | this repo | `ge_stereo_hudgate()` getenv stub in `getv/port/fast3d/gfx_pc.c`. **Not** the playable GETV tree. **No** `GETV_VR_HUD_DEPTH_PX` reader. |
| Public `no6969el/GEVR` | `packaging/`, `FEATURES.md`, docs `170`/`173`/`191`/`192`/`231` | Ship knobs, boot wipe list, historical metre depth, TEXRECT path. |
| Public decomp | `n64decomp/007` `bondview.c` / `textrelated.c` | Who emits the HUD GBI. |
| Workshop (private) | `F:\Projects\GEVR\GoldenEyeVR\goldeneye-native` | Real HUDGATE / rectangle-shift **call sites**. **Do not push.** |

`GETV_VR_HUD_DEPTH_PX` **does not appear** in public `goldeneye-native`, vr441 boot, KEEP inventory, or GEVR textbook. First chair grep: `getenv("GETV_VR_HUD_DEPTH_PX")` and `getenv("GETV_STEREO_HUDGATE")`. Expect the reader next to `ge_stereo_hudgate` / `gfx_draw_rectangle` in workshop `getv/port/fast3d/gfx_pc.c`. GEVR docs put other `geStereo*` helpers in workshop `vendor/ge-decomp/src/game/stereo.c`.

### 1.2 Game HUD stack (public decomp)

HUD is **not** a model. It is screen-space GBI, same as menus (`231`: every glyph is a `gSPTextureRectangle`).

| Function | File | What |
|----------|------|------|
| `maybe_mp_interface` | `bondview.c` | Combat HUD compositor. |
| `gunDrawSight` | `gunfire.c` | Gunsight. VR sight/reticle is **`GETV_VR_SIGHT2D` / `RETICLE` / `RETICLE_M=5`**, not #33. |
| `generate_ammo_total_microcode` | `bondview.c` | Ammo numbers + **picture**. Picture glitches = **#34**. |
| `countdownTimerRender` | bondview family | Timer overlay. |
| `hudmsgBottomRender` | `bondview.c` | Lower-left status / messages → `textMeasure` + `draw_blackbox_to_screen` + `textRenderOutlined`. |
| `textRender` / `textRenderOutlined` | `textrelated.c` | Glyphs → TEXRECT. In-level HUD uses outlined; front end is `textRender` only (`231`). |
| `draw_blackbox_to_screen` | text/menu path | FILLRECT behind text. **Must take the same X shift as the glyphs** or the box splits off the letters. |

`#ifdef GE_HOST_PORT` patches in this repo only fix pointer sign-extend at those sites. They do **not** add depth.

Architecture (`GE007-VR-ARCHITECTURE.md` §6.4 / `patches/DECOMP-PATCHES.md` §8) still wants a **head-locked quad at ~2 m**. GETV did **not** build that. It kept TEXRECT and stereo-gated it (`HUDGATE`).

### 1.3 Fast3D path (GETV)

`231`/`232` (worn GETV): `gfx_dp_texture_rectangle` → `gfx_draw_rectangle` → two `gfx_sp_tri1`. Rects have **no perspective matrix**. Placement is native px → NDC (`kx`/`ky`). That is why a **pixel** lever exists at all.

Public stub only:

```c
/* getv/port/fast3d/gfx_pc.c — getenv only; call site is workshop */
static int ge_stereo_hudgate(void)
{
    /* unset / empty = ON (KEEP). Dig sets 0. */
}
```

`GETV_STEREO_AIMRECT=1` KEEP is the sibling rectangle arm for aim sprites. Leave it **ON** during a #33 sit (do not confound HUD depth with a second rectangle gate).

**Do not shift full-width rects** (`173`: `coversScissorWidth` = framebuffer blit / fade / letterbox). HUD sprites are the complement. FILLRECT black boxes behind messages **are** HUD and **must** shift.

### 1.4 Two terms, already derived (docs `170` / `173` / `191`)

HUD sprites are **rectangles**, not ortho, not perspective. `170` wrote `[3][0]` on a matrix the HUD never used. `173` moved the lever to **`hudShiftPixels`** on the rectangle path. Sign: divergent = behind infinity (unfusable); parallel = infinity; extra inward = finite depth.

```
ndc_cant = -((tanR + tanL) / (tanR - tanL)) * sign     /* HUDGATE: infinity */
if (depth_m > 0.01)
    ndc += -(eyeX_m / depth_m) / halfW                 /* finite plane */
native_px = ndc * (native_width / 2)
```

`eyeX` from the runtime’s two eye poses, guarded ~`[0.01, 0.10]` m (PD’s guard). Zero depth = omit the second term.

| Term | GETV knob | Ship | Job |
|------|-----------|------|-----|
| Canting | `GETV_STEREO_HUDGATE` | **ON** | One fused HUD at **infinity** |
| Depth | parked `GETV_VR_HUD_DEPTH_PX` | **OFF / unnamed** | Extra pixels (fragile) |
| Depth (better) | **`GETV_VR_HUD_DEPTH_M`** | **unset = 0 = infinity** | Extra convergence in **metres** |
| Rim / XY | `GETV_VR_TEXTBAND_PAD` / `_X`, `GETV_VR_AMMOHUD_PAD` | **wiped** | Pull text off the HMD rim (`FEATURES.md`). **Not depth.** |
| Size | `GETV_VR_MSGSCALE` | C ~50 (wipe; dead name `GETV_MSGSCALE`) | Angular size. **Not depth.** |

`191`: canting alone is correct; residual doubling while looking at the gun is **vergence at infinity**. `192`: `-HudDepth 0.2` fused the **crosshair**. That is a **measurement of leftover convergence**, not a HUD-text preference (`174` AimTrim trap). `210` later taught: a parallax term must not silently override an explicit depth.

### 1.5 Public boot vs the parked PX knob

vr441 boot (still the public template; vr442 zip notes do not add a HUD-depth line):

| Knob | Boot | Class |
|------|------|--------|
| `GETV_STEREO_HUDGATE` | `1` | KEEP_SHIP |
| `GETV_STEREO_AIMRECT` | `1` | KEEP_SHIP |
| `GETV_VR_TEXTBAND_PAD` / `_X` / `AMMOHUD_PAD` | **wipe empty** | DIG / polish |
| `GETV_VR_MSGSCALE` | **wipe empty** (C ~50) | PLAYER_PREF |
| `GETV_VR_RETICLE_M` | `5.00` | PLAYER_PREF (aim mark) |
| `GETV_VR_SIGHTPX` | `6` | KEEP_SHIP (sight **size**) |
| `GETV_XR_PLAY_SCREEN` | `2` | KEEP (cinema quad) |
| **`GETV_VR_HUD_DEPTH_PX`** | **absent** (not even wiped) | parked / C 0 if the reader exists |

Issue #33’s “default off in ship boot” matches **unset → 0 extra pixels**, not a listed assign.

---

## 2. Validate `GETV_VR_HUD_DEPTH_PX`

| Test | Result |
|------|--------|
| Named in public boot / KEEP inventory / this repo? | **No.** |
| Unit matches the draw site? | **Yes, unfortunately.** Fast3D rects are native px (`173` / `231`). |
| Travels across headset / SS3 / 320 vs 440? | **No.** Same PX is a different plane on Pimax Crystal Super vs Quest 3, and on hi-res menus (`viSetXY(440,330)`). |
| Historical sit that actually fused? | **Metres.** `191` sweep `2.0 → 1.0 → 0.5`; wearer picked **`0.2` for the crosshair**, not for ammo text. |
| Sister knob already in metres? | **`GETV_VR_RETICLE_M=5`.** HUD should match that convention. |
| PD prior art | `vrComputeCrosshairParallax` is **impact-distance** for the **crosshair**, with a small NDC clamp. `192`: `0.2 m` is ~6× that max. Do not copy PD sources; map only. **Not** a HUD-text default. |

**Verdict:** If the workshop getenv exists, use PX **once** as a reader proof (Run P below). Do **not** KEEP-ON it. Do **not** put it on the boot allowlist. Replace with metres before any ship sit.

A PX value that “works” on one HMD is the AimTrim class of knob (`174`).

---

## 3. Better falsifier

**Primary sit knob (recommend):**

```
GETV_VR_HUD_DEPTH_M    unset / empty / 0 = OFF = infinity   (tonight; not KEEP-ON)
                       >0 = metres to the HUD plane
GETV_VR_HUD_DEPTH_PX   parked. Reader-proof only. Do not sit a sweep in PX.
GETV_STEREO_HUDGATE    KEEP ON. Dig 0 = path check only; restore after.
```

Convert at the HUDGATE rectangle site (workshop `gfx_pc.c`), not in decomp `textRender`:

```c
/* NOT APPLY READY — workshop sketch.
 * Same TU as ge_stereo_hudgate / gfx_draw_rectangle.
 * Skip full-framebuffer rects. Apply to HUD TEXRECT + message FILLRECT. */

static float ge_vr_hud_depth_m(void)
{
    static float d = -1.f;
    if (d < 0.f) {
        const char *e = getenv("GETV_VR_HUD_DEPTH_M");
        d = (e != NULL && *e != '\0') ? (float)atof(e) : 0.f; /* default OFF */
    }
    return d;
}

/* px = ndc * (native_w / 2)
 * ndc extra = -(eyeX_m / depth_m) / halfW     depth_m > 0.01
 * left eye: eyeX < 0 → extra shift is toward the right (inward)
 */
```

If `HUD_DEPTH_M` is a new name and PX already reads: **M wins when set**; else PX; else 0. Log once: `[getv] hud depth m=… px=… eye0=… eye1=…` (must **differ by a sign flip** — `170` §7). Do not add to `$requiredBootKnobs` until a sit PASS.

**Path falsifier (no new C):** `GETV_STEREO_HUDGATE=0`. If “too close” is **unchanged**, the complaint is not this path (size / rim / reticle / cinema). Restore `=1` before any depth sit.

**Not falsifiers for #33:**

| Knob | Why not |
|------|---------|
| `GETV_VR_TEXTBAND_PAD` / `_X` / `AMMOHUD_PAD` | XY inset. #34 already said placement moved. |
| `GETV_VR_MSGSCALE` | Size. “Hard to read” can be huge type, not depth. |
| `GETV_VR_RETICLE_M` / `SIGHTPX` / `SIGHT2D` | Aim mark. |
| `GETV_XR_PLAY_SCREEN` / cinema FOV | Hub quad (`175`). |
| `GETV_STEREO_AIMRECT=0` | Confounds a KEEP rectangle arm. |
| Diegetic ammo-on-gun / watch (#34 later, #57) | Different track. |

---

## 4. APPLY sketches — **NOT LANDED**

### 4.1 Do not land

- Do **not** KEEP-ON `HUD_DEPTH_PX` or `0.2` m.
- Do **not** C-default HUDGATE OFF.
- Do **not** retune `RETICLE_M` / TEXTBAND / MSGSCALE as the #33 fix.
- Do **not** build the architecture 2 m quad on this ticket (Tier 1 redesign). Extra convergence on the existing TEXRECT is the cheap sit.
- Do **not** drive HUD text from last-impact parallax (`B1` / U-06). That is the **crosshair**. HUD text at the wall you just shot will swim every trigger pull.

### 4.2 Smallest C (workshop, after chair)

**File:** workshop `getv/port/fast3d/gfx_pc.c` — `gfx_draw_rectangle` / HUDGATE call site. Optional helper in `stereo.c` only if eye tangents / `eyeX` already live there.

1. Confirm HUDGATE still applies canting (infinity) to HUD TEXRECT + message FILLRECT, not to full-width blits.
2. Add `ge_vr_hud_depth_m()` default **0**.
3. When `> 0.01`, add the second NDC term and convert to **native** px with the **same** `kx` the rectangle already uses (`231`: do not re-derive).
4. One banner / one log line. Sign must be opposite per eye.

**APPLY READY?** **No** from this repo. Need a vr442 sit (Runs H + D + M). Bodies are private. Do not land a stub getenv with no call site.

### 4.3 Out of scope

- Arm-attached watch (#57)
- Ammo picture stretch (#34)
- Boot allowlist / pack smoke until sit PASS
- Personal credit paths. ROM dumps

---

## 5. Chair stare — plain tester sentences

**Setup:** public **vr442** / Latest. `Start-GEVR.bat` (vr441-class boot). Recenter both sticks. Headset / OpenXR / SteamVR on or off — write them. Facility or Dam, **in combat** (ammo + a status message). Do **not** set `FOVMATCH`. Leave `HUDGATE=1`, `AIMRECT=1`, `RETICLE_M=5`.

**Split the complaint first.** “Too close” and “hard to read” are not the same sentence.

### Run H — path (HUDGATE)

Scratch **after** boot: `set GETV_STEREO_HUDGATE=0`. One launch. Then restore (unset or `=1`).

| | Tester sentence |
|--|-----------------|
| **H-PASS** | “With HUDGATE off, ammo / messages look **worse** (double, or stuck to one eye). With it back ON they **fuse into one** overlay.” → depth work is on this path. |
| **H-FAIL** | “Turning HUDGATE off does **not** change the glued-to-face feeling.” → stop. Not HUDGATE depth. Check MSGSCALE / TEXTBAND / cinema. |

### Run E — one-eye (what “wrong depth” is)

HUDGATE ON. Depth unset.

| | Tester sentence |
|--|-----------------|
| **E-PASS (infinity)** | “Close left, then right: the **same** ammo numbers sit in the **same world place**. Both eyes open, they fuse, but they feel **far / at the sky** while I look at my gun.” → canting is right; missing finite depth (`191`). |
| **E-DOUBLE** | “Each eye has the numbers in a **different** place; both open they will not fuse.” → canting/sign, **not** a depth sweep. Do not sit metres yet. |
| **E-NEAR** | “They fuse, and they sit **on my face** / I go cross-eyed to read them.” → extra convergence already too strong, or MSGSCALE huge. Do not add more PX. |

### Run P — PX reader proof (only if workshop getenv exists)

One extreme, then restore. Example (native px, **opposite** per eye by construction): `set GETV_VR_HUD_DEPTH_PX=8` vs unset.

| | Tester sentence |
|--|-----------------|
| **P-PASS** | “The numbers **moved in depth** (nearer or farther) when I set PX. Unset put them back.” → reader is live. **Stop using PX.** Go to Run M. |
| **P-FAIL** | “PX did nothing.” → knob is announced and disconnected (`80`). Do not sit a PX sweep. Metres APPLY (4.2) is owed before a depth sit. |
| **P-SIZE** | “They only got **bigger/smaller** or slid **sideways as a 2D sticker**, no depth.” → wrong site (TEXTBAND / MSGSCALE / full-width blit). |

### Run D — metres sweep (the real #33 sit)

HUDGATE ON. PX unset. After boot, **one value per launch**, halve never nudge (`191`):

```
set GETV_VR_HUD_DEPTH_M=2.0
set GETV_VR_HUD_DEPTH_M=1.0
set GETV_VR_HUD_DEPTH_M=0.5
```

Control (known too near, do **not** ship): `set GETV_VR_HUD_DEPTH_M=0.2`

| | Tester sentence |
|--|-----------------|
| **D-PASS** | “At **2 m or 1 m**, ammo and the bottom status line sit at a **comfortable reading distance**. I can look at the gun and the text still fuses. They are **not** glued to my face.” |
| **D-0.2** | “At **0.2** the text is **nearer than the gun** / I go cross-eyed. That matches the old crosshair number; **do not ship it for HUD.**” |
| **D-FAIL** | “No metre value fuses **and** reads comfortably.” / “Text depth moved but the **black box** did not.” / “The **reticle** or **cinema screen** moved.” |
| **D-SIZE** | “Depth is fine; the letters are just **huge or on the rim**.” → MSGSCALE / TEXTBAND, not this ticket. |

### Run R — regression (every launch)

| | Tester sentence |
|--|-----------------|
| **R-PASS** | “Squeeze ADS mark still on the **gun ray** at ~5 m. Hub cinema still a **screen in the room**. Gun aim / casings unchanged. Ammo **picture** no worse than #34.” |
| **R-FAIL** | “Reticle glued to my face.” / “Cinema billboard on my nose.” / “Ammo icon newly stretched.” |

Headset / runtime / SteamVR / bat name / which overlay (ammo, objectives, popup) — write them. No ROM.

---

## 6. Director decision

| If you say… | Then… |
|-------------|--------|
| **Green metres sit only** | Workshop already has HUDGATE. Chair Runs H, E, D on vr442. No C in this repo. |
| **Green 4.2 if P-FAIL / no PX reader** | Workshop `HUD_DEPTH_M` default 0. Sit D. Do not KEEP-ON. |
| **Ship PX / KEEP-ON 8 px** | Rejected. HMD-fragile. |
| **Ship 0.2 m as HUD default** | Rejected for **text**. That was crosshair fusion (`192`). Sit 2.0/1.0 first. |
| **Tune TEXTBAND / MSGSCALE as #33** | Rejected unless Run H-FAIL or D-SIZE. |
| **Move ammo onto the gun / watch** | #34 later / #57. Not this dig. |
| **Build the 2 m quad tonight** | Park. Bigger than extra NDC on TEXRECT. |
| **APPLY tonight from this repo** | **No.** Notes only. |

---

## 7. Attribution / legal

- No personal credit paths edited.
- No GoldenEye ROM, assets, or dumps.
- Decomp citations are public `n64decomp/007` (`bondview.c` HUD compositor, `textrelated.c` glyphs).
- PD HUD/crosshair numbers are **map-only** (public GEVR textbook `170`/`192`/`102`). Do not copy PD sources.
- Workshop C stays private until release policy flips.
