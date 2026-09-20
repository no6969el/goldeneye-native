/*
 * ge_vr_opt.c — option registry + world-locked hub panel (GEVR #76 Phase 3–4).
 *
 * Phase 2 chrome / host / laser+trigger unchanged. Ship rows:
 *   TURN SPEED  → GETV_XR_TURN_SCALE cache (default 60)
 *   TURN STYLE  → smooth vs snap on GETV_XR_TURN (no second yaw)
 *   HEIGHT      → GETV_XR_FLOOR_M cache (default -0.200)
 * Pose is cinema-hub world, wearer-right of PLAY_SCREEN=2. Never head-locked.
 * Does not read or write HEAD_TRANSLATE / PLAYSPACE / GUNREBASE.
 * Does not flip FLOOR_INJECT or mint GETV_XR_SNAP / GE_VR_SNAP_TURN.
 */

#include "ge_vr/ge_vr_opt.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct GeVrOptRow {
    char id[GE_VR_OPT_ID_MAX];
    char label[GE_VR_OPT_LABEL_MAX];
    GeVrOptKind kind;
    float smin, smax, sstep;
    char enum_labels[GE_VR_OPT_MAX_ENUM][GE_VR_OPT_LABEL_MAX];
    int enum_count;
    float (*get_f)(void *);
    void (*set_f)(void *, float);
    int (*get_i)(void *);
    void (*set_i)(void *, int);
    void *ctx;
} GeVrOptRow;

typedef struct GeVrOptState {
    GeVrOptRow rows[GE_VR_OPT_MAX_ROWS];
    int n;

    int hub_active;
    int dismissed;
    int hover;
    int panel_hit;
    int close_hot;
    int dropdown_row; /* -1 = closed */
    int trig_was[GE_VR_HAND_COUNT];
    GeVrOptPointer last_ptr[GE_VR_HAND_COUNT];
    float last_hit[GE_VR_HAND_COUNT][3];
    int last_on_panel[GE_VR_HAND_COUNT];

    float cinema_center[3];
    float cinema_right[3];
    float cinema_up[3];
    float cinema_normal[3];
    float cinema_width;
    float cinema_height;
} GeVrOptState;

static GeVrOptState G = {.hover = -1, .dropdown_row = -1};

typedef struct GeVrPrefCache {
    int scale_latched;
    int scale;
    const char *scale_src;
    int scale_logged;
    int armed_latched;
    int armed;
    int mode_latched;
    int mode;
    int floor_latched;
    float floor;
    const char *floor_src;
    int floor_logged;
} GeVrPrefCache;

static GeVrPrefCache P;

static void ge_opt_clamp_scale(int *v)
{
    if (v == NULL) return;
    if (*v < GE_VR_TURN_SCALE_MIN) *v = GE_VR_TURN_SCALE_MIN;
    if (*v > GE_VR_TURN_SCALE_MAX) *v = GE_VR_TURN_SCALE_MAX;
}

static void ge_opt_clamp_floor(float *v)
{
    float steps;
    if (v == NULL) return;
    if (*v < GE_VR_FLOOR_M_MIN) *v = GE_VR_FLOOR_M_MIN;
    if (*v > GE_VR_FLOOR_M_MAX) *v = GE_VR_FLOOR_M_MAX;
    steps = (*v - GE_VR_FLOOR_M_MIN) / GE_VR_FLOOR_M_STEP;
    *v = GE_VR_FLOOR_M_MIN + (float)((int)(steps + (steps >= 0.0f ? 0.5f : -0.5f))) *
         GE_VR_FLOOR_M_STEP;
    if (*v < GE_VR_FLOOR_M_MIN) *v = GE_VR_FLOOR_M_MIN;
    if (*v > GE_VR_FLOOR_M_MAX) *v = GE_VR_FLOOR_M_MAX;
}

static void ge_opt_persist(void)
{
    const char *path;
    FILE *f;
    int scale;
    float floor;

    path = getenv("GETV_VR_OPT_PREFS");
    if (path == NULL || path[0] == '\0')
        path = GE_VR_OPT_PREFS_NAME;

    scale = geVrTurnScaleGet();
    floor = geVrFloorMGet();

    f = fopen(path, "w");
    if (f == NULL) return;
    fprintf(f, "rem GEVR player prefs (#76). call AFTER boot GETV_XR_TURN_SCALE=60.\n");
    fprintf(f, "set GETV_XR_TURN_SCALE=%d\n", scale);
    fprintf(f, "set GETV_XR_FLOOR_M=%.3f\n", (double)floor);
    fprintf(f, "set GETV_XR_TURN=1\n");
    fprintf(f, "rem snap/smooth is geVrTurnMode on the GETV_XR_TURN path (not GETV_XR_SNAP).\n");
    fclose(f);
}

