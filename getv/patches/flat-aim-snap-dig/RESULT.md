# RESULT — flat / Play-on-monitor aim snap-to-center (GEVR #73) (DIG ONLY)

**Status:** DIG. **Not APPLY READY on this public tree.** No C landed.
**Tracker:** [GEVR #73](https://github.com/no6969el/GEVR/issues/73) — canonical. Do not file a second issue.
**Ask ([GEVR #73](https://github.com/no6969el/GEVR/issues/73)):** On **`Play-on-monitor.bat`** (`GETV_STEREO=0`), pressing the **aim button** pulls aim **back to center**. Owner wants free aim: leave look/aim where you pointed. No recenter-on-ADS.
**Issue as filed:** *“Flat / monitor: aim button snaps aim back to center.”* Build **vr442 / Latest**. Headset may differ. Please-include on the issue: bat, mouse vs controller, short clip (no ROM).
**Headset:** may differ (controller aim). Dig **flat first**; note if VR shares the same recenter.
**Date:** 2026-09-20.
**Evidence:** public `goldeneye-native` HEAD (KEEP fragments, XR ABI, vr440 `port_input` snippet), public `no6969el/GEVR` packaging + textbook + **[#73](https://github.com/no6969el/GEVR/issues/73)**, public `n64decomp/007` `bondview2.c` / `gunfire.c` / `file2.h`. Workshop `port_input.c` / product `bondviewProcessInput` patches are **not on any public remote** (`GEVR` `docs/RELEASE-POLICY.md`).

Director can green-light bat pins, a look-ahead / auto-aim A/B, workshop C, or reject from this page alone.

---

## Verdict (one screen)

| Question | Answer |
|----------|--------|
| GitHub issue? | **[#73](https://github.com/no6969el/GEVR/issues/73)** (2026-09-20, owner). **Do not open a second.** Same-family: [#35](https://github.com/no6969el/GEVR/issues/35) / ads-grip DIG (Honey steals stick); textbook `00-STATE` item *“CROSSHAIR AUTO-CENTRES; aiming with the mouse fights it.”* |
| What yanks on AIM | **Stock Honey ADS.** Aim bit (`CONT_R` **or** `CONT_L`) sets `insightaimmode`. That flips `controldef` to **KISSY**, shows the sight, and feeds the **stick** into the gun/crosshair integrator. Stick at rest → integrator **decays to screen center**. |
| Look Ahead? | **Related, not the ADS edge.** `OPTION_LOOKAHEAD` is still in `DEFAULT_OPTIONS`. `automovecentreenabled` auto-levels **pitch** while walking. `canLookAhead = !insightaimmode` — Look Ahead is **off while ADS**, on in hip-fire. Pitch snap while walking is a different chair. |
| AUTOAIM / `GETV_AUTOAIM`? | **Headset boot pins `GETV_AUTOAIM=0`. Monitor bat does not.** Auto-aim is **disabled in ADS** (`canAutoAim = !insightaimmode`). Hip-fire assist can leave the gun off-axis; ADS then looks like a yank to center. New saves already dropped `OPTION_AUTOAIM` (vr440). Old EEPROM can still have it on. |
| XR leftovers on flat? | **Yes, after KEEP graduation.** Monitor bat sets `GETV_STEREO=0` + `GE_VR_XR=0` (**no-op**). It does **not** pin `GETV_VR=0` / `GETV_VR_GUNAIM=0` / `GETV_VR_ADSSIGHT=0` / `GETV_XR_PLAY=0` / `GETV_XR_PLAY_AUTORECENTER=0`. Those C-default **ON**. No tracked hand → gun ray = view center. ADSSIGHT on AIM paints the mark **on that center**. |
| Same on VR? | **Same `insightaimmode` if squeeze becomes `CONT_R`.** Headset **hides** it: `GUNAIM` + `ADSSIGHT` put the mark on the **gun ray**, not face centre (`CONTROLS.md`). Flat has no ray, so the stock center yank is visible. #35: do **not** light pad-AIM if walk-while-ADS must live. |
| Smallest falsifier (default OFF) | **Scratch monitor bat, one line at a time** (§2). First pins: `GETV_VR_ADSSIGHT=0`, then `GETV_VR_GUNAIM=0`, then `GETV_VR=0` (+ `GETV_XR_PLAY=0`). Also A/B `GETV_AUTOAIM=0` and watch **Look Ahead OFF**. Optional `GETV_XR_PLAY_AUTORECENTER=0` (cinema recenter, lower odds). |
| APPLY tonight? | **No from this repo.** Bat pins are a **GEVR packaging** change. C is workshop `bondviewProcessInput` / `port_input.c` (do not set `insightaimmode` on flat ADS — same family as #35 owner layout). |

```
Play-on-monitor.bat
  STEREO=0  GE_VR_XR=0 (no-op)  FPS=60
  GETV_VR / GUNAIM / ADSSIGHT / XR_PLAY / AUTORECENTER  unset → C-default ON after KEEP
  GETV_AUTOAIM  unset (headset boot is the only pin =0)

hip fire (HONEY)  → stick walks / turns camera; gun swivel + optional auto-aim
press AIM         → insightaimmode=1
                     canLookAhead=0  canNaturalTurn/Pitch=0  canSwivelGun=0
                     canManualAim=1  → KISSY  → stick → 7F067FBC
                     sight bit NOTAIMING clears
                     stick ~0 → crosshair/gun integrator decays to screen centre
headset           → GUNAIM/ADSSIGHT overlay the gun ray (yank hidden)
flat              → no hand pose → ray/sight = view centre (yank visible)
```

**Chair PASS (plain):** Play-on-monitor. Look off-center. Press aim. **PASS = no yank to center.** Look stays where you pointed.

---

## 1. Root finding (files + functions)

### 1.1 Evidence boundary

| Layer | Where | What it proves |
|-------|--------|----------------|
| Public `goldeneye-native` | this repo | KEEP C-defaults (`getv/port/src/port_render.c` `ge_vr_adssight` / `ge_vr_gunaim` / `ge_xr_play_autorecenter`, `MANIFEST.json`), `synthesizePad` `AIM_R`→`CONT_R`, vr440 squeeze banner. |
| Public `no6969el/GEVR` | `packaging/templates/Play-on-monitor.bat`, `gevr-vr442-boot.cmd`, `KEEP-DEFAULTS-INVENTORY-vr441.md`, `docs/CONTROLS.md`, `docs/101-…`, `docs/165-…`, `docs/289-…`, `docs/00-STATE.md`, **[#73](https://github.com/no6969el/GEVR/issues/73)** | Flat bat vs headset boot. ADSSIGHT “not stuck in face centre.” Mouse→stick. Crosshair auto-centre report. |
| Public decomp | `n64decomp/007` `src/game/bondview2.c`, `gunfire.c`, `file2.h` | Honey/Kissy, Look Ahead, integrator. |
| Workshop (private) | `F:\Projects\GEVR\GoldenEyeVR\goldeneye-native` `getv/port/src/port_input.c`, product `bondview2.c` | Live mouse pend (`ge_mouse_pend_x`), squeeze OR, GUNAIM write. **Do not push.** |

First chair grep: `insightaimmode`, `getenv("GETV_VR_ADSSIGHT")`, `getenv("GETV_VR_GUNAIM")`, `Play-on-monitor.bat`.

### 1.2 What the monitor bat actually sets

`packaging/templates/Play-on-monitor.bat` (vr442):

| Knob | Value | Class |
|------|--------|--------|
| `GETV_STEREO` / `_MODE` | `0` | **FLAT_FORCE** — no stereo eyes |
| `GE_VR_XR` | `0` | Smoke gate. **No-op** in this binary (`geVrXrEnabled` is `GETV_VR`) |
| `GETV_FPS` | `60` | Monitor cadence |
| `GETV_AUDIO_CLOCK` / `_QUEUE_MS` | `device` / `33` | [#48](https://github.com/no6969el/GEVR/issues/48) |
| VFX / tank QoL | `1` | Picture parity; not aim |

**Not set** (and therefore **not** forced off):

| Knob | Headset boot | After KEEP graduation if unset |
|------|----------------|--------------------------------|
| `GETV_VR` | `1` | **ON** (`MANIFEST.json` `bool_on_unset`) |
| `GETV_XR_PLAY` | `1` | **ON** |
| `GETV_VR_GUNAIM` / `GUNMOUNT` | `1` | **ON** |
| `GETV_VR_ADSSIGHT` / `ADSCULL` | `1` | **ON** |
| `GETV_XR_PLAY_AUTORECENTER` | `1` | **ON** |
| `GETV_AUTOAIM` | **`0` (DIG_OFF)** | save / options table (vr440 default **off** for new folders) |
| `GETV_XR_BUTTONS` / squeeze | `1` / wiped | C-default; squeeze banner still `squeeze -> aim` |
| `GETV_CONTROLS` | wiped | Honey unless the save says otherwise |

[#36 no-HMD DIG](../no-hmd-stereo-fallback-dig/RESULT.md) already named the missing `GETV_VR=0` pin. This page adds: even if stereo stays off, **GUNAIM / ADSSIGHT C-default ON is enough to recenter aim on AIM** when there is no tracked controller.

`GETV_XR_PLAY_AUTORECENTER` is the **cinema → gameplay** playspace recenter (`CONTROLS.md`: same chord as both-stick click). Low odds it fires on the aim button. Still a one-line A/B because it C-defaults ON on the monitor path.

### 1.3 Stock GoldenEye — the recenter is ADS mode

Honey / 1.1 (`n64decomp/007` `bondview2.c` `bondviewProcessInput`):

```
aimButtons  = L_TRIG | R_TRIG;          /* CONT_L OR CONT_R */
insightaimmode = (buttons & aimButtons) != 0;   /* hold; toggle if Aim Control */

canSwivelGun   = !insightaimmode;       /* hip-fire Honey */
canAutoAim     = !insightaimmode;       /* hip-fire only */
canManualAim   =  insightaimmode;       /* ADS = KISSY stick-aim */
canLookAhead   = !insightaimmode;       /* stick walks only in hip-fire */
canNaturalTurn = !insightaimmode;
canNaturalPitch= !insightaimmode;
moveData.aiming / .zooming = insightaimmode;
```

Kissy / Goodnight swap the bits: **Z = aim**, A = fire. `GETV_CONTROLS=kissy` would make **trigger** ADS. Headset boot **wipes** that. Monitor bat does not wipe it; C-default should stay Honey unless a save/env armed it.

Then the same function picks the gun writer:

```
if (canSwivelGun)      controldef = HONEY;
else if (canManualAim) controldef = KISSY;

HONEY:  sub_GAME_7F067F58(autoaimx or speedtheta*0.3, autoaimy or -speedverta*0.1, …)
KISSY:  sub_GAME_7F067FBC(stickX * 0.65/80, stickY * 0.65/80)
```

Both call `caclulate_gun_crosshair_position_rotation` (`gunfire.c`). The integrators are:

```
crosshair_x_pos = crosshair_x_pos * guncrossdamp + turn_x;   /* per g_ClockTimer */
crosshair_angle.x = (crosshair_x_pos * (1-damp) * w * 0.5) + w*0.5;
```

**`turn_x == 0` decays the gun/crosshair to screen centre.** That is retail. It is also exactly “press aim, stick/mouse idle, aim pulls back to center.”

Hard snap helper `sub_GAME_7F06802C` writes `crosshair_angle` / `field_FFC` to the **pixel centre**. Public `n64decomp/007` has the **body only** — GitHub code search finds **no callers** outside `gunfire.c`. Vanilla does not invoke it on ADS enter. If a sit still yanks after KISSY is parked, first chair grep on the workshop: `7F06802C` / `geVr` wraps around that symbol. Do not assume GETV calls it.

Sight draw: `gunSetSightVisible(GUNSIGHTREASON_NOTAIMING, moveData.aiming)`. ADS **clears** `NOTAIMING` so the stock sight appears — at `crosshair_angle`, which just decayed to centre. Doc `165`: in VR nobody pressed R, so the sight stayed hidden; PLAY0 `ADSSIGHT` is the replacement mark **on the gun ray**.

Zoom is tied to the same bit (`moveData.zooming = insightaimmode`). Doc `37`: VR wants magnification without the coupling. Flat wants the opposite of tonight: **coupling without the recenter**.

### 1.4 Look Ahead / `automovecentre` / Honey kissy

| Symbol | What it is | On AIM? |
|--------|------------|---------|
| `OPTION_LOOKAHEAD` | Still in vr440 `DEFAULT_OPTIONS` (auto-aim was removed; this was **not**) | — |
| `currentPlayerSetLookAheadSetting` | Writes `automovecentreenabled` | Loaded from save |
| `lookaheadcentreenabled` | Decomp comment: **always true** — computes a **target pitch** (~−4° plus floor slope) | Always |
| `canLookAhead` | Stick = walk. **0 in ADS** | Off |
| `automovecentre` / `docentreupdown` | If Look Ahead on **and** you were walking hard, pitch **lerps to targetPitch** | Only if already latched; **not** started by AIM (canLookAhead is 0) |
| `disableLookAhead` | Forced **1** on 2-pad styles | 2-pad hip-fire also skips auto-pitch |

Look Ahead is the “Bond levels his view when not moving / when walking” option. It is **pitch**, not yaw. It does **not** start on the AIM edge. Chair A/B still: **Look Ahead OFF** in the watch. If the yank **survives**, it is not this option.

Honey ADS **steals the stick** (`canLookAhead=0`). That is ads-grip DIG [#35](https://github.com/no6969el/GEVR/issues/35): same `insightaimmode` family. Flat mouse look uses that stick sink (`docs/289`: `ge_mouse_pend_x` → `out->rx`). Press AIM → mouse/stick is now **KISSY gun aim**, camera stop-turn, gun integrator sees near-zero stick → **centre**.

### 1.5 AUTOAIM / `GETV_AUTOAIM` / XR map leftovers

| Knob / bit | Flat tonight | Role in the yank |
|------------|--------------|------------------|
| `GETV_AUTOAIM` | **Unset** on monitor bat. Headset boot `=0`. | Force `cur_player_set_autoaim`. Unset = **save/options win** (vr440 README). `docs/BETA.md` says Auto-Aim **defaults OFF in the shipped exe** (new-folder `DEFAULT_OPTIONS` dropped the bit). Old EEPROM can still have it on — A/B `=0` still. |
| `OPTION_AUTOAIM` | Removed from `DEFAULT_OPTIONS` (vr440). Old saves may still have it. | Hip-fire only (`canAutoAim`). ADS **turns it off**. Gun that was pulled toward a guard then **springs to centre** on ADS is this. |
| `GETV_XR_BUTTONS` | Unset on flat | C-default. Squeeze→aim is the vr440 **banner**, not a KEEP pin. |
| `GETV_XR_BTN_SQUEEZE` | Not in monitor bat (boot wipe is headset-only) | If a pad is plugged in, C-default squeeze can still light `CONT_R`. |
| Public `synthesizePad` | `AIM_R` → `N64_R`; `AIM_L` **not** OR’d; `CROUCH` → `N64_L` (Honey **also** aim) | Flat pad L-trigger / R / squeeze. |
| `GETV_GUN_AIM` | Headset section-0 **wipe**. Textbook: exists, **unread**. | Do not confuse with `GETV_VR_GUNAIM`. |
| `GETV_MOUSE` / `_SENS` / `_SELFTEST` | Textbook; workshop `port_input.c` | Mouse is a **stick accumulator**, not a true look-angle. Same sink KISSY steals. |
| `GETV_STEREO_AIMRECT` | Headset KEEP `1`; **not** C-default-ON in `MANIFEST.json` | Stereo aim rect. Flat stereo is 0. Unlikely. |
| `GETV_VR_LEVELYAW` / `HEADYAW` | Headset KEEP; not in `bool_on_unset` | Head/level yaw. Need a live HMD. Park for flat. |

### 1.6 Does VR share the recenter?

**The game bit is shared.** Any path that ORs `CONT_R` / Honey `CONT_L` / Kissy `Z` sets `insightaimmode` and runs KISSY.

**The picture is not shared:**

| Path | What you see on squeeze / aim |
|------|-------------------------------|
| **Headset PLAY0** | `GETV_VR_GUNAIM=1` + `GETV_VR_ADSSIGHT=1`: mark **on the gun ray**, “not stuck in face centre” (`CONTROLS.md`). Squeeze env is **wiped**; C-default may still pad-AIM (walk dies — #35). Gun ray hides the integrator centre. |
| **Flat monitor** | No OpenXR hands. `geVrHandIsTracked` is 0 (public ABI: fall back to **view-relative**, never a floor ray). View-relative + ADSSIGHT = **face/screen centre**. KISSY decay agrees. Yank is obvious. |

If headset squeeze is **picture-only** (no `CONT_R`), VR does **not** share the recenter. If squeeze still pad-AIMs, VR **shares the bit** and the gun-ray overlay **covers** it. Chair: one headset sit with squeeze, gun held **off** the view centre. **PASS** = mark stays on the barrel. **FAIL** = mark/view jumps to face centre — then VR shares it and the C fix must be common (do not set `insightaimmode`).

---

## 2. Smallest falsifier — default OFF (no C)

Scratch copy of `Play-on-monitor.bat`. **Do not** add these to `$requiredBootKnobs` until a sit PASS. One extra `set` per run. Keep the stock FLAT_FORCE stereo pins.

| Arm | Extra assign | What it falsifies |
|-----|----------------|-------------------|
| **0** | none (stock monitor bat) | Tonight: look off-center, press aim. **FAIL** expected. |
| **A-sight** | `set GETV_VR_ADSSIGHT=0` | If yank **dies**, PLAY0 sight-on-identity-ray is the flat leftover. **Smallest KEEP pin.** |
| **A-gun** | `set GETV_VR_GUNAIM=0` (ADSSIGHT stock) | If yank dies only here, GUNAIM writes view-relative displacement on AIM. |
| **A-vr** | `set GETV_VR=0` and `set GETV_XR_PLAY=0` | Whole VR arm. Needed if A-sight/A-gun do nothing but C-default `GETV_VR` still injects. Also the #36 packaging pin. |
| **A-auto** | `set GETV_AUTOAIM=0` | Hip-fire assist → ADS centre. Match headset DIG_OFF. |
| **A-look** | Watch **Look Ahead = OFF** (no env; `OPTION_LOOKAHEAD`). Optional `GETV_LOOKAHEAD=0` **only if** workshop already has that getenv (not public). | Pitch auto-level. **PASS** here = not the ADS edge. |
| **A-re** | `set GETV_XR_PLAY_AUTORECENTER=0` | Cinema/playspace auto-recenter leftover. Low odds. |
| **A-mouse** | `set GETV_MOUSE=0` (if the exe reads it) then retry with pad C-up / right-stick only | Isolates mouse-as-stick vs pad. Textbook says the knob exists. |
| **Forbidden** | `GETV_CONTROLS=kissy`, `GETV_STEREO=1`, flipping `GETV_XR_BTN_B`, KEEP picture off | Not this bug. Kissy makes **trigger** ADS. |

**Predicted from source:** Arm **A-sight** or **A-vr** is the KEEP leftover. Arm **A-auto** only if the folder still has Auto-Aim on. Arm **A-look** should **fail** the ADS-edge chair (Look Ahead is off during ADS). If **all** bat arms still FAIL, the yank is **stock KISSY** and needs workshop C (§3).

Public ABI maps pad/XR AIM to `CONT_R`. GEVR `CONTROLS.md` does **not** name a monitor keyboard AIM key (Tab = pause is the only keyboard line). Workshop `port_input.c` is the map. [#73](https://github.com/no6969el/GEVR/issues/73) asks testers to say **mouse vs controller** — log the physical control on Arm 0; do not invent a public key from other ports.

---

## 3. APPLY sketches — **NOT LANDED**

Workshop only. Confirm live names before typing aliases.

### 3.1 Packaging (GEVR repo, not this tree)

If A-sight / A-vr PASS, pin on `Play-on-monitor.bat` (FLAT_FORCE, next to `GETV_STEREO=0`):

```
set GETV_VR=0
set GETV_XR_PLAY=0
set GETV_VR_GUNAIM=0
set GETV_VR_ADSSIGHT=0
set GETV_AUTOAIM=0
```

Optional: `GETV_XR_PLAY_AUTORECENTER=0`. Smoke today only requires `GE_VR_XR=0` + `GETV_STEREO=0`. Update smoke **only** if you add VR-off pins; do not require GUNAIM on the monitor bat.

Headset boot: **leave GUNAIM/ADSSIGHT KEEP ON.** Flat pins must not leak into `gevr-*-boot.cmd`.

### 3.2 Smallest C — do not enter Honey ADS on flat

Same recommendation as ads-grip DIG owner layout, for **monitor**:

```
flat AIM button  → draw ADS picture if you want zoom/sight
                 → do NOT set insightaimmode / CONT_R / CONT_L
                 → keep HONEY canNaturalTurn/Pitch so mouse/stick look stays
headset squeeze  → ADSSIGHT on gun ray, still no CONT_R if walk-while-ADS
```

If scoped weapons need `moveData.zooming` without KISSY, latch zoom **beside** `insightaimmode`, not through it (doc `37` / `101` A1-R3: aim mode also kills turn).

Do **not** delete the 16-line crosshair clamp (`gunfire.c` ~4699) for this bug. That clamp is FOV lock, not the ADS edge.

Do **not** run HONEY `7F067F58` and KISSY `7F067FBC` in one frame (doc `101`).

### 3.3 Out of scope

- Two-hand snap / HANDCUBES
- #35 grip split C (separate APPLY; **same bit**)
- Flipping `GETV_XR_BTN_B` / `GETV_CONTROLS`
- `GETV_STEREO_MODE=2`
- Personal credit paths. ROM dumps

---

## 4. Chair stare (plain tester sentences)

**Setup:** vr442-class zip. **`Play-on-monitor.bat`**. No headset. Keyboard+mouse **and** a pad if you have one. Auto-Aim: note watch page 4 (ON/OFF). Look Ahead: note it too.

**PASS bar:** look off-center, press aim — **aim stays**. No yank to screen/horizon centre.

| | Tester sentence |
|--|-----------------|
| **F-FAIL (tonight)** | “I look left/up, press aim, and the gun or camera **snaps back to the middle**.” Log **which** aim control (#73: mouse vs controller). |
| **F-PASS** | “I look left/up, press aim, and I am still looking **there**. I can aim properly.” |
| **M-FAIL** | “Only the **mouse** yanks. Pad C-look + aim is fine.” (mouse→stick / KISSY) |
| **P-FAIL** | “Pad and mouse both yank.” (stock ADS / ADSSIGHT leftover) |
| **L-FAIL** | “I was **walking**, not pressing aim, and pitch levels itself.” (Look Ahead — **other chair**) |
| **V-FAIL** | Headset: squeeze ADS, gun held off-centre, mark **jumps to my face**. (shared bit) |
| **V-PASS** | Headset: squeeze ADS, mark **stays on the barrel**. (overlay hides KISSY; flat still needs pins/C) |

**Regression (every run):** fire still works; walk still works in hip-fire; Tab pause; local split-screen still starts; headset Start-GEVR still gun-aims.

Run Arm 0, then A-sight, then A-vr, then A-auto, then Look Ahead OFF, **before** any C. Log mouse vs pad.

---

## 5. Director decision

| If you say… | Then… |
|-------------|--------|
| **Green bat pins** | GEVR packaging: FLAT_FORCE `GETV_VR=0` `GUNAIM=0` `ADSSIGHT=0` `AUTOAIM=0` (and `XR_PLAY=0`). Sit F-PASS. Headset KEEP untouched. |
| **Green workshop C** | After bat arms still FAIL: do not set `insightaimmode` on flat AIM. Share the #35 “no CONT_R” rule. Zoom/sight as a separate latch if scopes need it. |
| **Bat A/B only** | Run §2. Enough to pick the pin. Not a close of #73 until F-PASS. |
| **Reject** | Leave stock Honey ADS. Flat keeps N64 “sight at screen centre.” Owner brief loses. |
| **New GitHub issue?** | **No.** Tracker is [#73](https://github.com/no6969el/GEVR/issues/73). Cross-link #35 and this RESULT from that issue if a comment is wanted — do not open #74 for the same yank. |

---

## 6. Attribution / legal

- No personal credit paths edited.
- No GoldenEye ROM, assets, or dumps.
- Decomp cites are public `n64decomp/007` line logic, not a copied TU.
- Workshop `port_input.c` / `bondview2.c` GETV patches stay private until release policy flips.
