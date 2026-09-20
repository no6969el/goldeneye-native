# RESULT — ADS while moving/crouching: grip split (#35) (DIG ONLY)

**Status:** DIG. **Not APPLY READY on this public tree.** No C landed.
**Ask (issue as filed):** chair tune wants ADS while walking or crouching on the **left grip**. vr441 ships PLAY0 ADS *picture* (`ADSSIGHT` / `ADSCULL`) but **clears** `GETV_XR_BTN_SQUEEZE` in boot.
**Owner want (this brief):** **walk-while-ADS with right grip ADS**; **crouch on left grip and/or free left X/Y**. Do **not** break fire / turn / recenter / gun aim.
**Date:** 2026-09-20.
**Evidence:** public `goldeneye-native` HEAD (XR ABI + `port_input` vr440 snippet), public `no6969el/GEVR` packaging + CONTROLS + #35, public `n64decomp/007` `bondview2.c` / `bondview.c`. Workshop `getv/port/src/port_input.c` body is **not on any public remote** (`GEVR` `docs/RELEASE-POLICY.md`). Brief names are the workshop symbols.

Director can green-light owner layout, issue-as-filed left-grip ADS, bat-only probe, or reject from this page alone.

---

## Verdict (one screen)

| Question | Answer |
|----------|--------|
| Why walk-while-ADS fails | **N64 Honey aim mode steals the stick.** `insightaimmode` (R **or** L held) sets `canLookAhead=0`. `analogWalk` is ignored. Crouch is **not** L. Crouch is C-down / stick **while aiming**. |
| Visual ADS vs pad ADS | **Two paths.** PLAY0 `GETV_VR_ADSSIGHT=1` is the gun-ray mark. `GETV_XR_BTN_SQUEEZE` → `GE_XRACT_AIM` → `CONT_R` is the **pad** aim bit that turns Honey into KISSY. |
| vr441 wipe | Section 0 **clears** `GETV_XR_BTN_SQUEEZE` / `_PCT` (and `WALKHAND` / `FIREHAND` / `BUTTON_HAND` / `GETV_CONTROLS`). That is **DIG wipe**, not a KEEP pin. Empty env → `geXrParseAct` **C default**. Banner still says walk-hand `squeeze -> aim`. |
| Issue #35 as filed | Left-grip ADS. Public ABI already has `GE_VR_BTN_AIM_L` **unmapped** to the pad (`synthesizePad` only ORs `AIM_R` → `CONT_R`). Left squeeze can look “free” by accident. |
| Owner layout | **Right squeeze = ADS (keep walking).** **Left squeeze = crouch.** **And/or unbind left X/Y** (today `BTN_A` is **A and X**, `BTN_B` is **B and Y**). |
| Bat-only enough? | **Probe only.** One `GETV_XR_BTN_SQUEEZE` cannot split hands. Pad-AIM will still steal walk. Crouch cannot be `CONT_L` on Honey. |
| APPLY tonight? | **No from this repo.** Smallest C is workshop `port_input.c` + a `bondviewProcessInput` VR keep-walk / crouch inject. Do not land a stub here. |

```
hip fire (HONEY)     → left stick walks; gun follows GUNAIM; no pad R
squeeze → CONT_R     → insightaimmode=1 → KISSY; stick aims; walk dies
crouch (stock)       → insightaimmode AND (C-down / 2-pad stick-Y)
Honey L_TRIG         → ALSO aimButtons  (CONT_L is not crouch)
vr441 public         → ADSSIGHT ON; SQUEEZE env wiped; A=weapon, B=use
                       A/X paired, B/Y paired; squeeze C-default = aim
```

**Do not ship issue-as-filed left-grip ADS if the owner brief stands.** Two-hand snap DIG (`#35` “leave left-grip ADS on the real left controller”) assumed the issue title. This page **supersedes** that: ADS belongs on the **right** grip; left grip is crouch (or free).

---

## 1. Root finding (files + functions)