int geVrTurnScaleGet(void)
{
    if (!P.scale_latched) {
        const char *e = getenv("GETV_XR_TURN_SCALE");
        if (e != NULL && e[0] != '\0') {
            P.scale = atoi(e);
            P.scale_src = "boot";
        } else {
            P.scale = GE_VR_TURN_SCALE_DEFAULT;
            P.scale_src = "default";
        }
        ge_opt_clamp_scale(&P.scale);
        P.scale_latched = 1;
    }
    if (!P.scale_logged) {
        printf("[getv][xrin] TURN_SCALE=%d (%s)\n", P.scale,
               P.scale_src ? P.scale_src : "default");
        P.scale_logged = 1;
    }
    return P.scale;
}

void geVrTurnScaleSet(int v)
{
    ge_opt_clamp_scale(&v);
    P.scale = v;
    P.scale_src = "prefs";
    P.scale_latched = 1;
    if (!P.scale_logged) {
        printf("[getv][xrin] TURN_SCALE=%d (prefs)\n", P.scale);
        P.scale_logged = 1;
    }
    ge_opt_persist();
}

int geVrTurnArmed(void)
{
    if (!P.armed_latched) {
        const char *e = getenv("GETV_XR_TURN");
        P.armed = (e != NULL && e[0] != '\0') ? (atoi(e) != 0) : 1;
        P.armed_latched = 1;
    }
    return P.armed ? 1 : 0;
}

int geVrTurnModeGet(void)
{
    if (!P.mode_latched) {
        P.mode = GE_VR_TURN_MODE_SMOOTH;
        P.mode_latched = 1;
    }
    return (P.mode == GE_VR_TURN_MODE_SNAP) ? GE_VR_TURN_MODE_SNAP
                                            : GE_VR_TURN_MODE_SMOOTH;
}

void geVrTurnModeSet(int mode)
{
    P.mode = (mode == GE_VR_TURN_MODE_SNAP) ? GE_VR_TURN_MODE_SNAP
                                            : GE_VR_TURN_MODE_SMOOTH;
    P.mode_latched = 1;
    /* Does not flip GETV_XR_TURN to 0 and does not mint GETV_XR_SNAP. */
}

float geVrFloorMGet(void)
{
    if (!P.floor_latched) {
        const char *e = getenv("GETV_XR_FLOOR_M");
        if (e != NULL && e[0] != '\0') {
            P.floor = (float)atof(e);
            P.floor_src = "boot";
        } else {
            P.floor = GE_VR_FLOOR_M_DEFAULT;
            P.floor_src = "default";
        }
        ge_opt_clamp_floor(&P.floor);
        P.floor_latched = 1;
    }
    if (!P.floor_logged) {
        printf("[getv][xrin] FLOOR_M=%.3f (%s)\n", (double)P.floor,
               P.floor_src ? P.floor_src : "default");
        P.floor_logged = 1;
    }
    return P.floor;
}

void geVrFloorMSet(float metres)
{
    ge_opt_clamp_floor(&metres);
    P.floor = metres;
    P.floor_src = "prefs";
    P.floor_latched = 1;
    if (!P.floor_logged) {
        printf("[getv][xrin] FLOOR_M=%.3f (prefs)\n", (double)P.floor);
        P.floor_logged = 1;
    }
    ge_opt_persist();
}

static float ship_scale_get(void *ctx)
{
    (void)ctx;
    return (float)geVrTurnScaleGet();
}

static void ship_scale_set(void *ctx, float v)
{
    (void)ctx;
    geVrTurnScaleSet((int)(v + (v >= 0.0f ? 0.5f : -0.5f)));
}

static int ship_mode_get(void *ctx)
{
    (void)ctx;
    return geVrTurnModeGet();
}

static void ship_mode_set(void *ctx, int v)
{
    (void)ctx;
    geVrTurnModeSet(v);
}

static float ship_floor_get(void *ctx)
{
    (void)ctx;
    return geVrFloorMGet();
}

static void ship_floor_set(void *ctx, float v)
{
    (void)ctx;
    geVrFloorMSet(v);
}

static int ge_opt_has_id(const char *id)
{
    int i;
    if (id == NULL) return 0;
    for (i = 0; i < G.n; ++i) {
        if (strcmp(G.rows[i].id, id) == 0) return 1;
    }
    return 0;
}

