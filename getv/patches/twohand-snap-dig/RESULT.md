# RESULT — left cube hide-on-near + two-hand support snap (DIG ONLY)

**Status:** DIG. **Not APPLY READY on this public tree.** No C landed.
**Ask (folded, one dig):** when the LEFT HANDCUBE is within grip/near of the RIGHT gun mount, **HIDE** it (skip draw). When farther, show it as now (`MASK=1` left). **SNAP** to a front support-grip is the **next layer on the same proximity** (hysteresis): near → hide cube **and/or** snap left pose so it reads two-handed.
**Constraints:** `GETV_VR_GUNARM=1` stays. Bond cuff / HANDMESH boxes / GHOSTHAND stay rejected or parked. Dual-wield: no snap (hide cube).
**Date:** 2026-09-20 (follow-up: hide-on-near folded in; draw site named `geVrHandCubesRender`).
**Evidence:** public `goldeneye-native` HEAD, public `no6969el/GEVR` textbook/packaging (vr441 boot), public `n64decomp/007` `gun.h`. Workshop C (`geVrHandCubesRender`, `geStereoXr*`, `geTouchBoxDist`) is **not on any public remote** (`GEVR` `docs/RELEASE-POLICY.md`). Brief names are the workshop symbols.

Director can green-light hide, snap, both, or neither from this page alone.

---

## Verdict (one screen)

| Question | Answer |
|----------|--------|
| One feature or two digs? | **One stack.** Shared near-gun latch. Layer 1 = hide. Layer 2 = snap. |
| Wear-only? | **No** for either layer. No env skips the left cube near the gun, and none snaps a pose. |
| Layer 1 hide | Skip left cube draw in **`geVrHandCubesRender`** when left empty, right has gun, and near mount. Smallest C. No new KEEP knob. |
| Layer 2 snap | **`GETV_VR_TWOHAND` default OFF.** Same latch. Draw left cube at front-support (and/or snap pose). |
| Snap what first? | **A — draw pose only.** Leave left aim / melee / touch / reticle on the real controller. |
| Dual-wield? | **Hide** left cube. **Do not snap.** |
| GUNARM? | **`GETV_VR_GUNARM=1` stays.** |
| APPLY tonight? | **No from this repo.** Hide is small **on the workshop**. Snap is not a one-liner. Bodies are private. |

```
far  → draw left cube at geStereoXrHandWorld / HandBasis   (MASK=1, tonight)
near → TWOHAND=0: skip draw                                 (hide-on-near)
       TWOHAND=1: draw at gunMount × Fwd                    (snap; replaces hide)
left has a gun → skip draw, never snap
```

---

## 1. Root finding (files + functions)

### 1.1 Evidence boundary

| Layer | Where | What it proves |
|-------|--------|----------------|
| Public `goldeneye-native` | this repo | Host-agnostic VR ABI (`ge_vr.h` / `ge_vr_bridge.cpp` / `xr_input.h`). **Not** the playable GETV tree. |
| Public `no6969el/GEVR` | docs + `packaging/` | Ship knobs, boot values, chair refusals, dual-wield cube-hide as **next cut**, PD two-hand numbers. |
| Workshop (private) | `F:\Projects\GEVR\GoldenEyeVR\goldeneye-native` | `geVrHandCubesRender`, `geStereoXr*`, TOUCHUSE / HANDMELEE. **Do not push.** |
| Public decomp | `n64decomp/007` `src/game/gun.h` | `getCurrentPlayerWeaponId`, `Gun_hand_without_item`, `gunUpdateAndFireBothHands`. |

`geVrHandCubesRender` / `geStereoXrHandWorld` / `HandBasis` / `geStereoXrGunMount` / `geTouchBoxDist` **do not appear in any public file**. First chair grep: `geVrHandCubesRender` and `getenv("GETV_VR_HANDCUBES")`. GEVR docs put other `geStereo*` helpers in workshop `vendor/ge-decomp/src/game/stereo.c`.

### 1.2 Left-hand world pose

**Workshop:**

- `geVrHandCubesRender` — **the draw site.** MASK, mm size, per-hand skip, and the new near test all land here (or in a helper it calls).
- `geStereoXrHandWorld` — left controller world translation (game units).
- `HandBasis` — left orientation from the same XR sample.
- Index trap (`GEVR` `docs/102` §4): game `HANDRIGHT=0`, `HANDLEFT=1`; OpenXR `0=left`. `ctrlIndex = 1 - handnum`. Wrong index hides/snaps the wrong hand.

