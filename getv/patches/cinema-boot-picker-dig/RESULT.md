# RESULT — in-HMD cinema vs gameplay-VR boot picker (DIG ONLY)

**Status:** DIG. **Not APPLY READY on this public tree.** No C landed.
**Ask (GEVR public tracker, not this repo):** a choice **in the HMD at boot** — watch intro / menus on the world-space big screen, or drop straight into gameplay VR. Cinema billboard already exists. There is no in-headset picker yet.
**Wear stays vr441-class / public Latest.** Do not ship a picker ON without a chair sit.
**Date:** 2026-09-20.
**Evidence:** public `goldeneye-native` HEAD, public `no6969el/GEVR` textbook + vr441 boot / FEATURES / CONTROLS, public PD `vr_hub.cpp` (map-only). Workshop cinema C (`GETV_XR_PLAY_*` readers) is **not on any public remote**. Brief names below are the **getenv strings** plus GEVR textbook symbols. First chair grep those strings; do not type aliases until the live names match.

Director can green-light env-only skip, in-hub picker, pause toggle, or none from this page alone.

---

## Verdict (one screen)

| Question | Answer |
|----------|--------|
| Cinema already in? | **Yes.** World-locked big screen in a small hub. Boot: `GETV_XR_PLAY_SCREEN=2`, `GETV_XR_PLAY_AUTOSCREEN=1`, `GETV_XR_PLAY_FOVSCALE_CINEMA=85`. |
| In-HMD picker tonight? | **No.** Autorecenter into gameplay when you leave cinema. No boot choice. |
| Smallest overall | **A — env bat.** Scratch skip / force cinema. No new UI. |
| Smallest in-HMD | **B — two touch pads in the existing hub.** Reuse `TOUCHUSE`. |
| Pause / watch option | **C — park for boot.** `#32` confirm is still rough. Pause is after you already picked a mode. |
| Falsifier | **`GETV_XR_PLAY_PICKER` default OFF.** Unset / empty / `0` = tonight. Explicit `1` = show pads. |
| Skip without picker | First sit existing `PLAY_SCREEN` / `AUTOSCREEN`. Mint `GETV_XR_PLAY_SKIPCINEMA` only if those do not drop into gameplay VR. Default OFF. |
| KEEP-ON / boot allowlist | **No.** Not `$requiredBootKnobs`. Not C-default ON. No ship ON without chair. |
| APPLY tonight? | **No from this repo.** Bodies are private. Layer A may be a scratch bat with **zero C** if an existing knob already skips cinema. |

```
boot (GETV_XR_PLAY=1, AUTOSCREEN=1, SCREEN=2, FOVSCALE_CINEMA=85)
  frontend / intro / menus  → world-locked cinema in small hub
  enter gameplay            → recenter; full stereo VR  (CONTROLS.md)

PICKER=0 (unset)            → that path, no pads
PICKER=1                    → two hub pads: CINEMA (stay) vs VR (skip billboard)
SKIPCINEMA=1 (scratch)      → skip billboard this launch; no pads
```

---

## 1. Root finding (files + knobs)

### 1.1 Evidence boundary

| Layer | Where | What it proves |
|-------|--------|----------------|
| Public `goldeneye-native` | this repo | Host-agnostic VR ABI (`ge_vr.h`). Reference `getv/port/*` KEEP fragments. **`OPTION_SCREENCINEMA` in `file2.h` is N64 letterbox, not the VR cinema.** No `FOVSCALE` / `PLAY_SCREEN` C. |
| Public `no6969el/GEVR` | docs + `packaging/` | Ship knobs, hub copy, chair refusals. Issue **#42** lives here. |
| Workshop (private) | `F:\Projects\GEVR\GoldenEyeVR\goldeneye-native` | `GETV_XR_PLAY_*` readers, hub draw, cinema FOV scale. **Do not push.** |
| PD (MIT, map-only) | `Alex-LeTux/perfect_dark_VR` `port/vr/vr_hub.cpp` | Decorative pause **room** (floor + sky, 12 verts). **Does not blit the game.** |

`GETV_XR_PLAY_FOVSCALE_CINEMA` / `GETV_XR_PLAY_SCREEN` / `GETV_XR_PLAY_AUTOSCREEN` **do not appear in any public C file in this repo.** First chair grep: those three `getenv` strings (plus `GETV_XR_PLAY`, `GETV_XR_PLAY_AT`, `GETV_VR_HUB`). GEVR docs put other play/stereo helpers in workshop `getv/port` and `vendor/ge-decomp/src/game/stereo.c`.

