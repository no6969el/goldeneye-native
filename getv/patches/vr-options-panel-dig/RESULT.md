# RESULT — VR options panel on intro / main-menu room (DIG ONLY)

**Status:** DIG. **Not APPLY READY on this public tree.** No C landed. Phases 2–4 (empty panel + registry, turn-speed row, add-option docs) are **not** this PR.
**Ask (GEVR public tracker, not this repo):** a **world-locked** VR options window on the **RIGHT** of the intro / front main-menu space (same era as the cinema billboard / placeholder hub). Point with controllers to change settings **before** a mission / VR gameplay session. First row later: right-stick turn speed on the **existing** `GETV_XR_TURN_SCALE` (default **60**). Build it as a **registry**, not a one-shot knob.
**Wear stays vr442 / public Latest.** Do not ship a panel ON without a chair sit.
**Date:** 2026-09-20.
**Evidence:** public `goldeneye-native` HEAD, public `no6969el/GEVR` textbook + vr441/vr442 boot / FEATURES / CONTROLS / KEEP inventory / U-19 (`docs/175`) / U-04 (`docs/169`) / docs `61` / #42 DIG, public `ge_vr.h` + `src/xr_input.cpp`. Workshop cinema / `port_input.c` bodies are **not on any public remote**. Brief names below are the **getenv strings** plus GEVR textbook symbols. First chair grep those strings; do not type aliases until the live names match.

Director can green-light Phase 2 shell, persist sidecar, or none from this page alone.

---

## Verdict (one screen)

