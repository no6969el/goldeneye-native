# RESULT — GUNREBASE chair FAIL after HandWorld-only APPLY (REDIG ONLY)

**Tracker:** [GEVR #74](https://github.com/no6969el/GEVR/issues/74) — **canonical.** Do not open a second issue. Do not retarget this REDIG.
**Status:** REDIG. **Not APPLY.** No C landed. **No ship.** Workshop symbols stay **private** (names only).
**Chair FAIL ([#74](https://github.com/no6969el/GEVR/issues/74), 2026-09-20):** `GETV_VR_GUNREBASE=1` after workshop APPLY (HandWorld head-subtract in `stereo.c`). Physical strafe after recenter: guns + cube still move **OPPOSITE** the feet (same sign-flip as Rank 2 `YAWONLY=0` FAIL). HandWorld-only APPLY **insufficient**.
**Chair update (new playspace, same APPLY):** holding guns, physical walk — forward → hands further in front; back → hands toward body; left/right → hands overshoot that side (higher accel than walk). Reads as **double roomscale / gain > 1** with `HEAD_TRANSLATE=0` eye locked, not only opposite-sign. Still FAIL.
**Wanted:** fists always track controllers after recenter; playspace size / standing outside the playspace must **not** orphan hands.
**Prior:** DIG [PR #32](https://github.com/no6969el/goldeneye-native/pull/32) `getv/patches/room-strafe-arms-dig/RESULT.md`. Host APPLY [PR #33](https://github.com/no6969el/goldeneye-native/pull/33) `getv/patches/room-strafe-arms-apply/APPLY.md` (public ABI only). Workshop HandWorld APPLY on **SimRig only** — not in this remote.
**Wear:** vr442 / Latest. `GUNARM=1`, `GUNMOUNT=1`, `BODY=0`, `BODY_NOARMS=1`, left cube `MASK=1`, `HEAD_TRANSLATE=0`, `RECENTER_YAWONLY=1`, `PLAYSPACE=1`.
**Constraints:** Do not flip `HEADYAW` / `AUTORECENTER` / `PLAYSPACE` / `GUNARM`. Do not restore `HEAD_TRANSLATE=1` as this fix. Parked `GUNZ` / `HANDSOLID` stay off. Do not push workshop `geStereoXr*` bodies.
**Date:** 2026-09-20.
**Evidence:** [#74](https://github.com/no6969el/GEVR/issues/74) chair comments, public `goldeneye-native` HEAD (PR #32 / #33), public `n64decomp/007` `gunmtx_camspace` + `G_MTX_LOAD`, public GEVR `docs/86` / `194` / `196` / `200`. Workshop C is **not on any public remote** (`GEVR` `docs/RELEASE-POLICY.md`). Brief names are the workshop symbols.

Director can green-light GunMount / shared-parent REAPPLY, park, or reject from this page alone.

---

## Verdict (one screen)

| Question | Answer |
|----------|--------|
| Why HandWorld-only failed | **Wrong site.** `geStereoXrHandWorld` is the **left-cube** translation helper. Live FP-gun draw XZ comes from **`geStereoXrGunMount`** (replaces `gunmtx_camspace`). They are **siblings**, not parent/child. Subtracting head on HandWorld does not rebase the gun mesh. Host PR #33 never reached this bake. |
| Where live GUNMOUNT draw gets XZ | **Stage-absolute grip**, located against **stage** at `predictedDisplayTime`, then workshop GunMount writes that world/controller pose into the FP gun. **Not** HandWorld. **Not** `geVrGetWeaponModelMatrixF` unless workshop calls it (playable bake does not). `G_MTX_LOAD` both eyes from one sim / first-eye tick. |
| Sign flip? | **Symptom, not the APPLY.** First chair = opposite the feet (classic `196`). Rank 2 `YAWONLY=0` already produced that invert. Second chair (bigger playspace) = **same-direction gain > 1** (`200` double room term) with the eye locked. Wrong **term count**, not a sign to flip. |
| Host recenter on mount? | **Not the live path.** Public ABI already does `applyRecenterYawXz` then head-relative XZ when `GUNREBASE=1`. Playable GunMount locates its **own** XZ and **missed** that ABI. A fatter chord (Rank 2) **inverted**. Do not call `geVrRecenter()` at mount / every frame. |
| SOFTXZ? | **No.** Would hide leftover by eating `recenter_pos.xz` every frame. Rejected as first APPLY in DIG #32 (eats room-scale the moment HT returns). Does not name the missing GunMount parent. |
| Rank 1 idea dead? | **No.** Head-relative fist, then current camera, **one term above draw and ray**, is still `196` written for HT=0. Rank 1 **HandWorld site** is falsified. Next sit: same getenv on **GunMount / shared parent**. |
| Playspace size / stand outside | **Orphans hands today.** STAGE origin is the guardian centre. HT=0 parks the eye on the Bond capsule. Raw GunMount XZ keeps the stage leftover → fists appear far from the body until chord. Wanted PASS includes large playspace and standing **outside** the reset spot. |
| APPLY tonight? | **No from this repo.** REAPPLY belongs on the workshop next to GunMount (and the ray if split), sharing one parent with the cube. Do **not** subtract again on HandWorld. |

```
stage / playspace     xrLocateSpace(grip, stage)           (raw XZ — live GunMount)
recenter snapshot     yaw (+ XZ only if not YAWONLY)       (chord; Rank 2 inverted)
HEADYAW / PLAYSPACE   camera yaw + look-around comfort     (KEEP)
HEAD_TRANSLATE        camera XZ lean                       (vr442 = 0 — eye locked)
geStereoXrGunMount    FP gun world pose → gunmtx_camspace  (LIVE DRAW XZ)
geStereoXrHandWorld   left cube translation                (workshop APPLY only)
host ABI GUNREBASE    geVrGetWeaponModelMatrixF / AimRay   (PR #33; playable missed)

HandWorld-only subtract
  → cube helper changes (or inverts)
  → GunMount XZ unchanged
  → guns (+ leftover cube parent) still walk / overshoot / opposite
  → #74 still wears; larger playspace makes gain > 1 obvious
```

---

## 1. What chair already proved

| Sit | Result | Keep |
|-----|--------|------|
| DIG #32 B0 (knobs unset) | Sidestep skews arms until chord. Stick-strafe does not. Recenter “fixes.” | **#74 wears.** |
| Host APPLY PR #33 `GUNREBASE=1` | Public ABI + `ctest`: grip / cube / aim **lock** after 0.8 m sidestep. Banner once. | **Host-only.** Not the headset bake. |
| Workshop HandWorld APPLY + `GUNREBASE=1` | Guns + cube still slide **opposite** the feet after recenter + physical strafe. | **FAIL.** Site wrong. |
| Rank 2 `RECENTER_YAWONLY=0` | Same **sign-flip** FAIL (opposite the feet). Full-origin chord does **not** lock fists. | **Not a ship fix.** Origin snapshot is not the parent. |
| New playspace, same HandWorld APPLY | Forward = hands further front; back = toward body; LR = overshoot / higher accel. **Gain > 1**, eye locked. | **Double room term** on the gun path. Still FAIL. |

Owner next line on #74: *REDIG/REAPPLY on GunMount / parent (not HandWorld-only).* This page is that REDIG.

C-unset in this repo still parks `GUNREBASE` **OFF**. PR #33 must stay default OFF until a **GunMount** sit PASSes. Do not KEEP-ON from the host ctest.

---

## 2. Why HandWorld-only failed

### 2.1 Two helpers, two XZ writers

Already mapped (GUNEYE / two-hand / body DIGS; DIG #32 §1.5):

| Workshop helper | Consumer | Job |
|-----------------|----------|-----|
| `geStereoXrGunMount` | `GUNMOUNT=1` FP gun (`gunfire` mount) | **Live gun draw XZ** — world/controller pose into `gunmtx_camspace` |
| `geStereoXrHandWorld` + `HandBasis` | `geVrHandCubesRender` | Left cube translation / basis |
| `geTouchBoxDist(HandWorld(LEFT), GunMount(RIGHT))` | two-hand latch (parked) | Proves they are **independent poses** |

APPLY [PR #33](https://github.com/no6969el/goldeneye-native/pull/33) already warned: playable GUNMOUNT / left cube may bake through those symbols and **miss** the host ABI. First SimRig grep was `geStereoXrGunMount`, `geStereoXrHandWorld`, `getenv("GETV_VR_GUNREBASE")`, **same TU as GUNMOUNT**.

Workshop APPLY put the head-subtract **on HandWorld** in `stereo.c` only. That is the cube sibling. It is **not** the matrix `G_MTX_LOAD` uses for the PP7 / KF7.

So:

1. Gun mesh XZ still **stage + `HANDYAW=2`**, Bond-centered view (`HT=0`).
2. Cube either (a) got a lone subtract and inverted / locked while the gun walked, or (b) still shares a **parent leftover** above HandWorld (yaw remap / playspace comfort) so both still fail the same way. Owner saw **guns and cube** still wrong together — the draw parent was never rebased.
3. `docs/196` §4: *one term, one place, above every consumer.* A correction on the cube helper only is the picture/physics split that DIG #32 already named for draw vs ray. Same split, wrong sibling.

### 2.2 Host APPLY did not reach the playable bake

`src/ge_vr_bridge.cpp` `gunRebasedPose()` (PR #33):

```93:109:src/ge_vr_bridge.cpp
// Head-relative XZ (keep grip/aim Y), then the camera parent. On this ABI the
// camera is the Bond capsule when HT=0 — GUNMOUNT already adds that origin.
Pose gunRebasedPose(const BridgeState& s, Pose raw) {
    Pose hand = applyRecenterYawXz(s, raw);
    if (!s.frame.head_valid) return hand;
    const Pose head = applyRecenterYawXz(s, s.frame.head);
    hand.position.x -= head.position.x;
    hand.position.z -= head.position.z;
    return hand;
}
```

Callers: `geVrGetWeaponModelMatrixF` (grip/draw), `geVrGetAimRay`, `geVrGetWeaponDisplacement`. `getv/port/src/port_render.c` has a matching `ge_vr_gunrebase()` getenv stub that is **never called** — documentation only.

Playable GETV writes the FP gun through workshop GunMount, **not** this ABI. Headset `GUNREBASE=1` therefore does **not** run `maybeGunRebase` on the mesh. Host ctest locking XZ is a different binary.

### 2.3 Subtracting on the wrong sibling can look like a sign flip

`docs/200` §2: add the room term twice and the gun runs at **double** walking speed; a double-counted term also *“reads as a working fix with an inverted sign.”*

`docs/196` §2 (HT **on**, pre-native GUNMOUNT): eye at `gameCamera + roomOffset`, drawn gun at `gameCamera + (hand − head)` → relative `-roomOffset` → walk forward, gun comes **at** you.

vr442 **inverts the premise** (`HT=0`): eye has **no** room term. Raw GunMount still has stage XZ → gun walks **with** the feet (DIG #32). HandWorld head-subtract adds a room term on the **cube** only. GunMount still has its stage term. Depending on playspace size that leftover reads as:

- **Opposite** (first chair / Rank 2) — view or yaw-remap gained an XZ snapshot the gun did not, or a subtract landed on a pose that was already camera-parented (`196`).
- **Same-direction gain > 1** (new playspace) — eye locked, gun has **stage XZ plus** another leftover (`HANDYAW` pivot around the old origin, un-eaten recenter XZ, or a second add). That is `200` double-count with the sign of HT=0.

Both are **term-count** errors at the GunMount parent. Flipping a minus on HandWorld will not weld the mesh to the controller.

---

## 3. Where live GUNMOUNT draw gets its XZ

### 3.1 Public locate (this repo)

Hands and eyes share one `predictedDisplayTime` (`src/xr_input.cpp`). Base space is **stage** (LOCAL fallback if no bounds — `src/xr_session.cpp`). Grip is `/input/grip/pose`. `modelMatrixFromPose` writes translation as `xrToGame(pose.position, 100)` — no parent, no Bond, no eye.

```
xrLocateSpace(grip_space, stage_space, t)     raw stage metres
geVrGetWeaponModelMatrixF                     ×100 u/m world mtx
  GUNREBASE=0                                 raw (tonight on host)
  GUNREBASE=1                                 recenter yaw+XZ, then −head.xz
```

Layer submit still uses **raw** `views[eye].pose` in `stage_space`. Recenter is a **bridge offset**, not a new OpenXR reference space. Public `geVrRecenter()` stores head yaw + XZ (`y` forced 0). Workshop may narrow that (`RECENTER_YAWONLY=1`).

### 3.2 Playable draw (names only — do not push bodies)

Stock decomp (`n64decomp/007`):

```
gunfire.c  gunmtx ← rot + gunofs  →  hand->gunmtx_camspace
model.c    gSPMatrix(..., G_MTX_LOAD | G_MTX_MODELVIEW)
```

`GUNMOUNT=1` **replaces** that camspace bake with a controller pose from `geStereoXrGunMount`. GUNEYE DIG: if that pose is baked on **one** view (cyclopean / first eye / sim tick) and `G_MTX_LOAD`’d for both eyes, the gun can sit on the hand and still be stereo-blind. #74 is the **parent**, not that stereo hole — but the same bake is where XZ is frozen for the frame.

So live draw XZ is:

```
stage grip  →  geStereoXrGunMount  →  gunmtx (camspace or world-as-LOAD)
            →  G_MTX_LOAD both eyes
            +  HANDYAW=2 / LEVELYAW=1   (yaw remap; KEEP)
            +  Bond capsule parent      (HT=0; GunMount already adds this)
```

**Not in that chain:** `geStereoXrHandWorld`. **Not in that chain unless workshop calls it:** `geVrGetWeaponModelMatrixF`.

Left cube XZ is HandWorld. Aim / hitscan is `geVrGetAimRay` or a workshop aim sibling of GunMount (`GUNAIM=1`). DIG #32: if fists lock and **aim** walks, the ray missed the same term — still Rank 1, second site, **after** GunMount PASSes.

### 3.3 Playspace size / standing outside

STAGE origin is the guardian centre (or LOCAL origin when bounds are missing). `HT=0` keeps the rendered eye on the Bond capsule. GunMount XZ stays stage-absolute:

| Standing | Eye (HT=0) | GunMount XZ | Wear |
|----------|------------|-------------|------|
| On reset spot | Bond | ~hand-relative leftover after last yaw snap | Slight skew (`HANDYAW` around old origin) — filed #74 |
| 0.5–1 m sidestep, small room | Bond | stage leftover tens of cm | Drift / opposite / “have to recenter” |
| Large playspace / **outside** guardian | Bond | stage leftover **metres** | Hands **orphan** — guns appear far from the body; walk **overshoots** (gain ≥ 1) |

Chord re-snaps yaw (and XZ only if not `YAWONLY`). That is why the owner recenters **often**, and why a bigger playspace made the same APPLY read as double roomscale. Wanted PASS is **controller lock at any standing XZ**, not “stay near the guardian.”

Comfort clamp on the public lean helper is **~0.45 m** and never writes the capsule. Owner stare is 0.5–1 m and “outside playspace.” Even if HT returns later, a stage-absolute gun **keeps going** after the eye stops. Rank 1 on GunMount is the parent that survives that.

PD `docs/86`: `gCtrlPos` is **HEAD-RELATIVE** — locate grip against **`viewSpace`, not playSpace**; placement is a plain sum. That is still the structural fix. Do it on the helper that **writes `gunmtx`**, not only the cube.

---

## 4. Rank 1 falsifier

Rank 1 = `GETV_VR_GUNREBASE=1`: each frame, express grip as head-relative XZ (keep Y), then add the **current** camera. One yaw number as `HEADYAW`. Default OFF.

Chair asked: was that idea wrong (sign flip / host recenter on mount / SOFTXZ)?

| Hypothesis | Chair | Verdict |
|------------|-------|---------|
| **Sign flip** (classic `196` opposite) | First FAIL + Rank 2 `YAWONLY=0` | **Incomplete.** Opposite happened. New playspace then showed **same-direction gain > 1**. Both are wrong term count. **Do not flip a sign** on HandWorld or host `gunRebasedPose`. Keep host sign: `hand.xz − head.xz`, then camera. |
| **Host recenter on mount** | Not the headset path | **No.** PR #33 already applies the chord’s yaw+XZ on the ABI, then head-relative. Playable GunMount **missed** it. Rank 2 (write XZ on the chord only) **inverted**. Do **not** `geVrRecenter()` at mount time or every frame. Do **not** treat `recenter_pos` as the per-frame parent. |
| **SOFTXZ** (`GETV_XR_SOFTXZ=1`) | Not run | **Reject as first REAPPLY.** Auto-rebase `recenter_pos.xz` to the HMD each frame hides leftover without naming GunMount. On HT=0 it fakes a lock; when HT returns it eats “walk the room = Bond walks.” DIG #32 already parked this behind Rank 1. |
| **HandWorld is the shared parent** | Workshop APPLY | **Falsified.** Guns still wrong. Cube helper is not the draw XZ. |
| **Extra +XZ on GunMount + eye locked** | New playspace comment | **Current best.** Double roomscale. REAPPLY the **same** getenv on GunMount (and ray if split). Cube must **consume** that parent — no second subtract. |

**Rank 1 idea stands. Rank 1 HandWorld site is dead.**

A/B still allowed (not a fix): scratch `HEAD_TRANSLATE=1`. If #74 dies and #70/#55 return, copy the lean term onto GunMount; do **not** ship HT=1. Restore 0.

---

## 5. Next APPLY — **NOT THIS PR**

Same knob. Same default OFF. Same banner. **Different site.**

```
GETV_VR_GUNREBASE        unset / empty / 0 = OFF
                         1 = head-relative grip + cube + aim, then camera
GETV_VR_GUNREBASE_TRACE  0   DIG_OFF
```

Workshop only (SimRig). Do not push `geStereoXr*` bodies.

```
/* NOT APPLY READY — workshop sketch, names only.
 * Same TU as GUNMOUNT (stereo.c or live name).
 * Tick per sim / first eye. Same pose both eyes.
 *
 * 1. First grep: geStereoXrGunMount, geStereoXrHandWorld,
 *    getenv("GETV_VR_GUNREBASE").
 * 2. Put the rebase on GunMount (the gunmtx writer) — or one shared
 *    HandLocal / viewSpace locate that GunMount AND HandWorld call.
 * 3. HandWorld must use the SAME parent. Do not subtract head again.
 * 4. Aim / GUNAIM sibling gets the same term if it does not already
 *    read GunMount (docs/196: above the split).
 * 5. Reuse GETV_VR_GUNREBASE. Do not invent GUNHEADREL as a second
 *    ship name. Do not KEEP-ON until sit PASS.
 *
 * if (ge_vr_gunrebase()) {
 *     /* PD docs/86: locate grip in viewSpace, or
 *        grip' = recenterYaw * (rawGrip - rawHead.xz)   -- keep Y
 *        gunWorld = bondCameraOrigin + grip'
 *        HandWorld = same parent
 *        AimRay    = same parent
 *     */
 * }
 * do NOT write vv_theta
 * do NOT call geVrRecenter() every frame / at mount
 * do NOT enable GUNZ / HANDSOLID
 * do NOT flip HEADYAW / AUTORECENTER / PLAYSPACE / HT
 */
```

Public-tree note: host `gunRebasedPose` already matches that arithmetic. If workshop GunMount is later wired to `geVrGetWeaponModelMatrixF`, do **not** also subtract in `stereo.c`. One term.

### Must not (unchanged)

- Flip `HEADYAW` / `AUTORECENTER` / `GUNARM` / `PLAYSPACE`
- Restore `HEAD_TRANSLATE=1` as the #74 fix
- Enable `GUNZ` / `HANDSOLID`
- Call `geVrRecenter()` every frame
- Add `GUNREBASE=1` to ship boot / `$requiredBootKnobs` until **GunMount** sit PASS
- Sign-flip the host helper because the chair said “opposite”
- SOFTXZ / Rank 2 as the APPLY
- Push workshop bodies

---

## 6. Chair stare (plain tester sentences)

**Setup:** vr442 Latest + `Start-GEVR.bat`. Boot keeps `GUNARM=1`, `GUNMOUNT=1`, `BODY=0`, `BODY_NOARMS=1`, `HANDCUBES=1`, `MASK=1`, `HEADYAW=1`, `AUTORECENTER=1`, `RECENTER_CHORD=1`, `RECENTER_YAWONLY=1`, `HEAD_TRANSLATE=0`, `PLAYSPACE=1`. Recenter both sticks. Dam or Facility. **Right hand holds a gun. Left empty (cube).** No stick on the sidestep. **Do not** set `GUNEYE`, `GUNZ`, `HANDSOLID`, `BODY=1`, `HEADYAW=0`, `HEAD_TRANSLATE=1`.

After PLAY0 / KEEP have exported (scratch, **not** the ship allowlist):

```bat
set GETV_VR_GUNREBASE=1
```

Console once: `[getv][gunrebase] GETV_VR_GUNREBASE=1 arms`. This REDIG expects **today’s zip (HandWorld-only) to FAIL** the rows below until GunMount REAPPLY.

**Run B0 — knobs unset (must still wear #74):**

| | Tester sentence |
|--|-----------------|
| **B0-PASS 1** | “I recenter, guns out, sidestep 0.5–1 m, no stick. Gun / cube drift or skew. I chord to put them back.” |
| **B0-PASS 2** | “Stick-strafe only (feet planted) does **not** do this.” |
| **B0-FAIL** | “Sidestep already welds the gun to the controller.” **Stop. Wrong zip, or #74 is gone.** |

**Run HW — HandWorld-only (already chaired FAIL; do not re-ship):**

| | Tester sentence |
|--|-----------------|
| **HW-FAIL 1** | “`GUNREBASE=1` after HandWorld subtract: guns + cube still slide **opposite** the feet.” |
| **HW-FAIL 2** | “New / large playspace: I walk forward, hands go **further** in front; back, toward my body; sidestep **overshoots**. World stays (HT=0).” |
| **HW-FAIL 3** | “I stand **outside** the guardian / far from the reset spot. Hands orphan — guns are not on my fists until I chord.” |

**Run GM — after GunMount / shared-parent REAPPLY only:**

| | Tester sentence |
|--|-----------------|
| **GM-PASS 1** | “Recenter, guns out, physical sidestep 0.5–1 m, no stick. **Guns stay locked to the controllers.** I do not want a recenter.” |
| **GM-PASS 2** | “I walk **forward and back**. Fists stay on the controllers. They do **not** come at me (`196`) and do **not** run ahead of my feet (`200` double).” |
| **GM-PASS 3** | “Large playspace. I stand **outside** the guardian / metres from the reset spot. Fists still on the controllers. Hands are **not** orphaned.” |
| **GM-PASS 4** | “Glance + stick-turn. Aim still on the **gun ray**. `HEADYAW` still turns the world. Cinema still autorecenters. Chord still works.” |
| **GM-PASS 5** | “Empty left cube stayed on my left hand. Dual-wield both guns stayed on both hands.” |
| **GM-PASS 6** | “Cover one eye: offset-fixed, not a new cross-eye. (#39 at rest unchanged.)” |
| **GM-FAIL** | “Still opposite.” / “Still overshoots / gain > 1.” / “Outside playspace orphans the hands.” / “Gun welded to my face.” / “Aim left the barrel.” / “`HEADYAW` died.” / “Gun vanished below my chest.” / “I have to recenter to enter play.” |

**Run Y — do not ship; already FAIL on file:**

`YAWONLY=0` produced the **same opposite-feet** wear. Restore `1`. Do not use a fatter chord as the fix.

**Run S3 — SOFTXZ (only if GM is refused):**

`GETV_XR_SOFTXZ=1` is still **not** the first APPLY. If Director insists: fists may lock on HT=0 while the world does not walk. Restore OFF. Prefer GM.

**Falsifier (optional):** `GETV_VR_PLAYSPACE=0` on B0. If that alone kills skew **and** brings back look-around shear, leftover is still in the comfort pass. Put `PLAYSPACE=1` back. APPLY is still GunMount Rank 1, not `PLAYSPACE=0`.

**Regression (every run):** trigger, squeeze ADS on the **gun ray**, B reload, both-stick recenter, cinema autorecenter, left cube when empty, no Bond sleeve, no GUNZ vanish. #39 size / sting unchanged. #45 floor height unchanged (`FLOOR_M`).

---

## 7. Director decision

| If you say… | Then… |
|-------------|--------|
| **Green GunMount REAPPLY** | Workshop sit §6 B0 (confirm wear) + GM + large-playspace / outside-guardian. Same `GETV_VR_GUNREBASE`, default OFF. Cube consumes the same parent. Public zip stays vr442 until PASS. |
| **Green host ABI only** | Rejected as the playable fix. PR #33 already landed; headset missed it. |
| **Re-sit HandWorld-only** | Rejected. Chair FAIL is on file (opposite + gain > 1). |
| **Flip the sign** | Rejected. Term count, not minus. |
| **Host recenter on mount / per-frame `geVrRecenter`** | Rejected. Rank 2 inverted. |
| **Ship SOFTXZ** | Rejected as first APPLY. |
| **Flip `HEADYAW` / `AUTORECENTER` / `PLAYSPACE` off** | Rejected. |
| **Restore `HEAD_TRANSLATE=1` as the #74 fix** | Rejected. Reopens #70 / #55. A/B only. |
| **Revive GUNZ / HANDSOLID** | Park. |
| **Merge #39 / #45 / #41 / two-hand** | Rejected. |
| **Park / reject** | Stop. Testers keep recentering; large rooms orphan hands. |

---

## 8. Canonical tracker

**[GEVR #74](https://github.com/no6969el/GEVR/issues/74)** is the tracker for DIG #32, host APPLY #33, this REDIG, and any later GunMount REAPPLY. Do not open a second issue.

Related, stay **open and split**:

| Issue | Why it is not #74 |
|-------|-------------------|
| [#45](https://github.com/no6969el/GEVR/issues/45) | Floor / reset-spot (`FLOOR_M`, `FLOOR_INJECT`). Comfort-pass **look-around** shear. |
| [#39](https://github.com/no6969el/GEVR/issues/39) | Huge + cross-eye **at rest** (GUNEYE). |
| [#41](https://github.com/no6969el/GEVR/issues/41) | Colocated Bond body. Wrong parent first = fake attach (`196` §5). |
| [#70](https://github.com/no6969el/GEVR/issues/70) / [#55](https://github.com/no6969el/GEVR/issues/55) | Why `HEAD_TRANSLATE=0` is PARK. Do not restore as the #74 fix. |

Suggested #74 comment (human / Director — this agent cannot write GitHub issues):

> REDIG (no APPLY): `no6969el/goldeneye-native` `getv/patches/room-strafe-arms-redig/RESULT.md` / PR on that repo.
> HandWorld-only FAIL on file (opposite + gain > 1). Live draw XZ is GunMount, not HandWorld.
> Rank 1 idea stands; site moves to GunMount / shared parent. Do not sign-flip. Do not SOFTXZ. Do not recenter-on-mount.
> Wanted: fists track controllers after recenter; standing outside playspace must not orphan hands.

---

## 9. Attribution / legal

- No personal credit paths edited.
- No GoldenEye ROM, assets, or dumps.
- PD viewSpace / one-yaw-recenter notes are **map-only** (public GEVR `docs/86`, `docs/102`). Roomscale-origin history is `docs/194` / `196` / `200`. Do not copy PD sources.
- Workshop C (`geStereoXr*`, live `HANDYAW` / playspace bodies, SimRig HandWorld APPLY) stays **private** until release policy flips. This page uses brief names already on the public DIG/APPLY.
- Decomp citations are `n64decomp/007` (all rights reserved); this REDIG describes call sites, it does not vendor the game.
