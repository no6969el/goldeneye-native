# RESULT — colocated Bond body with tracked arms (DIG ONLY)

**Status:** DIG. **Not APPLY READY on this public tree.** No C landed. **No ship.**
**Ask ([GEVR #41](https://github.com/no6969el/GEVR/issues/41)):** Bond's head and torso sit where the player is. Hands follow the controllers. Arms need **real bend points** — no fake attach that leaves the forearm on the walk/aim clip.
**Rank:** after **presence** + **controller aim** (both already shipped). Wear stays **vr441-class** (current zip **vr442**, same BODY / GUNARM pins).
**Constraints:** `GETV_VR_GUNARM=1` stays. Bond cuff / `BODY_Left_Suit_Hand_Floating_Arm` / HANDMESH / GHOSTHAND stay rejected or parked. `GETV_VR_BODY` stays **DIG_OFF=0** until a torso sit PASSes. Do not flip `BODY_NOARMS` off to "get arms."
**Date:** 2026-09-20.
**Evidence:** public `goldeneye-native` HEAD, public `no6969el/GEVR` textbook + `packaging/` (vr441 boot inventory), public `n64decomp/007` (`bondview.h`, `gun.c`/`gun.h`, `player.c`, `chr.h`, `model.h`, `bondconstants.h`). Workshop BODY draw / IK (`GETV_VR_BODY*`, `geStereoXr*`) is **not on any public remote** (`GEVR` `docs/RELEASE-POLICY.md`). Brief names are the workshop symbols.

Director can green-light **torso-only sit**, **park until later**, or **reject fake-attach forever** from this page alone. Arms with real bend are a **second phase**, not the first APPLY.

---

## Verdict (one screen)

| Question | Answer |
|----------|--------|
| Rank vs presence + aim? | **After.** Presence (`GETV_XR_HEAD_TRANSLATE`, `GETV_VR_PLAYSPACE`, stereo) and aim (`GETV_VR_GUNAIM` + `GETV_VR_GUNMOUNT`) already KEEP. Body is later (`FEATURES.md`, `ROADMAP.md`, `COMING-SOON.md`, #41). |
| Wear-only tonight? | **No body.** `GETV_VR_BODY=0` DIG_OFF. Empty left is a cube. Guns are floating VR guns. |
| Two Bond attach paths? | **Yes.** (1) First-person **hand / gun overlay** in `gun.c`. (2) Third-person **player prop + `bodyModel`** (`bondsub`) for MP / intro / death. They are not the same mesh. |
| Fake attach? | **Refuse.** Parenting the hand/gun leaf to the controller while shoulder/elbow keep playing `modelTickAnim` leaves the forearm on the clip. That is the failure #41 names. |
| Phase 1 | **Torso only.** Draw colocated `bodyModel` / player chr. Hide head (camera is inside it). **Keep `BODY_NOARMS=1`.** Guns stay `GUNARM=1`. Default **OFF**. |
| Phase 2 | **Arms with real bend.** Two-bone IK shoulder → elbow → wrist. Wrist = controller **grip**. Shoulder sits on the **smoothed torso**, not raw head yaw. After torso wears. Default **OFF**. |
| Phase 1 knob | Sit with `GETV_VR_BODY=1` on a scratch boot. Do **not** KEEP-ON. Do **not** add to `$requiredBootKnobs` until PASS. |
| Phase 2 knob | New `GETV_VR_BODY_IK` (name TBD on workshop) default OFF — or keep `BODY_NOARMS=1` until IK PASSes, then sit `NOARMS=0` + IK. **Never** `GUNARM=0` as a substitute. |
| APPLY tonight? | **No from this repo.** Workshop body path is private. Public `ge_vr.h` has head + hand pose, not a body drawer. |

```
tonight     BODY=0  NOARMS=1  GUNARM=1   →  no Bond mesh; floating guns; left cube
phase 1 sit BODY=1  NOARMS=1  GUNARM=1   →  torso + legs colocated; no arms; guns still float
phase 2 sit BODY=1  NOARMS=0  + IK       →  same torso; elbows actually bend to the grips
fake        BODY=1  NOARMS=0  no IK      →  FAIL: forearm stays on the fire/walk clip
sleeve      GUNARM=0                     →  FAIL: Bond cuff / floating suit arm (rejected)
```

---

## 1. Rank (why this is not next)

#41: *"Ranks after presence + controller aim. Wear stays vr441."*

| Layer | What "done" means | State on vr442 / vr441-class boot |
|-------|-------------------|-----------------------------------|
| **Presence** | You are in the room. Head 6DoF. Playspace recenter. Stereo fuses. | KEEP: `GETV_XR_HEAD_TRANSLATE=1`, `GETV_VR_PLAYSPACE=1`, `GETV_STEREO=1`, `GETV_XR_PLAY=1`. Public ABI: `geVrGetHeadPosition`, `geVrGetHeadAngles`, `geVrRecenter`. |
| **Aim** | Shot and gun follow the controller, not the face. | KEEP: `GETV_VR_GUNAIM=1`, `GETV_VR_GUNMOUNT=1`. Public ABI: `geVrGetAimRay` (aim/barrel), `geVrGetWeaponModelMatrixF` (grip/draw). |
| **Body** | You look down and Bond is there. Hands at the controllers. Elbows bend. | **Parked.** `GETV_VR_BODY=0`. Public FEATURES: *"Full colocated Bond body … later."* ROADMAP: *"Full-body Bond later."* COMING-SOON: *"ghost hand first."* |

Do **not** jump body ahead of residual presence/aim sits (#45 playspace, #59 shake, #39 gun scale). Those are comfort on a shipped path. Body is a new mesh in the first-person view.

Do **not** block the two-hand snap stack (`getv/patches/twohand-snap-dig/RESULT.md`). That is cubes + a latch on `GUNARM=1`. Body is a different drawer.

[#57](https://github.com/no6969el/GEVR/issues/57) arm-watch **waits for Phase 2** (a real left forearm). Near-term #57 fallback is a farther face billboard, not this.

---

## 2. Code map — current Bond prop / hand attach

Two systems. Mixing them is how you get a cuff at the hip and a gun at the controller.

### 2.1 Evidence boundary

| Layer | Where | What it proves |
|-------|--------|----------------|
| Public `goldeneye-native` | this repo | Host VR ABI (`ge_vr.h` / `ge_vr_bridge.cpp` / `xr_input.h`). **No** body drawer. Grip vs aim is already split. |
| Public `no6969el/GEVR` | docs + `packaging/` | Ship knobs, chair refusals, "full body later." |
| Public `n64decomp/007` | `bondview.h`, `gun.c`, `player.c`, `chr.h`, `model.h` | The two attach paths below. |
| Workshop (private) | `GETV_VR_BODY`, `GETV_VR_BODY_NOARMS`, `GETV_VR_GUNARM` | Already-tried body / sleeve / no-arms. **Do not push.** |

### 2.2 First-person overlay (what you see in SP)

Retail GoldenEye does **not** draw Bond's tuxedo in first person. It draws **per-hand viewmodels**.

**Player fields** (`n64decomp/007` `src/game/bondview.h`):

| Field | Job |
|-------|-----|
| `hands[2]` (`struct hand`) | Per-hand weapon, anim, sway, springs, hold time. Index `GUNRIGHT=0`, `GUNLEFT=1`. |
| `hand_item[2]` / `hand_invisible[2]` | What is equipped; visibility latch. |
| `lock_hand_model[2]` | Freeze swaps (`remove_item_in_hand` sets it). |
| `ptr_hand_weapon_buffer[2]` | Load buffer for the 1P model. |
| `copy_of_body_obj_header[2]` | **Name trap.** This is a **copy of the weapon / item `ModelFileHeader`**, not Bond's torso. `get_ptr_itemheader_in_hand` returns it. |
| `item_related[2]` | Texture pool for that hand's 1P model. |

**Load / swap** (`src/game/gun.c`):

- `used_to_load_1st_person_model_on_demand(hand)` — when `hand_invisible < 0` and unlocked, copies `gitem_structs[item].item_header` into `copy_of_body_obj_header[hand]` and `load_object_fill_header(...)`.
- `place_item_in_hand_swap_and_make_visible` / `draw_item_in_hand` / `remove_item_in_hand`.
- `Gun_hand_without_item(hand)` — empty if invisible **or** `hand_item == 0` (same helper the two-hand DIG uses).
- `getCurrentPlayerWeaponId(hand)`.

**Draw / pose:**

- `gunRenderFirstPersonGunModels` — FP overlay draw (declared `gun.h`).
- `gunSample1PTransform(keyframes, time, matrix, hand)` — view-relative 1P keyframes.
- `WeaponStats.PosX/Y/Z` / `PlayX/Y/Z` — on-screen gun placement and motion play (`gun.h`).
- `gunSetBondWeaponSway` — procedural sway into `weapon_theta/verta_displacement`. VR already wants this **off** while tracked (`patches/DECOMP-PATCHES.md` §5).

**Index trap** (GEVR `docs/102` §4): game `GUNRIGHT=0`, `GUNLEFT=1`; OpenXR `0=left`. `ctrlIndex = 1 - handnum`. Wrong index attaches the wrong sleeve.

**Retail "Bond cuff"** — `BODY_Left_Suit_Hand_Floating_Arm` (`bondconstants.h` body enum, next to the tuxedo bodies). That is a **disconnected suit hand / floating arm**, not a full skeleton. Chair already rejected it. Workshop `GETV_VR_GUNARM=0` is this family (Bond sleeve). **Keep `GUNARM=1`.**

Public ABI for the **replacement** of that overlay (already the ship path):

| Helper | Pose | Job |
|--------|------|-----|
| `geVrGetAimRay(hand)` | OpenXR **aim** | Hitscan / barrel |
| `geVrGetWeaponModelMatrixF(hand)` | OpenXR **grip** | Fist / weapon **draw** |
| `geVrHandIsTracked(hand)` | latch | Never stale floor-aim |
| `geVrGetWeaponDisplacement` | aim vs head | Existing sway fields |

`xr_input.h`: aim = barrel, grip = fist. Body IK wrists must track **grip**, same as gun draw.

### 2.3 Third-person Bond prop (what MP / intro / death already draw)

This is the mesh #41 wants **colocated in first person**.

**Player fields** (`bondview.h`, comments in-tree):

| Field | Offset | Job |
|-------|--------|-----|
| `prop` | `0x00a8` | World `PropRecord`. Collision, rooms, being shot. |
| `bodyModel` | `0x00d4` | **"third person model — other players in MP, and intro/outros/death in SP."** Canonical name **`bondsub`.** |
| `model` | ~`0x598` | Second `Model*` (gait / head-bob path; not the 1P gun). |
| `headbodyoffset` / `standbodyoffset` | `0x544` / `0x554` | Head vs standing body offsets (`bondhead` / gait). |
| `headpos` / `headlook` / `headup` | — | First-person head bob. **Do not** use these as the drawn torso yaw. |

`player.c` `initBONDdataforPlayer`: `prop = NULL`, `bodyModel = NULL` until spawn.

**When the 3P Bond exists:**

| Symbol | Meaning |
|--------|---------|
| `ACT_BONDINTRO` / `ACT_BONDDIE` / `ACT_BONDMULTI` | Third-person Bond actions. |
| `CHR_BOND_CINEMA` (`-8`) | *"only works when bond has a third person model (intro/exit cutscene)"* |
| `BODY_Brosnan_Tuxedo` (and parka / fatigues / formal) | Body enum for the tuxedo / mission kit. |
| `HEAD_Male_Pierce_Bond_*` | Separate **head** attach (`modelAttachHead` / `modelApplyHeadRelations`). |
| `player_gait_object` / `init_player_gait_object` | Walk-bob object header (`initplayergaitobject.c`). |
| `chrRenderProp(prop, gdl, ...)` | Draw a character prop (`chr.h`). |

**World weapon attach on that chr** (`player.c`):

```c
/* sub_GAME_7F09B398 — attach a world weapon prop to the player's chr hand */
chr = g_CurrentPlayer->prop->chr;
if (chr->weapons_held[hand] == NULL) {
    wepid = getCurrentPlayerWeaponId(hand);
    prop  = getPropForHeldItem(wepid);
    flags = (hand == GUNLEFT) ? PROPFLAG_WEAPON_LEFTHANDED : 0;
    something_with_generating_object(chr, prop, wepid, flags, NULL, NULL);
}
```

`ChrRecord.weapons_held[2]` — right, left (`bondtypes.h` ~`0x0160`). Readers: `chrGetEquippedWeaponProp(chr, GUNHAND)`. Drop: `chrSetWeaponFlag4` (`sub_GAME_7F09B368`).

That attach is **leaf-to-hand-node on the animated skeleton**. Fine for a guard. **Fatal as a VR shortcut:** the gun follows the **clip's** hand, not the controller, unless you also re-pose the arm.

### 2.4 Skeleton / bend points (what "real bend" has to write)

The chr model is a `Model` + `ModelNode` tree (`model.h`):

| Helper | Job |
|--------|-----|
| `modelTickAnim` | Advances the walk / fire clip. **This is what must not own the arm in Phase 2.** |
| `modelUpdateMatrices` / `subcalcmatrices` | Builds node Mtxs after anim. |
| `modelFindNodeMtx(model, node, ...)` | The matrix Phase 2 overwrites for shoulder / elbow / wrist. |
| `modelGetNodeRwData` / `setpartoffset` | Per-node RW. |
| `modelAttachHead` / `modelApplyHeadRelations` | Head is a **part**, not baked into the torso file. Phase 1 **skips drawing it**. |
| `modelAttachPart` | Generic part attach (hats, etc.). |

Hit-reaction table (`chr.c`) already names the parts a wearer will stare at: head, chest, pelvis, left/right shoulder, arm, hand. Those are the joints IK must drive. Exact workshop node IDs are **not** in the public tree — confirm on the private `vendor/ge-decomp` before typing aliases.

**Firing clips will fight IK** if left on: `weapon_firing_animation_table` + `ptr_*_firing_animation_groups` (`chr.h`) pose the gun arm every tick. Phase 2 must **replace** the arm channels (or skip arm anim) after `modelTickAnim`, not blend a controller onto a live `fire_standing_*` pose.

### 2.5 What the workshop already parked (knobs only)

Public boot (`GEVR` `packaging/KEEP-DEFAULTS-INVENTORY-vr441.md` §5). Same pins on the vr442 zip path:

| Knob | Ship | Class | Meaning |
|------|------|-------|---------|
| `GETV_VR_GUNAIM` | `1` | KEEP_SHIP | Shot follows gun. **Do not touch.** |
| `GETV_VR_GUNMOUNT` | `1` | KEEP_SHIP | World-space gun at controller. **Do not touch.** |
| `GETV_VR_GUNARM` | `1` | KEEP_SHIP | **Floating VR guns. Keep.** |
| `GETV_VR_GUNARM_TRACE` | `0` | DIG_OFF | |
| `GETV_VR_BODY` | `0` | DIG_OFF | Full colocated body **parked**. |
| `GETV_VR_BODY_NOARMS` | `1` | KEEP_SHIP | No retail / IK arms if a body path runs. |
| `GETV_VR_BODY_NOARMS_TRACE` | wipe | — | Instrument off. |
| `GETV_VR_HANDCUBES` | `1` | KEEP_SHIP | Orange cubes (left via `MASK=1`). |
| `GETV_VR_HANDCUBE_MASK` | `1` | KEEP_SHIP | Left only. |

Chair refusals (`FEATURES-CURRENT.md`): boxy articulated HANDMESH **LOOK REJECTED**. GHOSTHAND parked. Bond cuff rejected (two-hand DIG + this). Public copy: *"Full colocated body + fancy hand mesh unfinished (cube now; ghost fingers later)."*

**Read of the parked pair:** someone already drew a body and then **cut the arms** (`NOARMS=1`) and **cut the whole body** (`BODY=0`). That is the chair saying "a wrong arm is worse than none" — same sentence as GEVR `docs/161` (PD `86` §7): *no IK arm — a wrong arm is more distracting than none.*

### 2.6 Perfect Dark — map only (torso yaw + the elbow trap)

GEVR `docs/218` / `docs/102` §4 (PD VR `bondgun.c`, MIT; **do not copy PD sources**):

| Take | Why |
|------|-----|
| Smoothed **torso yaw** (`VrBodyYaw`, chase ~`0.02` / tick, shortest-angle + hemisphere) | Glance does not twist the chest. Swivel chair / stick turn still catch up. |
| Arm anchor = **head yaw relative to that torso**, not absolute head yaw | PD: *"a 360 spin unwound the elbows."* |
| `VrSeatedMode` as a concept | #41 wear is often a spinning chair. |

Constants are a **starting knob**, not KEEP. Tick-rate differs (60 vs 90). Attribution owed at the point of use if APPLY ever lands.

---

## 3. What "fake attach" is (so Phase 2 cannot "just parent")

```
anim clip  →  shoulder, elbow, forearm   (modelTickAnim)
controller →  hand node or gun leaf      (one matrix write)
```

Wear: cuff/forearm stays where Bond's walk or `fire_hip_*` put it. Hand/gun jumps to the controller. Stretch or a gap at the wrist. That is `BODY_Left_Suit_Hand_Floating_Arm` with extra steps.

**Real bend** (Phase 2 only):

```
torso yaw (smoothed) → shoulder origin
controller grip      → wrist target
IK                   → elbow (and then forearm) actually rotates
then                 → optional gun at the IK wrist (still GUNARM=1 composition, not the retail sleeve)
```

Write **three** node Mtxs (shoulder, elbow, wrist) after anim, each frame, both eyes on the same solved pose (solve once per sim tick; `lvframe60` trap — fire paths can run twice).

---

## 4. Phased plan

### Phase 0 — already shipped (do not reopen)

Presence + aim. `GUNARM=1`. Cubes for empty left. Body parked.

### Phase 1 — torso only (first sit, default OFF)

**Goal:** look down, see a tuxedo / kit torso and legs under you. No arms. Guns still float. Head not a helmet in your teeth.

**Draw:** `player.bodyModel` / `player.prop->chr` through the existing chr draw (`chrRenderProp` or the workshop body wrapper). Colocate with the **capsule + playspace**, not the raw HMD (lean is camera-only — `geVrGetHeadPosition` must not slide the torso through a wall; same rule as `ge_vr.h`).

**Cull:**

- Head part off (`modelApplyHeadRelations` / skip head attach). You are inside it.
- Arms off (`BODY_NOARMS=1` stays).
- Shadow: keep or sit (a blob under the torso is fine; a full 3P shadow that includes arms is not).

**Yaw:** smoothed torso follower (PD `218` maths). Stick turn and a held physical turn catch up; a glance does not spin the jacket.

**Legs:** existing walk / crouch / death on `bodyModel`. Physical crouch already has `geVrPhysicalCrouch`. Do not IK legs in this phase.

**Do not:**

- Flip `GUNARM`.
- Attach `weapons_held` to the hidden 3P hands and expect that to be VR.
- Draw `BODY_Left_Suit_Hand_Floating_Arm`.
- KEEP-ON `BODY`. Scratch boot only: `set GETV_VR_BODY=1` (with `NOARMS=1`, `GUNARM=1`).

**Smallest workshop change:** unpark the existing `GETV_VR_BODY` drawer **with NOARMS still on**, plus torso-yaw + head cull if those are not already in that path. Confirm live symbol names before typing. **Not APPLY READY here.**

**If the existing BODY=1 path still draws arm stumps or a cuff:** that is a Phase 1 FAIL, not a reason to enable IK early. Fix the cull.

### Phase 2 — arms with real bend (after Phase 1 PASS)

**Goal:** hands at the controllers; elbows bend; no leftover clip forearm.

**Solve (per hand, tracked only):**

1. Shoulder world pos from the Phase 1 torso (body-stable).
2. Wrist target = `geVrGetWeaponModelMatrixF` translation (grip). Untracked → hide that arm or freeze last good **only if** torso still draws; never a floor ray.
3. Two-bone IK for elbow. Pole vector: slightly behind/out so the elbow does not flip through the chest on a 360 spin (PD trap).
4. Write shoulder / elbow / wrist Mtxs. Skip or overwrite arm anim channels.
5. Dual-wield: both arms. Empty left: IK to the cube/fist pose, not to the right gun (two-hand snap is a different stack).
6. Gun stays `GUNARM=1` until a later sit parents the **already-correct** VR gun to the IK wrist. That sit is **not** `GUNARM=0`.

**Knob:** default OFF. Do not graduate `NOARMS` to `0` in boot until the stare PASSes.

**Watch (#57):** forearm-locked panel is allowed to *design* against this wrist/forearm mtx. Do not implement it in the body APPLY.

### Phase 3 — parked (not #41)

- HANDMESH / GHOSTHAND / fingers.
- Two-hand support snap (other RESULT).
- GUNZ / HANDSOLID vanish.
- Fancy hub, LAN, etc.

---

## 5. APPLY sketches — **NOT LANDED**

Workshop only. Confirm live names. Do not land stubs in public `goldeneye-native`.

### 5.1 Phase 1 — torso sit

```c
/* NOT APPLY READY — workshop sketch.
 * BODY unset/0 = tonight (parked). Scratch: GETV_VR_BODY=1.
 * NOARMS stays 1. GUNARM stays 1. */

static int ge_vr_body(void)
{
    /* getenv GETV_VR_BODY; empty/unset = 0 (already DIG_OFF). */
    return 0;
}

/* each sim tick, not per eye (lvframe60):
 *   torsoYaw += shortest(headYaw - torsoYaw) * follow;   -- PD 218 maths
 * if (ge_vr_body() && player->bodyModel) {
 *   place body at capsule / prop, yaw = torsoYaw, pitch 0
 *   hide head part
 *   hide arm parts          -- BODY_NOARMS
 *   draw via chr path
 * }
 * do not write hands[] 1P overlay
 * do not write weapons_held
 */
```

Optional wear knobs (not KEEP): `GETV_VR_BODY_FOLLOW` (torso chase), `GETV_VR_BODY_TRACE`.

**APPLY READY?** **No** from this repo. On the workshop, Phase 1 is "unpark + cull + yaw" — still a sit, not a one-liner.

### 5.2 Phase 2 — real bend

```c
/* NOT APPLY READY.
 * Requires Phase 1 PASS. Default OFF.
 * if (!geVrHandIsTracked(hand)) { hide or last-good; return; }
 * shoulder = node mtx on torso
 * wrist    = geVrGetWeaponModelMatrixF(hand)
 * elbow    = two-bone IK(shoulder, wrist, upperLen, lowerLen, pole)
 * modelFindNodeMtx(...) = those three
 * do not leave modelTickAnim as the last writer of the arm
 */
```

**APPLY READY?** **No.**

### 5.3 Out of scope

- `GETV_VR_GUNARM=0` / Bond sleeve / `BODY_Left_Suit_Hand_Floating_Arm`
- HANDMESH / GHOSTHAND / cuff
- KEEP-ON of `BODY` or `BODY_IK`
- Boot allowlist / pack smoke until a torso sit PASS
- Watch-on-arm APPLY (#57)
- Two-hand snap
- Personal credit paths. ROM dumps

---

## 6. Chair stare (plain tester sentences)

**Setup:** vr442 (or vr441-class) zip. Recenter both sticks. `GUNARM=1`, `HANDCUBES=1`, `MASK=1`. **Do not** change aim / mount / playspace.

**Run T — torso (scratch `GETV_VR_BODY=1`, `BODY_NOARMS=1`):**

| | Tester sentence |
|--|-----------------|
| **T-PASS 1** | “I look down and see Bond's **jacket / legs** under me, lined up with where I stand. Not a floating torso across the room.” |
| **T-PASS 2** | “I have **no arms / no suit cuff**. The gun is still a **floating VR gun**.” |
| **T-PASS 3** | “I glance left and the **jacket does not spin** with my nose. If I keep turning (stick or chair), the body **catches up**.” |
| **T-PASS 4** | “I lean; the **camera** moves, the **body stays** on the capsule. I do not clip a wall with a tuxedo.” |
| **T-PASS 5** | “I do **not** see the back of Bond's head / a helmet in my face.” |
| **T-FAIL** | “No body.” / “I grew sleeves or a cuff.” / “The body faces my glance.” / “The body is my head position (I am a giant leaning jacket).” / “Aim or gun mount broke.” |

**Run A — arms (only after T PASS; IK on, `NOARMS=0`):**

| | Tester sentence |
|--|-----------------|
| **A-PASS 1** | “My **hands sit on the controllers**. The **elbows bend** when I move my hands.” |
| **A-PASS 2** | “The forearm is **not** stuck on the walk / fire animation while the hand is out here.” |
| **A-PASS 3** | “I spin a full circle; the elbows **do not unwind** or flip through my chest.” |
| **A-PASS 4** | “Dual-wield: **both** arms follow. Empty left: left arm follows the **left** controller, not the right gun.” |
| **A-FAIL** | “Cuff at my hip, gun in the air.” / “Rubber hose, no elbow.” / “One arm.” / “I got the boxy hand mesh / ghost fingers.” / “`GUNARM=0` sleeve came back.” |

**Regression (every run):** recenter, trigger fire, squeeze ADS on the **gun ray**, touch-use, casings, left cube when empty. Body must not move `GUNAIM` / `GUNMOUNT`.

---

## 7. Director decision

| If you say… | Then… |
|-------------|--------|
| **Green torso sit** | Workshop APPLY 5.1 only. `BODY` stays off in the public boot. Sit Run T. |
| **Park body** | Stop. Leave `BODY=0`. This RESULT stays the map. |
| **Want arms now** | Rejected by this dig. Phase 2 without a torso that wears will re-ship the fake attach. |
| **Want GUNARM=0 / cuff** | Rejected. Same as two-hand DIG. |
| **Want HANDMESH / ghost** | Park. Not #41. |
| **Want watch on arm** | After Phase 2 PASS. Until then #57's farther billboard. |

---

## 8. Attribution / legal

- No personal credit paths edited.
- No GoldenEye ROM, assets, or dumps.
- PD torso-yaw / elbow-unwind notes are **map-only** (public GEVR textbook). Do not copy PD sources.
- Workshop C stays private until release policy flips.
- Decomp citations are `n64decomp/007` (all rights reserved); this DIG describes call sites, it does not vendor the game.
