/*
 * ge_vr.h — the C ABI between the GoldenEye 007 decomp game code and the VR layer.
 *
 * This header is deliberately plain C89-compatible so it can be included directly
 * from the decomp's own translation units (src/fr.c, src/game/bondview2.c,
 * src/game/gunfire.c, ...) without dragging C++ or OpenXR into the game build.
 *
 * Contract:
 *   - Every function here is safe to call when VR is inactive. In that case the
 *     "is active" queries return 0 and the getters write neutral values, so the
 *     patched game code takes its original path with no branching noise.
 *   - Nothing here allocates. Nothing here blocks.
 *   - All angles are radians. All positions are in GAME units (see GE_VR_UNITS_PER_METRE).
 *   - Coordinate handedness follows the game, not OpenXR. Conversion happens
 *     inside the bridge; game code never sees an OpenXR type.
 */

#ifndef GE_VR_H
#define GE_VR_H

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------------- *
 * Configuration
 * ------------------------------------------------------------------------- */

/*
 * GoldenEye's world unit. TODO(phase5): measure this properly against a known
 * in-game dimension (door height in Facility is the usual reference) — an
 * incorrect scale is THE most common cause of "the world feels like a dollhouse"
 * or "I am a giant" in retrofitted VR, and it is not fixable downstream.
 */
#define GE_VR_UNITS_PER_METRE 100.0f

typedef enum {
    GE_VR_EYE_LEFT  = 0,
    GE_VR_EYE_RIGHT = 1,
    GE_VR_EYE_COUNT = 2,
    /* Used for culling and for any game system that needs a single frustum
     * enclosing both eyes. See architecture doc §5.3. */
    GE_VR_EYE_UNION = 2
} GeVrEye;

typedef enum {
    GE_VR_HAND_LEFT  = 0,
    GE_VR_HAND_RIGHT = 1,
    GE_VR_HAND_COUNT = 2
} GeVrHand;

/* ------------------------------------------------------------------------- *
 * Lifecycle — called by the port's main(), not by game code.
 * ------------------------------------------------------------------------- */

int  geVrInit(void);       /* 0 on success; nonzero means run flat-screen.   */
void geVrShutdown(void);

/* ------------------------------------------------------------------------- *
 * Queries — safe from anywhere, including game code.
 * ------------------------------------------------------------------------- */

/* Nonzero when an OpenXR session is running and focused. Game code branches on
 * this to choose the VR path vs. the original path. */
int geVrIsActive(void);

/* Which eye is currently being rendered. Set by the frame loop before each
 * render pass; read by fr.c. Returns GE_VR_EYE_LEFT when VR is inactive. */
GeVrEye geVrCurrentEye(void);

/* ------------------------------------------------------------------------- *
 * Projection — replaces guPerspectiveF in viSetupCurrentPlayerView().
 * ------------------------------------------------------------------------- */

/*
 * Build the projection matrix for the eye currently being rendered.
 *
 * Produces the SAME layout and sign conventions as libultra's guPerspectiveF
 * (row-vector convention: mf[2][3] == -1, mf[3][2] holds the depth term), so
 * guMtxF2L() and gSPPerspNormalize() consume it unchanged. The difference is
 * that the frustum is ASYMMETRIC — built from the runtime's four per-eye
 * half-angles rather than a single symmetric fovy. See architecture doc §5.1.
 *
 * perspNorm is computed identically to guPerspectiveF so RSP W-divide precision
 * is unchanged; passing a wrong value here shows up as depth fighting at range,
 * not as an obviously broken image.
 *
 * Returns 0 if VR is inactive, in which case mf/perspNorm are untouched and the
 * caller must fall back to guPerspectiveF.
 */
int geVrBuildProjectionF(float mf[4][4], unsigned short *perspNorm,
                         float znear, float zfar, float scale);

/*
 * Post-multiply transform carrying this eye's offset from the head origin, plus
 * head roll and 6DoF positional offset. Applied to the view matrix BELOW the
 * game — the game's own guLookAt output stays authoritative for where the player
 * is standing and looking; this adds what the tracker knows and the game doesn't.
 *
 * Returns 0 if VR is inactive (mf untouched).
 */
int geVrGetEyeViewOffsetF(float mf[4][4]);

/* ------------------------------------------------------------------------- *
 * Head — feeds the player's view angles.
 * ------------------------------------------------------------------------- */

/*
 * Head orientation relative to the play-space origin, in the game's angle
 * convention (theta = yaw, verta = pitch, both radians).
 *
 * IMPORTANT: yaw is ADDITIVE to the player's stick-accumulated yaw. Writing it
 * as authoritative makes it impossible to turn past your own neck. See §6.2.
 */
