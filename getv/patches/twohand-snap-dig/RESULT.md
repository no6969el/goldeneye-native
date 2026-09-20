# RESULT — two-hand support-grip snap (DIG ONLY)

**Status:** DIG. **Not APPLY READY.** No C landed.
**Ask:** BarZ — when the LEFT empty hand (HANDCUBES left cube) gets near the RIGHT-hand gun, SNAP the left pose to a front support-grip so it reads as two-handed hold. Not merely hide the cube on intersect.
**Date:** 2026-09-20. Evidence from public `goldeneye-native` HEAD, public `no6969el/GEVR` textbook/packaging (vr441 boot), and public `n64decomp/007` `gun.h`. Workshop C bodies (`geStereoXr*`, HANDCUBES draw) are **not on any public remote** (`GEVR` `docs/RELEASE-POLICY.md`). Function names in the brief are treated as the workshop symbols to patch.

Director can green-light or reject from this page alone.

---

## Verdict (one screen)

| Question | Answer |
|----------|--------|
| Wear-only? | **No.** No existing env snaps a pose. `HANDCUBES` / `MASK` only draw / pick a hand. `GUNARM=0` is rejected as the fix. |
| Smallest change? | New knob **`GETV_VR_TWOHAND` default OFF** + ~40–80 lines in the HANDCUBES draw path. |
| Snap what? | **Approach A (recommend):** HANDCUBE **draw pose only**. Leave left aim / melee / touch / reticle on the real controller. |
| Dual-wield? | Snap **must not run** if left also holds a gun. Hide the cube (next-cut already wants that). |
| GUNARM? | **`GETV_VR_GUNARM=1` stays.** Do not propose retail Bond sleeve. |
| Rejected crutches | Bond cuff / suit hand. HANDMESH jointed boxes. GHOSTHAND (parked). |
| APPLY tonight? | **No.** Workshop TU is private; this is not a one-liner. |

---

## 1. Root finding (files + functions)

### 1.1 Evidence boundary (read this first)

| Layer | Where | What it proves |
|-------|--------|----------------|
| Public `goldeneye-native` | this repo | Host-agnostic VR ABI (`ge_vr.h` / `ge_vr_bridge.cpp` / `xr_input.h`). **Not** the playable GETV tree. README: playable workshop is **not published here**. |
| Public `no6969el/GEVR` | docs + `packaging/` | Ship knobs, boot values, chair refusals, dual-wield cube-hide as **next cut**, PD two-hand numbers. **No** `geStereoXrHandWorld` body. |
| Workshop (private) | `F:\Projects\GEVR\GoldenEyeVR\goldeneye-native` (`vendor/ge-decomp` + `getv`) | Live `geStereoXr*` helpers, HANDCUBES draw, TOUCHUSE / HANDMELEE. **Do not push.** |
| Public decomp | `n64decomp/007` `src/game/gun.h` | Per-hand inventory: `getCurrentPlayerWeaponId`, `Gun_hand_without_item`, `gunUpdateAndFireBothHands`. |

`geStereoXrHandWorld` / `HandBasis` / `geStereoXrGunMount` / `geTouchBoxDist` **do not appear in any public file**. GEVR docs place every other `geStereo*` helper in workshop `vendor/ge-decomp/src/game/stereo.c`. Treat that TU as the first grep on the chair tree.

### 1.2 Left-hand world pose

**Workshop (brief + stereo family):**

- `geStereoXrHandWorld` — left controller world translation (game units).
- `HandBasis` — left orientation basis from the same XR sample.
- Index trap already paid for in PD (`GEVR` `docs/102-WHAT-PERFECT-DARK-ALREADY-SOLVED.md` §4): game `HANDRIGHT=0`, `HANDLEFT=1`; OpenXR `0=left`. `ctrlIndex = 1 - handnum`. A left/right swap here makes the cube snap to the wrong gun.

**Public ABI (same split, different names) — `include/ge_vr/ge_vr.h`, `src/ge_vr_bridge.cpp`, `src/xr_input.h`:**

| Helper | Pose used | Job |
|--------|-----------|-----|
| `geVrGetAimRay(hand)` | OpenXR **aim** (`/input/aim/pose`) | Hitscan / barrel line |
| `geVrGetWeaponModelMatrixF(hand)` | OpenXR **grip** (`/input/grip/pose`) | Fist / weapon **draw** |
| `geVrGetWeaponDisplacement(hand)` | aim vs head | `weapon_theta/verta_displacement` |
| `geVrHandIsTracked(hand)` | tracking latch | Fall back; never stale floor-aim |

