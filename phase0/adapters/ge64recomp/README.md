# Phase 0 on `cblock85/GoldenEye64Recomp`

This is the host to run Phase 0 against. It is the N64 recomp — N64Recomp +
RT64 + N64ModernRuntime — and it renders today.

**Not `SunJaycy/GoldenEye-Recomp`.** That is the cancelled Xbox 360 remaster
recompiled via XenonRecomp: PowerPC, `default.xex`, raw addresses instead of C
symbol names, different engine, different geometry. PRIOR-ART §3.2 already ruled
it out. Nothing here applies to it.

## What was verified before writing this

Checked directly against the repo, not assumed:

| Claim | Evidence |
|---|---|
| The four accessors survive recompilation as named symbols | `dump.toml` — `currentPlayerGetProjectionMatrix` @ `0x80478374`, `...MatrixF` @ `0x804783E4`, `camGetWorldToScreenMtxf` @ `0x804783C4`, `currentPlayerSetMatrix10CC` @ `0x804783A4`, each with size |
| The setters are patchable too | `dump.toml` — `currentPlayerSetProjectionMatrix` @ `0x80478364`, `...MatrixF` @ `0x804783D4` |
| Patching is a supported, working mechanism | `RECOMP_PATCH` in `patches/patches.h`; `patches/skybox.c` already replaces `skyRenderTri` |
| The world projection is already successfully altered in this host | `patches/workbench_theboy.c:216,267` patches `viSetFovY`/`viSetAspect` for widescreen |
| `g_CurrentPlayer` and the struct fields are reachable from a patch | `patches/externs.h:408`; `patches/structs.h:2649-2650` (`projmatrix`, `projmatrixf`) |
| This patch actually compiles in their toolchain | built with their exact `patches/Makefile` flags for `-target mips`; both functions emitted into `.recomp_patch` |

That last row matters: the file in this directory was compiled, not sketched.

## End-to-end verification against a real ROM (2026-08-21)

The whole pipeline was run, using a user-supplied NTSC-U dump
(sha1 `abe01e...de83`, matching the decomp's own `ge007.u.sha1`):

1. `xdelta3 -d -s baserom.u.z64 vanilla_to_tlbfree.xdelta ge007.tlbfree.z64` — OK.
2. `N64Recomp us.toml` — **3,150 functions** recompiled from that ROM. OK.
3. `patches/` built with `ge_vr_bindtest.c` present → `patches.elf` containing
   `currentPlayerSetProjectionMatrix` (188 bytes) and `...MatrixF` (68 bytes).
4. `N64Recomp patches.toml` — 347 functions; both appear in
   `RecompiledPatches/patches.c` with the expected generated MIPS: counter
   increment, null check, then `mul.s` on the words at offsets `0x0` and `0x14`
   — which are exactly `m[0][0]` and `m[1][1]` of an `f32 m[4][4]`.
5. `python3 tools_weaken_patched.py` — "weakened 38 patched functions".

Step 5 is the one that proves the override actually binds. Afterwards:

```
funcs_36.c: RECOMP_FUNC __attribute__((weak)) void currentPlayerSetProjectionMatrixF(...)
funcs_59.c: RECOMP_FUNC __attribute__((weak)) void currentPlayerSetProjectionMatrix(...)
funcs_49.c: RECOMP_FUNC                       void camGetWorldToScreenMtxf(...)
funcs_58.c: RECOMP_FUNC                       void currentPlayerGetProjectionMatrixF(...)
```

**Exactly the two functions this patch replaces were weakened; the two it does
not touch were left strong.** The link-time override will happen. Everything
between the source file and the linker is verified — what remains is running it
on a GPU.

Not verified here, because this container has no Vulkan device and no display:
the final `GoldenRecomp` link (needs SDL2 / freetype / GTK3) and the image
itself.

That fifth row matters more. Widescreen in that host works by changing the
world projection through this chain. **Phase 0's question is substantially
answered before you run it** — what remains is confirming the specific seam VR
needs, which is the setter rather than the aspect helper.

## Install

`patches/Makefile` uses `$(wildcard *.c)`, so there is no Makefile edit:

```sh
cp ge_vr_bindtest.c  <GoldenEye64Recomp>/patches/
```

Then build the project as its README describes. The patch is picked up
automatically.

## Run it

Load any level. The world's field of view should be visibly wider — roughly 85
degrees vertical instead of 60.

To A/B without touching anything else, rebuild with the perturbation compiled
out but the counters live:

```sh
# in patches/ge_vr_bindtest.c
#define GE_BT_ENABLED 0
```

## Reading the result

| Image | `g_geBtSetMtxCalls` | Verdict |
|---|---|---|
| Visibly wider FOV | > 0 | **VR-PLAN §2 holds.** The attachment surface is real. Phases 1–4 are engineering. |
| Unchanged | 0 | The patch never ran. Build problem — confirm it reached `patches.elf`. Not a plan problem. |
| Unchanged | > 0 | **The result that matters.** The setter runs but the projection reaches the RSP another way. VR-PLAN §2 is wrong and stereo needs a different seam. |
| Black / corrupt | any | Should be impossible — only `[0][0]` and `[1][1]` are touched. Suspect the host. |

## Licence

That host is **GPL-3.0**, and it links `N64ModernRuntime`, also GPL-3.0 (note
this repo uses a fork, `kholdfuzion/N64ModernRuntime`, which inherits it). Per
PRIOR-ART §4.3, keep it **external** — build it separately, drop this file in.
Do not vendor it here. This patch file is ours and stays MIT; a binary built
from that tree is GPL-3.0 as distributed.
