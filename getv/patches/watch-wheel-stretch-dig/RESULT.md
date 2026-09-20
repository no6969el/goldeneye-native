# RESULT — VR watch / weapon-wheel hand stretch (#56) (DIG ONLY)

**Status:** DIG. **Not APPLY READY.** No C landed. Do not ship `GETV_HINGE_SLOT` / `GETV_HINGE_PROBE`.
**Ask:** In VR, pulling the watch stretches / breaks a finger or hand. The related **weapon-wheel / weapon-hand** (first-person hand on the gun path) also breaks when that hand or weapon UI is pulled out.
**Constraints:** `GETV_VR_GUNARM=1` stays. GHOSTHAND / HANDMESH / Bond sleeve stay parked. Diagnostic SLOT values stay **OFF** in public boot. Do not merge into #57 (arm-attached watch panel) or #58 (per-eye highlight).
**Date:** 2026-09-20.
**Evidence:** [GEVR #56](https://github.com/no6969el/GEVR/issues/56) + chair comments (2026-09-18/19), tester clip [z4B0Ceqrf6I](https://www.youtube.com/watch?v=z4B0Ceqrf6I), public `n64decomp/007` `gunfire.c` / `bondview2.c` / `gun.c` / `bondconstants.h`, public GEVR vr442 notes. Workshop `GETV_HINGE_*` bodies are **not on any public remote** (`docs/RELEASE-POLICY.md`). Harvest names cited from the issue thread only.

Director can green-light the next chair, reject a global PIN, or send APPLY from this page alone.

---

## Verdict (one screen)

| Question | Answer |
|----------|--------|
| Did Magnum-clip / watch-stretch APPLY land in **vr442**? | **Not claimed.** vr442 RELEASE-NOTES / COMING-SOON / boot template never list a watch-hand or hinge ship. #56 stays **OPEN**. Owner 2026-09-19: *“not claiming fixed in vr442.”* |
| What *did* land (workshop, diagnostic)? | INTERLINK **FAIL**. HINGE_PROBE Arm F **FAIL** (kills R1-*rotation* only). `GETV_HINGE_SLOT` APPLY in workshop `gunfire.c` (default OFF). SLOT=1 COLLAPSE + SLOT=2 PIN + SLOT=3 CLONE **chaired**. PIN rest-pose **PASS**. **Do not ship any SLOT value.** |
| Stretch site | Hinge DL stitch onto **`rwmtx[3]`**. Chair: item=**30** `ITEM_TRIGGER` (Detonator), `mtxidx=3`, `sw28=yes`, org retail hinge, SLOT=1 → `m3.scale=0` / `m3.t==m0.t`. |
| Watch pull vs weapon-wheel hand | **One stitch, two callers.** Watch pull swaps **left** to `ITEM_SUIT_LF_HAND` (first-person “weapon hand”). Watch Laser / Detonator stay on the **same** `gunfire.c` `rwmtx` builder as Magnum. |
| Magnum clip still a trap | **Yes.** `ITEM_RUGER` (`skeleton_gun_revolver`) also writes **`rwmtx[3]`** (cylinder). A **global** PIN of slot 3 would freeze the Magnum clip. Gate on `Switches[28]` / watch family, not “every mtx 3.” |
| Press-finger anim | **Open, separate.** SLOT=2 PIN: finger on the button; shoot does **not** press. Do not block a rest-pose ship on this. |
| Flat / monitor | **Different path.** Owner: Play-on-monitor.bat finger still weird. Not the VR stretch falsifier. |
| APPLY tonight? | **No.** Default path is still retail hinge unless a vr442 chair proves unset == SLOT=2. Next is **F1–F4** (section 4), not a new root dig. |

```
watch pull     → bondviewWatchAnimationTick
                 pause_time==1: draw_item_in_hand(GUNLEFT, ITEM_SUIT_LF_HAND)
                 pause_time==2: if TRIGGER/WATCHLASER → GUNRIGHT ITEM_UNARMED
gun / hand DL  → gunfire.c  rwmtx[]  (same builder for guns + watch gadgets + suit hand)
hinge          → Switches[6] node → mtxidx (chair: 3)
                 Switches[28] hinge axis  (sw28=yes)
                 guRotateF(field_A84 − get_value_if_watch_is_on_hand_or_not)
                 matrix_4x4_multiply_homogeneous(gunmtx, tmp, &rwmtx[sw6mtxidx])
Magnum clip    → skeleton_gun_revolver  Switches[4] → &rwmtx[3]   ← SAME INDEX, OTHER MODEL
stretch        → retail hinge on VR gunmtx  (INTERLINK out; R1-rotation out)
```

---

## 1. Root finding (files + functions)

### 1.1 Evidence boundary

| Layer | Where | What it proves |
|-------|--------|----------------|
| Public `goldeneye-native` | this repo | Host-agnostic watch **gesture** (`geVrWatchGestureActive`). **Not** the playable GETV hinge stitch. |
| Public `no6969el/GEVR` | #56 thread, vr442 tag, `packaging/` | Chair harvest (INTERLINK / PROBE / SLOT), ship notes, boot knobs. **No** `GETV_HINGE_*` in public boot. |
| Public decomp | `n64decomp/007` | Watch pull swap, `rwmtx` builder, Magnum cylinder, watch-on-hand angle. |
| Workshop (private) | `GETV_HINGE_SLOT` / `GETV_HINGE_PROBE` in `gunfire.c` | Diagnostic rewrite of the stitch. **Do not push.** |

vr442 public boot template is still `gevr-vr441-boot.cmd`. Smoke allowlist has `GUNARM=1`, `HANDCUBES=1`. **No hinge knob.** Cook 452 notes: tank, GL, rockets, #38 SETUPCOPY, far-vis, left cube, fault file. Watch-hand is not on that list.

### 1.2 What the tester clip actually shows

[z4B0Ceqrf6I](https://www.youtube.com/watch?v=z4B0Ceqrf6I) (owner callout 2026-09-18; early public wear):

- Watch opened from **keyboard Delete** looks **warped** / “doesn’t look right.”
- Right-hand / gun path has a **“weird long fingery.”**
- Separate from “cannot use the watch” (menu / input — #32 / #57 family).

That is the **geo** bug #56 names: stretch / break on pull-out, not highlight, not pause confirm.

### 1.3 Watch pull (left = weapon-wheel hand)

`bondview2.c` `bondviewWatchAnimationTick` (`WATCH_ANIMATION_0x1`):

```
draw_item_in_hand(GUNLEFT, ITEM_SUIT_LF_HAND);     /* pause_time == 1 */
if (right is ITEM_TRIGGER || ITEM_WATCHLASER)
    draw_item_in_hand(GUNRIGHT, ITEM_UNARMED);     /* pause_time == 2 */
```

`ITEM_SUIT_LF_HAND` is the first-person **suit left hand** on the **gun model path** (`gunRenderFirstPersonGunModels` / `get_item_in_hand_or_watch_menu`). That is the issue’s **“weapon version of the hand”** / weapon-wheel hand: not Bond’s body arm, not the orange HANDCUBE.

`draw_item_in_hand` only sets `weapon_current_animation = 0xE` and `weapon_next_weapon`. The geo is then built by the same `gunfire.c` matrix run as a held gun.

`ITEM_SUIT_LF_HAND` ticks **IDLE** (`gunfire.c` ~2939). It still allocates `rwmtx[]` and still hits Switches[6]/[28] if the hand header has them.

### 1.4 Hinge stitch (the stretch)

Retail `gunfire.c` (viewmodel matrix build, after `matrix_4x4_copy(&gunmtx, rwmtx)`):

```
node = mdlhdr->Switches[6];
sw6mtxidx = modelFindNodeMtxIndex(node, 0);
if (numSwitches >= 0x1D && Switches[28] != NULL)   /* chair: sw28=yes */
    guRotateF(..., field_A84 + τ − get_value_if_watch_is_on_hand_or_not(hand),
              hinge axis from Switches[28]->Data);
    matrix_4x4_set_position(Switches[6]->Data, &tmpmtx);
else
    matrix_4x4_set_position_and_rotation_around_y(...);
matrix_4x4_multiply_homogeneous(&gunmtx, &tmpmtx, &rwmtx[sw6mtxidx]);
```

`get_value_if_watch_is_on_hand_or_not` (`gun.c`):

| Held item | Rest angle |
|-----------|------------|
| `ITEM_TRIGGER` or `ITEM_WATCHLASER` | `0.08726647` (~5°) |
| else | `0.17453294` (~10°) |

Chair SLOT=1 on **item=30 `ITEM_TRIGGER`**: `mtxidx=3`, collapse into the watch arm. **Stretch geo is `rwmtx[3]`.** HINGE_PROBE FORCE=2 (Arm F) left the stretch: R1-*rotation input* is dead; the **slot / parent / scale** half of R1 is not.

SLOT meanings (workshop; default OFF):

| SLOT | Sit | Picture |
|------|-----|---------|
| 0 / unset | retail | stretch (vr441-class; **unclaimed on vr442**) |
| 1 COLLAPSE | finger jammed into arm | `m3.scale=0`, `m3.t==m0.t` — proves the geo |
| 2 PIN | finger **on the button** | rest-pose PASS; press anim missing |
| 3 CLONE | copies `rwmtx[0]` | finger on the **wrong side**; no independent press |

PIN is the **rest-pose** candidate. It is **not** a ship default until F1 says unset == PIN.

### 1.5 Magnum clip — same index, other occupant

`ITEM_RUGER` = 18 (Cougar / Magnum). `skeleton_gun_revolver`:

```
Switches[4] cylinder → matrix_4x4_multiply(gunmtx, tmp, &rwmtx[3]);
Switches[5] hammer   → &rwmtx[4];
```

Watch-menu preview (`set_enviro_fog_for_items_in_solo_watch_menu`) copies the same revolver mapping onto `matrices[3]` / `[4]`.

So **`rwmtx[3]` is overloaded**:

| Model | Who writes `[3]` |
|-------|-------------------|
| Watch Laser / Detonator / hinged watch-hand (`sw28`, chair mtxidx=3) | Switches[6] hinge |
| Magnum / revolver (`skeleton_gun_revolver`) | Switches[4] cylinder (“clip”) |
| KF7 family (flash, `skeleton_gun_kf7`) | can also copy a flash into `rwmtx[3]` |

Prior Magnum-clip / watch-stretch chairs **share this slot**. A Magnum-only cylinder sit can PASS while watch pull still stretches. A **global** slot-3 PIN can PASS watch rest-pose and **FAIL Magnum spin**. That is why F4 exists.

Watch Laser = 23. Detonator / `ITEM_TRIGGER` = 30. Owner last wear: **Watch Laser / Detonator family still stretched** (after SLOT APPLY, before a claimed vr442 default).

### 1.6 What is already closed

| Hypothesis | Verdict | Why |
|------------|---------|-----|
| INTERLINK | **FAIL** | Owner 2026-09-18. Reverted. |
| HINGE_PROBE / Arm F (FORCE=2) | **FAIL** for stretch | Rotation input exonerated. Stretch remained. |
| “Need another root dig before capture” | **FAIL** | SLOT capture works. Geo is `rwmtx[3]`. |
| SLOT=1 as ship | **Rejected** | Collapse is a probe, not a look. |
| SLOT=3 as ship | **Rejected** | Wrong side; kills press. |
| Same as #57 / #58 / #32 | **No** | Panel attach, highlight eye, confirm. Different draws. |
| vr442 already closed #56 | **No** | Issue open. Notes silent. Flat explicitly unclaimed. |

### 1.7 What remains open on vr442

1. **Default watch pull** (no `GETV_HINGE_*`) — stretch vs PIN rest. **Unchaired on Latest.**
2. **`ITEM_SUIT_LF_HAND` pull** — left weapon-hand at the start of watch anim. Same stitch or a second header?
3. **Watch Laser (23) vs Detonator (30)** — chair capture was 30. 23 still owner-reported stretched.
4. **Magnum `rwmtx[3]`** — must survive any default PIN.
5. **Press anim** — after rest-pose. Optional. Foundation note already filed on #56.
6. **Flat finger** — monitor bat; do not use to pass/fail VR.

---

## 2. Ranked remaining causes (only if F1 still stretches)

If F1 (unset vr442) still stretches, do **not** re-run INTERLINK / Arm F.

| Rank | Cause | Why it is still live | Kill |
|------|--------|----------------------|------|
| **R2** | Parent / scale of `rwmtx[3]` under VR `gunmtx` (not the rotate input) | PROBE failed; SLOT=1 collapsed the geo | F1 + F5: PIN-as-default gated on `sw28` |
| **R2b** | Left `ITEM_SUIT_LF_HAND` header uses a **different** mtxidx / no `sw28` | Watch anim swaps left before gadgets settle | F2 |
| **R2c** | Watch Laser header ≠ Detonator (item 23 vs 30) | Capture was 30 only | F3 |
| **Park** | Press-finger curve | Known missing on PIN | F6; later |
| **Park** | Flat / monitor | Owner split | F7; later |
| **Dead** | INTERLINK, R1-rotation | Chair FAIL | do not re-APPLY |

---

## 3. APPLY shape (only after F1–F4)

**Not APPLY READY.** If the director greens a rest-pose after F1 FAIL:

| Rule | Why |
|------|-----|
| Smallest C is **PIN rest on `sw28` rows only** | Chair SLOT=2 PASS. Global `[3]` hits Magnum. |
| Knob `GETV_HINGE_SLOT` stays **default OFF** until F1 unset == PIN | Diagnostic already exists. |
| Do **not** C-default PIN ON | Magnum + KF7 flash share the index. |
| Press anim is a **second** sit | `field_A84` / `sub_GAME_7F05E6B4` already drive the retail rotate. PIN skips it. |
| Workshop body only | Public tree has no `gunfire.c` stitch. |

If F1 **PASS** (unset vr442 already looks like PIN): close the **stretch** half of #56 after F2–F4; leave press + flat open.

---

## 4. Chair stare — remaining falsifiers

**Setup:** **vr442** zip (`Start-GEVR.bat`). Recenter both sticks. Facility. `GETV_HINGE_SLOT` / `GETV_HINGE_PROBE` **unset** unless the row says otherwise. Capture on (`RESULT` §6a from the 2026-09-19 harvest) if the knob is armed.

### F1 — default watch pull (the ship question)

| | Tester sentence |
|--|-----------------|
| **PASS** | “I pull the watch. The finger sits on the watch. It does **not** stretch off-screen or into a long spike.” |
| **FAIL** | “Same long / broken finger as vr441 / the YouTube clip.” |

FAIL → PIN did **not** become default. Next is F5 (SLOT=2 confirm) then APPLY gated PIN. **Do not** invent a new root.

### F2 — weapon-wheel / suit left hand

Same boot. Stare at the **left** hand in the first beats of the pull (`ITEM_SUIT_LF_HAND`).

| | Tester sentence |
|--|-----------------|
| **PASS** | “Left weapon-hand comes out the size of a hand. No rubber-band finger.” |
| **FAIL** | “Left hand is the stretch, even if the watch gadget looks OK.” |

FAIL → left header is a **second** row (maybe no `sw28`, maybe other mtxidx). Do not assume item=30 PIN covers it.

### F3 — Watch Laser vs Detonator

| | Tester sentence |
|--|-----------------|
| **PASS** | “Watch Laser **and** Detonator (item 23 and 30) both rest on the button. Neither stretches.” |
| **FAIL** | “Detonator is OK, Laser still stretches” (or the reverse). |

Capture must print `item=` so 23 vs 30 cannot be guessed.

### F4 — Magnum clip (regression / prior-dig check)

Hold **Cougar Magnum** (`ITEM_RUGER`). Fire / dry-cycle so the cylinder steps.

| | Tester sentence |
|--|-----------------|
| **PASS** | “Cylinder still turns with the shots. Hammer still moves. No collapsed drum.” |
| **FAIL** | “Drum is glued / collapsed into the frame” (global PIN of `[3]`). |

F4 is **mandatory** before any SLOT default. A Magnum-only prior sit does **not** close #56.

### F5 — SLOT=2 vs unset (only if F1 FAIL)

`GETV_HINGE_SLOT=2` vs unset, same Facility pull, **with** capture.

| | Tester sentence |
|--|-----------------|
| **PASS** | “=2 is the button rest. Unset is the stretch. Log shows `sw28=yes` `mtxidx=3`.” |
| **FAIL** | “=2 still stretches” (PIN regress / wrong binary) or “unset already matches =2” (then F1 was mis-scored). |

### F6 — press anim (do not block stretch)

SLOT=2 (or a PIN default). Shoot / use the watch gadget.

| | Tester sentence |
|--|-----------------|
| **PASS** | “Finger presses the button when I shoot.” |
| **FAIL** | “Finger stays parked on the button.” (**Expected today.**) |

### F7 — flat (parked)

`Play-on-monitor.bat` only. Owner already: finger a bit weird, maybe not the VR path. Record; do not pass/fail F1 on it.

**Regression (every VR run):** right-hand aim, squeeze ADS on the gun ray, casings, `GUNARM=1` (no Bond sleeve), left cube still a cube.

---

## 5. Director decision

| If you say… | Then… |
|-------------|--------|
| **F1 FAIL, F4 PASS** | Workshop APPLY: PIN rest **only** when `Switches[28]` (watch hinge). Default OFF until sit. |
| **F1 FAIL, F4 FAIL on a trial PIN** | PIN hit Magnum `[3]`. Narrow the gate. Do not ship. |
| **F1 PASS, F2/F3 PASS, F4 PASS** | Stretch half of #56 is closed on vr442. Leave F6/F7 open. Comment on #56; do not invent APPLY. |
| **F2 FAIL only** | Second header. Probe `ITEM_SUIT_LF_HAND` mtxidx. Do not copy item-30 PIN blindly. |
| **Want press tonight** | Reject as ship. F6 after rest-pose. |
| **Want INTERLINK / Arm F again** | Reject. Already FAIL. |
| **Merge into #57** | Reject. Panel attach is a different draw. |

---

## 6. Attribution / legal

- No personal credit paths edited.
- No GoldenEye ROM, assets, or dumps.
- Workshop `GETV_HINGE_*` C stays private until release policy flips.
- Decomp cites are public `n64decomp/007` names only.
- YouTube clip is the owner-linked tester evidence on #56; no ROM in that request.
