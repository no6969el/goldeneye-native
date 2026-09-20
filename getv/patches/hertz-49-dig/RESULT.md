# RESULT — #49 headset Hertz: request-Hz EXE + bat soft-fail (DIG ONLY)

**Status:** DIG. **Not APPLY READY on this public tree.** No C landed.
**Ask:** GEVR [#49](https://github.com/no6969el/GEVR/issues/49) — Quest 3 + Virtual Desktop and PSVR2 + SteamVR OpenXR **only enter VR at 90 Hz**. 72 / 80 instant-close to the desktop window. At 90 the intro races, then the level judders. Director: **public Hertz must land in the next EXE, not a bat-only pin.** Chair 72 / 80 / 90 later.
**Constraints:** Do not bake `90` as a C default. Do not unset `GETV_FPS` and call it fixed (that is [#2](https://github.com/no6969el/GEVR/issues/2)). `GETV_SIMDIV=1` and `GETV_SIMHZ=query` stay. Over-90 remains unsigned.
**Date:** 2026-09-20.
**Evidence:** public `goldeneye-native` HEAD, public `no6969el/GEVR` textbook + vr441/vr442 packaging, issues #2 and #49. Workshop C (`geVrXrPaceArm`, `gevr_xr.c`, `gfx_sdl2.c` `GETV_FPS` parse, `frametiming.c` `GETV_SIMHZ=query`) is **not on any public remote** (`GEVR` `docs/RELEASE-POLICY.md`). Brief names are the workshop symbols.

Director harvest (issue #49, 2026-09-19): `DIRECTOR-TIP-PUBLIC-HERTZ-NEXT-EXE-20260919.md`, `PASTE-OPUS-49-HERTZ-QUERY-EXE-20260919.md`. Those files are not on a public remote. This page is the DIG they asked for.

Director can green-light request-Hz, bat-only, both, or neither from this page alone.

---

## Verdict (one screen)

| Question | Answer |
|----------|--------|
| Why only 90 launches? | **Hardcode pin vs interlock.** Boot sets `GETV_FPS=90`. `geVrXrPaceArm` refuses handover unless `predictedDisplayPeriod` is 90 ± 0.500%. 72 / 80 miss the band → **no window** → desktop. |
| Is that a 72/80 render bug? | **No.** Those rates never get a session. The fail is attach. |
| HmdPace vs pin? | vr441 boot: *“90 pinned, not `-HmdPace`.”* Pin was the [#2](https://github.com/no6969el/GEVR/issues/2) fix (unset defaulted 60 vs a 144 Hz arming frame). **Request / query never shipped public.** |
| Bat-only unset / pin 72? | **Reject.** Unset is #2 again. Pin 72 breaks 80 / 90. Bake-90-in-EXE copies tonight’s fail. |
| What lands Hertz? | **EXE after a live session:** read runtime Hz, write `ge_pace_framerate` from it, stay in VR. Optional `xrRequestDisplayRefreshRateFB` is a **soft** prefer. Banner `xr hz=N fps=N simhz=query`. |
| Bat’s job? | **Soft-fail request.** Keep `GETV_FPS=90` as *prefer 90*, or spell `hmd`. EXE owns the actual. Never refuse attach on mismatch. |
| Chair tonight? | **No.** Chair Quest 3 + VD at 72 / 80 / 90 (/ 120) **after** the EXE. Until that PASS, public copy stays “90 works; #49 open.” |
| APPLY tonight? | **No from this repo.** Bodies are private. Sketch is APPLY-shaped **on the workshop**. |

```
headset 72  + GETV_FPS=90  →  13.889 vs 11.111 ms  →  REFUSE  →  desktop
headset 80  + GETV_FPS=90  →  12.500 vs 11.111 ms  →  REFUSE  →  desktop
headset 90  + GETV_FPS=90  →  AGREE                 →  VR     →  #49 play-feel is a later sit
headset  *  + GETV_FPS unset → default 60 vs arming period (often desktop 144) → REFUSE (#2)
headset  *  + EXE query/request + pace=actual        →  AGREE by construction → stay in VR
```

---

## 1. Root finding (files + functions)

### 1.1 Evidence boundary

| Layer | Where | What it proves |
|-------|--------|----------------|
| Public `goldeneye-native` | this repo | Scaffold `src/xr_session.cpp` first-frame delta is `1/90`. No `XR_FB_display_refresh_rate`. **Not** the playable GETV tree. |
| Public `no6969el/GEVR` | docs + `packaging/` | Boot pin, interlock banners, #2 / #49, 72/80 “should work” copy that the zip cannot honour. |
| Workshop (private) | `F:\Projects\GEVR\GoldenEyeVR\goldeneye-native` | `geVrXrPaceInterlock` / `PaceArm` / `PacingOwned`, `GETV_FPS` parse, `gePortSimHzResolve`. **Do not push.** |
| Issues | GEVR #2 (closed vr434), #49 (open) | #2 named the refuse string. #49 is the same latch at non-90. |

`geVrXrPaceArm` / `ge_pace_framerate` / `gePortSimHzResolve` **do not appear as C in any public file**. First chair grep: `HANDOVER REFUSED` and `getenv("GETV_FPS")`.

### 1.2 The hardcode path (what ships tonight)

**Public boot** (`GEVR` `packaging/templates/gevr-vr441-boot.cmd`; `Start-GEVR.bat` just calls it). vr442 zip still uses this file.

```
rem 2. Core VR + pacing (425w standing keepers; 90 pinned, not -HmdPace)
set GETV_FPS=90
set GETV_SIMDIV=1
set GETV_SIMHZ=query
```

`GETV_FPS` is **PLAYER_PREF**, not KEEP (`KEEP-DEFAULTS-INVENTORY-vr441.md`). Smoke (`_smoke-ship-zip.ps1`) still asserts `"GETV_FPS" = "90"`.

**Why the pin exists:** [#2](https://github.com/no6969el/GEVR/issues/2) vr420 bat set **no** `GETV_FPS`. Reporter (Quest 3, PC OpenXR):

```
[getv][play] armed from 6.9444 ms (144.0000 Hz). Interlock line follows once:
[getv][play] HANDOVER REFUSED BY THE INTERLOCK. No window. Pin GETV_FPS=90.
```

6.9444 ms is **144 Hz** — not a Quest 3 mode (72 / 80 / 90 / 120). The **arming frame advertised a desktop panel period**. Latch is process-final (`I-1c`, `docs/325`). Adding `GETV_FPS=90` *and* setting the headset to 90 cleared it. vr434 shipped the pin. That made Crystal / 90 work and **made every other HMD rate a refuse**.

**`-HmdPace`:** named only in that boot comment. No public C, no public bat switch. Chair 425w had a query/request path and **chose the pin instead** so the interlock would AGREE on the verified 90 Hz wear. Public copy then claimed “72 / 80 / 90 should work” (`README`, vr442 notes) while the zip cannot attach off 90.

### 1.3 The interlock (what actually kills 72 / 80)

**Workshop, measured in public textbook `docs/325`:**

| Symbol | Job |
|--------|-----|
| `geVrXrPaceInterlock()` | Pure: runtime Hz from `predictedDisplayPeriod` vs `ge_pace_framerate`. Band **0.500%**. |
| `geVrXrPaceArm()` | Latch once. Disagreement → refuse handover for the **process**. |
| `geVrXrPacingOwned()` | Caller-side gate (FORK 2). Session can be live while pacing is **not** owned (`I-1d`). |
| `ge_pace_framerate` | Written by `GETV_FPS` (`gfx_sdl2.c`). Numeric, `panel` (SDL), `0`/`off`. |
| `gePortSimHzResolve()` | `GETV_SIMHZ=query` copies `ge_pace_framerate` **once**, then caches (`frametiming.c`). |

Refuse banner (`325` live `I-1b`, 90 vs 60):

```
CONTAMINATED -- xrWaitFrame paces at 90.0001 Hz ... ge_pace_framerate claims 60 Hz.
HANDOVER REFUSED: ... Pin GETV_FPS=90 to agree, or accept the desktop pacer. Band is 0.500%.
```

#2’s play-path string is the same latch: **No window.**

| HMD Hz | Period | vs pin 90 (11.111 ms) | Band 0.500% | Tonight |
|--------|--------|------------------------|-------------|---------|
| 72 | 13.889 ms | 25% | miss | refuse → desktop |
| 80 | 12.500 ms | 12.5% | miss | refuse → desktop |
| 90 | 11.111 ms | 0% | hit | VR |
| 120 | 8.333 ms | 25% | miss | refuse (unsigned anyway) |
| unset → 60 vs 144 arming | — | miss | #2 |

**Baking 90 in the EXE** (C-default when env unset) is the same table. Director: *“Baking 90 in-exe would copy the same fail.”*

### 1.4 Request vs hardcode (OpenXR)

**Public scaffold** (`src/xr_session.cpp`): `xrCreateInstance` enables **only** the graphics extension. No `XR_FB_display_refresh_rate`. First `predicted_delta_s` is `1.0f / 90.0f` when there is no previous display time. Probe (`GEVR` `xr/xr_probe.cpp`) **enumerates** FB rates after `xrCreateSession` and never **requests**.

**Workshop play path (names only):**

1. **Hardcode (tonight).** Bat/EXE sets `ge_pace_framerate = 90`. Interlock demands the runtime already be 90. Non-90 → refuse.
2. **Request (FB ext, soft).** After session create, `xrEnumerateDisplayRefreshRatesFB` / `xrRequestDisplayRefreshRateFB(prefer)` / `xrGetDisplayRefreshRateFB`. Any `XR_ERROR_*` or missing ext → **keep current rate, stay in VR**.
3. **Query (always).** After the session is **VISIBLE / `shouldRender`**, read `xrGetDisplayRefreshRateFB` **or** `1e9 / predictedDisplayPeriod`. Write `ge_pace_framerate` from **that**. Interlock AGREES by construction.

**Do not arm on the first `xrWaitFrame`.** #2’s 144 Hz period is the trap. `docs/325` §2: period can be a perfect 11.111 ms on desk **and** worn; the contaminated number is the **desktop** period on an early arm, not the HMD. Director “after `xrCreateSession`” means **after a live HMD period**, not “first callback.”

`GETV_FPS=panel` is the **monitor** analog (`ge_panel_hz_query`, banner `GETV_FPS=panel -> N Hz, QUERIED FROM SDL`). SDL-miss is `exit(1)`. **HMD must not `exit(1)`.** Soft-fail = stay in VR at the rate the runtime actually has.

`GETV_SIMHZ=query` already chains off `ge_pace_framerate`. Once the EXE writes the real Hz, simhz follows **if resolve has not cached yet**. Arm **before** the first game frame, or the 60/90 cache (`325` / `274`) is back.

### 1.5 What #49 also reported — and what this DIG does not fix

| Report | Mechanism | This DIG? |
|--------|-----------|-----------|
| 72 / 80 instant close / desktop | Interlock refuse (§1.3) | **Yes — EXE query/request** |
| Only `Start-GEVR.bat` enters VR | Correct bat; RomStarter without boot knobs is #2-shaped | Bat still required; EXE must not depend on the 90 pin to attach |
| Intro “incredibly fast” at 90 | `speedgraph` / RB-04: 90/60 = **1.50×** (`docs/257`, `274`) | **No.** Rate-agnostic timestep is a later sit |
| Level judder at 90 | 2:3 cadence / interpolation leftovers (`docs/92` superseded by `134`/`257`; `SIMDIV=auto` at 90 is divider 3, already pinned `1`) | **No.** Chair after attach works |
| Late / thin NPC gunfire | `port_audio` buffer; `268` §7 present at 60 too | **No** |
| Guns “not 3D” | Stereo / viewmodel, not Hertz | **No** |

Public notes already say 72 / 80 should work and 90 is recommended. The zip cannot attach off 90. Until a chair PASS, keep “90 works; #49 open.”

---

## 2. How the two layers sit together

Not “bat vs EXE.” **EXE owns Hertz. Bat is a prefer.**

| `GETV_FPS` | FB request | Pace from | Interlock | Attach |
|------------|------------|-----------|-----------|--------|
| `90` (tonight) | none | 90 always | runtime must be 90 | 72/80 die |
| `90` (APPLY) | request 90 if listed | **actual** after live read | agrees | 72/80 live at 72/80 |
| `hmd` / `query` / `xr` / unset-after-EXE-default | none | actual | agrees | all listed rates |
| unset on **today’s** EXE | n/a | default 60 | vs arming period | #2 refuse |
| C-default 90 | n/a | 90 | same as tonight | **reject** |

Prefer-90 then soft-fail is the director path: Crystal / chair 90 still **asks** 90; Quest 3 at 72 **stays at 72** and stays in VR.

---

## 3. APPLY sketches — **NOT LANDED**

### 3.1 EXE — request-Hz + pace from actual (the feature)

**Files (workshop):** `gevr_xr.c` / `geVrXrPaceArm` call site (play handover), `gfx_sdl2.c` `GETV_FPS` parse, `frametiming.c` only if `gePortSimHzResolve` can cache too early.

**Enable `XR_FB_display_refresh_rate` on the play instance when advertised** (probe already knows the name). Graphics ext stays required; FB is optional.

```c
/* NOT APPLY READY — workshop sketch.
 * After xrCreateSession, once session is VISIBLE / shouldRender
 * (NOT the first arming WaitFrame — #2 144 Hz desktop period).
 * Never exit(1). Never bake 90. */

static int ge_fps_prefer(void)
{
    /* unset / hmd / query / xr / panel-on-XR-play → -1 (no request)
     * numeric N → request N if listed
     * panel on monitor path unchanged (SDL; may still refuse) */
}

/* 1. If FB ext:
 *      enumerate rates
 *      if prefer > 0 and listed: xrRequestDisplayRefreshRateFB(prefer)
 *         fail → log, keep current (soft-fail)
 *      xrGetDisplayRefreshRateFB → actual
 * 2. Else / Get failed:
 *      actual = 1e9 / predictedDisplayPeriod   (live HMD frame)
 * 3. ge_pace_framerate = actual   (nearest 1 Hz or nearest listed)
 * 4. geVrXrPaceArm(period, ge_pace_framerate)  -- AGREE
 * 5. Resolve SIMHZ=query NOW if it has not cached
 * 6. Banner once:
 *      [getv][play] xr hz=N fps=N simhz=query
 */
```

**Why this is the smallest change that honours the director tip:** one write of `ge_pace_framerate` from a **live** runtime number. Interlock stays (it still guards a real 60-vs-90 lie). It stops being a 90-only door.

**Do not:**

- C-default `GETV_FPS` to 90
- `exit(1)` on FB miss / request miss / band miss after we just wrote actual
- Arm from the first period
- Touch `GETV_SIMDIV` (stays 1) or `MoveBond` / audio

**APPLY READY?** On the **workshop**, yes-shaped: parse + one arm site + banner. **Not APPLY READY here** — those functions are not in public `goldeneye-native`. Do not land a stub.

### 3.2 Bat — soft-fail prefer (not a pin)

**File:** `GEVR` `packaging/templates/gevr-vr441-boot.cmd` (and the next zip’s boot). `Start-GEVR.bat` stays a caller.

```bat
rem Soft-fail: 90 is a REQUEST. EXE paces at the live HMD rate.
rem Do not refuse attach if the runtime stays at 72/80.
set GETV_FPS=90
set GETV_SIMDIV=1
set GETV_SIMHZ=query
```

Comment change is load-bearing: packers must not “fix” #49 by deleting the line (that is #2). Optional chair spelling, **not** KEEP-ON:

```
GETV_FPS=hmd
```

Do **not** add `hmd` to the smoke allowlist until a sit PASS. Scratch: `set GETV_FPS=hmd`.

**Bat-only APPLY (no EXE):** **Reject.** Unset or `hmd` on today’s binary is #2 or a no-op. Pinning 72 breaks 90.

**APPLY READY?** Bat comment + smoke note are packaging. They are useless until 3.1 ships in the **same** zip.

### 3.3 Out of scope

- Chair 72 / 80 / 90 / 120 **this DIG** (later)
- C-default 90 / KEEP graduation of Hertz
- Signing off >90
- RB-04 walk speed / intro 1.5×
- Judder interpolation / `SIMDIV=auto`
- Audio latency
- Personal credit paths. ROM dumps

---

## 4. Chair stare — **later** (plain tester sentences)

**Not this DIG.** After an EXE that prints `xr hz=`. Setup: next zip, `Start-GEVR.bat`. Recenter both sticks. Quest 3 + Virtual Desktop first (the #49 path). PSVR2 + SteamVR OpenXR second.

**Every run:** log must show `xr hz=N fps=N simhz=query` and **must not** show `HANDOVER REFUSED` / `No window`.

| | Tester sentence |
|--|-----------------|
| **H-PASS 72** | “Headset at **72**. I stay **in VR**. Banner `xr hz=72`. Intro is not a desktop window.” |
| **H-PASS 80** | “Same at **80**. Banner `xr hz=80`.” |
| **H-PASS 90** | “Same at **90**. Banner `xr hz=90`. Crystal / chair 90 is not worse than tonight.” |
| **H-PASS 120** | “If I try 120 it **either stays in VR at 120** (unsigned) **or stays in VR at the runtime’s fallback** — it does **not** dump me to desktop.” |
| **H-FAIL** | “72/80 instant desktop.” / “Banner says 90 while the headset is 72.” / “No banner and refuse.” / “Unset `GETV_FPS` dumps me like #2.” |
| **Feel (note only)** | Intro still fast or level still juddery at 90 is **not** an H-FAIL. File it on the sit; it is RB-04 / cadence, not attach. |

Regression: stereo fuse, gun aim, `SIMDIV=1` (no ghost-truck / portal holes from `268`). Monitor bat stays `GETV_FPS=60`.

---

## 5. Director decision

| If you say… | Then… |
|-------------|--------|
| **Green request-Hz EXE** | Workshop APPLY 3.1. Bat 3.2 in the **same** zip. Sit §4 later. |
| **Green bat-only** | Rejected by this DIG and by the 2026-09-19 tip. |
| **Green bake 90 in C** | Rejected. Copies tonight’s 72/80 refuse. |
| **Want `GETV_FPS=hmd` as ship spelling** | Fine after EXE. Prefer-90 + soft-fail is enough for vr44x. |
| **Want chair before merge** | Hold the zip. Public copy stays “90 works; #49 open.” |
| **Reject** | Stop. 90-only attach stays. #49 stays open. |

---

## 6. Attribution / legal

- No personal credit paths edited.
- No GoldenEye ROM, assets, or dumps.
- Workshop C stays private until release policy flips.
- #2 / #49 quotes are public GitHub. `325` / `257` / `268` / `92` are public GEVR textbook.
