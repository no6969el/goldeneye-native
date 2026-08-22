/*
 * ge_bind_test.h — VR-PLAN Phase 0: does a symbol-level hook on GoldenEye's
 * camera accessors actually reach the world projection?
 *
 * MIT. C89. No OpenXR, no C++, no host dependency — same hygiene rule as
 * include/ge_vr/ge_vr.h, for the same reason: this must attach to a decomp-
 * native host and a static-recompilation host without modification.
 *
 * ---------------------------------------------------------------------------
 * WHAT THIS TEST ANSWERS
 * ---------------------------------------------------------------------------
 * VR-PLAN §2 claims the entire attachment surface for stereo is four one-line
 * accessors on g_CurrentPlayer. Everything in Phases 1-4 rests on that claim.
 * This harness costs days to run and invalidates the plan if it fails, which is
 * why VR-PLAN §4 puts it first.
 *
 * It perturbs the world field of view through the accessor and asks: did the
 * world's FOV change?
 *
 * ---------------------------------------------------------------------------
 * WHY IT MUTATES IN PLACE RATHER THAN RETURNING A MATRIX
 * ---------------------------------------------------------------------------
 * The obvious shape is to return a pointer to our own Mtx. Do not. The game
 * feeds these pointers to gSPMatrix(gdl++, osVirtualToPhysical(m), ...), so a
 * pointer into host memory rather than RDRAM gets truncated to whichever 32
 * bits the allocator happened to hand out. That is precisely the fiber-stack
 * bug documented in RESUME-HERE.md, and it would present as a corrupt or black
 * frame — indistinguishable from "the hook does not work", which is the one
 * question this test exists to answer.
 *
 * Mutating the matrix the game already allocated sidesteps address translation
 * entirely and behaves identically on both host types.
 *
 * ---------------------------------------------------------------------------
 * WHY IT SCALES TWO ELEMENTS RATHER THAN REBUILDING THE MATRIX
 * ---------------------------------------------------------------------------
 * A projection built from scratch can be wrong in ways that render black. Black
 * is the ambiguous outcome: it could mean the hook bound and our matrix is
 * broken, or that the hook did something worse. A test whose failure mode is
 * ambiguous is not a test.
 *
 * guPerspectiveF puts cot(fovy/2)/aspect at [0][0] and cot(fovy/2) at [1][1],
 * and nothing else in the matrix depends on fovy. Scaling exactly those two
 * elements changes the field of view and can change nothing else: near, far,
 * the W row and perspNorm are never touched. The frame either renders with a
 * visibly different FOV or renders exactly as before. There is no third image.
 *
 * The original fovy is recoverable from [1][1] alone, so the harness needs to
 * be told nothing about aspect, near or far — which is good, because §6.1
 * established those are per-level data.
 */
#ifndef GE_BIND_TEST_H
#define GE_BIND_TEST_H

#ifdef __cplusplus
extern "C" {
#endif

/* The N64 fixed-point matrix, as the RSP consumes it: sixteen s15.16 values
 * split into eight words of integer parts followed by eight of fractions.
 * Declared here rather than included from libultra so this file has no
 * dependency on a host's headers — and because libultra's own sources carry
 * SGI's proprietary notice (VENDORING.md §1 rule 3) and must not be vendored. */
typedef union {
    int       m[4][4];       /* 16 words: 8 integer, then 8 fractional */
    double    force_structure_alignment;   /* 8-byte align, as gbi.h does with
                                            * long long; double keeps this
                                            * header strict C89. */
} GeBindTestMtx;

/* Default perturbation. 1.6x is chosen to be unmistakable at a glance while
 * still rendering a coherent, navigable world — a subtle change invites
 * "maybe?", and a violent one invites "is it broken?". Both are useless
 * answers. Override with GE_BIND_TEST_FOV_SCALE. */
#define GE_BIND_TEST_DEFAULT_FOV_SCALE 1.6f

/* Perturb a fixed-point projection matrix in place. Safe to call every frame
 * and on every matrix; a NULL pointer is ignored. */
void geBindTestPerturbMtx(GeBindTestMtx *m);

/* Perturb a float projection matrix in place. Same contract. */
void geBindTestPerturbMtxf(float mf[4][4]);

/* ---------------------------------------------------------------------------
 * Diagnosis
 * ---------------------------------------------------------------------------
 * "The FOV did not change" has two very different causes, and guessing between
 * them is how days get spent:
 *
 *   (a) the hook never ran            -> call count is 0
 *   (b) the hook ran, the game ignored the matrix -> call count climbs
 *
 * (b) would mean the world projection reaches the RSP by some path other than
 * these accessors, which contradicts VR-PLAN §2 and is the genuinely
 * interesting failure. Read the counter before concluding anything.
 */
unsigned long geBindTestCallCount(void);

/* Reset the counter. */
void geBindTestReset(void);

/* Enable/disable at runtime. Also read once from the environment variable
 * GE_BIND_TEST (0 or 1) on first use, so a host can be toggled without a
 * rebuild — useful for A/B-ing the same binary against the same save. */
void geBindTestSetEnabled(int enabled);
int  geBindTestEnabled(void);

/* The scale actually in force, after the environment override is applied. */
float geBindTestFovScale(void);

#ifdef __cplusplus
}
#endif
#endif /* GE_BIND_TEST_H */
