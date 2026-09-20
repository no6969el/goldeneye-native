# SUGGESTED-ROWS — live getenv only (GEVR #76)

Owner want: **not** a full options dump. Three families, player-picked, **existing vars only**. Do not invent knobs. Do not register any of these in Phase 2. Wire a row only after Owner green-lights it.

Sources: vr442 `packaging/templates/gevr-vr442-boot.cmd`, KEEP `MANIFEST.json` / `GRADUATED-KNOBS.md`, Phase 1 DIG `getv/patches/vr-options-panel-dig/RESULT.md` (PR #37).

---

## 1. Turn speed — Phase 3 first row

| Knob | vr442 | Class | Use |
|------|-------|-------|-----|
| **`GETV_XR_TURN_SCALE`** | `60` | PLAYER_PREF (`MANIFEST` `do_not_touch`) | **The row.** Slider. Drive the existing `port_input.c` cache (unlatch / setter if static). |
| `GETV_XR_TURN` | `1` | KEEP_SHIP | Right-stick yaw **armed**. Do not flip to 0 as the scale control. |
| `GETV_XR_TURN_DEAD` | `20` | PLAYER_PREF | Stick deadzone. Sibling, not the first row. |

Wiped on boot (not a second scale): `GETV_XR_TURN_INVERT`, `GETV_XR_TURN_HAND`.

**Do not mint** `GETV_XR_TURNSPEED` / a second integrator. Persist: player sidecar `call`ed **after** boot `=60` (U-04 latch).

---

## 2. Smooth vs snap turn — later registry enum

| Knob | vr442 | Notes |
|------|-------|-------|
| *(none seeded)* | — | Boot does **not** assign a snap/smooth getenv. |

Phase 1 DIG: U-03 snap-vs-smooth is a **later row on this panel**, still feeding the **existing** `GETV_XR_TURN` path. **Do not revive `GE_VR_SNAP_TURN`.** Architecture §9 “snap default / smooth opt-in” is a plan sentence, not a live name.

First chair grep (workshop `port_input.c`) before any enum lands:

```
getenv("GETV_XR_TURN")
getenv("GETV_XR_TURN_SCALE")
```

If a live snap symbol exists, **use that name**. If it does not, **stop** — do not invent `GETV_XR_SNAP` / `GETV_XR_TURNSNAP`.

---

## 3. Height adjustment — later row, #45 family only

| Knob | vr442 | Class | Use |
|------|-------|-------|-----|
| **`GETV_XR_FLOOR_M`** | `-0.200` | KEEP | Floor / reset-spot offset. This is the height number if Owner greens a slider. |
| `GETV_XR_FLOOR_INJECT` | `0` | DIG_OFF (`MANIFEST` `do_not_touch`) | #45 height **path**. Off on ship. Do not KEEP-ON from the menu. |

Related recenter (not height sliders): `GETV_XR_RECENTER_YAWONLY=1`, `GETV_XR_RECENTER_CHORD=1`. `geVrRecenter()` stores yaw + XZ and **forces Y=0** — height stays `#45` / `FLOOR_M`.

**Do not** expose as height:

| Knob | Why |
|------|-----|
| `GETV_XR_HEAD_TRANSLATE` | #74 / #70 / #55. vr442 **0**. Do not restore. |
| `GETV_VR_PLAYSPACE` | Look-around comfort, not floor height. |

Do not mint `GETV_XR_HEIGHT` / `GETV_VR_HEIGHT`.

---

## Not this panel

Do **not** copy a reference-video option list (Hand / 6DoF / Haptic / Button Sensitivity). Look/feel only.

## Phase 2

Empty glass + `GeVrOptKind` {slider, toggle, enum}. **Zero rows registered.** Rank 1: aim-ray **laser** (`geVrGetAimRay`) + **trigger**. Rank 2: TOUCHUSE ± poke highlight only. Face A is the #32 trap.
