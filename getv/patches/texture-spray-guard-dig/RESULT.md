# RESULT — Systemic texture-spray guard (DIG ONLY)

**Status:** DIG. **Not APPLY READY on this public tree.** No C landed.
**Ask (2026-09-20, widened):** Texture scraps / flashes that **spray across vision** (HMD) or land in a **far corner** (flat). Owner is tired of finding each one by playing every action on every level. Wants the **engine unable to do that class**, not a per-prop / per-stage whitelist. Related: [GEVR #70](https://github.com/no6969el/GEVR/issues/70) (Dam convert-modem is one specimen, not the whole class).
**Ship stays:** `GETV_XR_HEAD_TRANSLATE=0` (vr442 boot). `GETV_VR_SKYMESH=0` (#72 KEEP). `GETV_VR_TEXINVAL=1` (+ TEXDLRETAG / VFXTMEM / VFXSHIFT / TEX16BE) on **VR boot and Play-on-monitor** (#51 explosions PASS). **Do not** turn TEXINVAL off globally.
**Do not** re-sit `GETV_VR_MONFRAME` (REJECT / white verts). **Do not** Dam-global `GETV_STEREO_MTXGUARD=2` (#55 Facility/Bunker bats only).
**Date:** 2026-09-20.
**Evidence:** public `goldeneye-native` HEAD (KEEP getenv stubs + prior DIG PRs #5 / #6 / #27 / #29 / #11 / #23), public `no6969el/GEVR` #70/#51/#31/#34/#72 + vr442 boot + Play-on-monitor.bat, public `n64decomp/007` `texSelect` / monitor / explosion / sky / HUD. Workshop `gfx_pc.c` TEXINVAL **body** and `stereo.c` eye loop stay private.

Director can green a **class chair** (`TEXGUARD` and/or `SCRAPDROP`), park the leftover, or take the small `MONINVAL` subset from this page alone.

Owner add (same day): also rank a **safety net** that drops stray verts / out-of-bounds / screen-spray scraps so the engine refuses to draw garbage, layered with (not replacing) the TMEM path.

Prior #70 pages (specimens, not this class):

| PR | Page | What it closed |
|----|------|----------------|
| #5 | `getv/patches/dam-modem-flicker-dig/RESULT.md` | Downstairs frustum + per-eye `MonitorRecord` tick |
| #6 | `getv/patches/dam-modem-monframe-apply/` | `MONFRAME` APPLY — **chair REJECT**. Leave **OFF**. |
| #27 | `getv/patches/dam-modem-residual-dig/RESULT.md` | PARK HT0. Attach-white / `PROP_CHRBUG` near-field |
| #29 | `getv/patches/dam-modem-lefteye-dig/RESULT.md` | Left eye first = cold TMEM/TEXINVAL on monitor `texSelect`. Ranked park > chair TEXINVAL=0 > later MONINVAL |

This page **supersedes PR29’s “later MONINVAL only” rank.** MONINVAL stays a **subset falsifier**. The lead APPLY is a **shared-state guard** in Fast3D.

---

## Verdict (one screen)

| Question | Answer |
|----------|--------|
| Root class | **One Fast3D texture cache + current RDP tile / scissor, mutated by every `texSelect`, shared across sequential draws and across stereo eyes.** Leftover bind or mid-DL evict paints the **next** primitive (or the other eye’s half-FBO) with the previous tile. That is spray / scrap. |
| Same on HMD and flat? | **Same mechanism, different viewport geometry.** HMD: leftover fills a large per-eye rect → “through vision.” Flat: leftover is clipped to the 320-class scissor / unused window → “far-corner scrap.” Not a different bug. |
| Why per-prop knobs fail the ask | `MONINVAL` / glass-impact skip / HUD-only unbind each close **one call site**. The next animating `texSelect` (another monitor, explosion-adjacent, ammo stamp, sky tile) sprays again. Owner would still have to play every action. |
| System fix (root, long-term) | **`GETV_VR_TEXGUARD`** default **OFF**: per-eye TMEM/cache snapshot + eye-boundary unbind/scissor restore + **deferred inval once per sim tick**. Explosion mode 4 still invals (KEEP #51). **Texels + scissor state.** |
| Safety net (belt) | **`GETV_VR_SCRAPDROP`** default **OFF**: discard tris / texrects whose **verts** are NaN, saturated / already-converted, or outside an expanded clip. **Verts, not texels.** Does **not** erase leftover pixels already in the FBO. |
| One knob or two? | **Two knobs.** Different mechanisms. Do **not** pack both into `TEXGUARD=2` (MTXGUARD-mode lesson). Chair each alone, then both. One APPLY PR may land both getenv stubs. |
| Smallest falsifier (no C) | Existing `GETV_VR_TEXINVAL=0` (KEEP stays ON). If spray dies and explosions stay orange → greens TEXGUARD. If spray dies and explosions go purple → still greens TEXGUARD (must keep mode-4 inval). |
| Subset | `GETV_VR_MONINVAL=1` — skip inval **only** on monitor `texSelect` (`arg2` 1/2, `arg3` 8). Chair-cheap if Director wants a modem-only first C. **Not** the class close. |
| APPLY tonight? | **No.** Workshop body is private. Sketch is APPLY READY **on the workshop** after one class chair. |

```
sim once, draw twice          LEFT=0 then RIGHT=1   (one gfx texture cache)
any ONSCREEN texSelect        gDPSetTextureImage + LoadBlock + cache lookup
  monitors                    IMGTEXT / FIXED_MONITOR 8   UV tick per eye
  explosions / scorches       genericimage mode 4         KEEP TEXINVAL
  glass / wall impacts        impactimages[]
  HUD stamps / glyphs         texSelect + TEXRECT
  sky / water tiles           skywaterimages[]
TEXINVAL=1 (ship both bats)   evict/retag on the SHARED cache  (cold = left)
second eye / next draw        same hashmap + last tile + last scissor
leak                          HMD = spray across the eye rect
                              flat = scrap in the far corner of 320/window
TEXGUARD=1 (sketch)           snapshot per eye; unbind at eye edge;
                              inval once per sim, not per eye / per leftover
SCRAPDROP=1 (sketch)          drop NaN / sat / already-converted tris
                              (verts). Misses valid-tri + wrong texel.
                              Misses leftover scissor pixels already written.
```

---

## 1. Why this is a class, not a Dam modem

### 1.1 One function loads every “special” picture

Public decomp `othermodemicrocode.c` `texSelect(Gfx **gdlptr, sImageTableEntry *tconfig, u32 arg2, s32 arg3, u32 ulst)`:

1. `texSetRenderMode` (cycle / Z / xlu).
2. `gSPTextureL` / combiner from format.
3. `gDPSetTextureImage` + `gDPSetTile` + **`gDPLoadBlock` into TMEM**.
4. Tile size with `ulst` (monitor path passes `2`).

`tconfig == NULL` → shade-only (`G_CC_SHADE`) — the MONFRAME white, not this class.

There is **no per-prop texture isolator**. Fast3D (workshop `gfx_pc.c`) implements that LoadBlock as `gfx_texture_cache_lookup` + `import_texture_*`. One process-global hashmap. Eye is not a key.

### 1.2 Call-site families (what one APPLY must cover)

| Family | Site | `texSelect` shape | Why it can spray |
|--------|------|-------------------|------------------|
| **Monitors / TVs** | `propobj.c` `process_monitor_animation_microcode` | `texSelect(&gdl, tconfig, arg5=1\|2, arg4=mN, 2)` — Dam `PROP_MODEMBOX` is `PROPFLAG_FIXED_MONITOR` → **`mN = 8`**, `MONUSEIMAGE(IMGTEXT)` | Shared `MonitorRecord` UV/colour **mutated during each eye draw**. Cold first-eye load + TEXINVAL. Specimen = #70. |
| **Explosion / fire / scorches** | `explosion.c` ~1850 | `texSelect(..., genericimage, 4, 1, 2)` | Mode **4**. KEEP TEXINVAL / VFXTMEM / TEX16BE exist **for this**. Must stay ON (#51). |
| **Bullet impacts** | `explosion.c` ~2230 | `texSelect(..., &impactimages[type], unk1, unk2, 2)` | Glass 17–19 is `unk1=2, unk2=1` (xlu). Same cache as #70. One-eye holes may still be `render_pos` (#31) — leftover **spray** from this `texSelect` is this class. |
| **HUD stamps** | `gunfire.c` ammo + `textrelated.c` glyphs | `texSelect` then `gSPTextureRectangle` | 2D. Leftover texrect / last tile = **corner scrap** on flat, spray on HMD if scissor is the eye rect. Fat/speck of #34 is **import stride**, not this class. |
| **Sky / water** | `sky.c` | `texSelect(&skywaterimages[SkyImageId])` (and water when `IsWater`) | Extra TMEM/vtx. `SKYMESH=0` already cut #72 blue **and** shrunk #70 leftover (contention). Do not restore. Fill leftover is #72, not spray. |
| **Glass shards** | `glass.c` `glassoverlayimage` | Different buffer | Shatter overlay. Same cache. Not #31 decals. |
| **Blood / other VFX** | workshop + `GETV_VR_BLOODINVAL` (boot **wiped**) | Same neighborhood | Do **not** arm BLOODINVAL. TEXGUARD covers the cache without a blood whitelist. |

`sub_GAME_7F04AC20` (only if `PROPFLAG_ONSCREEN`):

- `PROPDEF_MONITOR` / `MULTI_MONITOR` + `mrData->flags & 1` (opaque) → `process_monitor_animation_microcode`.
- Ends in the `texSelect` above, then `gSPMatrix(model->render_pos)` + 4 verts.

Frustum gate explains “look toward it from downstairs”: no ONSCREEN → no `texSelect` → no leak **from that prop**. Any other in-frustum family still can leak. That is why a modem-only skip does not make the engine safe.

### 1.3 Shared state that leaks (inventory)

| Shared thing | Who mutates it | Leak |
|--------------|----------------|------|
| Fast3D **texture hashmap** | Every `texSelect` / TEXINVAL evict / TEXDLRETAG | Next lookup gets evicted name, half-imported texels, or the previous image |
| **Current tile / combiner** | `gDPSetTile` / `SetCombine` inside `texSelect` | Next primitive that does **not** re-`texSelect` (or whose cache hit is stale) draws the leftover |
| **Scissor / viewport** | `geStereoEyeViewport` (half-width HMD); `c_screen*` 320-class flat; HUDGATE texrects | Last rect not restored → leftover pixels in the **other eye** or the **unused window corner** |
| `MonitorRecord` (game) | `process_monitor_animation_microcode` per eye | Double UV tick (crawl). MONFRAME tried to wrap this and went **white**. Do not re-sit. |
| One-view **dyn / Gfx pools** (docs 292) | `dynAllocateVertices(4)` per monitor per eye; sky remesh | First-eye verts stomped. `VTXGUARD=64` KEEP. `SKYMESH=0` already helps. |
| **SrcFbo** (`GETV_XR_PLAY_SRCFBO=1` + SS3) | Second eye overwrites the shared colour target | Left present can show right’s leftover bind. Diagnostic: SS=1. Do not ship SS1. |

N64 TMEM was 4 KB and Rare reloaded it on every `texSelect`. Fast3D **caches** that reload. Stereo + TEXINVAL (evict) + animated tiles (monitor scroll, explosion frames, HUD flipY stamps) is a combination the N64 never ran.

---

## 2. HMD spray vs flat far-corner scrap — **same mechanism**

| | Headset (`Start-GEVR.bat`) | Flat (`Play-on-monitor.bat`) |
|--|----------------------------|------------------------------|
| Stereo | `GETV_STEREO=1`, LEFT then RIGHT | `GETV_STEREO=0`, one view |
| Target | One FBO, half-width eyes (`geStereoEyeViewport`); SrcFbo + SS3 | One 320-class viewport; N64-shaped |
| TEXINVAL stack | Boot §9 **ON** | **Now also ON** (#51 seed). Spray is **not** “flat missing TEXINVAL.” |
| How leftover reads | Eye rect is large → leftover tile / scissor **fills vision** (both eyes if the bind survives the eye edge) | Leftover is clipped to the 320 scissor or sits in the unused window → **far-corner scrap** |

**S1 (same class):** a `texSelect` leaves tile + cache + scissor dirty. The next draw or the other eye samples it.

**S2 (not this class):** a 3D mesh wearing the wrong texture for many frames on **world geometry**. That would show in the **middle** of the flat view too, not only a corner.

Owner report (headset sprays; flat often only a far-corner scrap) matches **S1**. Chair split (below) confirms: if `TEXGUARD=1` (or `TEXINVAL=0`) kills **both** the HMD spray **and** the flat scrap on the same action, S1 is proven. If only HMD dies, look at SrcFbo / eye-scissor. If only flat dies, look at window vs 320 letterbox (still TEXGUARD unbind, still this APPLY).

This is **screen-space leftover** (tile + scissor + cache), not a second Dam-modem UV theory. The modem is just the loudest `texSelect` in the frustum.

---

## 3. What already shipped (do not break)

| Sit | Result | Keep |
|-----|--------|------|
| `HEAD_TRANSLATE=0` | Downstairs modem **strobe** mostly gone | **PARK / vr442 boot.** Do not restore `=1`. |
| `SKYMESH=0` | #72 jump-strip blue **PASS**; #70 leftover **shrunk** | **#72 KEEP.** Contention evidence for this class. Do not restore. |
| TEXINVAL / TEXDLRETAG / VFXTMEM / VFXSHIFT / TEX16BE `=1` | #51 explosions orange on **VR and Play-on-monitor** | **KEEP.** Do not C-default OFF. Do not wipe from the monitor bat. |
| `MONFRAME=1` | **REJECT** — white verts (`tconfig=0` IMGBOND × 0xFF) | Never boot, never KEEP, **do not re-sit.** |
| Dam `MTXGUARD=2` | Tunnel **blue** | Facility/Bunker bats only. |
| PR29 left-eye leftover | First-eye cold TMEM/TEXINVAL | Explains **which eye**. Does **not** close spray on later draws. |

`GETV_TMEMMAP=0` (boot wipe). `GETV_VR_BLOODINVAL` wiped. `GETV_TILE1` / `BASETILE=1` ship with the VFX stack — do not sit them off on this chair.

---

## 4. Hypothesis table (the class)

| # | Hypothesis | Result |
|---|------------|--------|
| C1 | Shared Fast3D cache + current tile leak across draws / eyes | **Best class fit.** One APPLY site (`gfx_pc.c`). Covers monitors, impacts, HUD stamps, VFX-adjacent. |
| C2 | TEXINVAL evict on **non-explosion** `texSelect` is the trigger | **Open, chair-cheap.** Stack is ON during the bug. `TEXINVAL=0` is the falsifier. Do **not** ship OFF. |
| C3 | Eye-boundary scissor / SrcFbo leftover (HMD spray, flat corner) | **Same APPLY** (unbind + restore scissor at eye begin/end). SS=1 diagnostic only. |
| C4 | Shared `MonitorRecord` UV tick | **Source-proven** as **crawl on the quad**, not as vision spray. MONFRAME REJECT. Later `MONSCROLL` only if L0-BOTH and T-FAIL. |
| C5 | PR27 `PROP_CHRBUG` embed / attach white | **Different leftover.** Near-field pop. Not spray across vision. |
| C6 | #55 portal / HT / #72 sky fill / #31 `render_pos` / #34 RGBA32 line | **Other classes.** TEXGUARD must not “fix” them and must not regress them. |
| C7 | Per-prop whitelist (MONINVAL only, glass-impact skip, …) | **Rejected as the close.** Valid **subset** A/B. Owner ask is engine-level. |
| C8 | Safety net: drop stray verts / insane screen pos (owner add) | **Layer, not a substitute.** Catches **NaN / sat / already-converted** tris (docs 292; glass `render_pos` s32-as-f32). **Misses** valid-tri + wrong texel (C1) and leftover **scissor pixels**. Two knobs. |

---

## 5. System hardening (lead) vs subset knobs

### 5.1 One APPLY — `GETV_VR_TEXGUARD` (default OFF)

Workshop `getv/port/fast3d/gfx_pc.c` next to `ge_vr_texinval()`. **Not** `propobj.c`. **Not** a cmdlist wrap.

```
GETV_VR_TEXGUARD   unset / empty / 0 = OFF   (retail: one shared cache, inval every texSelect)
                   1 = class guard (below)
Banner once: [getv][texguard] GETV_VR_TEXGUARD=1
```

When `=1`:

1. **Per-eye private TMEM snapshot.** Cache lookup key includes `geVrCurrentEye()` **or** (cheaper) snapshot/restore the “current texture id + tile + combiner” at eye begin/end. Second eye must not evict first-eye names while SrcFbo still presents them.
2. **Deferred inval, once per sim tick.** `ge_vr_texinval()` still returns KEEP ON. The **evict** runs on `gePortSimShouldTick()` / first eye only. Eye 1 reuses the already-rebuilt upload. Explosion mode 4 (`arg2==4`) still invals on that sim tick — #51 stays.
3. **Stereo-safe `texSelect` wrapper.** After every LoadBlock path: if the next primitive is a different image, do not keep the previous GL bind. At eye boundary: unbind + restore scissor to that eye’s rect (HMD half / flat 320). No leftover tile survives into the other eye or the window corner.
4. **TEXDLRETAG** stays KEEP ON but retag **once per sim**, not per leftover `texSelect`.

That is the “game cannot spray textures” switch. One C, all families in §1.2.

Sketch: `getv/patches/texture-spray-guard-dig/gfx_pc_texguard.snippet.c`.

### 5.2 Safety net — `GETV_VR_SCRAPDROP` (default OFF) — **verts, not texels**

Owner idea: refuse to draw garbage (bad UVs, saturated verts, corner scraps, full-vision flashes) so the engine does not need per-level hunts.

**Name the leftover honestly** — three different things look like “spray / scrap”:

| Leftover | What is wrong | Who catches it | Who misses it |
|----------|---------------|----------------|---------------|
| **Texels** | Valid verts, **wrong GL bind** / stale cache after `texSelect` | `TEXGUARD` (C1) | `SCRAPDROP` — the tri is sane |
| **Verts** | NaN, Inf, saturated clip, docs-292 **already-converted** (s32 words read as f32), stomped `dynAllocate` | `SCRAPDROP` at `gfx_sp_tri1` | `TEXGUARD` — cache is fine |
| **Scissor pixels** | Last eye / texrect already **written** into the FBO | `TEXGUARD` eye-boundary **unbind + restore scissor** (maybe a dest-rect clamp on TEXRECT) | `SCRAPDROP` — no live verts to drop. A post-process “wipe the corner” is a fourth mechanism; do **not** start there (eats HUD). |

`GETV_VR_VTXGUARD=64` is a **heap poison pad**, not a draw filter. Do **not** reuse that name. Do **not** sit `=0`.

N64 `G_CULLDL` / `triangleRejected` (outcode `0x7030`) already drops **fully off-screen** runs. A corner scrap that the tester **sees** has mixed outcodes — it is **on screen**. Hardware cull will not eat it.

```
GETV_VR_SCRAPDROP   unset / empty / 0 = OFF   (retail: draw the tri)
                    1 = reject NaN / Inf / sat / already-converted
                        and screen pos outside ~8× the current eye viewport
Banner once: [getv][scrapdrop] GETV_VR_SCRAPDROP=1
```

**Safe rejects (generic — no prop list):**

1. Any clip or screen component is NaN / Inf.
2. Already-converted class: `|clip|` or `|screen|` huge (s15.16 bits as float), or `w <= 0` still submitted as a filled tri.
3. All three verts project **outside** an expanded current-eye viewport (not “tiny island in a corner”).

**Do not** reject “AABB < N px in a corner.” Ammo icons are **5×12**. Status text is lower-left. Dam vista is a few pixels. That heuristic **is** a per-HUD hunt.

**Do not** reject “insane UV.” `monAnim05GreenTextUp` `MONVERTSCROLL` *is* large `s`/`t` (`width * (xmid ± frac) * 32`). That would drop the modem **picture**.

Site: workshop `gfx_pc.c` `gfx_sp_tri1` / `gfx_draw_rectangle` (TEXRECT dest, not game `propobj.c`). Same file as TEXGUARD. **Two getenv.**

### 5.3 One falsifier or two?

| Packing | Verdict |
|---------|---------|
| One knob `TEXGUARD=1` does both | **No.** Chair cannot tell texel leak from sat verts. |
| `TEXGUARD=2` means +scrapdrop | **No.** MTXGUARD 1=observe / 2=skip already burned Dam. |
| **Two knobs, both default OFF** | **Yes.** One APPLY PR may add both stubs. Wear **D** then **G** then **D+G**. |

Long-term preferred: **TEXGUARD** (root). **SCRAPDROP** stays the belt for sat/NaN that isolation will never fix (glass `render_pos`, dyn stomp). Layer is legal. Substitute is not.

### 5.4 Subset — `GETV_VR_MONINVAL` (default OFF)

PR29 sketch, still valid as a **small falsifier**:

```
GETV_VR_MONINVAL   unset / empty / 0 = OFF
                   1 = skip TEXINVAL / TEXDLRETAG on monitor texSelect only
                       (arg2 1 or 2, arg3 8). Mode 4 unchanged.
Banner once: [getv][moninval] GETV_VR_MONINVAL=1
```

Closes Dam modem **if** C2 is monitor-only. Does **not** close glass-adjacent spray, HUD corner scraps, or the next TV. Do not KEEP. Do not boot.

### 5.5 Do not land

| Knob / move | Why |
|-------------|-----|
| `GETV_VR_MONFRAME=1` / retune | Chair REJECT. Wrong gate (`gePortSimShouldTick` during **draw**). White verts. |
| Dam `GETV_STEREO_MTXGUARD=2` | Tunnel blue. Not a texture bind. |
| C-default TEXINVAL / VFX / TEX16BE **OFF** | Purple explosions (vr440 / #51). |
| `HEAD_TRANSLATE=1` / `SKYMESH=1` | Reopens #70 downstairs / #72 blue. |
| `GETV_STEREO_REBUILD=0` / `GETV_STEREO=0` | Breaks fusion. Owner F0 FAIL on watch. |
| `GETV_VR_BLOODINVAL` / `TMEMMAP=1` | Wiped / off. Not a new whitelist. |
| Per-stage allowlist of props | Owner ask forbids it as the close. |
| “Drop tiny corner AABB < N px” | Eats HUD (5×12 ammo) and Dam vista. Not generic. |
| Post-process wipe of a corner island | Scissor-pixel erase. Eats HUD. Fourth mechanism. Do not start. |
| Reuse `GETV_VR_VTXGUARD` as a draw filter | Wrong knob (64 KB poison). KEEP 64. |

### 5.6 Later UV-once (not this class, not MONFRAME)

If chair T/G **FAIL** and leftover is **crawl on the screen in both eyes** (PR29 L2): new `GETV_VR_MONSCROLL`, gate `geVrCurrentEye()==LEFT`, skip scroll increments only, **always** `TVCMD_SETTEXTURE` + `texSelect`. Do **not** reuse `MONFRAME`.

---

## 6. Chair — one sitting for the **whole class**

**Setup:** vr442 Latest. Boot already HT=0, SKYMESH=0, TEXINVAL stack ON. No MONFRAME. No Dam MTXGUARD=2. No FOVMATCH.

**Ship still holding (write, do not fail the class on these):**

| | Tester sentence |
|--|-----------------|
| **S-PASS** | “Downstairs under the towers, looking up at the dish, no every-frame strobe.” (HT0) |
| **B-NOTE** | “Jump-strip turn: no blue flash.” (#72) |
| **E-PASS** | “Explosions still look like Latest (orange, not purple).” (#51) |

**Class (same actions on HMD, then `Play-on-monitor.bat`):**

| | Tester sentence |
|--|-----------------|
| **M-HMD** | Dam 007, attach convert modem, look from distance **and** on the dish: “No scrap / flash **through vision**.” |
| **M-FLAT** | Same attach, flat: “No far-corner scrap.” |
| **G-HMD** | Facility (or Archives) shoot **unbroken glass**: “No leftover texture sprayed across the view.” (Holes may still be one-eye — that is #31; write it.) |
| **H-HMD** | Combat HUD visible: “No stamp / glyph leftover in a corner or across the eye.” **HUD itself still draws** (ammo 5×12, status text). |
| **X-HMD** | One explosion in view: “Fire stays orange. No extra scrap besides the flare.” |

**Knob runs** (one change, restore after). Unset = ship.

| Run | Env | If **M+G+H scraps die** | If scraps **stay** |
|-----|-----|-------------------------|--------------------|
| **T** | `GETV_VR_TEXINVAL=0` | C2. Check **E-PASS**. Do **not** ship OFF. Greens **TEXGUARD**. | Restore. Not global inval. Go D or G. |
| **R** | `GETV_VR_TEXDLRETAG=0` | Retag. Same: scoped/deferred, KEEP ON. | Restore. |
| **D** | `GETV_VR_SCRAPDROP=1` only (TEXGUARD unset) | **Vert class.** See §6.1. | Restore. Not sat/NaN verts. Go G. |
| **G** | `GETV_VR_TEXGUARD=1` only (SCRAPDROP unset) | **Texel/scissor class.** Wear default OFF until KEEP talk. | Isolation insufficient; note which family remains. |
| **DG** | both `=1` | Layer PASS if D or G was partial. | Both miss — C4 UV or C6. |
| **N** | `GETV_VR_MONINVAL=1` only | **Modem-only.** If G/H still scrap → subset confirmed, still need TEXGUARD. | Restore. Not monitor-inval. |
| **S** | `GETV_SUPERSAMPLE=1` (restore **3**) | SrcFbo / first-eye present. **Do not** ship SS1. | Restore 3. |

**PASS for the texel class:** G (or T without purple) kills **both** HMD spray and flat corner on **modem + one non-monitor family**, and **E-PASS** + HUD still visible.

**PASS for the vert safety net:** D kills **M-FLAT or M-HMD** **and** one second family, HUD/vista still there. §6.1.

**FAIL for the class:** scraps remain with **DG** and TEXINVAL=0 → not cache/tile and not sat verts (revisit C4 UV or C6). **Park** that specimen; do not invent a prop whitelist.

### 6.1 Smallest chair that proves the safety net (no per-asset list)

The C has **no** `PROP_MODEMBOX` / glass / HUD branch. Chair uses two **specimens** of the same generic reject.

**Wear:** Latest. `set GETV_VR_SCRAPDROP=1` only. No TEXGUARD. No MONINVAL. No MONFRAME. No MTXGUARD=2.

| | Action | PASS sentence |
|--|--------|----------------|
| **D1** | Dam 007, attach convert modem, stare from distance + on dish. Flat **and** HMD. | “Modem corner scrap / vision spray is **gone**.” |
| **D2** | **Second known spray, different `texSelect` family** — Facility unbroken glass leftover, **or** a downstairs Dam **TV** if glass has no leftover that day. Not a new whitelist in C. | “That second leftover is **also gone**.” |
| **D-HUD** | Combat HUD on the same sitting. | “Ammo stamp and status text still **draw**. No new missing corner of the real HUD.” |
| **D-VISTA** | Dam far terrain / bridge from the dish. | “The vista is not punched out.” |
| **E-PASS** | One explosion. | Still orange. |

| D1+D2 | Meaning |
|-------|---------|
| **Both die, HUD/vista hold** | Safety net is a **class** vert reject. Layer with TEXGUARD. Do **not** KEEP yet. |
| **Only D1 dies** | Heuristic is still modem-shaped (too tight) or D2 was texel-only. Do not ship as the close. |
| **Neither dies** | Scraps are **texels / scissor pixels** (C1). SCRAPDROP is the wrong layer. Go G. |
| **HUD or vista dies** | Reject is too wide (corner-AABB leaked in). **FAIL.** Tighten to NaN/sat only; do not ship. |

---

## 7. What still needs a per-asset dig (hopefully little)

| Still per-asset | Why TEXGUARD will not close it |
|-----------------|--------------------------------|
| #51 purple if TEX16BE off | **Byte-order / decode.** KEEP stays. TEXGUARD must not flip TEX16BE. |
| #55 Facility/Bunker **black** | Portal / room drop / HT. Not a tile leak. MTXGUARD=2 stays bat-only. |
| #72 Dam **blue** fill | Sky fill / scissor. SKYMESH=0 already PASS. |
| #31 glass holes **one-eye** | Likely in-place `render_pos` convert, not leftover bind. TEXGUARD may still eat a **spray** around the pane. `SCRAPDROP` may eat **sat** hole tris (already-converted) — write it; do not call that a hole **placement** fix. |
| #34 ammo **fat / speck** | Dest-X aspect + RGBA32 `line` vs `import_texture_rgba32`. Wrong pixels, not leftover. |
| #29 crate pop | Fog / OCCLSKIP. |
| #58 watch highlight | Per-eye input toggle, not TMEM. |
| Attach **white** (PR27) | `PROP_CHRBUG` near-field. BUGHIDE if Director wants that pop. |
| Monitor **UV crawl both eyes** (L2) | Game `MonitorRecord`. `MONSCROLL` later, not TEXGUARD. |

If the class chair PASSes, owner should not need a new ticket for “this TV on this stage” or “this HUD stamp leftover.” New tickets stay for the table above.

---

## 8. Rank

| Rank | Move | When |
|------|------|------|
| **1. Root harden `TEXGUARD` default OFF** | **Preferred long-term.** Per-eye / once-per-sim tex state. Texels + scissor. One sitting (§6 run G). | Owner “engine cannot spray” ask. |
| **2. Safety net `SCRAPDROP` default OFF** | **Belt.** Drop NaN/sat/already-converted tris. Verts. Chair **D** then **D1+D2** (§6.1). Layer after each-alone, never as TEXGUARD=2. | Owner add. Catches garbage TEXGUARD will never see. |
| **3. Chair existing `TEXINVAL=0` only** | No C. Proves C2. KEEP stays ON. | Before any workshop edit. |
| **4. Subset `MONINVAL`** | Modem-only skip. Default OFF. | Smaller first C, or T-COST with only M-HMD dying. **Not** the close. |
| **5. Park leftover** | Honest if **DG** + T FAIL and scraps are slight. HT0 + SKYMESH=0 stay ship. | After the class chair, not instead of ranking 1–2. |

Park does **not** unblock MONFRAME or Dam MTXGUARD=2.

---

## 9. Director decision

| If you say… | Then… |
|-------------|--------|
| **Green class chair (T, then D, then G, then DG)** | **Recommended.** Sit §6 + §6.1. No C until those runs. |
| **Green `TEXGUARD` APPLY (default OFF)** | Workshop `gfx_pc.c` after T-PASS / T-COST / G. Root harden. KEEP TEXINVAL ON. |
| **Green `SCRAPDROP` APPLY (default OFF)** | Same file, **second** getenv. After D1+D2 PASS and D-HUD / D-VISTA hold. Layer, not substitute. |
| **Green both stubs in one APPLY PR** | Allowed. Still **two knobs**, both default OFF. |
| **Pack scrapdrop into `TEXGUARD=2`** | **Rejected.** |
| **Green `MONINVAL` only** | Allowed as subset. Expect G/H scraps to remain. |
| **Park leftover / close #70 as residual-known** | Legal after HT0. Does **not** answer the widened ask. |
| **Green MONFRAME again / Dam MTXGUARD=2** | **Rejected.** |
| **Flip TEXINVAL C-default OFF** | **Rejected.** |
| **Merge #51 / #55 / #72 / #31 / #34 into this C** | **Rejected.** Same neighborhood ≠ same ticket. TEXGUARD must not regress them. |
| **APPLY tonight from this repo** | **No.** Public tree has no workshop `gfx_pc.c` body. |

---

## 10. Attribution / legal

- No personal credit paths edited.
- No GoldenEye ROM, assets, or dumps.
- Decomp citations are public `n64decomp/007` (`othermodemicrocode.c` `texSelect` / `texSetRenderMode`; `propobj.c` `process_monitor_animation_microcode` / `sub_GAME_7F04AC20`; `explosion.c` mode 4 + `impactimages`; `sky.c` `skywaterimages`; `gunfire.c` / `textrelated.c` TEXRECT; `initobjects.c` `g_InitialMonitorAnimController`).
- GEVR textbook: `docs/292` (eye 0 first, one-view pools), `docs/231`–`232` (TEXRECT), packaging `gevr-vr442-boot.cmd` §8–§9, `Play-on-monitor.bat` #51 seed.
- Workshop C stays private until release policy flips.
