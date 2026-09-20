# RESULT — HandPoseRel/HeadPosRel REAPPLY chair FAIL (REDIG2 ONLY)

**Tracker:** [GEVR #74](https://github.com/no6969el/GEVR/issues/74) — **canonical.** Do not open a second issue. Do not retarget this REDIG.
**Status:** REDIG2. **Not APPLY.** No C landed. **No ship.** Workshop symbols stay **private** (names only).
**Chair FAIL ([#74](https://github.com/no6969el/GEVR/issues/74), 2026-09-20):** `GETV_VR_GUNREBASE=1` after workshop REAPPLY (`geVrXrPlayHandPoseRel` + `geVrXrPlayHeadPosRel` in **GunMount and HandWorld**). Physical walk still **amplified same-direction** (forward / back / left / right overshoot). **New:** bend / lean down → guns **and** cube extend **far toward the FLOOR**. Owner: attached to the **wrong center / inverse / accelerated — not their head**. Cube + dual guns **co-fail** (shared parent).
**Wanted:** fists always track controllers after recenter, including lean. Playspace size / standing outside the playspace must **not** orphan hands.
**Prior:** DIG [PR #32](https://github.com/no6969el/goldeneye-native/pull/32). Host APPLY [PR #33](https://github.com/no6969el/goldeneye-native/pull/33). REDIG [PR #35](https://github.com/no6969el/goldeneye-native/pull/35) (HandWorld-only FAIL → site to GunMount / shared parent). Workshop Rel-pair REAPPLY on **SimRig only** — not in this remote.
**Wear:** vr442 / Latest. `GUNARM=1`, `GUNMOUNT=1`, `BODY=0`, `BODY_NOARMS=1`, left cube `MASK=1`, `HEAD_TRANSLATE=0`, `RECENTER_YAWONLY=1`, `PLAYSPACE=1`, `HANDYAW=2`.
**Constraints:** Do not flip `HEADYAW` / `AUTORECENTER` / `PLAYSPACE` / `GUNARM` / `HANDYAW` (the KEEP pin). Do not restore `HEAD_TRANSLATE=1` as this fix. Parked `GUNZ` / `HANDSOLID` stay off. Do not push workshop `geStereoXr*` / `geVrXrPlay*` / `ge_st_qrotv` bodies.
**Date:** 2026-09-20.
**Evidence:** [#74](https://github.com/no6969el/GEVR/issues/74) chair comments (HandWorld FAIL → Rel-pair REAPPLY → lean-to-floor), public `goldeneye-native` HEAD (`src/ge_vr_bridge.cpp` `applyRecenterYawXz` / `gunRebasedPose`, `recenter_pos.y = 0`), public GEVR `docs/86` / `102` trap 6 / `194` / `196` / `200`, public `n64decomp/007` `gunmtx_camspace` + `G_MTX_LOAD`. Workshop C is **not on any public remote** (`GEVR` `docs/RELEASE-POLICY.md`). Brief names are the workshop symbols.

Director can green-light the Rank 1 composition sit, park, or reject from this page alone.

---

## Verdict (one screen)

| Question | Answer |
|----------|--------|
| Hypothesis falsified? | **Not falsified. Strengthened.** `PlayHandPoseRel` is already **recentre-relative** (play origin, Y from the floor / `recenter_pos.y = 0`). Subtracting `PlayHeadPosRel` then `HANDYAW` + `ge_st_qrotv` is a **second transform of a floor-origin vector**. HT=0 lean Y is the new discriminator: the eye stays, the fists swing about that origin toward the floor. |
| Live parent under HT=0 lean? | **Recentre / floor origin — not the HMD, not the Bond camera.** Camera XZ (+ public lean helper Y) stays on the capsule. `HeadPosRel.y` is live. `HandPoseRel.y` is standing-height from y=0. `qrotv` of that lever (pitch in the quat, or Head Y used as a world parent) sends guns **and** cube toward the floor. |
| HandPoseRel alone (no HeadPosRel subtract)? | **Diagnostic split, not the ship.** On HT=0, Rel-alone + Bond is docs/`200` “raw play-space hand” — original [#74](https://github.com/no6969el/GEVR/issues/74) 1× walk with the feet **unless** Sit R proves Rel is already head-relative. Use it to split Y-parent vs `qrotv`, not as APPLY. |
| Skip `HANDYAW` / `qrotv` when `GUNREBASE`? | **Rank 1 — yes, on the translation only.** Host `gunRebasedPose` never rotates the position. `docs/102` trap 6: a play-space pose + HMD / hand quat folds yaw (and pitch) in twice. Skip `HANDYAW` + `ge_st_qrotv` on the **GUNREBASE translation**. Keep `HANDYAW=2` as the look-around **orientation** pin. |
| Different site? | **No.** Cube + dual guns + GunMount + HandWorld **co-fail**. REDIG PR35 site is confirmed. The leftover is **composition**, not sibling. |
| Sign flip / SOFTXZ / fatter chord? | **No.** Gain > 1 is term count / wrong pivot (`200`), not a minus. Rank 2 `YAWONLY=0` already inverted. SOFTXZ still hides leftover and eats room-scale when HT returns. |
| Rank 1 idea dead? | **No.** Head-relative **XZ**, **keep Y**, current Bond camera, **one term above draw and ray**, is still `196` written for HT=0. Rank 1 **Rel-pair + `qrotv` composition** is falsified. |
| APPLY tonight? | **No from this repo.** Next sit is the same `GETV_VR_GUNREBASE` (default **OFF**) with the host-shaped composition on the **already-shared** parent. Do not invent a second getenv. |

```
stage / playspace     xrLocateSpace(grip, stage)              (raw)
PlayHandPoseRel       already applyRecenterYawXz(hand)        (Y = height from floor)
PlayHeadPosRel        already applyRecenterYawXz(head)        (Y LIVE — lean drops it)
HEAD_TRANSLATE        camera XZ (+ public lean Y)             (vr442 = 0 — eye locked)
recenter_pos.y        forced 0                                (floor / #45 cal)
HANDYAW + ge_st_qrotv rotates the Rel translation             (about Rel origin = floor)
geStereoXrGunMount    FP gun world pose → gunmtx_camspace     (shared parent)
geStereoXrHandWorld   left cube                               (same parent — co-fail)

Rel − HeadPosRel + qrotv(Rel origin)
  → XZ leftover does not cancel (frames mismatch) = gain > 1 walk
  → tall Y lever pitches toward the floor on lean
  → cube + both guns go together
```

---

## 1. What chair already proved

| Sit | Result | Keep |
|-----|--------|------|
| DIG #32 B0 (knobs unset) | Sidestep skews arms until chord. Stick-strafe does not. Recenter “fixes.” | **#74 wears.** |
| Host APPLY PR #33 `GUNREBASE=1` | Public ABI + `ctest`: grip / cube / aim **lock** after 0.8 m sidestep. Banner once. **Keeps Y.** No `qrotv`. | **Host-only.** Not the headset bake. |
| Workshop HandWorld-only | Opposite the feet, then same-direction gain > 1 on a larger playspace. | **FAIL.** Wrong sibling. |
| Rank 2 `RECENTER_YAWONLY=0` | Same opposite-feet invert. | **Not a ship fix.** |
| REDIG PR35 | Live draw XZ is GunMount → `gunmtx_camspace`, not HandWorld. Host ABI unused by playable bake. | **Site → shared parent.** |
| Rel-pair REAPPLY on GunMount **and** HandWorld | Cube + dual guns **both** amplified with walk. | **Site confirmed** (one parent). |
| Same Rel-pair, lean / bend down | Guns **and** cube shoot **far toward the floor**. Owner: wrong center / inverse / accelerated. | **Composition FAIL.** Floor/recentre pivot. |

C-unset in this repo still parks `GUNREBASE` **OFF**. PR #33 must stay default OFF until a **composition** sit PASSes walk **and** lean. Do not KEEP-ON from the host ctest.

---

## 2. Hypothesis — evaluated, not guessed

**Stated:** `HandPoseRel` is already recentre-relative; subtracting `HeadPosRel` then `HANDYAW` + `ge_st_qrotv` double-transforms; HT=0 lean Y exposes the floor / recentre pivot.

### 2.1 What the public names already say

`geVrXrPlayHandPoseRel` / `geVrXrPlayHeadPosRel` — **Play** + **Rel**. Public vocabulary for that pair is `applyRecenterYawXz`: subtract the chord snapshot, yaw-rotate the leftover. It is **not** PD `viewSpace` / `gCtrlPos` (head-relative, `docs/86`). Head-relative on this tree is a **second** step (`gunRebasedPose` XZ minus).

```65:104:src/ge_vr_bridge.cpp
Pose applyRecenterYawXz(const BridgeState& s, Pose p) {
    const Quat yaw = quatFromAxisAngle(Vec3{0, 1, 0}, -s.recenter_yaw);
    p.orientation = quatMul(yaw, p.orientation);
    Vec3 d{p.position.x - s.recenter_pos.x,
           p.position.y - s.recenter_pos.y,
           p.position.z - s.recenter_pos.z};
    p.position = quatRotate(yaw, d);
    return p;
}
// ...
Pose gunRebasedPose(const BridgeState& s, Pose raw) {
    Pose hand = applyRecenterYawXz(s, raw);
    if (!s.frame.head_valid) return hand;
    const Pose head = applyRecenterYawXz(s, s.frame.head);
    hand.position.x -= head.position.x;
    hand.position.z -= head.position.z;
    return hand;
}
```

Host facts that the Rel-pair REAPPLY left behind:

| Public fact | Why it matters tonight |
|-------------|------------------------|
| Recentre quat is **yaw only** | Pitch must not touch the translation. |
| `recenter_pos.y` forced **0** (`geVrRecenter`) | Rel origin is the **floor / cal plane**, not the eyes. Height is `#45`. |
| `gunRebasedPose` subtracts **X and Z only** | Lean Y on the head must **not** move the fist. Host ctest asserts “grip Y kept.” |
| `geVrGetEyeViewOffsetF` adds lean **X/Z only** | Even the public lean helper never writes Y into the eye. |
| `geVrPhysicalCrouch` reads **raw head Y** | Head Y is live. HT=0 does not park it. |
| No `qrotv` / `HANDYAW` on the host position | A second rotate of a Rel vector is workshop-only. |

### 2.2 Clause by clause

| Clause | Verdict | Why |
|--------|---------|-----|
| `HandPoseRel` is already recentre-relative | **Survives.** | Name is Play+Rel. After a HeadPosRel subtract the walk **still** has room (frames do not cancel). Lean-to-floor needs a **tall Y lever** (~1.2 m from y=0). A true head-relative sample is ~arm length; pitching that does not shoot **far** to the floor. |
| Subtracting `HeadPosRel` is *the* double | **Partially falsified as the sole cause.** | Rel-hand minus Rel-head **XZ** is the host Rank 1 term (`196` for HT=0), not a double. The double / wrong pivot is **`HANDYAW` + `qrotv` on a vector that still has floor Y**. Subtracting `HeadPosRel.y` is a **separate** bad extra (host keeps Y). |
| `camera + (hand − head)` including Y, HT=0 camera Y fixed | **Falsified as the floor mechanism.** | Lean **drops** `HeadPosRel.y`. `hand.y − head.y` **increases**. Guns would go **up** toward the capsule, not toward the floor. Chair is the other way. |
| HT=0 lean Y exposes floor / recentre pivot | **Survives. New discriminator.** | Eye locked. Fists inherit a transform about y=0. Owner sentence matches: wrong center / inverse / accelerated — **not their head**. |

### 2.3 Why walk is still 2× *and* lean hits the floor

`docs/200` §2: add the room term twice and the gun runs at **double** walking speed. `G-200`: walk — it must come with you, **not at double speed**. Tonight’s walk is that wear (forward = further front; back = toward body; LR overshoot).

`docs/102` trap 6: anything **already in play space** must use a **separate recenter quat**, not the HMD quat, **or head yaw folds in twice.** `HANDYAW=2` + a Rel translation is that trap with **pitch** on the table.

Two compositions both fit the chair. Rank 1 sit splits them.

**A — `qrotv` of the Rel position about the floor origin (leading).**

`ge_st_qrotv` rotates a vector by a quaternion. If the operand is `PlayHandPoseRel.pos` (from y=0, Y ≈ grip height) and the quat has **pitch** (full head / a non-yaw `HANDYAW`):

- Lean / look down swings the ~1.2 m **up** component into the look-down → **far toward the floor**.
- Walk lengthens the XZ lever from the same origin → overshoot / gain > 1, same direction.
- Cube and both guns share the rotate → **co-fail**.

If `qrotv` runs **before** the HeadPosRel subtract, the hand is rotated and the head is not. XZ no longer cancels. That is “subtract then still 2×.”

**B — `HeadPosRel` used as a world Y parent (secondary).**

Adding live `HeadPosRel.y` (or `Head − Hand` inverse, owner’s word) couples fist Y to the HMD while HT=0 leaves the picture on the capsule. Lean drops guns. Standing would also sit wrong (too high if added, through the floor if inverted). Chair named lean as **new**, so A is the better first sit: standing walk was already FAIL; lean revealed the **pivot**, not a new site.

`196` opposite-feet is **not** this sit. Rel-pair REAPPLY is same-direction gain + floor. Do not flip a minus.

---

## 3. Live parent under HT=0 lean

```
HT=0                 eye locked on Bond capsule          (picture stays)
recenter_pos.y = 0   floor / calibration plane           (#45; not a gun parent)
PlayHandPoseRel      recentre-relative fist              (Y = height above that plane)
PlayHeadPosRel.y     LIVE                                (bend drops it)
HANDYAW + qrotv      rotate translation about Rel origin
GunMount + HandWorld one parent                          (co-fail)

bend / lean down, feet planted, no stick
  → camera does not follow
  → Rel Y lever (and/or HeadPosRel Y parent) moves
  → guns + cube extend toward the FLOOR
```

| Candidate parent | Live on lean? | Matches chair? |
|------------------|---------------|----------------|
| Bond camera / capsule | **No** (HT=0) | No — picture stays; fists leave. |
| HMD / `viewSpace` (PD `gCtrlPos`) | Would track the head | **No** — owner: *not their head*. |
| Recentre / floor origin (`Play*Rel` + `qrotv`) | **Yes** — Y from y=0, Head Y live, quat may pitch | **Yes** — far toward floor; walk overshoots the same origin. |
| `#45` `FLOOR_M` | Calibration offset | **No** — world floor height, not fists diving. Keep `#45` split. |

Public host `GUNREBASE=1` would **not** wear lean-to-floor: yaw-only recenter, XZ minus, Y kept, no `qrotv`. Headset bake never ran that helper.

---

## 4. Rank the next composition (not a new site)

Same getenv. Same two consumers. Change **only** what the `GUNREBASE=1` branch does to the translation.

| Rank | Composition | Predict | Use |
|------|-------------|---------|-----|
| **1** | Skip `HANDYAW` + `ge_st_qrotv` on the **GUNREBASE translation**. `HeadPosRel` subtract **XZ only**, **keep Y**. Add Bond camera (HT=0). | Lean-to-floor **dies** (no pitch of the floor lever). Walk **locks** if Head XZ is the same Rel frame; walk still fails if Head XZ is dead / plus / mismatched. | **First sit.** Closest to public `gunRebasedPose`. |
| **1b Sit R** | `HandPoseRel` **alone** — no `HeadPosRel` subtract, no `qrotv` on the translation. | Walk returns to 1× with the feet (#74 B0) if Rel is only recentre-relative. Floor dies if floor was Head Y. Floor **remains** if `qrotv` was left on. Walk **and** lean PASS only if Rel was already head-relative (name lied). | **Split only** if Rank 1 still 2× or still floors. **Not the ship** on HT=0 (`200` is the wrong default while the eye has no room term). |
| **2** | Drop `HeadPosRel` Y only; leave `qrotv` | Floor **remains** if A is right. Floor dies and walk stays 2× if B is right. | Only after Rank 1. |
| — | Different site | Cube + guns already share a parent. | **Rejected.** |
| — | Sign flip / SOFTXZ / `YAWONLY=0` / per-frame `geVrRecenter` / HT=1 | Already inverted or parked. | **Rejected.** |

### Must not (unchanged)

- Flip `HEADYAW` / `AUTORECENTER` / `GUNARM` / `PLAYSPACE`
- Flip the **KEEP pin** `HANDYAW=2` (skip it **inside** the GUNREBASE translation only)
- Restore `HEAD_TRANSLATE=1` as the #74 fix
- Enable `GUNZ` / `HANDSOLID`
- Call `geVrRecenter()` every frame
- Add `GUNREBASE=1` to ship boot / `$requiredBootKnobs` until **walk + lean** PASS
- Sign-flip the host helper
- SOFTXZ / Rank 2 as the APPLY
- Push workshop bodies
- Invent `GUNHEADREL` / `GUNREBASE_NOQROTV` as a second ship name

### Rank 1 getenv (still default OFF — not landed)

```
GETV_VR_GUNREBASE        unset / empty / 0 = OFF
                         1 = chair composition in §4 Rank 1
GETV_VR_GUNREBASE_TRACE  0   DIG_OFF
```

```c
/* NOT APPLY READY — workshop sketch, names only.
 * Same TU as GUNMOUNT / HandWorld. Tick per sim / first eye.
 * Same pose both eyes. One parent, both consumers.
 *
 * if (ge_vr_gunrebase()) {
 *     /* PlayHandPoseRel is already recentre-relative — do not qrotv it.
 *        PlayHeadPosRel.xz  — subtract (host). Keep Y.
 *        then gunWorld = bondCamera + that XZ
 *        do NOT ge_st_qrotv / HANDYAW the translation
 *        do NOT subtract HeadPosRel.y
 *        do NOT use HeadPosRel as a world parent
 *     */
 * }
 * do NOT write vv_theta
 * do NOT call geVrRecenter() every frame
 * do NOT enable GUNZ / HANDSOLID
 */
```

If playable GunMount is later wired to `geVrGetWeaponModelMatrixF`, do **not** also subtract in `stereo.c`. One term. Host already matches Rank 1 arithmetic.

---

## 5. Chair stare (plain tester sentences)

**Setup:** vr442 Latest + `Start-GEVR.bat`. Boot keeps `GUNARM=1`, `GUNMOUNT=1`, `BODY=0`, `BODY_NOARMS=1`, `HANDCUBES=1`, `MASK=1`, `HEADYAW=1`, `AUTORECENTER=1`, `RECENTER_CHORD=1`, `RECENTER_YAWONLY=1`, `HEAD_TRANSLATE=0`, `PLAYSPACE=1`, `HANDYAW=2`. Recenter both sticks. Dam or Facility. **Right hand holds a gun. Left empty (cube).** No stick on the walk or the lean. **Do not** set `GUNEYE`, `GUNZ`, `HANDSOLID`, `BODY=1`, `HEADYAW=0`, `HEAD_TRANSLATE=1`.

After PLAY0 / KEEP have exported (scratch, **not** the ship allowlist):

```bat
set GETV_VR_GUNREBASE=1
```

Console once: `[getv][gunrebase] GETV_VR_GUNREBASE=1 arms`. This REDIG2 expects **today’s Rel-pair zip to FAIL** the lean row and the walk-gain row until the Rank 1 composition sit.

**Run B0 — knobs unset (must still wear #74):**

| | Tester sentence |
|--|-----------------|
| **B0-PASS 1** | “I recenter, guns out, sidestep 0.5–1 m, no stick. Gun / cube drift or skew. I chord to put them back.” |
| **B0-PASS 2** | “Stick-strafe only (feet planted) does **not** do this.” |
| **B0-FAIL** | “Sidestep already welds the gun to the controller.” **Stop. Wrong zip, or #74 is gone.** |

**Run RP — Rel-pair REAPPLY (already chaired FAIL; do not re-ship):**

| | Tester sentence |
|--|-----------------|
| **RP-FAIL 1** | “`GUNREBASE=1` after HandPoseRel − HeadPosRel: I walk forward, hands go **further** in front; back, toward my body; sidestep **overshoots**. World stays (HT=0).” |
| **RP-FAIL 2** | “I bend / lean **down** (feet planted, no stick). Guns **and** the left cube shoot **far toward the floor**.” |
| **RP-FAIL 3** | “Dual-wield: both guns do the same thing. Empty cube does the same thing. One parent.” |
| **RP-FAIL 4** | “I stand **outside** the guardian. Hands orphan until I chord.” |

**Run L — lean stare (owner; every later sit):**

| | Tester sentence |
|--|-----------------|
| **L-PASS** | “Recenter, guns out, **bend / lean down** looking at the floor, feet planted, no stick. Guns and cube **stay on the controllers**. They do **not** extend toward the floor.” |
| **L-FAIL** | “Fists dive / stretch toward the floor. World / picture stays (HT=0).” |

**Run P1 — Rank 1 composition (skip `qrotv` / `HANDYAW` on translation; Head XZ minus; keep Y):**

| | Tester sentence |
|--|-----------------|
| **P1-PASS 1** | L-PASS. |
| **P1-PASS 2** | “I walk **forward, back, left, right**. Fists stay on the controllers. They do **not** come at me (`196`) and do **not** run ahead of my feet (`200` double).” |
| **P1-PASS 3** | “Large playspace. I stand **outside** the guardian. Fists still on the controllers.” |
| **P1-PASS 4** | “Glance + stick-turn. Aim still on the **gun ray**. `HEADYAW` still turns the world. Look-around comfort still holds (`HANDYAW` orientation KEEP). Cinema still autorecenters. Chord still works.” |
| **P1-PASS 5** | “Empty left cube stayed on my left hand. Dual-wield both guns stayed on both hands.” |
| **P1-PASS 6** | “Cover one eye: offset-fixed, not a new cross-eye. (#39 at rest unchanged.) `#45` floor height unchanged.” |
| **P1-FAIL** | “Still overshoots / gain > 1.” / “Still dives to the floor.” / “Outside playspace orphans the hands.” / “Gun welded to my face.” / “Aim left the barrel.” / “`HEADYAW` died.” / “Look-around shears the way it did before PLAYSPACE.” / “Gun vanished below my chest.” |

**Run R — Sit R, only if P1 is refused or still 2× / still floors:**

`HandPoseRel` alone. No `HeadPosRel` subtract. No `qrotv` on the translation.

| | Tester sentence |
|--|-----------------|
| **R-B0** | “Walk is the old #74: fists go **with** my feet, about 1×, no floor dive.” → Rel is recentre-relative. APPLY is still Rank 1 XZ minus + keep Y, not Rel-alone. |
| **R-LOCK** | “Walk **and** lean stay glued.” → Rel was already head-relative. Then Rel-alone + Bond is the APPLY. Do not also subtract HeadPosRel. |
| **R-FLOOR** | “Lean still dives.” → `qrotv` was not actually off. Stop. Do not add terms. |

**Run Y / S3 — do not ship:**

`YAWONLY=0` already FAIL. SOFTXZ is still not the first APPLY.

**Regression (every run):** trigger, squeeze ADS on the **gun ray**, B reload, both-stick recenter, cinema autorecenter, left cube when empty, no Bond sleeve, no GUNZ vanish. #39 size / sting unchanged. #45 floor height unchanged (`FLOOR_M`). Lean must not be chaired as `#45`.

---

## 6. Director decision

| If you say… | Then… |
|-------------|--------|
| **Green Rank 1 composition** | Workshop sit §5 B0 (confirm wear) + RP (confirm today’s FAIL) + P1 + **L**. Same `GETV_VR_GUNREBASE`, default OFF. Skip `qrotv` / `HANDYAW` on the translation. Head XZ minus, keep Y. Public zip stays vr442 until walk **and** lean PASS. |
| **Green Sit R first** | Only if you want the Rel-already-head-rel split on file before P1. Restore the Rank 1 composition after. |
| **Green HandPoseRel-alone as APPLY** | Rejected until Sit R-LOCK. On HT=0 that is docs/`200` and original #74. |
| **Move site again** | Rejected. Co-fail already named the shared parent. |
| **Flip the sign** | Rejected. Term count / pivot, not minus. |
| **Host recenter on mount / per-frame `geVrRecenter`** | Rejected. Rank 2 inverted. |
| **Ship SOFTXZ** | Rejected as first APPLY. |
| **Flip `HEADYAW` / `AUTORECENTER` / `PLAYSPACE` / `HANDYAW` KEEP off** | Rejected. |
| **Restore `HEAD_TRANSLATE=1` as the #74 fix** | Rejected. Reopens #70 / #55. A/B only. |
| **Retune `FLOOR_M` / `FLOOR_INJECT`** | Rejected. `#45`. Lean-to-floor is fists, not the world floor. |
| **Revive GUNZ / HANDSOLID** | Park. |
| **Merge #39 / #45 / #41 / two-hand** | Rejected. |
| **Park / reject** | Stop. Testers keep recentering; lean dives the guns at the floor. |

---

## 7. Canonical tracker

**[GEVR #74](https://github.com/no6969el/GEVR/issues/74)** is the tracker for DIG #32, host APPLY #33, REDIG #35, this REDIG2, and any later composition REAPPLY. Do not open a second issue.

Related, stay **open and split**:

| Issue | Why it is not #74 |
|-------|-------------------|
| [#45](https://github.com/no6969el/GEVR/issues/45) | Floor / reset-spot (`FLOOR_M`, `FLOOR_INJECT`). Comfort-pass **look-around** shear. Lean-to-floor tonight is **fists**, not world height. |
| [#39](https://github.com/no6969el/GEVR/issues/39) | Huge + cross-eye **at rest** (GUNEYE). |
| [#41](https://github.com/no6969el/GEVR/issues/41) | Colocated Bond body. Wrong parent first = fake attach (`196` §5). |
| [#70](https://github.com/no6969el/GEVR/issues/70) / [#55](https://github.com/no6969el/GEVR/issues/55) | Why `HEAD_TRANSLATE=0` is PARK. Do not restore as the #74 fix. |

Suggested #74 comment (human / Director — this agent cannot write GitHub issues):

> REDIG2 (no APPLY): `no6969el/goldeneye-native` `getv/patches/room-strafe-arms-redig2/RESULT.md` / PR on that repo.
> Rel-pair REAPPLY FAIL on file (2× walk + lean-to-floor). Live parent under HT=0 lean is the recentre/floor origin, not the head.
> Hypothesis stands: PlayHandPoseRel is already recentre-relative; HANDYAW+qrotv on that translation is the double; HeadPosRel Y subtract is not the floor mechanism.
> Rank 1: same `GETV_VR_GUNREBASE` default OFF — skip qrotv/HANDYAW on the translation; Head XZ minus; keep Y. Not Rel-alone. Not a new site.

---

## 8. Attribution / legal

- No personal credit paths edited.
- No GoldenEye ROM, assets, or dumps.
- PD viewSpace / one-yaw-recenter / separate-recenter-quat notes are **map-only** (public GEVR `docs/86`, `docs/102`). Roomscale-origin history is `docs/194` / `196` / `200`. Do not copy PD sources.
- Workshop C (`geStereoXr*`, `geVrXrPlay*`, live `HANDYAW` / `ge_st_qrotv` / playspace bodies, SimRig Rel-pair APPLY) stays **private** until release policy flips. This page uses brief names already on the public DIG / APPLY / [#74](https://github.com/no6969el/GEVR/issues/74) comments.
- Decomp citations are `n64decomp/007` (all rights reserved); this REDIG describes call sites, it does not vendor the game.