**Public ABI (same split, different names):**

| Helper | Pose | Job |
|--------|------|-----|
| `geVrGetAimRay(hand)` | OpenXR **aim** | Hitscan / barrel |
| `geVrGetWeaponModelMatrixF(hand)` | OpenXR **grip** | Fist / weapon **draw** |
| `geVrGetWeaponDisplacement(hand)` | aim vs head | `weapon_theta/verta_displacement` |
| `geVrHandIsTracked(hand)` | latch | Fall back; never stale floor-aim |

`xr_input.h`: aim = barrel, grip = fist. Hide and snap both measure **grip/draw** vs gun mount, not the aim ray.

### 1.3 Gun mount / grip

**Workshop:** `geStereoXrGunMount` + gunfire mount under `GETV_VR_GUNMOUNT=1`.

**Public / boot (vr441 KEEP — do not flip):**

| Knob | Ship | Role |
|------|------|------|
| `GETV_VR_GUNMOUNT` | `1` | World-space gun at controller |
| `GETV_VR_GUNAIM` | `1` | Shot follows gun |
| `GETV_VR_GUNARM` | `1` | **Floating VR guns. Keep.** |
| `GETV_VR_BODY` | `0` | Full body parked |
| `GETV_VR_BODY_NOARMS` | `1` | No retail / IK arms |

Support-grip is a **second** offset **forward of** the firing-hand grip, not a copy of `GUN_OFF_*`. Historical grip shape: GEVR `docs/154` / `docs/82`.

### 1.4 HANDCUBES as it ships tonight

| Knob | Value | Class | Meaning |
|------|-------|-------|---------|
| `GETV_VR_HANDCUBES` | `1` | KEEP_SHIP | Draw orange cubes |
| `GETV_VR_HANDCUBE_MM` | `60` | PLAYER_PREF | Cube edge ~60 mm |
| `GETV_VR_HANDCUBE_MASK` | `1` | KEEP_SHIP | **Left only.** Right cube off while right holds the gun. |

Chair refusals (`FEATURES-CURRENT.md`, `ROADMAP.md`, `COMING-SOON.md`): Bond cuff rejected; HANDMESH boxes **LOOK REJECTED**; GHOSTHAND parked; dual-wield cube hide is **next cut** (fold into layer 1 here).

Tonight: left cube follows the left controller even when it sits inside the right gun. Any existing intersect skip (if present) is not a named near-gun latch with hysteresis. This dig **names** that latch and uses it for hide, then snap.

### 1.5 Shared proximity (one helper, two consumers)

| Helper | Ship | Reuse? |
|--------|------|--------|
| `GETV_VR_TOUCHUSE` + `_R=12` | ON | **Pattern** only. Door-poke radius. |
| `GETV_VR_HANDMELEE` + `_R=8` + `_COOL=30` | ON | **Pattern** + cooldown idea. Fist radius — too tight. |
| `geTouchBoxDist` (workshop) | — | **Yes, call it** for left-world vs gun-mount. Own thresholds. |
| PD `VR_2H_SEP_MIN 9.0` | prior art | Controllers touching still read ~9 apart. Enter **> 9**. |
| PD two-hand ease | prior art | Guard latch on sim / first eye (`lvframe60`). Fire can run twice per frame. |

**One sticky latch** (`nearGun`). Hide and snap must not each run a raw `d < R` or they chatter against each other.

First-chair numbers (PLAYER_PREF, not KEEP; not `TOUCHUSE_R` / `HANDMELEE_R`):

- Enter ~`14`–`18` game units
- Exit = enter + ~`6`–`8`
- Snap forward offset: small +Z in **gun** frame, then wear

### 1.6 Dual-wield gate

- `Gun_hand_without_item(HANDLEFT)` / `getCurrentPlayerWeaponId(HANDLEFT)`
- If left is **not** empty: **skip left cube**, **do not snap**
- Do not use `getPlayerCount()` (stereo trap; `bondinv.c` already gates dual-wield on `== 1`)

---

## 2. How the two layers sit together

Not “hide vs snap.” Same `nearGun` bit.