void geVrGetHeadAngles(float *out_theta, float *out_verta, float *out_roll);

/*
 * Head position relative to the play-space origin, in GAME units.
 *
 * This is the actual 6DoF term. It must be applied to the CAMERA only — never
 * to the player collision capsule — or leaning becomes a wall-clip exploit.
 * Callers should clamp against geVrGetPositionalClamp() before use.
 */
void geVrGetHeadPosition(float out_xyz[3]);

/*
 * How far the head is allowed to stray from the capsule centre before comfort
 * greyout kicks in, in game units. The bridge shrinks this when the renderer
 * reports the head is near geometry.
 */
float geVrGetPositionalClamp(void);

/* 0.0 = clear, 1.0 = fully blanked. Driven by the clamp above and by locomotion
 * vignette. Composited by the renderer, not by game code. */
float geVrGetComfortFade(void);

/* ------------------------------------------------------------------------- *
 * Hands — feeds weapon aim and hitscan.
 * ------------------------------------------------------------------------- */

/* Nonzero if this hand's pose is currently tracked and valid. When 0, callers
 * must fall back to view-relative aim — dropped tracking must degrade to the
 * original behaviour, never to a ray pointing at the floor. */
int geVrHandIsTracked(GeVrHand hand);

/*
 * The firing ray for this hand, in world space, game units.
 *
 * This is what src/game/gunfire.c must use instead of the camera ray. Everything
 * downstream of the ray (penetration, bondwalkItemGetObjectsShootThrough,
 * damage application) is unchanged. See architecture doc §6.2.
 */
void geVrGetAimRay(GeVrHand hand, float out_origin[3], float out_dir[3]);

/*
 * Angular displacement of this hand's aim relative to the current view
 * direction. Written straight into the player struct's existing
 * weapon_theta_displacement / weapon_verta_displacement fields (bondview.h) —
 * the mechanism GoldenEye already uses for sway and recoil.
 */
void geVrGetWeaponDisplacement(GeVrHand hand, float *out_dtheta, float *out_dverta);

/*
 * Full model transform for drawing the weapon at the controller, in world space.
 * Replaces the view-relative first-person weapon transform. Row-vector layout,
 * same as the game's Mtx.
 */
int geVrGetWeaponModelMatrixF(GeVrHand hand, float mf[4][4]);

/* Recoil, impacts, watch interaction. amplitude 0..1, duration in seconds.
 * Recoil goes here — NOT into the view. View kick from an untracked source is
 * a top cause of VR discomfort. */
void geVrHaptic(GeVrHand hand, float amplitude, float duration_s, float frequency_hz);

/* ------------------------------------------------------------------------- *
 * Controller state — synthesised into an OSContPad by the input shim so all
 * unmodified game input code keeps working. Exposed here for the few systems
 * that need the analogue values directly.
 * ------------------------------------------------------------------------- */

typedef struct {
    float move_x, move_y;      /* -1..1, left stick  */
    float turn_x, turn_y;      /* -1..1, right stick */
    float trigger[GE_VR_HAND_COUNT];  /* 0..1 */
    unsigned int buttons;      /* GE_VR_BTN_* bitfield */
} GeVrPadState;

#define GE_VR_BTN_FIRE_L   (1u << 0)
#define GE_VR_BTN_FIRE_R   (1u << 1)
#define GE_VR_BTN_AIM_L    (1u << 2)
#define GE_VR_BTN_AIM_R    (1u << 3)
#define GE_VR_BTN_RELOAD   (1u << 4)
#define GE_VR_BTN_USE      (1u << 5)
#define GE_VR_BTN_WATCH    (1u << 6)
#define GE_VR_BTN_SWAP     (1u << 7)
#define GE_VR_BTN_CROUCH   (1u << 8)
#define GE_VR_BTN_PAUSE    (1u << 9)

void geVrGetPadState(GeVrPadState *out);

/* Nonzero when the player has physically raised the left wrist toward the face.
 * The watch menu is GoldenEye's ready-made diegetic HUD; this is the gesture
 * that should open it. See architecture doc §6.4. */
int geVrWatchGestureActive(void);

/* Physical crouch, derived from head height vs. calibrated standing height.
 * Returns 0..1 where 1 is fully crouched. ORed with the crouch button. */
float geVrPhysicalCrouch(void);

/* Recentre play space to the current head yaw and position. */
void geVrRecenter(void);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* GE_VR_H */
