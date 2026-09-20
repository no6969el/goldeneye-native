# RESULT — Glass bullet holes sometimes one-eye only (#31) (DIG ONLY)

**Status:** DIG. **Not APPLY READY on this public tree.** No C landed.
**Ask:** After shooting glass, bullet-hole **decals** can appear in **one eye only** or look stereo-wrong. Living status / vr442 notes still list this.
**Constraints:** Do not merge into watch one-eye highlight (#58) or Dam modem flicker (#70). KEEP-ON stereo / tex / HITSNAP stay ON unless chair proves otherwise. New chair knob (if any) defaults **OFF**. `GETV_VR_IMPACTEYE` is **census only** — it does not draw a fix.
**Date:** 2026-09-20.
**Evidence:** public `goldeneye-native` HEAD, public `no6969el/GEVR` packaging + #31 / FEATURES / BETA, public `n64decomp/007` `explosion.c` / `propobj.c` / `bg.c` / `chrprop.c` / `bondview2.c`. Workshop bodies (`GETV_VR_IMPACTEYE` reader, HITSNAP snap, STEREO_REBUILD / VIEWRESTORE beyond getenv stubs) are **not on any public remote**.

Director can green-light chair A/B, reject, or park from this page alone.

---

## Verdict (one screen)

| Question | Answer |
|----------|--------|
| Draw site | **`explosionRenderBulletImpactOnProp(gdl, prop, arg2=1)`** inside prop xlu (`propobj.c` ~7337). Not wall holes. Not shards. |
| Types | Glass panes (`PROPDEF_GLASS` / `TINTED_GLASS`, door glass) are **non-penetrating**. Create uses **`impact_type = (rand%3)+0x11` → 17 / 18 / 19**. Those rows in `g_ImpactTypes` are `{6–12, apptype=1, unk1=2, unk2=1}`. |
| Why glass, not walls | Wall holes: `explosionCallRenderBulletImpactOnProp` from **`bg.c:678`**, `prop=NULL`, `arg2=0` (unk2≥2). Glass holes: **xlu only** (`unk2<2 && unk1==2`). Different pass, different matrix. |
| Stereo / per-eye | Each eye runs PRIMARY (opaque) then SECONDARY (xlu) with `GETV_STEREO_REBUILD` KEEP ON. After glass xlu, **`bondviewTransformManyPosToViewMatrix` writes s32 into `render_pos.pos` in place**. Hole draw **loads that same `render_pos` as MODELVIEW**. |
| `GETV_VR_IMPACTEYE` | **Read-only census.** Issue #31: does not draw a fix. Same class as `GETV_VR_WALLCENSUS` (DIG_OFF, forbidden in public boot). |
| APPLY tonight? | **No.** Need one Facility (or Archives) sit that splits wall vs glass, then IMPACTEYE counts, then HITSNAP=0. |

```
shot hits PROPDEF_GLASS / TINTED_GLASS / door glass
  → penetrates = FALSE
  → objHit: types 17–19 into g_BulletImpactBuffer (shared, world/model Vtx[4])
  → obj->state |= PROPSTATE_2

per eye (STEREO_REBUILD):
  bg PRIMARY  → chrpropsRenderPass(room, 2) → chrpropRender(prop, 0)   // opaque; glass with alpha SKIPS
              → explosionCallRenderBulletImpactOnProp  // WALL holes only
  bg SECONDARY→ chrpropsRenderPass(room, 1) → chrpropRender(prop, 1)   // xlu
              → explosionRenderBulletImpactOnProp(gdl, prop, 1)        // GLASS holes
              → bondviewTransformManyPosToViewMatrix(render_pos)       // f32→s32 IN PLACE
```

---

## 1. Root finding (files + functions)

### 1.1 Evidence boundary

| Layer | Where | What it proves |
|-------|--------|----------------|
| Public `goldeneye-native` | this repo | `geVrCurrentEye()` ABI (`ge_vr.h` / `ge_vr_bridge.cpp`). KEEP getenv stubs: `GETV_VR_HITSNAP` (`getv/port/src/port_render.c`), `GETV_STEREO_REBUILD` / `HUDGATE` (`getv/port/fast3d/gfx_pc.c`). **No** `explosion.c` / `propobj.c` bodies. **No** `IMPACTEYE` reader. |
| Public `no6969el/GEVR` | packaging + docs | vr441 boot: REBUILD=1, VIEWRESTORE=1, HITSNAP=2, TEXINVAL=1, WALLCENSUS=0. README / FEATURES / BETA / ROADMAP / vr442 notes: **one-eye glass holes still open**. |
| Public decomp | `n64decomp/007` | Create, type table, two draw passes, in-place `render_pos` convert. |
| Workshop (private) | `F:\Projects\GEVR\GoldenEyeVR\goldeneye-native` | `GETV_VR_IMPACTEYE` census, HITSNAP snap body, STEREO_REBUILD / VIEWRESTORE beyond stubs. **Do not push.** |

### 1.2 Create (sim, once per shot — not per eye)

**Glass is a stop, not a shoot-through:**

```
n64decomp/007 src/game/propobj.c  ~9441–9453
  if PROPDEF_GLASS or PROPDEF_TINTED_GLASS → penetrates = FALSE
  door skeleton + Switches[3] (glass pane on door) → same
```

**`objHit` when `countsAsPenetration == 0`:**

```
propobj.c ~9549–9551
  impact_type = (randomGetNext() % 3) + 0x11;   /* 17, 18, 19 */
  explosionCreateBulletImpact(hitpos, normal, impact_type,
                              room=1, hitprop, hit->room /* mtx index */, room_clear_flag);
```

`explosionCreateBulletImpact` (`explosion.c:1979`):

- Writes `g_BulletImpactBuffer[i].vertex_list[4]` in **model / room space** (prop branch rotates by `model->render_pos[mtxindex]`).
- For `unk2<2 && unk1==2` (these types): `obj->state |= PROPSTATE_2` (bit 1) so the **xlu** draw will run.
- Shared ring of 100 (`BULLET_IMPACT_BUFFER_LEN`). Not per-eye.

**Not this create path:** BG wall hits (`chrprop.c:1129`, `prop=NULL`, texture `g_HitTypeSounds`). Held-gun / hat decals (`chr.c:3565`). Paintball cheat forces type `0x10` (16) — wall-class, not 17–19.

### 1.3 Type table (why 17–19 are the glass pass)

`g_ImpactTypes[]` (`explosion.c:189`): `{width, height, apptype, unk1, unk2}`.

| Index | Size | unk1 (texSelect arg2) | unk2 (z / pass) | Draw when |
|-------|------|------------------------|-----------------|-----------|
| 0–13, 16 | 6–24 | 2 | **8** | `arg2==0` (wall / scorch neighborhood) |
| **14, 15, 17, 18, 19** | 6–12 | 2 | **1** | **`arg2==1` only** (`unk2<2 && unk1==2`) |

Glass create uses **17–19**. `texSelect(&gdl, &impactimages[type], unk1=2, unk2=1, 2)`. Doc `238` already named `s_impactimages` rows 8..15 as RGBA16 — glass rows sit in the same table.

### 1.4 Draw — two passes, one of them is glass

**Wall holes (control class):**

```
bg.c:677–678   (after PRIMARY rooms, before SECONDARY)
  explosionRenderScorchBuffer
  explosionCallRenderBulletImpactOnProp
    → explosionRenderBulletImpactOnProp(gdl, NULL, 0)
  applyRoomMatrixToDisplayList(room)   // rebuilt per eye
```

**Glass holes (this ticket):**

```
bg.c:719          chrpropsRenderPass(gdl, room, 1)     // SECONDARY / xlu
chrprop.c:562     chrpropRender(gdl, prop, withalpha=1)
propobj.c:7372    chrobjRenderProp  → sub_GAME_7F04AC20
propobj.c:7335–7338
  if (obj->state & (1 << arg2))          // bit 1 on xlu
      gdl = explosionRenderBulletImpactOnProp(gdl, prop, arg2);

explosion.c:2253–2278  (arg2==1 filter)
  render_pos = model->render_pos[model_render_pos_index]
  gSPMatrix(..., render_pos, LOAD | MODELVIEW)
  texSelect(impactimages[17|18|19], 2, 1, 2)
  gSPVertex(vertex_list) + gSP2Triangles
```

Tinted / faded glass **skips opaque** (`chrobjRenderProp`: `objAlpha < 0xFF` and `arg2==0` → return). Holes only exist on the xlu call.

**After that draw, same function:**

```
propobj.c:7354–7362  if (arg2)  /* xlu */
  bondviewTransformManyPosToViewMatrix(model->render_pos, numMatrices);
```

```
bondview2.c:10662
  copy float pos → f32_to_s32 **into render_pos[i].pos**   // IN PLACE
```

GEVR `292` named this class: second eye `already-converted`, values look like `nan` / huge — **fixed-point words read back as floats**. `303` later measured `already-converted=0` on **one route** (not a glass-hole stare). Board C2 (“who owns `render_pos` per eye”) stayed **HIGH**.

### 1.5 Stereo loop around that draw

Public ABI: `geVrCurrentEye()` (`GE_VR_EYE_LEFT=0`, `RIGHT=1`). Workshop eye loop is not in this repo.

Public boot **already ON** (vr441 template, still the pack allowlist; vr442 zip notes still list the bug):

| Knob | Ship | Role vs #31 |
|------|------|-------------|
| `GETV_STEREO_REBUILD` | **1** KEEP | Per-eye world / `render_pos` rebuild (ARM 3). Bug exists **with it armed**. |
| `GETV_STEREO_VIEWRESTORE` | **1** KEEP | Known matrix falsifier when **off** (`RUN-SHEET-295`). Ship is ON. |
| `GETV_STEREO_HUDGATE` | **1** KEEP | HUD / second-eye gate. Glass holes are **world xlu**, not HUD. Chair-cheap anyway. |
| `GETV_VR_HITSNAP` | **2** KEEP | **Hit placement.** Only ship int whose job is impacts. Body is workshop. |
| `GETV_VR_TEXINVAL` / `TEXDLRETAG` / `VFXTMEM` / `VFXSHIFT` | **1** | Same tex neighborhood as #70. Already ON during the bug. |
| `GETV_VR_WALLCENSUS` | **0** DIG_OFF | Forbidden in public boot. |
| `GETV_VR_IMPACTEYE` | **not in public boot / not in this tree** | Issue #31: read-only census. Does **not** draw. |

Do **not** set `getPlayerCount()` to 2 for stereo (glass shard buffer is `200 / getPlayerCount()`; dual-wield gates). Eye loop is inside one player.

### 1.6 Not the shard path

`glassRenderShards` (`glass.c:267`) draws **broken** triangles with `glassoverlayimage` after shatter. Different buffer (`ptr_shattered_window_pieces`), different tex. #31 is **decals on remaining glass**. Do not APPLY shard code for this.

---

## 2. Same as #58 / #70 / #29 / #55?

| | #31 glass holes | Other |
|--|------------------|--------|
| #58 watch highlight one-eye | World xlu prop + `render_pos` | HUD / scissor / HUDGATE / menu tags (`NOTE-WATCH-PER-EYE-HIGHLIGHT-MISS`) |
| #70 Dam modem | `texSelect` on **monitor** UV tick per eye | Related **tex** neighborhood only. Different object. |
| #29 Dam crates | `PROPFOGALPHA` / `OCCLSKIP` | Unrelated. |
| #55 black flicker | portal / `HEAD_TRANSLATE` room drop | Full black, not a hole quad. |

**Do not merge.** Family resemblance: “second eye sees a different draw.” Chair may still A/B TEXINVAL and HUDGATE because they are env-only.

---

## 3. Hypothesis table

| # | Hypothesis | Result |
|---|------------|--------|
| 1 | Glass holes are a **different pass** than wall holes (xlu + types 17–19 + `render_pos`) | **Source-proven.** First chair split (F0) must confirm the wear matches this, not “all impacts.” |
| 2 | Second eye loads **already-converted** `render_pos.pos` (f32→s32 in place after xlu) | **Best mechanism fit.** REBUILD + VIEWRESTORE are **already ON**. Either they do not cover this `gSPMatrix(render_pos)` site, or HITSNAP / tex wins. |
| 3 | `GETV_VR_HITSNAP=2` mutates **shared** `vertex_list` with the current eye | **Open, chair-cheap.** Create writes world/model verts once; KEEP snap body is private. `=0` is the falsifier. |
| 4 | `texSelect(impactimages[17–19])` + TEXINVAL between eyes | **Open, chair-cheap.** Do **not** C-default TEXINVAL OFF (purple explosions). |
| 5 | `GETV_VR_IMPACTEYE` is itself a missing draw | **Falsified by the issue text.** Census only. |
| 6 | `getROOMID_isRendered` / ONSCREEN drops the **whole pane** | Then the pane would vanish too. If the tester still sees glass in both eyes, this is not sufficient. |
| 7 | Shard / overlay image path | **Falsified** as the decal path. |

---

## 4. The falsifier (Director: this is the ask)

One instrument, then two env A/Bs. Restore the previous knob before the next. Unset = ship.

### F0 — split the class (no env)

Facility halls (or Archives glass). Recenter both sticks. Headset path (`Start-GEVR.bat`). **Do not** set FOVMATCH.

| | Tester sentence |
|--|-----------------|
| **W-PASS** | “Bullet holes on a **stone / metal wall** are in **both** eyes.” |
| **G-FAIL** (tonight’s bug) | “Holes on **unbroken glass** are in **one eye only**, or sit at the wrong depth / swim.” |
| **G-FAIL-pane** | “The **glass pane itself** is also missing in that eye.” → ONSCREEN / room / REBUILD, not the hole filter. Still this ticket; different APPLY. |
| **Both FAIL** | Wall and glass both one-eye → generic impact / REBUILD, not 17–19. |

Close left, then right. Write which eye is missing. Headset / OpenXR / SteamVR / HMD vs monitor on the sit.

### F1 — `GETV_VR_IMPACTEYE=1` (census; does not fix)

Workshop reader. Public boot must stay **off** (same rule as WALLCENSUS). Chair only.

| Counts | Meaning |
|--------|---------|
| Glass-type (17–19) **eye0 ≠ eye1** (one side ~0) | **Draw skip.** state bit, xlu pass, ONSCREEN, `getROOMID_isRendered`, or REBUILD not rebuilding this prop. APPLY = emit on both eyes. |
| Counts **match**, still one-eye visually | Tris left the DL. **Matrix / tex / scissor after emit.** APPLY = `render_pos` copy or skip in-place convert; or scoped tex skip. |
| Counts match **and** look correct | Census run is not the wear (wrong map / already shattered). |

IMPACTEYE **PASS as a fix** is not a legal outcome. If holes look fixed with it on, the reader is not read-only — stop and treat as a bug in the census.

### F2 — `GETV_VR_HITSNAP=0` (KEEP is 2)

| | If G-FAIL **dies** | If G-FAIL **stays** |
|--|--------------------|---------------------|
| HITSNAP=0 | Snap is poisoning shared verts or one-eye projection. **Do not** ship HITSNAP=0 (PLAY0 placement). APPLY: snap in **world / model**, once per sim, not per eye. | HITSNAP not sufficient. Restore `=2`. |

### F3 — matrix class (only if F1 says “emitted both, looks one-eye” or F0 pane-visible)

| Run | Env | Read |
|-----|-----|------|
| **3a** | `GETV_STEREO_VIEWRESTORE=0` | Known falsifier. If holes **get worse**, it is the `render_pos` class and VIEWRESTORE does not cover **this** `gSPMatrix`. Restore to 1. **Do not** ship OFF. |
| **3b** | `GETV_STEREO_MTXGUARD=1` (observe) | Glass-hole stare. If `already-converted` spikes on **eye=1** here, `303`’s zero does not cover this route (board C1a). |
| **3c** | `GETV_STEREO_REBUILD=0` | **Last, not first.** Breaks stereo broadly. Only to prove REBUILD is the remaining owner. |

### F4 — tex (only if F2 FAIL and F1 says emitted)

Same as #70 Runs 1–3: `TEXINVAL=0`, then `TEXDLRETAG=0`, then VFX pair `=0`. If holes appear in both eyes but explosions go purple → scoped skip, **KEEP stays ON**.

### F5 — monitor bat

`Play-on-monitor.bat`: if SBS / flat shows holes in **both** halves, the headset present path is in play; if still one-sided, it is the game DL.

---

## 5. APPLY sketch (DIG ONLY — not landed)

**APPLY READY? No.** Need F0 + F1. Workshop bodies are private.

### 5.1 Do not land

- Do **not** C-default HITSNAP to 0, TEXINVAL OFF, REBUILD OFF, VIEWRESTORE OFF.
- Do **not** merge with #58 / #70 / #55.
- Do **not** touch `glassRenderShards` / `getPlayerCount()`.
- Do **not** arm IMPACTEYE in public boot.

### 5.2 Smallest C (workshop, after chair)

**5.2a — if F1 counts differ:** Ensure xlu `explosionRenderBulletImpactOnProp(prop, 1)` runs for both eyes when the pane is drawn. Likely STEREO_REBUILD must `objBuildRenderState` / not skip `chrpropRender(..., 1)` on eye 1. No new KEEP.

**5.2b — if F1 counts match (likely) and F3/MTXGUARD bites:** Stop converting `render_pos.pos` in place on the same pointer the hole `gSPMatrix` uses.

```
/* NOT APPLY READY — workshop sketch, propobj.c after xlu hole draw
 * bondviewTransformManyPosToViewMatrix overwrites .pos with s32.
 * Eye 1 then gSPMatrix(render_pos) on the converted words.
 *
 * Smallest: convert into .view (the helper at bondview2.c:10650 already
 * writes .view) OR dynAllocate a Mtx copy for the hole pass only.
 * GETV_STEREO_REBUILD must still rebuild .pos as float for eye 1.
 */
```

Optional chair pin (default OFF, not KEEP):

```
GETV_VR_GLASSEYE   unset / 0 = today
                   1 = hole MODELVIEW from a copy / .view, not in-place .pos
```

**5.2c — if F2 PASS:** HITSNAP once per sim on world/model verts; do not rewrite `vertex_list` with `geVrCurrentEye()`. Keep ship `HITSNAP=2`.

**5.2d — if F4 PASS without purple:** Skip TEXINVAL on `texSelect` unk1=2, unk2=1 (glass impact), not explosion mode 4.

### 5.3 Out of scope

- Watch highlight (#58)
- Dam modem (#70) / tanks (#55) / crates (#29)
- Shards, blood, scorches (except as F0 control)
- Boot allowlist / pack smoke until a sit PASS

---

## 6. Chair stare — plain tester sentences

**Setup:** public **vr442** / Latest. `Start-GEVR.bat`. Facility (halls with glass). Recenter both sticks. Right hand a gun. Do **not** shatter the pane on the first look — **holes on remaining glass**.

| | Tester sentence |
|--|-----------------|
| **F0 W** | “Wall holes: **both** eyes.” |
| **F0 G** | “Glass holes: **one** eye missing or stereo-wrong. The pane is still there in both.” |
| **F1** | IMPACTEYE on: write the two eye counts (or the banner). Holes should look **the same as F0 G** (census, not a fix). |
| **F2** | HITSNAP=0: “Glass holes now in **both** eyes” **or** “still one-eye.” |
| **F-FAIL** | “I broke the glass; I am looking at flying shards.” → restart; this ticket is the **decal**. |

**Regression every run:** wall holes, gun aim, explosions still vr442-coloured, no full-screen black.

Headset / OpenXR / SteamVR on or off / HMD vs monitor — write them on the sit. No ROM.

---

## 7. Director decision

| If you say… | Then… |
|-------------|--------|
| **Green chair only** | Sit F0, then F1, then F2. Stop. |
| **Green 5.2b if F1 match + F3 bite** | Workshop: do not in-place convert hole MODELVIEW. `GLASSEYE` default OFF. |
| **Green 5.2c if F2 PASS** | HITSNAP world-once; keep ship `=2`. |
| **Green 5.2a if F1 counts differ** | REBUILD / xlu emit on eye 1. |
| **Merge #58 / #70** | Rejected. |
| **Ship IMPACTEYE or HITSNAP=0** | Rejected. |
| **APPLY tonight from this repo** | **No.** No workshop `propobj.c` / `explosion.c` here. |

---

## 8. Attribution / legal

- No personal credit paths edited.
- No GoldenEye ROM, assets, or dumps.
- Decomp citations are public `n64decomp/007` (`explosion.c` / `propobj.c` / `bg.c` / `chrprop.c` / `bondview2.c` / `glass.c`).
- Workshop C stays private until release policy flips.