| `nearGun` | `GETV_VR_TWOHAND` | Left empty + right gun | Left cube |
|-----------|-------------------|------------------------|-----------|
| 0 | 0 or 1 | yes | Draw at hand (`MASK=1`) |
| 1 | **0** (unset) | yes | **Hide** (skip draw) |
| 1 | **1** | yes | **Snap** draw to front support (layer 2 replaces hide so a support hand is visible) |
| * | * | left has a gun | **Hide.** No snap |

**A — snap draw pose only (recommend for layer 2).** Melee / touch / watch / `#35` left-grip ADS stay on the real left controller.

**B — also snap left aim/pose feed.** Do not ship first. Punches and door pokes teleport to the barrel.

If A wears as “cube glued on, my real fist still floating,” sit B later. Hide-on-near still runs when `TWOHAND=0`.

---

## 3. APPLY sketches — **NOT LANDED**

### 3.1 Shared helper (write once)

Workshop: next to `geVrHandCubesRender` (same TU or `stereo.c`).

```c
/* NOT APPLY READY — workshop sketch.
 * One latch for hide (layer 1) and snap (layer 2).
 * Tick on sim-owner / first eye only (lvframe60 trap). */

static int geVrHandNearRightGun(void)
{
    static int latched;
    /* emptyL = Gun_hand_without_item(HANDLEFT);
     * gunR   = !Gun_hand_without_item(HANDRIGHT);
     * if (!emptyL || !gunR) { latched = 0; return 0; }
     * if (!geVrHandIsTracked(LEFT) || !geVrHandIsTracked(RIGHT)) { latched = 0; return 0; }
     * d = geTouchBoxDist(geStereoXrHandWorld(LEFT), geStereoXrGunMount(RIGHT));
     * if (!latched && d < enter) latched = 1;
     * if ( latched && d > exit)  latched = 0;
     * return latched;
     */
    return latched;
}
```

Optional wear knobs (only if enter/exit need a sit without rebuild): `GETV_VR_TWOHAND_R` / `_EXIT`. Unset → compiled defaults. **Not** KEEP-ON.

### 3.2 Layer 1 — hide-on-near (smallest)

**File:** workshop `geVrHandCubesRender` (grep `GETV_VR_HANDCUBES` / `HANDCUBE_MASK` / `HANDCUBE_MM`).

**No new KEEP knob.** `HANDCUBES=1` + `MASK=1` already ship. Hide is a skip inside that draw.

```c
/* AFTER MASK=1 (left only). GUNARM stays 1. Do not write left aim.
 *
 * for hand in drawn set:
 *   if (hand == LEFT && !Gun_hand_without_item(HANDLEFT))
 *       continue;                     -- dual-wield: hide
 *   if (hand == LEFT && !ge_vr_twohand() && geVrHandNearRightGun())
 *       continue;                     -- hide-on-near
 *   if (hand == LEFT && ge_vr_twohand() && geVrHandNearRightGun())
 *       draw(supportMtx);             -- layer 2; see 3.3
 *   else
 *       draw(geStereoXrHandWorld / HandBasis);   -- tonight
 */
```

**Why this is the smallest change:** one `continue` on the left cube when `nearGun && !TWOHAND`. Dual-wield hide is the same `continue` on `!emptyL` (already wanted for the next cut).

**Knob if chair must A/B hide without a rebuild** (optional, still not KEEP):

```
GETV_VR_HANDCUBE_HIDEGUN   unset / empty = ON    (ship the skip)
                           explicit 0 = OFF      (dig: always show left cube)
```

Prefer **no env** unless the sit needs a wipe. Default-ON getenv is a second sit; do not add it to `$requiredBootKnobs` until hide PASSes.

**APPLY READY?** On the **workshop**, hide is a trivial skip once `geVrHandCubesRender` and the shared latch exist. **Not APPLY READY here** — that function is not in public `goldeneye-native`. Do not land a stub.

### 3.3 Layer 2 — snap-to-support (`GETV_VR_TWOHAND` default OFF)

```
GETV_VR_TWOHAND        unset / empty / 0 = OFF   (chair A/B; not KEEP-ON)
GETV_VR_TWOHAND_FWD    gun-forward offset (wear)
GETV_VR_TWOHAND_TRACE  0   DIG_OFF
```

Do **not** add `TWOHAND` to `gevr-vr441-boot.cmd` allowlist until a sit passes. Scratch boot: `set GETV_VR_TWOHAND=1`.

