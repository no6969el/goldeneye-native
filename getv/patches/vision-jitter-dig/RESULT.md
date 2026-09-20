# RESULT — vision shaking / jitter on head move (GEVR #59) (DIG ONLY)

**Status:** DIG. **Not APPLY READY.** No C landed.
**Ask:** BrunoFBK / GEVR GitHub [#59](https://github.com/no6969el/GEVR/issues/59) — whole-game **vision shakes** while pinpointing look; **“like a low-DPI mouse making small jumps.”** Public **vr441 KEEP**. **Quest 2 + Steam Link** (dedicated 5 GHz router). Same tester as [#60](https://github.com/no6969el/GEVR/issues/60) slowdown. Slight improvement on a 5060, jitter remains.
**Do not conflate:** [#49](https://github.com/no6969el/GEVR/issues/49) is **Hertz attach** (non-90 will not enter VR / 90 feels bad). #59 is an **attached** session whose **head look steps**. Related Hertz math; different sit.
**Date:** 2026-09-20.
**Evidence:** public `goldeneye-native` HEAD, public `no6969el/GEVR` textbook + vr441/vr442 boot, GEVR issues #49/#59/#60. Workshop C (`gevr_xr.c`, `gfx_sdl2.c`, `frametiming.c`, `posespine.c`) is **not on any public remote** (`GEVR` `docs/RELEASE-POLICY.md`). Brief names are the workshop symbols.

Director can treat this as **setup-first**, **code Hertz handover**, **pose/image honesty**, or **park** from this page alone.

---

## Verdict (one screen)

| Question | Answer |
|----------|--------|
| Wear-only? | **Mostly setup until the sit below PASSes.** Steam Link + Quest 2 is **not** a chaired path. Verified: Pimax+SteamVR OpenXR, PimaxXR, **Quest 3 + Virtual Desktop (VDXR)**. |
| Same as old recomp “two lines / rubber-band”? | **No as stated.** #59 is **quantized look** (low-DPI jumps while pinpointing). Recomp `95`/`97` was **doubling on fast head turn**, stick clean. Keep that split in the sit. |
| Primary setup root | **Hz pin vs panel + SteamVR Motion Smoothing (ASW) on Steam Link.** Boot is **`GETV_FPS=90` pinned, not `-HmdPace`**. Quest 2 Steam Link often **72**. App at 90 into compositor at 72 (or ASW filling the gap) **steps** the world. |
| Primary code root (if setup sit still FAILs) | **Product loop is still the SDL pacer.** Session `330`: `xrWaitFrame` **blocks in `goldeneye.exe`** at `11.1111 ms` / `90.0001 Hz`, **`geVrXrPaceArm()` not called**, **`sync_framerate_with_timer()` keeps the clock.** Dual pacer. Head pose is **not** the interpolator’s job (`97`). |
| ASW / motion smoothing | **Fight, not a fix.** Architecture: never rely on runtime ASW — HUD / gun smear. Chair history: Pimax **Smooth Motion was OFF**; Steam Link **defaults ON**. Stepped camera + compositor warp = jumps, not smoothness. |
| Pose prediction | Public ABI locates views **and** hands at `predictedDisplayTime` and submits **that** pose (`xr_session.cpp`). Workshop **recomp** path once submitted a **fresh locate with a stale image** (`93`/`ge_vr_xr.cpp`) — that **disables** compositor correction. Native GETV submit-pose honesty is **workshop-only**; not proven on vr441 Steam Link. |
| #60 slowdown | **Same wear.** Over-budget frames **coarsen** the same steps. 5060 “slightly better” is load, not a different bug. |
| APPLY tonight? | **No from this repo.** Hertz handover is the **#49 EXE** job (`HmdPace` / `geVrXrPaceArm`). Do not flip KEEP. Do not “turn on ASW” as a patch. |

```
still head          → should be rock-solid (if it jumps, encoding / tracking / vsync — not pose lag)
slow head pinpoint  → #59 (“low-DPI mouse”); SETUP sit first (Hz match, ASW off, VD vs Steam Link)
fast head turn      → old 95/97 doubling; separate row
stick turn          → control: if THIS jumps too, it is cadence / load, not the head-pose path
```

---

## 1. What the report is (and is not)

### 1.1 The issue, verbatim shape

| Field | Value |
|-------|--------|
| Title | Vision shaking when moving the head |
| Feel | “low-DPI mouse making small jumps to reach the target point” |
| Where | Entire game (so far) |
| Build | public vr441 / KEEP (`Start-GEVR`) |
| Path | Steam Link, dedicated 5 GHz, **Quest 2** |
| GPU note | 3070-Ti (with #60) and a **5060** — “slight improvement,” still present |
| Comments | none |

**“Pinpoint jumps” is angular quantization**, not smear, not stereo doubling, not a stuck gun. A tester trying to rest their gaze on a corner and watching the view **click** into place is this ticket. Ghosted mountain edges are **#49 / old judder**, a different sentence.

### 1.2 Related tickets (do not merge the sits)

| Ticket | What it is | Share with #59 |
|--------|------------|----------------|
| **#59** | Head look **steps** while attached | this DIG |
| **#60** | World **slow** on the **same** Quest 2 + Steam Link wear | same sit, extra row (budget / `SIMHZ`) |
| **#49** | Non-90 **will not attach**; 90 **judders** / intro too fast (Quest 3 VD, PSVR2 SteamVR) | Hertz pin. Director: public Hertz in **next EXE**, not a bat-only 90. Baking 90 in-exe copies the fail. |

---

## 2. Evidence boundary

| Layer | Where | What it proves |
|--------|--------|----------------|
| Public `goldeneye-native` | this repo | Host-agnostic OpenXR scaffold: `xrWaitFrame` → locate at **`predictedDisplayTime`** → simulate once → submit **same** pose (`src/xr_session.cpp`). Hands located at the **same** `t` (`xr_input.cpp`). **`predictedDisplayPeriod` unused.** Not the playable GETV loop. |
| Public `no6969el/GEVR` | docs + `packaging/` | Ship knobs, verified headsets, vr441 boot **`GETV_FPS=90` “not -HmdPace”**, session `330` wait-frame vs SDL-pacer split. |
| Workshop (private) | `F:\Projects\GEVR\GoldenEyeVR\goldeneye-native` | `gevr_xr.c`, `gfx_sdl2.c` `GETV_FPS`, `frametiming.c` `GETV_SIMHZ`/`GETV_SIMDIV`, `posespine.c`, live `HEADYAW`/`HEADFRAME`. **Do not push.** |
| Issues | GEVR #49 / #59 / #60 | Wear reports. No logs, no Hz, no ASW on/off, no stick-vs-head split. |

`gevr_xr.c` / `geVrXrPaceArm` / `sync_framerate_with_timer` / `GETV_SIMDIV` **do not appear as bodies in this public tree.** First chair grep (workshop): `GETV_FPS`, `geVrXrPaceArm`, `xrLocateViews`, `HEADFRAME`.

---

## 3. Setup vs code (keep these in two columns)

### 3.1 SETUP — do this sit before any C

Steam Link + Quest 2 is **outside** the chaired matrix (`FEATURES-CURRENT.md` / README Play):

| Chaired | #59 wear |
|---------|----------|
| Pimax Crystal Super + **SteamVR OpenXR** | Quest **2** |
| Native **PimaxXR** | **Steam Link** (wireless encode + SteamVR compositor) |
| Quest **3** + **Virtual Desktop VDXR** | Dedicated 5 GHz (good radio, still Steam Link) |

**A. Hertz mismatch (highest leverage, shared with #49).**

vr441 boot (`packaging/templates/gevr-vr441-boot.cmd`):

```
rem 2. Core VR + pacing (425w standing keepers; 90 pinned, not -HmdPace)
set GETV_FPS=90
set GETV_SIMDIV=1
set GETV_SIMHZ=query
set GETV_BUDGET=120
```

- Quest 2 native modes: **72** (stock), also 60 / 90 / 120.
- Steam Link / SteamVR often **presents 72** while the bat **caps the game at 90**.
- `325` interlock: **`GETV_FPS=90` vs measured `60` REFUSES handover**; `90` vs `90.0001` AGREES. A 90-cap vs a 72 panel is the **disagree** case with **no product handover armed** (`330`: `geVrXrPaceArm()` not called).
- Result on the retina: **pulldown** — held frames then a jump. Feels like **low-DPI stepping** on slow look, worse on pinpoint.

**B. SteamVR Motion Smoothing / ASW (the fight).**

| Fact | Source |
|------|--------|
| Do **not** budget missed frames on runtime ASW; HUD/weapon **smear** | `GE007-VR-ARCHITECTURE.md` §8 |
| Pimax “Smooth Motion” was **disabled** in the chair; runtime **did not** reproject | GEVR `97` (O79) |
| SteamVR **Motion Smoothing** (ASW) is **on by default** on many Steam Link installs | SteamVR compositor, not GEVR |
| Submitting **current pose + stale image** tells the compositor **zero warp is needed** | GEVR `93` (recomp `ge_vr_xr.cpp`) |
| If ASW **is** on, it warps from a **lie** or from a **stepped** camera → **oscillation / clicks**, not smoothness | `95` (two-state vs lag) |

**Setup action:** SteamVR → per-app / global **Motion Smoothing OFF**. Steam Link overlay: extra prediction / spacewarp **OFF**. Then re-wear #59. If jumps **die**, this ticket is **setup**. If they **live**, it is code (Hertz handover + pose honesty).

**C. Steam Link encode (wireless).**

Dedicated 5 GHz is the right radio. It does **not** make Steam Link a pose-honest OpenXR path. Encode + client prediction **quantize** small head deltas — the exact “mouse DPI” metaphor. **Control:** same PC, **Quest 2 Link cable** or **Virtual Desktop** (the chaired Quest path is **Q3+VDXR**). If cable/VD is smooth and Steam Link jumps, **do not patch GEVR for Steam Link ASW**.

**D. Load (#60).**

Same tester: game **slow** everywhere; 5060 only **slightly** faster. Over-budget CPU frames (`GETV_BUDGET=120` exists to print `OVER=`) drop pose updates → **coarser** jumps. Setup sit still required; a 5060 does not close #59.

**E. Not setup (do not send the tester here first).**

| Knob | Why not |
|------|---------|
| `GETV_XR_TURN_DEAD=20` | **Stick** deadzone, not head |
| `GETV_VR_HEADYAW=1` / `HEADFRAME=2` / `HEADYAW_IPD=1` | KEEP_SHIP. Flipping them is a **code** A/B, not a Steam Link toggle |
| `GETV_SIMDIV` unset / `auto` | At 90, `auto` **self-selects divider 3** (sim 30 Hz) — `268`, **already pinned `1` in boot**. Do not “try auto” |
| Recenter / floor | Wrong height is dollhouse, not DPI jumps |

### 3.2 CODE — only if the setup sit still FAILs on a chaired path **or** on Steam Link with ASW off + Hz matched

**C1. Dual pacer (the standing handover).**

| Clock | Who owns it on vr441/vr442 public boot |
|-------|----------------------------------------|
| SDL / `GETV_FPS=90` | `gfx_sdl2.c` `sync_framerate_with_timer()` — **the live clock** (`330`) |
| OpenXR | `xrWaitFrame` **can** block in-process (`330` K-3, `11.1111 ms`, `0` discarded) and **does not** own the game loop |

`329`/`330` **NEXT** line, still the job: arm `geVrXrPaceArm()` on a **measured** `predictedDisplayPeriod`. Director tip on #49: **query runtime Hz after `xrCreateSession`, pace from it, stay in VR, banner `xr hz=N fps=N simhz=query`.** Pinning 90 in the bat (or baking 90 in the EXE) is the #49 fail on 72/80 VD **and** the #59 fail on Quest 2 Steam Link 72.

Public scaffold already has the **shape** (`src/xr_session.cpp`): `xrWaitFrame` → `predictedDisplayTime` locate → `xrEndFrame` at **that** `t`. It does **not** read `predictedDisplayPeriod` to drive `GETV_FPS`. Workshop must **hand the product loop** to that, not copy the scaffold into a second clock.

**C2. Head pose is not interpolated. Stick is.**

GEVR `97` (still the right physics, native or recomp):

- **Stick turn** = game camera in the **sim**, RT64 / present interpolator fills 60→90.
- **Head turn** = pose at **eye-render**. Interpolator **never sees it**. Nothing synthesises intermediate head frames.

So: stick can feel “not quite 90 but clean”; head **clicks**. That **is** the low-DPI sentence. Fix is **not** “more interpolation on props.” Fix is **one present clock** (`xrWaitFrame`) + **honest layer pose** (image and `projViews[].pose` from the **same** locate) so the compositor’s **rotational** warp is legal. Do **not** turn SteamVR ASW on to fake the missing intermediates.

**C3. Pose / image mismatch (workshop native still unproven on this wear).**

Recomp fact (`93`, `ge_vr_xr.cpp`): locate at predicted time, copy **whatever RT64 last drew**, submit **this tick’s** pose. Compositor computes **zero** correction. Angular error = ω × latency — **zero when still, steps when you pinpoint**.

Native GETV: `GETV_STEREO_SRC=xr`, `HEADYAW=1`, `HEADFRAME=2` (KEEP; enum **2** is ship, body private). Public ABI submits the locate it drew. **Workshop `gevr_xr.c` submit vs draw** needs a `posecheck` / layer-pose delta on **Steam Link** before anyone “fixes” ASW.

Layer-pose **yaw lie** (`36`, recenter in draw, raw pose in layer) produces **per-eye warp**, fusion failure — closer to “two of everything” than DPI clicks. Still a **head-move-only** fault. Sit T1 (mirror) splits it.

**C4. `HEADFRAME=2` / writing head into `vv_theta`.**

Ship writes head into the **game camera** (`HEADYAW=1`) so aim stays in frustum (`194`/`226`). If that write is **per sim tick** (or integer degrees) while the compositor presents faster, **vv_theta itself is the low-DPI grid**. A/B (workshop only, not KEEP):

- `GETV_VR_HEADYAW=0` — head only in the eye view matrix (old `36` composition). If jumps **die** and guns/aim **break**, the grid is the **theta write**.
- Leave `HEADFRAME=2` until that A/B. Do not guess enum 0/1 from this repo.

**C5. Out of scope for this DIG**

- Changing the 1/60 quantum (`92` route B) — aim integrators loop on `g_ClockTimer`.
- `GETV_SIMDIV=auto` back on at 90 (`268` ghosts).
- `GETV_REALCLOCK=1` as a 90 Hz present (`274`: it **renders 60**).
- Baking `GETV_FPS=90` into the EXE (#49).
- Steam Link encoder patches inside GEVR.

---

## 4. OpenXR path (what must agree)

```
xrWaitFrame                         → predictedDisplayTime, predictedDisplayPeriod
xrLocateViews(displayTime = t)      → eye poses + FOV
xrLocateSpace(hands, same t)        → gun / cubes
simulate ONCE
draw each eye from those poses
xrEndFrame(displayTime = t,
           projViews[eye].pose = the pose THAT EYE WAS DRAWN AT)
```

| Must | Why a miss feels like #59 |
|------|---------------------------|
| One clock | SDL 90 + compositor 72 = pulldown clicks |
| Same `t` for views, hands, submit | Gun/world disagree, or compositor warps a lie |
| Layer pose = draw pose | Fresh pose + stale image → **no** warp (`93`) or **wrong** warp (`36`) |
| Do not dual-predict | App prediction **plus** SteamVR ASW = two-state oscillation (`95`) |
| `shouldRender` / discarded | Missed frames: ASW smears HUD; extra frames: jumps |

Public `src/xr_session.cpp` does the locate/submit identity. It ignores `predictedDisplayPeriod`. Product GETV still sleeps on `GETV_FPS`. That gap **is** the code root if setup is clean.

---

## 5. APPLY sketches — **NOT LANDED**

No C in this tree. Workshop-only, after the setup sit, likely **folded into the #49 Hertz EXE** rather than a third pacer.

```c
/* NOT APPLY READY — workshop sketch, same session as #49 HmdPace.
 * After xrCreateSession / first xrWaitFrame:
 *   hz = 1e9 / frameState.predictedDisplayPeriod
 *   if GETV_FPS is unset or "hmd"/"panel": ge_pace_framerate = hz
 *   geVrXrPaceArm(hz)  -- 330 NEXT; refuse if SDL cap disagrees (325)
 * Do NOT bake 90. Do NOT call this from goldeneye-native public ABI only.
 */
```

Optional wear (not KEEP): `GETV_FPS=hmd` in a **scratch** boot. Do **not** add to vr441 allowlist until #49 chair PASSes at 72/80/90.

Pose honesty (only if T1 says mirror smooth / lenses jump):

```c
/* Carry the pose used to BUILD the eye through to projViews[eye].pose.
 * One pose, not two (GEVR 93/96). Do not invent a second locate at EndFrame.
 */
```

**APPLY READY?** **No.** Hertz + interlock + Steam Link sit are the #49/#59 pair. Bodies private.

---

## 6. Chair stare — tester checklist (plain sentences)

**Boot:** current Latest zip (vr442 if that is Latest; reporter was on vr441 — same **`GETV_FPS=90` pin**). `Start-GEVR.bat`. Recenter both sticks. Dam or Facility, **stand still**, high-contrast corner (door frame, crate edge).

Record for the log: headset, **OpenXR runtime**, SteamVR on/off, Steam Link vs VD vs cable, **headset Hz** (Quest UI + SteamVR video), Motion Smoothing on/off, GPU, zip tag. No ROM.

### Run 0 — classify (30 seconds, no settings change)

| | Tester sentence |
|--|-----------------|
| **0-A** | “Head **perfectly still**, staring at a corner: the world **does not** click or shimmer.” |
| **0-B** | “I turn **slowly** to put that corner on a speck: the view **jumps in steps** like a cheap mouse. That is this bug.” |
| **0-C** | “I turn **fast**: I see **double edges / rubber-band**, not steps.” (old `95` — **different row**, still file it) |
| **0-D** | “**Stick turn** at the same speed is **smooth** (maybe not ‘true 90’, but not stepping).” |
| **0-FAIL class** | Still jumps → encoding/tracking. Stick **also** steps → cadence/load (#60), not head-only. Fast doubling only → not #59. |

### Run S — SETUP (do in order; stop at first PASS)

| | Change | PASS sentence | Meaning |
|--|--------|----------------|---------|
| **S1** | SteamVR **Motion Smoothing OFF**; Steam Link extra warp/prediction **OFF**. Same 90 boot. | “Pinpoint steps **gone** or **clearly better**.” | ASW fight. **Setup.** Ship note: Steam Link + smoothing off. |
| **S2** | Headset **72 Hz** (Quest 2 stock). Boot still 90. Smoothing off. | “Worse / same steps.” | **90-into-72 pulldown.** Hertz DIG (#49). |
| **S3** | Headset **90 Hz** if Quest 2 offers it. Boot 90. Smoothing off. | “Steps **gone**.” | Pin matched panel. **Setup** until HmdPace exists. |
| **S4** | **Virtual Desktop** (or Link cable) on the **same** PC/Quest. Smoothing off. 72 and 90 once each. | “VD/cable **smooth**; Steam Link **steps**.” | Steam Link encode/prediction. **Do not patch GEVR.** |
| **S5** | `#60` row: look at `[getv][budget] OVER=` / whether Bond feels slow. | “Slow **and** stepping together; faster GPU helps a little.” | Load coarsens steps; not the only cause. |

**S-FAIL (code):** S1–S4 all still step, **including VD or cable at matched Hz**. Then Run C.

### Run C — CODE (workshop / next EXE; not this PR)

| | Tester sentence |
|--|-----------------|
| **C-PASS 1** | “Banner shows `xr hz=` **same number** as the headset, and `fps=` that number — not a stuck 90 on a 72 panel.” |
| **C-PASS 2** | “Slow pinpoint is **continuous**, not clicks. Stick is no worse.” |
| **C-PASS 3** | “Still head is still solid. Fast turn does **not** grow double edges vs tonight.” |
| **C-FAIL** | “72/80 still desktop-only” (#49). / “90 banner, 72 panel, still clicks.” / “Smoothing off + matched Hz + VD still clicks” → pose/image (`93`) or `HEADYAW` grid (C4). |

**HEADYAW A/B (only if C-FAIL after Hertz):** scratch `GETV_VR_HEADYAW=0`. PASS: “jumps gone, gun/aim wrong” → theta grid. FAIL: “still jumps” → submit pose, not theta.

**Regression every run:** gun aim, squeeze ADS on the **gun ray**, stereo fuse, no 1.5× Bond (`RB-04`). Do not flip `GUNARM` / `GUNAIM` / `SIMDIV=1`.

---

## 7. Director decision

| If you say… | Then… |
|-------------|--------|
| **Green setup note only** | Reply on #59: Steam Link unverified; Motion Smoothing **off**; match Hz to `GETV_FPS`; prefer **VD / Link**. No EXE. |
| **Green Hertz EXE (#49)** | Workshop `HmdPace`: query `predictedDisplayPeriod`, pace from it, stay in VR. **This DIG’s code root is that same change.** Sit #49 72/80/90 **and** #59 Run S3/S4/C. |
| **Green pose-honesty** | Only after S1–S4 FAIL. Carry draw pose to layer. `posecheck` MAX delta → ~0. |
| **Green HEADYAW A/B** | After Hertz. Do not KEEP-off `HEADYAW` without the aim sit (`226`). |
| **Want ASW on as the fix** | **Reject.** Architecture + `97`: runtime warp on a stepped/lied pose **is** the jitter. |
| **Park** | Leave vr441/vr442 90 pin. Tell Quest 2 Steam Link testers the chaired path is **Q3+VDXR** or Pimax. |

**Recommend:** **setup note now** (S1–S4) **and** fold code into **#49 Hertz EXE**. Do not open a third pacer. Do not APPLY from `goldeneye-native`.

---

## 8. Attribution / legal

- No personal credit paths edited.
- No GoldenEye ROM, assets, or dumps.
- Recomp judder docs (`92`–`97`) and session `330` are **map-only** (public GEVR textbook). Do not copy workshop `gevr_xr.c`.
- Workshop C stays private until release policy flips.
