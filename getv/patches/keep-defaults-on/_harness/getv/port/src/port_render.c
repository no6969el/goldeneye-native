/* Harness: int KEEP gates and stereo/play bools. */
static int ge_supersample(void)
{
    static int ss = -1;
    if (ss < 0) {
        const char *e = getenv("GETV_SUPERSAMPLE");
        ss = (e != NULL && *e != '\0') ? atoi(e) : 0;
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
        v = (e != NULL && *e != '\0') ? atoi(e) : 0;
    }
    return v;
}

static int ge_vr_adssight(void)
{
    static int on = -1;
    if (on < 0) {
        const char *e = getenv("GETV_VR_ADSSIGHT");
        on = (e != NULL && *e != '\0') ? (atoi(e) != 0) : 0;
    }
    return on;
}

static int ge_vr_adscull(void)
{
    static int on = -1;
    if (on < 0) {
        const char *e = getenv("GETV_VR_ADSCULL");
        on = (e != NULL && *e != '\0') ? (atoi(e) != 0) : 0;
    }
    return on;
}

static int ge_xr_head_translate(void)
{
    static int on = -1;
    if (on < 0) {
        const char *e = getenv("GETV_XR_HEAD_TRANSLATE");
        on = (e != NULL && *e != '\0') ? (atoi(e) != 0) : 0;
    }
    return on;
}

static int ge_xr_play_autorecenter(void)
{
    static int on = -1;
    if (on < 0) {
        const char *e = getenv("GETV_XR_PLAY_AUTORECENTER");
        on = (e != NULL && *e != '\0') ? (atoi(e) != 0) : 0;
    }
    return on;
}
