# VENDORING.md

Policy and findings for reusing third-party code in `GoldeneyeVR-N64-OG`.

Audited against `perfect-dark-pc-port/perfect_dark` @ `32a1cb9` (2026-08-13).

---

## 1. Hard rules

1. **No decompiled game source is vendored into this repo.** Not GoldenEye's,
   not Perfect Dark's. Decomp source is fetched at build time from its upstream
   and modified via `patches/`. This repo stays MIT-clean.
2. **No ROM data, extracted assets, or asset-derived binaries are committed.**
   The user supplies their own ROM. This includes textures, audio banks,
   sequences, and any `.bin`/`.z64`/`.v64`/`.n64` payload.
3. **Nothing under a proprietary notice is vendored**, regardless of what the
   containing repository's LICENSE file says. A project cannot license code it
   does not own. See §4.
4. **Every vendored subtree keeps its own LICENSE file** in place, plus an entry
   in §5 of this document.
5. **Every vendored subtree is pinned to a commit** recorded in §5, so provenance
   is auditable and upstream drift is visible.

## 2. Layout

```
third_party/
  <name>/
    LICENSE           # upstream's, verbatim, never edited
    PROVENANCE.md     # upstream URL, pinned commit, date, what was changed
    ...sources...
```

Local modifications to a vendored subtree are made in-tree and described in that
subtree's `PROVENANCE.md`. Do not silently diverge.

## 3. Perfect Dark port — what to take and what to leave

The PD port has a physical seam (`port/` vs `src/`) but **no build-level
boundary**: `CMakeLists.txt` globs `port/*.c` straight into a single
`add_executable(pd ...)`. There is no `libport`, and no public port API header.
Extraction is a refactor, not a configuration.

Coupling is concentrated rather than smeared. Counting includes of
`game/`, `lib/`, `bss.h`, `data.h` across `port/src`:

### Take — zero coupling to PD game code

| File | Notes |
|---|---|
| `port/fast3d/` (whole subtree) | Highest-value target. Zero `game/`/`lib/` includes. Depends only on `<PR/gbi.h>`, SDL2, and `platform.h`/`system.h`. Fork of libultraship's fast3d. **Blocked on the `PR/gbi.h` swap — see §4.** |
| `port/src/config.c` | INI-style config. Clean. |
| `port/src/input.c` | SDL input, mouselook, dual-analog. Clean. |
| `port/src/fs.c` | Path resolution, platform data dirs. Clean. |
| `port/src/crash.c` | Crash handler / backtrace. Clean. |
| `port/src/video.c` | Window/GL setup wrapper over fast3d. Clean. |
| `port/src/system.c`, `utils.c` | Small platform shims. Clean. |
| `port/src/preprocess/*` | PD asset-format specific in content, but the *technique* (load-time byteswap + fixup) is the reference for our own `ge_swap.c`. Read, don't copy. |

### Leave — PD-specific by construction

| File | Game-header includes | Why |
|---|---|---|
| `port/src/pdmain.c` | 70 | This *is* the PD frame loop. |
| `port/src/pdsched.c` | 20 | PD scheduler/gfx task glue. |
| `port/src/mpsetups.c` | 8 | PD multiplayer setups. |
| `port/src/optionsmenu.c` | 7 | PD's own menu system. |
| `port/src/mod.c` | 2 | PD mod loader. |
| `port/src/romdata.c` | 1 | Hardcodes PD ROM offsets (`ROMDATA_FILES_OFS 0x28080`) and is parameterized on `ROMID`/`VERSION` through CMake. |

### Verify before taking

| File | Issue |
|---|---|
| `port/src/mixer.c` | Credited to sm64-port in the README but carries **no license header**. sm64-port's own licensing is unclear. Treat as verify-or-reimplement; do not assume it is covered by PD's MIT. |

### The real coupling is the ABI, not the includes

Everything in `port/` speaks libultra: `osThread`, message queues, segment
addressing, big-endian ROM data, an 8 MB heap. That is an interface, and it is
the same interface `hostcompat/` and `src/ultra/` already implement here. The
practical question is therefore not "can we lift their port layer" but "can our
host layer present the surface fast3d expects" — a GBI stream plus
`platform.h`/`system.h`.

### Microcode caveat

`fast3d` targets **F3DEX2**. Per `MICROCODE-SPEC.md`, GoldenEye is **F3D with a
custom `G_TRI4`**. Taking fast3d gets us the framework, the GL/SDL backend, the
combiner handling, and the texture cache — **not** a working command decoder.
Budget for replacing the decode path.

## 4. The `include/PR/` problem

`include/PR/gbi.h`, `include/PR/ultratypes.h`, and their siblings in the PD
repo are verbatim Nintendo/SGI Ultra64 SDK headers carrying:

> Copyright (C) 1994, Silicon Graphics, Inc. These coded instructions,
> statements, and computer programs contain unpublished proprietary information
> of Silicon Graphics, Inc., and are protected by Federal copyright law. They
> may not be disclosed to third parties or copied or duplicated in any form, in
> whole or in part, without the prior written consent of Silicon Graphics, Inc.