`xr_input.h` states the rule in one line: aim = barrel, grip = fist. Two-hand snap wants the **grip / draw** side, not the aim ray.

### 1.3 Gun mount / grip

**Workshop (brief):** `geStereoXrGunMount` + gunfire mount (the matrix already placing the right-hand gun under `GETV_VR_GUNMOUNT=1`).

**Public / boot (vr441 KEEP, do not flip):**

| Knob | Ship | Role |
|------|------|------|
| `GETV_VR_GUNMOUNT` | `1` | World-space gun at controller |
| `GETV_VR_GUNAIM` | `1` | Shot follows gun |
| `GETV_VR_GUNARM` | `1` | **Floating VR guns. Keep.** |
| `GETV_VR_BODY` | `0` | Full body parked |
| `GETV_VR_BODY_NOARMS` | `1` | No retail / IK arms |

Historical grip math (GEVR textbook, still the right *shape*): offset along the gun’s own frame from the authored origin to the hand (`docs/154` / `docs/82`). PD pistol default `(0, 16, −4)` cm; GE units are not 1:1 with that table. Support-grip is a **second** offset **forward of that grip**, not a copy of the firing-hand trim.

Public hook sketch (not the live GETV site): `patches/DECOMP-PATCHES.md` §4–6 — `gunfire.c` `gunUpdateAndFireBothHands` + `geVrGetWeaponModelMatrixF`.

### 1.4 HANDCUBES as it ships tonight

From `GEVR` `packaging/templates/gevr-vr441-boot.cmd` and `KEEP-DEFAULTS-INVENTORY-vr441.md`:

| Knob | Value | Class | Meaning |
|------|-------|-------|---------|
| `GETV_VR_HANDCUBES` | `1` | KEEP_SHIP | Draw orange cubes |
| `GETV_VR_HANDCUBE_MM` | `60` | PLAYER_PREF | Cube edge ~60 mm |
| `GETV_VR_HANDCUBE_MASK` | `1` | KEEP_SHIP | **Left only.** Right cube off while right holds the gun. |

Chair refusals already written down (`FEATURES-CURRENT.md`, `ROADMAP.md`, `COMING-SOON.md`):

- Bond cuff / suit hand — rejected.
- HANDMESH jointed boxes — **LOOK REJECTED**.
- GHOSTHAND / see-through fingers — **parked** (cooking, not this cut).
- Dual-wield cube hide — **next cut**, not claimed on public vr441.

Current “hide cube on intersect” is a **draw skip**. BarZ wants a **pose write** so the left cube sits on the forend.

### 1.5 Proximity helpers — reusable, wrong radii

| Helper | Ship | What it is | Reuse? |
|--------|------|------------|--------|
| `GETV_VR_TOUCHUSE` + `_R=12` | ON | Reach-to-USE (doors / consoles). Closed as product ask `#40`. | **Pattern** (sphere vs world point). Radius is door-poke, not grip. |
| `GETV_VR_HANDMELEE` + `_R=8` + `_COOL=30` | ON | Punch volume + cooldown. | **Pattern** + latch idea. Radius is a fist, too tight for “near the gun.” |
| `geTouchBoxDist` (workshop) | — | Box distance used by touch/melee. | **Yes, call it.** Do not share the USE/melee thresholds. |
| PD `VR_2H_SEP_MIN 9.0` | prior art | Controllers physically touching still read ~9 apart (`102` §3). | **Why hysteresis exists.** Enter must be **larger** than “touching.” |
| PD two-hand ease | prior art | Guard on `lvframe60` — fire path can run **twice per frame** (`102` §4.3). | Sticky latch, not a per-call lerp that doubles while firing. |

**Recommended first-chair numbers (PLAYER_PREF, not KEEP):**

- Enter: ~`14`–`18` game units (above melee 8, near/above touch 12, above PD 9).
- Exit: enter + ~`6`–`8` (sticky; kills chatter).
- Forward offset: a few gun-frame units ahead of `geStereoXrGunMount` (“front support”), then wear.

Do **not** drive snap from `TOUCHUSE_R` / `HANDMELEE_R`. Those knobs will get retuned for doors and punches and would move two-hand feel by accident.

### 1.6 Dual-wield gate

Public `gun.h`:

