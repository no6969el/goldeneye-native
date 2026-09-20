# RESULT — melee / hand hit should require a real swing (#75) (DIG ONLY)

**Status:** DIG. **Not APPLY READY on this public tree.** No C landed.
**Ask (GEVR public tracker, not this repo):** testers kill guards by **standing next to them**. Hand / melee hit should register only on an **actual swing** (controller velocity / punch gesture), not idle proximity.
**Wear:** vr442 / Latest. HMD. Hands near NPCs without intending to attack.
**Constraints:** Related family **HANDSOLID / hand collision (U-23)**. Do **not** confuse with **gun fire**. New knob default **OFF**. Do **not** ship ON without a chair. Do **not** turn `HANDMELEE` off as the “fix.”
**Date:** 2026-09-20.
**Evidence:** public `goldeneye-native` HEAD, public `no6969el/GEVR` textbook + vr441 boot / FEATURES / CONTROLS / ship checklist / #75 / #40, public `n64decomp/007` `gun.h` / `chr.h`. Workshop HANDMELEE / TOUCHUSE bodies (`geTouchBoxDist`, `geStereoXrHandWorld`) are **not on any public remote** (`GEVR` `docs/RELEASE-POLICY.md`). Brief names are the getenv strings plus workshop symbols from the two-hand DIG.

Director can green-light Rank1 `SWINGHIT`, a later radius sit, park, or reject from this page alone.

---

## Verdict (one screen)

| Question | Answer |
|----------|--------|
| Why idle proximity kills | **`GETV_VR_HANDMELEE=1` is KEEP_SHIP.** Hit is **fist vs chr volume** (`d < HANDMELEE_R`) plus **`_COOL=30`**. No swing / speed / gesture gate. Standing next to a guard puts an idle controller inside that volume. |
| Not gun fire | Trigger / `GUNAIM` / `HITSNAP` / `gunUpdateAndFireBothHands` are a **different** path. This is the **hand-contact** overlay. |
| Not HANDSOLID / U-23 | **`GUNZ` / `HANDSOLID`** = parked **gun vanish below chest** (world clip). U-23 is that **hand-collision** family. **Do not revive it** to “fix” melee. |
| Not TOUCHUSE | Door / console poke (`TOUCHUSE=1`, `_R=12`). Same distance helper **pattern**. Different consumer. Leave it. |
| Smallest falsifier | **`GETV_VR_SWINGHIT` unset / empty / `0` = OFF** (tonight). **`=1` = require a swing-speed floor** on an otherwise unchanged HANDMELEE hit. |
| APPLY tonight? | **No from this repo.** Gate is small **on the workshop**. Bodies are private. Do not land a stub. |

```
tonight (vr442 boot)
  HANDMELEE=1  R=8  COOL=30
  fistWorld vs chr box   d < R  &&  cooldown elapsed  →  punch damage
  no |v| test

SWINGHIT=0 (unset)  →  that path  (idle stand still kills)
SWINGHIT=1          →  same overlap + cooldown, but only if |v_grip| >= SWINGHIT_V
HANDMELEE=0         →  NOT the sit  (kills all punches; hides the gate)
```

---

## 1. Root finding (files + functions)

### 1.1 Evidence boundary

| Layer | Where | What it proves |
|-------|--------|----------------|
| Public `goldeneye-native` | this repo | Host-agnostic VR ABI. `geVrGetAimRay` = **barrel / hitscan**. `geVrGetWeaponModelMatrixF` = **grip / fist**. `HandState` has **pose only** — **no velocity**. `xr_input.cpp` `xrLocateSpace` does **not** chain `XrSpaceVelocity`. |
| Public `no6969el/GEVR` | docs + `packaging/` | `HANDMELEE=1` KEEP_SHIP. `_R=8` / `_COOL=30` PLAYER_PREF. FEATURES: *“Punch / melee with your hands.”* HANDSOLID parked with GUNZ. #40 TOUCHUSE already ships. #75 is this ticket. |
| Workshop (private) | `F:\Projects\GEVR\GoldenEyeVR\goldeneye-native` | HANDMELEE tick, TOUCHUSE, `geTouchBoxDist`, `geStereoXrHandWorld`. **Do not push.** |
| Public decomp | `n64decomp/007` `gun.h` / `chr.h` | N64 punch SFX (`PunchSounds`) lives on the **gun / bondwalk** side. Chr damage is `chrHandleBulletHit` / `chrpropAddBulletHit` / `chrAddHealth`. VR HANDMELEE is a **spatial overlay**, not the fire-button punch. |

