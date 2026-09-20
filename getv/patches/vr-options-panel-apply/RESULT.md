# RESULT — intro glass blit + hub gate (GEVR #76)

**Tracker:** [GEVR #76](https://github.com/no6969el/GEVR/issues/76). Dig: [PR #37](https://github.com/no6969el/goldeneye-native/pull/37). Shell: [PR #38](https://github.com/no6969el/goldeneye-native/pull/38). Rows: [PR #39](https://github.com/no6969el/goldeneye-native/pull/39).
**Status:** Public host ABI + workshop-owed blit notes. **`GETV_VR_OPT_PANEL` default OFF.** Not KEEP-ON. Not ship boot.
**Ask:** Chair cannot see the intro VR options panel. Workshop already **Ticks** `ge_vr_opt` but draws no glass. Hub gate may ignore **`PLAY_SCREEN=2`** (ship cinema).
**Date:** 2026-09-20.
**Evidence:** public `goldeneye-native` only. No workshop `gevr_xr` / cinema TU on this remote.

---

## Verdict

| Question | Answer |
|----------|--------|
| Why the chair sees no panel | Two gaps: (1) hub gate that drops `PLAY_SCREEN=2`; (2) no world-locked glass blit next to the cinema quad. Tick alone does not present pixels. |
| Gate | `geVrOptPanelHubShouldBeActive` / `ApplyHubGate`: intro / frontend / file-select / **SCREEN=2** = hub. Gameplay stereo eyes = off. `SetHubActive(2)` is cinema, not “not hub.” |
| Glass | `geVrOptPanelGetGlassLayer` — dark translucent quad, **RIGHT** of cinema, world-locked. Laser + trigger unchanged. |
| Cinema blit on this remote? | **No.** Workshop `gevr_xr` hub / `XrCompositionLayerQuad` / `PLAY_SCREEN` present is private. Snippet: `opt_panel_cinema.snippet.c`. |
| Chair | Scratch `GETV_VR_OPT_PANEL=1` + **BODY_TRANSLATE** stack. Do **not** bake `OPT_PANEL` into ship boot. |
| #74 / HT / PLAYSPACE / GUNREBASE | **Untouched.** No `bondview2`. |

```
GETV_VR_OPT_PANEL=1          chair scratch (default OFF)
GETV_XR_PLAY_SCREEN=2        ship cinema — MUST count as hub
GETV_XR_BODY_TRANSLATE=1     chair stack only (grep live workshop name)
```

---

## 1. What this tree changed

| Path | Change |
|------|--------|
| `include/ge_vr/ge_vr_opt.h` | `HubShouldBeActive` / `ApplyHubGate` / `PlayScreenEnv` / `GetGlassLayer` |
| `src/ge_vr_opt.c` | SCREEN=2 is cinema hub; glass layer descriptor |
| `tests/test_opt_panel.cpp` | SCREEN=2 gate + glass RIGHT + gameplay-eyes off |
| `opt_panel_cinema.snippet.c` | Workshop blit: ApplyHubGate + gevr_xr hub/quad layer |
| `chair-opt-panel.cmd.snippet` | `OPT_PANEL=1` + BODY_TRANSLATE stack |

`SetHubActive(0)` still means mission / off. `PLAY_SCREEN=2` is KEEP_SHIP even in a mission — **do not** treat getenv `=2` alone as always-on. Pass `gameplay_stereo_eyes=1` when the cinema→VR gate drops.

---

## 2. Workshop-owed blit (not on this remote)

Playable cinema is the TU next to `getenv("GETV_XR_PLAY_SCREEN")` / `gevr_xr` hub quad. After this PR, on SimRig:

1. First grep: `GETV_XR_PLAY_SCREEN`, `GETV_XR_PLAY_AUTOSCREEN`, `GETV_VR_OPT_PANEL`, `geVrOptPanelTick`, `geVrOptPanelSetHubActive`, `XrCompositionLayerQuad`, hub quad submit.
2. Same TU as the cinema billboard. **Do not** parent to HMD / watch / `GETV_VR_HUB`.
3. Replace `SetHubActive(play_screen == 1)` / “ignore 2” with:

```c
geVrOptPanelApplyHubGate(frontend_or_intro, play_screen, gameplay_stereo_eyes);
```

   `play_screen` is the live `GETV_XR_PLAY_SCREEN` int (**2** on vr442). `gameplay_stereo_eyes` is the same boolean that tears down the cinema.

4. `geVrOptPanelSetCinemaFrame` from the live `PLAY_SCREEN=2` quad, then `Tick(first_eye)`.
5. `geVrOptPanelGetGlassLayer` → submit a **world-locked** dark glass quad in the **same present family** as the cinema (gevr_xr hub / `XrCompositionLayerQuad`). Wearer-**RIGHT**. Hover glow. Header **VR SETTINGS**. Rows LEFT name+value / RIGHT chevron. Dropdown sibling RIGHT.
6. Draw `GetLaser` beams (origin → hit) while hub is up. **Trigger** commit. Face **A** is the trap.
7. Tick / blit per sim / first eye only. Second eye reuses the pose.

Do **not** push workshop `gevr_xr` / cinema bodies. Do **not** edit `bondview2`. Do **not** flip `HEAD_TRANSLATE` / `PLAYSPACE` / `GUNREBASE`.

Until that blit lands, the chair only sees the banner / cache — Tick without a quad layer is invisible.

---

## 3. Chair (after boot)

vr442 / Latest. `Start-GEVR.bat`. Recenter both sticks. **Stay on intro / file-select.**

```bat
set GETV_VR_OPT_PANEL=1
set GETV_XR_BODY_TRANSLATE=1
```

Grep workshop for the live BODY_TRANSLATE name if `GETV_XR_BODY_TRANSLATE` is unread. Do **not** add either assign to `gevr-*-boot.cmd` / `$requiredBootKnobs`. Do **not** restore `HEAD_TRANSLATE=1`.

| | Tester sentence |
|--|-----------------|
| **H-PASS 1** | “Intro: cinema is still a **screen in a little room**.” |
| **H-PASS 2** | “On my **right** I see dark glass (**VR SETTINGS**). Head turn does not glue it to my face.” |
| **H-PASS 3** | “I start a mission: the panel is **gone**. HT=0 / guns feel like Latest.” |
| **P-PASS** | “**Laser** hits the glass. **Trigger** selects. Face A is not the confirm.” |
| **H-FAIL** | “Banner only / no glass.” / “Panel on my nose.” / “No panel until Dam.” / “HT=1 / arms sheared.” |

Console once: `[getv][optpanel] GETV_VR_OPT_PANEL=1 hub`.

---

## Must not

- Touch `bondview2` / `HEAD_TRANSLATE` / `PLAYSPACE` / `GUNREBASE`
- Bake `OPT_PANEL=1` into ship boot
- Head-lock the glass
- Treat `PLAY_SCREEN=2` as “not cinema”
- Face-A confirm