- `getCurrentPlayerWeaponId(GUNHAND hand)`
- `Gun_hand_without_item(GUNHAND hand)` — empty-hand test
- `gunUpdateAndFireBothHands()` — both hands already first-class

`bondinv.c` still gates dual-wield on `getPlayerCount() == 1` (solo). Next-cut copy already says *“Orange hand cubes hide when you dual-wield.”*

**Rule:** if left is **not** empty (`!Gun_hand_without_item(HANDLEFT)` / left `getCurrentPlayerWeaponId` is a gun), **do not snap** and **do not draw** the left cube. Snap is empty-hand-only.

---

## 2. Approach A / B

### A — snap HANDCUBE draw pose only (**recommend**)

When `GETV_VR_TWOHAND` is on, left is empty, right holds a gun, both tracked, and distance (left world pos → gun mount, with hysteresis) is inside the band:

1. Build support matrix = `geStereoXrGunMount` (right) × forward offset × support orientation (left-hand-ish, not a mirrored right fist).
2. Draw the left HANDCUBE with that matrix instead of `geStereoXrHandWorld` / `HandBasis`.
3. Leave left **aim**, **melee**, **touch-use**, **reticle**, **watch raise** on the real left controller.

**Why A first:** BarZ asked for a *read* (“two-handed hold”), not a new input mode. Left empty hand still punches, pokes doors, and raises the watch (`FEATURES.md`: “Hands do Bond things”). `#35` still wants left-grip ADS while walking — that must stay on the real stick.

### B — also snap left aim / pose feed (**do not ship first**)

Same latch, but write the snapped pose back into the left controller sample that melee / touch / `RETICLE_HAND` / hitscan consume.

| | A draw-only | B also snap feed |
|--|-------------|------------------|
| Looks like two hands on the gun | Yes, if offset is right | Yes |
| Left melee / touch / watch | Stay at the real hand | Teleport to the forend |
| Dual-wield safety | Easy (skip draw) | Can steal left gun aim |
| Chatter if hysteresis fails | Cube pops | Cube **and** gameplay pop |
| Chair debug | `TWOHAND=0` restores tonight | Harder to A/B |

**Pick B later only if** A wears as “cube glued to the gun but my real fist is still floating beside it” **and** testers want the fist to *be* the support. That is a second sit, not this patch.

---

## 3. APPLY sketch — **NOT APPLY READY**

Wear-only is impossible. Smallest C is still a new getenv + latch + matrix, in the **private workshop**.

### Knob

```
GETV_VR_TWOHAND        unset / empty / 0 = OFF   (chair A/B; not KEEP-ON)
GETV_VR_TWOHAND_R      enter radius, default ~16 (only if A needs a wear tune)
GETV_VR_TWOHAND_EXIT   exit radius, default ~22  (or enter + 6)
GETV_VR_TWOHAND_FWD    gun-forward offset, default small +Z in gun frame
```

Unset must stay **OFF**. Do **not** add this to `gevr-vr441-boot.cmd` `$requiredBootKnobs` until a sit passes. Chair A/B: one extra `set GETV_VR_TWOHAND=1` in a scratch boot, not a ship allowlist change.

### Files (workshop grep, then patch)

1. **`vendor/ge-decomp/src/game/stereo.c`** (and `.h` if the helpers are exported)  
   Confirm `geStereoXrHandWorld`, `HandBasis`, `geStereoXrGunMount`. If the names differ by a suffix, **do not invent aliases** — patch the live symbols.
2. **HANDCUBES draw site** — first `getenv("GETV_VR_HANDCUBES")` / `HANDCUBE_MASK` / `HANDCUBE_MM`. Likely same TU or `getv/port/src` overlay that already skips the right cube when `MASK=1` and hides on intersect. **Replace the hide-on-intersect branch** with: if two-hand latch, draw at support matrix; else existing hide/draw.
3. **Empty-hand / dual-wield test** — `Gun_hand_without_item(HANDLEFT)` or `getCurrentPlayerWeaponId(HANDLEFT)` next to that draw. Do not use `getPlayerCount()`.
4. **Optional traces** — `GETV_VR_TWOHAND_TRACE=0` DIG_OFF, same shape as `GUNARM_TRACE` / `TOUCHUSE_TRACE`.

### Latch (sketch, not a patch)

