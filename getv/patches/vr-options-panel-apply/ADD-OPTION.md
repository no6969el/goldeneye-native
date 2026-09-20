# ADD-OPTION — register row #N (GEVR #76 Phase 4)

How to add another option to the cinema-hub **RIGHT** panel without minting a second system.

Ship rows today (do not reorder without a sit): `turn_scale`, `turn_mode`, `floor_m`. See `SUGGESTED-ROWS.md` for live getenv names.

---

## 1. Only existing knobs

The row must write a **live** workshop cache (or a sidecar the boot already reads). First chair grep the getenv. If the symbol is not there, **stop** — do not invent `GETV_XR_*`.

```
getenv("GETV_XR_…")     workshop reader
geVr…Get / geVr…Set     public unlatch cache (U-04)
```

Latch trap: `static int v = -1; if (v < 0) getenv` — `_putenv` after first read is a no-op. Phase 3 pattern: one `Get` / `Set` pair. Menu `set_f` / `set_i` call `Set`.

## 2. Register once

Call from `geVrOptEnsureShipRows()` (or a later `geVrOptEnsure*` the Tick already runs). Idempotent: skip if `geVrOptId` already matches.

```c
GeVrOptDesc d;
memset(&d, 0, sizeof(d));
d.id = "deadzone";              /* stable, not GETV_XR_TURN_DEAD */
d.label = "TURN DEADZONE";      /* HUD, white sans */
d.kind = GE_VR_OPT_SLIDER;      /* or TOGGLE / ENUM */
d.slider_min = 5.0f;
d.slider_max = 40.0f;
d.slider_step = 5.0f;
d.get_f = ship_dead_get;        /* reads existing cache */
d.set_f = ship_dead_set;        /* writes existing cache + optional sidecar */
geVrOptRegister(&d);
```

Enum: `d.enum_labels` / `d.enum_count`. Dropdown is already a sibling to the **RIGHT** of the main glass (`geVrOptPanelOpenDropdown`). Do not draw it on top of the list.

## 3. Chrome / interaction (do not restyle)

- Dark glass, white sans, hover glow outline (`geVrOptPanelGetChrome`)
- Row: name+value **LEFT**, chevron **RIGHT**
- Rank 1: `geVrGetAimRay` laser + **trigger**. Face **A** is the #32 trap
- Draw `geVrOptPanelGetLaser` while the panel is up
- Hub-only (`geVrOptPanelSetHubActive`). Gone in a mission
- World-locked RIGHT of `PLAY_SCREEN=2`. Not head-glued

## 4. Persist (optional)

If the boot already seeds the getenv, write the same name into `gevr-player-prefs.cmd` from `Set`. Boot `call`s that file **after** the ship assign. Scratch env still wins.

Do **not** add the row’s getenv to `$requiredBootKnobs` / KEEP `bool_on_unset` until a sit PASS.

## 5. Must not

- Touch `HEAD_TRANSLATE` / `PLAYSPACE` / `GUNREBASE` (#74)
- Mint a second turn / height system
- Flip `GETV_XR_TURN=0` or KEEP-ON `FLOOR_INJECT`
- Copy Hand / 6DoF / Haptic / Button Sensitivity
- Ship **UNLOCKALL** / ROM
- Face-A confirm; steal the right stick while the panel is up

## 6. Test

`tests/test_opt_panel.cpp`: register, nudge, getenv latch then setter, no invented ids. `ge_vr_opt_panel_off` / `_on`.
