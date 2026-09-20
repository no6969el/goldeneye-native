# APPLY — GETV_VR_GUNREBASE (GEVR #74)

**Tracker:** [GEVR #74](https://github.com/no6969el/GEVR/issues/74) — canonical. DIG: [PR #32](https://github.com/no6969el/goldeneye-native/pull/32) `getv/patches/room-strafe-arms-dig/RESULT.md`.
**Status:** APPLY landed on the public host ABI. **Default OFF.** Not KEEP-ON. Chair-only until PASS.
**Ask:** Physical room sidestep away from the recentered spot no longer skews VR arms/guns.

```
GETV_VR_GUNREBASE    unset / empty / 0 = OFF
                     1 = head-relative grip + aim + left cube, then camera
```

Banner once when ON: `[getv][gunrebase] GETV_VR_GUNREBASE=1 arms`

---

## What landed (this tree)

`geStereoXrGunMount` / `HandWorld` / `stereo.c` are **not on this remote**. Rank 1 is the public host path:

| Path | Change |
|------|--------|
| `src/ge_vr_bridge.cpp` | getenv + same recenter yaw+XZ as `recenteredHead()` on grip **and** aim, then head-relative XZ (keep Y) |
| `include/ge_vr/ge_vr.h` | Documents the knob on `geVrGetWeaponModelMatrixF` / `geVrGetAimRay` |
| `getv/port/src/port_render.c` | Reference getenv stub, default **0** (not KEEP-ON) |

One yaw number (`recenter_yaw`). Both hands. Same pose both eyes (no `current_eye` term). Displacement compares two recentered poses when ON.

Unset / `0` = tonight (raw stage grip/aim). HT=0 modem KEEP is untouched.

---

## Workshop follow-up (not in this remote)

Playable GUNMOUNT / left cube may still bake through workshop `geStereoXrGunMount` / `geStereoXrHandWorld` and **miss** this ABI. After this PR, on SimRig:

1. First grep: `geStereoXrGunMount`, `geStereoXrHandWorld`, `getenv("GETV_VR_GUNREBASE")`.
2. Same TU as GUNMOUNT (`vendor/ge-decomp/src/game/stereo.c` or live name).
3. Tick per sim / first eye. Same pose both eyes.
4. Reuse this env (do not invent `GUNHEADREL` as a second ship name).
5. If PLAY0 locks fists and **aim** walks away, the draw path ate the rebase and the ray did not — still Rank 1, second site.

Do **not** push workshop `geStereoXr*` bodies.

---

## Must not

- Flip `HEADYAW` / `AUTORECENTER` / `GUNARM` / `PLAYSPACE`
- Restore `HEAD_TRANSLATE=1` (breaks HT=0 modem KEEP / #70)
- Enable `GUNZ` / `HANDSOLID`
- Call `geVrRecenter()` every frame
- Add `GUNREBASE=1` to ship boot / `$requiredBootKnobs` until sit PASS

---

## Chair (after boot)

vr442 / Latest. `Start-GEVR.bat`. Recenter both sticks. Dam or Facility. Right hand holds a gun; left empty (cube). **No stick** on the sidestep. Do not set `GUNEYE` / `GUNZ` / `HANDSOLID` / `BODY=1` / `HEADYAW=0` / `HEAD_TRANSLATE=1`.

After PLAY0 / KEEP have exported (scratch line, **not** the ship allowlist):

```bat
set GETV_VR_GUNREBASE=1
```

Snippet: `chair-gunrebase.cmd.snippet`.

| | Tester sentence |
|--|-----------------|
| **P0-PASS** | “Recenter, guns out, physical sidestep 0.5–1 m, no stick. **Guns stay locked to the controllers.** Aim still on the gun ray. I do not want a recenter.” |
| **P0-FAIL** | “Still skewed.” / “Gun welded to my face.” / “Aim moved off the barrel.” / “`HEADYAW` died.” / “Gun vanished below my chest.” |

Console once: `[getv][gunrebase] GETV_VR_GUNREBASE=1 arms`. Unset restores raw stage (B0 = #74 wears).