### 1.1 Evidence boundary

| Layer | Where | What it proves |
|-------|--------|----------------|
| Public `goldeneye-native` | this repo | `include/ge_vr/ge_vr.h` bits, `src/xr_input.cpp` squeeze→`AIM_L/R`, `synthesizePad` **AIM_R only**, vr440 `port_input` snippet (`geXrParseAct` / `geXrPadAct` / `GE_XRACT_*`). |
| Public `no6969el/GEVR` | `packaging/templates/gevr-vr441-boot.cmd`, `KEEP-DEFAULTS-INVENTORY-vr441.md`, `docs/CONTROLS.md`, `docs/ship-feature-checklist.md`, issue #35 | Boot wipe vs KEEP pins. Sit gate: “Squeeze ADS mark sits on the gun ray.” A/X and B/Y pairing. |
| Public decomp | `n64decomp/007` `src/game/bondview2.c` `bondviewProcessInput`, `bondview.c` `currentPlayerAdjustCrouchPos` | Honey aim bits, walk steal, crouch-only-while-ADS. |
| Workshop (private) | `F:\Projects\GEVR\GoldenEyeVR\goldeneye-native` `getv/port/src/port_input.c` | Live `geXrActSqueeze` / pad OR. **Do not push.** |

First chair grep: `getenv("GETV_XR_BTN_SQUEEZE")`, `geXrParseAct`, `geXrPadAct`, `insightaimmode`.

### 1.2 Public XR ABI (host-agnostic)

`ge_vr.h`:

| Bit | Name | `synthesizePad` (public `xr_input.cpp`) |
|-----|------|------------------------------------------|
| `1<<1` | `GE_VR_BTN_FIRE_R` | `CONT_Z` |
| `1<<2` | `GE_VR_BTN_AIM_L` | **not OR’d** |
| `1<<3` | `GE_VR_BTN_AIM_R` | `CONT_R` |
| `1<<8` | `GE_VR_BTN_CROUCH` | `CONT_L` |
| `1<<0` / `1<<7` | `FIRE_L` / `SWAP` | both `CONT_A` |

Squeeze action is OpenXR `aim_mode`, both hands, `/input/squeeze/value`. Left squeeze lights `AIM_L` and **never becomes a pad bit** on this ABI. Right squeeze becomes Honey **aim**. Mapping crouch to `CONT_L` on this ABI would **also** enter aim mode (Honey `aimButtons = L_TRIG \| R_TRIG`).

Quest suggested faces (same file): right A reload, right B use, **left X swap, left Y watch**. GETV boot does **not** use those names; it uses `GETV_XR_BTN_A` / `_B` with A/X and B/Y **paired** (vr439/vr440 comments, still the vr441 map).

### 1.3 GETV remap (`port_input.c`) — workshop, named from public snippets

vr440 snippet / patch (`getv/port/src/port_input_vr440.snippet.c`):

```
GETV_XR_BUTTONS=1 on the walk hand:
  A  -> use / reload     (vr441 boot overrides: weapon)
  B  -> start / pause    (vr441 boot overrides: use)
  squeeze -> aim, trigger -> fire
  both thumbstick clicks -> recenter
```

`geXrActBtnB()`: `geXrParseAct(getenv("GETV_XR_BTN_B"), DEFAULT)`. Known strings from boot: `use`, `weapon`, `start`. Enum samples: `GE_XRACT_WEAPON=2`, `GE_XRACT_START=5`. `geXrPadAct()` writes `OSContPad` (`CONT_START` for start).

**Squeeze is the same shape.** Expect `geXrActSqueeze()` reading `GETV_XR_BTN_SQUEEZE` with C-default **aim**. `GETV_XR_BTN_SQUEEZE_PCT` is the analog threshold (same family as `GETV_XR_INPUT_TRIGGER=50`).

Wiped with squeeze (vr441 §0) — C-default after empty, **not** KEEP-assigned:

