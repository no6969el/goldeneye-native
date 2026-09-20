# RESULT — viewmodel gun scale + stereo/depth (#39) (DIG ONLY)

**Status:** DIG. **Not APPLY READY on this public tree.** No C landed.
**Ask:** Held gun feels **too big** on Quest 3S / Quest 3, with a **slight cross-eye** until the eyes settle. Less on Pimax (higher PPD). Wear is public **vr441** / Start-GEVR.bat (public zip still that cut).
**Constraints:** **Not** [#33](https://github.com/no6969el/GEVR/issues/33) HUD depth. **Not** [#35](https://github.com/no6969el/GEVR/issues/35) ADS grip. Do **not** crank `GETV_XR_UNITS_PER_M`. New knobs default **OFF**. GUNMOUNT / GUNAIM / GUNARM stay KEEP-ON. Parked `GUNZ` / `HANDSOLID` (gun vanish below chest) stay off.
**Date:** 2026-09-20.
**Evidence:** public `goldeneye-native` HEAD, public `no6969el/GEVR` #39 + textbook/packaging (vr441 boot), public `n64decomp/007` `gunfire.c` / `model.c` / `bondview.h`. Workshop bodies (`geStereoXrGunMount`, vr444 `GUNEYE`, 369 sight offset) are **not on any public remote** (`GEVR` `docs/RELEASE-POLICY.md`). Brief names are the workshop symbols.

Director can green-light Rank 1 only, Rank 1+2, Rank 1+3, all three, or none from this page alone.

---

## Verdict (one screen)

| Question | Answer |
|----------|--------|
| Root (Rank 1) | **Both eyes get the same gun matrix.** World has stereo; the gun does not. Zero disparity on a near, large-angular object reads **huge + brief cross-eye**. Cover one eye → size and strain go. |
| Scale? | **Symptom of Rank 1 first.** Angular size at zero disparity. Mesh scale is **not** the first knob. Units-per-metre is **not** this path (`GETV_XR_UNITS_PER_M=100` KEEP). |
| Depth / Z? | **Rank 3, after Rank 1.** Stock FP gun draws with **z-buffer OFF** (`gunRenderFirstPersonGunModels`). That is the wall-clip dodge. It can also make the gun sit “in front of” the room. |
| Quest vs Pimax? | **Same fault.** Louder on Quest (lower PPD / stronger lenses). Not a Quest-only scale bug. |
| Same as #33? | **No.** HUD is ortho / `HUDGATE` / parked `HUD_DEPTH_PX`. Different draw. |
| Same as #35? | **No.** ADS grip / squeeze binding. Do not retune `ADSSIGHT` / `SQUEEZE` here. |
| `STEREO_GUNOFS` already ON? | **Different knob.** That is game `gunofs` on the stereo rebuild, not per-eye IPD on the mesh. Do **not** flip KEEP. |
| Wear-only? | **No.** Unset `GUNEYE` is tonight (huge / strain). |
| APPLY tonight? | **No from this repo.** Workshop vr444 already wrote `GETV_VR_GUNEYE` default OFF, **not chaired**. Do not put it in the public zip. |

```
world     → STEREO_REBUILD + geVrGetEyeViewOffsetF     (per-eye, KEEP)
sight     → 369 / SIGHT2D / ADSSIGHT / SIGHTPX         (already per-eye)
gun mesh  → one gunmtx_camspace, G_MTX_LOAD both eyes  (zero disparity)
            + zbufferenabled = 0                       (always on top)
Quest     → same error, easier to see
```

---

## 1. Root finding (files + functions)

### 1.1 Evidence boundary

| Layer | Where | What it proves |
|-------|--------|----------------|
| Public `goldeneye-native` | this repo | Per-eye world (`geVrGetEyeViewOffsetF`, `geVrCurrentEye`). Weapon **draw** ABI is `geVrGetWeaponModelMatrixF` (grip, world). Units `GE_VR_UNITS_PER_METRE 100`. **Not** the playable GETV tree. |
| Public `no6969el/GEVR` | #39 comments, `packaging/templates/gevr-vr441-boot.cmd`, `docs/36` / `77` / `78` / `83` / `170` | Wear, chair plan, historical viewmodel isolation, HUD is a different path. |
| Public decomp | `n64decomp/007` | Camera-space gun, `G_MTX_LOAD`, z-buffer off on FP draw. |
| Workshop (private) | `F:\Projects\GEVR\GoldenEyeVR\goldeneye-native` | `geStereoXrGunMount`, vr444 `GUNEYE`, 369 sight offset. **Do not push.** |

`geStereoXrGunMount` / `ge_vr_guneye` / the 369 sight helper **do not appear in any public file**. First chair grep: `getenv("GETV_VR_GUNEYE")` and `[getv][stereo] 444`.

### 1.2 What the decomp actually draws

**Placement is camera space, by name:**

```
n64decomp/007 src/game/bondview.h:210     Mtxf gunmtx_camspace
n64decomp/007 src/game/gunfire.c:570-574  gunmtx ← rot + gunofs
                                          copy → hand->gunmtx_camspace
                                          ViewToWorld × camspace → throw_item_pos_related
n64decomp/007 src/game/gunfire.c:818      field_B64 = -gunmtx_camspace.m[3][2]
                                          (camera looks down −Z; depth is −m[3][2])
```

**Draw is an absolute modelview, not a multiply on the eye view:**

```
n64decomp/007 src/game/model.c:4854
  gSPMatrix(..., G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW)
```

`G_MTX_LOAD` replaces the modelview. If both eyes load the **same** `gunmtx_camspace`, the gun has **zero inter-ocular disparity** no matter how correct the world frustum is.

**FP gun draw kills the z-buffer:**

```
n64decomp/007 src/game/gunfire.c:1488-1588
  gunRenderFirstPersonGunModels
    renderdata.zbufferenabled = 0;          /* :1549 */
    subdraw(..., hand model)
    bondviewTransformManyPosToViewMatrix(...)
```

That is the stock “do not clip into walls” layer. A gun with z-test off always wins the draw, so it reads **closer than the room** even when the controller is at a wall. Casings use the same `zbufferenabled = 0` trick (`gunfire.c:5614`). World / third-person weapon props use `TRUE` (`:1924`) — different path.

GEVR `docs/83`: *“The viewmodel draws with the z-buffer OFF, so a gun behind the camera does not vanish.”*

### 1.3 What tonight’s KEEP already does (and does not)

vr441 boot / inventory (`GETV_*` ship values). **Do not flip these for #39:**

| Knob | Ship | Role vs #39 |
|------|------|-------------|
| `GETV_STEREO` / `_SRC=xr` / `_REBUILD` / `_VIEWRESTORE` | ON | World stereo. Not the gun mesh. |
| `GETV_STEREO_GUNOFS` | **1 KEEP** | Copies / rebuilds game **`gunofs`** on the stereo pass. Same offset both eyes ≠ IPD. **Not GUNEYE.** |
| `GETV_STEREO_HUDGATE` | ON | **#33** HUD gate. Leave it. |
| `GETV_STEREO_AIMRECT` | ON | Sight rect. Leave it. |
| `GETV_XR_UNITS_PER_M` | **100** | Hand / world units. Prior DIG + #39 comments: **not this path.** |
| `GETV_VR_GUNMOUNT` / `GUNAIM` / `GUNARM` | 1 | World gun at controller, shot follows gun, floating VR guns. **Keep.** |
| `GETV_VR_SIGHT2D` / `ADSSIGHT` / `SIGHTPX=6` / `ADSCULL` | ON | **369-class sight** already per-eye. Gun mesh does not reuse that offset tonight. |
| `GETV_VR_RETICLE` / `_HAND` / `_M` | ON / 5 m | World reticle. Not the mesh. |
| `GETV_XR_HEAD_TRANSLATE` | ON | Head lean. Lean test below uses this. |
| `GETV_XR_BTN_SQUEEZE` | **wiped** | **#35.** Do not arm here. |

Public ABI (`include/ge_vr/ge_vr.h` / `src/ge_vr_bridge.cpp`):

| Helper | Space | Job |
|--------|--------|-----|
| `geVrGetEyeViewOffsetF` | head⁻¹ × eye | World IPD / roll / lean. **Gun LOAD skips this** if camspace is absolute. |
| `geVrGetWeaponModelMatrixF` | OpenXR **grip** × 100 u/m | Fist / weapon **draw**. One matrix. No `current_eye` term. |
| `geVrGetAimRay` | OpenXR **aim** | Hitscan. Not size. |
| `geVrBuildProjectionF` | per-eye FOV | World frustum. znear floor 10 units. |

Workshop: `geStereoXrGunMount` under `GUNMOUNT=1` writes a world/controller pose into the gun path. If that pose is then baked through **one** view (cyclopean / first eye / sim tick) and `G_MTX_LOAD`’d for both eyes, GUNMOUNT can be “on the hand” and still **stereo-blind**. That is the Rank 1 mechanism.

### 1.4 Scale, measured, and why u/m is the wrong knob

GEVR `docs/36` (worn): PP7 fills the frame; at 100 u/m that is **~15–25 cm** from the face — inside the comfortable fusion near point (~40–50 cm). Eyes cannot converge. *“I have to close one eye.”*

GEVR `docs/77`: gun stayed **big** at 25 / 35 / 50 / 100 u/m. It only shrank at 400, when the **whole world** shrank. An object that keeps angular size across a 16× world-scale sweep is **not** a world-scale fault.

GEVR `docs/159`: world u/m and hand u/m are different quantities. Hand **100** is 1 cm per game unit (Bond eye 175, guard `chrheight` 185). `#39` wear is the **gun mesh**, not the doorway.

Owner on #39 (2026-09-18): *“Units-per-metre is not on this path — do not crank that knob.”* This DIG agrees.

**Quest vs Pimax:** same zero-disparity + z-off stack. Quest 3 / 3S have fewer pixels per degree and stronger lens distortion, so a near card with no stereo sting is easier to notice. Pimax Crystal Super (ship-verified) hides the same error. Not a Quest-only units or mesh-scale bug.

### 1.5 Owner workshop DIG (already written, not chaired)

#39 comments (owner):

1. Rank 1 = same gun matrix both eyes. One-eye sit named, then skipped.
2. Workshop **vr444** FIX: `GETV_VR_GUNEYE` default **off**. PLAY0 turns it on: **each eye gets its own gun translation** — *“same offset the 369 sight already uses.”* Mesh scale unchanged. Units-per-metre unchanged.
3. Chair: `_launch444.ps1 -Arm B0` must still look like vr441 (huge / strain), no `[getv][stereo] 444` line. Then PLAY0, banner `GUNEYE=1`. **Strain gone?** and **size closer to a pistol?** are **two answers**.
4. Later note: testers still say huge. Second hypothesis = **Z / draw-layer bias** so the gun does not clip walls. Chair GUNEYE first; only then dig Z-bias. Do not crank u/m.

This public DIG **confirms Rank 1 from source**, **separates scale from stereo**, and **names Rank 3** from the decomp z-off site. It does **not** replace the vr444 chair.

### 1.6 Not this ticket

| Ticket / knob | Why it stays out |
|---------------|------------------|
| [#33](https://github.com/no6969el/GEVR/issues/33) `GETV_VR_HUD_DEPTH_PX` | Ortho HUD / messages. `docs/170`: HUD was never given an eye. `STEREO_HUDGATE` KEEP. Parked polish, default off in ship boot. |
| `GETV_VR_MSGSCALE` / `TEXTBAND_*` / `AMMOHUD_PAD` | HUD text / ammo picture (`#34`). Wiped in vr441 section 0. |
| [#35](https://github.com/no6969el/GEVR/issues/35) `GETV_XR_BTN_SQUEEZE` | Left-grip ADS while walk/crouch. Binding, not mesh stereo. |
| `GETV_VR_ADSSIGHT` / `ADSCULL` | PLAY0 sight KEEP. Do not retune to “fix size.” |
| `GETV_VR_TWOHAND` | Two-hand snap DIG (PR #4). Different latch. |
| `GETV_VR_GUNZ` / `HANDSOLID` | Parked: gun **vanishes below chest**. COMING-SOON / FEATURES-CURRENT. Not a ship lever. |
| `GETV_XR_UNITS_PER_M` / `GE_VR_UNITS_PER_METRE` | KEEP 100. Dollhouse / hyperstereo is a **world** knob. |

---

## 2. How the three ranks sit together

Chair **in this order**. Do not stack 2 or 3 until 1 has a B0 + PLAY0 pair.

| Rank | Fault | Knob (default OFF) | What PLAY0 should change |
|------|--------|---------------------|---------------------------|
| **1** | Zero gun disparity | `GETV_VR_GUNEYE` | Strain first. Size may drop as a **side effect** (object now at hand depth). |
| **2** | Mesh still huge after 1 PASS | `GETV_VR_GUNSCALE` | Angular size about the **root**. Halve any live `GUN_OFF_*` with it (`docs/159`). |
| **3** | Still huge / “in front of” walls, or wall clip after 1 | `GETV_VR_GUNZBIAS` (bias) or `GETV_VR_GUNZTEST` (real z) | Depth cue vs world. `GUNZTEST` is the parked vanish family — last. |

```
GUNEYE=0  → tonight: one matrix, z-off, Quest sting
GUNEYE=1  → per-eye translation (369 sight offset). Scale 1. Z-off stays.
GUNEYE=1 + GUNSCALE=n → same stereo, smaller mesh
GUNEYE=1 + GUNZBIAS   → same stereo, less “pasted on top”
GUNEYE=1 + GUNZTEST   → gun can hide in walls / vanish below chest (known)
```

---

## 3. Proposed knobs — **default OFF** — NOT LANDED

All getenv: unset / empty / `0` = **tonight**. Explicit `1` (or a number) is chair-only. **Not KEEP-ON.** Do **not** add to `gevr-vr441-boot.cmd` / `$requiredBootKnobs` until a sit PASS.

### 3.1 Rank 1 — `GETV_VR_GUNEYE` (recommend first)

```
GETV_VR_GUNEYE         unset / empty / 0 = OFF    (B0 = vr441)
                       1 = per-eye gun translation
GETV_VR_GUNEYE_TRACE   0   DIG_OFF
```

Workshop already has this on vr444. **Reuse the 369 sight offset.** Do not invent a second IPD. Do not scale the mesh. Do not write `UNITS_PER_M`.

```c
/* NOT APPLY READY — workshop sketch.
 * Next to geStereoXrGunMount / gunmtx bake (same TU or stereo.c).
 * Tick per eye (geVrCurrentEye). Do not bake once on sim / first eye. */

static int ge_vr_guneye(void)
{
    static int on = -1;
    if (on < 0) {
        const char *e = getenv("GETV_VR_GUNEYE");
        on = (e != NULL && *e != '\0') ? (atoi(e) != 0) : 0; /* default OFF */
    }
    return on;
}

/* if (ge_vr_guneye())
 *     gunTranslation += sight369EyeOffset(geVrCurrentEye());
 *     -- same delta ADSSIGHT / SIGHT2D already applies
 *     -- do NOT multiply scale, do NOT touch aim ray
 */
```

**Why this is the smallest change:** one translation, already proven on the sight. `G_MTX_LOAD` then differs per eye by IPD, so the gun converges at the hand instead of at infinity.

**Public-tree note:** `geVrGetWeaponModelMatrixF` has no eye term. If the workshop still draws through that ABI, GUNEYE belongs **after** the grip matrix, in the eye pass — or the ABI grows an eye argument. Confirm live names before typing aliases.

**APPLY READY?** On the **workshop**, vr444 claims this is written. **Not APPLY READY here** — no body to land. Do not stub it.

### 3.2 Rank 2 — `GETV_VR_GUNSCALE` (only if 1 PASS and still huge)

```
GETV_VR_GUNSCALE       unset / empty = 1.0 identity   (OFF)
                       n = scale about gun root
```

Historical name `GE_VR_VIEWMODEL_SCALE` (`docs/78`) did this: scale about the **root**, then `t = root + (t - root) * scale`. Scaling about the eye is a no-op on angular size. Per-node scale pulls slide / hammer / muzzle apart.

**Do not ship a value other than 1.0 until Rank 1 PLAY0 answers “size” separately from “strain.”** Owner vr444 left mesh scale unchanged on purpose.

### 3.3 Rank 3 — Z / draw layer (only if 1 PASS and size or clip remains)

```
GETV_VR_GUNZBIAS       unset / empty / 0 = OFF     (today: z-off, always on top)
                       n  = polygon-offset / depth bias toward the camera
GETV_VR_GUNZTEST       unset / empty / 0 = OFF     (today)
                       1  = renderdata.zbufferenabled = 1 on FP gun
```

`GUNZTEST=1` is the honest fix for “pasted on the room” and the honest way to **re-introduce wall clip + below-chest vanish**. That is why `GUNZ` / `HANDSOLID` are parked. Prefer **GUNZBIAS** (small, default 0) if the chair only needs “less cardboard, still never buried.”

Do **not** name a new knob `GETV_VR_GUNZ` — that token already means the parked vanish.

### 3.4 Out of scope

- `GETV_XR_UNITS_PER_M` / world / hand u/m sweep
- `GETV_STEREO_GUNOFS=0` as a “fix” (falsifier only; KEEP stays 1)
- `GETV_VR_HUD_DEPTH_PX` / HUDGATE / MSGSCALE (#33)
- `GETV_XR_BTN_SQUEEZE` / ADSSIGHT retune (#35)
- `GETV_VR_GUNARM=0` / Bond sleeve / BODY
- KEEP-ON graduation of GUNEYE / GUNSCALE / GUNZBIAS
- Boot allowlist / pack smoke until Rank 1 sit PASS
- Personal credit paths. ROM dumps

---

## 4. Chair stare (plain tester sentences)

**Setup:** vr441-class zip **or** workshop `_launch444.ps1`. Recenter both sticks. PP7 or KF7 in the **right** hand. Facility or Dam doorway. **Do not** set `HUD_DEPTH_PX`, `MSGSCALE`, `SQUEEZE`, or `UNITS_PER_M`. Leave `GUNMOUNT=1`, `GUNARM=1`, `STEREO_GUNOFS=1`.

**Run B0 — knobs unset (must match today):**

| | Tester sentence |
|--|-----------------|
| **B0-PASS 1** | “The gun is still **too big**, same as Start-GEVR.bat.” |
| **B0-PASS 2** | “Looking at the gun still **stings / goes a bit cross-eyed**, then my eyes give up.” |
| **B0-PASS 3** | “No `GUNEYE=1` and no `[getv][stereo] 444` (or equivalent) in the banner.” |
| **B0-FAIL** | “B0 already looks like a real pistol / no strain.” **Stop. The workshop binary is not vr441.** |

**Run P0 — `GETV_VR_GUNEYE=1` only:**

| | Tester sentence |
|--|-----------------|
| **P0-PASS 1 (strain)** | “I can look at the gun with **both eyes**. The brief cross-eye is **gone**.” |
| **P0-PASS 2 (one-eye)** | “Cover one eye: size does **not** suddenly shrink. (Tonight it does.)” |
| **P0-PASS 3 (lean)** | “I lean 20 cm sideways. The gun stays on my **hand**. The room slides. It is **not** glued to my face.” |
| **P0-PASS 4 (size)** | “It reads closer to a **real pistol**.” **Separate checkbox from PASS 1.** |
| **P0-PASS 5 (KEEP)** | “Aim, squeeze ADS on the **gun ray**, casings, touch-use: same as tonight.” |
| **P0-FAIL** | “Strain still there.” / “Gun welded to my face.” / “Aim moved.” / “Unset GUNEYE does not restore huge/strain.” / “HUD or ADS bindings changed.” |

**Run Q — Quest 3 / 3S vs Pimax (same boot, both P0):**

| | Tester sentence |
|--|-----------------|
| **Q-PASS** | “Quest sting is gone. Pimax was already milder; it did **not** get worse.” |
| **Q-FAIL** | “Only Pimax improved” / “Quest still huge after strain is gone” → unlock Rank 2, not u/m. |

**Run S — Rank 2, only if P0-PASS 1 and P0-PASS 4 is NO:**

`GETV_VR_GUNEYE=1` + `GETV_VR_GUNSCALE=0.5` (first chair step; then wear). Gun smaller **where it is**. World / guards unchanged. Slide does not come apart.

**Run Z — Rank 3, only if P0 strain PASS and (size still wrong **or** “gun through walls” is the remaining complaint):**

`GUNZBIAS` small first. `GUNZTEST=1` last. **Z-FAIL:** “Gun vanished when I dropped my hand” = parked `GUNZ` / `HANDSOLID`. Turn `GUNZTEST` off.

**Falsifier (optional, not a fix):** `GETV_STEREO_GUNOFS=0` on B0. If that **alone** kills strain, this DIG is wrong about GUNEYE vs GUNOFS. Put the KEEP back to 1 before leaving.

**Regression (every run):** HUD text depth (#33) unchanged. Squeeze ADS still parked unless the player already armed it (#35). No Bond sleeve. Left cube / two-hand DIG untouched.

---

## 5. Director decision

| If you say… | Then… |
|-------------|--------|
| **Green Rank 1** | Workshop chair vr444 B0 then PLAY0 (already written). Sit §4 B0 + P0 + Q. Public zip stays vr441 until PASS. |
| **Green 1 then 2** | Only if P0 strain PASS and size still huge. `GUNSCALE` default stays 1.0. |
| **Green 1 then 3** | Only if P0 strain PASS and cardboard / wall-clip remains. Prefer `GUNZBIAS` over `GUNZTEST`. |
| **Green all three tonight** | Rejected. Stacking hides which knob did the work. |
| **Crank `UNITS_PER_M`** | Rejected. Not this path. |
| **Ship GUNEYE KEEP-ON** | After P0 PASS on Quest **and** Pimax. Then boot allowlist. |
| **Revive GUNZ / HANDSOLID** | Park. Vanish below chest is a known FAIL. |
| **Merge into #33 / #35** | Rejected. |
| **Reject** | Stop. Testers keep the huge / sting gun on vr441. |

---

## 6. Attribution / legal

- No personal credit paths edited.
- No GoldenEye ROM, assets, or dumps.
- PD viewmodel numbers in GEVR textbook are **map-only**. Do not copy PD sources.
- Workshop C (vr444 GUNEYE, `geStereoXr*`, 369 sight helper) stays private until release policy flips.
