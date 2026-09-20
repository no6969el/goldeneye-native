/*
 * ge_vr_opt.h — world-locked hub options panel + option registry (GEVR #76).
 *
 * Phase 2 chrome / host / laser+trigger are unchanged. Phase 3–4 ship rows:
 *   turn_scale  slider  GETV_XR_TURN_SCALE (default 60)
 *   turn_mode   enum    smooth vs snap on the existing GETV_XR_TURN path
 *   floor_m     slider  GETV_XR_FLOOR_M (default -0.200)
 *
 * Menu writes those caches (setter unlatches). Optional sidecar
 * gevr-player-prefs.cmd is meant to be `call`ed AFTER boot `=60`.
 * No second yaw. No GETV_XR_SNAP / GE_VR_SNAP_TURN. No FLOOR_INJECT flip.
 *
 * Host-agnostic C89 ABI. Workshop cinema / PLAY_SCREEN draw calls the pose
 * + hub gate + tick. Aim-ray laser + trigger select (not Face A).
 * This header does not parent to the HMD.
 *
 * GETV_VR_OPT_PANEL  unset / empty / 0 = OFF (ship)
 *                    1 = show the panel while the intro / front hub is up
 *
 * Not KEEP-ON. Do not add to gevr-*-boot.cmd until a sit PASS.
 */

#ifndef GE_VR_OPT_H
#define GE_VR_OPT_H

#include "ge_vr/ge_vr.h"

