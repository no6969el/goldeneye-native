# RESULT — Play-on-monitor purple explosions (#51) (DIG ONLY)

**Status:** DIG. **Not APPLY READY on this public tree.** No C landed. No bat landed.
**Ask:** On public Latest **vr441** (still the public boot template; vr442 notes do not claim a flat-hue wear), `Play-on-monitor.bat` explosions look **purple / not bright**. Same cut in VR (`Start-GEVR.bat`) looks fine.
**Constraints:** Do not merge into #70 (modem flicker) or #55 (portal black). Do **not** C-default TEXINVAL / VFXTMEM / VFXSHIFT / TEX16BE **OFF**. Do **not** arm `GETV_RGBA16BE=1`. Do **not** make `Play-on-monitor.bat` call `gevr-*-boot.cmd` (pack smoke forbids it).
**Date:** 2026-09-20.
**Evidence:** public `goldeneye-native` HEAD (KEEP getenv stubs + `keep-defaults-on`), public `no6969el/GEVR` packaging + docs 237/238/239/244/252 + #51/#48, public `n64decomp/007` `explosion.c`. Workshop `gfx_pc.c` **bodies** (what TEX16BE / VFXTMEM actually do past the getenv) are **not on any public remote** (`GEVR` `docs/RELEASE-POLICY.md`).

Director can green-light bat-seed A, C-default B, both, or reject from this page alone.

---

## Verdict (one screen)

| Question | Answer |
|----------|--------|
| Root | **Launcher env branch, not a stereo shader.** Headset calls `gevr-vr441-boot.cmd` §9 (VFX/tex KEEP). Monitor bat does **not**. vr441 exe C-defaults those gates **OFF** (same `goldeneye.exe` as vr440). Flat therefore runs the Phase-0 purple flare. |
| Same as early purple (docs 237)? | **Yes, the hue.** Upstream-measured magenta `(153, 76, 168)` vs orange `(204, 157, 66)` is RGBA16 **byte-order**. Ship DECODE side is `GETV_TEX16BE=1`. Upload side `GETV_RGBA16BE` stays **0**. |
| Stereo / XR code gate? | **Unproven on public stubs.** `ge_tex16be` / `ge_vr_vfxtmem` / `ge_vr_vfxshift` / `ge_vr_texinval` are independent `getenv`s — they do **not** call `ge_stereo()` / `ge_xr_play()` in the public fragment. Chair run **T** is the falsifier. |
| Missing texture / pink checker? | **No.** Docs 229/244: not a missing bind. The flare is present and the wrong **endian**. |
| Same as #70? | **No.** Modem is `texSelect` monitor mode (`arg2` 1/2, `arg3` 8). Explosions are `explosion.c` `texSelect(..., genericimage, 4, 1, 2)`. Same `gfx_pc` neighborhood. **Do not** flip TEXINVAL OFF to “fix” either. |
| APPLY tonight? | **No from this repo.** Smallest wear is a **GEVR bat seed** (path A) or already-sketched **C-default ON** (path B). Workshop bodies stay private. |

```
Start-GEVR.bat
  → gevr-vr441-boot.cmd §9
  → TEXINVAL=1 TEXDLRETAG=1 VFXTMEM=1 VFXSHIFT=1
     TEX16BE=1 RGBA16BE=0 TEX32BE=1
  → explosion texSelect mode 4 decodes orange

Play-on-monitor.bat
  → setlocal; STEREO=0 STEREO_MODE=0 GE_VR_XR=0 FPS=60
     AUDIO_CLOCK=device AUDIO_QUEUE_MS=33   (#48 only)
  → does NOT call boot.cmd (smoke-forbidden)
  → VFX/tex KEEP left at exe C-default OFF (vr441/vr440 exe)
  → same flare, magenta / dim
```

---

## 1. Root finding (files + functions)

### 1.1 Evidence boundary