First chair grep: `getenv("GETV_VR_HANDMELEE")`, `HANDMELEE_R`, `HANDMELEE_COOL`, `geTouchBoxDist`. Two-hand DIG already placed TOUCHUSE / HANDMELEE next to `geVrHandCubesRender` / `geStereoXr*` in workshop `stereo.c` (or the same TU). Exact melee function name is workshop-private; **do not invent a public alias** until the live symbol matches.

### 1.2 Tonight on the wear (vr441 boot, still the vr442 template)

`packaging/templates/gevr-vr441-boot.cmd` §5 and `KEEP-DEFAULTS-INVENTORY-vr441.md`:

| Knob | Value | Class | Job |
|------|-------|-------|-----|
| `GETV_VR_HANDMELEE` | `1` | KEEP_SHIP | Fist melee **on** |
| `GETV_VR_HANDMELEE_R` | `8` | PLAYER_PREF | Fist / pad radius (game units) |
| `GETV_VR_HANDMELEE_COOL` | `30` | PLAYER_PREF | Frames between hits |
| `GETV_VR_HANDMELEE_TRACE` | `0` | DIG_OFF | Census. Stay off in public boot. |
| `GETV_VR_TOUCHUSE` | `1` | KEEP_SHIP | Door poke. **Sibling, not this.** |
| `GETV_VR_TOUCHUSE_R` | `12` | PLAYER_PREF | Door radius. **Do not retune for #75.** |
| `GETV_VR_HANDCUBES` / `_MASK` | `1` / `1` | KEEP_SHIP | Left cube is the empty-hand stand-in. |
| `GETV_VR_GUNAIM` / `GUNMOUNT` | `1` / `1` | KEEP_SHIP | Gun **fire** / mount. **Not melee.** |
| `GETV_VR_HITSNAP` | `2` | KEEP_SHIP | Bullet-hole **placement**. **Not melee.** |
| `GETV_VR_GUNZ` / `HANDSOLID` | off / parked | COMING-SOON | Gun **vanishes below chest**. U-23 family. |

`docs/ship-feature-checklist.md` requires `GETV_VR_HANDMELEE=1` in the zip boot. Sit gates name **touch-use on a door**, not “idle stand next to a guard.” FEATURES ships punch as a wanted hand verb. There is **no swing sentence** in CONTROLS.

Two-hand DIG (`getv/patches/twohand-snap-dig/RESULT.md` §1.5):

> `GETV_VR_HANDMELEE` + `_R=8` + `_COOL=30` — ON. **Pattern + cooldown idea.** Fist radius — too tight *(for a gun-hide latch, not for chr contact)*.

That is the map: **overlap + cooldown**, same helper family as `TOUCHUSE` / `geTouchBoxDist`. **No speed term.**

### 1.3 Why standing next to a guard kills

Playspace is colocated (`GETV_XR_HEAD_TRANSLATE` / `PLAYSPACE` KEEP). A guard’s collision is a **chr cylinder / box** (`chrGetChrWidthHeight` / `chrUpdateCollisionBounds` in public `chr.h`), not an 8-unit marble in empty air.

`HANDMELEE_R=8` at `GETV_XR_UNITS_PER_M=100` is **~8 cm of pad** on that volume (two-hand DIG already called 8 “fist radius”). Idle hands at your sides, or a right-hand gun-fist held at chest height, sit **inside** a person-sized prop as soon as you stand next to them.

Cooldown **30** only spaces **repeat** hits (⅓ s at 90 Hz, ½ s at 60). The **first** overlap still registers. No `|v|` test means a still controller is enough.

Both hands: left empty cube **and** right grip/fist. Armed right hand does **not** disable HANDMELEE. Dual-wield / two-hand snap DIG already warned: leave melee on the **real** controller, not the barrel.

Stereo trap (same as TOUCHUSE / two-hand): if the tick is not **sim-owner / first-eye only**, a frame can fire twice. That is a **double-hit** bug, not the idle-kill bug. Rank1 must keep the existing once-gate if one exists; do not add a second path.

```
guard chr box  (width/height from chrGetChrWidthHeight)
       + HANDMELEE_R pad (8)
            ↑
idle grip / left cube   d < R  →  punch   (no swing)
```

### 1.4 What N64 punch is (so we do not “fix” the wrong site)

Public `gun.h` has `struct PunchSounds`. Public `chr.h` applies hits through `chrHandleBulletHit` / `chrpropAddBulletHit`. `gunUpdateAndFireBothHands` / `gunTickGameplay` / `geVrGetAimRay` are the **fire / hitscan** family.

Retail punch is **fire-button + close** (unarmed / knife-class), not “hand volume vs standing guard.” VR HANDMELEE is the extra overlay FEATURES advertised. **Do not gate trigger fire** to sit #75. **Do not** require `ITEM_UNARMED` (watch-wheel DIG already uses that id for a **draw** swap). A swing-gated fist hit must still work **while a gun is held**, or the chair will read as “melee died.”