| Knob | Role if C still reads it |
|------|--------------------------|
| `GETV_XR_INPUT_WALKHAND` | Which controller is the N64 stick / walk |
| `GETV_XR_INPUT_FIREHAND` | Which controller is fire / gun |
| `GETV_XR_TURN_HAND` / `_INVERT` | Right-stick turn |
| `GETV_XR_BUTTON_HAND` | Which hand gets face `BTN_A/B` |
| `GETV_CONTROLS` | Honey vs Kissy vs 2-pad. **Do not arm.** Kissy makes **Z** the aim bit — trigger would ADS. |

vr441 **does** pin (leave these alone on a probe):

| Knob | Ship | Must not break |
|------|------|----------------|
| `GETV_XR_BUTTONS` | `1` | Face + squeeze banner |
| `GETV_XR_BTN_A` | `weapon` | A **and X** |
| `GETV_XR_BTN_B` | `use` | B **and Y** |
| `GETV_XR_TURN` + scale/dead | on | Right stick turn |
| `GETV_XR_RECENTER_CHORD` | `1` | Both stick clicks |
| `GETV_XR_INPUT_TRIGGER` | `50` | Fire threshold |
| `GETV_VR_GUNAIM` / `GUNMOUNT` | `1` | Gun ray |
| `GETV_VR_ADSSIGHT` / `ADSCULL` | `1` | PLAY0 ADS picture |
| `GETV_AUTOAIM` | `0` | Stay off |

Inventory class: `BTN_A/B` are **PLAYER_PREF**. `BTN_SQUEEZE` is **wipe-only / C_DEFAULT UNKNOWN**. KEEP graduation `do_not_touch` already lists `GETV_XR_BTN_A/B` and `GETV_XR_BUTTONS` — do not C-default-ON those.

### 1.4 PLAY0 ADS picture vs pad aim

vr441 §6 PLAY0 (KEEP_SHIP): `GETV_VR_ADSSIGHT=1`, `HITSNAP=2`, `SIGHTPX=6`, `ADSCULL=1`.

Sit gate (`docs/ship-feature-checklist.md`): “Squeeze ADS mark sits on the gun ray.” CONTROLS.md: “Squeeze / grip → ADS / aim mark on the gun ray (not stuck in face centre).”

That mark is **GETV gun-ray / ADSSIGHT**, not proof that `CONT_R` is held. Doc `165` is the opposite history: nobody pressed R, so `gunsightmode=2` (`GUNSIGHTREASON_NOTAIMING`) hid the stock sight. PLAY0 ADSSIGHT is the replacement. Parking `GETV_XR_BTN_SQUEEZE` in boot is “do not pin a pad remap”; it is **not** “squeeze does nothing.”

### 1.5 Stock GoldenEye (`bondviewProcessInput`)

Honey / 1.1 (`CONTROLLER_CONFIG_HONEY` family, not Kissy/Goodnight):

```c
shootButtons = Z_TRIG;
aimButtons  = L_TRIG | R_TRIG;   /* CONT_L OR CONT_R */
invButtons  = A_BUTTON;

insightaimmode = (buttons & aimButtons) != 0;   /* hold-to-aim */
canLookAhead   = !insightaimmode;
canNaturalTurn = !insightaimmode;
canManualAim   =  insightaimmode;   /* KISSY branch: stick aims the gun */

/* crouch — 1-pad Honey */
crouchDown = insightaimmode && (buttons & (D_JPAD | D_CBUTTONS));
crouchUp   = insightaimmode && (~buttons & (U_JPAD | U_CBUTTONS));
```

2-pad styles crouch with **stick Y while aiming**, not a button.

Then later:

```c
if (moveData.canLookAhead)
    speedforwards = analogWalk / 70.0f;   /* left stick walk */

if (moveData.crouchDown) currentPlayerAdjustCrouchPos(-2);
else if (moveData.crouchUp) currentPlayerAdjustCrouchPos(2);
```

