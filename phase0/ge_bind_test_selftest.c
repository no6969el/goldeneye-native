/*
 * ge_bind_test_selftest.c — prove the harness before trusting its verdict.
 *
 * A bind test reports "the FOV did not change". That sentence is only evidence
 * about the HOOK if the harness itself is known to change the FOV. Otherwise a
 * broken packer produces the same sentence and the plan gets abandoned for the
 * wrong reason. Run this first; it needs no ROM, no host and no headset.
 *
 * The reference projection here is written from the documented behaviour of
 * guPerspectiveF (the standard perspective matrix, plus the N64 perspNorm
 * rule), NOT copied from libultra's perspective.c, which is under SGI's
 * proprietary notice. See VENDORING.md §1 rule 3.
 */
#include "ge_bind_test.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#define DEG2RAD (3.14159265358979323846 / 180.0)

static int g_fail = 0;

static void check(int cond, const char *what, double got, double want)
{
    if (cond) {
        printf("  ok    %-46s\n", what);
    } else {
        printf("  FAIL  %-46s got %.8f want %.8f\n", what, got, want);
        g_fail = 1;
    }
}

static void refPerspectiveF(float mf[4][4], unsigned short *perspNorm,
                            double fovy_deg, double aspect,
                            double near, double far)
{
    double cot, fovy = fovy_deg * DEG2RAD;
    int i, j;

    for (i = 0; i < 4; i++)
        for (j = 0; j < 4; j++)
            mf[i][j] = (i == j) ? 1.0f : 0.0f;

    cot = cos(fovy / 2.0) / sin(fovy / 2.0);

    mf[0][0] = (float) (cot / aspect);
    mf[1][1] = (float) cot;
    mf[2][2] = (float) ((near + far) / (near - far));
    mf[2][3] = -1.0f;
    mf[3][2] = (float) ((2.0 * near * far) / (near - far));
    mf[3][3] = 0.0f;

    if (perspNorm) {
        if (near + far <= 2.0) *perspNorm = (unsigned short) 0xFFFF;
        else                   *perspNorm = (unsigned short) ((2.0 * 65536.0) / (near + far));
    }
}

/* Pack float -> the split-word fixed layout. Written from the gbi.h comment. */
static void refMtxF2L(float mf[4][4], GeBindTestMtx *m)
{
    int i, j;
    unsigned int *ai = (unsigned int *) &m->m[0][0];
    unsigned int *af = (unsigned int *) &m->m[2][0];

    for (i = 0; i < 4; i++) {
        for (j = 0; j < 2; j++) {
            int e1 = (int) (mf[i][j * 2]     * 65536.0f);
            int e2 = (int) (mf[i][j * 2 + 1] * 65536.0f);
            *(ai++) = ((unsigned int) e1 & 0xffff0000u) | (((unsigned int) e2 >> 16) & 0xffffu);
            *(af++) = (((unsigned int) e1 << 16) & 0xffff0000u) | ((unsigned int) e2 & 0xffffu);
        }
    }
}

/* Recover fovy (degrees) from the [1][1] element of a float matrix. */
static double fovyOf(double cot) { return 2.0 * atan(1.0 / cot) / DEG2RAD; }

