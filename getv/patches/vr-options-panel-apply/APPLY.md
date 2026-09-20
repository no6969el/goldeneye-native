# APPLY — empty VR options panel + registry (GEVR #76 Phase 2)

**Tracker:** [GEVR #76](https://github.com/no6969el/GEVR/issues/76) — canonical. DIG: [PR #37](https://github.com/no6969el/goldeneye-native/pull/37) `getv/patches/vr-options-panel-dig/RESULT.md`.
**Status:** Phase 2 APPLY on the public host ABI. **Default OFF.** Not KEEP-ON. Chair-only until PASS.
**Ask:** Empty **world-locked** options quad on the **RIGHT** of the intro / front main-menu hub (cinema-billboard family) + a registry that can host sliders / toggles / enums. Glass chrome + aim-ray **laser** + trigger select. No turn-speed row yet.

```
GETV_VR_OPT_PANEL    unset / empty / 0 = OFF
                     1 = draw the panel while the cinema / front hub is up
```

Banner once when ON: `[getv][optpanel] GETV_VR_OPT_PANEL=1 hub`

Not a full options dump. Owner-greenlit families (later rows only): turn speed, smooth vs snap, height. Live names: `SUGGESTED-ROWS.md`. **Phase 3** is the first row (`GETV_XR_TURN_SCALE`). Do not invent knobs.

---

## What landed (this tree)

Cinema / `PLAY_SCREEN` draw is **not on this remote**. Rank 1 is the public host path workshop can call:

| Path | Change |
|------|--------|
| `include/ge_vr/ge_vr_opt.h` | Registry + panel ABI (C89) |
| `src/ge_vr_opt.c` | Empty world-locked right quad, aim-ray / poke, first-eye tick |
| `include/ge_vr/ge_vr.h` | Pointer only — does not touch gun / HT / playspace getters |
| `getv/port/src/port_render.c` | Reference getenv stub, default **0** |
| `tests/test_opt_panel.cpp` | Registry kinds, right-of-cinema pose, cinema miss, hub gate |

Registry starts at **zero rows**. Types exist so Phase 3 is `geVrOptRegister` of the existing `TURN_SCALE` cache — not a second turn system.

**Interaction (Owner lock):** Rank 1 = `geVrGetAimRay` laser from the hand onto the glass; **trigger** commits. Face **A** is the #32 trap. TOUCHUSE ± is fallback highlight only. Workshop draws `geVrOptPanelGetLaser` while the panel is up.

**Chrome (look/feel, not a row dump):** dark translucent glass, bright white sans, glowing border; header **VR SETTINGS** + close; rows later = name+value LEFT / chevron RIGHT; hover = glowing row outline; enum dropdown = smaller sibling to the **RIGHT** of the main list (never overlaps). Do **not** copy Hand / 6DoF / Haptic / Button Sensitivity. Footer RESET is later.

---

## Workshop follow-up (not in this remote)

Playable cinema is the workshop TU next to `getenv("GETV_XR_PLAY_SCREEN")`. After this PR, on SimRig:

1. First grep: `GETV_XR_PLAY_SCREEN`, `GETV_XR_PLAY_AUTOSCREEN`, `GETV_VR_OPT_PANEL`, `gePortSimShouldTick`.
2. Same TU as the cinema billboard. **Do not** parent to HMD / watch / `GETV_VR_HUB`.
3. `geVrOptPanelSetHubActive(1)` on frontend / intro / file-select; **`0` when gameplay stereo eyes exist**.
4. `geVrOptPanelSetCinemaFrame` from the live `PLAY_SCREEN=2` quad, then `geVrOptPanelTick(first_eye)` and `geVrOptPanelGetQuad` + draw.
5. Tick per sim / first eye only. Draw `GetLaser` beams (origin → hit) while hub is up. Trigger commit. Face **A** is the trap. TOUCHUSE poke is fallback only.

Snippet: `opt_panel_cinema.snippet.c`. Do **not** push workshop cinema bodies.

---

## Must not

- Block or rewrite **#74** Bond playspace / **HT=0** (`HEAD_TRANSLATE`, `PLAYSPACE`, `GUNREBASE`)
- Head-lock the panel (watch / face HUD is #57)
- Require a mission to open — gate **out** of gameplay
- Ship **UNLOCKALL** or ROM content
- Register `GETV_XR_TURN_SCALE` (Phase 3) or mint `GETV_XR_SNAP` / `GE_VR_SNAP_TURN`
- Face-A confirm; steal the right stick for a slider while the panel is up
- Add `OPT_PANEL=1` to ship boot / `$requiredBootKnobs` / KEEP `bool_on_unset` until sit PASS

---

## Chair (after boot)

vr442 / Latest. `Start-GEVR.bat`. Recenter both sticks. **Stay on intro / file-select.** Do not set `FOVMATCH`. Do not restore `HEAD_TRANSLATE=1`. Do not flip `GUNREBASE` / `PLAYSPACE` for this sit.

After PLAY0 / KEEP have exported (scratch line, **not** the ship allowlist):

```bat
set GETV_VR_OPT_PANEL=1
```

Snippet: `chair-opt-panel.cmd.snippet`.

Workshop draw must be wired or the chair only sees the banner (public tree has no cinema blit).

| | Tester sentence |
|--|-----------------|
| **H-PASS 1** | “Intro / file-select: cinema is still a **screen in a little room**. I look left; it stays put.” |
| **H-PASS 2** | “On my **right** I see a second floating panel. It stays nailed when I turn my head. It is **not** glued to my face.” |
| **H-PASS 3** | “I start a mission: the panel is **gone**. I recenter into the world. #74 HT=0 / guns feel like Latest.” |
| **P-PASS 1** | “A **laser** from my hand hits the right-side glass. Looking at the cinema does **not** tick it.” |
| **P-PASS 2** | “**Trigger** selects. Face A is not the confirm. No double-eye flicker.” |
| **H-FAIL** | “Panel on my nose.” / “No panel until I load Dam.” / “Cinema covered.” / “Fancy hub I did not ask for.” / “HT=1 / arms sheared like #74.” |

Console once: `[getv][optpanel] GETV_VR_OPT_PANEL=1 hub`. Unset hides the panel.

---

## Files in this folder

| File | Role |
|------|------|
| `APPLY.md` | This page |
| `SUGGESTED-ROWS.md` | Live getenv map for later rows (no invented names) |
| `opt_panel_cinema.snippet.c` | Hand-apply next to `PLAY_SCREEN` draw |
| `chair-opt-panel.cmd.snippet` | Scratch `OPT_PANEL=1` |
