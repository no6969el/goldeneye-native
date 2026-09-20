/*
 * ge_vr_opt.h — world-locked hub options panel + option registry (GEVR #76).
 *
 * Phase 2: empty panel + registry shell. No ship rows. Phase 3 may register
 * GETV_XR_TURN_SCALE as the first slider. Later rows only if Owner green-lights
 * them; see getv/patches/vr-options-panel-apply/SUGGESTED-ROWS.md.
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
    /* Host owns persist. Phase 3 wires existing getenv caches here. */
    float (*get_f)(void *ctx);
    void  (*set_f)(void *ctx, float v);
    int   (*get_i)(void *ctx);
    void  (*set_i)(void *ctx, int v);
    void *ctx;
} GeVrOptDesc;

/* Returns row index, or -1. Phase 2 ships with zero registered rows. */
int geVrOptRegister(const GeVrOptDesc *desc);
int geVrOptCount(void);
void geVrOptClear(void);

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
 * Workshop cinema / AUTOSCREEN path: 1 while frontend / intro / file-select
 * cinema is up. 0 in a mission (same gate as cinema → gameplay VR).
 */
void geVrOptPanelSetHubActive(int cinema_or_hub);
int  geVrOptPanelHubActive(void);

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

/* Tests: clear rows + hover + hub. Does not unlatch GETV_VR_OPT_PANEL. */
void geVrOptReset(void);

#ifdef __cplusplus
}
#endif

#endif /* GE_VR_OPT_H */
