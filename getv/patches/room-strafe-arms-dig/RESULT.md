# RESULT — real-world strafe off playspace center messes up arm/gun position (DIG ONLY)

**Status:** DIG. **Not APPLY READY on this public tree.** No C landed. **No ship.**
**Ask ([GEVR #74](https://github.com/no6969el/GEVR/issues/74), owner 2026-09-20):** physically strafing in the room away from the recentered playspace spot makes VR **arms / guns feel slightly wrong / off position**. Have to recenter often (both sticks / Home). **Not** stick-strafe only — **HMD / playspace translation** relative to origin.
**Wear:** public **vr442 / Latest**. Path: `GETV_VR_GUNARM=1` + grip matrix (`geVrGetWeaponModelMatrixF` / workshop `geStereoXrGunMount`). `BODY=0`, `BODY_NOARMS=1`, left cube `MASK=1`.
**Constraints:** Recenter chord already ships (`RECENTER_CHORD`). [#45](https://github.com/no6969el/GEVR/issues/45) playspace **floor** is a different ask (height / reset spot). `HEADYAW` / `AUTORECENTER` stay KEEP — do not casually disable. Parked `GUNZ` / `HANDSOLID` (vanish below chest) stay off. Two-hand snap DIG (PR #4) stays parked.
**Date:** 2026-09-20.
**Evidence:** public `goldeneye-native` HEAD, public `no6969el/GEVR` #74 / #45 / #39 + textbook/packaging (vr442 boot), public `n64decomp/007` FP gun camspace. Workshop `geStereoXr*`, `HANDYAW` body, playspace comfort C are **not on any public remote** (`GEVR` `docs/RELEASE-POLICY.md`). Brief names are the workshop symbols.

**#45 playspace-floor DIG:** **not present** in this repo (`getv/patches/` has no playspace/floor RESULT). Shipped #45-family wear is the vr442 boot pin set below (`PLAYSPACE`, `RECENTER_YAWONLY`, `FLOOR_M`, `FLOOR_INJECT=0`). This page is **arms vs origin**, not floor height.

Director can green-light Rank 1 rebase sit, Rank 1 + YAWONLY A/B, park, or reject from this page alone.

---

## Verdict (one screen)

| Question | Answer |
|----------|--------|
| Parenting tonight? | **Mixed frames.** Guns are **not** Bond-body (BODY=0). They are **floating GUNARM** at OpenXR **grip**. Head / view go through **recenter + `HEADYAW`**. Grip / aim on the public ABI are **raw stage**. Workshop then remaps hand **yaw** (`HANDYAW=2`, `LEVELYAW=1`) and playspace comfort (`PLAYSPACE=1`, `RECENTER_YAWONLY=1`). Physical **XZ is not in the same parent as the camera.** |
| Lag vs skew? | **Skew / origin, not a pose-age lag.** Hands already locate at the same `predictedDisplayTime` as the views (`xr_input.cpp`). Recenter **fixes** it → snapshot / origin, not a missed `xrLocateSpace`. |
| Grip vs view / IPD on strafe? | **Yes, distinct from IPD mesh stereo.** vr442 pins `HEAD_TRANSLATE=0` so the **view origin stays on the Bond capsule**. `HEADYAW_IPD=1` still writes IPD through the head-yaw path. Grip keeps **stage XZ**. Sidestep moves the fist in playspace while the eye assumes the **centered standing** pose. |
| Same as #39 GUNEYE? | **No.** #39 is **zero disparity both eyes** (huge + brief cross-eye **at rest**). #74 is **offset / skew after room strafe**, gone after recenter. Do not chair `GUNEYE` as this fix. |
| Same as #45 floor? | **No.** #45 is floor / reset-spot comfort (`FLOOR_M=-0.200`, `FLOOR_INJECT=0`, yaw-only recenter). FEATURES “comfort pass” is **look-around shear**, not fist-vs-origin after a sidestep. Related family; keep issues split. |
| Why recenter “fixes” it | Chord re-snaps the **yaw / standing snapshot** the gun yaw-remap uses. `RECENTER_YAWONLY=1` means that snap may **not** eat XZ — which is why the owner has to do it **often** as they wander. |
| Wear-only tonight? | **No.** Unset knobs keep the off-hand feel after a room sidestep. |
| APPLY tonight? | **No from this repo.** Rebase belongs next to `geStereoXrGunMount` / `geVrGetWeaponModelMatrixF` on the workshop. |

```
stage / playspace     hands, cubes, raw grip/aim          (xrLocateSpace → stage)
recenter snapshot     yaw (+ XZ only if not YAWONLY)      (chord / AUTORECENTER)
HEADYAW / HEADFRAME=2 game camera yaw + IPD               (KEEP; do not flip)
HEAD_TRANSLATE        camera XZ lean                      (vr442 = 0  — #70 PARK)
GUNMOUNT / GUNARM     world gun at grip + HANDYAW=2       (KEEP ON)
BODY                  colocated torso                     (0; not this ticket)

sidestep 0.5–1 m without stick
  → fist XZ moves in stage
  → eye stays on capsule (HT=0) and/or last yaw-only origin
  → HANDYAW rotates that leftover around the WRONG pivot
  → arms “slightly off” until chord
```

---

## 1. Root finding (files + functions)

### 1.1 Evidence boundary

| Layer | Where | What it proves |
|-------|--------|----------------|
| Public `goldeneye-native` | this repo | Recenter is **head-only**. Grip / aim matrices are **raw**. Eyes located in **stage**. Comfort clamp is **camera-only**. |
| Public `no6969el/GEVR` | #74 (filed this ask), #45, #39, `FEATURES.md`, `docs/CONTROLS.md`, `docs/86-6DOF-PLAN.md`, `packaging/templates/gevr-vr442-boot.cmd`, `KEEP-DEFAULTS-INVENTORY-vr441.md` | Wear, shipped pins, PD **viewSpace** prior art, comfort-pass copy. |
| Public decomp | `n64decomp/007` `gunfire.c` / `bondview.h` | Stock FP gun is **camspace** + `G_MTX_LOAD`. GUNMOUNT replaces that with a controller pose. |
| Workshop (private) | `geStereoXrGunMount`, `geStereoXrHandWorld`, `HANDYAW` / `PLAYSPACE` bodies | Live composition. **Do not push.** First chair grep: `getenv("GETV_VR_GUNREBASE")` / `HANDYAW` / `RECENTER_YAWONLY`. |

`geStereoXrGunMount` / `geStereoXrHandWorld` / live `HANDYAW` **do not appear in any public file**.

### 1.2 Public ABI — head is recentered; gun is not

`src/ge_vr_bridge.cpp` is the only host place OpenXR and GoldenEye meet. The recenter is a **yaw + XZ offset**, not a new OpenXR reference space (`xr_session.cpp` says the same).

```59:68:src/ge_vr_bridge.cpp
Pose recenteredHead(const BridgeState& s) {
    Pose p = s.frame.head;
    const Quat yaw = quatFromAxisAngle(Vec3{0, 1, 0}, -s.recenter_yaw);
    p.orientation = quatMul(yaw, p.orientation);
    Vec3 d{p.position.x - s.recenter_pos.x,
           p.position.y - s.recenter_pos.y,
           p.position.z - s.recenter_pos.z};
    p.position = quatRotate(yaw, d);
    return p;
}
```

Callers of that helper: `geVrGetHeadAngles`, `geVrGetHeadPosition`, comfort fade, **and** `geVrGetEyeViewOffsetF` (lean). **Not** the gun.

```271:282:src/ge_vr_bridge.cpp
extern "C" int geVrGetWeaponModelMatrixF(GeVrHand hand, float mf[4][4]) {
    // ...
    // Grip pose, not aim pose: the model should sit in the fist.
    const Mtx4 m = modelMatrixFromPose(s.input.hand[h].grip, GE_VR_UNITS_PER_METRE);
    // raw copy — no recenter_yaw / recenter_pos
```

Same raw stage pose:

| Helper | Space tonight (public ABI) | Job |
|--------|----------------------------|-----|
| `geVrGetHeadPosition` / `GetHeadAngles` | **recentered** head | Camera / `HEADYAW` writer |
| `geVrGetEyeViewOffsetF` | recentered head⁻¹ × **raw** eye + clamped lean | IPD / roll / XZ lean |
| `geVrGetWeaponModelMatrixF` | **raw grip** × 100 u/m | Fist / weapon **draw** |
| `geVrGetAimRay` | **raw aim** | Hitscan |
| `geVrGetWeaponDisplacement` | recentered head angles **minus raw** aim angles | Sway fields — **mixed frames** |
| `geVrWatchGestureActive` | raw head vs raw grip (relative) | OK (same space) |

`modelMatrixFromPose` (`src/xr_math.cpp`) is a straight model matrix. No parent, no Bond, no eye.

`geVrRecenter()` stores head yaw + **XZ** (`y` forced 0 — height is calibration / #45 `FLOOR_M`). That is the public chord. Workshop may **narrow** it (`RECENTER_YAWONLY=1`).

### 1.3 Locate space — stage, not view

```57:65:src/xr_session.cpp
    // Two spaces, deliberately:
    //  - stage: the physical play area. 6DoF head position is measured here.
    //  - view:  the head. Used to derive head-relative quantities.
```

Eyes: `xrLocateViews(..., space = stage_space)`. Head: `xrLocateSpace(view_space, stage_space, t)`. Hands: `xrLocateSpace(grip/aim, base_space, t)` at the **same** `t` (`src/xr_input.cpp` — “or the gun and the world disagree”).

Public scaffold therefore gives **stage-absolute** fists. Perfect Dark (GEVR `docs/86`) does the opposite on purpose:

> `gCtrlPos` is **HEAD-RELATIVE.** They locate the controller against **`viewSpace`, not `playSpace`**. Gun placement is a plain sum — no head cancellation.

That is the single structural difference this DIG names. GEVR `FEATURES.md` still claims “walk around your playspace and Bond walks with you” **and** “the playspace comfort pass so straying off your reset spot does not **shear the gun and world when you look around**.” Look-around shear ≠ sidestep fist lock.

Layer submit still uses **raw** `views[eye].pose` in `stage_space` (`xr_session.cpp`). Vision-jitter DIG already called a draw-recenter vs raw-layer pair a **yaw lie**. #74 can wear that as **per-eye warp after you leave the origin**, but the owner sentence is **arms / guns off**, not “two of everything.” Sit T1 (cover one eye) splits it.

### 1.4 vr442 pins that load this wear

`packaging/templates/gevr-vr442-boot.cmd` (public GEVR):

| Knob | vr442 | Class | Role vs #74 |
|------|-------|-------|-------------|
| `GETV_XR_HEAD_TRANSLATE` | **0** | PARK (#70 / #55) | Camera **does not** take physical XZ. C-unset in this repo is still **ON**. |
| `GETV_XR_PLAY_AUTORECENTER` | `1` | KEEP | Cinema → play chord. **Do not flip.** |
| `GETV_XR_RECENTER_CHORD` | `1` | KEEP | Both sticks / Home. Ships. |
| `GETV_XR_RECENTER_YAWONLY` | **1** | KEEP | Comfort pass: yaw snap, **not** full origin. Prime #45 leftover. |
| `GETV_XR_FLOOR_INJECT` | `0` | DIG_OFF | #45 height path off. |
| `GETV_XR_FLOOR_M` | `-0.200` | KEEP | #45 floor offset. Not a gun parent. |
| `GETV_VR_HEADYAW` / `_IPD` | `1` / `1` | KEEP | Head into `vv_theta` + IPD. **Do not flip** for this sit. |
| `GETV_VR_HEADFRAME` | `2` | KEEP | Ship enum. Body private. |
| `GETV_VR_HANDYAW` | **2** | KEEP | Hand yaw remap into the level / head-yaw frame. |
| `GETV_VR_LEVELYAW` | `1` | KEEP | Same family. |
| `GETV_VR_PLAYSPACE` | `1` | KEEP | Origin comfort (look-around). |
| `GETV_VR_GUNMOUNT` / `GUNAIM` / `GUNARM` | `1` | KEEP | Floating VR guns. **Keep.** |
| `GETV_VR_BODY` / `BODY_NOARMS` | `0` / `1` | parked / KEEP | No Bond sleeve. |
| `GETV_VR_GUNZ` / `HANDSOLID` | `0` | parked | Vanish below chest. **Do not revive.** |

Public stub `ge_vr_playspace()` in `getv/port/src/port_render.c` still C-defaults **0** when unset. Boot and `GRADUATED-KNOBS.md` want **1**. The playable tree is the workshop + bat, not this stub.

`geVrGetEyeViewOffsetF` clamps lateral lean to **45 game units (~0.45 m)** and **never** writes the capsule (`ge_vr.h` §6.2). Owner stare is **0.5–1 m**. Even if HT were on, camera would **stop** at ~0.45 m and a stage-absolute gun would **keep going**. vr442 HT=0 makes the split show up **immediately**.

### 1.5 Workshop gun path (names only)

Already mapped by the GUNEYE / two-hand / body DIGS:

- `geStereoXrGunMount` under `GUNMOUNT=1` writes a **world / controller** pose into the FP gun (replaces `gunmtx_camspace`).
- Left cube: `geStereoXrHandWorld` + `HandBasis` in `geVrHandCubesRender`.
- If that pose is **stage + HANDYAW** baked on the **sim / first eye** through a **Bond-centered** view (`HEAD_TRANSLATE=0`), both eyes `G_MTX_LOAD` a fist that walked away from the camera.

`HANDYAW=2` is why the fail can read **slight / skewed** rather than “the gun is a metre to the left.” A yaw remap of an un-rebased XZ rotates the leftover around the **old** origin. Recenter writes a new yaw snapshot → snap back.

### 1.6 Not this ticket

| Ticket / knob | Why it stays out |
|---------------|------------------|
| [#39](https://github.com/no6969el/GEVR/issues/39) `GETV_VR_GUNEYE` | Same matrix both eyes. Huge + cross-eye **standing still**. Lean 20 cm sit on that DIG is “gun stays on hand.” |
| [#45](https://github.com/no6969el/GEVR/issues/45) floor | Floor too high/low after recenter. `FLOOR_M` / `FLOOR_INJECT`. |
| [#41](https://github.com/no6969el/GEVR/issues/41) BODY | Torso + IK later. `BODY=0` stays. Do not “get arms” by flipping `NOARMS`. |
| [#59](https://github.com/no6969el/GEVR/issues/59) `HEADYAW=0` | Head-move clicks. Do not KEEP-off `HEADYAW` to chase #74. |
| [#70](https://github.com/no6969el/GEVR/issues/70) / [#55](https://github.com/no6969el/GEVR/issues/55) `HEAD_TRANSLATE=1` | Restoring lean reopens Dam modem / portal black. A/B only. |
| `GETV_VR_GUNZ` / `HANDSOLID` | Parked vanish. Not a parent fix. |
| Two-hand snap (PR #4) | Near-gun latch. Different stack. |
| Stick strafe | Game capsule. Owner: **physical** sidestep. |

---

## 2. Answers to the five investigate questions

### 2.1 Parent — HMD, playspace origin, or Bond?

**Bond body: no.** `BODY=0`. What the owner calls “arms” tonight is **GUNARM guns + left cube**.

**HMD yaw: partially.** `HEADYAW=1` writes head into the **game camera**. `HANDYAW=2` + `LEVELYAW=1` remap **hand yaw** into that / the level frame. That is why look-around after the comfort pass mostly works (FEATURES).

**HMD position: no (vr442).** `HEAD_TRANSLATE=0`. Camera XZ stays on the capsule. Public lean helper exists but is parked in the zip.

**Playspace / stage origin: yes for translation.** Grip and cubes are located in **stage**. Recenter on the public ABI **would** rebase head XZ; workshop **YAWONLY=1** likely **does not**. Guns stay parented to the **last full origin** (spawn / cinema `AUTORECENTER` / a chord that still wrote XZ once).

So: **yaw follows HMD / level; XZ follows playspace origin.** Room strafe without a full origin snap is the disagree.

### 2.2 Does strafe change IPD / view origin while the gun assumes a centered pose?

**View origin:** with `HEAD_TRANSLATE=0`, **no** — the eye stays Bond-centered. The **physical** IPD does not change when you sidestep. `HEADYAW_IPD=1` still composes IPD through the **head-yaw** writer, so a sidestep + glance can move the **stereo origin** without moving the gun parent.

**Gun:** `geVrGetWeaponModelMatrixF` has **no** `current_eye` and **no** head subtract. GUNMOUNT assumes the controller pose is already in the same world the camspace bake expects — the **centered standing** frame after recenter.

That is **view-origin vs fist-origin**, not “IPD changed.” #39 is the IPD-on-the-**mesh** ticket.

### 2.3 Same as #39 or distinct?

**Distinct.**

| | #39 GUNEYE | #74 room strafe |
|--|------------|-----------------|
| When | Looking at the gun **still** | After **walking sideways** in the room |
| Feel | Huge + brief **cross-eye** | Arms / guns **off / skewed** |
| Fix tonight | Recenter does **not** shrink the gun | Recenter **does** put fists back |
| Knob family | Per-eye gun translation | Head-relative rebase / origin |

Chair both if both wear. Do **not** merge APPLY.

### 2.4 Smallest falsifier (default OFF)

Rank **1** only for the first sit. Do not stack 2/3 until 1 has a B0 + PLAY0 pair.

| Rank | Knob (unset / 0 = tonight) | What it does | Why this order |
|------|----------------------------|--------------|----------------|
| **1** | `GETV_VR_GUNREBASE=1` (workshop alias OK: `GUNHEADREL`) | Each frame, express grip / left-cube as **head-relative** (PD `viewSpace` / `head⁻¹ × grip`), then add the **current** camera. Yaw uses the same number as `HEADYAW`. | Smallest structural fix. Does **not** flip `HEADYAW`, `AUTORECENTER`, `GUNARM`, HT, GUNZ. |
| **2** | `GETV_XR_RECENTER_YAWONLY=0` | Chord (and only the chord) writes **full origin** again. | Diagnosis: if Rank 1 is refused, does a **full** recenter stop the wander? Owner already recenters — this is not a ship fix. |
| **3** | `GETV_XR_SOFTXZ=1` | Auto rebase **XZ only** to current HMD each frame (or when lateral > N cm). Yaw untouched. | Seated-like. On HT=0 this is “fists stay, world does not walk.” Conflicts with CONTROLS “walk the room = Bond walks” **if** HT is later restored. |

**A/B, not a fix:** scratch `GETV_XR_HEAD_TRANSLATE=1`. If #74 **dies** and #70/#55 **return**, the gun is not eating the same lean as the eye. APPLY is “copy the lean term onto GUNMOUNT,” **not** ship HT=1.

**Do not name** a new `GETV_VR_GUNZ`. **Do not** `HEADYAW=0` / `AUTORECENTER=0` / `PLAYSPACE=0` / `GUNARM=0`.

### 2.5 Plain stare (owner)

See §4 Run S. **PASS** = guns stay locked to the controllers after a 0.5–1 m sidestep, no chord. **FAIL** = arms drift / skew until recenter.

---

## 3. Proposed knobs — **default OFF** — NOT LANDED

All getenv: unset / empty / `0` = **tonight**. Explicit `1` is chair-only. **Not KEEP-ON.** Do **not** add to `gevr-vr442-boot.cmd` / `$requiredBootKnobs` until a sit PASS.

### 3.1 Rank 1 — `GETV_VR_GUNREBASE` (recommend first)

```
GETV_VR_GUNREBASE        unset / empty / 0 = OFF
                         1 = head-relative grip + cube each frame
GETV_VR_GUNREBASE_TRACE  0   DIG_OFF
```

Workshop only. Confirm live names (`geStereoXrGunMount`, `geStereoXrHandWorld`, or a shared `geStereoXrHandLocal`).

```c
/* NOT APPLY READY — workshop sketch.
 * Next to geStereoXrGunMount / HandWorld (same TU as GUNMOUNT).
 * Tick per sim / first eye (lvframe60). Same pose both eyes.
 * HEADYAW / AUTORECENTER / GUNARM / HT pins unchanged. */

static int ge_vr_gunrebase(void)
{
    static int on = -1;
    if (on < 0) {
        const char *e = getenv("GETV_VR_GUNREBASE");
        on = (e != NULL && *e != '\0') ? (atoi(e) != 0) : 0; /* default OFF */
    }
    return on;
}

/* if (ge_vr_gunrebase()) {
 *     /* PD docs/86: locate grip in viewSpace, or
 *        grip' = recenterYaw * (rawGrip - rawHead.xz)   -- keep grip Y
 *        then gunWorld = bondCameraOrigin + grip'
 *        HandWorld(LEFT) = same parent as the gun
 *     */
 * }
 * do NOT write vv_theta
 * do NOT call geVrRecenter() every frame
 * do NOT enable GUNZ / HANDSOLID
 */
```

Public-tree note: the honest host fix is the same helper on `geVrGetWeaponModelMatrixF` **and** `geVrGetAimRay` (one yaw number, rule 8 / `docs/86`). Displacement then compares two recentered poses. **Not APPLY READY here** — no playable GETV body.

If PLAY0 locks the fists and **aim** walks away, the bake ate draw and missed the ray — same sit, second line, still Rank 1.

### 3.2 Rank 2 — `GETV_XR_RECENTER_YAWONLY=0` (A/B only)

Already a KEEP **1**. Scratch `0` on a copy of Latest. **Restore 1 before leaving** if Rank 1 is the APPLY.

YAWONLY-PASS: “I sidestep; guns stay wrong until I chord; after a **full-origin** chord they stay put **even if I sidestep again**.” That would mean the live bug is **frozen XZ origin**, and Rank 1 is still the right APPLY (you should not have to chord to walk).

YAWONLY-FAIL: “Full-origin chord still needs a re-chord after the next sidestep.” Origin snapshot is not enough — you need per-frame rebase (Rank 1) or SOFTXZ.

### 3.3 Rank 3 — `GETV_XR_SOFTXZ` (only if Rank 1 refused and YAWONLY A/B says origin is the leftover)

```
GETV_XR_SOFTXZ     unset / 0 = OFF
                   1 = rebase recenter_pos.xz to current HMD each frame (yaw kept)
```

This **is** auto-soft-recenter **XZ-only**. It is **not** `HEADYAW=0`. It will make physical walk **not** move Bond if HT is on. On vr442 (HT=0) it is a **fist** fix. Prefer Rank 1 (local gun parent) so a later HT restore does not silently eat room-scale.

### 3.4 Out of scope

- `GETV_VR_HEADYAW=0` / `HEADYAW_IPD=0` / `HEADFRAME` retune
- `GETV_XR_PLAY_AUTORECENTER=0`
- `GETV_VR_PLAYSPACE=0` as a “fix”
- `GETV_XR_HEAD_TRANSLATE=1` as ship
- `GETV_VR_GUNARM=0` / BODY / `BODY_NOARMS=0`
- `GETV_VR_GUNZ` / `HANDSOLID` / `GUNZTEST`
- `GETV_VR_GUNEYE` / `GUNSCALE` / `UNITS_PER_M` (#39)
- `GETV_XR_FLOOR_INJECT` / `FLOOR_M` retune (#45)
- Two-hand snap / watch-on-arm
- KEEP-ON of GUNREBASE until sit PASS
- Personal credit paths. ROM dumps

---

## 4. Chair stare (plain tester sentences)

**Setup:** vr442 Latest zip + `Start-GEVR.bat`. Boot keeps `GUNARM=1`, `GUNMOUNT=1`, `BODY=0`, `BODY_NOARMS=1`, `HANDCUBES=1`, `MASK=1`, `HEADYAW=1`, `AUTORECENTER=1`, `RECENTER_CHORD=1`, `RECENTER_YAWONLY=1`, `HEAD_TRANSLATE=0`, `PLAYSPACE=1`. Recenter both sticks. Dam or Facility. **Right hand holds a gun. Left empty (cube).** No stick walk on the sidestep. **Do not** set `GUNEYE`, `GUNZ`, `HANDSOLID`, `BODY=1`, `HEADYAW=0`.

**Run B0 — knobs unset (must match tonight):**

| | Tester sentence |
|--|-----------------|
| **B0-PASS 1** | “I recenter, hold the guns out, **sidestep 0.5–1 m** without the stick. The gun / left cube **drift or skew**. I have to recenter to put them back.” |
| **B0-PASS 2** | “Stick-strafe only (feet planted) does **not** do this.” |
| **B0-PASS 3** | “Standing on the reset spot, turning my head, the comfort pass still holds — the world does **not** shear the way it did before PLAYSPACE.” |
| **B0-FAIL** | “Sidestep already keeps the gun welded to the controller.” **Stop. This binary is not vr442 Latest, or #74 is not wearing.** |

**Run S — owner stare (B0 is the FAIL we are fixing):**

| | Tester sentence |
|--|-----------------|
| **S-PASS** | “Recenter, guns out, physical sidestep 0.5–1 m, no stick. **Guns stay locked to the controllers.** I do not want a recenter.” |
| **S-FAIL** | “Arms drift / skew until I chord.” |

**Run P0 — `GETV_VR_GUNREBASE=1` only:**

| | Tester sentence |
|--|-----------------|
| **P0-PASS 1** | S-PASS. |
| **P0-PASS 2** | “I glance and stick-turn. Aim is still on the **gun ray**. `HEADYAW` still turns the world with my head.” |
| **P0-PASS 3** | “Cinema → mission still **autorecenters**. I did not lose the chord.” |
| **P0-PASS 4** | “Empty left cube stayed on my left hand. Dual-wield both guns stayed on both hands.” |
| **P0-PASS 5** | “Cover one eye: it is **offset-fixed**, not a new cross-eye. (#39 still whatever it was at rest.)” |
| **P0-FAIL** | “Still skewed.” / “Gun welded to my face.” / “Aim moved off the barrel.” / “I have to recenter to enter play.” / “`HEADYAW` died.” / “Gun vanished below my chest” (GUNZ family — wrong patch). |

**Run Y — Rank 2, only if P0 is refused / not built:**

Scratch `GETV_XR_RECENTER_YAWONLY=0`. One chord, then sidestep **without** another chord.

| | Tester sentence |
|--|-----------------|
| **Y-PASS** | “After **one** full-origin recenter I can sidestep and the guns stay.” → leftover is frozen XZ; still APPLY Rank 1 (do not make people chord). Restore YAWONLY=1. |
| **Y-FAIL** | “I still need to chord after every sidestep.” → per-frame rebase, not a fatter chord. |

**Run H — HT A/B, only if P0-FAIL and you must know camera vs gun:**

Scratch `GETV_XR_HEAD_TRANSLATE=1` on Latest. **Restore 0.** If S-PASS **and** Dam modem / portal black return, copy lean onto the gun; do **not** ship HT=1 for #74.

**Run Q — Quest 3 / VDXR vs Pimax (same P0):**

Same sidestep. PASS = both keep the fist. FAIL-only-on-one → runtime `LOCAL` vs `STAGE` (`xr_session.cpp` already falls back to LOCAL). Still Rank 1; do not invent a Quest scale knob.

**Falsifier (optional, not a fix):** `GETV_VR_PLAYSPACE=0` on B0. If that **alone** kills the skew **and** brings back look-around shear, #74 is a hole **in** the comfort pass (yaw remap without XZ). Put PLAYSPACE back to 1 before leaving; APPLY is still Rank 1, not PLAYSPACE=0.

**Regression (every run):** trigger, squeeze ADS on the **gun ray**, B reload, both-stick recenter, cinema autorecenter, left cube when empty, no Bond sleeve, no GUNZ vanish. #39 size / sting unchanged. #45 floor height unchanged (`FLOOR_M`).

---

## 5. Director decision

| If you say… | Then… |
|-------------|--------|
| **Green Rank 1** | Workshop sit §4 B0 + P0 + Q. Public zip stays vr442 until PASS. Then boot allowlist. |
| **Green 1 then Y** | Only if you also want the YAWONLY story on file. Restore YAWONLY=1 after. |
| **Ship SOFTXZ / per-frame `geVrRecenter`** | Rejected as first APPLY. Eats room-scale the moment HT returns. |
| **Flip `HEADYAW` / `AUTORECENTER` / `PLAYSPACE` off** | Rejected. Keepers toward the body / play path. |
| **Restore `HEAD_TRANSLATE=1` as the #74 fix** | Rejected. Reopens #70 / #55. A/B only. |
| **Revive GUNZ / HANDSOLID** | Park. Vanish below chest is a known FAIL. |
| **Merge #39 / #45 / #41 / two-hand** | Rejected. |
| **Want Bond arms so they “follow”** | #41 later. Wrong parent will fake-attach. |
| **Park / reject** | Stop. Testers keep recentering after they wander. |

---

## 6. GEVR issue

**Do not file a new issue.** [GEVR #74](https://github.com/no6969el/GEVR/issues/74) already **is** this ask (opened 2026-09-20, label `bug`, “Dig started.”).

Suggested #74 comment (human / Director — this agent cannot write GitHub issues):

> DIG (no APPLY) on `goldeneye-native`: `getv/patches/room-strafe-arms-dig/RESULT.md`.  
> Distinct from #39 (GUNEYE / rest stereo) and #45 (floor / `RECENTER_YAWONLY`).  
> First sit: `GETV_VR_GUNREBASE=1` default OFF — head-relative grip each frame.  
> Do not flip `HEADYAW` / `AUTORECENTER`. Do not restore `HEAD_TRANSLATE=1` as the fix.

Related, do **not** close: #45 (floor), #39 (gun scale / cross-eye), #41 (body).

---

## 7. Attribution / legal

- No personal credit paths edited.
- No GoldenEye ROM, assets, or dumps.
- PD viewSpace / one-yaw-recenter notes are **map-only** (public GEVR `docs/86`). Do not copy PD sources.
- Workshop C (`geStereoXr*`, live `HANDYAW` / playspace bodies) stays private until release policy flips.
- Decomp citations are `n64decomp/007` (all rights reserved); this DIG describes call sites, it does not vendor the game.
