# Tester checklist — GEVR #60 slowdown

Paste results back on [GEVR#60](https://github.com/no6969el/GEVR/issues/60) (no ROM). Full dig: [`RESULT.md`](RESULT.md).

**Do the setup block first.** Do not edit KEEP knobs until A–G are written down. Restore any boot edits when you are done.

---

## 0. Name the feel (pick one)

- [ ] **Slow-mo** — Bond, guards, doors, and audio all run at the wrong speed
- [ ] **Hitch / stutter / smear** — timing feels OK, picture catches or warps
- [ ] **Smooth but not “90”** — stick turn is fine; head turn looks like film (this can be normal; see notes)
- [ ] **Mix** — say which is worse

Also write:

- Headset + **OpenXR runtime** (SteamVR / VDXR / PimaxXR / other)
- SteamVR on or off
- **`Start-GEVR.bat` yes/no** and the `gevr-*-boot.cmd` filename next to the exe
- Zip tag (vr441 / vr442 / other)
- GPU / CPU
- Quest refresh in the headset/Steam Link UI (72 / 80 / 90 / …)

---

## A. Setup (no boot edits)

| # | Try | Pass if… |
|---|-----|----------|
| A1 | Use **`Start-GEVR.bat`**, not bare `goldeneye.exe` | Still slow? Stay on this sheet. |
| A2 | **Do not maximise** the desktop window. Leave it small. Close extra SteamVR / NVIDIA overlays. | Hitch eases with a tiny window → **mirror hitch** ([#46](https://github.com/no6969el/GEVR/issues/46)). |
| A3 | SteamVR: render resolution **100%** (not 150–500%). Motion smoothing / extra SS **off**. SteamVR Home **off**. | Large gain here → SteamVR SS stacked on GEVR SS3. |
| A4 | Quest 2: set **72 Hz** in Steam Link / headset (match what the headset actually runs). Avoid forcing 90 on Q2 wireless if 72 is available. | Slow-mo or hitch changes with Hz → **pacing** ([#49](https://github.com/no6969el/GEVR/issues/49)). |
| A5 | RTSS / Afterburner **frame limiter off**. NVIDIA overlay off. | Exact 60 or 90 with limiter on is a false lead. |
| A6 | Control: **`Play-on-monitor.bat`** (no headset). | Flat fine + VR slow → XR present / encode, not the ROM. Both slow → PC/cache/CPU first. |
| A7 | Same PC, different path if you have it: **Quest 3 + Virtual Desktop (VDXR)** is the signed-off Quest wear. Crystal Super / PimaxXR also signed off. **Quest 2 Steam Link is not.** | VDXR fine + Steam Link bad → **setup**, not a weak 3070-Ti. |
| A8 | Facility (tight) vs Dam (open), 30 s each, standing still then turning. | **Same feel both maps** → not cull/fill. Dam much worse → then do block C (DRAWALL). |

A 5060 that stays on **Quest 2 Steam Link** will only be *slightly* faster. The signed-off 5060 wear was **Quest 3 + VDXR**.

---

## B. Diagnostic knobs (temporary)

Copy `gevr-*-boot.cmd`, edit the copy, point a throwaway bat at it — or set the vars in that cmd **after** the KEEP block. **One change per run.** Write SS / window / DRAWALL for each sit.

| # | Change | Expect | Restore to |
|---|--------|--------|------------|
| B1 | `GETV_SUPERSAMPLE=1` (SrcFbo may stay 1) | Big win → fill + mirror size. Picture softer. | `3` |
| B2 | `GETV_WINDOW=1280x720` (only if the window actually becomes that size) | Win with blurry HMD → #46 still coupled. Win with **sharp HMD** → you found the split #46 wants. | wiped / default |
| B3 | `GETV_VR_DRAWALL=0` | **Only if A8 showed Dam >> Facility.** Props/rooms may pop in one eye. | `1` |
| B4 | `GETV_VR_CULLWIDE=1.0` and `GETV_VR_SCREENWIDE=1.0` | Same as B3. | `3.0` |
| B5 | If the headset is 72: `GETV_FPS=72` (keep `GETV_SIMHZ=query`) | Slow-mo/hitch tracks the pin → #49. **Never `GETV_FPS=0` for play** (Cradle can freeze). | `90` |

Do **not** start with `GETV_STEREO_REBUILD=0` (breaks fusion). Do **not** turn off `GETV_XR_PLAY_SRCFBO` while SS>1. Do **not** arm census/trace knobs (`CULLWHY`, `ROOMTRACE`, …) for a wear sit.

---

## C. What “pass” looks like

- **Setup pass:** Steam Link 100% SS + small window + 72 Hz feels like the video; KEEP SS3 still sharp. → Close #60 as setup; leave #46/#49 open.
- **SS=1 pass, SS=3 fail:** fill/mirror. Product is #46 + a wireless SS preset — not a new renderer.
- **DRAWALL=0 pass only on Dam:** cull ticket, **not** the “whole game” #60 filing.
- **Hz pin pass:** send numbers to #49. Stay on vr441/vr442 public copy until HmdPace chairs.

---

## D. Notes testers often hit

- A console `60.0 fps` line is the **N64 VI clock**, not headset Hz.
- Public boot **pins 90**. Virtual Desktop at 72/80 can **fail to attach** (#49). Steam Link may still attach and then feel wrong.
- Stick turn smooth + head turn “not 90” can be the **60 sim / 90 display** interpolator. Say so; it is not a 3070-Ti failure.
- Dedicated 5 GHz is good and **does not** remove Steam Link encode.

No ROM uploads. Headset + runtime + bat + boot cmd + which row (A/B) changed the feel is enough.