When `canSwivelGun` is false and `canManualAim` is true, `controldef = KISSY` and the stick is fed to `sub_GAME_7F067FBC` (doc `101` / `88`). **That is walk-while-ADS dying.** It is also why right-stick GETV turn (`GETV_XR_TURN=1`) and GUNAIM must stay **off** that branch: KISSY would fight the gun ray.

`CONT_L` as “crouch” is the public ABI’s mistake on Honey. Left grip → `CONT_L` would ADS, not duck.

### 1.6 Why the issue title and the owner brief disagree

| | Issue #35 (chair) | Owner brief |
|--|-------------------|-------------|
| ADS hand | Left grip | **Right grip** |
| Why left looked attractive | `AIM_L` does not hit the pad, so walk may survive | Walk must survive **and** ADS must sit on the gun hand |
| Crouch | Implied “while ADS” (stock C-down) | **Left grip** (dedicated) |
| Left X/Y | Unmentioned | **Free** (stop duplicating A/B) |

Chair “ADS while walking or crouching on the left grip” is a **one-control** ask: put the ADS bit where it does not steal the walk stick. Owner is a **two-control** ask: gun-hand ADS + off-hand crouch, walk stays on the left stick.

Two-hand snap DIG said melee / touch / watch / “#35 left-grip ADS stay on the real left controller.” If owner layout wins, snap **must not** bind or steal **right** squeeze, and left squeeze becomes crouch, not ADS.

---

## 2. What a bat can and cannot do

### 2.1 Bat can probe (scratch boot — not `$requiredBootKnobs`)

Copy `gevr-vr441-boot.cmd` (or vr442-class zip boot). **Do not** clear `GETV_XR_BUTTONS`, `BTN_A`, `BTN_B`, `TURN*`, `RECENTER_CHORD`, `GUNAIM`, `ADSSIGHT`. After the wipe block, **assign** only the probe line.

| Arm | Extra assigns | What it falsifies |
|-----|---------------|-------------------|
| **0 control** | none (stock wipe) | Tonight: walk + squeeze + fire + turn + recenter |
| **S-aim** | `set GETV_XR_BTN_SQUEEZE=aim` | If identical to 0, C-default is already aim. If walk **dies** vs 0, pad-AIM was parked and this **arms** Honey steal. |
| **S-none** | `set GETV_XR_BTN_SQUEEZE=none` (or `0` / `off` if that is how `geXrParseAct` spells idle — try `none` first) | If the gun-ray mark **vanishes**, squeeze ADS is the pad remap. If the mark **stays**, ADSSIGHT reads squeeze analog **beside** `geXrPadAct`. |
| **S-crouch** | `set GETV_XR_BTN_SQUEEZE=crouch` | If Bond ducks, `GE_XRACT_CROUCH` exists **and** it is not `CONT_L` (or Honey is not live). If he **ADS** instead, crouch was encoded as `CONT_L`. If **nothing**, parse string is wrong or unused. |
| **Hands** | `set GETV_XR_INPUT_FIREHAND=right` and/or `GETV_XR_BUTTON_HAND=right` | Whether face/squeeze already split. **PASS** if left X/Y go dead and right A/B still weapon/use. |
| **Pct** | `set GETV_XR_BTN_SQUEEZE_PCT=80` vs `20` | Analog threshold. Light squeeze vs full fist. |
| **Forbidden** | `GETV_CONTROLS=kissy` / any `GETV_XR_TURN=0` / `BUTTONS=0` / `BTN_B=start` | Not a #35 probe. Kissy makes Z=aim. `BUTTONS=0` kills the banner. |

`GETV_INPUT_DEBUG=1` / `GETV_XR_INPUT_EVERY=1` are boot-forbidden smoke knobs. Chair-only, not a zip.

**Predicted from source:** Arm S-aim **fails walk-while-ADS** if squeeze reaches `CONT_R`. Arm S-none **keeps walk** and may keep or drop the gun-ray mark. Neither arm gives **right=ADS, left=crouch**.

