# APPLY — VR options panel rows (GEVR #76 Phase 3–4)

**Tracker:** [GEVR #76](https://github.com/no6969el/GEVR/issues/76) — canonical. DIG: [PR #37](https://github.com/no6969el/goldeneye-native/pull/37) `getv/patches/vr-options-panel-dig/RESULT.md`. Shell: [PR #38](https://github.com/no6969el/goldeneye-native/pull/38) (not rewritten).
**Status:** Phase 3–4 APPLY on the public host ABI. **`GETV_VR_OPT_PANEL` default OFF.** Not KEEP-ON. Chair-only until PASS.
**Ask:** Cinema-hub **RIGHT** glass (Phase 2) plus the three Owner-wanted rows, persist sidecar, and ADD-OPTION. Laser + trigger. No second yaw. No #74 touch.

```
GETV_VR_OPT_PANEL    unset / empty / 0 = OFF
                     1 = draw the panel while the cinema / front hub is up
GETV_XR_TURN_SCALE   default 60; menu writes the same cache port_input.c must read
GETV_XR_TURN         KEEP 1 = right-stick yaw armed; menu does not flip to 0
GETV_XR_FLOOR_M      default -0.200; height number. FLOOR_INJECT stays 0
```

Banner once when panel ON: `[getv][optpanel] GETV_VR_OPT_PANEL=1 hub`
Banner once on first scale read: `[getv][xrin] TURN_SCALE=60 (default|boot|prefs)`

---

## What landed (this tree)

Cinema / `PLAY_SCREEN` draw is **not on this remote**. Rank 1 is the public host path workshop can call:

| Path | Change |
|------|--------|
| `include/ge_vr/ge_vr_opt.h` | Registry + panel ABI + TURN_SCALE / TURN mode / FLOOR_M caches |
| `src/ge_vr_opt.c` | Phase 2 chrome + 3 ship rows + setter/unlatch + sidecar write |
| `include/ge_vr/ge_vr.h` | Pointer only — does not touch gun / HT / playspace getters |
| `getv/port/src/port_render.c` | Reference getenv stubs (panel OFF; scale 60; floor −0.200) |
| `tests/test_opt_panel.cpp` | Ship rows, latch/setter, sidecar, laser+trigger nudge |

Phase 2 host / chrome / interaction **unchanged:** world-locked RIGHT of `PLAY_SCREEN=2`, aim-ray laser (`geVrGetAimRay`) + **trigger**, visible `geVrOptPanelGetLaser` while up, dark glass / white sans / hover outline / dropdown sibling to the RIGHT.

### Ship rows

| Row | Kind | Cache | Default |
|-----|------|-------|---------|
| **TURN SPEED** | slider 10–150 step 10 | `geVrTurnScaleGet/Set` → `GETV_XR_TURN_SCALE` | 60 |
| **TURN STYLE** | enum SMOOTH / SNAP | `geVrTurnModeGet/Set` on the existing `GETV_XR_TURN` integrator | SMOOTH |
| **HEIGHT** | slider −0.50..0.30 step 0.05 | `geVrFloorMGet/Set` → `GETV_XR_FLOOR_M` | −0.200 |

Latch (U-04): first `Get` reads getenv (or C default). **Setter** writes the same static so a later getenv / `_putenv` is not required. Workshop `port_input.c` must call `geVrTurnScaleGet()` instead of its own `static … = -1`. Snippet: `port_input_turn.snippet.c`.

TURN STYLE does **not** mint `GETV_XR_SNAP` / `GE_VR_SNAP_TURN` and does **not** set `GETV_XR_TURN=0`. Snap is a mode on the same yaw path. HEIGHT does **not** flip `FLOOR_INJECT` (DIG_OFF / #45 path).

### Persist sidecar (optional, small)

Menu `Set` writes `gevr-player-prefs.cmd` (or `GETV_VR_OPT_PREFS` path):

```
set GETV_XR_TURN_SCALE=…
set GETV_XR_FLOOR_M=…
set GETV_XR_TURN=1
```

Boot still assigns `GETV_XR_TURN_SCALE=60`. **`call` the sidecar AFTER that line** so the player wins. Scratch env still wins over the file. Snippet: `gevr-player-prefs.cmd.snippet`. Do **not** add the call to ship `gevr-*-boot.cmd` / `$requiredBootKnobs` until sit PASS.

---

## Workshop follow-up (not in this remote)

Playable cinema is the workshop TU next to `getenv("GETV_XR_PLAY_SCREEN")`. After this PR, on SimRig:

1. First grep: `GETV_XR_PLAY_SCREEN`, `GETV_XR_PLAY_AUTOSCREEN`, `GETV_VR_OPT_PANEL`, `getenv("GETV_XR_TURN_SCALE")`, `getenv("GETV_XR_TURN")`, `getenv("GETV_XR_FLOOR_M")`, `gePortSimShouldTick`.
2. Same TU as the cinema billboard. **Do not** parent to HMD / watch / `GETV_VR_HUB`.
3. `geVrOptPanelSetHubActive(1)` on frontend / intro / file-select; **`0` when gameplay stereo eyes exist**.
4. `geVrOptPanelSetCinemaFrame` from the live `PLAY_SCREEN=2` quad, then `geVrOptPanelTick(first_eye)` and `geVrOptPanelGetQuad` + draw rows (name+value LEFT / chevron RIGHT; hover glow).
5. Tick per sim / first eye only. Draw `GetLaser` beams (origin → hit) while hub is up. Trigger commit. Face **A** is the trap. TOUCHUSE poke is fallback only.
6. In `port_input.c`, replace the `TURN_SCALE` static latch with `geVrTurnScaleGet()`. Apply snap vs smooth via `geVrTurnModeGet()` on the **same** `GETV_XR_TURN` yaw (no second integrator). Floor reader: `geVrFloorMGet()`.

Snippet: `opt_panel_cinema.snippet.c`. Do **not** push workshop cinema bodies.

---

## Must not

- Block or rewrite **#74** Bond playspace / **HT=0** (`HEAD_TRANSLATE`, `PLAYSPACE`, `GUNREBASE`)
- Head-lock the panel (watch / face HUD is #57)
- Require a mission to open — gate **out** of gameplay
- Ship **UNLOCKALL** or ROM content
- Mint `GETV_XR_SNAP` / `GE_VR_SNAP_TURN` / `GETV_XR_TURNSPEED` / `GETV_XR_HEIGHT`
- Flip `GETV_XR_TURN=0` as the scale or style control
- KEEP-ON `FLOOR_INJECT` from the menu
- Face-A confirm; steal the right stick for a slider while the panel is up
- Add `OPT_PANEL=1` to ship boot / `$requiredBootKnobs` / KEEP `bool_on_unset` until sit PASS

---

## Chair (after boot)

vr442 / Latest. `Start-GEVR.bat`. Recenter both sticks. **Stay on intro / file-select.** Do not set `FOVMATCH`. Do not restore `HEAD_TRANSLATE=1`. Do not flip `GUNREBASE` / `PLAYSPACE` for this sit.

After PLAY0 / KEEP have exported (scratch line, **not** the ship allowlist):

```bat
set GETV_VR_OPT_PANEL=1
```

Optional persist check: `call gevr-player-prefs.cmd` **after** boot `TURN_SCALE=60`.

Snippet: `chair-opt-panel.cmd.snippet`.

Workshop draw + `port_input.c` getter swap must be wired or the chair only sees the banner / cache (public tree has no cinema blit / yaw apply).

| | Tester sentence |
|--|-----------------|
| **H-PASS 1** | “Intro / file-select: cinema is still a **screen in a little room**. I look left; it stays put.” |
| **H-PASS 2** | “On my **right** I see a second floating panel. It stays nailed when I turn my head. It is **not** glued to my face.” |
| **H-PASS 3** | “I start a mission: the panel is **gone**. I recenter into the world. #74 HT=0 / guns feel like Latest.” |
| **P-PASS 1** | “A **laser** from my hand hits the right-side glass. Looking at the cinema does **not** tick it.” |
| **P-PASS 2** | “**Trigger** selects. Face A is not the confirm. No double-eye flicker.” |
| **T-PASS 1** | “Unset menu: right stick still feels like **60** (ship).” |
| **T-PASS 2** | “I raise / lower **TURN SPEED**. The **next** stick yaw is faster / slower **this process**, before I start a mission.” |
| **T-PASS 3** | “I quit and `Start-GEVR.bat` again after `call`ing the sidecar: scale **survived**. Scratch `GETV_XR_TURN_SCALE=30` still wins.” |
| **S-PASS** | “TURN STYLE SNAP / SMOOTH still uses the **same** right-stick yaw. `TURN` is still on.” |
| **F-PASS** | “HEIGHT writes `FLOOR_M` only. Arms / HT=0 feel like Latest.” |
| **H-FAIL** | “Panel on my nose.” / “No panel until I load Dam.” / “Cinema covered.” / “Fancy hub I did not ask for.” / “HT=1 / arms sheared like #74.” |
| **T-FAIL** | “Menu wrote a new env name and stick ignore it.” / “`TURN=0` / invert flipped / #74 arms moved.” |

Console once: `[getv][optpanel] GETV_VR_OPT_PANEL=1 hub`. Unset hides the panel.

---

## Files in this folder

| File | Role |
|------|------|
| `APPLY.md` | This page |
| `SUGGESTED-ROWS.md` | Live getenv map (now the three ship rows) |
| `ADD-OPTION.md` | Phase 4 — how to register row #N |
| `opt_panel_cinema.snippet.c` | Hand-apply next to `PLAY_SCREEN` draw |
| `port_input_turn.snippet.c` | Replace workshop `TURN_SCALE` latch |
| `gevr-player-prefs.cmd.snippet` | Sidecar `call`ed after boot `=60` |
| `chair-opt-panel.cmd.snippet` | Scratch `OPT_PANEL=1` |