void geVrOptEnsureShipRows(void)
{
    static const char *k_turn_style[] = { "SMOOTH", "SNAP" };
    GeVrOptDesc d;

    if (!ge_opt_has_id(GE_VR_OPT_ID_TURN_SCALE)) {
        memset(&d, 0, sizeof(d));
        d.id = GE_VR_OPT_ID_TURN_SCALE;
        d.label = "TURN SPEED";
        d.kind = GE_VR_OPT_SLIDER;
        d.slider_min = (float)GE_VR_TURN_SCALE_MIN;
        d.slider_max = (float)GE_VR_TURN_SCALE_MAX;
        d.slider_step = (float)GE_VR_TURN_SCALE_STEP;
        d.get_f = ship_scale_get;
        d.set_f = ship_scale_set;
        geVrOptRegister(&d);
    }

    if (!ge_opt_has_id(GE_VR_OPT_ID_TURN_MODE)) {
        memset(&d, 0, sizeof(d));
        d.id = GE_VR_OPT_ID_TURN_MODE;
        d.label = "TURN STYLE";
        d.kind = GE_VR_OPT_ENUM;
        d.enum_labels = k_turn_style;
        d.enum_count = 2;
        d.get_i = ship_mode_get;
        d.set_i = ship_mode_set;
        geVrOptRegister(&d);
    }

    if (!ge_opt_has_id(GE_VR_OPT_ID_FLOOR_M)) {
        memset(&d, 0, sizeof(d));
        d.id = GE_VR_OPT_ID_FLOOR_M;
        d.label = "HEIGHT";
        d.kind = GE_VR_OPT_SLIDER;
        d.slider_min = GE_VR_FLOOR_M_MIN;
        d.slider_max = GE_VR_FLOOR_M_MAX;
        d.slider_step = GE_VR_FLOOR_M_STEP;
        d.get_f = ship_floor_get;
        d.set_f = ship_floor_set;
        geVrOptRegister(&d);
    }
}

static int ge_opt_ray_quad(const GeVrOptPanelQuad *q, const float origin[3],
                           const float dir[3], float *t_out, float uv[2]);

static void ge_opt_strlcpy(char *dst, const char *src, int cap)
{
    int i = 0;
    if (dst == NULL || cap <= 0) return;
    if (src == NULL) {
        dst[0] = '\0';
        return;
    }
    for (; i < cap - 1 && src[i] != '\0'; ++i) dst[i] = src[i];
    dst[i] = '\0';
}

static void ge_opt_norm3(float v[3])
{
    float n;
    if (v == NULL) return;
    n = v[0] * v[0] + v[1] * v[1] + v[2] * v[2];
    if (n < 1.0e-12f) return;
    n = 1.0f / sqrtf(n);
    v[0] *= n;
    v[1] *= n;
    v[2] *= n;
}

static void ge_opt_copy3(float dst[3], const float src[3])
{
    dst[0] = src[0];
    dst[1] = src[1];
    dst[2] = src[2];
}

static void ge_opt_default_cinema(void)
{
    /* U-19: ScreenDist 2.5 m, ScreenSize 2.6 m. Player at origin faces -Z. */
    G.cinema_center[0] = 0.0f;
    G.cinema_center[1] = GE_VR_OPT_CINEMA_CENTER_Y;
    G.cinema_center[2] = -GE_VR_OPT_CINEMA_DIST_UNITS;
    G.cinema_right[0] = 1.0f;
    G.cinema_right[1] = 0.0f;
    G.cinema_right[2] = 0.0f;
    G.cinema_up[0] = 0.0f;
    G.cinema_up[1] = 1.0f;
    G.cinema_up[2] = 0.0f;
    G.cinema_normal[0] = 0.0f;
    G.cinema_normal[1] = 0.0f;
    G.cinema_normal[2] = 1.0f;
    G.cinema_width = GE_VR_OPT_CINEMA_WIDTH_UNITS;
    G.cinema_height = GE_VR_OPT_CINEMA_HEIGHT_UNITS;
}

static int ge_vr_opt_panel(void)
{
    static int on = -1;
    if (on < 0) {
        const char *e = getenv("GETV_VR_OPT_PANEL");
        on = (e != NULL && e[0] != '\0') ? (atoi(e) != 0) : 0;
        if (on) {
            printf("[getv][optpanel] GETV_VR_OPT_PANEL=1 hub\n");
        }
    }
    return on;
}

static int ge_opt_valid_index(int index)
{
    return index >= 0 && index < G.n;
}

