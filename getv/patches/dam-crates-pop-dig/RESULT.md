# RESULT — Dam crates / props pop at mid range (DIG ONLY)

**Status:** DIG. **Not APPLY READY on this public tree.** No C landed.
**Ask:** GEVR [#29](https://github.com/no6969el/GEVR/issues/29) — Dam (first level) mid-range crates and props pop in and out. Related **VISFAR** was **far guards** (`MaxVisRange` on the chr draw). This ticket is **props / crates**. Prefer **widen the shared visibility / cull path** over per-object exceptions. **`FOVMATCH` stays off.**
**Date:** 2026-09-20.
**Evidence:** public `goldeneye-native` HEAD (KEEP fragments only), public `no6969el/GEVR` textbook + vr441 boot + issue #29 chair notes, public `n64decomp/007` `bgfog.c` / `propobj.c` / `chr.c` / `UsetupdamZ.c`. Workshop C (`GETV_VR_PROPFOGALPHA`, `GETV_VR_OCCLSKIP`, `GETV_VR_VISFAR` bodies) is **not on any public remote** (`GEVR` `docs/RELEASE-POLICY.md`). Brief names are the workshop knobs; sites below are the public decomp functions they wrap.

Director can green-light shared widen, keep the already-passed skip pair, leftover fog-colour, or none from this page alone.

---

## Verdict (one screen)

| Question | Answer |
|----------|--------|
| Same as VISFAR / 007 far-vis? | **No.** Same `NearFogRecord.MaxVisRange` **number**. VISFAR wore on **`chrRenderProp`**. This is **`chrobjRenderProp`** + the **admit** that sets `PROPFLAG_ONSCREEN`. vr442 already lists far guards as improved and **still** lists Dam crates as a quirk. |
| Same as #70 modem / #55 black? | **No.** #70 is a tagged monitor quad. #55 is portal / `HEAD_TRANSLATE` full black. Do not merge. |
| Why Dam, not Facility? | Dam `fog_tables[]` has a **live** Nfd: `NearFog=3333`, `MaxVisRange=4444`, `MaxObfuscationRange=600`. Facility `NearFog=0` → `fogLoadCurrentEnvironment` sets `g_NearFogValuesP = NULL` → both shared testers no-op. |
| Shared path (prefer this) | **`fogGetNearFogValuesP()`** → **`chrobjFogVisRangeRelated`** (draw fade / 0-alpha skip) **and** **`sub_GAME_7F054C58`** inside **`posIsOnScreen`** (admit snap-off). One helper, two consumers. Guards already call the first from `chrRenderProp`. |
| Per-crate list? | **Reject.** Dam is full of `PROP_WOOD_SM_CRATE4` (82) and `PROP_OIL_DRUM7` (62). Size already biases small props earlier (`* 100 / getinstsize`). Do not `if (modelnum == WOOD_SM_*)`. |
| Chair keep (already PASSed) | **`GETV_VR_PROPFOGALPHA=0` + `GETV_VR_OCCLSKIP=1` together.** OCCLSKIP alone left the ~1% fade. Public this tree already C-defaults both. Next zip hard-wires; do not invent a third skip. |
| Leftover after that keep | **Fog colour lerp + a second alpha** (`fogGetPropDistColor` → `fogcolour` / `G_RM_FOG_PRIM_A`), not another admit skip. Chair 2026-09-17. |
| `FOVMATCH`? | **Stays off.** Headset FOV would move `c_lodscalez` (`currentPlayerSetCameraScale`) and the whole frustum. Not a crate knob. |
| Do not wear | `GETV_VR_PROPFOGW` (443). FOGSKIP / DISTSKIP / WATERTILE. 434 / 435. `VISFAR` as a crate-only pin. |

```
Facility: Nfd = NULL                         → crates stay (no fade, no Nfd admit cull)
Dam:      Nfd live (3333 / 4444 / 600)

admit:  posIsOnScreen(..., applyFogCull)
          fogPositionIsVisibleThroughFog     (zfar / intensity)
          sub_GAME_7F054C58                  (same Nfd as draw)   ← OCCLSKIP sits here
          room / box / 32000

draw:   fogGetPropDistColor                  (RGB + fog α; return 0 = gone)
        chrobjFogVisRangeRelated             (objAlpha 255→0)     ← PROPFOGALPHA sits here
        lerp → fogcolour                     (leftover tint / 2nd α)
```

---

## 1. Root finding (files + functions)

### 1.1 Evidence boundary

| Layer | Where | What it proves |
|-------|--------|----------------|
| Public `goldeneye-native` | this repo | KEEP fragments: `GETV_VR_PROPFOGALPHA` unset → `0`, `GETV_VR_OCCLSKIP` unset → `1`. Manifest already refuses `FOVMATCH` / `PROPFOGW` / `FOGSKIP` / `DISTSKIP`. |
| Public `no6969el/GEVR` | docs + `packaging/` + issue #29 | Chair PASS on the two knobs. VISFAR wiped in vr441 boot. Far-vis shipped for **guards** on vr442. `FOVMATCH` forbidden in smoke. |
| Workshop (private) | `F:\Projects\GEVR\GoldenEyeVR\goldeneye-native` | Actual `getenv` sites on the decomp. **Do not push.** |
| Public decomp | `n64decomp/007` | The shared Nfd path below. Line numbers are that tree. |

### 1.2 Why Dam is the first-level repro

`bgfog.c` NTSC `fog_tables[]` (`LEVELID_DAM`):

| Field | Dam | Facility |
|-------|-----|----------|
| `Visibility.FarFog` (zfar) | 15000 | 5000 |
| `Nfd.NearFog` | **3333** | **0** → `g_NearFogValuesP = NULL` |
| `Nfd.MaxVisRange` | **4444** | 0 |
| `Nfd.MaxObfuscationRange` | 600 | 0 |
| Fog RGB | `0x10, 0x30, 0x60` | `0x10, 0x20, 0x10` |
| `bg.c` visibility scale | **0.2** | 1.0 |

Fade band on the raw Nfd is only **1111** units (3333→4444). Dam pads already span thousands of units (e.g. `{3807,-18,2997}` vs `{−1374,-18,−779}`). “Mid range” on the first walk **is** that band.

`UsetupdamZ.c` `StandardProp` rows are mostly **`PROP_WOOD_SM_CRATE4` (82)** and **`PROP_OIL_DRUM7` (62)** — small `getinstsize` (`BoundingVolumeRadius * scale`). The shared tester **punishes small models first**:

```
temp = ((zDepth - MaxObfuscationRange) * 100 / size + MaxObfuscationRange) * c_lodscalez
invisible when temp >= MaxVisRange
```

A small crate crosses 4444 much closer than a guard body. That is why VISFAR-on-chr can look “fixed” while Dam crates still pop.

### 1.3 Shared visibility / cull path (widen **this**)

One record, two functions, three call sites:

| Site | File | Job |
|------|------|-----|
| `fogGetNearFogValuesP` | `bgfog.c` | Returns Dam’s `NearFogRecord` (or NULL). |
| `chrobjFogVisRangeRelated` | `propobj.c` | `0..1` fade. `0` → skip draw (`objAlpha <= 0`). |
| `sub_GAME_7F054C58` | `propobj.c` | Same Nfd test, boolean. `posIsOnScreen` uses it when `applyFogCull`. |
| `chrRenderProp` | `chr.c` | **Guards.** Multiplies `chr->fadealpha` by the shared fade. VISFAR / 007 far-vis lives here (or on the Nfd the chr path reads). |
| `chrobjRenderProp` | `propobj.c` | **Crates / drums / props.** Writes `objAlpha` into `envcolour` (`PropType = 5` = `PROP_TYPE_PLAYER` when faded). |
| `posIsOnScreen` | `propobj.c` | Admit. Room rendered + fog-through + **Nfd** + screen box + `32000²`. False → clear `PROPFLAG_ONSCREEN` → pop. |

`applyFogCull` is **TRUE** for ordinary props (crates, drums). Glass turns it off. Guards call `posIsOnScreen(..., 1)` from `chr.c` as well.

**Preferred APPLY:** one helper around the Nfd distance (the `temp_f12` / `sp20` scale). Raise **`MaxVisRange` and `NearFog` together** (keep the fade band, move it out), or multiply both by a shared scale. Call it from **both** `chrobjFogVisRangeRelated` and `sub_GAME_7F054C58`. Guards inherit the draw fade for free. Do **not** add a `PROP_WOOD_*` / Dam-only model list.

If workshop VISFAR already scales `MaxVisRange` only inside `chrRenderProp`, **fold that scale into the helper** and leave VISFAR as a wiped dig pin. That is the “widen shared, not per-object” ask.

Do **not** do this via `GETV_XR_FOVMATCH`. `c_lodscalez = c_scalelod / c_scalelod60` and `c_scalelod` is `c_scaley` from `c_perspfovy`. FOV-match would retune every LOD and the frustum to hide a 4444-unit Nfd. Chair already refused it (`FEATURES-CURRENT.md`).

Already-shipped widen that is **not** this path (leave them):

| Knob | vr441 boot | What it actually is |
|------|------------|---------------------|
| `GETV_VR_DRAWALL=1` | KEEP | Rooms, not Nfd. |
| `GETV_VR_CULLWIDE=3.0` / `SCREENWIDE=3.0` | PLAYER_PREF | Frustum / screen box. |
| `GETV_VR_LODDIST=0.25` | PLAYER_PREF | LOD distances, not `MaxVisRange`. |
| `GETV_PROPCULLBOX=0` | DIG_OFF | Per-room rectangle in `posIsOnScreen`. Planes unreachable when this is on (`288` / `289`). Not the Dam fade. |

### 1.4 Two alphas + leftover fog colour

`chrobjRenderProp` writes **two** colours after the Nfd fade:

1. **`objAlpha = fade * 255`** → `mrData.envcolour.word = objAlpha` when `< 255`. That is the ~1% ghost draw. **`GETV_VR_PROPFOGALPHA=0`** is the workshop gate that stops that write (opaque `PropType = 9`). Chair: crates then stay visible much farther; they can still **tint**.
2. **`fogGetPropDistColor`** fills `rgba_f32` with **Dam fog RGB** `(16, 48, 96)` and a **distance alpha**. `lerp_rgba_s32_with_rgba_f32` then:
   - lerps shade **RGB toward fog RGB** (`src->a` as t)
   - writes **`dest->a = src->a * (255 - dest->a) + dest->a`** — a **second alpha**
   - packs `mrData.fogcolour`
3. Model types 3/4 (`modelApplyRenderModeType3` / `Type4`) do `gDPSetFogColor(fogcolour)` and **`G_RM_FOG_PRIM_A`**. Fog prim **alpha** is the blend amount. Even with opaque envcolour, crates still slide into Dam’s blue-grey.

That is the chair leftover: **fog colour vs second alpha write — not another skip knob.**

`fogGetPropDistColor` can also `return 0` (fully obscured) or `2` (no fog). That is a third kill. Do not paper it with `FOGSKIP` (wiped falsifier).

### 1.5 OCCLSKIP vs the skip ladder

`posIsOnScreen` is the **admit**. Chair: `OCCLSKIP=1` stops the occlusion admit from snapping crates off farther out. OCCLSKIP **alone** failed because `chrobjFogVisRangeRelated` still drew them at ~1% (`PropType = 5` XLU). Both knobs: crates stay.

Issue #29 body (vr441): OCCLSKIP / FOGSKIP / DISTSKIP / VISFAR **did not** fix the picture as a ladder. The later chair scrape is the **pair**, not VISFAR-as-crate.

| Knob | Class | Use on #29 |
|------|--------|------------|
| `GETV_VR_PROPFOGALPHA=0` | KEEP int (already C-default here) | Draw: kill ~1% env alpha. |
| `GETV_VR_OCCLSKIP=1` | KEEP int (already C-default here) | Admit: skip Nfd/occl snap-off. |
| `GETV_VR_VISFAR` | **Wipe** in vr441 boot | Far **guards**. Not the crate APPLY. Fold into shared helper if it is a MaxVisRange scale. |
| `GETV_VR_FOGSKIP` / `DISTSKIP` | Wipe / do-not-touch | Falsifiers. Stay off. |
| `GETV_VR_PROPFOGW` | do-not-touch | Chair: do not wear 443. |
| `GETV_XR_FOVMATCH` | smoke-forbidden | Stays off. |

Public `getv/port/src/port_render.c` already documents the pair as “Dam crates #29”. Graduation does **not** replace a shared MaxVisRange widen; it only stops the next zip depending on a bat line.

---

## 2. How the layers sit together

Not “crate list vs skip.” One Nfd, then colour.

| Layer | What | When |
|-------|------|------|
| **A — shared widen** (prefer) | Scale / raise `NearFog` + `MaxVisRange` in **one** helper used by fade **and** admit. | Structural. Helps crates **and** leftover far props/guards. |
| **B — chair keep** | `PROPFOGALPHA=0` + `OCCLSKIP=1` C-default (already in this tree). | Ship tonight’s next zip if A is not sat. Chair PASSed 2026-09-17 on workshop vr442. |
| **C — leftover colour** | Stop or clamp `fogGetPropDistColor` RGB lerp and/or `fogcolour.a` after B. | Only if A+B still read as a fade-into-sky. **Not** a new skip. **Not** PROPFOGW. |

```
far (temp >= MaxVisRange)
  A off, B off → admit false and/or objAlpha 0          (pop)
  B on         → still admitted, opaque env             (stay; may tint)
  A on         → temp still in band                     (stay, retail-like fade later)

mid (NearFog < temp < MaxVisRange)
  fade → ~1% XLU                                        (PROPFOGALPHA=0 stops this)
  fog RGB lerp + fogcolour.a                            (layer C)
```

---

## 3. APPLY sketches — **NOT LANDED**

Workshop only (`vendor/ge-decomp/src/game/propobj.c`, `bgfog.c`). Public tree has no bodies.

### 3.1 Shared Nfd helper (prefer)

```c
/* NOT APPLY READY — workshop sketch.
 * One scaled Nfd read for fade (chrobjFogVisRangeRelated)
 * and admit (sub_GAME_7F054C58). Do not branch on PROP_WOOD_*. */

static void geVrVisNfd(f32 *nearFog, f32 *maxVis, f32 *maxObf)
{
    struct NearFogRecord *nfd = fogGetNearFogValuesP();
    f32 s = 1.0f; /* ship: widen outdoor Nfd; unset = 1; dig 0 = stock */
    /* if VISFAR already has a scale, read it here — one place, chr+prop */
    if (!nfd) { *nearFog = *maxVis = *maxObf = 0; return; }
    *nearFog = nfd->NearFog * s;
    *maxVis  = nfd->MaxVisRange * s;
    *maxObf  = nfd->MaxObfuscationRange;
}
```

First chair numbers (PLAYER_PREF if a sit needs a rebuild-free lever; **not** KEEP-ON): start `s ≈ 2` on Dam (4444→~8888, still inside zfar 15000). Raise NearFog by the same `s` so the fade band does not become a 5000-unit ghost.

Do **not** set Dam `NearFog=0` (Facility trick). That deletes the retail fade for every Dam prop and every Dam guard.

### 3.2 Chair keep (already sketched in this repo)

No new C on public. Workshop already has the ternaries. This repo’s `MANIFEST.json` + `port_render.c` already unset→ship. Next zip: do not wipe them in `gevr-*-boot.cmd` section 0 (vr441 still `set GETV_VR_OCCLSKIP=`).

### 3.3 Leftover fog colour (only after A/B sit)

In `chrobjRenderProp` after `fogGetPropDistColor`:

- Keep RGB lerp **off** for the residual (force `spAC != 1` or `src->a = 0` on the lerp), **or**
- Force `fogcolour.a` to the opaque/no-fog value the Type3/4 path expects when env is already opaque.

Measure which of the two the chair still sees (tint vs hole). Do not add `PROPFOGW`. Do not add `FOGSKIP`.

---

## 4. Chair sit (workshop / next zip)

Dam, first walk, headset. Look at the **small wooden crates / drums** on the dam face and the tower approach — not Facility. Same pad, walk until they pop, note approximate distance.

**Run K — keep only** (next zip / C-default, `FOVMATCH` unset, `VISFAR` unset, `PROPFOGW` unset):

| | Tester sentence |
|--|-----------------|
| **K-PASS 1** | “Mid-range crates on Dam **stay put** as I walk. They do not blink in.” |
| **K-PASS 2** | “Facility still looks like Facility (no new far-prop soup).” |
| **K-FAIL** | “They still pop at the same walk.” / “They stay but turn Dam-sky blue.” / “Guards vanished / doubled.” |

**Run A — shared widen** (workshop helper, keep pair **off** or on; say which):

| | Tester sentence |
|--|-----------------|
| **A-PASS** | “Same crates stay, and far guards still read as the 007 far-vis keep.” |
| **A-FAIL** | “Crates stay only with OCCLSKIP.” / “Everything draws to zfar.” / “FOV / LOD jumped.” (`FOVMATCH` must still be off.) |

**Run C — leftover colour** (only if K-PASS 1 but crates **tint**):

| | Tester sentence |
|--|-----------------|
| **C-PASS** | “Crate colour stays crate-coloured at that range. No extra skip knob.” |
| **C-FAIL** | “They go transparent again.” / “PROPFOGW was needed.” |

**Regression:** `HEAD_TRANSLATE` still on (#55 is separate). Modem TV downstairs (#70) unchanged. `DRAWALL` / `CULLWIDE` / `GUNAIM` / `GUNMOUNT` untouched.

---

## 5. Director decision

| If you say… | Then… |
|-------------|--------|
| **Green shared widen** | Workshop 3.1. Fold VISFAR scale into the helper if it exists. Sit A, then K. |
| **Green keep only** | Already C-defaulted here. Next zip: stop wiping OCCLSKIP. Close #29 only if K-PASS holds on that tag (chair 19 Sep). |
| **Green leftover colour** | 3.3 after K. Not a skip. Not PROPFOGW. |
| **Want crate allowlist** | Rejected by this dig. Size already selects them. |
| **Want FOVMATCH** | Rejected. |
| **Want VISFAR as the crate knob** | Rejected. Wrong consumer. |
| **Reject all** | Leave #29 open. Public quirks line stays. |

---

## 6. Attribution / legal

- No personal credit paths edited.
- No GoldenEye ROM, assets, or dumps.
- Decomp cited as public `n64decomp/007` map only.
- Workshop C stays private until release policy flips.