### 2.2 Bat cannot

- Bind **right** squeeze to ADS and **left** squeeze to crouch with one env.
- Keep Honey `canLookAhead` while `CONT_R` is down.
- Crouch without aim mode unless C injects `crouchDown` / `currentPlayerAdjustCrouchPos`.
- Free **only** left X/Y while right A/B stay mapped — `GETV_XR_BTN_A` is A **and** X.

If Arm Hands (`BUTTON_HAND=right`) already frees X/Y, that half of the owner brief is **bat**. Still does not split grips.

---

## 3. APPLY sketches — **NOT LANDED**

Workshop only. Confirm live `geXrAct*` names before typing aliases.

### 3.1 Owner layout (recommend)

```
right squeeze  → ADS picture (ADSSIGHT)  +  do NOT set insightaimmode / CONT_R
left squeeze   → crouch inject            +  do NOT set CONT_L
left X / Y     → GE_XRACT_NONE (or omit)
right A / B    → weapon / use (tonight)
trigger        → fire (unchanged)
right stick    → GETV_XR_TURN (unchanged)
both clicks    → recenter (unchanged)
GUNAIM=1       → gun ray (unchanged)
```

Walk stays HONEY. ADS is GETV sight-on-gun, not KISSY. Crouch is `currentPlayerAdjustCrouchPos`, same writer stock already uses.

### 3.2 Parse / knobs (PLAYER_PREF, not KEEP-ON)

Do **not** add to vr441 `$requiredBootKnobs` until a sit PASS.

```
GETV_XR_BTN_SQUEEZE      unset = C default (today: aim, both or walk hand)
GETV_XR_BTN_SQUEEZE_R    unset = aim-picture  (owner)
GETV_XR_BTN_SQUEEZE_L    unset = crouch       (owner)
GETV_XR_BTN_X            unset = none         (free left X)
GETV_XR_BTN_Y            unset = none         (free left Y)
GETV_XR_BTN_SQUEEZE_PCT  analog % (already exists, wiped)
```

If the workshop already has one squeeze getter, split it; do not overload `=aim` to mean both hands.

Issue-as-filed alternative (only if owner **retracts**): `SQUEEZE_L=aim-picture`, `SQUEEZE_R=none`, still **no** `CONT_R`.

### 3.3 Smallest C — `port_input.c`

```c
/* NOT APPLY READY — workshop sketch.
 * geXrPadAct: AIM must not OR CONT_R while GETV_VR_GUNAIM is on
 * (or a new GE_XRACT_ADS that only latches ADSSIGHT).
 * CROUCH must call AdjustCrouchPos / set a sticky crouchDown, not CONT_L.
 */

static s32 geXrActSqueezeHand(int hand /* 0=left */)
{
    /* getenv GETV_XR_BTN_SQUEEZE_L / _R, else GETV_XR_BTN_SQUEEZE, else
     * owner defaults: R=ads, L=crouch.
     * geXrParseAct strings already used: use, weapon, start.
     * add: aim, crouch, none, ads (ads = picture only).
     */
    return GE_XRACT_NONE;
}
```

Do not change `geXrActBtnB` / `BTN_A` defaults in the same patch.

### 3.4 Walk-while-ADS — `bondviewProcessInput` (workshop `vendor/ge-decomp`)

Only if squeeze still has to light `insightaimmode` for the sight/zoom bit:

```c
/* NOT APPLY READY.
 * After stock canLookAhead = !insightaimmode:
 * if (geVrIsActive() && ge_vr_ads_keepwalk())
 *     moveData.canLookAhead = 1;
 *     moveData.canNaturalTurn = 1;  -- or leave turn to GETV_XR_TURN
 * Skip the KISSY stick feed while GUNAIM=1 (keep HONEY swivel off;
 * do not run both — doc 101).
 */
```