### 1.2 Do not confuse these four “cinema / FOV” things

| Name | What it actually is | This ask? |
|------|---------------------|-----------|
| **`GETV_XR_PLAY_FOVSCALE_CINEMA=85`** | PLAYER_PREF. Scales the **world-locked cinema frustum** so intro/menus sit on a big screen, not a face-fill. vr441 boot. | **Yes — the billboard.** |
| **`GETV_XR_FOVMATCH`** | Forbidden falsifier. Camera rewrite. Wiped in boot §0. Smoke fails if armed. | **No. Leave wiped.** |
| **`GETV_XR_FOVSYM=1`** | KEEP_SHIP. Symmetric eye FOV. | No. Do not flip. |
| **`OPTION_SCREENCINEMA 0x0800`** | Stock GoldenEye **letterbox** save bit (`file2.h`). Dam “cinema entry” `znear=30` in `VR-PLAN.md` is **fog**, not the hub. | **No.** |

Architecture (`GE007-VR-ARCHITECTURE.md` §9): cutscenes that yank the view are the hard comfort case; **“play cutscenes on a 2D screen” is the default.** That is this cinema path. Full-VR intro cameras are the nauseating option, not the ship default.

### 1.3 Cinema / billboard / hub as it ships (vr441 boot, Latest copy)

Public player copy (GEVR `FEATURES.md`, `CONTROLS.md`):

> Menus and intro cinema sit on a screen in a small hub room. Look left and right — the screen stays nailed in space; you are not wearing a billboard on your face.
>
> Recenter is the same recenter the game uses when you enter gameplay from the cinema / menu.

| Knob | Boot | Class | Role |
|------|------|-------|------|
| `GETV_XR_PLAY` | `1` | KEEP_SHIP (also C-default ON in public KEEP manifest) | Play / cinema arm. **Do not flip.** |
| `GETV_XR_PLAY_AT` | `300` | PLAYER_PREF | Timing / latch (name only on public tree). Confirm at grep. |
| `GETV_XR_PLAY_STEREO` | `1` | KEEP_SHIP | Stereo play. |
| `GETV_XR_PLAY_SCREEN` | `2` | KEEP_SHIP | Cinema / screen **mode**. `2` is the shipped world-lock. **`0` / `1` are unread here.** |
| `GETV_XR_PLAY_AUTOSCREEN` | `1` | KEEP_SHIP | Auto cinema on frontend / intro / menus; drop to gameplay VR when an eye pair exists. **Tonight’s “no picker”.** |
| `GETV_XR_PLAY_FOVSCALE_CINEMA` | `85` | PLAYER_PREF | Billboard scale. 85 = big screen, not HMD-fill. |
| `GETV_XR_PLAY_AUTORECENTER` | `1` | KEEP_SHIP | Recenter into play (C-default ON). |
| `GETV_VR_HUB` | wipe only | C_DEFAULT UNKNOWN | Fancier PD-style room. **Not armed.** FEATURES: “fancier hub room are later.” |
| `GETV_VR_TITLEBG` | wipe only | C_DEFAULT UNKNOWN | Title-walk backdrop. Not this picker. |
| `GETV_CINETRACE` / `GETV_FRONTTRACE` | wipe | DIG_OFF / smoke-forbidden | Chair instruments only. |

U-19 / `docs/175` (older RT64 host, still the mechanism): when there is **no eye split** (frontend, menus, cutscenes, opening frames), draw a **real-depth rectangle** from the flat present target. Red/blue remains “no image.” U-20 (`docs/169`): PD hub is **two more quads behind that screen**. GEVR already shipped a **small** hub; `GETV_VR_HUB` is the parked fancy one.

`PLAY_SCREEN` / `AUTOSCREEN` / `FOVSCALE_CINEMA` are **not** in pack `$requiredBootKnobs` (39-knob smoke). A scratch bat may override them without a pack-smoke edit. They are still live KEEP / pref on the headset boot — **do not change the public boot** until a sit PASSes.

### 1.4 Perfect Dark room — what transfers, what does not

PD `vr_hub.cpp` (read 2026-09-20, `port` branch):

