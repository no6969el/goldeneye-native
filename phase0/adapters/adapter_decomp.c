/*
 * adapter_decomp.c — attaching the Phase 0 harness to a DECOMP-NATIVE host
 * (goldeneye-native, or any build that compiles src/game/bondview.c).
 *
 * Here the "hook" is not a hook at all: the four accessors are named,
 * non-static and one line each, so you edit them. This file shows the edit;
 * apply it via patches/ rather than by hand, per VENDORING.md §1 rule 1.
 *
 * Original (src/game/bondview.c):
 *
 *     Mtx  *currentPlayerGetProjectionMatrix(void)  { return g_CurrentPlayer->projmatrix;  }
 *     Mtxf *currentPlayerGetProjectionMatrixF(void) { return g_CurrentPlayer->projmatrixf; }
 *
 * Patched: mutate in place, return the game's own pointer unchanged. Do NOT
 * return a pointer to storage of your own — see the header for why that
 * reproduces the RESUME-HERE.md fiber-stack truncation bug.
 */
#include "../ge_bind_test.h"

/* Sketch only — the real edit lives in patches/. Types come from the host. */
#if 0
Mtx *currentPlayerGetProjectionMatrix(void)
{
    Mtx *m = g_CurrentPlayer->projmatrix;
    geBindTestPerturbMtx((GeBindTestMtx *) m);
    return m;
}

Mtxf *currentPlayerGetProjectionMatrixF(void)
{
    Mtxf *m = g_CurrentPlayer->projmatrixf;
    geBindTestPerturbMtxf((float (*)[4]) m);
    return m;
}
#endif

/* Prefer the F variant if you must pick one. VR-PLAN §6.2: the fixed-point
 * conversion applies a per-level world scale (0.2 on Dam, Surface, Surface 2),
 * so anything composed at the Mtx level is in a different space than anything
 * composed at the Mtxf level. For a FOV-only test it does not matter — fovy is
 * scale-invariant — but forming the habit now avoids the IPD bug later. */
