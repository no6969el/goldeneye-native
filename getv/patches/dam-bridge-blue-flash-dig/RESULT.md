# RESULT — Dam bridge / jump-strip blue flash (#72) (DIG ONLY)

**Status:** DIG. **Not APPLY READY.** No C landed.
**Ask:** On Dam, on the **bridge / jump-strip**, **turning around** shows a **blue** flash. Owner suspects sky or water. Seen during the #70 covert-modem chair with `GETV_XR_HEAD_TRANSLATE=0`.
**Constraints:** Separate from #70 modem green-text flicker (`MONFRAME` **REJECTED**; HT=0 kept). Do **not** ship `GETV_STEREO_MTXGUARD=2` on Dam / global again without a Dam-safe proof. Bunker bats may keep `=2`. New chair knob (if any) defaults **OFF**.
**Date:** 2026-09-20.
**Evidence:** public `goldeneye-native` HEAD, public `no6969el/GEVR` packaging + docs 14/19/292/281 + issues #72/#70/#55/#30, public `n64decomp/007` `bgfog.c` Dam env + `sky.c` `skyRender`. Workshop SKYMESH / SKYSCISSOR / SKYFILL **bodies** (what they do past getenv) and MTXGUARD call sites are **not on any public remote**.

Director can green-light the chair A/B, reject, or send a scoped APPLY from this page alone.