| Layer | Where | What it proves |
|-------|--------|----------------|
| Public `goldeneye-native` | this repo | KEEP getenv stubs in `getv/port/fast3d/gfx_pc.c`. `keep-defaults-on` flips those unset branches **ON** (vr441 exe still shipped **OFF**). Host blob `PexplosionbitZ` (288 B) is the extracted-C-array class docs 244 named. |
| Public `no6969el/GEVR` | `packaging/` + textbook | The **actual branch**: two bats. Inventory already parked this gap. Docs 237–244 measured the hue. Smoke **forbids** monitor → boot.cmd. |
| Public decomp | `n64decomp/007` `src/game/explosion.c` | Flare draw: `texSelect(&arg0, genericimage, 4, 1, 2)` (~1850). Scorch/impact is a different `texSelect` (~2230). |
| Workshop (private) | `F:\Projects\GEVR\GoldenEyeVR\goldeneye-native` | Real `ge_tex16be()` / `ge_vr_vfxtmem()` **call sites** inside `import_texture_rgba16` / TMEM. **Do not push.** |

`ge_tex16be` / `ge_vr_vfxtmem` / `ge_vr_vfxshift` **appear as getenv stubs only** on the public tree. First chair grep on the workshop: `getenv("GETV_TEX16BE")`, `getenv("GETV_VR_VFXTMEM")`, `import_texture_rgba16`.

### 1.2 The two launchers (the branch)