- Pause-menu **environment only**. Floor grid + gradient sky + logo decal. `vr_hub_render(eyeViewProj[2][16])`.
- **Never displays the game image.** GEVR `docs/169` U-19 said this out loud. Do not copy PD sources.
- Design that transfers: world-locked surround **behind** the cinema quad; per-eye view-proj; no game-state dependency for the room itself.
- Design that does **not** transfer as a boot picker: PD hub is **pause**, not a boot chooser. Colors flip on end-screen win/lose. OpenGL / GLSL vs our D3D12 XR layer.

GEVR already has the screen (U-19 lineage) plus a small room. **This ask is a latch on top of that, not a new hub.**

### 1.5 Not this ask

| Thing | Why parked |
|-------|------------|
| `docs/208` `-AutoLevel` | Skip menus because the front end crashed. Hook was inert. **Not** “cinema vs VR.” |
| `#32` pause confirm / `#58` highlight | Face-button / first-eye toggle debt. Blocks **C**. |
| `#57` arm-watch panel | Different DIG. Forearm HUD later. |
| `GETV_STAGE` / `GETV_FRONTTRACE` / `GETV_CINETRACE` in a public boot | Smoke-forbidden. Chair only. |
| Full colocated Bond / fancy hub | ROADMAP later. |

---

## 2. Rank — smallest picker

Goal of #42: **a choice in the HMD at boot.** “Always skip cinema” / “always big screen” can also be an env for people who never want to pick.

| Rank | Path | New C? | In-HMD? | Reuses | Risk |
|------|------|--------|---------|--------|------|
| **1 — A env bat** | Scratch override of cinema vs skip | **Maybe none** | No | Existing `PLAY_SCREEN` / `AUTOSCREEN` (sit first) | Wrong enum → red/blue or face-billboard |
| **2 — B in-room pads** | Two `TOUCHUSE` volumes in the **existing** hub | Yes, small | **Yes — this is the ask** | `GETV_VR_TOUCHUSE` + `_R=12`, hub pose | Double-fire per eye (`#32` trap); pads in gameplay |
| **3 — C pause / watch** | New pause row | Yes, and `#32` | After pause | Watch / Menu button | Confirm still rough; too late for **boot** |

**Recommend A then B.** A answers “I always skip” / “I always want the screen” with a bat. B is the smallest **in-HMD** chooser. C is a later **session** toggle after confirm PASSes — not the boot picker.

Do **not** build a new PD-style room, a mods menu (`U-04`), or `geVrWatchGestureActive` for this.

### 2.1 Why B beats C for “at boot”

- The player is **already** in the hub looking at the cinema. Two pads sit in that room.
- Pause is Menu / system button (`CONTROLS.md`; vr440 snippet mapped B→Start, later ship moved pause off B). You open it **after** cinema or **after** gameplay. That is a preference toggle, not a boot menu.
- `#32`: A beeps / double-toggles / Yes-No fails in stereo. Putting the first cinema-vs-VR choice on that path inherits the debt.

### 2.2 Why A is still the first sit

If `PLAY_SCREEN=0` or `AUTOSCREEN=0` already means “no billboard, gameplay VR,” the always-skip crowd needs **zero C**. Chair that before minting `SKIPCINEMA`. A new getenv that duplicates a live enum is how this project ships dead names (`GETV_SRCFBO` / `GETV_MSGSCALE`).

---

## 3. APPLY sketches — **NOT LANDED**

Workshop only. Public `goldeneye-native` has no play path to patch.

### 3.0 First chair — no rebuild (do this before any C)

vr441-class zip. Recenter both sticks. **Do not** arm `FOVMATCH`, `CINETRACE`, `FRONTTRACE`, `STAGE` on a pack boot (scratch bat is fine for traces).

| Run | Scratch | Tester sentence |
|-----|---------|-----------------|
| **A0 — ship** | vr441 boot as-is | “Intro / menus are a **screen in a room**. Head turn does not carry the picture on my face.” |
| **A1 — AUTOSCREEN off** | `GETV_XR_PLAY_AUTOSCREEN=0` | Either “I dropped into gameplay VR / menus are in-world stereo” **or** “red/blue / face-fill / broken frontend.” Write which. |
| **A2 — SCREEN modes** | `GETV_XR_PLAY_SCREEN=0` then `=1` (keep `=2` as control) | Name what `0` and `1` **do**. If one of them **is** skip-cinema, **do not mint `SKIPCINEMA`.** |
| **A3 — FOVSCALE** | `GETV_XR_PLAY_FOVSCALE_CINEMA=50` then `100` | “The screen got smaller / larger. Gameplay VR after a mission start is unchanged.” |