int geVrOptRegister(const GeVrOptDesc *desc)
{
    GeVrOptRow *r;
    int i;

    if (desc == NULL || desc->id == NULL || desc->id[0] == '\0' ||
        desc->label == NULL || desc->label[0] == '\0') {
        return -1;
    }
    if (desc->kind != GE_VR_OPT_SLIDER && desc->kind != GE_VR_OPT_TOGGLE &&
        desc->kind != GE_VR_OPT_ENUM) {
        return -1;
    }
    if (G.n >= GE_VR_OPT_MAX_ROWS) return -1;

    if (desc->kind == GE_VR_OPT_SLIDER) {
        if (!(desc->slider_step > 0.0f) || !(desc->slider_max > desc->slider_min))
            return -1;
    }
    if (desc->kind == GE_VR_OPT_ENUM) {
        if (desc->enum_count < 1 || desc->enum_count > GE_VR_OPT_MAX_ENUM ||
            desc->enum_labels == NULL) {
            return -1;
        }
        for (i = 0; i < desc->enum_count; ++i) {
            if (desc->enum_labels[i] == NULL || desc->enum_labels[i][0] == '\0')
                return -1;
        }
    }

    r = &G.rows[G.n];
    memset(r, 0, sizeof(*r));
    ge_opt_strlcpy(r->id, desc->id, GE_VR_OPT_ID_MAX);
    ge_opt_strlcpy(r->label, desc->label, GE_VR_OPT_LABEL_MAX);
    r->kind = desc->kind;
    r->smin = desc->slider_min;
    r->smax = desc->slider_max;
    r->sstep = desc->slider_step;
    r->enum_count = (desc->kind == GE_VR_OPT_ENUM) ? desc->enum_count : 0;
    for (i = 0; i < r->enum_count; ++i) {
        ge_opt_strlcpy(r->enum_labels[i], desc->enum_labels[i], GE_VR_OPT_LABEL_MAX);
    }
    r->get_f = desc->get_f;
    r->set_f = desc->set_f;
    r->get_i = desc->get_i;
    r->set_i = desc->set_i;
    r->ctx = desc->ctx;
    return G.n++;
}

int geVrOptCount(void)
{
    return G.n;
}

void geVrOptClear(void)
{
    G.n = 0;
    G.hover = -1;
    memset(G.rows, 0, sizeof(G.rows));
}

int geVrOptGetKind(int index, GeVrOptKind *out)
{
    if (!ge_opt_valid_index(index) || out == NULL) return 0;
    *out = G.rows[index].kind;
    return 1;
}

const char *geVrOptId(int index)
{
    return ge_opt_valid_index(index) ? G.rows[index].id : NULL;
}

const char *geVrOptLabel(int index)
{
    return ge_opt_valid_index(index) ? G.rows[index].label : NULL;
}

int geVrOptGetFloat(int index, float *out)
{
    if (!ge_opt_valid_index(index) || out == NULL) return 0;
    if (G.rows[index].get_f == NULL) return 0;
    *out = G.rows[index].get_f(G.rows[index].ctx);
    return 1;
}

int geVrOptGetInt(int index, int *out)
{
    if (!ge_opt_valid_index(index) || out == NULL) return 0;
    if (G.rows[index].get_i == NULL) return 0;
    *out = G.rows[index].get_i(G.rows[index].ctx);
    return 1;
}

const char *geVrOptEnumLabel(int index, int enum_index)
{
    if (!ge_opt_valid_index(index)) return NULL;
    if (G.rows[index].kind != GE_VR_OPT_ENUM) return NULL;
    if (enum_index < 0 || enum_index >= G.rows[index].enum_count) return NULL;
    return G.rows[index].enum_labels[enum_index];
}

int geVrOptNudge(int index, int dir)
{
    GeVrOptRow *r;
    if (!ge_opt_valid_index(index)) return 0;
    if (dir == 0) dir = 1;
    r = &G.rows[index];

    if (r->kind == GE_VR_OPT_SLIDER) {
        float v, nv;
        if (r->get_f == NULL || r->set_f == NULL) return 0;
        v = r->get_f(r->ctx);
        nv = v + ((dir > 0) ? r->sstep : -r->sstep);
        if (nv < r->smin) nv = r->smin;
        if (nv > r->smax) nv = r->smax;
        if (nv == v) return 0;
        r->set_f(r->ctx, nv);
        return 1;
    }

    if (r->kind == GE_VR_OPT_TOGGLE) {
        int v;
        if (r->get_i == NULL || r->set_i == NULL) return 0;
        v = r->get_i(r->ctx) ? 1 : 0;
        r->set_i(r->ctx, v ? 0 : 1);
        return 1;
    }

    if (r->kind == GE_VR_OPT_ENUM) {
        int v, n;
        if (r->get_i == NULL || r->set_i == NULL || r->enum_count <= 0) return 0;
        n = r->enum_count;
        v = r->get_i(r->ctx);
        if (dir > 0) v = (v + 1) % n;
        else v = (v - 1 + n) % n;
        r->set_i(r->ctx, v);
        return 1;
    }

    return 0;
}

int geVrOptPanelEnabled(void)
{
    return ge_vr_opt_panel();
}

void geVrOptPanelSetHubActive(int cinema_or_hub)
{
    int on = cinema_or_hub ? 1 : 0;
    if (!on) G.dismissed = 0;
    G.hub_active = on;
}