#ifdef __cplusplus
extern "C" {
#endif

#define GE_VR_OPT_MAX_ROWS     16
#define GE_VR_OPT_MAX_ENUM      8
#define GE_VR_OPT_ID_MAX       32
#define GE_VR_OPT_LABEL_MAX    48

/* U-19 cinema placeholder (game units) used until workshop sets the live quad. */
#define GE_VR_OPT_CINEMA_DIST_UNITS    250.0f
#define GE_VR_OPT_CINEMA_WIDTH_UNITS   260.0f
#define GE_VR_OPT_CINEMA_HEIGHT_UNITS  146.0f
#define GE_VR_OPT_CINEMA_CENTER_Y      160.0f

#define GE_VR_OPT_PANEL_WIDTH_UNITS    110.0f
#define GE_VR_OPT_PANEL_HEIGHT_UNITS    90.0f
#define GE_VR_OPT_PANEL_GAP_UNITS       20.0f
#define GE_VR_OPT_PANEL_FORWARD_UNITS   40.0f  /* toward player vs cinema plane */
#define GE_VR_OPT_POKE_RADIUS_UNITS     12.0f  /* TOUCHUSE fallback only */
#define GE_VR_OPT_TRIGGER_CLICK          0.55f
#define GE_VR_OPT_HEADER_BAND            0.16f
#define GE_VR_OPT_CLOSE_BAND             0.14f  /* header right = close */
#define GE_VR_OPT_ROW_MIN_H              0.14f  /* tall aim-ray hitboxes */
#define GE_VR_OPT_CHEVRON_U              0.86f  /* row: name+value LEFT, icon RIGHT */
#define GE_VR_OPT_DROPDOWN_GAP_UNITS    12.0f
#define GE_VR_OPT_DROPDOWN_WIDTH_UNITS  70.0f
#define GE_VR_OPT_LASER_MISS_UNITS     400.0f

#define GE_VR_OPT_PANEL_TITLE "VR SETTINGS"

#define GE_VR_OPT_ID_TURN_SCALE "turn_scale"
#define GE_VR_OPT_ID_TURN_MODE  "turn_mode"
#define GE_VR_OPT_ID_FLOOR_M    "floor_m"
#define GE_VR_OPT_SHIP_ROWS     3

#define GE_VR_TURN_SCALE_DEFAULT 60
#define GE_VR_TURN_SCALE_MIN     10
#define GE_VR_TURN_SCALE_MAX     150
#define GE_VR_TURN_SCALE_STEP    10

#define GE_VR_TURN_MODE_SMOOTH 0
#define GE_VR_TURN_MODE_SNAP   1

#define GE_VR_FLOOR_M_DEFAULT (-0.200f)
#define GE_VR_FLOOR_M_MIN     (-0.50f)
#define GE_VR_FLOOR_M_MAX     (0.30f)
#define GE_VR_FLOOR_M_STEP    0.05f

#define GE_VR_OPT_PREFS_NAME "gevr-player-prefs.cmd"

/* Look/feel chrome (workshop blit). Not a row dump — do not copy Hand/6DoF/Haptic. */
#define GE_VR_OPT_CHROME_GLASS_R 0.06f
#define GE_VR_OPT_CHROME_GLASS_G 0.07f
#define GE_VR_OPT_CHROME_GLASS_B 0.10f
#define GE_VR_OPT_CHROME_GLASS_A 0.72f
#define GE_VR_OPT_CHROME_TEXT_R  1.00f
#define GE_VR_OPT_CHROME_TEXT_G  1.00f
#define GE_VR_OPT_CHROME_TEXT_B  1.00f
#define GE_VR_OPT_CHROME_GLOW_R  0.75f
#define GE_VR_OPT_CHROME_GLOW_G  0.92f
#define GE_VR_OPT_CHROME_GLOW_B  1.00f
#define GE_VR_OPT_CHROME_PILL_R  1.00f
#define GE_VR_OPT_CHROME_PILL_G  1.00f
#define GE_VR_OPT_CHROME_PILL_B  1.00f
#define GE_VR_OPT_CHROME_PILL_TEXT_R 0.08f
#define GE_VR_OPT_CHROME_PILL_TEXT_G 0.09f
#define GE_VR_OPT_CHROME_PILL_TEXT_B 0.12f

typedef enum {
    GE_VR_OPT_SLIDER = 0,
    GE_VR_OPT_TOGGLE = 1,
    GE_VR_OPT_ENUM   = 2
} GeVrOptKind;

typedef struct GeVrOptDesc {
    const char *id;     /* stable key, e.g. "turn_scale" */
    const char *label;  /* HUD text */
    GeVrOptKind kind;
    float slider_min, slider_max, slider_step;
    const char *const *enum_labels;
    int enum_count;
    /* Host owns persist. Ship rows write the existing TURN_SCALE / TURN / FLOOR_M caches. */
    float (*get_f)(void *ctx);
    void  (*set_f)(void *ctx, float v);
    int   (*get_i)(void *ctx);
    void  (*set_i)(void *ctx, int v);
    void *ctx;
} GeVrOptDesc;

/* Returns row index, or -1. Reset / Tick / Evaluate register the 3 ship rows. */
int geVrOptRegister(const GeVrOptDesc *desc);
int geVrOptCount(void);
void geVrOptClear(void);
void geVrOptEnsureShipRows(void);

/*
 * Existing GETV_XR_TURN_SCALE cache (default 60). First get latches getenv.
 * Setter unlatches the U-04 trap — do not _putenv after first read.
 * Workshop port_input.c must call Get, not a second static.
 */
int  geVrTurnScaleGet(void);
void geVrTurnScaleSet(int v);

/* GETV_XR_TURN: 1 = right-stick yaw armed (KEEP). Menu does not flip this to 0. */
int geVrTurnArmed(void);

/*
 * Smooth vs snap on the existing GETV_XR_TURN integrator. Not a second yaw
 * and not GETV_XR_SNAP / GE_VR_SNAP_TURN. Default SMOOTH.
 */
int  geVrTurnModeGet(void);
void geVrTurnModeSet(int mode);

/* Existing GETV_XR_FLOOR_M cache (default -0.200). Does not flip FLOOR_INJECT. */
float geVrFloorMGet(void);
void  geVrFloorMSet(float metres);

int geVrOptGetKind(int index, GeVrOptKind *out);
const char *geVrOptId(int index);
const char *geVrOptLabel(int index);
int geVrOptGetFloat(int index, float *out);
int geVrOptGetInt(int index, int *out);
const char *geVrOptEnumLabel(int index, int enum_index);

/* ±1 slider/enum step, or flip a toggle. 1 if a setter ran. */
int geVrOptNudge(int index, int dir);

/* GETV_VR_OPT_PANEL. Latched on first read. Default OFF. */
int geVrOptPanelEnabled(void);

/*
 * Workshop cinema / AUTOSCREEN path.
 *
 * cinema_or_hub:
 *   0 = mission / gameplay stereo eyes (panel gone)
 *   1 = frontend / intro / file-select
 *   2 = GETV_XR_PLAY_SCREEN=2 ship world-lock cinema — hub ON (do not drop 2)
 *   other nonzero = hub (legacy)
 *
 * Chair bug: a caller that only treats SCREEN==1 (or ignores 2) hides the
 * glass on vr442 boot (KEEP PLAY_SCREEN=2). Prefer ApplyHubGate.
 */
void geVrOptPanelSetHubActive(int cinema_or_hub);
int  geVrOptPanelHubActive(void);

/* Ship KEEP when unset. 2 = world-lock cinema. */
int geVrOptPanelPlayScreenEnv(void);

/*
 * 1 for intro / frontend / file-select / cinema, including PLAY_SCREEN=2.
 * 0 once gameplay stereo eyes exist (same gate as cinema → VR).
 * play_screen==2 is cinema even if the frontend flag was dropped.
 */
int geVrOptPanelHubShouldBeActive(int frontend_or_intro, int play_screen,
                                  int gameplay_stereo_eyes);

/* play_screen < 0 → PlayScreenEnv (default 2). Then SetHubActive. */
void geVrOptPanelApplyHubGate(int frontend_or_intro, int play_screen,
                              int gameplay_stereo_eyes);

/* Enabled AND hub active AND not dismissed. Gone in a mission even if ON. */
int geVrOptPanelVisible(void);

/*
 * Live cinema quad in hub world (game units). right = wearer-right when facing
 * the billboard. Workshop overwrites the U-19 placeholder from PLAY_SCREEN=2.
 */
void geVrOptPanelSetCinemaFrame(const float center[3],
                                const float right[3],
                                const float up[3],
                                const float normal[3],
                                float width, float height);

void geVrOptPanelResetCinemaFrame(void);

typedef struct GeVrOptPanelQuad {
    float center[3];
    float right[3];
    float up[3];
    float normal[3]; /* toward the player */
    float width, height;
} GeVrOptPanelQuad;

/* Pose only — never multiplied by head. */
void geVrOptPanelComputeQuad(GeVrOptPanelQuad *out);

/* 0 if the panel is not visible this frame. */
int geVrOptPanelGetQuad(GeVrOptPanelQuad *out);

/* Ray vs the options plane only (cinema misses). uv 0..1, origin at bottom-left. */
int geVrOptPanelRayHit(const float origin[3], const float dir[3],
                       float *t, float uv[2]);

/* -1 if the registry is empty or uv is in the header / close. */
int geVrOptPanelRowAtUv(const float uv[2]);
int geVrOptPanelCloseAtUv(const float uv[2]);

typedef struct GeVrOptRowRect {
    float uv0[2];
    float uv1[2];
    int index;
} GeVrOptRowRect;

/* Tall body slot for row index. 0 if empty shell / bad index. */
int geVrOptPanelGetRowRect(int index, GeVrOptRowRect *out);

typedef struct GeVrOptChrome {
    float glass_rgba[4];
    float text_rgb[3];
    float glow_rgb[3];
    float pill_rgb[3];
    float pill_text_rgb[3];
} GeVrOptChrome;

void geVrOptPanelGetChrome(GeVrOptChrome *out);

/*
 * World-locked dark glass for the workshop gevr_xr hub / quad-layer blit
 * (same present family as PLAY_SCREEN=2 — not a head-locked HUD).
 * 1 if the panel is visible this frame.
 */
typedef struct GeVrOptPanelGlassLayer {
    GeVrOptPanelQuad quad;
    GeVrOptChrome chrome;
    int world_locked; /* always 1 */
    int visible;
} GeVrOptPanelGlassLayer;

int geVrOptPanelGetGlassLayer(GeVrOptPanelGlassLayer *out);

/* Enum dropdown: smaller sibling to the RIGHT of the main glass. Never overlaps. */
int geVrOptPanelDropdownOpen(void);
int geVrOptPanelDropdownRow(void);
void geVrOptPanelOpenDropdown(int row_index);
void geVrOptPanelCloseDropdown(void);
int geVrOptPanelGetDropdownQuad(GeVrOptPanelQuad *out);
int geVrOptPanelDropdownItemAtUv(const float uv[2]);

typedef struct GeVrOptPointer {
    float origin[3];
    float dir[3];
    float trigger; /* 0..1 — commit. Face A is the #32 trap. */
    int tracked;
} GeVrOptPointer;

typedef struct GeVrOptLaser {
    float origin[3];
    float dir[3];
    float hit[3];   /* panel hit, else origin + dir * miss reach */
    int tracked;
    int on_panel;   /* Rank 1 aim-ray vs the glass */
    int hand;
} GeVrOptLaser;

/*
 * Rank 1: aim-ray laser (geVrGetAimRay family) + trigger commit.
 * Rank 2: TOUCHUSE poke highlights only — not the select path.
 * First eye only. Does not read Face A. Does not steal the right stick.
 */
int geVrOptPanelEvaluate(int first_eye, const GeVrOptPointer pointers[GE_VR_HAND_COUNT]);

/* Workshop: geVrGetAimRay + pad trigger → Evaluate. Then draw GetLaser beams. */
void geVrOptPanelTick(int first_eye);

int geVrOptPanelGetLaser(GeVrHand hand, GeVrOptLaser *out);

int geVrOptPanelHoverIndex(void);
int geVrOptPanelHovered(void);
int geVrOptPanelCloseHot(void);
void geVrOptPanelDismiss(void);

/* Tests: clear rows + hover + hub + pref caches. Does not unlatch GETV_VR_OPT_PANEL. */
void geVrOptReset(void);

#ifdef __cplusplus
}
#endif

#endif /* GE_VR_OPT_H */