The PD repo's MIT LICENSE does not and cannot cover these files. **They are not
vendorable.** This matters because `fast3d` — the piece we most want — includes
`<PR/gbi.h>`.

**Required mitigation before vendoring fast3d:** replace the `PR/gbi.h`
dependency with a clean header. Options, in preference order:

1. Generate our own GBI defines from `MICROCODE-SPEC.md`, which we derived
   independently and validated against a real ROM. This is the cleanest story
   and we already own the knowledge.
2. Use libdragon's GBI headers (permissive, clean-room lineage).
3. Use libultraship's own header variant, if it proves to be independently
   authored rather than inherited.

The same concern applies to `src/lib/ultra/*` in the PD repo (decompiled
libultra). Do not vendor it; `src/ultra/` here is our own.

## 5. Vendored inventory

| Subtree | Upstream | Pinned | License | Obligations |
|---|---|---|---|---|
| *(none yet)* | | | | |

**Candidates, with license status established:**

| Candidate | License | Obligation |
|---|---|---|
| PD port (`port/`, root) | MIT, (c) 2022 Ryan Dwyer | Reproduce copyright + permission notice. |
| `port/fast3d/` | MIT, (c) 2020 Emill, MaikelChan (`port/fast3d/LICENSE.txt`) | Ship that LICENSE file with the subtree. Separate from the root MIT. |
| `port/include/external/minimp3.h` | CC0 / public domain | None. |
| `port/fast3d/glad/khrplatform.h` | Khronos, MIT-style | Reproduce notice. |
| `include/PR/*` | **Proprietary (SGI)** | **Do not vendor.** |
| `port/src/mixer.c` | Unknown — no header, credited to sm64-port | Verify or reimplement. |

## 6. Attribution

The PD root MIT header names only Ryan Dwyer (the decompilation author), but the
port layer was written largely by `fgsfds` and contributors, licensed inbound
under the same terms via GitHub's Terms of Service. Attribution should therefore
credit the projects, not just the named copyright holder:

> Portions derived from the Perfect Dark decompilation and the Perfect Dark PC
> port, and their contributors. Licensed under the MIT License.
> Portions derived from fast3d, (c) 2020 Emill and MaikelChan, MIT License.

## 7. Upstream license posture

| Project | License |
|---|---|
| `n64decomp/007` (GoldenEye decomp) | **No LICENSE file — all rights reserved by default.** |
| `n64decomp/perfect_dark` / PD port | MIT |

GoldenEye's decomp being unlicensed is the direct reason for Hard Rule 1. Our
`patches/` + external-clone model is not a stylistic preference; it is what keeps
this repository distributable.

Note also the umbrella issue that applies to both: an MIT grant from decompilation
authors does not convey rights over the original copyright holders' expression
(Rare / Nintendo / Microsoft). The accepted posture for these projects — ship no
ROM data, require a user-supplied ROM, distribute source and tooling only — is
what we follow, with residual risk acknowledged.

**This document is engineering guidance, not legal advice.** If this project is
ever distributed as a binary product, obtain a real legal opinion first.

---

## 8. Addendum — audit of THIS repository, 2026-08-21

§4 raises the `include/PR/` problem for the PD port. The same question applies
to us, so it was checked rather than assumed.

**This repository ships no SGI headers.** `include/` contains only `ge_vr/`.
Nothing under `hostcompat/`, `src/` or `tools/` is a Nintendo/SGI Ultra64 SDK
file. The decomp's own `include/PR/` is reached at build time through
`tools/link_game.sh`'s `-I $REPO/include/PR`, from a clone the user supplies —
which is Hard Rule 1 working as intended, and is why it keeps working.

**Inventory in §5 stands at zero, and that is still true.** Nothing has been
vendored.

### The `PR/gbi.h` mitigation is now cheaper than §4 assumed

§4 offers three ways to replace fast3d's `<PR/gbi.h>` dependency and prefers
option 1 — generate our own defines from `MICROCODE-SPEC.md`. That was already
the right call, and it is now better supported: `src/gbi/gbi.h` in this
repository is already exactly that header. It is our own enumeration of the
thirteen opcodes GoldenEye actually uses, written from the spec, carrying no SGI
text, and it is the header `src/gbi/gbi_interp.cpp` compiles against today.

The gap between it and what fast3d needs is the RDP-side defines (combiner
modes, othermode bits, image formats), not the SP opcodes. That is the piece to
size before committing to fast3d.

**Caveat that got sharper today:** `src/gbi/gbi.h` was missing `0xB2`
(`G_RDPHALF_CONT`) until this same date — see `MICROCODE-SPEC.md` §2.1. A clean
header we author ourselves is only as good as the corpus we validated it
against, and ours had never walked a sky. Any header offered to a third party
should carry that provenance, and the list of paths it has *not* seen.