| | `Start-GEVR.bat` | `Play-on-monitor.bat` |
|--|------------------|------------------------|
| Calls `gevr-vr441-boot.cmd` | **Yes** | **No** (and must not; `_smoke-ship-zip.ps1` fails if it does) |
| `GETV_STEREO` / `GE_VR_XR` | `1` / `1` | **`0` / `0`** (FLAT_FORCE) |
| §9 VFX/tex KEEP | **Armed** | **Unset** |
| Audio pacing | boot | seeded inline (#48) |
| `GEVR_SHIP_TAG` | via boot | set on the bat (same stamp; cache is **not** the hue) |

Headset KEEP knobs are **not** run on the monitor bat unless the player exports them in the shell. Both bats `setlocal`, so a prior `Start-GEVR` in another window does not leak. Shared `%LOCALAPPDATA%\GEVR\cache` does not encode TEX16BE.

`GE_VR_XR` is a **no-op** in this binary (real arm is `GETV_VR`). It is kept only as the flat gate. Play-on-monitor does **not** set `GETV_VR=0`.

### 1.3 Boot §9 — the chair stack vr440 never armed

`gevr-vr441-boot.cmd` comment: vr440 shipped the **picture** KEEP only. Corpses, **explosion texture**, and PLAY0 were in the exe and the boot never turned them on. vr441 RELEASE-NOTES: *“Explosion and fire draw with the right texture and byte order.”*

| Knob | Headset boot | Monitor bat | vr441 C-default | Public stub unset |
|------|--------------|-------------|-----------------|-------------------|
| `GETV_VR_TEXINVAL` | `1` | unset | **OFF** | ON after `keep-defaults-on` |
| `GETV_VR_TEXDLRETAG` | `1` | unset | **OFF** | ON |
| `GETV_VR_VFXTMEM` | `1` | unset | **OFF** | ON |
| `GETV_VR_VFXSHIFT` | `1` | unset | **OFF** | ON |
| `GETV_TEX16BE` | `1` DECODE | unset | **OFF** | ON |
| `GETV_RGBA16BE` | **`0` pinned** | unset (=0) | OFF | **do_not_touch** (stay OFF) |
| `GETV_TEX32BE` | `1` | unset | OFF in public stub | listed ON in MANIFEST |
| `GETV_TILE1` / `GETV_BASETILE` | `1` | unset | unknown | not in MANIFEST |

Inventory label: `GETV_VR_TEXINVAL` = **“Explosion / fire texture path.”**

Boot interlock (verbatim): *“EXACTLY ONE 16-bit byte-order side may be on. DECODE side = TEX16BE.”*

Parked note in `KEEP-DEFAULTS-INVENTORY-vr441.md` (already filed this ticket’s shape):

> Flat purple explosions — headset TEXINVAL / byte-order KEEP is armed in vr441 boot; monitor path does not run that boot. Wrong-color explosions on flat remain a known gap; not closed by vr441 headset sit alone.

### 1.4 Why the hue is purple (measured, not guessed)

Docs **237** (Bunker 1, frame 680, chromatic pixels in the explosion):

| `GETV_RGBA16BE` | mean RGB | reads as |
|-----------------|----------|----------|
| **0** (then default) | **(153, 76, 168)** | **magenta** |
| **1** | (204, 157, 66) | orange |
| **2** (u16 swap control) | (206, 164, 71) | orange |

Ruled out there: paintball cheat, fog (three levels, three fog colours, same magenta), RGBA32 (zero uploads). *“Sparkly and purple”* is one cause: a 16-bit texel byte-order error scrambles adjacent texels (hue + speckle).

Docs **238**: chair glimpse, `=1` *“looks like it's working.”*

Docs **239**: **retracted** promoting `GETV_RGBA16BE=1` to `goldeneye.cfg` — it made *everything look weird*.

Docs **244**: the renderer read was already MSB-first and correct. `RGBA16BE=1` **undoes a corruption already in the texel data** (extracted `u32[]` C arrays reverse every 4-byte group on LE). ROM-blob textures are already right at `=0`. **A global import switch cannot be right for both.** Principled fix = normalise **at ingest**, like `ge_font_convert`. Ship workaround that survived: **DECODE-side `GETV_TEX16BE=1`**, import-side `RGBA16BE` **pinned 0**.

Docs **252**: purple with `RGBA16BE` commented out is *expected*, not a regression.

Public stub (`getv/port/fast3d/gfx_pc.c`):

```c
/* Reference port fragment: bool KEEP gates (explosion path, stereo, SrcFbo). */
static int ge_tex16be(void)  { /* unset → 1 after keep-defaults-on; vr441 exe was 0 */ }
static int ge_vr_vfxtmem(void);
static int ge_vr_vfxshift(void);
static int ge_vr_texinval(void);
static int ge_vr_texdlretag(void);
```

Those five getters do **not** consult `GETV_STEREO` / `GETV_XR_PLAY` in the public fragment. If workshop **call sites** wrap them in `if (ge_stereo())` / `if (ge_xr_play())`, path A/B both fail and run **T** says so.

### 1.5 Explosion draw (decomp)

`n64decomp/007` `src/game/explosion.c`:

- `explosionRenderPart` / prop explosion → `texSelect(..., genericimage, 4, 1, 2)` — **mode 4**, the fireball/flare.
- Impact/scorch → `texSelect(..., &impactimages[impact_type], ...)` — different images (docs 238: wall-hole rows 8..15 are the next RGBA16 census subject, not this ticket).

#70 DIG already split this: monitor `texSelect` is a different mode. Same KEEP names, different `arg2`.

Host-only blob `PexplosionbitZ` (288 B @ `0x007DDE60`) is the extracted-C-array class. GEVR Beta slices images from the player ROM into `%LOCALAPPDATA%\GEVR\cache` — provenance may be ROM-blob **or** a C-array leftover for the flare. Chair does not need to decide; TEX16BE DECODE is the wear that already makes VR orange.

---

## 2. Same as… — no

| | #51 flat purple | Early Phase-0 purple | #70 Dam modem | #48 flat audio |
|--|-----------------|----------------------|---------------|----------------|
| Symptom | Monitor **hue** (purple / dim). VR orange | Same hue, both paths | Texture **flicker** on a monitor quad | Desync, not colour |
| Branch | Bat / C-default | `RGBA16BE` off | Frustum `texSelect` + per-eye tick | Bat missing audio KEEP |
| Fix class | Seed VFX/tex KEEP on flat, or C-default ON | DECODE TEX16BE; do not global RGBA16BE | `MONFRAME` (already APPLY, default OFF) | Seed audio on the monitor bat |

#48 is the **precedent**, not the bug. They seeded `GETV_AUDIO_CLOCK=device` and `GETV_AUDIO_QUEUE_MS=33` on `Play-on-monitor.bat` instead of calling boot. Smoke then required those two lines.

---

## 3. APPLY sketches — **NOT LANDED**

### 3.1 Path A — bat seed (smallest zip wear, no rebuild)

**File:** `no6969el/GEVR` `packaging/templates/Play-on-monitor.bat` (and smoke).

**Do not** `call gevr-vr441-boot.cmd`. Smoke (`Test-MonitorBat`) fails if that string appears. Do not arm stereo, XR, hands, SS3, or PLAY0.

Add the **§9 explosion/tex subset only** (same values as headset boot):

```bat
rem Explosion / fire KEEP (vr441 boot §9). Do not call gevr-*-boot.cmd.
rem EXACTLY ONE 16-bit byte-order side. DECODE = TEX16BE. Not RGBA16BE.
set GETV_VR_TEXINVAL=1
set GETV_VR_TEXDLRETAG=1
set GETV_VR_VFXTMEM=1
set GETV_VR_VFXSHIFT=1
set GETV_TEX16BE=1
set GETV_RGBA16BE=0
set GETV_TEX32BE=1
```

Optional if run **S** says TEX16BE alone is not enough and TILE* were the miss (unlikely; not in the 237 measurement): `GETV_TILE1=1`, `GETV_BASETILE=1`.

Smoke follow-up (same shape as #48): require those `set` lines on `Play-on-monitor.bat`. Still forbid boot.cmd.

**APPLY READY?** On **GEVR packaging**, yes — a bat edit. **Not APPLY READY here** — this repo does not ship the player zip.

### 3.2 Path B — C-default ON (already sketched, not the vr441 exe)

`getv/patches/keep-defaults-on` already lists `GETV_TEX16BE`, `GETV_VR_VFXTMEM`, `GETV_VR_VFXSHIFT`, `GETV_VR_TEXINVAL`, `GETV_VR_TEXDLRETAG` as unset = ON. Public stubs match. **vr441/vr440 `goldeneye.exe` still defaults OFF** (that is why boot §9 exists).

After a cut whose exe actually has those C-defaults, path A becomes redundant **if** run **T** PASSes (no stereo wrap). Until then, do not claim `_smoke-keep-nobat` closed #51 — that smoke only checks the **string** is in the exe.

**Caution:** full KEEP graduation also C-defaults `GETV_VR` and `GETV_XR_PLAY` ON. Play-on-monitor does **not** pin those to 0 (only `GETV_STEREO` / `GE_VR_XR`). Path B for #51 must be the **VFX/tex subset**, not “graduate everything and hope flat stays flat.”

`GETV_RGBA16BE` stays in `do_not_touch` / DIG_OFF.

### 3.3 Path C — rejected

| Move | Why not |
|------|---------|
| `GETV_RGBA16BE=1` on the monitor bat or as C-default | 239/244. Fixes the flare, breaks ROM-blob textures. Double-swap if TEX16BE is also 1. |
| Call `gevr-*-boot.cmd` from Play-on-monitor | Smoke-forbidden. Arms stereo/XR/hands. |
| C-default TEXINVAL / VFX* **OFF** | vr440 lesson. #70 DIG already refused this. |
| Asset ingest normalise (244 §4) | Principled, not a tonight one-liner. Out of scope. |

### 3.4 Out of scope

- #70 `MONFRAME`, #55 `HEAD_TRANSLATE`, #29 crate fog
- `GETV_VR_BLOODINVAL`, `GETV_TMEMMAP`, FOGSKIP / DISTSKIP / WATERTILE / FOVMATCH
- Explosion **interpolation** (docs 23 — view-sticky fireball, not hue)
- Personal credit paths. ROM dumps

---

## 4. Chair stare — the falsifier (plain tester sentences)

**Setup:** public **vr441** zip first (the ticket cut). Repeat on **vr442** / Latest if you want to know whether C-defaults already moved. Recenter. Keyboard / pad on monitor. Rocket or grenade on a wall (Bunker 1 is the 237 level; any indoor wall is enough).

Write: bat used, whether Start-GEVR was ever run **in that folder**, GPU, headset/runtime if comparing VR. No ROM.

**Run F — flat baseline (reproduce #51):** `Play-on-monitor.bat` as shipped. No extra `GETV_*`.

| | Tester sentence |
|--|-----------------|
| **F-PASS (bug)** | “On the monitor, the explosion is **purple / dim / sparkly**, not a bright orange fireball.” |
| **F-FAIL (already fixed)** | “Monitor explosions already look like VR — orange, not purple.” → stop. Path A/B already in that cut, or a leftover user env is arming TEX16BE. |

**Run V — VR control:** `Start-GEVR.bat`, same map, same explosive.

| | Tester sentence |
|--|-----------------|
| **V-PASS** | “In the headset the same explosion is **orange / bright**. Not purple.” |
| **V-FAIL** | “VR is purple too.” → not this ticket’s branch. Check boot §9 actually assigned; then TEX16BE=0 accident / double-swap. |

**Run A — path A on flat (one change):** same `Play-on-monitor.bat` process, then in that window (or a scratch bat that still has `STEREO=0` / `GE_VR_XR=0` and does **not** call boot):

```
set GETV_VR_TEXINVAL=1
set GETV_VR_TEXDLRETAG=1
set GETV_VR_VFXTMEM=1
set GETV_VR_VFXSHIFT=1
set GETV_TEX16BE=1
set GETV_RGBA16BE=0
set GETV_TEX32BE=1
```

| | Tester sentence |
|--|-----------------|
| **A-PASS** | “Monitor explosion is now **orange**, like VR. Still no headset session.” |
| **A-FAIL** | “Still purple with the whole §9 stack on and stereo still 0.” → stereo/XR **call-site** wrap (run **T**), or TILE*/ingest. |

**Run S — split (TEX16BE alone):** flat, only `GETV_TEX16BE=1` and `GETV_RGBA16BE=0`. VFX* unset.

| | Tester sentence |
|--|-----------------|
| **S-PASS** | “TEX16BE alone turned it orange.” → bat/C-default can be **TEX16BE + RGBA16BE=0** only. |
| **S-FAIL** | “Still purple until VFXTMEM / VFXSHIFT / TEXINVAL are also 1.” → keep the full §9 seed. |

**Run T — stereo-gate falsifier (the important one):** `Start-GEVR.bat`, then **wipe only** the VFX/tex stack (leave `GETV_STEREO=1` / XR play on):

```
set GETV_TEX16BE=0
set GETV_VR_VFXTMEM=0
set GETV_VR_VFXSHIFT=0
set GETV_VR_TEXINVAL=0
set GETV_VR_TEXDLRETAG=0
```

| | Tester sentence |
|--|-----------------|
| **T-PASS (confirms env branch)** | “Headset explosions went **purple** with those five at 0. Stereo was still on.” → VR looks fine **only** because boot assigns KEEP. No hidden stereo shader. Path A/B are enough. |
| **T-FAIL** | “Headset stayed orange with those five at 0.” → some other boot knob (TILE1 / BASETILE / TEX32BE) or a leftover cfg. Add those to the next A. |

**Run R — anti-RGBA16BE (do not ship):** flat, `GETV_RGBA16BE=1` and `GETV_TEX16BE=0`.

| | Tester sentence |
|--|-----------------|
| **R-expect** | “Flare may go orange, but **other** textures look wrong / swapped.” → 239/244. Leave RGBA16BE=0. |
| **R2 double-swap** | Both `TEX16BE=1` **and** `RGBA16BE=1` → “Purple again.” Interlock holds. |

**Run C — C-default (only on a post-graduation exe):** `Play-on-monitor.bat`, **no** VFX/tex `set`s. If F is already orange, B shipped. If F is still purple, that exe did not graduate those gates (or run T-FAIL’s wrap is real).

**Regression every run:** no OpenXR requirement on the monitor bat. Audio still in sync (#48). Headset sit still orange with stock `Start-GEVR.bat`. Dam modem (#70) not in scope — do not flip TEXINVAL OFF if a monitor flickers.

---

## 5. Director decision

| If you say… | Then… |
|-------------|--------|
| **Green A (bat seed)** | GEVR `Play-on-monitor.bat` += §9 explosion/tex lines. Smoke requires them. No boot.cmd. Sit F → A (+ S if you want the minimal set). |
| **Green B (C-default)** | Already sketched in `keep-defaults-on`. Ships only with a **new exe**. Sit C on that cut. Do not C-default `GETV_XR_PLAY` as the #51 fix. |
| **Green A + B** | Bat unblocks vr441-class zips tonight. C-default makes A redundant later. Prefer this if vr441/vr442 exe is still unset=OFF. |
| **Green RGBA16BE=1** | Rejected. |
| **Call boot.cmd from the monitor bat** | Rejected. |
| **Flip TEXINVAL C-default OFF** | Rejected (#70 / vr440). |
| **APPLY tonight from this repo** | No. RESULT only. A lives in `no6969el/GEVR` packaging. B is already a public patch waiting on the product exe. |
| **Reject — not the hue** | Only if F-FAIL (flat already orange) or T-FAIL after TILE* too. |

---

## 6. Attribution / legal

- No personal credit paths edited.
- No GoldenEye ROM, assets, or dumps.
- Decomp citations are public `n64decomp/007` `explosion.c` (`texSelect` mode 4).
- Textbook citations are public `no6969el/GEVR` docs 237–244, 252, packaging templates, #51.
- Workshop C stays private until release policy flips.
