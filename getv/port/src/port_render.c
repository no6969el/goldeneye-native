/* Reference port fragment: int KEEP gates, ads/play path, crate fog defaults. */
#include <stdlib.h>
#include <string.h>

static int ge_supersample(void)
{
    static int ss = -1;
    if (ss < 0) {
        const char *e = getenv("GETV_SUPERSAMPLE");
        ss = (e != NULL && *e != '\0') ? atoi(e) : 3 /* ship default ON; dig sets 0 */;
    }
    return ss;
}

static int ge_vr_vtxguard_kb(void)
{
    static int kb = -1;
    if (kb < 0) {
        const char *e = getenv("GETV_VR_VTXGUARD");
        kb = (e != NULL && *e != '\0') ? atoi(e) : 0;
    }
    return kb;
}

static int ge_vr_hitsnap(void)
{
    static int v = -1;
    if (v < 0) {
        const char *e = getenv("GETV_VR_HITSNAP");
        v = (e != NULL && *e != '\0') ? atoi(e) : 2 /* ship default ON; dig sets 0 */;
    }
    return v;
}

static int ge_vr_corpsekeep_max(void)
{
    static int v = -1;
    if (v < 0) {
        const char *e = getenv("GETV_VR_CORPSEKEEP_MAX");
        v = (e != NULL && *e != '\0') ? atoi(e) : 48 /* ship default ON; dig sets 0 */;
    }
    return v;
}

static int ge_vr_corpsekeep_ceil(void)
{
    static int v = -1;
    if (v < 0) {
        const char *e = getenv("GETV_VR_CORPSEKEEP_CEIL");
        v = (e != NULL && *e != '\0') ? atoi(e) : 440 /* ship default ON; dig sets 0 */;
    }
    return v;
}

static int ge_vr_playspace(void)
{
    static int v = -1;
    if (v < 0) {
        const char *e = getenv("GETV_VR_PLAYSPACE");
        v = (e != NULL && *e != '\0') ? atoi(e) : 0;
    }
    return v;
}

static int ge_vr_adssight(void)
{
    static int on = -1;
    if (on < 0) {
        const char *e = getenv("GETV_VR_ADSSIGHT");
        on = (e != NULL && *e != '\0') ? (atoi(e) != 0) : 1 /* ship default ON; dig sets 0 */;
    }
    return on;
}

static int ge_vr_adscull(void)
{
    static int on = -1;
    if (on < 0) {
        const char *e = getenv("GETV_VR_ADSCULL");
        on = (e != NULL && *e != '\0') ? (atoi(e) != 0) : 1 /* ship default ON; dig sets 0 */;
    }
    return on;
}

static int ge_xr_head_translate(void)
{
    static int on = -1;
    if (on < 0) {
        const char *e = getenv("GETV_XR_HEAD_TRANSLATE");
        on = (e != NULL && *e != '\0') ? (atoi(e) != 0) : 1 /* ship default ON; dig sets 0 */;
    }
    return on;
}

static int ge_xr_play_autorecenter(void)
{
    static int on = -1;
    if (on < 0) {
        const char *e = getenv("GETV_XR_PLAY_AUTORECENTER");
        on = (e != NULL && *e != '\0') ? (atoi(e) != 0) : 1 /* ship default ON; dig sets 0 */;
    }
    return on;
}

static int ge_vr_gunaim(void)
{
    static int on = -1;
    if (on < 0) {
        const char *e = getenv("GETV_VR_GUNAIM");
        on = (e != NULL && *e != '\0') ? (atoi(e) != 0) : 1 /* ship default ON; dig sets 0 */;
    }
    return on;
}

static int ge_vr_gunmount(void)
{
    static int on = -1;
    if (on < 0) {
        const char *e = getenv("GETV_VR_GUNMOUNT");
        on = (e != NULL && *e != '\0') ? (atoi(e) != 0) : 1 /* ship default ON; dig sets 0 */;
    }
    return on;
}

/* GEVR #74 Rank 1. Unset / empty / 0 = OFF (ship). Chair-only until PASS.
 * Do not KEEP-ON. Do not add to gevr-*-boot.cmd. Live rebase is in
 * src/ge_vr_bridge.cpp (geVrGetWeaponModelMatrixF / geVrGetAimRay).
 * Workshop follow-up: same getenv next to geStereoXrGunMount / HandWorld. */
static int ge_vr_gunrebase(void)
{
    static int on = -1;
    if (on < 0) {
        const char *e = getenv("GETV_VR_GUNREBASE");
        on = (e != NULL && *e != '\0') ? (atoi(e) != 0) : 0; /* default OFF */
    }
    return on;
}

/* GEVR #76. Unset / empty / 0 = OFF (ship). Chair-only until PASS.
 * Do not KEEP-ON. Do not add to gevr-*-boot.cmd. Live panel + caches are
 * include/ge_vr/ge_vr_opt.h (geVrTurnScaleGet/Set, geVrFloorMGet/Set).
 * Workshop: same getenv next to PLAY_SCREEN draw / port_input.c yaw.
 * Does not touch HEAD_TRANSLATE / PLAYSPACE / GUNREBASE. */
static int ge_vr_opt_panel(void)
{
    static int on = -1;
    if (on < 0) {
        const char *e = getenv("GETV_VR_OPT_PANEL");
        on = (e != NULL && e[0] != '\0') ? (atoi(e) != 0) : 0; /* default OFF */
    }
    return on;
}

/* Reference only. Live cache+setter is geVrTurnScaleGet/Set — do not copy
 * this latch into port_input.c (U-04). Default 60. */
static int ge_xr_turn_scale(void)
{
    static int v = -1;
    if (v < 0) {
        const char *e = getenv("GETV_XR_TURN_SCALE");
        v = (e != NULL && e[0] != '\0') ? atoi(e) : 60;
    }
    return v;
}

/* Reference only. Live cache+setter is geVrFloorMGet/Set. Default -0.200.
 * Do not KEEP-ON FLOOR_INJECT from the menu. */
static float ge_xr_floor_m(void)
{
    static int on = -1;
    static float v = -0.200f;
    if (on < 0) {
        const char *e = getenv("GETV_XR_FLOOR_M");
        v = (e != NULL && e[0] != '\0') ? (float)atof(e) : -0.200f;
        on = 1;
    }
    return v;
}

/* Dam crates #29: ship opaque prop fog + occlusion skip ON when unset. */
static int ge_vr_propfogalpha(void)
{
    static int v = -1;
    if (v < 0) {
        const char *e = getenv("GETV_VR_PROPFOGALPHA");
        v = (e != NULL && *e != '\0') ? atoi(e) : 0 /* ship default ON; dig sets 0 */;
    }
    return v;
}

static int ge_vr_occlskip(void)
{
    static int v = -1;
    if (v < 0) {
        const char *e = getenv("GETV_VR_OCCLSKIP");
        v = (e != NULL && *e != '\0') ? atoi(e) : 1 /* ship default ON; dig sets 0 */;
    }
    return v;
}

static const char *ge_stereo_src(void)
{
    static const char *cached = NULL;
    if (cached == NULL) {
        const char *e = getenv("GETV_STEREO_SRC");
        if (e == NULL || *e == '\0') {
            cached = "xr" /* ship default ON; dig sets 0 */;
        } else {
            cached = e;
        }
    }
    return cached;
}