int geVrOptPanelHubActive(void)
{
    return G.hub_active;
}

int geVrOptPanelVisible(void)
{
    return (ge_vr_opt_panel() && G.hub_active && !G.dismissed) ? 1 : 0;
}

void geVrOptPanelSetCinemaFrame(const float center[3],
                                const float right[3],
                                const float up[3],
                                const float normal[3],
                                float width, float height)
{
    if (center == NULL || right == NULL || up == NULL || normal == NULL) return;
    if (!(width > 1.0f) || !(height > 1.0f)) return;
    ge_opt_copy3(G.cinema_center, center);
    ge_opt_copy3(G.cinema_right, right);
    ge_opt_copy3(G.cinema_up, up);
    ge_opt_copy3(G.cinema_normal, normal);
    ge_opt_norm3(G.cinema_right);
    ge_opt_norm3(G.cinema_up);
    ge_opt_norm3(G.cinema_normal);
    G.cinema_width = width;
    G.cinema_height = height;
}

void geVrOptPanelResetCinemaFrame(void)
{
    ge_opt_default_cinema();
}

void geVrOptPanelComputeQuad(GeVrOptPanelQuad *out)
{
    float offset;
    if (out == NULL) return;
    if (G.cinema_width <= 0.0f) ge_opt_default_cinema();

    offset = (G.cinema_width * 0.5f) + GE_VR_OPT_PANEL_GAP_UNITS +
             (GE_VR_OPT_PANEL_WIDTH_UNITS * 0.5f);

    out->center[0] = G.cinema_center[0] + G.cinema_right[0] * offset +
                     G.cinema_normal[0] * GE_VR_OPT_PANEL_FORWARD_UNITS;
    out->center[1] = G.cinema_center[1] + G.cinema_right[1] * offset +
                     G.cinema_normal[1] * GE_VR_OPT_PANEL_FORWARD_UNITS;
    out->center[2] = G.cinema_center[2] + G.cinema_right[2] * offset +
                     G.cinema_normal[2] * GE_VR_OPT_PANEL_FORWARD_UNITS;
    ge_opt_copy3(out->right, G.cinema_right);
    ge_opt_copy3(out->up, G.cinema_up);
    ge_opt_copy3(out->normal, G.cinema_normal);
    out->width = GE_VR_OPT_PANEL_WIDTH_UNITS;
    out->height = GE_VR_OPT_PANEL_HEIGHT_UNITS;
}

int geVrOptPanelGetQuad(GeVrOptPanelQuad *out)
{
    if (out == NULL || !geVrOptPanelVisible()) return 0;
    geVrOptPanelComputeQuad(out);
    return 1;
}

int geVrOptPanelRayHit(const float origin[3], const float dir[3],
                       float *t_out, float uv[2])
{
    GeVrOptPanelQuad q;
    if (origin == NULL || dir == NULL) return 0;
    geVrOptPanelComputeQuad(&q);
    return ge_opt_ray_quad(&q, origin, dir, t_out, uv);
}

int geVrOptPanelRowAtUv(const float uv[2])
{
    float body, row_h, band;
    int idx, slots;

    if (uv == NULL || G.n <= 0) return -1;
    if (uv[0] < 0.0f || uv[0] > 1.0f || uv[1] < 0.0f || uv[1] > 1.0f) return -1;
    if (geVrOptPanelCloseAtUv(uv)) return -1;

    band = 1.0f - GE_VR_OPT_HEADER_BAND;
    if (uv[1] >= band) return -1;

    slots = G.n;
    row_h = band / (float)slots;
    if (row_h < GE_VR_OPT_ROW_MIN_H) row_h = GE_VR_OPT_ROW_MIN_H;
    body = band - uv[1]; /* 0 at top of body */
    idx = (int)(body / row_h);
    if (idx < 0 || idx >= G.n) return -1;
    return idx;
}

int geVrOptPanelCloseAtUv(const float uv[2])
{
    float band;
    if (uv == NULL) return 0;
    if (uv[0] < 0.0f || uv[0] > 1.0f || uv[1] < 0.0f || uv[1] > 1.0f) return 0;
    band = 1.0f - GE_VR_OPT_HEADER_BAND;
    if (uv[1] < band) return 0;
    return (uv[0] >= (1.0f - GE_VR_OPT_CLOSE_BAND)) ? 1 : 0;
}

int geVrOptPanelGetRowRect(int index, GeVrOptRowRect *out)
{
    float band, row_h, top;
    if (out == NULL || !ge_opt_valid_index(index)) return 0;
    band = 1.0f - GE_VR_OPT_HEADER_BAND;
    row_h = band / (float)G.n;
    if (row_h < GE_VR_OPT_ROW_MIN_H) row_h = GE_VR_OPT_ROW_MIN_H;
    top = band - row_h * (float)index;
    out->index = index;
    out->uv0[0] = 0.0f;
    out->uv0[1] = top - row_h;
    out->uv1[0] = 1.0f;
    out->uv1[1] = top;
    if (out->uv0[1] < 0.0f) out->uv0[1] = 0.0f;
    return 1;
}

