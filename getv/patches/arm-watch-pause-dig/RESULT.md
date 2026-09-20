# RESULT — arm-attached watch / pause panel (DIG ONLY)

**Status:** DIG. **Not APPLY READY on this public tree.** No C landed.
**Ask:** rank **left-Y open + forearm-locked panel** vs **farther face billboard**. Propose the **smallest falsifier, default OFF**. No ship ON without a chair. Related **GEVR #32** (confirm / nav). Later: lift-watch auto-pause.
**Date:** 2026-09-20.
**Evidence:** public `goldeneye-native` HEAD, public `no6969el/GEVR` textbook / vr441 boot / CONTROLS (vr442), public `n64decomp/007` `bondview2.c` / `options.c` / `options.h`, GEVR issues **#57**, **#32**, **#58**. Workshop watch C (`WATCHEYE_ONCE`, `WATCHYN`, `geXrPadAct`) is **not on any public remote**. Brief names are the workshop symbols.

Director can green-light FAR, FAR+Y, ARM, or none from this page alone.

---

## Verdict (one screen)

| Question | Answer |
|----------|--------|
| Rank | **1 — farther billboard.** **2 — forearm-locked panel.** **3 — lift-watch (parked).** |
| Smallest falsifier | **`GETV_WATCHFAR` unset / empty / 0 = OFF.** Skip the 5.9° watch zoom; keep tonight's Menu / Tab open and left-stick nav. |
| Left-Y open | **Separate sit.** Tonight pause is **Menu / system**, not Y. `GETV_XR_BTN_Y=start` (or WATCH→START) default **OFF**. Do not combine with FAR on the first chair. |
| Nav hand | **FAR sit: keep left stick** (N1 already works). **ARM later: right stick + right A.** Left holds the panel. No laser poke first. |
| Arm still moves while paused | **Sit as a stare**, not a first-cut feature. Load-bearing for ARM. Nice-to-have for FAR. |
| Physical arm / auto-pause | **Parked.** `GETV_VR_BODY=0`, `BODY_NOARMS=1`. `geVrWatchGestureActive()` already exists; do not wire it. |
| #32 | Confirm / once-gate is **a different family**. N1 nav works. Remaining #32 chair blocker **is** this unreadability. FAR is the readability half. Do not re-sit `GETV_STEREO=0` (F0 FAIL). Do not conflate #58 highlight or text-stereo. |
| Ship ON? | **No.** Do not add `WATCHFAR` / `WATCHARM` / `BTN_Y` to `$requiredBootKnobs` or vr441/vr442 allowlist. |
| APPLY tonight? | **No from this repo.** F0 (skip zoom) is small **on the workshop**. ARM is not a one-liner. Bodies are private. |

```
tonight     → START/Menu/Tab raises watch + slam FOV to 5.9°  (face-filling; gun on text)
WATCHFAR=1  → same open / same left-stick; skip 5.9° zoom    (billboard farther)
WATCHARM=1  → skip zoom; parent watch page to left grip      (square on forearm)
left Y      → later input sit; not the first variable
lift-watch  → parked until a physical arm exists
```

---

## 1. Root finding (files + functions)

### 1.1 Evidence boundary

| Layer | Where | What it proves |
|-------|--------|----------------|
| Public `goldeneye-native` | this repo | Host-agnostic VR ABI. `GE_VR_BTN_WATCH` = left Y. `GE_VR_BTN_PAUSE` = Menu. `synthesizePad` maps **PAUSE→START only** — **WATCH is not synthesized**. `geVrWatchGestureActive()` is already the lift-watch predicate. |
| Public `no6969el/GEVR` | docs + `packaging/` | vr442 CONTROLS: pause = **Menu / system**, **not Y**, **not B**. Tab on keyboard. Left stick moves the watch highlight. Cinema / frontend already uses a **world-locked hub screen**. |
| Workshop (private) | `F:\Projects\GEVR\GoldenEyeVR\goldeneye-native` | `WATCHEYE_ONCE`, `WATCHYN`, `WATCHTRACE`, `geXrPadAct` / `GETV_XR_BTN_*`. **Do not push.** |
| Public decomp | `n64decomp/007` `bondview2.c`, `options.c` | START opens watch. FOV slam is **5.9°**. Page draw is `draw_watch_current_page`. Select toggle is `watch_item_is_actively_selected`. |