Prefer **not** setting `insightaimmode` at all if ADSSIGHT already draws the mark. Scoped weapons that need `moveData.zooming` are the reason to keep a **GETV ADS latch** separate from Honey R.

### 3.5 Crouch inject

```c
/* NOT APPLY READY.
 * if (left squeeze latched && !DISABLE_CROUCH)
 *     currentPlayerAdjustCrouchPos(-2);   -- hold = stay squat (clamp)
 * else if (was latched)
 *     currentPlayerAdjustCrouchPos(+2);   -- release = stand
 * Do not OR CONT_L. Do not require insightaimmode.
 * Physical crouch (geVrPhysicalCrouch) already exists on the public ABI;
 * if workshop already ORs it, left-grip can share that writer.
 */
```

### 3.6 Out of scope

- `GETV_CONTROLS` / Kissy / 2-pad
- Flipping `GETV_XR_BTN_B` back to `start` (vr440) or `BTN_A` off `weapon`
- KEEP-ON graduation of new squeeze knobs
- Two-hand snap / HANDCUBES (other DIG)
- Auto-aim, GUNARM, BODY
- Personal credit paths. ROM dumps

---

## 4. Chair stare (plain tester sentences)

**Setup:** vr441- or vr442-class zip. Stock boot first (Arm 0). Recenter both sticks. Right hand holds a gun. Auto-Aim OFF.

**Regression (every run):** trigger fires; right stick turns; both-stick recenter; gun ray still aims; B reloads; A cycles weapon; pause still Menu / system (not Y).

| | Tester sentence |
|--|-----------------|
| **W-PASS** | “I hold **right grip** to ADS. The mark sits on the **gun**. I can **walk** with the left stick at the same time. I do **not** stop or start strafing from KISSY.” |
| **W-FAIL** | “ADS and I cannot walk.” / “Left stick aims the gun.” / “Squeeze does nothing.” / “Hip-fire mark and ADS mark are the same and grip does nothing.” |
| **C-PASS** | “**Left grip** ducks Bond. Release stands up. I can ADS on the right grip **while** crouched, and I can still walk crouched.” |
| **C-FAIL** | “Left grip ADS instead of duck.” / “I only duck if I also ADS and click C / stick down.” / “Left grip does nothing.” |
| **X-PASS** | “Left **X** and **Y** do nothing. Right **A** still cycles, right **B** still reloads.” |
| **X-FAIL** | “X still cycles weapon / Y still reloads or pauses.” |
| **R-FAIL** | “Trigger dead.” / “Cannot turn.” / “Recenter gone.” / “Gun no longer follows the controller.” |

Run Arm 0, then S-aim, then S-none, then S-crouch **before** any C. Log which arm matches W/C/X. If S-none keeps the gun-ray mark **and** walk, pad-AIM is already off and C is only the **split** + crouch inject + free X/Y.

---

## 5. Director decision

| If you say… | Then… |
|-------------|--------|
| **Green owner layout** | Workshop 3.1–3.5. Sit W + C + X + regression. New `_L/_R` knobs stay off the public allowlist until PASS. |
| **Green issue-as-filed (left ADS)** | Only if you retract the owner brief. Still **no** `CONT_R` if walk must live. Crouch stays unsolved. |
| **Bat-only** | Run §2.1. Expect **not** to close #35. Use the log to pick parse strings. |
| **Free X/Y only** | Try `GETV_XR_BUTTON_HAND=right` first. If that PASSes X without killing A/B, maybe no C. Grips still need C. |
| **Reject** | Leave vr441 wipe + C-default squeeze→aim. Walk-while-ADS stays Honey-broken if pad-AIM is live. |

---

## 6. Attribution / legal

- No personal credit paths edited.
- No GoldenEye ROM, assets, or dumps.
- Decomp cites are public `n64decomp/007` line logic, not a copied TU.
- Workshop `port_input.c` / `bondview2.c` GETV patches stay private until release policy flips.