```c
/* NOT APPLY READY — workshop sketch only.
 * getenv TWOHAND default 0. HANDCUBES draw path, after MASK left-only.
 * GUNARM stays 1. Do not write left aim. */

static int ge_vr_twohand(void) {
    static int on = -1;
    if (on < 0) {
        const char *e = getenv("GETV_VR_TWOHAND");
        on = (e != NULL && *e != '\0') ? (atoi(e) != 0) : 0; /* default OFF */
    }
    return on;
}

/* per-frame, sim-owner / first eye only (lvframe60 trap):
 * emptyL = Gun_hand_without_item(HANDLEFT);
 * gunR   = !Gun_hand_without_item(HANDRIGHT);
 * if (!ge_vr_twohand() || !emptyL || !gunR) { latched = 0; return; }
 * d = geTouchBoxDist(leftWorld, gunMount)  -- or length(left - mount)
 * if (!latched && d < enter) latched = 1;
 * if ( latched && d > exit)  latched = 0;
 * if (latched) drawLeftCube(mount * Fwd(TWOHAND_FWD) * supportBasis);
 * else         existing HANDCUBES / hide-on-intersect
 */
```

**Do not** land this from `goldeneye-native` public. The draw call and `geStereoXr*` bodies are not here.

### Out of scope (do not sneak in)

- `GETV_VR_GUNARM=0` / Bond sleeve.
- HANDMESH / GHOSTHAND / cuff.
- KEEP-ON graduation of `TWOHAND`.
- Boot allowlist / pack smoke until sit PASS.
- Personal credit paths. ROM dumps.

---

## 4. Chair stare — PASS / FAIL (plain tester sentences)

**Setup (A/B):** same vr441-class zip. Scratch boot keeps `GETV_VR_GUNARM=1`, `GETV_VR_HANDCUBES=1`, `GETV_VR_HANDCUBE_MASK=1`. Run 1: `GETV_VR_TWOHAND` unset. Run 2: `GETV_VR_TWOHAND=1`. Recenter both sticks. One gun in the **right** hand. Left hand empty. Do not dual-wield on the first sit.

**PASS — two-hand on**

1. “I put my left controller near the front of the right-hand gun and the orange left cube **jumps onto the gun** and stays there, like a support hand, not a floating box.”
2. “If I hold that pose, the cube **does not flicker** on and off.”
3. “If I pull my left hand away, the cube **comes back to my left controller** after a short gap, not instantly chatter.”
4. “The **gun still floats on the right controller**. I did not grow a Bond sleeve or a boxy finger mesh.”
5. “I can still **punch and poke a door with my left hand** when I am not supporting the gun. Melee and touch did not move to the barrel.”
6. “When I **put a gun in my left hand too**, the left cube **does not snap onto the right gun**. It hides or stays a normal left cube, not a glued support.”

**FAIL — any of these**

1. “The left cube just **vanishes** when I get close. It never sits on the gun.” (old hide-on-intersect)
2. “The cube **buzzes** between my hand and the gun when I hover.” (no hysteresis / enter==exit)
3. “The cube snapped on but now my **left punch / door poke / watch** happens at the gun, not at my left hand.” (B leaked in)
4. “Turning `TWOHAND` off does **not** restore tonight’s cubes.” (default / getenv wrong)
5. “`GUNARM` looks like a **suit arm** again.” (wrong fix)
6. “Dual-wield: left cube **glued to the right gun** while I am holding two weapons.”

**Also stare (regression, not the feature):** right-hand aim, squeeze ADS mark on the **gun ray**, touch-use with the **right** hand, casings. Two-hand must not move `GUNAIM` / `GUNMOUNT`.

---

## 5. Director decision

| If you say… | Then… |
|-------------|--------|
| **Green A** | Workshop APPLY: `GETV_VR_TWOHAND` default OFF, draw-pose snap + hysteresis + dual-wield skip. Sit with the sentences above. |
| **Green A + hide** | Same, and ship the already-planned dual-wield cube hide in the same draw site (still not GHOSTHAND). |
| **Reject — hide is enough** | Stop. Leave intersect hide. No new knob. |
| **Want B** | Sit A first. Only then snap left feed. Expect melee/touch fallout. |
| **Want ghost fingers** | Park. Separate cut. Not this snap. |

---

## 6. Attribution / legal

- No personal credit paths edited.
- No GoldenEye ROM, assets, or dumps.
- PD two-hand numbers are **map-only** (already in public GEVR textbook). Do not copy PD sources into product.
- Workshop C stays private until release policy flips.