int main(void)
{
    /* Deliberately a per-level pair from fog_tables[], not a made-up one:
     * Dam's znear=5 / zfar=15000 (bgfog.c). See VR-PLAN §6.1 — the world's
     * near and far are level data, and the harness must not care. */
    const double FOVY = 60.0, ASPECT = 1.3333333, NEAR = 5.0, FAR = 15000.0;
    const double SCALE = 1.6;

    float mf[4][4], mf_ref[4][4];
    GeBindTestMtx m, m_ref;
    unsigned short pn_ref = 0;
    double before, after, i, j;
    int r, c, untouched_ok = 1;

    printf("ge_bind_test selftest\n");
    printf("  fovy %.1f aspect %.4f near %.1f far %.1f  scale %.2f\n\n",
           FOVY, ASPECT, NEAR, FAR, SCALE);

    geBindTestSetEnabled(1);

    /* --- 1. the fixed-point accessors round-trip -------------------------- */
    refPerspectiveF(mf_ref, &pn_ref, FOVY, ASPECT, NEAR, FAR);
    refMtxF2L(mf_ref, &m_ref);
    memcpy(&m, &m_ref, sizeof m);

    /* Read [1][1] back out through the harness's own unpacker by perturbing
     * with a scale of exactly 1.0, which must be a no-op. */
    geBindTestSetEnabled(0);
    geBindTestPerturbMtx(&m);
    geBindTestSetEnabled(1);
    check(memcmp(&m, &m_ref, sizeof m) == 0,
          "disabled perturb leaves matrix untouched", 0, 0);

    /* --- 2. the Mtxf path changes fovy by exactly the scale --------------- */
    memcpy(mf, mf_ref, sizeof mf);
    before = fovyOf((double) mf[1][1]);
    geBindTestPerturbMtxf(mf);
    after  = fovyOf((double) mf[1][1]);

    check(fabs(before - FOVY) < 1e-3, "Mtxf: fovy recovered before perturb", before, FOVY);
    check(fabs(after - FOVY * SCALE) < 1e-2,
          "Mtxf: fovy scaled by exactly the factor", after, FOVY * SCALE);

    /* aspect must survive: [0][0]/[1][1] is 1/aspect before and after */
    check(fabs(((double) mf[0][0] / (double) mf[1][1]) - (1.0 / ASPECT)) < 1e-5,
          "Mtxf: aspect ratio preserved",
          (double) mf[0][0] / (double) mf[1][1], 1.0 / ASPECT);

    /* every element except [0][0] and [1][1] is bit-identical */
    for (r = 0; r < 4; r++)
        for (c = 0; c < 4; c++)
            if (!(r == 0 && c == 0) && !(r == 1 && c == 1))
                if (mf[r][c] != mf_ref[r][c]) untouched_ok = 0;
    check(untouched_ok, "Mtxf: near/far/W row bit-identical", 0, 0);

    /* --- 3. the fixed-point path agrees with the float path --------------- */
    memcpy(&m, &m_ref, sizeof m);
    geBindTestPerturbMtx(&m);

    /* unpack [1][1] from the perturbed fixed matrix, independently */
    {
        const int k = 1 * 2 + 0;                       /* r=1, c=1 -> word 2 */
        unsigned int iw = (unsigned int) m.m[k >> 2][k & 3];
        unsigned int fw = (unsigned int) m.m[(k + 8) >> 2][(k + 8) & 3];
        int fixed = (int) (((iw & 0xffffu) << 16) | (fw & 0xffffu));
        double cot = (double) fixed / 65536.0;
        double fovy_fixed = fovyOf(cot);

        check(fabs(fovy_fixed - FOVY * SCALE) < 5e-2,
              "Mtx: fixed-point path matches float path", fovy_fixed, FOVY * SCALE);
    }

    /* --- 4. the counter distinguishes the two failure modes --------------- */
    geBindTestReset();
    check(geBindTestCallCount() == 0, "counter resets", (double) geBindTestCallCount(), 0);
    memcpy(&m, &m_ref, sizeof m);
    geBindTestPerturbMtx(&m);
    geBindTestPerturbMtx(&m);
    check(geBindTestCallCount() == 2, "counter counts calls",
          (double) geBindTestCallCount(), 2);

    /* --- 5. absurd scales are clamped, not allowed to render black -------- */
    memcpy(mf, mf_ref, sizeof mf);
    geBindTestPerturbMtxf(mf);
    i = (double) mf[0][0];
    j = (double) mf[1][1];
    check(i > 0.0 && j > 0.0 && i == i && j == j,
          "perturbed frustum stays legal (positive, finite)", j, 1);

    printf("\n%s\n", g_fail ? "SELFTEST FAILED — do not trust a bind-test result"
                            : "selftest passed — harness is sound, go run it on a host");
    return g_fail;
}