Prior harvest `RESULT-ARM-WATCH-PANEL-20260918` (issue comment only; **not in the public tree**) already ranked **single-eye / hub panel, forearm later**. This page names the files and the falsifier. It does not reopen F0.

### 1.2 Tonight on the wear (vr442 / vr441 boot)

| Knob / bind | Value | Class | Meaning |
|-------------|-------|-------|---------|
| Pause open | Menu / system click | SHIP | `GETV_XR_BTN_B=use` (reload). **Not Y.** Tab on monitor. |
| Watch nav | left stick | SHIP | N1 chair: highlight moves; Quit works after inventory once-gate. |
| `GETV_VR_GUNARM` | `1` | KEEP_SHIP | Floating VR guns. **Keep.** |
| `GETV_VR_BODY` | `0` | KEEP | Full body parked. |
| `GETV_VR_BODY_NOARMS` | `1` | KEEP | No retail / IK arms. There is **no forearm mesh** to attach to. |
| `GETV_VR_HANDCUBES` | `1` / `MASK=1` | KEEP_SHIP | Left cube is the only left-arm stand-in. |
| `GETV_XR_PLAY_SCREEN` | `2` | KEEP | **Frontend / cinema** world-locked quad. In-game watch does **not** use this. |
| `GETV_VR_HUB` | wiped | DIG | Boot clears it. Not a watch knob. |

Chair facts already on #32 / #57:

- F0 (`GETV_STEREO=0`): **FAIL / blocked.** Watch text unreadable in the HMD (high / top). Do not re-chair F0.
- N1 inventory once-gate: **nav works.** Blocker for the next pack: menu **too close** + **gun overlays text**.
- Owner: **pull existing watch farther** for the next zip; **arm-detach (#57) later.**

### 1.3 Why the watch fills the face

Retail pause is not a 2D overlay. It is a **raised watch object** plus a **camera zoom**.

**Open (START):** `bondview2.c:4818` — `START_BUTTON` (or `open_close_solo_watch_menu`) → `trigger_solo_watch_menu(0)`.

**Animation:** `watch_animation_state` 0 → 1/0xd → … → **5 or 12** (held open). `pause_state` 0 unpaused, 1 entering, 2 leaving, **3 paused**.

**The face-fill:** `bondviewZoomToWatchOnOpen` (`bondview2.c:2970`) slams `zoominfovy` to **`WATCHZOOM2` = 5.9°** (EU 6.1). Exit restores **60°** (`bondviewZoomFromWatchOnExit`). On a 320×240 N64 that is "fill the watch face." In an HMD it is a billboard glued to the cornea. N1 "too close" / issue "fills the view" is this slam, not a missing compositor.

**Draw site:** `bondview2.c` ~8455–8543.

- Watch world pose is **camera / Bond-head relative** (`field_488`, `headbodyoffset`, `vv_theta`), then `camGetWorldToScreenMtxf()`.
- Held pose uses `pause_watch_position` + `watch_scale_destination`.
- Page text: `draw_watch_current_page(gdl, finalmtx, state==5 \|\| state==12)` (`options.c`).

There is **no** forearm parent. `GUNARM=1` does not move this mtx. The left cube and the watch are different objects.

### 1.4 Input as it ships vs as #57 wants

**Public ABI** (`ge_vr.h` / `xr_input.cpp`):

| Action | Touch bind | `synthesizePad` |
|--------|------------|-----------------|
| `GE_VR_BTN_WATCH` | left **Y** | **unmapped** (does not become START) |
| `GE_VR_BTN_PAUSE` | left **menu / click** | `N64_START` |

**Workshop vr441/vr442:** `GETV_XR_BUTTONS=1`, `GETV_XR_BTN_A=weapon`, `GETV_XR_BTN_B=use`. CONTROLS: pause is Menu, **not Y**. Keyboard Tab still opens the watch (`CONT_START` / the solo-watch path).

So "press Y to pause" is **not wear-only** — Y is already an OpenXR action on the public ABI and is **dropped** on the way to `OSContPad`. Smallest Y sit is a pad-synth / `geXrParseAct` line, not a new action.

**Nav tonight:** `options.c` page handlers read `joyGetStick*` / `joyGetButtonsPressedThisFrame` on **PLAYER_1**. Workshop maps the **left** stick there. Right stick is turn (`GETV_XR_TURN=1`) and must stay turn **out of** the watch. Inside the watch, turn should not yaw the world under the panel.

**Confirm (#32):** `watch_item_is_actively_selected` toggles on A (`watch_play_beep_sound`, `options.c:547`). Dual-eye without a first-eye gate = on+beep then off. Inventory sites were gated (`WATCHEYE_ONCE`, N1). **Leave that family alone on this sit.** A FAR chair with a pre-N1 binary will look like #32 again.

### 1.5 Arm-still-moves / freeze

Retail: after `pause_state==3` the world and the raise-arm anim **hold**. The watch mtx is not the XR left grip.

VR tonight (`GUNARM=1`): gun / left-cube draw **may** still sample XR each frame, or pause may skip that tick. **Not proven on a public binary.** Chair it:

- If the left cube still tracks while paused → ARM can parent to live grip.
- If it freezes → ARM needs a pause keep-alive on the left grip sample only (do not unpause AI / guns / clocks).

FAR does not need that keep-alive. The billboard is head- or world-locked.

### 1.6 Lift-watch (later)

`geVrWatchGestureActive()` (`ge_vr_bridge.cpp:309`): left **grip** within 0.40 m of the head **and** watch-face (+Y in grip) toward the eyes (`toward > 0.5`). Distance alone is rejected (reload false trigger).

Do **not** OR this into START until a physical forearm exists. Tonight the left cube is a fist box, not a watch face. Wiring the gesture now is a false-pause machine.

---

## 2. Rank: two placements, one later gesture

Not "ARM vs FAR as equals." Same watch page. Different parent.

| Rank | Placement | Open | Nav | Needs | First sit? |
|------|-----------|------|-----|-------|------------|
| **1** | **Farther billboard** — skip 5.9° zoom; leave the existing watch / page where the camera put it, just not in the cornea | Menu / Tab (tonight) | **Left stick** (tonight) | One getenv in `bondviewZoomToWatchOnOpen` | **Yes** |
| **2** | **Forearm-locked panel** — skip zoom; parent `watchmtx` / `draw_watch_current_page` to left **grip** + forearm offset; small square | Menu or Y | **Right stick + right A** | Live left pose while paused; scale; offset; nav-hand split | After FAR PASS |
| **3** | **Lift-watch auto-pause** | `geVrWatchGestureActive()` | same as 2 | Physical arm (`BODY` / real cuff). Rejected while `BODY_NOARMS=1` | Parked |

**Why FAR is smaller than ARM**

1. The unreadability is already named: 5.9° zoom + gun in the text. Skip the zoom without inventing a parent.
2. Owner + prior DIG already ordered **farther first, arm later**.
3. No forearm exists (`BODY_NOARMS=1`). ARM tonight = "glue a square to the left cube." That is a second product, not a zoom skip.
4. FAR reuses the cinema lesson (FEATURES: hub screen is world-locked, "not a billboard on your face") **without** entering the hub room or flipping `GETV_XR_PLAY_SCREEN`.
5. FAR keeps #32's working left-stick nav. ARM adds a second variable (which hand aims the menu).

**Why ARM still wins later**

- Desired end state: HUD-on-arm, look down, right hand picks options, world frozen, **arm still moves**.
- FAR will never feel like a watch. It is the **readable pause** that lets Secret Agent / 00 Agent finish.
- Gesture is a third layer on ARM, not a rival to FAR.

**Reject as first sits**

| Idea | Why not first |
|------|----------------|
| Hub-room teleport (reuse cinema `PLAY_SCREEN=2` for in-game pause) | Leaves the level. Heavier than a zoom skip. Prior DIG's "hub panel" is a **fallback if F0/F1 fail**, not the default. |
| Laser poke / ray select | New hit-test. #32 is stick + A. |
| `GETV_STEREO=0` | F0 FAIL. Unreadable; do not re-chair. |
| Left-Y + ARM + FAR in one binary | Three variables. Any FAIL is ambiguous. |
| Default ON | Issue: no ship ON without chair. |

---

## 3. APPLY sketches — **NOT LANDED**

### 3.1 Layer 1 — `GETV_WATCHFAR` default OFF (smallest falsifier)

**File:** workshop `vendor/ge-decomp/src/game/bondview2.c` — `bondviewZoomToWatchOnOpen` (`:2970`). Optional wear: `pause_watch_position` / `watch_scale_destination` at the draw site (`:8475`).

```
GETV_WATCHFAR        unset / empty / 0 = OFF   (chair A/B; not KEEP-ON)
GETV_WATCHFAR_FOV    optional; unset = 60      (do not slam 5.9)
GETV_WATCHFAR_Z      optional push (wear only if skip-zoom is still too near)
GETV_WATCHFAR_TRACE  0   DIG_OFF
```

Do **not** add `WATCHFAR` to `gevr-vr441-boot.cmd` / `Start-GEVR.bat` until a sit PASSes. Scratch: `set GETV_WATCHFAR=1`.

```c
/* NOT APPLY READY — workshop sketch. Default OFF. */

static int ge_watch_far(void)
{
    static int on = -1;
    if (on < 0) {
        const char *e = getenv("GETV_WATCHFAR");
        on = (e != NULL && *e != '\0') ? (atoi(e) != 0) : 0; /* default OFF */
    }
    return on;
}

/* bondviewZoomToWatchOnOpen:
 *   if (ge_watch_far()) { trigger_watch_zoom(60.f or WATCHFAR_FOV, f); return; }
 *   else retail 5.9f slam.
 * Do not touch trigger_solo_watch_menu open. Do not remap Y. Do not parent mtx.
 */
```

**F0 (skip zoom only)** is the smallest C: one getenv, one branch, retail close path unchanged.

**F1 (skip zoom + Z/scale)** if F0 still reads "in my teeth." Same knob. Wear `WATCHFAR_Z` / scale. Still not ARM.

**F2 (cinema-style world-locked quad)** only if F0+F1 fail. Reuse the virtual-screen / `GETV_XR_PLAY_SCREEN` compositor for the **watch page RT**, ~1–2 m, world-locked (head-turn does not carry the text). Heavier. Not the first APPLY.

**Why this is the smallest change:** the face-fill is one function. Everything else (START, pages, N1 once-gate, left stick) stays.

**APPLY READY?** On the **workshop**, F0 is a trivial skip. **Not APPLY READY here** — `bondview2.c` is not in public `goldeneye-native`. Do not land a stub.

### 3.2 Layer 1b — left Y → pause (default OFF, **second** sit)

```
GETV_XR_BTN_Y        unset = tonight (Y unused / not START)
                     start = CONT_START / watch open
```

Or, on the public-ABI side: `synthesizePad` `GE_VR_BTN_WATCH → N64_START` behind the same default-OFF getenv.

Ship Menu / Tab **stay**. Y is additive. Do **not** steal B (reload, #38 family). Do **not** put Y on the boot allowlist.

Chair **after** FAR PASS so "Y opened a still-unreadable wall" is not a FAR FAIL.

### 3.3 Layer 2 — `GETV_WATCHARM` default OFF (after FAR)

```
GETV_WATCHARM        unset / empty / 0 = OFF
GETV_WATCHARM_MM     square edge (wear; start ~80–120 mm)
GETV_WATCHARM_OFF_*  grip → forearm (wear)
GETV_WATCHARM_TRACE  0   DIG_OFF
```

**File:** same draw block `bondview2.c` ~8455. Replace camera-relative `watchpos` / `watchmtx` with left **grip** (`geVrGetWeaponModelMatrixF(LEFT)` / workshop `geStereoXrHandWorld(LEFT)` + `HandBasis`). Keep `draw_watch_current_page` on that mtx. Skip zoom (FAR or implicit).

**Nav:** when `WATCHARM=1` and `watch_animation_state` is 5/12, feed **right** stick into `joyGetStick*` for page handlers; leave **left** stick dead for walk. Right A = confirm (same #32 A path, still first-eye gated). Left Y / Menu still close.

**Pause keep-alive:** if the left cube freezes at `pause_state==3`, sample left grip in the watch draw only. Do not run chr / gunfire / clocks.

**GUNARM=1 stays.** Do not grow a Bond sleeve (`BODY=0` stays). Dual-wield: still attach to left grip (the watch is the left hand).

**APPLY READY?** **No.** Offset + basis + pause keep-alive + nav-hand split need a sit.

### 3.4 Out of scope

- `GETV_VR_GUNARM=0` / Bond cuff / HANDMESH / GHOSTHAND
- Lift-watch OR into START
- KEEP-ON graduation of `WATCHFAR` / `WATCHARM` / `BTN_Y`
- Boot allowlist / pack smoke until FAR sit PASS
- Re-opening #32 once-gate, #58 highlight, text-stereo overlay
- Hub-room teleport as the default
- Personal credit paths. ROM dumps

---

## 4. Chair stare (plain tester sentences)

**Setup:** vr442-class zip (or a workshop rebuild that already has **N1** `WATCHEYE_ONCE`). Boot keeps `GUNARM=1`, `BODY=0`, `BODY_NOARMS=1`, `HANDCUBES=1`, `BTN_B=use`. Recenter both sticks. Dam or Archives. **Do not** set `GETV_STEREO=0`.

**Run F — farther only:** `GETV_WATCHFAR=1`. `WATCHARM` unset. **Y not remapped.**

| | Tester sentence |
|--|-----------------|
| **F-PASS 1** | "Menu / Tab still opens the watch. I did **not** need Y." |
| **F-PASS 2** | "The watch / options are **readable in the headset**. They are **not** glued to my eyes. I can see the room around the panel." |
| **F-PASS 3** | "The **gun is not sitting on the text**. I can read Inventory and Game Options." |
| **F-PASS 4** | "Left stick still moves the highlight **one step**. A still beeps **once** and Quit can reach Yes/No." (#32 must still hold) |
| **F-PASS 5** | "Guards / world are frozen. I can still turn my **head**. The panel does not smear." |
| **F-STARE** | "The left cube / gun **does / does not** follow my hands while paused." (record; do not fail FAR on freeze) |
| **F-FAIL** | "Still fills my face." / "Text is at the top / cut off." / "Gun still eats the words." / "Unset `WATCHFAR` does not restore tonight." / "Highlight double-toggles." (that's #32, not FAR) |

If F-PASS 2/3 FAIL after skip-zoom only → one more workshop pass with `WATCHFAR_Z` (F1), **same knob**. If that still fails → F2 world-locked quad. **Do not jump to ARM to fix unreadability.**

**Run Y — left Y (only after F-PASS):** same FAR boot plus `GETV_XR_BTN_Y=start` (or equivalent).

| | Tester sentence |
|--|-----------------|
| **Y-PASS** | "Left **Y** opens and closes the same readable watch as Menu / Tab. **B still reloads.** Menu still works." |
| **Y-FAIL** | "Y does nothing." / "Y steals reload." / "Y opens an unreadable wall." (Y sat too early) |

**Run A — forearm (only after F-PASS; separate binary / knob):** `GETV_WATCHARM=1`. `WATCHFAR` may stay on (zoom skip).

| | Tester sentence |
|--|-----------------|
| **A-PASS 1** | "A **small square** sits on my **left forearm / left cube**, not in the middle of my face." |
| **A-PASS 2** | "While paused I can **move my left arm** and the panel comes with it." |
| **A-PASS 3** | "I pick options with the **right** stick / right A. Left stick does **not** walk me." |
| **A-PASS 4** | "Looking away from the watch does **not** auto-close. Y / Menu still closes." (gesture still off) |
| **A-FAIL** | "Panel stuck in my face." / "Arm frozen, panel left behind." / "I grew a suit arm." / "Right stick turns the world." |

**Regression (every run):** right-hand aim, squeeze ADS, B reload, both-stick recenter, casings. FAR/ARM must not move `GUNAIM` / `GUNMOUNT`. Flat `Play-on-monitor.bat` + Tab must stay tonight when knobs are unset.

---

## 5. Director decision

| If you say… | Then… |
|-------------|--------|
| **Green FAR F0** | Workshop 3.1 skip-zoom only. Sit Run F. Leave Y and ARM off. |
| **Green FAR F0+F1** | Same, plus `WATCHFAR_Z` if F0 is still near. |
| **FAR failed, try hub quad** | F2. Still not ARM. Still default OFF. |
| **Green Y after FAR PASS** | 3.2. Sit Run Y. |
| **Green ARM** | Only after FAR is readable. 3.3. Sit Run A. |
| **Want lift-watch now** | Rejected. No physical arm. Gesture stays parked. |
| **Ship ON / boot allowlist** | Rejected until a named sit PASSes. Then KEEP is a **later** graduation, not this dig. |
| **Reject all** | Stop. Tonight's Menu / Tab + face-filling 5.9° watch stays. |

---

## 6. Attribution / legal

- No personal credit paths edited.
- No GoldenEye ROM, assets, or dumps.
- Decomp line numbers are **map-only** from public `n64decomp/007`.
- Workshop C stays private until release policy flips (`GEVR` `docs/RELEASE-POLICY.md`).
- Prior harvest RESULT names in #57/#32 comments are cited as issue text, not copied from private files (those files are **not** on the public remotes).