### 1.5 HANDSOLID / U-23 — related, not this

| Name | Job | Tonight | This DIG |
|------|-----|---------|----------|
| **HANDMELEE** | Fist **damage** on overlap | ON | **Gate with swing.** Keep ON. |
| **TOUCHUSE** | Fist **use** on overlap | ON | **Leave.** |
| **HANDSOLID / GUNZ** | Hand / gun **solid vs world**; known FAIL = **vanish below chest** | Parked off | **Do not revive.** |
| **U-23** | Issue’s name for that **hand-collision** family | Not in public U-doc tree (U-01…U-26 published; **U-23 absent**) | Cite as parked collision, not a damage knob. |

Public FEATURES-CURRENT / COMING-SOON: *“Gun vanish below chest (GUNZ / HANDSOLID) left off until fixed.”* Viewmodel DIG: `GUNZTEST=1` re-introduces that vanish. **#75 is a damage predicate, not a collision solid.**

### 1.6 Velocity is not on the public ABI

`include/ge_vr/ge_vr.h` hands: tracked bit, aim ray, weapon matrix, haptic. No `geVrGetHandVelocity`.

`src/xr_input.h` `HandState`: `aim`, `grip`, `trigger`, `tracked`.

`src/xr_input.cpp` `inputSyncFrame`: locate aim + grip pose. **No** `XrSpaceVelocity` next-chain.

Rank1 does **not** need a new ABI. Workshop already has last-frame `geStereoXrHandWorld` (or the HANDMELEE pose it already samples). Finite-difference `|p - p_last| * hz` is enough for a chair. OpenXR linear velocity can wait.

---

## 2. Rank1 knob sketch (default OFF)

### 2.1 Names

```
GETV_VR_SWINGHIT         unset / empty / 0 = OFF    (tonight; chair A)
                         1 = require swing speed     (chair B)
GETV_VR_SWINGHIT_V       PLAYER_PREF  speed floor    (game units / second)
GETV_VR_SWINGHIT_TRACE   0   DIG_OFF
```

**C-default when env unset: OFF.** Same getenv shape as `TWOHAND` / `WATCHFAR` / `GUNEYE`. Do **not** add to `$requiredBootKnobs` or vr441/vr442 allowlist until a sit PASS + KEEP call.

Do **not** C-default `HANDMELEE` to 0. Do **not** sit `_R` or `_COOL` as Rank1 (those change *who* you can reach, not *whether idle counts*).

First-chair `SWINGHIT_V` (PLAYER_PREF, not KEEP): **~80–150** game units / second at `UNITS_PER_M=100` (**~0.8–1.5 m/s**). Idle tremor is well below. A committed punch is well above. Natural walk-swing next to a guard is the **edge** the chair must name — do not “fix” that by shrinking `HANDMELEE_R` on the same night.

### 2.2 Where it lands (workshop)

**File:** same TU as `getenv("GETV_VR_HANDMELEE")` (workshop `stereo.c` / HANDMELEE tick). **One predicate** on the existing overlap success, **before** damage / `PunchSounds` / chr hit.

```c
/* NOT APPLY READY — workshop sketch.
 * Default OFF. Do not land in public goldeneye-native.
 * Tick sim-owner / first eye only (lvframe60 / dual-eye trap). */

static int ge_vr_swinghit(void)
{
    static int on = -1;
    if (on < 0) {
        const char *e = getenv("GETV_VR_SWINGHIT");
        on = (e != NULL && *e != '\0') ? (atoi(e) != 0) : 0; /* dig default OFF */
    }
    return on;
}

/* existing:
 *   if (!ge_vr_handmelee()) return;
 *   if (!geVrHandIsTracked(hand)) return;
 *   d = geTouchBoxDist(geStereoXrHandWorld(hand), chrBox);
 *   if (d >= HANDMELEE_R) return;
 *   if (cool[hand] > 0) return;
 *
 * Rank1 insert:
 *   if (ge_vr_swinghit()) {
 *       v = length(handWorld - lastHandWorld) * sim_hz;   // grip, not aim ray
 *       if (v < swinghit_v) return;                       // idle / drift
 *   }
 *
 *   apply punch (existing HANDMELEE damage / PunchSounds / chr hit);
 *   cool[hand] = HANDMELEE_COOL;
 */
```

**Why this is the smallest change:** one `if` on a path that already has overlap + cooldown. No new collision. No HANDSOLID. No fire remap. No ABI.

**APPLY READY?** On the **workshop**, yes, after the live HANDMELEE symbol is grepped. **Not APPLY READY here.** Do not land a stub.

### 2.3 What Rank1 does *not* change

