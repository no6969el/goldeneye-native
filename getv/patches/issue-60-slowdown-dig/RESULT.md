# RESULT — Game runs very slowly (#60) (DIG ONLY)

**Status:** DIG. **Not APPLY READY.** No C, no boot edit, no KEEP flip.
**Ask:** BrunoFBK — entire game feels slow vs the wear video. Quest 2 + Steam Link (dedicated 5 GHz) on RTX 3070-Ti; a 5060 was only slightly faster. Public vr441 KEEP / `Start-GEVR.bat`.
**Constraints:** Dig only. Separate **setup vs code**. Four named killers: **resolution-mirror hitch, uncapped desktop, stereo cost, cull.** Do not chair-wear from this VM.
**Date:** 2026-09-20.
**Evidence:** `no6969el/GEVR` #60/#46/#49/#29 + FEATURES verified table; `packaging/templates/gevr-vr441-boot.cmd`; `docs/258`, `273`, `92`, `49` (render-path), `RELEASE-POLICY.md`; public `goldeneye-native` GETV getenv stubs + `src/xr_session.cpp`. Workshop GETV bodies (`gevr_xr.c`, `gfx_sdl2.c`, `stereo.c`) are **not on any public remote**.

Director can send testers the checklist, or park #60 as setup, from this page alone.

Tester paste sheet: [`TESTER-CHECKLIST.md`](TESTER-CHECKLIST.md).

---

## Verdict (one screen)