| Question | Answer |
|----------|--------|
| Where does a second panel go? | **Same small hub as the cinema.** World-locked quad **to the player’s right** of `PLAY_SCREEN=2`. Cinema family — **not** head-glued, **not** the watch, **not** `GETV_VR_HUB`. |
| Host TU | Workshop cinema / hub draw next to `getenv("GETV_XR_PLAY_SCREEN")` (same space U-19 / #42 already named). Public tree has **no** cinema C. |
| Point / select tonight? | **No UI picker.** Hands already publish **aim pose** + **TOUCHUSE** poke. Face **A** is the #32 trap. |
| Smallest select | **1 — aim-ray vs the panel plane + trigger.** **2 — TOUCHUSE ± pads** (same family as #42). Stick highlight is fallback, not “point.” |
| `TURN_SCALE` persist? | **Process env only.** Boot seeds `GETV_XR_TURN_SCALE=60`. No `vr.json` / ini. Reader is workshop `getv/port/src/port_input.c`. |
| Menu must drive | The **existing** `TURN_SCALE` static (or a sidecar the boot `call`s **after** the ship `=60`). **Do not mint a second turn system.** |
| Latch trap | Typical `static … = -1; if (x < 0) getenv` (U-04 / `docs/61`). Env write after first read is a no-op unless unlatched / setter. |
| In a mission? | **No.** Gate on frontend / intro / cinema hub only. Gone after gameplay VR. |
| #74 / HT=0? | **Do not touch.** `HEAD_TRANSLATE=0`, `PLAYSPACE=1`, `GUNREBASE` stay #74’s. |
| APPLY tonight? | **No from this repo.** Phase 2 is workshop shell + registry. This page is the map. |

```
boot (PLAY=1, SCREEN=2, AUTOSCREEN=1, FOVSCALE_CINEMA=85)
  frontend / intro / file-select  → small hub + world-locked cinema (CENTER)
                                  → options panel belongs RIGHT of that screen
  enter gameplay                  → recenter; panel GONE

TURN=1            KEEP_SHIP     right-stick yaw armed
TURN_SCALE=60     PLAYER_PREF   deg/s-class scale (boot + C default 60)
TURN_DEAD=20      PLAYER_PREF   stick deadzone
TURN_INVERT=      wipe          do not invent a second invert
TURN_HAND=        wipe          right stick stays turn
```

---

## 1. Host — second world-locked panel on the RIGHT

### 1.1 Evidence boundary

| Layer | Where | What it proves |
|-------|--------|----------------|
| Public `goldeneye-native` | this repo | Host-agnostic VR ABI. **No** cinema / hub draw. **No** `PLAY_SCREEN` C. |
| Public `no6969el/GEVR` | docs + `packaging/` | Cinema already ships. #42 DIG + FEATURES / CONTROLS copy. Issue **#76** lives here. |
| Workshop (private) | `F:\Projects\GEVR\GoldenEyeVR\goldeneye-native` | `GETV_XR_PLAY_*` readers, hub / cinema draw. **Do not push.** |
| Textbook | GEVR `docs/175` U-19, `docs/169` U-20 / **U-04**, `docs/178` | Mechanism: real-depth rectangle when there is **no eye split**. Fancy hub is **later**. |

`GETV_XR_PLAY_FOVSCALE_CINEMA` / `PLAY_SCREEN` / `AUTOSCREEN` **do not appear in any public C file in this repo.** First chair grep those three plus `GETV_XR_PLAY`, `GETV_XR_PLAY_AT`, `GETV_VR_HUB`. Same grep #42 already named. GEVR docs put other play helpers in workshop `getv/port` and `vendor/ge-decomp/src/game/stereo.c`.

### 1.2 Tonight’s room (vr442 / Latest)

Player copy (GEVR `FEATURES.md`, `CONTROLS.md`):

> Menus and intro cinema sit on a screen in a small hub room. Look left and right — the screen stays nailed in space; you are not wearing a billboard on your face.

| Knob | Boot | Class | Role |
|------|------|-------|------|
| `GETV_XR_PLAY` | `1` | KEEP_SHIP | Play / cinema arm. **Do not flip.** |
| `GETV_XR_PLAY_SCREEN` | `2` | KEEP_SHIP | World-lock mode. `2` is the shipped billboard. |
| `GETV_XR_PLAY_AUTOSCREEN` | `1` | KEEP_SHIP | Auto cinema on frontend / intro / menus; drop to gameplay VR when an eye pair exists. |
| `GETV_XR_PLAY_FOVSCALE_CINEMA` | `85` | PLAYER_PREF | Billboard scale. 85 = big screen, not HMD-fill. |
| `GETV_XR_PLAY_AUTORECENTER` | `1` | KEEP_SHIP | Recenter into play. Same chord as both-stick click. |
| `GETV_VR_HUB` | wipe | C_DEFAULT UNKNOWN | Fancy PD-style room. **Not armed.** FEATURES: “fancier hub room are later.” |
| `GETV_VR_TITLEBG` | wipe | C_DEFAULT UNKNOWN | Title-walk backdrop. Not this panel. |

U-19 (`docs/175`, older RT64 host, still the mechanism): when there is **no eye split** (frontend, menus, cutscenes, opening frames), draw a **real-depth rectangle** from the flat present target. Original knobs: `-ScreenDist` **2.5 m**, `-ScreenSize` **2.6 m** width. Each eye’s own frustum → the quad has parallax; head yaw does **not** carry the picture. U-20 (`docs/169`): PD hub is **two more quads behind that screen**. GEVR already shipped a **small** hub; `GETV_VR_HUB` is the parked fancy one.

`OPTION_SCREENCINEMA` in `file2.h` is N64 **letterbox**, not this hub. `GETV_XR_FOVMATCH` is the forbidden camera rewrite. Neither is a host for this panel.

### 1.3 Where the RIGHT panel attaches

The cinema quad is already a **world-locked** textured rectangle in hub space. A second panel is **another quad in that same pose frame**, offset to the wearer’s **right** when they face the cinema.

```
                    [ cinema / intro / file-select ]
                    world-locked, FOVSCALE=85
   player  --->            CENTER
                              |
           [ VR OPTIONS ] ----+---- (do not put it on the HMD)
              RIGHT, world-locked
              same hub origin as cinema
```

| Do | Don’t |
|----|-------|
| Parent to the **hub / cinema world** (same TU as `PLAY_SCREEN`) | Parent to HMD / view / `bondviewZoomToWatchOnOpen` (that is #57 face-fill) |
| Visible on **intro load + front main menu** (AUTOSCREEN cinema era) | Require `bossSetLoadedStage` / a mission / `#208` AutoLevel |
| Hide when gameplay stereo eyes exist (same gate as cinema→VR) | Leave it up in Facility / Dam |
| Sit **right** of the billboard so the cinema stays readable | Cover the cinema; grow `GETV_VR_HUB=1` |
| Share hub recenter (both sticks / AUTORECENTER) | Invent a second playspace / `HEAD_TRANSLATE` |

#42’s optional **CINEMA / VR** pads (if greened) sit **in front of** the billboard as poke volumes. This panel is the **right wall**. Sibling, not a rewrite. Do not reuse `GETV_XR_PLAY_PICKER`. Do not block #42.

Architecture (`GE007-VR-ARCHITECTURE.md` §6.4 / §9): full-screen 2D (pause, briefing, intro) composites as a **large world-locked quad**. That is this family. Head-locked HUD at ~2 m is the **other** tier — reject it for #76.

### 1.4 Not this host

| Thing | Why parked |
|-------|------------|
| Pause / watch (`options.c`) | After you already picked a mode. #32 confirm still rough. #57 is forearm / farther **watch**, not the front room. |
| `GETV_VR_HUB=1` / U-20 art | Fancy room. FEATURES: later. #76 is a **second quad**, not a new room. |
| `front.c` N64 start-menu | 2D file-select on the **cinema texture**. Do not replace it; the options window is **beside** it. |
| In-mission floating HUD | Ask is **before** stage inject. |
| PD `vr_hub.cpp` | Pause **environment** only. Never blits the game. Map-only. Do not copy sources. |

---

## 2. Controller point / select

### 2.1 What already publishes

| Signal | Public / ship | Use for #76? |
|--------|---------------|--------------|
| `/input/aim/pose` per hand | `src/xr_input.cpp` → `HandState.aim`; `geVrGetAimRay()` | **Yes — the point ray.** Same pose the gun uses. |
| `/input/grip/pose` | fist / `geVrGetWeaponModelMatrixF` | Volume poke, not the pointer. |
| Trigger | `HandState.trigger`; fire bits | **Select / click** on a hovered row (not a shot). |
| `GETV_VR_TOUCHUSE=1` `_R=12` | KEEP_SHIP / PLAYER_PREF | **Poke ±** if ray UI is late. Door family. `#40` already ships. |
| `geTouchBoxDist` + `geVrHandIsTracked` | workshop + public ABI | Hand vs pad box. No stale floor poke. |
| Left stick | walk / watch highlight | **Fallback nav** only. Works in pause (#32). Not “point.” |
| Right stick | `GETV_XR_TURN=1` | **Stays turn.** Do not steal it for the slider while the panel is up unless a sit says so — then **pause yaw** while hovering, do not invent a new turn. |
| A / X (`BTN_A=weapon` → N64 A) | PLAYER_PREF | **#32 trap.** Stereo double-toggle. Do **not** first-cut confirm here. |
| B / Y | reload / (Y not pause) | Leave. Pause is Menu / system. |
| `GETV_VR_HANDCUBES` / `HANDMESH_POINT` | visual | Cubes / finger curl. **Not** a picker. |
| `geVrWatchGestureActive` | ABI exists | Lift-watch. Parked. Not this room. |

Sit gate already on the zip (`ship-feature-checklist.md`): “Left stick walks. Right stick turns.” “Touch-use: poke a door / console with either hand.”

### 2.2 Rank — how the wearer changes a row

Goal of #76 UX: **pointable / selectable with controllers** in the hub, before a mission.

| Rank | Path | Reuses | Risk |
|------|------|--------|------|
| **1 — aim ray + trigger** | Intersect `geVrGetAimRay` (or workshop aim pose) with the **world-locked panel plane**. Hover highlight. Trigger (or a dedicated click, **not** A) commits ± / toggle. | Aim pose + first-eye once-gate | Double-fire per eye (`#32` trap); ray fights cinema if the plane is too deep |
| **2 — TOUCHUSE ± pads** | Two poke volumes on the row (`−` / `+`) in hub space. Same pattern #42 ranked for boot pads. | `TOUCHUSE` + `_R=12` + `geTouchBoxDist` | Chatter; pads leaking into gameplay |
| **3 — stick while hovered** | Left stick Y steps the focused row. | Watch nav (PLAYER_1 `joyGetStick*`) | Works; not “point.” Keep as accessibility, not the headline. |

**Recommend 1, with 2 as the no-new-math sit.** Rank 1 matches the ask (“point with controllers”). Rank 2 ships a usable panel if ray-vs-plane is not grepped in one night. Rank 3 does not replace either.

Confirm is **trigger or poke**, not face-button A. Tick **sim-owner / first-eye only** (`lvframe60` / `#32` once-gate). `joyGetButtonsPressedThisFrame` is **not** cleared on read — both eyes see the same edge.

### 2.3 Do not wire these first

| Path | Why |
|------|-----|
| N64 A / `watch_play_beep_sound` | #32: beeps / double-toggles / Yes-No never stays. |
| Pause watch row | Too late (after cinema or after gameplay). #32 + #57 unreadability. |
| Head-locked laser HUD | Violates “cinema-billboard family, not head-glued.” |
| Gun **fire** as select in a mission | Panel must be **gone** in gameplay. |
| New OpenXR action set | `gameplay` already has aim + trigger + turn. Do not attach a second set for one quad. |

Public `synthesizePad` maps move→N64 stick, trigger→Z, use→B, pause→START. It does **not** map turn onto the pad (C-buttons stay zero). Workshop `port_input.c` is what applies `GETV_XR_TURN*` to yaw. Panel select must not OR `CONT_A` twice per frame.

### 2.4 Hover vs cinema

The cinema is a big quad in the **same** room. A right-side panel that is too far forward will steal the aim ray meant for “I’m looking at the intro.” Sit: hover the **options** quad only when the ray hits **that** plane; looking at the cinema must not tick rows. First-eye once-gate on enter/exit, not per-eye chatter (same `#32` / TOUCHUSE lesson).

---

## 3. Pref persist — `GETV_XR_TURN_SCALE` (default 60)

### 3.1 Live reader (workshop `port_input.c`)

Public tree has **no** `getv/port/src/port_input.c` body. What exists:

- vr440 snippet / patch (`getv/port/src/port_input_vr440.snippet.c`) — `getenv("GETV_XR_BTN_B")` + `geXrParseAct` / `geXrPadAct`. Proves the TU and the getenv shape.
- KEEP harness / `port_render.c` — typical latch:

```c
static int v = -1;
if (v < 0) {
    const char *e = getenv("GETV_XR_TURN_SCALE");
    v = (e != NULL && *e != '\0') ? atoi(e) : 60; /* ship / boot default */
}
```

First chair grep (workshop):

```
getenv("GETV_XR_TURN_SCALE")
getenv("GETV_XR_TURN")
getenv("GETV_XR_TURN_DEAD")
getenv("GETV_XR_TURN_INVERT")
getenv("GETV_XR_TURN_HAND")
```

Do **not** type `geXrTurnScale()` until the live symbol matches. Sibling names from boot + ads-grip DIG: `GETV_XR_TURN_HAND` / `_INVERT` are **wiped** on ship (C_DEFAULT UNKNOWN after empty).

Architecture (`GE007-VR-ARCHITECTURE.md` §6.2): stick yaw is **additive** to head yaw (`stick_yaw + head_yaw`). `TURN_SCALE` scales that stick term. Head look is not this knob (`TURN_DEAD` is stick deadzone, not head — vision-jitter DIG).

### 3.2 Boot seed (the persist path that exists)

`packaging/templates/gevr-vr441-boot.cmd` and `gevr-vr442-boot.cmd` (headset via `Start-GEVR.bat`):

| Knob | vr441 / vr442 | Class | Meaning |
|------|---------------|-------|---------|
| `GETV_XR_TURN` | `1` | KEEP_SHIP | Right-stick yaw **on**. Sit: “Right stick turns.” |
| `GETV_XR_TURN_SCALE` | `60` | PLAYER_PREF | Scale. **Default 60.** `do_not_touch` in KEEP `MANIFEST.json`. |
| `GETV_XR_TURN_DEAD` | `20` | PLAYER_PREF | Stick deadzone. |
| `GETV_XR_TURN_INVERT` | wipe (`set …=`) | C_DEFAULT UNKNOWN | Not seeded. Menu may expose later; **same getenv**, no new name. |
| `GETV_XR_TURN_HAND` | wipe | C_DEFAULT UNKNOWN | Right stick stays turn. |

`Start-GEVR.bat` is only `call gevr-vr*-boot.cmd` then `GevrRomStarter.exe`. **No** player sidecar. **No** `%LOCALAPPDATA%` ini. **No** `vr.json`.

U-04 (`docs/169`): “SETTINGS FILE + IN-GAME VR MENU” — PD `pd-vr.ini` + main-menu sliders. Blocker already recorded: flags are **`static`, latched on first call**. Docs `61` amendment 2 designed a hot-reloaded `vr.json` (**env > file > default**) and then **did not build it** (no consumer yet). `TURN_SCALE` is that first consumer. **Do not revive `GE_VR_SNAP_TURN` / a second yaw.** U-03 snap-vs-smooth is a **later registry row** on this panel, still feeding the existing turn path.

KEEP graduation (`GRADUATED-KNOBS.md`): turn scale/dead stay **PLAYER_PREF**, not C-default-ON. Smoke `$requiredBootKnobs` does **not** include `TURN_SCALE` (39-knob picture allowlist). A player sidecar can override 60 without a pack-smoke edit. **Do not** put the sidecar assign into the ship boot as KEEP.

### 3.3 What the menu is allowed to write

**This session (before stage inject):** write the **same cached int** `port_input.c` already uses for `TURN_SCALE`. If it is latched (`x < 0` once), Phase 3 **must** add a setter / unlatch. `_putenv("GETV_XR_TURN_SCALE=90")` after first read is a **no-op** on that shape (U-04).

**Next boot:** persist a value the **boot reads**. Smallest:

```
gevr-player-prefs.cmd          (or .ini the bat converts)
  set GETV_XR_TURN_SCALE=45    player won

gevr-vr442-boot.cmd            ship still sets =60
  call gevr-player-prefs.cmd   AFTER ship assigns → player wins
```

Precedence (docs `61`, still the right order):

```
scratch / diagnostic env   (chair bat, always wins)
  > player sidecar         (menu persist)
    > ship boot 60         (PLAYER_PREF seed)
      > C default 60       (unset / empty)
```

Log the source once (`[getv][xrin] TURN_SCALE=45 (prefs)` vs `(boot)` vs `(default)`). Silent precedence is how leftover env misdiagnoses a sit.

**Forbidden:** `GETV_XR_TURNSPEED`, `GE_VR_SNAP_TURN`, a second integrator, rewriting `GETV_XR_TURN=0`, stealing `TURN_HAND`, flipping `TURN_INVERT` as the scale row, KEEP-ON of a new name, `$requiredBootKnobs` until a sit PASS.

Later rows (snap vs smooth, invert, height) **register** on this panel and still write **existing** knobs (`TURN`, `TURN_INVERT`, floor / #45 — not this DIG). Do not pre-build them.

### 3.4 APPLY sketches — **NOT LANDED** (map only; Phase 2+)

Workshop only. Public `goldeneye-native` has no play path to patch.

```c
/* NOT APPLY READY — workshop sketch.
 * Panel: same TU as PLAY_SCREEN cinema draw. Hub world, +X from cinema.
 * Gate: cinemaOrHubThisFrame() only. First-eye / sim-owner.
 * Scale: setter on the existing TURN_SCALE cache; persist sidecar last.
 */

/* Phase 2: empty world-locked quad + option registry (no rows yet). */
/* Phase 3: one row — TURN SPEED — reads/writes GETV_XR_TURN_SCALE. */
/* Phase 4: docs for adding option #2. */
```

**APPLY READY?** **No.** Hub pose + right offset + ray/poke + latch + persist sidecar need a sit. This PR does not start Phase 2.

---

## 4. Must not (this DIG and later APPLY)

| Forbidden | Why |
|-----------|-----|
| Block or rewrite **#74** Bond playspace / **HT=0** | vr442 `HEAD_TRANSLATE=0`, `PLAYSPACE=1`, `GUNREBASE` default OFF. Panel is hub UI. Do not restore `HT=1`, do not touch gun rebase. |
| Head-lock the panel | Ask is cinema-billboard family. Watch zoom / face HUD is #57, not #76. |
| Require a mission to open | Visible on intro / front room. Gate **out** of gameplay, not in. |
| Ship **UNLOCKALL** or ROM content | BYO-ROM. No cart in the zip. No cheat unlock. |
| Implement Phases 2–4 in this PR | Dig only. |
| Face-A as first confirm | #32. |
| `GETV_XR_FOVMATCH`, `GETV_STAGE`, `FRONTTRACE` / `CINETRACE` on a pack boot | Smoke-forbidden. |
| Mint a second turn system | Drive `GETV_XR_TURN_SCALE` (or persist the boot already reads). |

---

## 5. Chair stare (plain tester sentences)

**Setup:** vr442 / Latest zip. `Start-GEVR.bat`. Recenter both sticks. **No** Phase 2–4 C on this tree — these sentences are for the **first APPLY sit**, not tonight.

**Hub host (after Phase 2 empty panel, default OFF unless a chair knob is greened):**

| | Tester sentence |
|--|-----------------|
| **H-PASS 1** | “Intro / file-select: cinema is still a **screen in a little room**. I look left; it stays put.” |
| **H-PASS 2** | “On my **right** I see a second floating panel. It stays nailed when I turn my head. It is **not** glued to my face.” |
| **H-PASS 3** | “I start a mission: the panel is **gone**. I recenter into the world. #74 HT=0 / guns feel like Latest.” |
| **H-FAIL** | “Panel on my nose.” / “No panel until I load Dam.” / “Cinema covered.” / “Fancy hub I did not ask for.” / “HT=1 / arms sheared like #74.” |

**Point / select (Phase 2+):**

| | Tester sentence |
|--|-----------------|
| **P-PASS 1** | “I **point** a controller at a row; it highlights. Looking at the cinema does **not** tick the panel.” |
| **P-PASS 2** | “Trigger (or poke ±) changes the row **once**. It does not flicker both eyes.” |
| **P-FAIL** | “A beeps twice / undoes itself.” / “Rows fire in Facility.” / “Right stick stopped turning in the hub and never came back.” |

**Turn scale (Phase 3) — existing knob only:**

| | Tester sentence |
|--|-----------------|
| **T-PASS 1** | “Unset menu: right stick still feels like **60** (ship).” |
| **T-PASS 2** | “I raise / lower **turn speed** on the panel. The **next** stick yaw is faster / slower **this process**, before I start a mission.” |
| **T-PASS 3** | “I quit and `Start-GEVR.bat` again: the value **survived** (sidecar / prefs). Scratch `GETV_XR_TURN_SCALE=30` still wins over the file.” |
| **T-FAIL** | “Menu wrote a new env name and stick ignore it.” / “Value died on relaunch.” / “`TURN=0` / invert flipped / #74 arms moved.” |

**Regression (every run):** both-stick recenter, squeeze ADS on the **gun ray**, touch-use doors, no `FOVMATCH`, cinema 85 still a screen, HT=0 / PLAYSPACE untouched.

---

## 6. Director decision

| If you say… | Then… |
|-------------|--------|
| **Green Phase 2** | Workshop: empty world-locked right quad + option registry. Default **OFF** or always-on-in-hub (name it). No turn row yet. |
| **Green persist sidecar first** | Allowed: `call` player prefs after boot `TURN_SCALE=60` with **zero** panel C. Still not KEEP. |
| **Green Phase 3 with 2** | Turn-speed row writes **existing** `TURN_SCALE` cache + sidecar. Unlatch if static. |
| **Head-lock / watch host** | Rejected. That is #57 / #32. |
| **In-mission options** | Rejected for v1. Session toggle later, after hub PASS. |
| **Ship panel ON in boot** | **No** until H/P/T sits PASS. Then still PLAYER_PREF, not KEEP-ON. |
| **New turn getenv** | Rejected. |
| **Touch #74 / HT / PLAYSPACE / GUNREBASE** | Rejected. |
| **Reject** | Stop. Tonight’s cinema + `TURN_SCALE=60` env stay. |

---

## 7. Attribution / legal

- GEVR [#76](https://github.com/no6969el/GEVR/issues/76) is the public ask. Related [#42](https://github.com/no6969el/GEVR/issues/42) cinema vs VR boot (DIG: `getv/patches/cinema-boot-picker-dig/RESULT.md`). Do not file `#76` here.
- Do not block [#74](https://github.com/no6969el/GEVR/issues/74) Bond playspace / HT=0.
- No personal credit paths edited.
- No GoldenEye ROM, assets, dumps, or UNLOCKALL.
- PD hub / `pd-vr.ini` numbers are **map-only**. Do not copy PD sources.
- Workshop C stays private until release policy flips.