Keep tonight: `HANDMELEE=1`, `_R=8`, `_COOL=30`, `TOUCHUSE=1`, `GUNAIM=1`, `GUNMOUNT=1`, `GUNARM=1`, `HITSNAP=2`, `HANDCUBES=1`. `BODY=0`. GUNZ / HANDSOLID stay parked.

---

## 3. Out of scope / must-not

- **APPLY / ship / C-default `SWINGHIT` ON.** Chair first. Then KEEP if PASS.
- **`GETV_VR_HANDMELEE=0` as the fix.** That is “melee died,” not “swing required.”
- **Retune `HANDMELEE_R` / `_COOL` as Rank1.** Later sit only if swing-PASS still clips on *reach*.
- **TOUCHUSE / door poke.** Same helper family. Different consumer. #40 stays closed.
- **Gun fire / `GUNAIM` / trigger / `HITSNAP` / `gunUpdateAndFireBothHands`.** Not #75.
- **Revive `GUNZ` / `HANDSOLID` / U-23 solidity.** Vanish-below-chest is a known FAIL.
- **`GETV_VR_TWOHAND` / HANDCUBES hide-snap.** Leave melee on the **real** controller.
- **`GETV_VR_BODY` / ghost hand / HANDMESH.** Parked / chair-refused.
- **New `geVrGetHandVelocity` ABI** for the first sit. Workshop pose delta is enough.
- **Require `ITEM_UNARMED` or empty hands.** Swing must work with a gun in the fist.
- **Per-eye double tick.** Do not add a second HANDMELEE caller.
- **Boot allowlist / pack smoke** until sit PASS.
- **Personal credit paths. ROM dumps.** Workshop C stays private.

---

## 4. Chair stare (plain tester sentences)

**Setup:** vr442 zip. `Start-GEVR.bat`. Recenter both sticks. Facility or Dam. Walk up to a **live** guard. Hands **still**. Do **not** pull trigger. Do **not** set `GETV_VR_HANDMELEE=0`. Do **not** set `GUNZ` / `HANDSOLID` / `GUNZTEST`. Do **not** set `GETV_STEREO=0`.

**Run A — tonight (unset `GETV_VR_SWINGHIT`):**

| | Tester sentence |
|--|-----------------|
| **A-PASS (repro)** | “I stand next to the guard with my hands hanging. I do **not** swing. The guard **dies** or takes a fist hit.” |
| **A-FAIL (no repro)** | “Idle stand does nothing. I have to punch to hit.” (then #75 is already false on this wear — stop; do not APPLY SWINGHIT.) |

**Run B — scratch `set GETV_VR_SWINGHIT=1` (same boot, same `HANDMELEE=1`):**

| | Tester sentence |
|--|-----------------|
| **B-PASS 1** | “I stand next to the guard with still hands. **Nothing.** He does not die.” |
| **B-PASS 2** | “I **punch / swing** through him. He **takes the hit** (flinch / death / punch sound).” |
| **B-PASS 3** | “I can still **poke a door** with a still or moving hand. Touch-use did not die.” |
| **B-PASS 4** | “Trigger still **fires the gun**. Squeeze ADS mark is still on the **gun ray**.” |
| **B-FAIL** | “Idle stand still kills.” / “I swing and nothing happens.” / “Doors ignore me.” / “The gun will not shoot.” / “The gun **vanished** when I dropped my hand.” (that last one is HANDSOLID / GUNZ — turn those off.) |

**Run V — only if B-PASS 1 and B-FAIL “swing does nothing”:** scratch `GETV_VR_SWINGHIT_V` lower (try `80`, then `50`). If idle kills again, the floor is too low — go back up. Do not touch `HANDMELEE_R` on this run.

**Regression (every run):** recenter, walk stick, right-stick turn, casings, Auto-Aim stays OFF. Two-hand cube still tonight (TWOHAND unset).

---

## 5. Director decision

| If you say… | Then… |
|-------------|--------|
| **Green Rank1** | Workshop APPLY §2.2 only. `SWINGHIT` default OFF. Sit A then B. KEEP-ON only after B-PASS 1+2+3+4. |
| **Green HANDMELEE=0** | Rejected by this dig. That is a melee-off bat, not a swing gate. |
| **Green shrink `_R` first** | Rejected as Rank1. Sit only after swing-PASS if reach is wrong. |
| **Green HANDSOLID / U-23** | Rejected. Wrong family. Vanish-below-chest FAIL. |
| **Green ABI velocity** | Park. Pose delta first. |
| **Reject** | Stop. Leave vr442 proximity punches. Docs already honest that hands punch. |

---

## 6. Attribution / legal

- No personal credit paths edited.
- No GoldenEye ROM, assets, or dumps.
- Public decomp citations are map-only.
- Workshop C stays private until release policy flips.
- No product C landed. This page is the sketch.