```c
static int ge_vr_twohand(void)
{
    static int on = -1;
    if (on < 0) {
        const char *e = getenv("GETV_VR_TWOHAND");
        on = (e != NULL && *e != '\0') ? (atoi(e) != 0) : 0; /* default OFF */
    }
    return on;
}

/* if (ge_vr_twohand() && geVrHandNearRightGun())
 *     support = geStereoXrGunMount(RIGHT) * Fwd(TWOHAND_FWD) * supportBasis;
 *     drawLeftCube(support);     -- A: draw only
 *     -- do NOT write left aim / melee / touch
 */
```

Same files as 3.1–3.2. Confirm live `geStereoXr*` names before typing aliases.

**APPLY READY?** **No.** Offset + basis + dual-wield + hysteresis need a sit.

### 3.4 Out of scope

- `GETV_VR_GUNARM=0` / Bond sleeve
- HANDMESH / GHOSTHAND / cuff
- KEEP-ON graduation of `TWOHAND`
- Boot allowlist / pack smoke until hide sit PASS (layer 1) and snap sit PASS (layer 2)
- Personal credit paths. ROM dumps

---

## 4. Chair stare — both layers (plain tester sentences)

**Setup:** vr441-class zip. Boot keeps `GETV_VR_GUNARM=1`, `GETV_VR_HANDCUBES=1`, `GETV_VR_HANDCUBE_MASK=1`. Recenter both sticks. **Right hand holds a gun. Left hand empty.** No dual-wield on the first two runs.

**Run H — hide only:** `GETV_VR_TWOHAND` **unset**.

| | Tester sentence |
|--|-----------------|
| **H-PASS 1** | “When I bring my left controller **next to the right-hand gun**, the orange left cube **disappears**. The gun is still a floating VR gun, not a Bond sleeve.” |
| **H-PASS 2** | “When I **pull my left hand away**, the left cube **comes back** on my left controller. It does **not flicker** if I hover at the edge.” |
| **H-PASS 3** | “Far from the gun, the left cube is **exactly tonight** — left only, about fist-sized.” |
| **H-PASS 4** | “I can still **punch and poke a door** with my left hand while the cube is hidden. Touch and melee did not die.” |
| **H-FAIL** | “The cube stays stuck in the gun.” / “It blinks on and off at the edge.” / “It never hides.” / “It hides even when my left hand is across the room.” / “I grew a suit arm.” |

**Run S — snap on:** same boot plus `GETV_VR_TWOHAND=1`.

| | Tester sentence |
|--|-----------------|
| **S-PASS 1** | “Near the gun, the left cube **sits on the front of the gun** like a support hand. It does **not** just vanish.” |
| **S-PASS 2** | “If I hold that pose, it **does not flicker** between my hand, hidden, and the gun.” |
| **S-PASS 3** | “When I pull away, the cube **returns to my left controller** after a short gap.” |
| **S-PASS 4** | “Left **punch / door / watch** still happen at my real left hand, not at the barrel.” |
| **S-FAIL** | “Cube only vanishes — snap did not draw.” / “Buzzes.” / “My left punch is at the gun.” / “Unset `TWOHAND` does not restore hide-only.” |

**Run D — dual-wield (both H and S):**

| | Tester sentence |
|--|-----------------|
| **D-PASS** | “A gun in each hand: **no** left cube glued to the right gun. Left cube **hides** (or I see two guns, no extra box).” |
| **D-FAIL** | “Left cube snapped onto the right gun while I am holding two weapons.” |

**Regression (every run):** right-hand aim, squeeze ADS on the **gun ray**, right-hand touch-use, casings. Hide/snap must not move `GUNAIM` / `GUNMOUNT`.

---

## 5. Director decision

| If you say… | Then… |
|-------------|--------|
| **Green hide only** | Workshop APPLY 3.1 + 3.2. `TWOHAND` stays off. Sit Run H + D. |
| **Green hide + snap A** | Same, plus 3.3. Sit H, then S, then D. |
| **Green snap, skip hide** | Rejected by this follow-up. Near with `TWOHAND=0` would leave the cube in the gun. |
| **Want B** | Sit A first. Then snap left feed. Expect melee/touch fallout. Hide-only run still required. |
| **Want ghost fingers** | Park. Not this stack. |
| **Reject both** | Stop. Leave tonight’s always-on left cube. |

---

## 6. Attribution / legal

- No personal credit paths edited.
- No GoldenEye ROM, assets, or dumps.
- PD two-hand numbers are **map-only** (public GEVR textbook). Do not copy PD sources.
- Workshop C stays private until release policy flips.
