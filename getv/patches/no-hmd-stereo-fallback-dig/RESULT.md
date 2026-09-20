# RESULT — no-HMD stereo fallback ([GEVR #36](https://github.com/no6969el/GEVR/issues/36)) (DIG ONLY)

**Status:** DIG. **Not APPLY READY on this public tree.** No C landed.
**Ask:** without a working HMD session and without `Play-on-monitor.bat`, there is no good in-exe stereo fallback. Expected today: the monitor bat (flat, no VR stereo KEEP). A proper no-HMD stereo fallback inside `goldeneye.exe` is parked.
**Constraints:** Wear stays vr441/vr442-class KEEP. `GETV_STEREO_MODE=2` must never ship. Do not fold into [#42](https://github.com/no6969el/GEVR/issues/42) cinema picker (that needs a live HMD). Do not “fix” monitor-purple explosions ([#51](https://github.com/no6969el/GEVR/issues/51)) by turning KEEP boot on for the flat bat.
**Date:** 2026-09-20.
**Evidence:** public `goldeneye-native` HEAD (`src/xr_session.*`, `ge_vr.h`, KEEP graduation), public `no6969el/GEVR` packaging + textbook (`Play-on-monitor.bat`, vr441 boot, `stereo.c` knobs in RUN-SHEET-292, `geVrXrEnabled` notes). Workshop bodies (`gevr_xr.c`, product `stereo.c` SRC=xr, `port_render.c` play loop) are **not on any public remote**.

Director can green-light keep-bat, auto-flat-on-XR-fail, SBS fallback, or packaging pins from this page alone.

---

## Verdict (one screen)

| Question | Answer |
|----------|--------|
| What #36 is | **Start-GEVR / KEEP stereo with no live HMD session.** Not “monitor 3D glasses.” Not cinema ([#42](https://github.com/no6969el/GEVR/issues/42)). |
| What works today | **`Play-on-monitor.bat`.** FLAT_FORCE: `GETV_STEREO=0`, `GETV_STEREO_MODE=0`, `GE_VR_XR=0`. Sit-gated. Smoke-gated. |
| Historical “no-HMD stereo” | **`GETV_STEREO=1` `MODE=1` side-by-side** in workshop `stereo.c`. Measurement gate (`258` STAGE 6 / RUN-SHEET-292). Default was **OFF**. Offset `500` was absurd on purpose. **Not a product.** |
| Ship stereo | **`GETV_STEREO=1` + `GETV_STEREO_SRC=xr`.** Eyes from OpenXR `PRIMARY_STEREO`. Needs a session. |
| Smallest *safe* in-exe path | **Auto-flat on XR init fail** — same one-eye path as the bat. **Not stereo.** Hang/crash preventer only. |
| SBS as silent fallback | **Reject for ship.** Split framebuffer is a lab picture. Dual-wield / split-screen / `getPlayerCount()` traps. `MODE=2` is a falsifier and must never ship. |
| APPLY tonight? | **No.** Keep the bat. In-exe work is workshop `gevr_xr.c` / `stereo.c` SRC, not this repo. |

```
Start-GEVR.bat + HMD     → KEEP stereo, SRC=xr, session FOCUSED     (ship)
Start-GEVR.bat, no HMD   → SRC=xr with no views  → broken / refuse / hang
Play-on-monitor.bat      → GETV_STEREO=0         → flat 2D            (workaround)
bare goldeneye.exe       → skip ROM starter      → unsupported
parked in-exe stereo     → MODE=1 SBS if no session                  (not ship)
smallest in-exe later    → if xrGetSystem/init fail, force stereo off (auto-flat)
```

---

## 1. Root finding (files + functions)

### 1.1 Evidence boundary

| Layer | Where | What it proves |
|--------|--------|----------------|
| Public `goldeneye-native` | this repo | Host ABI: `Session::init` **returns false** and the caller **must run flat**. Bridge `geVrInit` is still TODO(phase4) inactive. KEEP fragments default `GETV_STEREO_SRC` to `xr`. |
| Public `no6969el/GEVR` | packaging + textbook | Two bats, FLAT_FORCE vs KEEP_SHIP, `geVrXrEnabled` = `GETV_VR`, `GE_VR_XR` **no-op**. Historical SBS knobs. |
| Workshop (private) | `F:\Projects\GEVR\GoldenEyeVR\goldeneye-native` | Live `gevr_xr.c`, product `vendor/ge-decomp/src/game/stereo.c` (`GETV_STEREO_SRC`), `port_render.c` play/interlock. **Do not push.** |

`geVrXrEnabled` / `geVrXrInit` / `geStereoOffsetCameraPos` **do not appear in any public C file**. First chair grep: `getenv("GETV_VR")`, `getenv("GETV_STEREO")`, `getenv("GETV_STEREO_SRC")`.

### 1.2 Three pictures that share the word “stereo”

| Picture | Knobs | Needs HMD? | Player sees |
|---------|--------|------------|-------------|
| **A. Flat** | `GETV_STEREO=0` `MODE=0` | No | One eye, full window. Keyboard/pad. Local split-screen. |
| **B. Lab SBS** | `GETV_STEREO=1` `MODE=1` `OFFSET>0`, SRC **not** xr | No | Two viewports in **one** framebuffer (`stereo.c:233`). Gate, not ship. |
| **C. Headset** | `GETV_STEREO=1` `SRC=xr` + `GETV_XR_PLAY=1` | **Yes** | Per-eye swapchains from `xrLocateViews`. |

#36 asks for a fallback when **C** cannot start and the player did not pick **A**. Parked “proper no-HMD stereo” is **B**. Ship docs and sit gates only promise **A** (`CONTROLS.md`: “Monitor / no headset: `Play-on-monitor.bat` — VR off, no stereo eyes”).

`MODE=2` is projection-only B1 (`stereo.c:284-295`, `geStereoOffsetCameraPos` returns 0). RUN-SHEET-292: **falsifier, MUST NEVER SHIP ON.**

### 1.3 Launch matrix (vr441 boot + KEEP graduation)

**Headset boot** (`Start-GEVR.bat` → `gevr-vr441-boot.cmd`):

| Knob | Value | Class |
|------|-------|--------|
| `GETV_STEREO` | `1` | KEEP_SHIP (**not** C-default-ON in `MANIFEST.json`) |
| `GETV_STEREO_SRC` | `xr` | KEEP_SHIP **and** C-default `xr` |
| `GETV_STEREO_REBUILD` / `_HUDGATE` / `_VIEWRESTORE` / `_AIMRECT` / `_GUNOFS` | `1` | KEEP_SHIP (rebuild/hudgate also C-default ON) |
| `GETV_XR_PLAY` / `_STEREO` | `1` | KEEP_SHIP (`PLAY` C-default ON) |
| `GETV_VR` | `1` | KEEP_SHIP (`geVrXrEnabled`; C-default ON after graduation) |
| `GETV_FPS` | `90` | PLAYER_PREF (interlock — [#2](https://github.com/no6969el/GEVR/issues/2)) |
| `GE_VR_XR` | `1` | **No-op.** Real arm is `GETV_VR`. |

**FLAT_FORCE** (`Play-on-monitor.bat` only):

| Knob | Value | Notes |
|------|-------|--------|
| `GETV_STEREO` | `0` | No stereo eyes |
| `GETV_STEREO_MODE` | `0` | Mode off |
| `GE_VR_XR` | `0` | Smoke still requires this; **no-op in binary** |
| `GETV_FPS` | `60` | Monitor cadence |
| `GETV_AUDIO_CLOCK` / `_QUEUE_MS` | `device` / `33` | [#48](https://github.com/no6969el/GEVR/issues/48) |

The monitor bat does **not** assign `GETV_VR=0` or `GETV_XR_PLAY=0`. Smoke (`_smoke-ship-zip.ps1`) still documents `geVrXrEnabled`: **unset means off** — that was true **before** KEEP graduation. After graduation, unset `GETV_VR` / `GETV_XR_PLAY` means **ON**. Whether the shipped vr442 exe already flipped those C defaults is **workshop-unknown here**. If yes, the bat’s “No OpenXR” comment depends on `GETV_STEREO=0` actually skipping session create. If no, unset `GETV_VR` still means off and the bat is enough.

`GETV_STEREO` itself was **default 0** when the eye loop landed (RUN-SHEET-292). KEEP graduation did **not** flip it. Bare `goldeneye.exe` (unsupported) can therefore have `SRC=xr` + `GETV_VR=1` with **stereo still off** unless boot ran.

### 1.4 What “no working HMD session” actually does

Public ABI (`src/xr_session.h`):

> Returns false if no runtime is present or the headset is unavailable. The caller must fall back to the flat-screen port rather than aborting — an unplugged headset should not be a crash.

Public `src/xr_session.cpp`: `xrCreateInstance` fail prints `running flat` and returns false. `xrGetSystem` uses `XR_CHECK` (any fail → return false).

Textbook measurements (not this binary):

| Symptom | Doc / issue | Meaning for #36 |
|---------|-------------|-----------------|
| `xrGetSystem failed (-35)` | `docs/29`, `docs/32` | Headset asleep / unplugged. Expected. |
| Interlock `HANDOVER REFUSED` | [#2](https://github.com/no6969el/GEVR/issues/2) | Session can exist; play loop never enters. Pin `GETV_FPS`. |
| `xrWaitFrame` blocks to period | `docs/322` / `330` | Live session paces even with `shouldRender=0`. Desk can stay `SYNCHRONIZED` never `VISIBLE`. |
| `-NoXr` one-eye PASS | `docs/73` | Flat path is proven **when XR is off on purpose**. |

Product call site (workshop, named in GEVR textbook): `geVrXrEnabled` → `geVrXrInit` → `geVrXrBeginSessionCurrent`. Any of the three refuse and **`geVrXrFrameKill` is never reached** (`PLAN-330-CHAIR`). That is a **print**, not a documented latch that clears `GETV_STEREO` / `SRC=xr`.

So today: KEEP stereo stays **armed** while the session is missing. There is no in-exe switch to picture **A** or **B**. The only supported switch is **launch a different bat**.

### 1.5 Why SBS is not a small drop-in

Workshop `stereo.c` (RUN-SHEET-292 / 311):

- Eye loop nested in the **per-player** body. Never `getPlayerCount()=2` (`258` §1.5 — dual-wield / glass-buffer trap).
- `SRC=xr` (ship C-default) feeds OpenXR views, not `GETV_STEREO_OFFSET`.
- Falling back to `MODE=1` means: detect no session, ignore `SRC=xr`, pick an OFFSET, present **halves of one window**.
- Human IPD was **never** the ship default (offset `500` was the instrument). Tuning ~6 cm is a sit, not a one-liner.
- Local split-screen **is** the monitor product (`FEATURES.md`). SBS stereo on the same framebuffer is a second N=2.

Public `ge_vr.h`: getters are safe when VR is inactive (neutral values, original path). That is **flat**, not SBS.

---

## 2. Paths compared

| Path | Size | What the player gets | Risk |
|------|------|----------------------|------|
| **0. Keep bat-only** | Zero C. Docs already say it. | Flat 2D if they pick the right bat. | #36 stays OPEN as parked-by-design. Wrong bat + no HMD still bad. |
| **1. Auto-flat on XR fail** | Small workshop: on `geVrXrInit` / `xrGetSystem` fail, force the GETV_STEREO=0 / skip-SRC=xr path already used when XR is off. | Same as monitor bat, without finding the bat. | Headset asleep → silent 2D (looks like “VR died”). Must not clear KEEP picture knobs (TEXINVAL etc.) or you inherit [#51](https://github.com/no6969el/GEVR/issues/51) purple on a path that thought it was VR. Interlock-refuse with a **live** session is a different fail (do not flatten a focused HMD). |
| **2. Auto SBS (`MODE=1`)** | Not small. SRC latch + OFFSET + present + split-screen coexistence. | Two squeezed eyes on the desktop. | Lab picture shipped as comfort. `MODE=2` footgun. MP/split-screen. |
| **3. Pin `GETV_VR=0` `GETV_XR_PLAY=0` on the monitor bat** | Packaging only (`no6969el/GEVR`). | Makes FLAT_FORCE match C-default-ON. | Does **not** help Start-GEVR-without-HMD. Smoke must keep `GETV_STEREO=0`. |

**Recommend 0 now.** **1** is the only in-exe candidate worth a later APPLY. **2** stays parked. **3** is a separate GEVR-packaging dig if chair proves graduation made the monitor bat start XR.

Do not use [#42](https://github.com/no6969el/GEVR/issues/42) (cinema vs full VR **in the HMD**) as this fallback. Cinema already needs `GETV_XR_PLAY` + a session (`PLAY_SCREEN=2` in boot).

---

## 3. APPLY sketches — **NOT LANDED**

### 3.1 Keep bat-only (recommend)

No product C. Player door already:

- README / CONTROLS / COMING-SOON: headset = `Start-GEVR.bat`, no headset = `Play-on-monitor.bat`.
- Do not double-click `goldeneye.exe`.
- Ship checklist sit: monitor bat “Boots with no HMD session. No one-eye / OpenXR requirement.”

Optional doc-only: on #36, state **parked / workaround is the bat**. Not a code close.

### 3.2 Auto-flat (smallest in-exe, later)

Workshop: next to `geVrXrInit` fail / `geVrXrEnabled` refuse (same TU as `gevr_xr.c`, or the stereo SRC reader in `stereo.c`).

```c
/* NOT APPLY READY — workshop sketch.
 * If OpenXR cannot give PRIMARY_STEREO views, do not keep SRC=xr armed.
 * Match Play-on-monitor: one eye, no SBS.
 * Do NOT set MODE=2. Do NOT invent OFFSET. Do NOT wipe TEXINVAL KEEP. */

static int ge_stereo_xr_live(void)
{
    /* 0 if geVrXrInit failed, no system id, or session never READY.
     * Interlock refuse with session FOCUSED is NOT this — leave stereo on
     * and let play print HANDOVER REFUSED (issue #2), do not flatten. */
    return 0;
}

/* stereo SRC reader (today unset = "xr"):
 * if (src is xr && !ge_stereo_xr_live()) treat as stereo off.
 */
```

Public-tree **shape only** (already written, not wired to GETV):

```97:101:src/xr_session.cpp
    if (XR_FAILED(xrCreateInstance(&ici, &impl_->instance))) {
        std::fprintf(stderr, "[ge-xr] no OpenXR runtime available; running flat.\n");
        return false;
    }
```

**APPLY READY?** On the workshop, this is a latch + one SRC/STEREO gate. **Not APPLY READY here** — those functions are private. Do not land a stub. Sit before KEEP-ON.

### 3.3 SBS fallback — parked

```
GETV_STEREO=1 MODE=1 OFFSET=<human IPD> SRC=offset-or-empty
```

Only if director wants **monitor 3D** as a product. Default **OFF**. Never `MODE=2`. Never `getPlayerCount()=2`. Scratch boot only; not vr441 allowlist until a sit PASS.

**APPLY READY?** **No.**

### 3.4 Out of scope

- `#42` in-HMD cinema picker
- `#51` purple explosions on the monitor bat (KEEP tex not in FLAT_FORCE; C-default-ON of TEXINVAL may already close it — separate sit)
- `#48` audio (already seeded on the bat)
- Bare-exe ROM-starter skip
- `GETV_STEREO` C-default-ON (would make nobat launches two-eye without a session — worse)
- Personal credit paths, ROM dumps

---

## 4. Chair stare (plain tester sentences)

**Setup:** vr442-class zip. Two bats. One machine **without** a working OpenXR HMD (runtime missing, or headset unplugged/asleep). Recenter N/A on flat.

**Run B — bat (today’s PASS bar):** `Play-on-monitor.bat`

| | Tester sentence |
|--|-----------------|
| **B-PASS 1** | “The game boots on the monitor. I do not wait on a headset.” |
| **B-PASS 2** | “The picture is **one** full view, not two skinny eyes side by side.” |
| **B-PASS 3** | “Keyboard / pad work. I can start local split-screen.” |
| **B-FAIL** | “Black forever.” / “Two halves.” / “It hangs like it is waiting for SteamVR.” |

**Run S — Start-GEVR, no HMD (the bug):** `Start-GEVR.bat` with headset unplugged.

| | Tester sentence |
|--|-----------------|
| **S-FAIL (tonight)** | “It does not become a good monitor game. Stereo KEEP is still on and there is no session.” (expected until APPLY) |
| **S-PASS if 3.2 lands** | “Same picture as Play-on-monitor. One eye. No hang.” |
| **S-FAIL if 3.2 lands badly** | “Headset was only asleep and the game went 2D with no way back.” / “Explosions went purple.” / “Focused HMD flattened because the interlock refused.” |

**Run H — headset regression:** `Start-GEVR.bat` **with** HMD. Recenter both sticks.

| | Tester sentence |
|--|-----------------|
| **H-PASS** | “VR stereo is tonight. Fallback code did not steal the session.” |
| **H-FAIL** | “I have a headset on and I am looking at a flat or SBS window.” |

Do not score SBS (`MODE=1`) unless director greens path 2.

---

## 5. Director decision

| If you say… | Then… |
|-------------|--------|
| **Keep bat-only** | Stop. Leave #36 parked. Workaround is `Play-on-monitor.bat`. Optional comment on the issue. |
| **Green auto-flat** | Workshop APPLY 3.2. Sit B, S, H. Do not add SBS. Do not KEEP-ON a new knob until S-PASS. |
| **Green SBS fallback** | Rejected by this dig unless you want monitor 3D as a named product. Then 3.3, default OFF, sit separately. |
| **Pin GETV_VR=0 on the monitor bat** | GEVR packaging PR, not this repo. Only if Run B starts XR after C-default-ON. |
| **Want cinema picker** | That is [#42](https://github.com/no6969el/GEVR/issues/42). Needs an HMD. |

---

## 6. Attribution / legal

- No personal credit paths edited.
- No GoldenEye ROM, assets, or dumps.
- Workshop C stays private until release policy flips.
- SBS / OFFSET numbers are **map-only** from public GEVR textbook (RUN-SHEET-292 / 311).