void geVrOptPanelGetChrome(GeVrOptChrome *out)
{
    if (out == NULL) return;
    out->glass_rgba[0] = GE_VR_OPT_CHROME_GLASS_R;
    out->glass_rgba[1] = GE_VR_OPT_CHROME_GLASS_G;
    out->glass_rgba[2] = GE_VR_OPT_CHROME_GLASS_B;
    out->glass_rgba[3] = GE_VR_OPT_CHROME_GLASS_A;
    out->text_rgb[0] = GE_VR_OPT_CHROME_TEXT_R;
    out->text_rgb[1] = GE_VR_OPT_CHROME_TEXT_G;
    out->text_rgb[2] = GE_VR_OPT_CHROME_TEXT_B;
    out->glow_rgb[0] = GE_VR_OPT_CHROME_GLOW_R;
    out->glow_rgb[1] = GE_VR_OPT_CHROME_GLOW_G;
    out->glow_rgb[2] = GE_VR_OPT_CHROME_GLOW_B;
    out->pill_rgb[0] = GE_VR_OPT_CHROME_PILL_R;
    out->pill_rgb[1] = GE_VR_OPT_CHROME_PILL_G;
    out->pill_rgb[2] = GE_VR_OPT_CHROME_PILL_B;
    out->pill_text_rgb[0] = GE_VR_OPT_CHROME_PILL_TEXT_R;
    out->pill_text_rgb[1] = GE_VR_OPT_CHROME_PILL_TEXT_G;
    out->pill_text_rgb[2] = GE_VR_OPT_CHROME_PILL_TEXT_B;
}

int geVrOptPanelDropdownOpen(void)
{
    return (G.dropdown_row >= 0) ? 1 : 0;
}

int geVrOptPanelDropdownRow(void)
{
    return G.dropdown_row;
}

void geVrOptPanelOpenDropdown(int row_index)
{
    if (!ge_opt_valid_index(row_index)) {
        G.dropdown_row = -1;
        return;
    }
    if (G.rows[row_index].kind != GE_VR_OPT_ENUM) {
        G.dropdown_row = -1;
        return;
    }
    G.dropdown_row = row_index;
}

void geVrOptPanelCloseDropdown(void)
{
    G.dropdown_row = -1;
}

int geVrOptPanelGetDropdownQuad(GeVrOptPanelQuad *out)
{
    GeVrOptPanelQuad mainq;
    float gap, items, h;
    if (out == NULL || G.dropdown_row < 0) return 0;
    geVrOptPanelComputeQuad(&mainq);
    items = (float)G.rows[G.dropdown_row].enum_count;
    if (items < 1.0f) items = 1.0f;
    h = GE_VR_OPT_ROW_MIN_H * mainq.height * items;
    if (h < 24.0f) h = 24.0f;
    if (h > mainq.height) h = mainq.height;
    gap = mainq.width * 0.5f + GE_VR_OPT_DROPDOWN_GAP_UNITS +
          GE_VR_OPT_DROPDOWN_WIDTH_UNITS * 0.5f;
    out->center[0] = mainq.center[0] + mainq.right[0] * gap;
    out->center[1] = mainq.center[1];
    out->center[2] = mainq.center[2] + mainq.right[2] * gap;
    ge_opt_copy3(out->right, mainq.right);
    ge_opt_copy3(out->up, mainq.up);
    ge_opt_copy3(out->normal, mainq.normal);
    out->width = GE_VR_OPT_DROPDOWN_WIDTH_UNITS;
    out->height = h;
    return 1;
}

int geVrOptPanelDropdownItemAtUv(const float uv[2])
{
    int n, idx;
    if (uv == NULL || G.dropdown_row < 0) return -1;
    n = G.rows[G.dropdown_row].enum_count;
    if (n <= 0) return -1;
    if (uv[0] < 0.0f || uv[0] > 1.0f || uv[1] < 0.0f || uv[1] > 1.0f) return -1;
    idx = (int)((1.0f - uv[1]) * (float)n);
    if (idx < 0) idx = 0;
    if (idx >= n) idx = n - 1;
    return idx;
}

