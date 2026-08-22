# PRIOR-ART.md

Landscape of existing GoldenEye 007 / Perfect Dark PC port efforts, and where
`GoldeneyeVR-N64-OG` sits among them.

Surveyed 2026-08-21. Re-check before making architecture decisions — this space
moved fast in the two weeks after the decomp hit 100%.

---

## 1. Executive summary

**Every working GoldenEye-on-PC effort is static recompilation.** Not one of them
compiles the decompiled C on a host the way `hostcompat/` does. There is no
existing wheel for our architecture; the Perfect Dark PC port remains the only
prior art for it, and it is PD-specific in exactly the places that matter (see
`VENDORING.md`).

Two consequences:

1. Our decomp-native scaffold is genuinely novel work, not duplicated effort.
2. The fastest route to *a VR build that runs* may not be our scaffold. See §4.

## 2. Timeline

| Date | Event |
|---|---|
| 2026 (early) | AI-assisted recompilation of the cancelled Xbox 360 build released |
| 2026-06-18 | `SunJaycy/GoldenEye-Recomp` latest release (360 path) |
| 2026-08 | **`n64decomp/007` reaches 100% decompilation** after ~9 years |
| 2026-08-20 | `cblock85/GoldenEye64Recomp` active — N64 recomp running on macOS/Linux |
| 2026-08-21 | `chrissotraidis/goldenpad` — same stack retargeted to iOS/Metal |

## 3. The landscape

### 3.1 N64 static recompilation — the live path

