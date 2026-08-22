/*
 * ge_vr_bindtest.c — VR-PLAN Phase 0, as a drop-in patch for
 * cblock85/GoldenEye64Recomp.
 *
 * Copy to that repo's patches/ and add to patches/Makefile. See README.md here.
 *
 * ---------------------------------------------------------------------------
 * WHY THIS PATCHES THE SETTERS, NOT THE GETTERS
 * ---------------------------------------------------------------------------
 * VR-PLAN §2 names the getters as the attachment surface, and for STEREO they
 * are the right seam. For this test they are the wrong one, because we mutate
 * in place and the getters are called many times per frame — bg.c:692, :729 and
 * six more sites, sky.c:811, bondview2.c:8244. Perturbing in a getter would
 * rescale the same matrix on every call and compound within a single frame,
 * producing a field of view that collapses to nothing. That renders black,
 * which is the one outcome a bind test must never produce, because black is
 * indistinguishable from "the hook is broken".
 *
 * The setters are called exactly once per frame, from src/fr.c:720-721, right
 * after guPerspectiveF rebuilds the matrix. Perturbing there is idempotent by
 * construction.
 *
 * Both setters are pure one-line assignments in the decomp
 * (src/game/bondview.c:802 and :828), so replacing them wholesale via
 * RECOMP_PATCH loses nothing — there is no original behaviour to preserve
 * beyond the store itself.
 *
 * ---------------------------------------------------------------------------
 * WHY IT SCALES cot DIRECTLY INSTEAD OF SCALING THE ANGLE
 * ---------------------------------------------------------------------------
 * The host-side harness (phase0/ge_bind_test.c) recovers fovy via atan() so it
 * can scale the angle by an exact factor. This patch is compiled into the
 * game's MIPS address space, where libm is not dependably available. Scaling
 * cot(fovy/2) by a constant changes the field of view just as visibly and needs
 * nothing but a multiply. A bind test asks "is it different", not "is it
 * different by exactly 1.6x", so the trig buys nothing here.
 *
 * GE_BT_COT_SCALE = 0.625 takes the game's 60 degree vertical FOV to roughly
 * 85 degrees. Unmistakable at a glance, still a coherent and navigable world.
 * Smaller values widen further.
 */
#include "patches.h"

#define GE_BT_COT_SCALE 0.625f

/* Set to 0 to compile the patch in but leave the image untouched — the A/B
 * control. The call counters still increment, which is the whole point. */
#ifndef GE_BT_ENABLED
#define GE_BT_ENABLED 1
#endif

/* Observable from a debugger, or print them from an existing patch. The two
 * counters answer different questions:
 *   both zero      -> the patch never ran. Attachment problem, not a plan
 *                     problem: check patches.elf actually contains it.
 *   nonzero, image unchanged
 *                  -> the setters run but the world projection reaches the RSP
 *                     by another path. THIS falsifies VR-PLAN §2 and is the
 *                     only result that should stop the project. */
u32 g_geBtSetMtxCalls  = 0;
u32 g_geBtSetMtxfCalls = 0;

#if GE_BT_ENABLED
/* --- fixed-point element access ------------------------------------------ *
 * Mtx is sixteen s15.16 values: eight words of integer halves followed by
 * eight of fractional halves (PR/gbi.h). Element (r,c) lives in word
 * k = r*2 + c/2; even columns take the high halves, odd columns the low.
 * Written from that documented layout, not from libultra's mtxutil.c, which
 * carries SGI's proprietary notice (VENDORING.md §1 rule 3). */
static void geBtScaleFixed(Mtx* m, s32 r, s32 c, f32 scale) {
    s32 k = r * 2 + (c >> 1);
    u32* iw = (u32*) &m->m[k >> 2][k & 3];
    u32* fw = (u32*) &m->m[(k + 8) >> 2][(k + 8) & 3];
    s32 fixed;
    f32 value;

    if ((c & 1) == 0) fixed = (s32) ((*iw & 0xffff0000u) | ((*fw >> 16) & 0xffffu));
    else              fixed = (s32) (((*iw & 0xffffu) << 16) | (*fw & 0xffffu));

    value = ((f32) fixed / 65536.0f) * scale;
    fixed = (s32) (value * 65536.0f);

    if ((c & 1) == 0) {
        *iw = (*iw & 0x0000ffffu) | ((u32) fixed & 0xffff0000u);
        *fw = (*fw & 0x0000ffffu) | (((u32) fixed << 16) & 0xffff0000u);
    } else {
        *iw = (*iw & 0xffff0000u) | (((u32) fixed >> 16) & 0xffffu);
        *fw = (*fw & 0xffff0000u) | ((u32) fixed & 0xffffu);
    }
}
#endif

/* guPerspectiveF writes cot(fovy/2)/aspect at [0][0] and cot(fovy/2) at [1][1],
 * and nothing else in the matrix depends on fovy. Touching exactly those two
 * leaves near, far, the W row and perspNorm provably intact — so this cannot
 * produce a black frame, only a wider one or an identical one. Those are the
 * only two images Phase 0 needs to tell apart.
 *
 * The per-level near/far established in VR-PLAN §6.1 never enter into it. */

RECOMP_PATCH void currentPlayerSetProjectionMatrix(Mtx* matrix) {
    g_geBtSetMtxCalls++;
#if GE_BT_ENABLED
    if (matrix) {
        geBtScaleFixed(matrix, 0, 0, GE_BT_COT_SCALE);
        geBtScaleFixed(matrix, 1, 1, GE_BT_COT_SCALE);
    }
#endif
    g_CurrentPlayer->projmatrix = matrix;
}

RECOMP_PATCH void currentPlayerSetProjectionMatrixF(Mtxf* matrix) {
    g_geBtSetMtxfCalls++;
#if GE_BT_ENABLED
    if (matrix) {
        matrix->m[0][0] *= GE_BT_COT_SCALE;
        matrix->m[1][1] *= GE_BT_COT_SCALE;
    }
#endif
    g_CurrentPlayer->projmatrixf = matrix;
}