Ticket already filed: [GEVR #72](https://github.com/no6969el/GEVR/issues/72). Do not file another.

---

## Verdict (one screen)

| Question | Answer |
|----------|--------|
| What is the blue? | Dam **sky / fog fill RGB(16, 48, 96)**. Same colour the engine uses when nothing else drew. Not the modem. Not #55 black. |
| Sky or water? | **Sky fill / sky mesh.** Dam `IsWater=0` — `sky.c` does **not** emit a water plane here. The lake is **BG room mesh** (#30 murky look, parked `WATERTILE`). A **flat** medium-blue flash is fill, not rippled lake. |
| Same as old Dam **tunnel band**? | **No.** That was a **persistent edge strip** pressed against a wall (near-plane clip → fill). Closed by `GE_VR_MAX_ZNEAR_UNITS=2` (GEVR `docs/19-blue-band.md`). This is a **flash on yaw** on the **open crest**. |
| Same as MTXGUARD=2 Dam **tunnel flicker**? | **Same colour family, different wear.** Global `MTXGUARD=2` PASSed Bunker #55 black and **failed Dam tunnel blue** → C-default/boot back to **unset/0**; Bunker bats keep `=2`. Tonight’s Latest does **not** arm MTXGUARD. Do **not** re-globalize `=2` onto Dam. |
| HT=0 involved? | **Unproven.** Flash was **seen in** the #70 HT=0 sit; Latest now **ships HT=0**. Chair must A/B HT=1. Do **not** flip HT back to 1 as the #72 fix without a #70/#55 stare. |
| APPLY tonight? | **No.** Bat-only A/B first. Smallest C (if fill/scissor PASS) is a **default-OFF** skip, not KEEP-OFF of SKYMESH / SKYFILL / SKYSCISSOR. |

```
Dam crest yaw  → per-eye view mtx + body pos feed skyRender
                 Clouds=1 → fill RGB(16,48,96) THEN sky mesh
                 SKYMESH=1 / SKYSCISSOR=1 / SKYFILL=1 KEEP
one frame miss → fill shows through = BLUE FLASH
not: sky.c water plane (IsWater=0)
not: PROP_MODEMBOX green text (#70)
not: wall-press near-plane band (docs 19, closed)
```

---

## 1. Root finding (files + functions)

### 1.1 Evidence boundary

| Layer | Where | What it proves |
|-------|--------|----------------|
| Public `goldeneye-native` | this repo | KEEP getenv stubs (`GETV_VR_SKYMESH` in `getv/port/fast3d/gfx_pc.c`, `GETV_XR_HEAD_TRANSLATE` in `port_render.c`). **Not** the playable GETV tree. |
| Public `no6969el/GEVR` | `packaging/`, `docs/14`, `19`, `292`, `281`, issues | Sky KEEP list, old tunnel-band close, MTXGUARD=2 is a falsifier **and** later Bunker-only wear, #72 filing. |
| Public decomp | `n64decomp/007` `bgfog.c` / `sky.c` | Dam env RGB, Clouds, IsWater, `skyRender` fill-then-mesh. |
| Workshop (private) | `F:\Projects\GEVR\GoldenEyeVR\goldeneye-native` | `gfx_sky_rdp_tri`, SKYSCISSOR / SKYFILL call sites, MTXGUARD modes. **Do not push.** |

### 1.2 Dam environment (the actual blue)

NTSC `fog_tables[]` `LEVELID_DAM` (`src/game/bgfog.c`):

| Field | Value | Meaning |
|-------|--------|---------|
| Fill / fog RGB | **16, 48, 96** (`0x10, 0x30, 0x60`) | Medium blue. `viSetFillColor` / `gDPFillRectangle` when sky or rooms miss. |
| `Clouds` | **1** | `skyRender` builds a **cloud mesh**, not fill-only. |
| Cloud tint | 255, 255, 255 | White clouds over that blue. |
| `IsWater` | **0** | **No** `sky.c` water plane on Dam. |
| `WaterRepeat` | −1000 | Unused while `IsWater=0`. |
| BlendMultiplier | 5 | Dam native znear neighbourhood (workshop clamped **down** to 2; tunnel **band** closed). |

Facility fill is a different green-grey; Bunker indoor fill is not this blue. A Dam flash that reads **blue** is this record, not a random clear colour.

### 1.3 What `skyRender` draws (`src/game/sky.c`)

`skyRender` (decomp `0x7F094438` region):

1. `env = fogGetCurrentEnvironmentp()`.
2. If `!Clouds` and one player: **full-view** `gDPFillRectangle` of `env->Red/Green/Blue` and return. Dam does **not** take this (Clouds=1).
3. With Clouds: still `viSetFillColor(gdl, env->Red, env->Green, env->Blue)` **then** classify the four screen corners:
   - `skyGetWorldPosFromScreenPos` → `currentPlayerGetViewToWorldMtxf()` (per-eye view).
   - `skyIsScreenCornerInSky` / `skyIsCornerInWater` → `bondviewGetCurrentPlayersPosition()` (**body**, not headset lean).
4. Switch on which corners are “in sky” builds the cloud (and, if `IsWater`, water) mesh.

On Dam, step 4 is **sky mesh only**. The lake you see from the crest is **BG room triangles**, not this generator.

**GEVR KEEP on top of that** (vr441-class boot, still the public template; Latest pins HT=0 after #70):

| Knob | Ship | Role |
|------|------|------|
| `GETV_VR_SKYMESH` | **1** KEEP | VR sky mesh (`gfx_sky_rdp_tri` neighbourhood). Public stub getenv; C-unset in the **reference** fragment is `0` — **boot assigns 1**. Workshop MANIFEST graduates unset→ON. |
| `GETV_VR_SKYSCISSOR` | **1** KEEP | Per-eye sky scissor. **No** public getenv stub. |
| `GETV_VR_SKYFILL` | **1** KEEP | Fill rectangle of env RGB. |
| `GETV_VR_SKYFILL3` | **1** KEEP | Second fill arm. |
| `GETV_VR_SKYFILL2` | **0** DIG_OFF | Leave off. |
| `GETV_SKYTRACE` | wiped / forbidden in public boot | Chair-only census. |

A **flash** is one frame of (3) without (4), or (4) scissored away, or rooms dropped so (3) is all you see.

### 1.4 Jump-strip / bridge (where the chair stood)

Dam **crest**: long outdoor walkway, spawn at one end, lake + sky when you look out or **turn around**. Not the indoor tunnel. Not downstairs under the towers (#70 modem stare).

Yaw 180° on that strip:

- Look direction swings from dam geometry to **open sky + lake**.
- `skyRender` corner bits change case in the switch.
- Stereo eye 2 has a different view mtx; body pos is shared.
- With **HT=0**, there is no lean, but **yaw still rebuilds** the view (`GETV_STEREO_REBUILD=1` KEEP).

### 1.5 `HEAD_TRANSLATE=0` (the sit, not yet the proof)

| | HT=1 (old boot KEEP) | HT=0 (Latest after #70) |
|--|----------------------|-------------------------|
| Lean | Eye can leave the body / portals | Rotation only |
| #55 / #70 | Eye can drop a room → **black** / help modem flicker | Run 0 **PARTIAL PASS** on #70; #55 still open at Facility end |
| Sky | Body pos vs eye mtx already disagreed | Body ≈ eye **translation**; yaw still per-eye |

Flash **observed while wearing HT=0**. That does not prove HT=0 **causes** it. Latest now ships HT=0, so #72 is a **ship** stare, not a scratch-only one.

### 1.6 MTXGUARD (do not ignore, do not re-arm)

| Era | What happened |
|-----|----------------|
| Session 292 / docs 293 | `GETV_STEREO_MTXGUARD=1` observe; **`=2` skip** “showed better”. **Falsifier.** Double-conversion of `bondviewTransformManyPosToViewMatrix`. **Do not ship =2** as that fix. |
| Later chair (owner history) | Global **`=2` KEEP** PASSed **Bunker #55 black**. Same global **Dam tunnel blue flicker**. C-default / boot **reverted to unset/0**. **Bunker bats keep `=2` only.** |
| Public inventory | **`MTXGUARD` is not in** `KEEP-DEFAULTS-INVENTORY-vr441.md`, not in `gevr-vr441-boot.cmd`, not in this repo’s MANIFEST. Ship = **unset/0**. |
| Parallel | #55 Facility-end mine-throw dig: MTXGUARD=2 **Facility-safe path only** — **will not re-globalize onto Dam.** |

`=2` skips already-converted matrices on the second eye. Missing sky/room matrices → env fill shows → **blue on Dam**, **black** where fill is near-black (Bunker). That is why Dam and Bunker disagreed.

**Do not** put `GETV_STEREO_MTXGUARD=2` on Dam boot, C-default, or a “fix #72” bat.

---

## 2. Same as old Dam tunnel blue? — **Colour family yes. Mechanism no.**

| | #72 crest flash (tonight) | Tunnel **band** (`docs/14` + `19`) | Tunnel **flicker** (MTXGUARD=2) |
|--|---------------------------|-------------------------------------|----------------------------------|
| Colour | Dam fill **(16,48,96)** | Same fill (“same colour as Dam water”) | Same fill |
| Shape | **Flash** on **turn** | **Persistent** full-height **edge strip** | Flicker in the **tunnel** |
| Place | Open **crest / jump-strip** | Pressed **against a wall** | Tunnel |
| Cause (source) | Sky fill showing through for a frame (mesh / scissor / stereo yaw) | Near-plane **clipped the wall** | Second-eye matrices **skipped** |
| Status | **Open (#72)** | **Closed** (znear clamped down to 2) | **Wear reverted**; Bunker-only `=2` |
| HT=0 | Seen in this wear | Unrelated | Unrelated |

Also **not**:

| Ticket | Why not |
|--------|---------|
| #70 modem | Green-text **monitor** UV. `MONFRAME` **REJECTED**. Owner split the blue flash onto #72. |
| #55 black | **Black** room drop / tanks. Parallel MTXGUARD Facility-safe dig. Do not merge. |
| #30 Dam water | **Murky lake look**. `WATERTILE` parked. Lake is BG mesh (`IsWater=0`). Not a yaw flash. |
| #29 crates | Fog / occl. Unrelated. |
| 292 dead-eye rectangle | One eye **stuck** on sky fill (shared viewport). **Fixed.** Would not be a turn flash. |
| 281 portal scissor slab | Axis-aligned hole **through a doorway**. Crest is open sky. Low prior unless the flash is a hard rectangle in a portal. |

---

## 3. Hypothesis table

| # | Hypothesis | Result |
|---|------------|--------|
| 1 | Sky **fill** (`SKYFILL` / `SKYFILL3`) for one frame while mesh/rooms catch yaw | **Best source fit.** Fill is always set; mesh is KEEP. Flash = fill without coverage. |
| 2 | `SKYMESH` / `SKYSCISSOR` stereo yaw (wrong eye scissor or mesh parent vs HT=0) | **Open, chair-cheap.** Both KEEP ON during the bug. A/B `=0` one at a time. **Do not** C-default them OFF (sky would die). |
| 3 | `sky.c` water plane | **Falsified** as Dam env. `IsWater=0`. |
| 4 | Lake BG tiles (`WATERTILE`) | **Unlikely** for a **flat blue** flash. Chair last. Do not ship. |
| 5 | Near-plane wall band | **Falsified** by place + motion (open crest, turn, not wall-press). Already closed. |
| 6 | MTXGUARD=2 still on | **Unlikely** on Latest (unset/0). Chair confirms no mtxguard banner. **Do not arm `=2` on Dam.** |
| 7 | HT=0 is the cause | **Open.** Seen in HT=0; may only be the sit. A/B HT=1. Do not ship HT=1 as #72 without #70/#55. |
| 8 | Portal / DRAWALL | **Low** on open crest. Do **not** disable DRAWALL / PORTALWIDE. |

---

## 4. KEEP already armed (public Latest / vr442-class)

Public GEVR still checks in `gevr-vr441-boot.cmd` except #70 **silent zip**: `GETV_XR_HEAD_TRANSLATE=0` (was pinned `1`).

| Knob | Public boot / C unset | vs #72 |
|------|------------------------|--------|
| `GETV_VR_SKYMESH` | **ON** (boot `1`; workshop unset→ON) | First A/B. |
| `GETV_VR_SKYSCISSOR` | **ON** (boot `1`) | Second A/B. |
| `GETV_VR_SKYFILL` / `SKYFILL3` | **ON** | Third A/B. Do not C-default OFF. |
| `GETV_VR_SKYFILL2` | **OFF** | Leave. |
| `GETV_XR_HEAD_TRANSLATE` | **OFF on Latest** (was ON) | Restore `=1` only as a #72 A/B. |
| `GETV_STEREO_REBUILD` | **ON** | Per-eye world. Do not turn off first. |
| `GETV_VR_DRAWALL` / `PORTALWIDE` | **ON** | Leave (same as #55). |
| `GETV_STEREO_MTXGUARD` | **unset / 0** | Bunker bats only `=2`. |
| `GETV_VR_WATERTILE` | wiped | Parked. |
| `GETV_VR_MONFRAME` | OFF / rejected | Do not retry for blue. |
| `GETV_SKYTRACE` | forbidden in ship boot | Optional chair census. |

---

## 5. APPLY sketch (DIG ONLY — default OFF)

**APPLY READY? No.** Need one Dam crest sit. Workshop bodies are private.

### 5.1 Do not land

- Do **not** C-default or boot `GETV_STEREO_MTXGUARD=2` on Dam / global. Bunker (and any Facility-safe path) stay bat-local until a **Dam-safe** proof.
- Do **not** C-default `SKYMESH` / `SKYSCISSOR` / `SKYFILL` / `SKYFILL3` OFF.
- Do **not** flip Latest HT=0 back to 1 as the #72 fix without #70 + #55.
- Do **not** ship `WATERTILE`, `FOGSKIP`, `DISTSKIP`, `FOVMATCH`, `MONFRAME=1`.

### 5.2 Chair A/B (no rebuild) — **one change per run**, restore after

Unset / empty = ship. Explicit `0` = dig (except MTXGUARD, which you must **not** leave on).

**Control first:** Latest, HT=0 as shipped. Dam, walk onto the **jump-strip / crest**. Turn around. Confirm the blue flash. Then:

| Run | Env | If flash **dies** | If flash **stays** |
|-----|-----|-------------------|--------------------|
| **S** | `GETV_VR_SKYMESH=0` | Mesh path. **Do not** ship SKYMESH off. APPLY 5.3 scoped to Dam/yaw. Note whether the sky becomes **flat fill** (expected). | Mesh not sufficient. Restore `=1`. |
| **C** | `GETV_VR_SKYSCISSOR=0` | Scissor. APPLY 5.3 per-eye scissor, KEEP stays ON. If sky **leaks**, scissor was containing it. | Restore. |
| **F** | `GETV_VR_SKYFILL=0` then `SKYFILL3=0` | Fill rectangle **is** the flash. APPLY 5.3 skip fill when mesh draws / skip fill on second eye. **Do not** C-default FILL off. | Restore. |
| **H** | `GETV_XR_HEAD_TRANSLATE=1` | HT=0 is involved. Keep issues split: #70 still wants HT=0. Do not ship HT=1 as #72 alone. | HT=0 is the sit, not the root. Restore Latest HT=0. |
| **M** | `GETV_STEREO_MTXGUARD=2` | **Do not treat as a fix.** Expected **worse** or old tunnel blue. Restore **immediately**. Proves colour family only. | Restore. Confirms tonight is **not** MTXGUARD. |
| **W** | `GETV_VR_WATERTILE=1` | Lake tiles. Unlikely. **Do not ship.** | Restore wipe. |

If **S/C/F** all FAIL and **H** FAIL: still sky/room coverage on yaw, not KEEP-OFF. Then 5.3 `SKYFRAME` (second-eye sky frozen) default OFF.

Optional: `GETV_SKYTRACE=1` on a scratch boot (not the ship allowlist) to print sky fill vs mesh vs scissor for one crest yaw. Forbidden in public boot.

### 5.3 Smallest C (workshop only, after chair)

**5.3a — if Run F PASS:** In workshop `sky.c` / `gfx_pc.c` (fill before `SKYMESH`), skip `gDPFillRectangle` / `viSetFillColor` when `SKYMESH` is drawing, **or** skip fill on the non-sim eye.

```c
/* NOT APPLY READY — workshop sketch.
 * GETV_VR_SKYFRAME unset/0 = today (fill every eye).
 * 1 = first eye / gePortSimShouldTick fills; second eye reuses mesh, no extra fill.
 * Analogous to rejected MONFRAME — chair only, default OFF.
 */
static int ge_vr_skyframe(void)
{
    static int on = -1;
    if (on < 0) {
        const char *e = getenv("GETV_VR_SKYFRAME");
        on = (e != NULL && *e != '\0') ? (atoi(e) != 0) : 0; /* chair; not KEEP-ON */
    }
    return on;
}
```

Files: workshop `vendor/ge-decomp/src/game/sky.c` (`skyRender`), `getv/port/fast3d/gfx_pc.c` (`ge_vr_skymesh`, `gfx_sky_rdp_tri`). Confirm live names before typing.

**5.3b — if Run C PASS:** Per-eye sky scissor from that eye’s frustum, not last eye / shared rect. KEEP `SKYSCISSOR=1`. No new KEEP.

**5.3c — if Run S PASS and F FAIL:** Mesh rebuild per eye (same pairing as `STEREO_REBUILD`). Do **not** ship `SKYMESH=0`.

**5.3d — MTXGUARD:** **Out.** Any Facility/Bunker `=2` path stays **stage-gated**. Dam must keep unset/0 unless a later sit proves Dam-safe (this dig does not).

### 5.4 Out of scope

- Turning off `GETV_STEREO_REBUILD` / `GETV_VR_DRAWALL`
- #70 `MONFRAME`, TEXINVAL C-default
- #30 lake look, #29 crates, #55 black
- Boot allowlist / pack smoke until a sit PASS

---

## 6. Chair stare — plain tester sentences

**Setup:** public **vr442 / Latest** (HT=0 in boot after #70 zip refresh). `Start-GEVR.bat`. Dam (any difficulty). Recenter both sticks. Headset (say which). **Do not** set FOVMATCH, MONFRAME, or MTXGUARD. Walk onto the **dam top / jump-strip** (the long crest, lake on one side).

| | Tester sentence |
|--|-----------------|
| **PASS** | “I stand on the dam crest / jump-strip and **turn around**. The picture stays **steady**. No blue flash.” |
| **FAIL** | “When I turn around on that strip, I get a **blue** flash (flat sky colour, not the green modem screen).” |
| **NOTE — colour** | “The flash is **flat medium blue**, like empty sky, not rippled lake and not black.” (confirms fill, not #30 / #55) |
| **NOTE — tunnel** | “In the **tunnel**, not pressed to a wall, I do **not** see the old edge-strip.” (splits closed band) |

**Knob runs** (only if FAIL). Restore the previous knob before the next.

| Run | Sentence if it **fixed** it |
|-----|------------------------------|
| **S** `SKYMESH=0` | “Turn flash gone. Sky may look flatter / fill-only.” → 5.3c, **do not** ship SKYMESH off. |
| **C** `SKYSCISSOR=0` | “Turn flash gone.” / “Sky leaked at the edges.” → 5.3b. |
| **F** `SKYFILL=0` | “Turn flash gone. Sky mesh still there.” → 5.3a. |
| **H** `HEAD_TRANSLATE=1` | “Flash gone when lean is back on.” → HT=0 involved; **do not** ship as #72 alone. |
| **M** `MTXGUARD=2` | **Expected FAIL / tunnel blue.** Restore. Do not keep. |
| **None of S/C/F** | “Still flashes with those at 0.” → green-light **5.3a `SKYFRAME`** (default OFF) after a Director yes. |

**Regression every run:** covert-modem under-towers as Latest (HT=0 wear), Dam crates as now, no full-screen **black**, Bunker not part of this PASS (Bunker bats may still use MTXGUARD=2).

Headset / OpenXR / SteamVR on or off / HMD vs monitor — write them on the sit. No ROM.

---

## 7. Director decision

| If you say… | Then… |
|-------------|--------|
| **Green chair only** | Sit control + S/C/F then H. No C. |
| **Green 5.3a if F PASS** | Workshop skip fill on second eye / when mesh draws. `SKYFRAME` default OFF. KEEP FILL stays ON. |
| **Green 5.3b if C PASS** | Per-eye sky scissor. KEEP SCISSOR ON. |
| **Ship MTXGUARD=2 globally / on Dam** | **Rejected** until a Dam-safe proof. This dig is not that proof. |
| **Fix #72 by setting HT=1** | Rejected as the only change. Latest HT=0 is a #70 wear. Split tickets. |
| **Merge into #70 / #55 / #30** | Rejected. |
| **APPLY tonight from this repo** | **No.** DIG only. |

---

## 8. Attribution / legal

- No personal credit paths edited.
- No GoldenEye ROM, assets, or dumps.
- Decomp citations are public `n64decomp/007` (`bgfog.c` Dam `fog_tables` row, `sky.c` `skyRender`).
- GEVR textbook: `docs/14-near-plane-clipping.md`, `docs/19-blue-band.md`, `docs/292-…`, `docs/281-…`, packaging KEEP inventory.
- Workshop C stays private until release policy flips.
