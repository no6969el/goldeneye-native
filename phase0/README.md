# Phase 0 — the bind test

VR-PLAN §4 puts this first because it is the cheapest thing that can invalidate
the whole plan. It answers one question:

> Does overriding GoldenEye's camera accessors actually reach the world
> projection?

Everything in Phases 1–4 assumes yes. Days to find out, versus months to find
out later.

## Run the selftest first

```sh
cmake -S . -B build && cmake --build build && ./build/ge_bind_test_selftest
```

This needs no ROM, no host and no headset. It exists because the bind test's
negative result — "the FOV did not change" — is only evidence about the *hook*
if the harness is known to change the FOV. A broken packer yields the identical
sentence and would get the plan abandoned for the wrong reason.

**If the selftest fails, a bind-test result means nothing.**

## Then attach it

See `adapters/README.md`. Three mechanisms — a source edit on a decomp host,
`RECOMP_PATCH_FUNC` on a recomp host, `LD_PRELOAD` on a dynamically-linked one.
The harness is identical in all three.

## Reading the result

Load any level and look at the world.

| What you see | Call count | Verdict |
|---|---|---|
| Noticeably wider FOV | > 0 | **Hook model works.** Phases 1–4 are engineering. Proceed. |
| No change | 0 | Hook never ran. Attachment problem, *not* a plan problem — check the symbol survived (inlining, LTO, static linking) before concluding anything. |
| No change | > 0 | **The interesting failure.** The accessor runs but the world projection reaches the RSP another way, contradicting VR-PLAN §2. The four-symbol attachment surface is wrong and stereo needs a different seam. |
| Black / corrupt frame | any | Should be impossible by construction — only `[0][0]` and `[1][1]` are touched. If it happens, suspect the host, and re-run the selftest. |

That third row is the one worth the whole exercise. It is the only outcome that
falsifies the plan, and it is invisible without the counter — which is why the
counter increments even when the perturbation is disabled.

## Controls

| Variable | Default | Meaning |
|---|---|---|
| `GE_BIND_TEST` | `1` | `0` disables the perturbation but keeps counting — the A/B control. |
| `GE_BIND_TEST_FOV_SCALE` | `1.6` | Multiplier on fovy. Out-of-range values are rejected rather than rendering black. |

A/B the same binary against the same save with `GE_BIND_TEST=0` and `=1`. If you
cannot tell the two apart, that is your answer.

## What this deliberately does not do

No stereo, no OpenXR, no `ge_vr` code at all — the harness does not link
`include/ge_vr/`. Phase 0 is a question about hooks, not about VR, and keeping
it independent means a negative result costs you nothing but this directory.

## Licence note

The fixed-point matrix pack/unpack here is written from the layout documented in
`gbi.h`, not copied from libultra's `mtxutil.c` / `perspective.c` — both carry
SGI's proprietary notice, which VENDORING.md §1 rule 3 forbids vendoring
regardless of the containing repository's LICENSE.