**A-PASS (env-only product):** one existing assign skips cinema **and** leaves gameplay VR intact. Document it. Stop. Optional: a second player bat, not a KEEP flip.

**A-FAIL:** no existing assign means “drop into gameplay VR” without breaking frontend. Then mint 3.1.

Grep after A0 (workshop):

```
getenv("GETV_XR_PLAY_SCREEN")
getenv("GETV_XR_PLAY_AUTOSCREEN")
getenv("GETV_XR_PLAY_FOVSCALE_CINEMA")
getenv("GETV_XR_PLAY")
getenv("GETV_VR_HUB")
```

Tick cinema vs play on **sim-owner / first eye only** (`lvframe60` / `#32` first-eye gate). Autorecenter already fires on cinema→gameplay; a skip must not recenter twice or never.

### 3.1 Layer A — skip knob (only if A1/A2 fail)

```
GETV_XR_PLAY_SKIPCINEMA   unset / empty / 0 = OFF   (cinema as tonight)
                          1 = skip billboard this process
GETV_XR_PLAY_PICKER       unset / empty / 0 = OFF   (no pads)
                          1 = in-hub pads (3.2)
```

Both: **DIG_OFF.** Not KEEP-ON. Not `gevr-*-boot.cmd`. Not `MANIFEST.json` `bool_on_unset`. Scratch: `set GETV_XR_PLAY_SKIPCINEMA=1`.

```c
/* NOT APPLY READY — workshop sketch.
 * Default OFF. Same ternary shape as GETV_VR_TWOHAND / GETV_INVERTLOOK digs. */

static int ge_xr_play_skipcinema(void)
{
    static int on = -1;
    if (on < 0) {
        const char *e = getenv("GETV_XR_PLAY_SKIPCINEMA");
        on = (e != NULL && *e != '\0') ? (atoi(e) != 0) : 0; /* dig default OFF */
    }
    return on;
}
```

Wire at the **same site** that honors `AUTOSCREEN` / `PLAY_SCREEN` (grep those). Skip = take the gameplay-VR branch for frontend / intro (or skip intro cinema and hold file-select in VR — wearer names it on A1). Do **not** call `bossSetLoadedStage` / `#208` AutoLevel. Do **not** write `GETV_XR_PLAY=0`.

**APPLY READY?** Only after A1/A2 fail **and** the getenv site is named. **Not APPLY READY here.**

### 3.2 Layer B — in-hub pads (`PICKER` default OFF)

**File:** workshop hub / cinema draw next to `PLAY_SCREEN` (same TU as the billboard). Pads are **world-locked** in hub space, not head-locked.

Reuse:

| Helper | Ship | Use |
|--------|------|-----|
| `GETV_VR_TOUCHUSE` + `_R=12` | ON | **Pattern + radius.** Door poke. |
| `geTouchBoxDist` (workshop) | — | Hand vs pad box. Own enter/exit if it chatters. |
| `geVrHandIsTracked` | ABI | No stale floor poke. |

```c
/* NOT APPLY READY — workshop sketch.
 * Two pads only while cinema/hub is up. Latch once per boot.
 * First-eye / sim-owner only — do not toggle twice per frame. */

static int ge_xr_play_picker(void)
{
    static int on = -1;
    if (on < 0) {
        const char *e = getenv("GETV_XR_PLAY_PICKER");
        on = (e != NULL && *e != '\0') ? (atoi(e) != 0) : 0; /* dig default OFF */
    }
    return on;
}

/* if (!ge_xr_play_picker()) return;          -- tonight
 * if (!cinemaOrHubThisFrame()) return;       -- never in a mission
 * if (latched) apply(latchedChoice); return;
 * pokeL = touch(padCinema); pokeR = touch(padVr);
 * if (pokeR && !pokeL) { latched = VR; skipCinema(); }
 * if (pokeL && !pokeR) { latched = CINEMA; stay(); }
 */
```

**Smallest draw:** two `HANDCUBES`-sized boxes or two text quads already used by the cinema path. Do not author a new room. Do not enable `GETV_VR_HUB`.

Labels the wearer must be able to read without `#32`: **CINEMA** / **VR** (or “BIG SCREEN” / “PLAY”). Confirm is **touch**, not face-button A.