**[`cblock85/GoldenEye64Recomp`](https://github.com/cblock85/GoldenEye64Recomp)** — GPL-3.0

The most relevant project to us.

- **Approach:** N64Recomp static recompilation. Original MIPS translated to C
  (or to machine code at launch), runs natively. No emulator, no decomp C.
- **Stack:** RT64 renderer (Metal on macOS, Vulkan on Linux) over
  N64ModernRuntime.
- **Works today:** full intro (Nintendo + Rareware logos, gunbarrel), menus,
  file select, briefings, missions, audio, controller support, frame
  interpolation to display refresh, widescreen.
- **Build modes:** standard (recompiled at build time) and "clean" (recompiled
  from your own ROM at first launch, ~2s, via N64Recomp's live recompiler).
- **Known gaps:** **skyboxes render black and water is flat — unimplemented
  custom microcode**; incomplete multiplayer UI; weapon fire rate wrong at
  native 60 Hz; NTSC-U only.
- **Legal posture:** user-supplied ROM required; ships only "factual symbol data
  — function names, addresses and sizes."

**[`chrissotraidis/goldenpad`](https://github.com/chrissotraidis/goldenpad)** — **no outbound license grant**

- Native GoldenEye for iPhone/iPad. Built *on top of* GoldenEye64Recomp +
  N64ModernRuntime, AOT-generating ARM64. RT64/Metal. SwiftUI host layer, touch
  + MFi/Xbox controllers. Audio via `AVAudioEngine` through a bounded stereo ring.
- Runs missions on real iPad hardware. "Preview 1" developer release.
- **Significance to us:** it is the proof that this stack retargets to a new
  platform and a new host layer quickly. That is the same shape of problem as
  retargeting it to OpenXR.
- **Do not vendor** — the repo explicitly has no outbound license grant.

### 3.2 Xbox 360 recompilation — different codebase, not our path

**[`SunJaycy/GoldenEye-Recomp`](https://github.com/SunJaycy/GoldenEye-Recomp)** — Unlicense (public domain), 699 stars, 7 releases

Native PC recompilation of the **cancelled Xbox 360 remaster**, not the N64
game. XenonRecomp lineage, ReXGlue SDK / MSVC / Python 3 build. Known AMD GPU
crashes. Author notes AI assistance in development.

Re-confirmed 2026-08-21 by direct inspection, because this entry is easy to
misread as a viable recomp target: `ge_manifest.toml` names `assets/default.xex`,
`ge_config.toml` is PowerPC (`r3`, `r30`, `cr6`) hooking raw addresses like
`0x82117B44` with no C symbol names anywhere, and it patches `XamInputGetState`.
Nothing in `n64decomp/007` exists in it. **The four camera accessors have no
counterpart here.**

One thing in it is worth having, though. Its config contains:

> `# hardcode_near_clip_to_2: re-store 2.0f at the per-fog near-clip store site`

A separate team, on a different port of this game, independently found the near
clip to be **per-fog data** and pinned it at 2.0 — the exact low end of the
2..30 range traced through `fog_tables[]` in `VR-PLAN.md` §6.1. Independent
corroboration from a codebase we are not using.

**[`ericksa1417/GoldenEye-Recomp`](https://github.com/ericksa1417/GoldenEye-Recomp)** — Unlicense. Windows-focused fork of the above.

Irrelevant to a 6DoF VR project built on the N64 game: different assets,
different engine build, different geometry. Listed so nobody re-investigates it.

### 3.3 Decompilation upstream

**[`n64decomp/007`](https://github.com/n64decomp/007)** — **no LICENSE file, all rights reserved**

Mirror of `gitlab.com/kholdfuzion/goldeneye_src`. 100% decompiled as of Aug 2026.
Builds NTSC/JPN/PAL ROMs; ships no assets, requires a prior copy of the game.

17 forks surveyed. All are plain mirrors — including `jeffory/007portable`,
whose name suggests a port but which is an unmodified decomp fork with no host
layer. `w0lfheart/007-Performance-Recomp` likewise carries no port work in tree.
**No fork of the decomp contains a native host layer.** As of this survey, ours
is the only one.

Status tracker: <https://kholdfuzion.github.io/goldeneyestatus/>

### 3.4 Announced but not yet public

Time Extension (2026-08) reports a "community-managed AI port" in development by
a community member holding it back for quality, and a "human-made recomp and PC
port" described as coming. Neither has a public repository yet. Worth re-checking
before committing to a long build-out.

### 3.5 Perfect Dark

**[`perfect-dark-pc-port/perfect_dark`](https://github.com/perfect-dark-pc-port/perfect_dark)** — MIT, fork of `n64decomp/perfect_dark`

The only decomp-native port of a Rare N64 FPS that actually ships. Fully analysed
in `VENDORING.md`. Architecturally our closest sibling; legally the friendliest;
practically limited to `fast3d` plus a handful of platform shims.

## 4. Strategic read

### 4.1 We hold something GoldenEye64Recomp needs

Its stated limitation — *skyboxes black, water flat, unimplemented custom
microcode* — is precisely the work already finished here:

- `MICROCODE-SPEC.md`, derived independently and verified against a real ROM
- F3D + `G_TRI4` display-list walker, validated over all 1,937 room display lists
- `G_CULLDL` real culling, vertex pipeline validated across all 1,296 rooms

This is a concrete contribution to trade, and a way to get our microcode work
exercised against a renderer that already draws frames.

### 4.2 The recomp path may be the better VR host

`RT64-INTEGRATION.md` has us at "wired — compiles and links; needs a display to
run." GoldenEye64Recomp has RT64 running with GoldenEye's real display lists
today. Building the OpenXR layer on a stack that already renders is plausibly a
matter of weeks rather than months, and the entire libultra-ABI problem that
`hostcompat/` and `src/ultra/` exist to solve simply does not arise — N64Recomp
translates the MIPS directly.

**The objection:** 6DoF VR needs game-state access — camera angles, player
position, weapon transforms — which is trivial in decomp C and awkward in
recompiled MIPS.

**Why the objection is weaker than it looks:** 100% decomp means a complete
symbol map. The Zelda64Recomp mod model replaces recompiled functions with
native C hooks by symbol. Our pose→angle decomposition, asymmetric-frustum
stereo projection, and view/model matrix construction are already written and
tested; they would bind to hooks instead of to patched decomp source.

### 4.3 The blocker is licensing

`GoldenEye64Recomp` is **GPL-3.0**. It cannot be vendored into this MIT
repository without relicensing the combined work under GPL-3. Options:

| Option | Consequence |
|---|---|
| **Keep it external** — user builds it separately, we ship the VR layer against it | Cleanest. Same pattern as our `patches/` model. Preserves our MIT. **Preferred.** |
| Relicense `GoldeneyeVR-N64-OG` as GPL-3 | Permitted (we own the copyright) but forecloses permissive reuse forever |
| Go to upstream components directly | ~~`N64Recomp` and `N64ModernRuntime` are MIT~~ — **wrong, see below** |

**Settled 2026-08-21.** Every component of the recomp stack was checked
directly rather than taken second-hand. The distinction that matters is not
which licences appear, but which components are *linked into the shipped
binary*:

| Component | Licence | Linked into shipped host? |
|---|---|---|
| `rt64/rt64` | MIT (`LICENSE`) | yes |
| `N64Recomp/N64Recomp` | MIT (`LICENSE`) | **no — build-time recompiler, a tool** |
| `N64Recomp/N64ModernRuntime` (`librecomp`, `ultramodern`) | **GPL-3.0** (`COPYING`, verbatim GPLv3) | **yes** |

`N64ModernRuntime` carries a single top-level `COPYING` and no per-subtree
licence files; `librecomp` and `ultramodern` have no separate grant. The one MIT
header inside it (`librecomp/src/euc-jp.cpp`) is vendored third-party code, not
a carve-out.

So the GPL-3.0 obligation comes from **exactly one component**, and
`N64Recomp` being MIT is irrelevant to the outcome — a compiler's licence does
not travel into what it compiles. RT64 being MIT does not rescue the
combination either:

- **Row 3 of the table above is dead.** Going to upstream components directly
  does not produce a permissively-licensed host; it produces the same GPL-3.0
  obligation by a longer route.
- **Row 1 survives and is now the only clean option.** Keeping the recomp host
  external, with the MIT VR layer shipped against it, is unaffected — MIT links
  into GPL-3.0 without difficulty, and `include/ge_vr/` stays MIT and stays
  reusable.
- **Anything we distribute as a combined binary is GPL-3.0, with source.** That
  is a licensing decision for this project to make deliberately, and it should
  be made before Phase 0 of `VR-PLAN.md`, not after Phase 3.
- It is a standing argument for keeping `goldeneye-native` alive: it is the path
  that is not encumbered.

`goldenpad` has no outbound license grant at all and must not be vendored under
any option.

### 4.4 Recommended posture

Keep the decomp-native scaffold as the reference implementation we own outright
and can license freely. In parallel, spike the OpenXR layer against
GoldenEye64Recomp as an **external** GPL-3 dependency, purely to answer one
question fast: does VR-on-recomp work? If it does, the hardest remaining half of
the roadmap is already built by someone else.

## 5. Sources

- <https://github.com/cblock85/GoldenEye64Recomp>
- <https://github.com/chrissotraidis/goldenpad>
- <https://github.com/SunJaycy/GoldenEye-Recomp>
- <https://github.com/n64decomp/007>
- <https://github.com/perfect-dark-pc-port/perfect_dark>
- <https://kholdfuzion.github.io/goldeneyestatus/>
- <https://www.timeextension.com/news/2026/08/after-9-years-of-work-goldeneye-007-n64-is-now-100percent-decompiled>
- <https://www.tomshardware.com/video-games/retro-gaming/goldeneye-007-for-n64-has-been-100-percent-decompiled-success-of-half-decade-project-opens-up-possibilities-for-complex-mods-and-ports>

---

## 6. Addendum — verified 2026-08-21, same day

Three claims above were checked against primary sources rather than left as
survey notes. Two resolve in our favour; one does not.

### 6.1 The licence gate in §4.3 is closed, and it opens a fourth option

§4.3 listed RT64's licence as an **open item that gates the decision**. It is
**MIT** — `LICENSE`, "Copyright (c) 2024 RT64 Contributors". `N64Recomp` is
**MIT** as well.

So the entire stack under `GoldenEye64Recomp` is permissive; only cblock85's own
GoldenEye-specific glue is GPL-3. The table in §4.3 should therefore read:

| Option | Consequence |
|---|---|
| Keep GoldenEye64Recomp external | Still fine, still preserves our MIT. |
| Relicense as GPL-3 | Still forecloses permissive reuse. |
| **Build on N64Recomp + N64ModernRuntime + RT64 directly** | **All MIT. No GPL contact at all.** What we would give up is cblock85's GoldenEye symbol/config work — which is exactly the part their README calls "factual symbol data", and the part our 100%-decompiled upstream can regenerate. |

That third row was rated "more work, better licence outcome". With the licences
confirmed it is closer to "comparable work, no licence problem" and should be
costed properly before the GPL-3 route is accepted.

### 6.2 §4.1 overstates what we hold — and finding out why was the useful part

§4.1 asserts that GoldenEye64Recomp's black skyboxes and flat water are
"precisely the work already finished here". **That was not established, and
checking it found a hole in our own work.**

`MICROCODE-SPEC.md` listed opcode `0xB2` as "F3DEX-only, not present". It is
`G_RDPHALF_CONT` — `(G_IMMFIRST - 13)`, stock `PR/gbi.h:164` — and
`src/game/sky.c` emits it constantly, paired with `G_RDPHALF_1`, to stream the
extra words of long RDP commands. `src/gbi/gbi.h` did not define it and
`src/gbi/gbi_interp.cpp` did not accept it.

The reason we never saw it is the interesting part: every validation run behind
that document walked **room** display lists — all 1,937 of them. The sky and the
water are not stored per room; `sky.c` builds their lists at runtime. Total
coverage of one path told us nothing about a path we never walked, and "not
present" was an inference from absence in that corpus, written down as fact.

Corrected in `MICROCODE-SPEC.md` §2.1 and in the interpreter.

It remains a **candidate** explanation for their black sky, not a demonstrated
one: `0xB2` means something else under F3DEX2, which is what RT64 targets, so an
F3DEX2 decoder will mis-handle it — but nobody has traced a black sky back to a
dropped `0xB2`. Doing that trace is cheap and is the natural first collaboration.

### 6.3 What is actually tradeable

After 6.2, the honest inventory of what we hold that the recomp projects do not:

- An independently derived, ROM-validated map of the **F3D + `G_TRI4`** dialect,
  now including `G_RDPHALF_CONT`.
- A display-list walker validated over 1,937 room lists, with real `G_CULLDL`.
- The knowledge of **which paths remain unvalidated** — sky, water, and anything
  else built at runtime rather than stored per room. That is a smaller claim
  than §4.1 made, and it is one we can stand behind.
