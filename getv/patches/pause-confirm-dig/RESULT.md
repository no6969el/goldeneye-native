# RESULT — VR pause confirm face-button debt (#32) on vr442 (DIG ONLY)

**Status:** DIG. **Not APPLY READY on this public tree.** No C landed.
**Ask:** GEVR [#32](https://github.com/no6969el/GEVR/issues/32) — in VR pause, **stick highlight moves**; **face-button confirm is still rough**. Prior vr439 A/X boot work may be partial. Find remaining debt on **vr442**. Related [#57](https://github.com/no6969el/GEVR/issues/57).
**Constraints:** DIG only. Do not steal B from reload on ship (`GETV_XR_BTN_B=use` stays). Do not claim face-button select fixed. Do not re-chair `GETV_STEREO=0` (already FAIL / blocked). Workshop C stays private.
**Date:** 2026-09-20.
**Evidence:** public `goldeneye-native` HEAD, public `no6969el/GEVR` docs + packaging + #32/#57/#58, public `n64decomp/007` `options.c` + `joy.c`. Harvest RESULT files named in #32 comments are **not on any public remote**.

Director can green-light the once-gate, park WATCHYN, send #57, or reject from this page alone.

---

## Verdict (one screen)

| Question | Answer |
|----------|--------|
| Is vr439 A/X still on vr442? | **The A half, yes. The B half, no.** `GETV_XR_BTN_A=weapon` still ships. `GETV_XR_BTN_B=start` was **reverted** to `use` (reload). Pause open is Menu / system, not B/Y. |
| Did that packaging close #32? | **No.** It only made N64 A **reach** the watch. Confirm still **toggles twice** in stereo. |
| Why stick works and A does not | Stick never touches `watch_item_is_actively_selected`. A/Z on every **non-inventory** page calls `watch_play_beep_sound()` — a **toggle** — from `draw_watch_current_page`. Both eyes see the same `joyGetButtonsPressedThisFrame` edge. |
| What N1 already gated | Inventory **nav** sites only (workshop `options.c` ~2383 / ~2437). Owner: nav / quit-around works. **The A toggle site was not in that cut.** |
| WATCHYN / Yes-No | **Blocked on the toggle.** `draw_abort_cancel_confirm` draws Confirm/Cancel only when `watch_item_is_actively_selected != 0`. Chair: stick page-swaps, A beeps, Quit never stays in Yes/No. |
| Start-menu Quit “works” | **Different file** (`front.c`). Does not close watch/pause #32. |
| #57 / #58 | #58 is the **same toggle** (left-eye highlight flash). #57 is a **readability / attach** feature, not the confirm wire. |
| APPLY tonight? | **No from this repo.** Smallest workshop APPLY is a first-eye once-gate on the **A toggle** (extend existing `WATCHEYE_ONCE`). No new KEEP knob. No new boot map. |

```
A/X (BTN_A=weapon)     → N64 A bit set          [vr439 packaging; still on vr442]
joyConsumeSamples      → buttonspressed edge     [once per sim tick; NOT cleared on read]
draw_watch_current_page (eye 0)
  A/Z && not inventory → watch_play_beep_sound   → selected=1 + beep
draw_watch_current_page (eye 1)
  same edge still live → watch_play_beep_sound   → selected=0, no second beep
stick Y / D-pad        → highlight only          → looks fine
Yes/No draw            → needs selected!=0       → never appears
```

---

## 1. Root finding (files + functions)

### 1.1 Evidence boundary

| Layer | Where | What it proves |
|-------|--------|----------------|
| Public `goldeneye-native` | this repo | Host-agnostic XR ABI (`src/xr_input.cpp` oculus/touch + `khr/simple` bindings). vr440 `geXrActBtnB` snippet. **Not** the playable GETV tree. |
| Public `no6969el/GEVR` | docs + `packaging/` | vr439 PRs #23/#24; vr441 boot map; vr442 player door (no `gevr-vr442-boot.cmd` in the public tree); CONTROLS / BETA / RELEASE-NOTES still call confirm **partly wired**. |
| Public decomp | `n64decomp/007` `src/game/options.c`, `src/joy.c` | Toggle, A site, Yes/No gate, `buttonspressed` lifetime. |
| Workshop (private) | `F:\Projects\GEVR\GoldenEyeVR\goldeneye-native` | `GETV_XR_BUTTONS` remap, `WATCHEYE_ONCE`, `WATCHYN`, harvest RESULTs. **Do not push.** |

`watch_play_beep_sound` / `draw_watch_current_page` **are** on public decomp. Workshop line numbers in #32 comments differ; **names** are the same.

### 1.2 vr439 A/X — what actually shipped

Packaging only. No game C.

| PR | What | Still on vr442? |
|----|------|-----------------|
| GEVR [#23](https://github.com/no6969el/GEVR/pull/23) | `GETV_XR_BTN_B=start` so B/Y opens pause | **No.** vr441 boot + vr442 notes: **B = use / reload**. Pause = **Menu / system**. Issue #32: do not steal B from reload. |
| GEVR [#24](https://github.com/no6969el/GEVR/pull/24) | `GETV_XR_BTN_A=weapon` so A/X is N64 A (confirm) | **Yes.** vr441 `gevr-vr441-boot.cmd` still sets it. Default without boot was `use` (N64 B) — nothing sent N64 A. |

vr440 boot copied both. vr441 **kept A, flipped B**. Public GEVR has **no** `gevr-vr442-boot.cmd`; player copy is the source of truth:

- `packaging/templates/RELEASE-NOTES-vr442.txt`: “Left stick moves the pause highlight.” “B reloads. Menu / system button opens pause.”
- `docs/CONTROLS.md`: “Face-button confirm in menus is still partly wired.”
- `docs/ship-feature-checklist.md`: “Pause / N64 A confirm is **not** claimed fixed” — then wrongly says vr441 kept the **vr440** A/B map. **B did change.** Checklist is stale on B.

`GETV_XR_BTN_A` / `GETV_XR_BTN_B` / `GETV_XR_BUTTONS` are **PLAYER_PREF / do_not_touch** in KEEP graduation. Another boot remap will not close #32.

### 1.3 How A becomes confirm (workshop vs public ABI)

**vr442 ship (workshop `GETV_XR_BUTTONS=1`, from boot + CONTROLS):**

| Control | Maps to | In pause |
|---------|---------|----------|
| Right **A** / left **X** | `GETV_XR_BTN_A=weapon` → N64 **A** | Confirm / toggle select |
| Right **B** | `GETV_XR_BTN_B=use` → N64 **B** | Reload (not pause) |
| Menu / system | N64 **START** | Open / close pause |
| Left stick | walk / watch highlight | Highlight **works** |
| Tab | keyboard Start | Monitor path |

**Public ABI** (`src/xr_input.cpp` — Phase 0, **not** the zip):

| oculus/touch | Action | `synthesizePad` |
|--------------|--------|-----------------|
| `/user/hand/right/input/a/click` | reload | *(no N64 A)* |
| `/user/hand/right/input/b/click` | use | N64 B |
| `/user/hand/left/input/x/click` | swap | **N64 A** |
| `/user/hand/left/input/y/click` | watch | *(watch bit; not Start)* |
| `/user/hand/left/input/menu/click` | pause | N64 START |
| `khr/simple` left `menu/click` | pause | N64 START |
| `khr/simple` right `select/click` | fire | — |

Issue body: “On Quest Touch, the proper system menu / click binding is still not wired on the oculus/touch paths.” **Public ABI does bind left `menu/click`.** If Quest testers cannot **open** pause, that is workshop-table omission and/or **runtime steal** (VD / Steam overlay eats Menu). Separate from confirm **if the watch is already open** (owner chair: highlight moves).

`GETV_XR_BUTTONS=2` was named as a falsifier on #32 and **never recorded as a sit**. Do not treat it as evidence.

### 1.4 The toggle (public decomp — this is the remaining C)

`n64decomp/007` `src/game/options.c`:

```c
void watch_play_beep_sound(void) {
    if (watch_item_is_actively_selected == 1) {
        watch_item_is_actively_selected = 0;          /* no beep */
    } else {
        watch_item_is_actively_selected = 1;
        sndPlaySfx(..., CAMERA_BEEP1_SFX, ...);       /* beep */
    }
}

/* draw_watch_current_page — A/Z on every page except inventory */
if ((watch_screen_index != WATCH_INDEX_INVENTORY)
    && (joyGetButtonsPressedThisFrame(PLAYER_1, Z_TRIG|A_BUTTON)))
{
    watch_play_beep_sound();
}
```

Inventory **does not** use that toggle (equips via `sub_GAME_7F0A8378` on A/Z/Start). That is the #32 “inventory is a free cross-check.” Options / Quit / briefing **do**.

`src/joy.c` `joyGetButtonsPressedThisFrame` returns `buttonspressed[i] & mask`. That word is rebuilt in `joyConsumeSamples` (rising edges since last consume). **Read does not clear it.** One sim tick, two eye draws → two toggles → beep then clear. Matches every chair line: “A beeps but does not advance”; highlight **left-eye flash** (#58).

Stick highlight is `watch_stick_y_pressed_*` / D-pad / C-buttons. No toggle. **That is why highlight moves and confirm does not.**

### 1.5 Yes/No (why WATCHYN never got a fair test)

`draw_abort_cancel_confirm`: Confirm/Cancel **only** if `watch_item_is_actively_selected != 0`. Then stick X / C-bits (`0x111` / `0x222`) set `D_800409A4`. A second A/Z (`options.c` abort path) commits when `D_800409A4` is set.

If the toggle leaves `selected==0`, Yes/No never draws. Chair: “stick right on either controller only swaps to another options **page**.” That is page-nav, not Cancel vs Confirm.

WATCHYN chair FAIL + empty `watchtrace` did **not** close E1. #32 later accepted: select stayed 0, so WATCHYN was never armed.

### 1.6 What already landed (workshop; not in this repo)

From #32 comments only (harvest files not public):

| Cut | Scope | Chair |
|-----|--------|-------|
| Start-menu khr button-drop | Frontend, not watch | Owner: start-menu Quit **works** (2026-09-19) |
| `WATCHEYE_ONCE` first-eye | Some watch sites | F0 `STEREO=0`: FAIL / unreadable; do **not** re-chair |
| N1 | Inventory **nav** only, existing once-gate, **no new knob** | Nav works; can move / quit around |
| WATCHYN + TRACE | Yes/No helper | FAIL — select never stuck |
| Watch farther / #57 | Readability | N1 chair: menu **too close**, gun overlays text |

vr441 zip aimed to include pause confirm. vr442 notes **do not** claim it. #32 stays OPEN.

---

## 2. Remaining debt on vr442 (ranked)

| ID | Debt | Closes #32? | Ship now? |
|----|------|-------------|-----------|
| **D1** | `draw_watch_current_page` A/Z → `watch_play_beep_sound` runs **per eye** | **Yes — this is the bug** | Workshop APPLY. Extend `WATCHEYE_ONCE` first-eye to **this** site (and any other caller of the toggle). |
| **D2** | Yes/No / `WATCHYN` | Only after D1 sticks `selected=1` | Park until D1 chair PASS |
| **D3** | Quest `menu/click` / runtime steal | Opens pause, not confirm | Scratch sit if a Quest cannot **open** pause. Do **not** ship `BTN_B=start` |
| **D4** | vr439 B=start reverted | Intentional | Keep `BTN_B=use` |
| **D5** | Watch too close / gun on text | Wear, not the edge | #57 farther billboard (near-term) or forearm panel (later) |
| **D6** | #58 left-eye highlight | Same D1 | Closes with D1 |
| **D7** | No public `gevr-vr442-boot.cmd`; checklist lies about B | Docs / pack | Maintainer; not a confirm fix |
| **D8** | `watchtrace` 0-byte capture | Instrument | Park. Direct `goldeneye.exe` stdout if needed after D1 |

**Not remaining:** “A is still mapped to use.” It is not, on the vr441/vr442 boot that testers run.

---

## 3. APPLY sketches — **NOT LANDED**

### 3.1 D1 — first-eye gate on the A toggle (smallest)

**File:** workshop `options.c` — `draw_watch_current_page` A/Z block (public name above). Same `WATCHEYE_ONCE` helper N1 already uses on inventory nav.

**No new KEEP knob.** Unset / empty once-gate = **ON** if that is already how N1 ships; do not add `GETV_WATCHYN` here.

```c
/* NOT APPLY READY — workshop sketch.
 * Tick the toggle on the sim-owner / first eye only (lvframe60 / WATCHEYE_ONCE).
 * Second eye draws with the bit the first eye left.
 *
 * if (watch_screen_index != WATCH_INDEX_INVENTORY
 *     && joyGetButtonsPressedThisFrame(PLAYER_1, Z_TRIG|A_BUTTON)
 *     && geWatchEyeOnce())          -- existing first-eye guard
 *     watch_play_beep_sound();
 */
```

Also grep workshop for every other `watch_play_beep_sound` / write of `watch_item_is_actively_selected` inside an eye loop. One leftover site = same FAIL.

**Why this is the smallest change:** N1 already proved the once-gate pattern. This is the site the toggle actually lives on. Boot A/X map stays.

**APPLY READY?** On the **workshop**, yes — small, named, no new knob. **Not APPLY READY here** — `options.c` is not in public `goldeneye-native`. Do not land a stub.

### 3.2 D2 — WATCHYN (after D1 PASS only)

Do not APPLY WATCHYN on the same cut as D1. If D1 chair still cannot enter Yes/No with stick-X + A, then WATCHYN. Not before.

### 3.3 D3 — Quest menu bind (only if pause will not open)

Workshop: confirm `oculus/touch` suggests `/user/hand/left/input/menu/click` → Start. Public ABI already does.

Scratch (not ship): `GETV_XR_BTN_B=start` restores vr439 B/Y pause. **Steals reload.** Issue #32 forbids shipping that.

### 3.4 #57 — farther / arm (parallel UX, not D1)

See `docs/NOTE-ARM-WATCH-PAUSE-PANEL-DIG-20260918.md` and GEVR #57.

| Approach | Rank |
|----------|------|
| **Farther face billboard** on the same open | Smallest readable fallback. Default **OFF**. |
| Left **Y** opens pause (today Tab / Menu) | Separate bind. Conflicts with vr442 “not Y”. Default **OFF**. |
| Forearm-locked square panel | Wants a real arm. Park until `GETV_VR_BODY` / gunarm story changes. `GUNARM=1` stays. |

#57 does **not** replace D1. A farther panel that still double-toggles is still broken.

### 3.5 Out of scope

- Flipping ship `GETV_XR_BTN_B` back to `start`
- KEEP-ON graduation of `WATCHEYE_ONCE` / `WATCHYN` / any new watch knob
- `GETV_STEREO=0` chair
- Start-menu / `front.c` (owner says Quit works)
- #38 reload, two-hand snap, ghost hand
- Personal credit paths. ROM dumps

---

## 4. Chair stare (plain tester sentences)

**Setup:** vr442 zip (`Start-GEVR.bat`). Recenter both sticks. Open pause with **Menu / system** (or Tab on a monitor clone). Do **not** set `GETV_STEREO=0`. Do **not** set `GETV_XR_BTN_B=start` on the ship sit.

**Run C — confirm (D1 APPLY):**

| | Tester sentence |
|--|-----------------|
| **C-PASS 1** | “Left stick still moves the highlight, one line at a time.” |
| **C-PASS 2** | “One A/X on Game Options / Quit Mission **beeps once** and the highlight **stays** in **both** eyes.” |
| **C-PASS 3** | “A second A/X (or the Yes/No row) reaches **Confirm / Cancel**. Stick left/right picks Cancel vs Confirm, not another page.” |
| **C-PASS 4** | “I can finish Secret Agent / 00 Agent from the watch without the keyboard.” |
| **C-FAIL** | “A beeps and the highlight flashes off.” / “Highlight only in the left eye.” / “Stick swaps pages instead of Yes/No.” / “I grew a B=start mapping and lost reload.” |

**Run Q — Quest open (only if Menu does nothing):**

| | Tester sentence |
|--|-----------------|
| **Q-PASS** | “Left Menu opens pause. A/X still confirms after D1.” |
| **Q-FAIL** | “Menu is eaten by Steam / VD. Face buttons never open pause.” Scratch `BTN_B=start` is a **sit**, not a ship. |

**Run 57 — farther (only if C-PASS but unreadable):**

| | Tester sentence |
|--|-----------------|
| **57-PASS** | “I can read the watch and still hit C-PASS without taking the headset off.” |
| **57-FAIL** | “Gun still covers the text” / “Panel is glued to my face” / “I grew a Bond sleeve.” |

**Regression (every run):** B still reloads in-world. Trigger / squeeze ADS / touch-use / casings unchanged. Auto-Aim stays OFF.

---

## 5. Director decision

| If you say… | Then… |
|-------------|--------|
| **Green D1** | Workshop APPLY 3.1. Sit Run C. Close #32 only if C-PASS 2+3 hold. #58 likely closes with it. |
| **Green D1 + WATCHYN same night** | Rejected by this dig. WATCHYN after C-PASS or C-FAIL that is specifically Yes/No with `selected` stuck on. |
| **Green #57 farther first** | Wear only. Confirm stays broken. Do not swap the order. |
| **Green left-Y open** | #57 bind, default OFF. Not the #32 fix. Conflicts with “pause is not Y.” |
| **Green B=start again** | Rejected for ship. Reload stays on B. |
| **Reject D1** | Stop. Leave vr442 “stick moves, confirm rough.” Docs already honest. |

---

## 6. Attribution / legal

- No personal credit paths edited.
- No GoldenEye ROM, assets, or dumps.
- Public decomp citations are map-only.
- Workshop C stays private until release policy flips.