**APPLY READY?** **No.** Hub pose + pad frames + one-shot latch + “not in gameplay” gate need a sit.

### 3.3 Layer C — pause row (out of scope for boot)

A pause option that writes the same latch as 3.1 is a **later** session toggle. Blocked on `#32` confirm + watch distance. Do not APPLY. If someone asks later: same `SKIPCINEMA` bit, default still OFF, still not KEEP.

### 3.4 Out of scope

- `GETV_XR_FOVMATCH`, `GETV_XR_FOVSYM` flips
- `OPTION_SCREENCINEMA` / Dam cinema fog
- `GETV_VR_HUB=1` / themed U-20 art
- `#208` autolaunch stage
- KEEP-ON graduation of `PICKER` / `SKIPCINEMA`
- Boot allowlist / pack smoke until a sit PASSes
- Personal credit paths. ROM dumps. PD source copies

---

## 4. Chair stare (plain tester sentences)

**Setup:** vr441-class / Latest zip. `Start-GEVR.bat`. Recenter both sticks. Wear stays this cut.

**Run A — env (no picker):** `GETV_XR_PLAY_PICKER` **unset**.

| | Tester sentence |
|--|-----------------|
| **A-PASS 1** | “Stock boot: intro and menus are a **screen in a little room**. I can look left; the screen stays put.” |
| **A-PASS 2** | “When I start a mission, I **recenter into the world**. Guns and rooms are full VR, not a billboard.” |
| **A-PASS 3** | “Scratch skip (whatever A1/A2 named): I **do not** sit in cinema. I am in gameplay VR (or in-world menus). Unset skip brings cinema back.” |
| **A-FAIL** | “Skip made red/blue.” / “Menus glued to my face.” / “Gameplay stayed a billboard.” / “I skipped all the way into Dam via AutoLevel.” |

**Run B — pads on:** same boot plus `GETV_XR_PLAY_PICKER=1`. `SKIPCINEMA` unset.

| | Tester sentence |
|--|-----------------|
| **B-PASS 1** | “In the hub I see **two things I can poke**: cinema vs VR. Unset `PICKER` they are gone.” |
| **B-PASS 2** | “Poke **VR**: billboard goes away; I am in gameplay VR. I do not have to poke every boot after that **this process**.” |
| **B-PASS 3** | “Poke **CINEMA**: I stay on the big screen. Starting a mission still drops me into VR.” |
| **B-PASS 4** | “In a mission the pads are **gone**. I cannot poke them mid-Facility.” |
| **B-PASS 5** | “Hovering the edge does **not** flicker cinema on and off.” |
| **B-FAIL** | “No pads.” / “Pads in the mission.” / “Poke fires twice / flickers.” / “Unset `PICKER` still shows pads.” / “I grew a fancy hub I did not ask for.” |

**Regression (every run):** squeeze ADS on the **gun ray**, touch-use doors, both-stick recenter, no `FOVMATCH`. Cinema scale 85 still reads as a screen, not a wall, on the cinema choice.

---

## 5. Director decision

| If you say… | Then… |
|-------------|--------|
| **Green A only** | Sit 3.0. If an existing knob skips, ship a **scratch / optional bat**, not a KEEP change. If not, workshop 3.1 only. `PICKER` stays off. |
| **Green A + B** | Same, plus 3.2. Sit A then B. Pads default OFF. |
| **Green B, skip A** | Allowed if you only want the in-HMD chooser. Still need a skip **bit** behind the VR pad (3.1 body, no env required if picker is the only writer). |
| **Green C** | Rejected for boot. Re-open after `#32` PASS as a session toggle. |
| **Ship PICKER=1 in boot** | **No** until B sits PASS. Then still PLAYER_PREF / optional, not KEEP-ON. |
| **Always skip cinema for everyone** | Comfort fight. Architecture default is 2D cinema. Needs a chair, not a silent KEEP flip. |
| **Reject all** | Stop. Tonight’s AUTOSCREEN cinema stays. |

---

## 6. Attribution / legal

- GEVR #42 is the public ask. This repo is code + DIG notes only. Do not file `#42` here.
- No personal credit paths edited.
- No GoldenEye ROM, assets, or dumps.
- PD hub numbers / layout are **map-only**. Do not copy `vr_hub.cpp`.
- Workshop C stays private until release policy flips.