static int ge_opt_ray_quad(const GeVrOptPanelQuad *q, const float origin[3],
                           const float dir[3], float *t_out, float uv[2])
{
    float denom, t, hx, hy, hz, dx, dy, dz, u, v, hw, hh;
    if (q == NULL || origin == NULL || dir == NULL) return 0;
    denom = dir[0] * q->normal[0] + dir[1] * q->normal[1] + dir[2] * q->normal[2];
    if (fabsf(denom) < 1.0e-6f) return 0;
    t = ((q->center[0] - origin[0]) * q->normal[0] +
         (q->center[1] - origin[1]) * q->normal[1] +
         (q->center[2] - origin[2]) * q->normal[2]) / denom;
    if (t <= 0.0f) return 0;
    hx = origin[0] + dir[0] * t;
    hy = origin[1] + dir[1] * t;
    hz = origin[2] + dir[2] * t;
    dx = hx - q->center[0];
    dy = hy - q->center[1];
    dz = hz - q->center[2];
    u = dx * q->right[0] + dy * q->right[1] + dz * q->right[2];
    v = dx * q->up[0] + dy * q->up[1] + dz * q->up[2];
    hw = q->width * 0.5f;
    hh = q->height * 0.5f;
    if (u < -hw || u > hw || v < -hh || v > hh) return 0;
    if (t_out) *t_out = t;
    if (uv) {
        uv[0] = (u + hw) / q->width;
        uv[1] = (v + hh) / q->height;
    }
    return 1;
}

static int ge_opt_poke(const float origin[3])
{
    GeVrOptPanelQuad q;
    float dx, dy, dz, u, v, w, hw, hh;
    if (origin == NULL) return 0;
    geVrOptPanelComputeQuad(&q);
    dx = origin[0] - q.center[0];
    dy = origin[1] - q.center[1];
    dz = origin[2] - q.center[2];
    u = dx * q.right[0] + dy * q.right[1] + dz * q.right[2];
    v = dx * q.up[0] + dy * q.up[1] + dz * q.up[2];
    w = dx * q.normal[0] + dy * q.normal[1] + dz * q.normal[2];
    hw = q.width * 0.5f + GE_VR_OPT_POKE_RADIUS_UNITS;
    hh = q.height * 0.5f + GE_VR_OPT_POKE_RADIUS_UNITS;
    if (u < -hw || u > hw || v < -hh || v > hh) return 0;
    if (fabsf(w) > GE_VR_OPT_POKE_RADIUS_UNITS) return 0;
    return 1;
}

