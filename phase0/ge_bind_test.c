/*
 * ge_bind_test.c — VR-PLAN Phase 0 harness. MIT. C89.
 *
 * The fixed-point pack/unpack below is written from the Mtx layout documented
 * in gbi.h ("first 8 words are the integer portion, last 8 words are the
 * fraction portion", s15.16). It is deliberately NOT derived from libultra's
 * mtxutil.c, which carries SGI's proprietary notice — VENDORING.md §1 rule 3
 * forbids vendoring that regardless of what any containing LICENSE says.
 */
#include "ge_bind_test.h"

#include <math.h>
#include <stdlib.h>

#define GE_BT_FIX_ONE 65536.0

static int   g_inited  = 0;
static int   g_enabled = 1;
static float g_scale   = GE_BIND_TEST_DEFAULT_FOV_SCALE;
static unsigned long g_calls = 0;

static void geBtInit(void)
{
    if (g_inited) return;
    g_inited = 1;

#ifndef GE_BIND_TEST_NO_ENV
    {
        const char *e = getenv("GE_BIND_TEST");
        if (e && (e[0] == '0')) g_enabled = 0;

        e = getenv("GE_BIND_TEST_FOV_SCALE");
        if (e) {
            double v = atof(e);
            /* Reject nonsense rather than silently rendering a black frame and
             * letting the operator read it as "the hook does not work". */
            if (v > 0.05 && v < 20.0) g_scale = (float) v;
        }
    }
#endif
}

/* --- s15.16 access into the split-word Mtx layout ------------------------- *
 * Element (r,c) lives in word k = r*2 + (c/2). Its integer half is in that
 * word of the first eight; its fractional half is in word k+8. Even columns
 * take the high halves, odd columns the low halves.                          */

static int geBtWordIndex(int r, int c) { return r * 2 + (c >> 1); }

static double geBtGet(const GeBindTestMtx *m, int r, int c)
{
    const int k  = geBtWordIndex(r, c);
    const unsigned int iw = (unsigned int) m->m[k >> 2][k & 3];
    const unsigned int fw = (unsigned int) m->m[(k + 8) >> 2][(k + 8) & 3];
    int fixed;

    if ((c & 1) == 0)
        fixed = (int) ((iw & 0xffff0000u) | ((fw >> 16) & 0xffffu));
    else
        fixed = (int) (((iw & 0xffffu) << 16) | (fw & 0xffffu));

    return (double) fixed / GE_BT_FIX_ONE;
}

static void geBtSet(GeBindTestMtx *m, int r, int c, double value)
{
    const int k = geBtWordIndex(r, c);
    unsigned int *iw = (unsigned int *) &m->m[k >> 2][k & 3];
    unsigned int *fw = (unsigned int *) &m->m[(k + 8) >> 2][(k + 8) & 3];
    const int fixed = (int) (value * GE_BT_FIX_ONE);

    if ((c & 1) == 0) {
        *iw = (*iw & 0x0000ffffu) | ((unsigned int) fixed & 0xffff0000u);
        *fw = (*fw & 0x0000ffffu) | (((unsigned int) fixed << 16) & 0xffff0000u);
    } else {
        *iw = (*iw & 0xffff0000u) | (((unsigned int) fixed >> 16) & 0xffffu);
        *fw = (*fw & 0xffff0000u) | ((unsigned int) fixed & 0xffffu);
    }
}

/* --- the perturbation ----------------------------------------------------- *
 * guPerspectiveF writes cot(fovy/2)/aspect at [0][0] and cot(fovy/2) at [1][1].
 * Nothing else in the matrix depends on fovy, so recovering the angle from
 * [1][1] and rescaling both by the same ratio changes the field of view and
 * provably nothing else — near, far, the W row and perspNorm are untouched.
 * aspect cancels out of the ratio, so we never need to know it.              */

static double geBtRatio(double cot_current)
{
    double fovy_half, cot_target;

    if (!(cot_current > 1e-6)) return 1.0;   /* also rejects NaN */

    fovy_half  = atan(1.0 / cot_current);
    fovy_half *= (double) g_scale;

    /* Keep the result a legal frustum: past ~179 degrees cot goes to zero and
     * then negative, which would invert the image rather than widen it. */
    if (fovy_half > 1.5533430) fovy_half = 1.5533430;   /* 89 degrees */
    if (fovy_half < 1e-4)      fovy_half = 1e-4;

    cot_target = cos(fovy_half) / sin(fovy_half);
    return cot_target / cot_current;
}

void geBindTestPerturbMtx(GeBindTestMtx *m)
{
    double cot, r;

    geBtInit();
    if (!m) return;

    g_calls++;                 /* counted even when disabled: that is what
                                * distinguishes "never called" from "called
                                * and ignored". */
    if (!g_enabled) return;

    cot = geBtGet(m, 1, 1);
    r   = geBtRatio(cot);
    if (r == 1.0) return;

    geBtSet(m, 0, 0, geBtGet(m, 0, 0) * r);
    geBtSet(m, 1, 1, cot * r);
}

void geBindTestPerturbMtxf(float mf[4][4])
{
    double cot, r;

    geBtInit();
    if (!mf) return;

    g_calls++;
    if (!g_enabled) return;

    cot = (double) mf[1][1];
    r   = geBtRatio(cot);
    if (r == 1.0) return;

    mf[0][0] = (float) ((double) mf[0][0] * r);
    mf[1][1] = (float) (cot * r);
}

unsigned long geBindTestCallCount(void) { return g_calls; }
void  geBindTestReset(void)             { g_calls = 0; }
void  geBindTestSetEnabled(int e)       { geBtInit(); g_enabled = e ? 1 : 0; }
int   geBindTestEnabled(void)           { geBtInit(); return g_enabled; }
float geBindTestFovScale(void)          { geBtInit(); return g_scale; }