| Question | Answer |
|----------|--------|
| One bug or a stack? | **Stack.** #60 is the *feel*. The four killers are already tickets / KEEP, not a new mystery shader. |
| Setup or code first? | **Setup first on this report.** Quest 2 + Steam Link is **not** a signed-off path. Level-independent + “5060 only slightly faster” is the encode / Hertz / desktop-present signature, not Cradle-vs-Facility fill. |
| Resolution-mirror hitch | **CODE, already filed as [#46](https://github.com/no6969el/GEVR/issues/46).** HMD draw size is coupled to the desktop window. SS3 KEEP makes that hitch expensive. SteamVR SS **stacks** on top. |
| Uncapped desktop | **SETUP + missing boot pin.** Public boot sets `GETV_FPS=90` and does **not** assign `GETV_VSYNC`. Chair visual runs pin `VSYNC=1`. `GETV_FPS=0` is a **measure-only** arm that **locks Cradle** (`273`). Steam Link adds a second compositor that does not care about our SDL present. |
| Stereo cost | **CODE, KEEP-ON, constant.** `GETV_STEREO_REBUILD=1` + two eyes. Architecture: simulate once, build/draw twice. Not scene-dependent. |
| Cull | **CODE, KEEP-ON, scene-dependent if it were the floor.** `GETV_VR_DRAWALL=1`, `CULLWIDE=3.0`, `SCREENWIDE=3.0`, prop-box cull off, `OCCLSKIP=1` (crate keep). Trades GPU/CPU for no one-eye pop. **If Dam ≉ Facility, cull/fill is not #60’s floor.** |
| 90 Hz pin | **CODE + setup.** Boot comment: `90 pinned, not -HmdPace`. [#49](https://github.com/no6969el/GEVR/issues/49): that pin **blocks VD attach at 72/80**. Quest 2 Steam Link often *wants* 72. Opposite report on #49 is “too fast”; #60 is “too slow” — both are Hertz/pacing, not Dam geometry. |
| Same as the 5060 wear video? | **No.** FEATURES wear: **RTX 5060 laptop + Quest 3 + VDXR**. Reporter 5060 test that stayed on **Quest 2 Steam Link** is not that path. |
| APPLY tonight? | **No.** Next product: #46 (HMD res ≠ desktop blit) and #49 (query runtime Hz in-exe). KEEP SS3 / DRAWALL / STEREO_REBUILD stay until a chair A/B says otherwise. |

```
Quest 2 Steam Link  → SteamVR OpenXR + NVENC + often 72 Hz
Start-GEVR KEEP     → GETV_FPS=90, SS3+SrcFbo, STEREO_REBUILD, DRAWALL, PLAY_SCREEN=2
desktop window      → same huge buffer as HMD (#46)  → Present hitch every frame
GPU swap 3070-Ti→5060 → only a little faster          → not fill-bound
signed-off wear     → Q3+VDXR or Crystal Super        → not this report
```

---

## 1. What #60 actually reported

[`no6969el/GEVR#60`](https://github.com/no6969el/GEVR/issues/60) (open, no comments at dig time):

- Slow **everywhere**, not one stage.
- Compared to **a video**, not to a number (`pacehist`, `DISPLAY PERIOD`, GPU draw).
- **vr441 KEEP** via `Start-GEVR.bat`.
- **Meta Quest 2 + Steam Link**, dedicated 5 GHz router.
- **RTX 3070-Ti / Ryzen 7 / 32 GB**; later a **5060, only slightly faster**.
- Author already suspects setup.

“Slow” is three different bugs. Testers must pick one before any knob:

| Feel | Likely family | Not |
|------|---------------|-----|
| Bond / guards / doors in **slow-mo** | Timebase / Hertz (`GETV_FPS` vs compositor Hz, `SIMHZ=query`) | SS3 fill |
| **Stutter / hitch / ASW smear**, motion otherwise timed | Present path: mirror blit, Steam Link encode, missed `xrWaitFrame` | Sim quantum |
| Smooth but **not 90** (stick turn clean, head turn “film”) | `92`: sim is 60, display is 90. Honest KEEP. | A 3070-Ti being “too weak” |

`60-the-60-fps-is-the-n64-vi-clock.md`: a console line that reads `60.0 fps` is the **VI clock**, not the HMD present rate. Do not diagnose #60 from that banner.

---

## 2. Evidence boundary

| Layer | Where | What it proves |
|-------|--------|----------------|
| Public GEVR issues | #60 #46 #49 #29 #36 | Symptoms, director Hertz tip, HMD-vs-desktop ticket, crate-cull KEEP. |
| Public GEVR packaging | `gevr-vr441-boot.cmd`, KEEP inventory, FEATURES.md | Exact ship knobs. vr442 release notes do **not** change SS / DRAWALL / FPS. |
| Public GEVR textbook | `docs/258`, `273`, `92`, `49-render-path`, `PRIORITY-BOARD-*` X4 | Dual output surface; uncapped GPU number; 90 is not a native sim rate; stereo = one FB split + a **mirror window**. |
| Public `goldeneye-native` | `getv/port/*` getenv stubs, `src/xr_session.cpp` | SS3 / DRAWALL / SrcFbo / STEREO_REBUILD **C-default ON**. Scaffold swapchains use **runtime recommended** rect, no desktop blit. |
| Workshop (private) | `gevr_xr.*`, `gfx_sdl2.c`, `stereo.c`, `port_render.c` bodies | How PLAY_SCREEN=2 and SrcFbo actually present. **Do not push.** |

Public `xr_session.cpp` is the **architecture**, not the playable exe:

```148:156:src/xr_session.cpp
    for (int eye = 0; eye < kEyeCount; ++eye) {
        const auto& vc = impl_->view_config[eye];
        XrSwapchainCreateInfo ci{XR_TYPE_SWAPCHAIN_CREATE_INFO};
        ...
        ci.width       = vc.recommendedImageRectWidth;
        ci.height      = vc.recommendedImageRectHeight;
```

No scale knob. SteamVR “render resolution” **is** that recommended rect. GEVR SS3 is a **second** multiplier on the game’s own FB (KEEP).

---

## 3. The four killers

### 3.1 Resolution-mirror hitch — **CODE, open as #46**

[#46](https://github.com/no6969el/GEVR/issues/46) in the owner’s words: *raise what the headset draws without forcing the same huge size on the desktop mirror. vr441 `Start-GEVR.bat` already locks a chaired sharpness path. This ticket is the next lever: HMD res without the desktop hitch.*

`258` §2 measured the same shape on the engine side:

> one framebuffer to be simultaneously a headset swapchain image at headset resolution **and a desktop window at window resolution.** That is a second output target.

PRIORITY-BOARD X4: stereo today is **one framebuffer split in halves**; a runtime wants **per-eye swapchains plus a mirror window**.

Ship KEEP that makes the hitch large:

| Knob | vr441 boot | Why it hurts the mirror |
|------|------------|-------------------------|
| `GETV_SUPERSAMPLE` | `3` | Sharpness KEEP. “Never SS>1 without SrcFbo.” |
| `GETV_XR_PLAY_SRCFBO` | `1` | Real name (not dead `GETV_SRCFBO`). |
| `GETV_XR_PLAY_SCREEN` | `2` | KEEP. Enum body is workshop-only; boot still **forces a screen present**. |
| `GETV_WINDOW` | **wiped** | Default / XR path sizes the SDL window. Maximising it is the hitch testers can feel. |

**Setup amplifier (Quest 2 Steam Link):** SteamVR SS (often >100%) × GEVR SS3 × two eyes × NVENC of the compositor. Dedicated 5 GHz does not remove encode.

**Falsifier:** shrink **only** the desktop window (or SteamVR SS) and the *feel* improves while the HMD looks the same → hitch confirmed. If HMD sharpness and hitch always move together, they still share a buffer (#46).

### 3.2 Uncapped desktop — **SETUP, plus a missing pin**

| Fact | Source |
|------|--------|
| Public headset boot: `GETV_FPS=90`, `GETV_SIMHZ=query`, `GETV_SIMDIV=1` | `gevr-vr441-boot.cmd` §2. Comment: **90 pinned, not HmdPace.** |
| `GETV_VSYNC` **not assigned** (and not wiped) | same boot. Inherit from user shell / exe default. |
| Chair runs the owner *looks at*: `GETV_VSYNC=1` | `287` onward. `VSYNC=0` was the wrong default for visual arms. |
| Uncapped `GETV_FPS=0` + `VSYNC=0` | **Measure GPU only.** Cradle **locks up** in 3/3 (`273` §7.3). Testers must not use this to “go faster.” |
| `273` GPU draw, 1280×960, RTX 5090, **uncapped** | Cradle **0.832 ms**, Facility **0.334 ms**. Capped “GPU draw” **is the frame period** — refuse it. |
| Desktop vsync was a false 60 Hz lead on the recomp | `docs/49-render-path-investigation.md` wrong-turn 1. Killed by a 180 Hz monitor. |

Steam Link / SteamVR is a **second pacer**. An SDL window that presents every frame without a cap, or at a different Hz than `xrWaitFrame`, races the compositor. That is “uncapped desktop” in this stack even when `GETV_FPS=90`.

**Falsifier:** `Play-on-monitor.bat` (`GETV_FPS=60`, no XR). If **flat is fine** and **VR is slow**, the floor is the XR present / encode path, not the interpreter. If **both** are slow, look at CPU / antivirus / ROM cache first.

### 3.3 Stereo cost — **CODE, KEEP, constant across levels**

Architecture (`GE007-VR-ARCHITECTURE.md` §5.2, `src/xr_session.cpp` `pumpFrame`): **simulate once**, render **per eye**. `getPlayerCount()` must stay 1 (`258` 1.5).

Ship KEEP:

| Knob | Value | Cost |
|------|-------|------|
| `GETV_XR_PLAY_STEREO` | `1` | Two views. |
| `GETV_STEREO_SRC` | `xr` | Headset frusta, not a desktop fake. |
| `GETV_STEREO_REBUILD` | `1` | **Rebuild the display list per eye**, not “same list, swap projection.” CPU ×2 on the interpreter. |
| `GETV_STEREO_VIEWRESTORE` / `HUDGATE` / `AIMRECT` / `GUNOFS` | `1` | Correctness KEEP. Cheap vs rebuild. |

`273` §6: **nobody has measured two views.** Pre-stereo GPU on a 5090 at 1280×960 was <1 ms. That number does **not** budget SS3 × recommended Q2/SteamVR rect × two eyes × SrcFbo blit × desktop Present.

Level-independent slowness **fits stereo+SS+present**. It does **not** fit “Dam is too big.”

**Falsifier (diagnostic only, restores KEEP after):** `GETV_SUPERSAMPLE=1` first. Only then, as a **break-the-picture** A/B, `GETV_STEREO_REBUILD=0` — if that is the only thing that saves the frame, the CPU list-walk is the floor. Do not ship that.

### 3.4 Cull — **CODE, KEEP, should be scene-dependent**

vr441 boot §7 (rooms / cull / portals):

| Knob | Ship | Class |
|------|------|--------|
| `GETV_VR_DRAWALL` | `1` | KEEP — skip room visibility, draw the lot. C-default ON in public stubs. |
| `GETV_VR_CULLWIDE` | `3.0` | PLAYER_PREF — widen the cull frustum. |
| `GETV_VR_SCREENWIDE` | `3.0` | PLAYER_PREF |
| `GETV_VR_ROOMBUDGET` | `64` | PLAYER_PREF |
| `GETV_VR_LODDIST` | `0.25` | PLAYER_PREF |
| `GETV_ROOMSCISSOR` | `0` | DIG_OFF |
| `GETV_PROPCULLBOX` | `0` | DIG_OFF |
| `GETV_VR_OCCLSKIP` | wiped on vr441 boot; **C-default 1** on later KEEP | Crate keep [#29](https://github.com/no6969el/GEVR/issues/29). **Visibility, not speed.** |

Why DRAWALL shipped: VR FOV is wider than the N64 camera; per-eye cull pops in **one eye** (`GE007-VR-ARCHITECTURE.md` §5.3 — union frustum is the real fix; DRAWALL is the blunt one).

`278`: widescreen *holes* were **port fill / `GETV_WIDESCREEN`**, not the room-visibility pass. Do not “fix #60” by turning DRAWALL off and calling remaining holes a win.

Public `gbi_interp.cpp` `G_CULLDL` is real outcodes. GETV DRAWALL sits **above** that (room admit). Different layer.

**Falsifier:** Facility vent vs Dam towers, same headset settings. **If they feel the same, cull is not the floor.** If Dam tanks and Facility is fine, then DRAWALL/CULLWIDE are in play — still a **second** ticket, not #60 as filed.

---

## 4. Setup vs code (this report)

### Setup (do these before any KEEP A/B)

| Item | Why it matches #60 |
|------|--------------------|
| **Quest 2 + Steam Link** | Not in FEATURES verified table (Crystal Super + SteamVR OpenXR, native PimaxXR, **Quest 3 + VDXR**). |
| SteamVR as OpenXR runtime | Extra compositor + SS slider + optional motion smoothing. |
| Wireless encode | GPU/NVENC every frame on top of SS3 stereo. Dedicated router ≠ zero encode. |
| Q2 default **72 Hz** vs boot **`GETV_FPS=90`** | #49: 90-only attach on VD; Q2 Steam Link at 90 is a hard mode. |
| Desktop window maximised / same res as HMD | #46 hitch. |
| NVIDIA overlay, RTSS, Afterburner | `49` wrong-turn 3. Exact 60 looks like a limiter even when `Limit=0`. |
| Comparing to the **BarZ 5060 video** | That wear was **Q3 + VDXR**, not Q2 Steam Link. |
| `GETV_VSYNC` inherited | Boot does not pin it. |

### Code / KEEP (product, not the user’s router)

| Item | Ticket / home | Do not flip for ship without a chair |
|------|----------------|--------------------------------------|
| HMD rect == desktop blit | **#46** | Split present. Downscale mirror. |
| `GETV_FPS=90` baked | **#49** | Query runtime Hz after `xrCreateSession`; banner `xr hz=N fps=N simhz=query`. |
| SS3 + SrcFbo | KEEP picture | Per-path default (wireless / Q2) is a **later** product choice. |
| `STEREO_REBUILD=1` | KEEP fusion | Measure two-view CPU before changing. |
| `DRAWALL=1` + CULLWIDE 3 | KEEP + #29 family | Union-frustum cull, then DRAWALL off. |
| `PLAY_SCREEN=2` | KEEP | Workshop enum. Likely the desktop present. |
| Uncapped GPU sample | `273` | Instrument only. Contaminated if the pacer slept. |

### What this dig does **not** claim

- That a 3070-Ti “cannot run GoldenEye.” Pre-stereo fill is tiny. The **present stack** is the tax.
- That SS3 is 3× vs 9× pixels (workshop `gfx_opengl` body not public). Treat as **large**.
- That `GETV_XR_PLAY_SCREEN=2` means a specific blit layout.
- A worn fps number for Bruno’s rig (none attached).

---

## 5. Product follow-ups (not this PR)

1. **#46** — HMD swapchain size independent of SDL window; mirror is a **downscale blit**, skippable.
2. **#49** — in-exe Hertz query / `HmdPace`. Stop pinning 90 in the bat. Until that chairs, public copy stays “90 works; #49 open.”
3. **Budget line** that cannot lie: present Hz, `predictedDisplayPeriod`, desktop Present µs, encode-not-our-job. `273` gate: capped GPU draw must print CONTAMINATED.
4. **DRAWALL** off only after union-frustum + #29 crate keep still PASS.
5. Optional **path preset**: “wireless / Quest” SS=1 or 2, DRAWALL stays until (4).

---

## 6. APPLY

**None.** DIG ONLY. Testers use [`TESTER-CHECKLIST.md`](TESTER-CHECKLIST.md). Chair may A/B SS / DRAWALL / window on SimRig; do not graduate those from this page.
