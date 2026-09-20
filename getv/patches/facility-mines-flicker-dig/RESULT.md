# RESULT — Facility end mine-throw black flicker (#55) (DIG ONLY)

**Status:** DIG. **Not APPLY READY.** No C landed. No Dam boot / C-default change.
**Ask:** Facility **very end**, when you **throw the mines** at the gas tanks (bottling-tank objective), **black flicker**. Owner wondered if Dam modem / `MONFRAME` helps — Director: **no**; this is #55 `PROP_GASTANK` family, not monitor UV.
**Constraints:** Do not re-globalize `GETV_STEREO_MTXGUARD=2` onto Dam (tunnel **blue**). Do not merge into #70 `MONFRAME` (REJECTED) or #72 Dam bridge blue. Bunker bats already keep `=2`. C-default / public boot stay **unset / 0** for MTXGUARD. `HEAD_TRANSLATE` Latest pin stays `0` (do not restore `1` for this stare).
**Date:** 2026-09-20.
**Evidence:** public `goldeneye-native` HEAD (after #5/#6), public `no6969el/GEVR` #55/#70/#72 + packaging, public `n64decomp/007` Facility setup + throw/draw. Workshop `GETV_STEREO_MTXGUARD` **body** is **not on any public remote**. Chair Facility MTXGUARD=2 stare is **UNRUN** on this dig.

Director can green-light the Facility chair bat, reject C, or send a Dam-safe stage gate from this page alone.

---

## Verdict (one screen)

| Question | Answer |
|----------|--------|
| Same MTXGUARD=2 root as Bunker #55? | **Same function, different caller. Likely yes for throw flicker.** Bunker PASS was black shirts / broken matrix = in-place `f32→s32` run twice per stereo frame. Facility tanks + thrown mines go through **the same** `bondviewTransformManyPosToViewMatrix`. |
| Portal / `HEAD_TRANSLATE` room drop? | **Weaker for this leftover.** Early #55 DIG (2026-09-18) named eye-past-portal black. vr442 Latest boot now pins `GETV_XR_HEAD_TRANSLATE=0`. If throw flicker still happens on Latest, lean is not sufficient. Do not disable `DRAWALL` / `PORTALWIDE`. |
| Dam modem / `MONFRAME`? | **No. Falsified as the mechanism.** Tanks are `PROPDEF_GAS_RELEASING` (36), not `PROPDEF_MONITOR`. Chair already **REJECTED** `MONFRAME=1`. Keep #70 split. |
| Safe APPLY tonight? | **No C.** Chair **Facility bat** `GETV_STEREO_MTXGUARD=2` only (same knob as Bunker bats). Dam boot stays unset. If that PASS, optional later C is a **Dam denylist**, not a global `=2` C-default. |
| Prop-only gate? | **Incomplete vs Bunker.** A `PROP_GASTANK`-only skip would miss Bunker shirts (`chr.c`) and can miss the mine in flight (`PROP_CHRREMOTEMINE`) until it embeds. Do not ship a tank-only skip as the family fix. |

```
Facility spawn     → ITEM_REMOTEMINE (29) + ITEM_TRIGGER (30), 5 mines
very end           → 10× PROP_GASTANK (117) GasProp, pads 10000–10009
                     (x≈10781/11165, y=−577, z −3800…−5853)
throw              → gunfire generate_player_thrown_object
                   → gun.c PROP_CHRREMOTEMINE, canEmbed
                   → child of tank when stuck
look + throw       → objTick builds render_pos (eye 0)
                   → both eyes: bondviewTransformManyPosToViewMatrix IN PLACE
flicker            → second convert saturates ±32768 → black / broken mesh
MTXGUARD=2         → skip second convert (Bunker PASS). Global =2 → Dam tunnel blue.
```

**Plain stare (chair, this dig did not wear):** Facility end, throw mines at tanks, `GETV_STEREO_MTXGUARD=2` after vr442 boot — **PASS or FAIL is the chair’s line.** Source says the knob is the right A/B.

---

## 1. Root finding (files + functions)

### 1.1 Evidence boundary

| Layer | Where | What it proves |
|-------|--------|----------------|
| Public `goldeneye-native` | this repo | KEEP getenv stubs; `PgastankZ` host blob; `HEAD_TRANSLATE` C-default still **ON** when unset; MTXGUARD **not** in MANIFEST (stays 0). **Not** the playable GETV tree. |
| Public `no6969el/GEVR` | issues #55/#70/#72, `packaging/`, docs `292`/`293` | Bunker MTXGUARD=2 PASS (owner/Director, this ticket). Global `=2` Dam tunnel blue. `HEAD_TRANSLATE=0` silent zip on vr442. `MONFRAME` REJECT. MTXGUARD mode 1 observe / mode 2 skip. |
| Public decomp | `n64decomp/007` | Facility tanks, remote-mine throw, in-place matrix convert. |
| Workshop (private) | `F:\Projects\GEVR\GoldenEyeVR\goldeneye-native` | Real `GETV_STEREO_MTXGUARD` site on `bondviewTransformManyPosToViewMatrix`. **Do not push.** |

### 1.2 Facility end tanks (not a monitor)

`UsetuparkZ.c` (`bg/bg_ark_all_p.seg`, `LEVELID_FACILITY = 34`). Comment in setup: **Type = GasProp**.

| Field | Value |
|-------|--------|
| Propdef | **`PROPDEF_GAS_RELEASING` (36)** |
| Model | **117 `PROP_GASTANK`** — “Bottling Tank” |
| Count | **10** (indices 106, 108, … 124; tags 1–10) |
| Pads | **10000–10009** (PAD3D) |
| Positions | two columns **x 10781 / 11165**, **y −577**, **z −3800, −4182, −4632, −5013, −5466** plus a 10th at **−5853** |
| Flags | `0x00001001`, `0x00000100` |

Spawn pads in the same file sit near **{420, −470, 999}**. Tanks are the far negative-Z hall — **very end**, matching the owner add.

**Not the lab bins:** later GasProps at indices 175–191 are `PROP_GASBARREL` (113) / `PROP_GASBARRELS` (114). Different models. Objective is the **117** bottling tanks.

**Not `PROPDEF_MONITOR`.** Destroy path (`propobj.c` `chrobjMaybeDetonateObjectIfFlags`):

```
obj->type == PROPDEF_GAS_RELEASING
  && destroyed level 1
  → init_trigger_toxic_gas_effect(&obj->runtime_pos)
```

AI `ai_42` waits on `if_gas_is_leaking` then sets bits `0x40000000` / `0x04000000`. Gas leak / fog is **after** tanks die, not the throw flicker.

### 1.3 Throw path (the owner timing)

Intro (`UsetuparkZ.c` `intro[]`):

| Slot | Value | Meaning |
|------|--------|---------|
| StartWeapon | **5** | `ITEM_WPPKSIL` |
| StartWeapon | **29** | **`ITEM_REMOTEMINE`** |
| StartWeapon | **30** | `ITEM_TRIGGER` |
| StartAmmo 7 | **5** | `AMMO_REMOTEMINE` × 5 |

| Step | File | Function |
|------|------|----------|
| Fire gadget | `src/game/gunfire.c` ~963 | `ITEM_REMOTEMINE` → `generate_player_thrown_object` |
| Spawn world prop | `src/game/gun.c` ~2001 | default **`PROP_CHRREMOTEMINE`**; timer `THROWN_ITEM_TIMER_SOLO` |
| Stick | `src/game/propobj.c` ~4603 | `canEmbed = TRUE` for remote / timed / prox / bomb / bug / microcam / plastique |
| Detonate (later) | `propobj.c` ~3483 | Facility uses **`EXPLOSION_DEF_FACILITY_REMOTE`**, not the Dam monitor path |

Throw is **embed-on-tank**, same stick family as Dam’s covert modem — but the parent is a **GasProp tank**, not a scrolling monitor. `MONFRAME` never runs here.

Child draw is recursive in `sub_GAME_7F04AC20` (`propobj.c` ~7348): stuck mine is drawn with the tank. **New `render_pos` on throw** is why flicker can be **throw-timed** even if staring at tanks was already ugly.

### 1.4 The matrix (Bunker black shirts = this function)

`bondviewTransformManyPosToViewMatrix` (`bondview2.c` ~10662) copies each matrix then **`matrix_4x4_f32_to_s32` in place**. Not a view transform. **Not idempotent.** Second call saturates entries at **±32768**.

GEVR `292` / `293` / ROADMAP: stereo runs the render body twice; tick rebuilds `render_pos` once (eye 0); alpha pass converts per eye. **Mode 2 skips the second convert.** Historical docs called that a **falsifier that must not ship**. Later **Bunker #55 chair PASS** still used it as a **wear** for black shirts. This dig does not reopen “ship global =2”; it only names the same skip for Facility **if** the chair matches.

Callers of the same convert (public decomp):

| Site | Who |
|------|-----|
| `chr.c` ~3025 | Character + held + hat — **Bunker shirts** |
| `propobj.c` ~7361 | Object models, **including GasProp tanks and thrown mines** (alpha, not destroyed) |
| `gunfire.c` | Hand / projectile models |
| `bondview2.c` ~8545 | Player |

`GETV_STEREO_MTXGUARD` (workshop, from `292` S2 / `RUN-SHEET-299`):

| Value | Behaviour |
|-------|-----------|
| unset / **0** | ship — convert every time (C-default / boot **must stay here**) |
| **1** | observe only (`already-converted=…`, banner `[getv][mtxguard]`) |
| **2** | skip second convert — **Bunker bats only** today |

Global `=2` **Dam tunnel blue** is the measured cost of wearing this skip on Dam geometry. #72 (bridge jump-strip blue with `HEAD_TRANSLATE=0`, MTXGUARD **off**) is a **different** Dam-blue ticket. Do not merge Facility mines into #72.

---

## 2. Same as Bunker MTXGUARD=2? — **Likely. Chair must say PASS/FAIL.**

| | Bunker #55 (proven wear) | Facility end throw (this leftover) |
|--|--------------------------|-------------------------------------|
| Symptom | Black / broken mesh (shirts) | Black flicker on **throw mines at tanks** |
| Convert site | `chr.c` → same `bondviewTransformManyPosToViewMatrix` | `propobj.c` → **same function** |
| Why now | Many chrs ONSCREEN, stereo second convert | 10 large tanks + **new** `PROP_CHRREMOTEMINE` ONSCREEN |
| Wear that PASS | **`GETV_STEREO_MTXGUARD=2` bat** | **Same bat, Facility only** (unrun) |
| Must not | Put `=2` on Dam | Same |

**Not proven until the Facility stare.** A FAIL with `=2` (flicker remains, Facility picture intact) means throw flicker is **not** the saturated-matrix symptom — then stop MTXGUARD APPLY and look at explosion/tex (`TEXINVAL`) only if the flash is **detonate**, or leftover portal if `HEAD_TRANSLATE` was not actually 0 on that zip.

---

## 3. Relation to `HEAD_TRANSLATE=0` now on vr442 boot

| Layer | Value today | Role |
|-------|-------------|------|
| Public C-default (`getv/port/src/port_render.c`, MANIFEST) | unset → **ON (1)** | Graduation still says ON. This dig **does not** flip it. |
| vr441 git boot template | pinned **1** | Stale vs Latest zip. |
| vr442 Latest zip (silent refresh, #70) | pinned **0** | Dam modem Run 0; also kills eye-lean past portals. |
| Early #55 DIG (2026-09-18) | chair `=0` for tanks/Bunker doorways | Portal: body room drawn, eye room dropped. |

**For this leftover:** run the MTXGUARD Facility bat **on top of Latest** (boot already `HEAD_TRANSLATE=0`). Do **not** set `HEAD_TRANSLATE=1` to “see if lean comes back” on the same sitting as the mine throw — that confounds Dam modem wear and the portal theory.

If throw flicker is **gone on Latest with MTXGUARD unset**, `HEAD_TRANSLATE=0` already ate it (portal family). Then **do not** arm MTXGUARD=2 for Facility. If it **remains** on Latest, MTXGUARD is the A/B.

Do **not** turn off `GETV_VR_DRAWALL` / `GETV_VR_PORTALWIDE` (early #55: leave on). Do **not** land `GETV_VR_EYEROOM` for this throw.

---

## 4. KEEP / boot (do not change Dam)

`GETV_STEREO_MTXGUARD` is **not** in `keep-defaults-on/MANIFEST.json`. Unset = 0. **Keep it that way.**

| Knob | Latest / C unset | This leftover |
|------|------------------|---------------|
| `GETV_STEREO_MTXGUARD` | **unset / 0** | Chair Facility `=2` only. Bunker bats already `=2`. **Never** Dam boot. |
| `GETV_XR_HEAD_TRANSLATE` | zip **0**; C unset still **1** | Leave Latest pin. Do not C-default OFF in this PR. |
| `GETV_VR_MONFRAME` | **OFF** (C-default 0; chair REJECT) | Do not sit Facility with `=1`. |
| `GETV_VR_TEXINVAL` / `TEXDLRETAG` / `VFXTMEM` / `VFXSHIFT` | KEEP **ON** | Detonate-only A/B if throw PASS and explode FAIL. Do not C-default OFF. |
| `GETV_STEREO_REBUILD` | ON | Do not turn off. |
| `GETV_VR_DRAWALL` / `PORTALWIDE` | ON | Leave. |
| `GETV_STAGE` | forbidden in public boot | Optional chair `=34` to skip to Facility. AGENT folder caveats apply. |

---

## 5. Safe APPLY sketch (DIG ONLY — not tonight)

**APPLY READY? No.** Need one Facility end throw sit. Workshop MTXGUARD body is private.

### 5.1 Do not land

- Do **not** C-default `GETV_STEREO_MTXGUARD=2`.
- Do **not** add `=2` to `gevr-*-boot.cmd` (Dam would inherit).
- Do **not** Facility-only skip that **ignores** env `=2` on Bunker — that would **break Bunker bats**.
- Do **not** `PROP_GASTANK`-only skip as the family fix (misses shirts + flying mine).
- Do **not** `MONFRAME`, EYEROOM, TEXINVAL C-default OFF, FOVMATCH, #72 merge.

### 5.2 Chair A/B (no rebuild) — Facility bat

Same shape as Bunker bats. After `Start-GEVR.bat` / PLAY0 boot (Latest already has `HEAD_TRANSLATE=0`):

```
set GETV_STEREO_MTXGUARD=2
```

See `chair-facility-mtxguard2.cmd.snippet` in this folder. **One sitting, then unset before any Dam run.**

| Run | Env | If throw flicker **dies** | If it **stays** |
|-----|-----|---------------------------|-----------------|
| **L** | Latest only (MTXGUARD unset, HEAD_TRANSLATE=0) | Portal/lean leftover already gone. **Stop.** No MTXGUARD APPLY. | Still #55 throw. Go F. |
| **F** | `GETV_STEREO_MTXGUARD=2` | Same root as Bunker. Wear = Facility **bat**, not Dam boot. Optional 5.3 later. | Not saturation. Restore `=`. If flash is **boom**, try TEXINVAL=0 (explosions must stay coloured). |
| **D** | **Do not** take F’s env onto Dam | — | Dam tunnel blue is the known FAIL of global `=2`. |

Banner (workshop): `[getv][mtxguard]` must show **MODE 2**. If it does not print, the bat did not reach the binary.

### 5.3 Smallest C (workshop only, after F PASS)

**Not Facility-only auto-arm** (C-default must stay 0; Bunker still uses the bat).

**Dam denylist** — honor env `=2` everywhere **except** Dam (and any other stage that went blue):

```c
/* workshop GETV_STEREO_MTXGUARD site (stereo.c / bondview wrapper).
 * C-default 0. Boot does not set this.
 * env=2 skip second f32_to_s32, BUT:
 *   if (bossGetStageNum() == LEVELID_DAM) treat as 0;
 * Bunker bats (LEVELID_BUNKER1=9, BUNKER2=27) still skip.
 * Facility (34) still skip when the Facility bat sets =2.
 */
```

Allowlist equivalent: skip only when `=2` **and** stage is Facility **or** Bunker1 **or** Bunker2. Same Dam protection. Prefer **denylist Dam** so a future stage that needs the Bunker wear is not forgotten — unless that stage is Dam-like outdoor blue.

Files: workshop mtxguard helper; `bossGetStageNum()`. Public tree: getenv stub only if Director wants a documented default-0 helper next to `ge_xr_head_translate`. **Not this PR.**

---

## 6. Chair stare — plain tester sentences

**Setup:** public **vr442** / Latest. `Start-GEVR.bat`. Recenter both sticks. Headset (say which). Facility, any difficulty that still has the tank objective. **Do not** set `MONFRAME`, FOVMATCH, or MTXGUARD on a Dam save.

Optional skip: `GETV_STAGE=34` (Facility). AGENT + folder-1 options; fine for a throw sit.

**L — Latest, no MTXGUARD (control):**

| | Tester sentence |
|--|-----------------|
| **L-PASS look** | “I stand in the bottling hall and look at the tanks. **No** black flicker before I throw.” |
| **L-FAIL look** | “Just looking at the tanks already blacks / flickers.” (stare-at-prop, still this ticket) |
| **L-PASS throw** | “I throw mines onto the tanks. Picture stays. No black flash at throw or stick.” |
| **L-FAIL throw** | “The moment I throw (or when it sticks), the picture **blacks / flickers**.” (tonight’s leftover) |

If **L-PASS throw**, #55 Facility throw is gone on `HEAD_TRANSLATE=0`. Close this leftover; do not arm `=2`.

**F — same hall, then `set GETV_STEREO_MTXGUARD=2`, restart or new process so the getenv caches:**

| | Tester sentence |
|--|-----------------|
| **F-PASS** | “Throw mines at the tanks. **No** black flicker. Tanks and mines look solid. Guards still look like Latest, not melted.” |
| **F-FAIL** | “Still blacks when I throw, with MTXGUARD=2 on.” → not this skip. Restore unset. |
| **F-COST** | “Throw flicker gone but Facility glass / sky / a character went wrong.” → do not ship even a Facility bat without a note. |

**Dam (separate sitting, MTXGUARD unset):**

| | Tester sentence |
|--|-----------------|
| **Dam KEEP** | “Tunnel is **not** the blue flash we got when MTXGUARD was 2 everywhere. Bridge blue (#72) is whatever Latest already does — not this bat.” |

Headset / OpenXR / SteamVR on or off / HMD vs monitor / Start-GEVR.bat yes — write them on the sit. No ROM.

---

## 7. Director decision

| If you say… | Then… |
|-------------|--------|
| **Green Facility bat only** | Sit L then F on Latest. No C. Bunker bats unchanged. Dam boot unchanged. |
| **Green 5.3 Dam denylist after F-PASS** | Workshop: env `=2` ignored on `LEVELID_DAM`. C-default 0. No public boot line. |
| **Green C-default MTXGUARD=2** | **Rejected.** Dam tunnel blue. |
| **Green Facility-only C that zeros Bunker env=2** | **Rejected.** Breaks Bunker bats. |
| **Merge MONFRAME / #70 / #72** | **Rejected.** |
| **APPLY tonight from this repo** | **No.** Public tree has no mtxguard body. |

---

## 8. Attribution / legal

- No personal credit paths edited.
- No GoldenEye ROM, assets, or dumps.
- Decomp citations are public `n64decomp/007` (`UsetuparkZ.c`, `gun.c`, `gunfire.c`, `propobj.c`, `bondview2.c`, `bondconstants.h`).
- Workshop C stays private until release policy flips.