int geVrOptPanelEvaluate(int first_eye, const GeVrOptPointer pointers[GE_VR_HAND_COUNT])
{
    int h, hit, poke, click, drop_hit;
    float best_t, best_uv[2], drop_uv[2];
    GeVrOptPanelQuad dropq;

    if (!geVrOptPanelVisible()) {
        G.hover = -1;
        G.panel_hit = 0;
        G.close_hot = 0;
        G.trig_was[0] = G.trig_was[1] = 0;
        memset(G.last_ptr, 0, sizeof(G.last_ptr));
        return 0;
    }
    geVrOptEnsureShipRows();
    if (!first_eye) return 1;

    hit = 0;
    poke = 0;
    drop_hit = 0;
    best_t = 1.0e9f;
    best_uv[0] = 0.5f;
    best_uv[1] = 0.5f;
    drop_uv[0] = drop_uv[1] = 0.0f;

    for (h = 0; h < GE_VR_HAND_COUNT; ++h) {
        float t = 0.0f, uv[2];
        G.last_on_panel[h] = 0;
        G.last_hit[h][0] = G.last_hit[h][1] = G.last_hit[h][2] = 0.0f;
        if (pointers == NULL || !pointers[h].tracked) {
            memset(&G.last_ptr[h], 0, sizeof(G.last_ptr[h]));
            continue;
        }
        G.last_ptr[h] = pointers[h];
        G.last_hit[h][0] = pointers[h].origin[0] +
                           pointers[h].dir[0] * GE_VR_OPT_LASER_MISS_UNITS;
        G.last_hit[h][1] = pointers[h].origin[1] +
                           pointers[h].dir[1] * GE_VR_OPT_LASER_MISS_UNITS;
        G.last_hit[h][2] = pointers[h].origin[2] +
                           pointers[h].dir[2] * GE_VR_OPT_LASER_MISS_UNITS;

        if (geVrOptPanelRayHit(pointers[h].origin, pointers[h].dir, &t, uv)) {
            G.last_on_panel[h] = 1;
            G.last_hit[h][0] = pointers[h].origin[0] + pointers[h].dir[0] * t;
            G.last_hit[h][1] = pointers[h].origin[1] + pointers[h].dir[1] * t;
            G.last_hit[h][2] = pointers[h].origin[2] + pointers[h].dir[2] * t;
            if (t > 0.0f && t < best_t) {
                best_t = t;
                best_uv[0] = uv[0];
                best_uv[1] = uv[1];
                hit = 1;
            }
        }
        if (G.dropdown_row >= 0 && geVrOptPanelGetDropdownQuad(&dropq) &&
            ge_opt_ray_quad(&dropq, pointers[h].origin, pointers[h].dir, &t,
                            uv)) {
            drop_hit = 1;
            drop_uv[0] = uv[0];
            drop_uv[1] = uv[1];
            if (!G.last_on_panel[h]) {
                G.last_hit[h][0] = pointers[h].origin[0] + pointers[h].dir[0] * t;
                G.last_hit[h][1] = pointers[h].origin[1] + pointers[h].dir[1] * t;
                G.last_hit[h][2] = pointers[h].origin[2] + pointers[h].dir[2] * t;
            }
        }
        /* Rank 2 only if the laser missed — poke is fallback highlight. */
        if (!G.last_on_panel[h] && ge_opt_poke(pointers[h].origin)) poke = 1;
    }

    G.panel_hit = (hit || poke || drop_hit) ? 1 : 0;
    G.close_hot = hit ? geVrOptPanelCloseAtUv(best_uv) : 0;
    if (hit && !G.close_hot) G.hover = geVrOptPanelRowAtUv(best_uv);
    else if (!hit && poke && G.n > 0) G.hover = 0;
    else if (!hit) G.hover = -1;

    /* Trigger rising edge only. Face A / buttons are the #32 trap — ignored. */
    click = 0;
    for (h = 0; h < GE_VR_HAND_COUNT; ++h) {
        int down = 0;
        if (pointers != NULL && pointers[h].tracked &&
            pointers[h].trigger >= GE_VR_OPT_TRIGGER_CLICK) {
            down = 1;
        }
        if (down && !G.trig_was[h]) click = 1;
        G.trig_was[h] = down;
    }

    if (click && G.close_hot) {
        geVrOptPanelDismiss();
        return 1;
    }
    if (click && drop_hit) {
        int item = geVrOptPanelDropdownItemAtUv(drop_uv);
        if (item >= 0 && G.rows[G.dropdown_row].set_i &&
            G.rows[G.dropdown_row].get_i) {
            G.rows[G.dropdown_row].set_i(G.rows[G.dropdown_row].ctx, item);
        }
        geVrOptPanelCloseDropdown();
        return 1;
    }
    if (click && G.hover >= 0) {
        if (G.rows[G.hover].kind == GE_VR_OPT_ENUM) {
            geVrOptPanelOpenDropdown(G.hover);
        } else {
            int dir = (best_uv[0] < GE_VR_OPT_CHEVRON_U * 0.5f) ? -1 : 1;
            geVrOptNudge(G.hover, dir);
            geVrOptPanelCloseDropdown();
        }
    } else if (click && !hit) {
        geVrOptPanelCloseDropdown();
    }
    return 1;
}

void geVrOptPanelTick(int first_eye)
{
    GeVrOptPointer p[GE_VR_HAND_COUNT];
    GeVrPadState pad;
    int h;

    memset(p, 0, sizeof(p));
    geVrGetPadState(&pad);
    for (h = 0; h < GE_VR_HAND_COUNT; ++h) {
        p[h].tracked = geVrHandIsTracked((GeVrHand)h);
        geVrGetAimRay((GeVrHand)h, p[h].origin, p[h].dir);
        p[h].trigger = pad.trigger[h];
    }
    geVrOptPanelEvaluate(first_eye, p);
}

int geVrOptPanelGetLaser(GeVrHand hand, GeVrOptLaser *out)
{
    int h = (int)hand;
    if (out == NULL || h < 0 || h >= GE_VR_HAND_COUNT) return 0;
    memset(out, 0, sizeof(*out));
    out->hand = h;
    if (!geVrOptPanelVisible() || !G.last_ptr[h].tracked) return 0;
    ge_opt_copy3(out->origin, G.last_ptr[h].origin);
    ge_opt_copy3(out->dir, G.last_ptr[h].dir);
    ge_opt_copy3(out->hit, G.last_hit[h]);
    out->tracked = 1;
    out->on_panel = G.last_on_panel[h];
    return 1;
}

int geVrOptPanelHoverIndex(void)
{
    return G.hover;
}

int geVrOptPanelHovered(void)
{
    return G.panel_hit;
}

int geVrOptPanelCloseHot(void)
{
    return G.close_hot;
}

void geVrOptPanelDismiss(void)
{
    G.dismissed = 1;
    G.hover = -1;
    G.panel_hit = 0;
    G.close_hot = 0;
    G.dropdown_row = -1;
}

void geVrOptReset(void)
{
    memset(&G, 0, sizeof(G));
    memset(&P, 0, sizeof(P));
    G.hover = -1;
    G.dropdown_row = -1;
    ge_opt_default_cinema();
    geVrOptEnsureShipRows();
}
