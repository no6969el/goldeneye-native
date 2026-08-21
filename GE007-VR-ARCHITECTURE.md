# GoldenEye 007 — Native PC Port with 6DoF OpenXR VR

**Architecture & Implementation Plan**
Base: [`n64decomp/007`](https://github.com/n64decomp/007) (matching N64 decompilation)
Target: native PC executable, room-scale 6DoF VR via OpenXR
Status: design document — **v2**

> **v2 revision notice.** §4 of v1 claimed the custom microcode was the
> project's dominant cost and that no stock GBI interpreter could work. **That
> was wrong, and wrong in the favourable direction.** Microcode archaeology
> (see `MICROCODE-SPEC.md`) established three things that materially rescope the
> project:
>
> 1. The game builds against **plain F3D, not F3DEX** — `F3DEX_GBI` is never
>    defined. So the vertex cache is 16 entries and command packing is F3D.
> 2. **`G_SETTEX` is not an RSP command.** `0xC0` is `G_NOOP` in the F3D
>    immediate range; the RSP forwards it and the RDP discards it. It is
>    expanded **on the CPU** by `texLoadFromGdl()` (`src/game/tex.c:817`) before
>    submission — code that is already decompiled and already ours.
> 3. **Exactly one opcode is nonstandard: `G_TRI4` (0xB1).**
>
> Phase 2 is therefore "a stock F3D interpreter plus one opcode", not "reverse-
> engineer and reimplement a custom RSP". The revised estimate is in §10.
> The v1 estimate of 60–70% of total effort was an overestimate by a wide
> margin. §4 below has been rewritten.

---

## 0. Executive summary

The plan is a two-layer build:

1. **`ge-native`** — a PC port of the decomp. Game code (`src/game/`, ~242 translation units) is kept essentially as-is. `libultra` and the RCP are replaced: a host-side OS shim (threads, message queues, timers, controller, audio) plus a **display-list interpreter** that consumes the game's real GBI stream and issues modern GPU draws.
2. **`ge-xr`** — an OpenXR layer that (a) drives the frame loop, (b) renders the scene twice per frame with per-eye view/projection, and (c) feeds head and controller poses into the game's existing camera and weapon-aim state.

The critical architectural insight is that **the game already renders N independent cameras into independent viewports** (4-way split-screen). VR stereo is that same mechanism with N=2, identical player state, and eye-offset transforms. The renderer does not need a new concept; it needs a second pass with different matrices.

The critical architectural *risk* is that **GoldenEye does not use stock F3DEX microcode.** It ships a custom `gsp3D` microcode (`rsp/graphics/gmain.s`, 1545 lines) with at least two nonstandard commands. No off-the-shelf N64 graphics plugin will run this correctly. Section 4 covers this.

---

## 1. What we are actually porting

### 1.1 Repository shape

| Area | Path | Notes |
|---|---|---|
| Game logic | `src/game/` | 242 files. Bond, AI, weapons, levels, HUD. |
| Frame/VI driver | `src/fr.c`, `src/vi.c` | Framebuffer, viewport, projection setup. **Primary VR hook site.** |
| RCP task submission | `src/game/rsp.c` | Builds `OSTask`, points at custom ucode. |
| Custom microcode | `rsp/graphics/gmain.s` | The `gsp3D` RSP program. |
| GBI extensions | `include/gbi_extension.h` | `G_TRI4`, `G_SETTEX`, custom combine/rendermodes. |
| libultra | `src/libultra/`, `src/libultrare/` | To be replaced wholesale. |
| Player/camera state | `src/game/bondview.h`, `bondview2.c` | `vv_theta`, `vv_verta`, weapon displacement. |
| Weapons | `src/game/gun.c`, `gunfire.c` | Aim, hitscan, hands. |

Assets are **not** in the repo. A legally-obtained ROM is required; the existing `tools/` + `assets/` extraction flow produces them. Nothing copyrighted may be distributed.

### 1.2 Threading model to unwind

`src/init.c` and friends start: idle, main, scheduler (`src/sched.c`), audio (`src/audi.c`), rmon, TLB-crash. The scheduler thread is the one that matters — it owns retrace timing and RSP/RDP task dispatch, and in VR it must be replaced rather than emulated, because **OpenXR owns frame pacing**, not the VI retrace.

---

## 2. Layered target architecture

```
┌───────────────────────────────────────────────────────────┐
│  ge-xr          OpenXR runtime interface                  │
│                 session, swapchains, frame loop, actions  │
│                 head/hand poses, comfort options          │
├───────────────────────────────────────────────────────────┤
│  ge-vr-bridge   pose → game state; per-eye render driver  │
│                 aim decoupling, world-space HUD           │
├───────────────────────────────────────────────────────────┤
│  ge-game        UNMODIFIED decomp C (src/game/**)         │
│                 + a small set of surgical patches         │
├───────────────────────────────────────────────────────────┤
│  ge-gbi         display-list interpreter                  │
│                 gsp3D dialect → command stream            │
├───────────────────────────────────────────────────────────┤
│  ge-rhi         D3D12 / Vulkan backend, RDP emulation     │
│                 combiner→shader, N64 texture decode       │
├───────────────────────────────────────────────────────────┤
│  ge-ultra       libultra HLE: threads, MQ, timers,        │
│                 PI/SI/VI, controller, audio (RSP audio)   │
└───────────────────────────────────────────────────────────┘
```

Rule of thumb: **the game keeps writing real GBI into a real RDRAM-shaped buffer.** We do not intercept at the `gSP*` macro level. The reason is concrete: `src/game/bg.c:2772` and `src/game/lightfixture.c:170–180` *read back and rewrite the display list they just built*, scanning for `G_VTX`. Any port that replaces the macros with immediate-mode calls breaks those systems. Keep the buffer, interpret it.

---

## 3. `ge-ultra` — the libultra shim

Reimplement the API surface the game actually calls, not all of libultra.

**Threads / sync.** `osCreateThread`/`osStartThread`/`osStopThread`, `OSMesgQueue`, `osSetTimer`, `osGetTime`. Map to real host threads with a global cooperative lock, or (preferred) collapse to a single host thread with fibers. Fibers are the safer choice: the game assumes N64 priority scheduling semantics, and true preemption will surface latent races the original hardware never hit.

**VI.** `osViSwapBuffer`/`osViSetMode`/retrace messages become the bridge's frame signal. In VR this is driven by `xrWaitFrame`, not by a 60Hz timer.

**PI / DMA.** ROM reads become file reads from the extracted asset tree. Segment addressing (`gSPSegment`, `SPSEGMENT_BG_VTX`) must be preserved — the interpreter resolves segments the same way the RSP did.

**SI / controller.** `osContStartReadData`/`osContGetReadData` synthesize an `OSContPad` from OpenXR actions (Section 7). Keep the shape: the game's input code stays untouched.

**Audio.** GoldenEye uses the standard N64 audio microcode path (`src/audi.c`, `src/libultra/audio/`). The pragmatic move is to keep the game's synthesis structures and run the audio-ucode command list on the CPU into a float mixer, then out through a host API. This is well-trodden ground in other N64 ports.

**Do not implement:** TLB (`tlb_*.c`), `crash.c`, `rmon.c`, `usb.c`. Stub them.

---

## 4. `ge-gbi` — smaller than it looked

Full evidence in **`MICROCODE-SPEC.md`**. Summary:

### 4.1 The baseline is F3D, not F3DEX

Neither `F3DEX_GBI` nor `F3DEX_GBI_2` is defined anywhere in the build, so
`PR/gbi.h` resolves to the plain-F3D branch — and the microcode's own decode
matches F3D packing (`gmain.s:329-331,464-465` reads `G_VTX`'s count and index
as nibbles of `w0` byte 1, which is the F3D layout and is incompatible with
F3DEX's).

Consequences: **16-entry** vertex cache (not 32), F3D command packing, F3D
`G_MTX` flag encoding, and **no** `G_MODIFYVTX`, `G_BRANCH_Z`, `G_LOAD_UCODE`,
or `G_QUAD` — those are F3DEX-only and simply are not in this GBI.

The 16-entry figure is not inferred from the 4-bit index alone. The DMEM map
tiles exactly: modelview `0x360`, projection `0x3A0`, MVP `0x3E0`, vertex cache
`0x420`, and `0x420 + 16 × 40 = 0x6A0`, which is precisely the display-list
buffer base. No gap, no overlap.

> v1 said the cache "must be mutable mid-list because the code uses
> `gSPModifyVertex`". That was wrong — the one apparent use
> (`src/game/glass2.c:446`) is a *comment* mislabelling a hand-encoded `G_TRI4`
> command, and `G_MODIFYVTX` does not exist in a non-F3DEX GBI.

### 4.2 `G_TRI4` (0xB1) — the only nonstandard opcode

Four triangles per command, 4-bit indices, `w0` carrying the `z` nibbles and
`w1` the `x`/`y` pairs. Implemented and tested in `src/gbi/`.

The microcode does not loop: it consumes one triangle, shifts `w0 >>= 4` /
`w1 >>= 8`, writes the shifted words back into its DMEM command buffer, rewinds
the DL pointer by 8 and re-dispatches (`gmain.s:202-213`).

**The termination rule is the trap.** The branch is on the *entire remaining
`w1`* being zero, tested before extracting the current triangle's nibbles, and
the `z` nibbles are never examined. So trailing empty slots terminate — the
intended use — but an interior `(x=0, y=0)` slot followed by a nonzero slot does
**not**, and is emitted as a degenerate triangle. v1 repeated the macro
comment's claim that all-zero triangles "are not drawn"; that holds only for
trailing slots. An interpreter that helpfully skips interior zero slots
silently disagrees with hardware.

Indices are *not* pre-multiplied by 10 in the command — but the microcode
restores the stock `×10` form via a 16-byte lookup table at DMEM `0x2D0`, then
reuses the unmodified `G_TRI1` triangle-setup core. So downstream of decode,
`G_TRI4` and `G_TRI1` are the same path.

### 4.3 `G_SETTEX` is not an RSP command at all

This is the correction that rescopes the project.

`G_SETTEX == G_NOOP == 0xC0` (`gbi_extension.h:49` vs `gbi.h:174`). In F3D,
`0xC0–0xFF` is the RDP passthrough range and `0xC0` is the RDP's no-op. The RSP
forwards it; the RDP discards it. **There is no RSP-resident texture bank, no
DMEM table, and no microcode expansion to reverse-engineer.**

The expansion happens on the CPU, in `texLoadFromGdl()` — `src/game/tex.c:779–1040`,
`case G_NOOP:` at line 817 — which reads a display list and writes an expanded
one. It resolves `texnum = w1 & 0xfff` against `g_Textures[]` and a runtime
texture pool, then emits ordinary `SetTextureImage`/`SetTile`/`LoadBlock`/
`LoadTLUT` sequences per `TextureTypes`.

**That code is already decompiled. It is already ours.** The port compiles and
runs it as-is. A list that has been through the pre-pass contains no `0xC0`
commands, so the interpreter never sees one.

Two consequences worth stating plainly:

- Whatever "preswapped" means, we do not need to know. The only difference
  between `TEXTURETYPE_TILE` and `TEXTURETYPE_TILE_PRESWAPPED` in `tex.c` is
  that the former emits two identical tile descriptors and the latter one; the
  `LoadBlock` is byte-identical. The name presumably refers to build-time asset
  processing, but nothing in the expansion branches on it.
- `texTrySetTileState`/`texTrySetTileSize` (`tex.c:180-232`) form a redundancy
  cache that *omits* tile commands already in effect. Because we run the real
  `tex.c`, we inherit that for free.

Also nonstandard but trivial: `gDPLoadTLUT06`/`07`/`Cmd2` variants,
`RM_CUSTOM_AA_ZB_XLU_SURF` (XLU with Z-update), and the `*FADE` combiner modes.
All of these are ordinary RDP command encodings that fall out of a correct
generic path — none needs a special case.

### 4.4 What the interpreter actually is

Two clearly separated layers. Conflating them is the main design risk left:

**Layer A — CPU pre-pass.** The game's own `texLoadFromGdl()`, compiled and run
unmodified. Off the render path (once per room/object load). Consumes `0xC0`.

**Layer B — display-list walker.** A stock F3D interpreter with exactly two
deltas: opcode `0xB1` is `G_TRI4`, and the vertex cache is 16 entries.
Everything else — matrices, segments, lighting, fog, clipping, othermode, RDP
passthrough — is stock F3D.

Implemented in `src/gbi/`. Notes from building it:

- Keep the fixed-point `Mtx` → float conversion exact. The N64 stores all 16
  integer parts and *then* all 16 fractional parts, not interleaved per element.
- Segment resolution uses only **four** bits of the segment nibble
  (`gmain.s:72-73` does `srl 22` + `andi 0x3c`), so `0x1E000000` resolves
  through segment 14, not out of range.
- `G_CULLDL` cannot be honoured without transformed clip outcodes, which we
  deliberately do not compute (the GPU does the transform). Never-cull is the
  safe failure: it costs performance. Guessing the other way makes world
  geometry vanish.
- Matrix stack overflow and `G_POPMTX` underflow are **silently ignored** on
  hardware. Reproduce that. Game code may rely on the forgiving behaviour.

### 4.5 Still use RT64

The RDP side — combiner→shader generation, N64 texture decode, blender and
render-mode emulation — is unchanged in scope and remains the bulk of the
rendering work. [RT64](https://github.com/rt64/rt64) already implements it
against D3D12/Vulkan. Fork it and feed it from Layer B. The saving is real; the
v1 recommendation stands even though its justification was wrong.

---

## 5. Stereo rendering

### 5.1 The hook

`viSetupCurrentPlayerView()` in `src/fr.c:695` is the seam. It currently does, per player per frame:

```c
g_CurrentPlayer->viewports[g_ViBackIndex].vp.vscale[0] = g_ViBackData->viewx * 2;
...
gSPViewport(gdl++, ...);
guPerspectiveF(g_viProjectionMatrixF, &g_viPerspNorm,
               g_ViBackData->fovy, g_ViBackData->aspect,
               g_ViBackData->znear, g_ViBackData->zfar, 1.0f);
guMtxF2L(g_viProjectionMatrixF, g_viProjectionMatrix);
gSPMatrix(gdl++, ..., G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_PROJECTION);
gSPPerspNormalize(gdl++, g_viPerspNorm);
currentPlayerSetProjectionMatrix(g_viProjectionMatrix);
currentPlayerSetProjectionMatrixF(g_viProjectionMatrixF);
```

`guPerspective` builds a **symmetric** frustum from a single `fovy`. HMD frusta are **asymmetric** — `XrFovf` gives four independent half-angles (`angleLeft/Right/Up/Down`), and they differ per eye. So:

**Patch:** replace the `guPerspectiveF` call with `geVrBuildProjectionF(eye, znear, zfar, out_mtx, out_perspNorm)`, which builds the asymmetric frustum directly from `XrFovf` when VR is active and falls through to `guPerspectiveF` when it is not. `perspNorm` must still be computed the way `guPerspective` does (roughly `65536/(near+far)` clamped), because `gSPPerspNormalize` feeds the RSP's W-divide precision and getting it wrong produces Z fighting at range.

`currentPlayerSetProjectionMatrixF` matters: game code reads that matrix back for billboarding, muzzle-flash placement, and screen-space projection of world points. It must hold the *current eye's* matrix during that eye's pass, or those effects land in the wrong place in one eye — which reads as nauseating double-vision, not as a visual bug.

### 5.2 The per-eye loop

Do **not** run gameplay twice. One simulation tick, two render passes:

```
xrWaitFrame
xrBeginFrame
xrLocateViews(PREDICTED_DISPLAY_TIME)   → view[0], view[1]
  ├─ push poses into bridge state
  ├─ game tick (AI, physics, input) ....... ONCE
  └─ for eye in {L, R}:
        bind eye swapchain image
        ge_vr_set_eye(eye)                  ← changes what fr.c builds
        build display list (existing game render path)
        interpret + submit
xrEndFrame(projection layer, 2 views)
```

Views must be located with the *predicted display time*, and the game tick must consume the same predicted time — otherwise head motion and world motion disagree by a frame and the result is uncomfortable.

### 5.3 Culling

The game frustum-culls rooms and props against the single player frustum (`src/game/bg.c` portal system). Per-eye culling with per-eye frusta causes objects to pop in one eye only. Cull once against the **union frustum** (a symmetric frustum enclosing both eyes), then render both eyes from that visible set.

---

## 6. 6DoF: decoupling view from aim

### 6.1 What the game already gives us

This is where GoldenEye is unexpectedly cooperative. `src/game/bondview.h` has, on the player struct:

| Field | Offset | Meaning |
|---|---|---|
| `vv_theta` | 0x0148 | view yaw |
| `vv_verta` | 0x015c | view pitch |
| `vv_verta360` | — | normalized pitch |
| `vv_costheta` / `vv_sintheta` | 0x0154/0x0158 | cached trig |
| `weapon_theta_displacement` | — | **gun yaw offset from view** |
| `weapon_verta_displacement` | — | **gun pitch offset from view** |
| `gunpos` (`coord3d`) | bondtypes 0x1c | gun muzzle position |

The weapon already carries an angular displacement relative to the view — that's how sway, recoil, and the aim-mode reticle work (`gunSetBondWeaponSway(f32, f32, speed_verta, speed_theta)`, `sub_GAME_7F067F58(turn_x, turn_y, max_aim_lock_speed)`). **6DoF aiming is an extension of a mechanism that exists**, not a new one. That is the difference between this being hard and being impossible.

### 6.2 Mapping

**Head → view.** Each frame, decompose the HMD pose relative to the play-space origin:
- Yaw feeds `vv_theta` as `stick_yaw + head_yaw`, where `stick_yaw` is the accumulated snap/smooth-turn from the controller. Head yaw must be *additive*, never authoritative, or the player cannot turn past their neck limit.
- Pitch writes `vv_verta` directly from head pitch. Clamp to the game's existing pitch limits only if a system downstream depends on them; prefer removing the clamp and validating that nothing divides by `cos(verta)`.
- Roll: the N64 camera has no roll. Either add a roll term to the eye view matrix *below* the game (in the bridge, post-`guLookAt`) or discard it. Discarding roll in VR is very uncomfortable — add it in the bridge.
- **Head position** (the actual 6DoF part) is a translation applied to the eye view matrix in the bridge. It must *not* move the player collision capsule, or leaning through a wall becomes a noclip exploit and a physics crash. Cap positional offset by a room-collision test and soft-blank ("greyout") when the head exceeds it.

**Controller → weapon.** Compute the aim-pose direction in world space, convert to `(theta, verta)`, and write the delta relative to view into `weapon_theta_displacement` / `weapon_verta_displacement`. Then — and this is the load-bearing change — **`gunfire.c`'s hitscan must originate from the controller pose, not the camera.** Locate the ray construction in the fire path and route it through a bridge accessor that returns muzzle origin + direction from the tracked hand. Everything downstream (penetration, `bondwalkItemGetObjectsShootThrough`, damage) is unchanged.

Two hands are already a first-class concept — `gunUpdateAndFireBothHands()`, `gunFireTankShell(s32 hand)`. Dual-wielding maps cleanly onto two tracked controllers.

### 6.3 Weapon models

Weapons are drawn in view space as a first-person overlay. In VR they must be drawn in **world space at the controller pose**. This means finding the weapon-render path and replacing its view-relative transform with a bridge-supplied model matrix. Expect the muzzle-flash and shell-eject attach points to need re-authoring, since they were positioned to look right in a fixed screen composition.

### 6.4 HUD

The HUD (health/armour rings, ammo, watch, objectives, boot-up screens) is drawn with `gDPFillRectangle` / `gSPTextureRectangle` in screen space. In VR, screen-space overlays at infinity are unreadable and nauseating. Two tiers:

- **Tier 1 (ship first):** render the entire HUD to an offscreen RT, then composite it as a world-locked or head-locked quad at ~2m. Cheap, works immediately, looks like a floating HUD.
- **Tier 2:** diegetic. Health/armour on the watch, ammo on the weapon, objectives on the watch when raised. GoldenEye's watch menu is *already* a diegetic object the player raises — it's the best HUD affordance in the game and VR should lean on it hard.

The pause/watch menu and the mission briefing screens are full-screen 2D. Composite those as a large curved quad and freeze the world behind them.

---

## 7. Input

OpenXR action set `gameplay`:

| Action | Type | Suggested binding (Touch/Index) |
|---|---|---|
| `hand_pose` (L/R) | pose | `/user/hand/*/input/aim/pose` |
| `grip_pose` (L/R) | pose | `/user/hand/*/input/grip/pose` |
| `move` | vec2 | left thumbstick |
| `turn` | vec2 | right thumbstick |
| `fire` (L/R) | float | trigger |
| `aim_mode` (L/R) | bool | grip / squeeze |
| `reload` | bool | A / A |
| `watch` | bool | Y — or a pose-gated gesture (raise left wrist to face) |
| `use` | bool | B |
| `change_weapon` | bool | X |
| `haptic` (L/R) | vibration output | — |

Synthesize an `OSContPad` from `move`/`turn`/buttons so all unmodified game input code keeps working; route only aim and fire through the bridge.

Recoil is a haptic pulse *and* — deliberately — should **not** kick the view. View kick from an untracked source is a top cause of VR discomfort. Kick the weapon model, not the head.

---

## 8. Performance

The original targets ~20–30 fps at 320×240. VR needs 72–120 fps at ~2000×2000 per eye. The good news: the geometry and texture budgets are 1997-scale and trivially within a modern GPU's reach. The bad news is all CPU-side.

- **The interpreter is the bottleneck**, not the GPU. Two display-list builds + two interpretations per frame at 90Hz is 180 list-walks/sec. Optimize by building the list once and re-interpreting with a substituted projection matrix where possible — most of the list is eye-invariant.
- **Simulation rate.** Game logic is written against variable delta but tuned for ~20–30Hz. Running AI/physics at 90Hz will expose tuning assumptions (`speedtheta`, `speedverta`, `thetadie`). Run the sim at a fixed 60Hz and interpolate render transforms, rather than running it at display rate.
- **Reprojection.** Never rely on the runtime's ASW to cover a missed frame; the HUD quad and weapon models will smear. Budget for a real frame.

---

## 9. Comfort

Non-negotiable defaults, all user-overridable:

- Snap turn (30°/45°) default on; smooth turn opt-in.
- Vignette on locomotion, tunable.
- No forced view rotation from any source (recoil, explosions, damage, cutscene camera). Cutscenes are the hard case — GoldenEye's intro/outro cameras move the view aggressively. Offer "play cutscenes on a 2D screen" as the default.
- Height calibration + play-space recenter bound to a menu action.
- Seated/standing modes; crouch by physical crouch *and* by button.

---

## 10. Phased roadmap

| Phase | Deliverable | Gate |
|---|---|---|
| **0** | Build the ROM. Verify `ge007.u.z64` sha1 matches. | Toolchain works. |
| **1** | `ge-ultra` shim + headless boot. Game reaches the main loop, ticks, produces display lists. No rendering. | Sim runs on PC. |
| **2** | `ge-gbi` + RT64 fork. Display-list walker (**done, tested**) + RDP backend. Flat-screen port renders a level. | **The real milestone.** Everything after this is comparatively tractable. |
| **3** | Flat-screen port playable: input, audio, save. | Ship this on its own; it's valuable independent of VR. |
| **4** | OpenXR session + stereo. Asymmetric frusta, per-eye pass, head-locked HUD quad. Gamepad controls, head look only. | Comfortable seated stereo. |
| **5** | 6DoF: head position, controller aim, hitscan redirect, world-space weapon models. | The thing you actually asked for. |
| **6** | Diegetic HUD, watch, dual-wield, comfort options, cutscene handling. | Polish. |

**Revised phase 2 estimate.** v1 called this 60–70% of total effort, "almost all
of it `gmain.s` archaeology". The archaeology is now done (§4,
`MICROCODE-SPEC.md`) and the display-list walker is written and tested. What
remains in phase 2 is the RDP backend — combiner→shader, texture decode, render
modes — which is real work but is *known* work with an existing implementation
to fork. Call it 30–40%, and the risk profile changed from "unknown unknowns in
undocumented microcode" to "integration effort against RT64".

The new long pole is **phase 1**, the libultra shim, specifically the threading
model. GoldenEye runs six threads under N64 priority scheduling
(`src/init.c:170-227`, `src/sched.c:183`, `src/audi.c:397`). Collapsing that
onto host threads will surface latent races that real hardware never hit,
because the original scheduling was effectively deterministic. Use fibers, not
preemptive threads.

---

## 11. Legal

- Distribute **source only**. No ROM, no extracted assets, no textures, no audio.
- Users supply their own cartridge dump; the build extracts from it locally.
- The decomp is clean-room matching-decompilation work; keep the port's provenance equally clean and document it.
- Do not bundle, mirror, or auto-download game data. This is the line that determines whether the project survives.

---

## 12. Open questions

1. ~~`gmain.s` — is `G_SETTEX`'s texture bank indexed into a ROM-resident table, or a RAM-resident one populated per level?~~ **RESOLVED.** Neither — `G_SETTEX` never reaches the RSP. `texLoadFromGdl()` (`src/game/tex.c:817`) resolves `texnum` against the static `g_Textures[]` array *and* a runtime texture pool in main RAM. We run that code as-is. See §4.3.
2. Does any game system depend on the *symmetric* projection matrix read back via `currentPlayerSetProjectionMatrixF`? If so, those call sites need the union frustum, not the eye frustum.
3. `vv_verta` pitch clamp — is it enforced anywhere downstream, or purely an input clamp?
4. Where exactly does the hitscan ray originate in `gunfire.c`? (Named-function search needed; the fire path goes through `gunUpdateAndFireBothHands`.)
5. Multiplayer: split-screen in VR is meaningless, but the viewport machinery is shared. Confirm N=2 stereo doesn't collide with N-player viewport indices (`g_ViBackIndex`, `viewports[]`).

---

*Next: `ge-xr` scaffold — a compilable OpenXR skeleton implementing Sections 5, 6, and 7 with typed stubs at every game-side seam.*
