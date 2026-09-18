/* Harness fragment: typical bool KEEP gate (gfx / image path). */
static int ge_vr_texinval(void)
{
    static int on = -1;
    if (on < 0) {
        const char *e = getenv("GETV_VR_TEXINVAL");
        on = (e != NULL && *e != '\0') ? (atoi(e) != 0) : 0;
    }
    return on;
}

static int ge_vr_texdlretag(void)
{
    static int on = -1;
    if (on < 0) {
        const char *e = getenv("GETV_VR_TEXDLRETAG");
        on = (e != NULL && *e != '\0') ? (atoi(e) != 0) : 0;
    }
    return on;
}

static int ge_vr_vfxtmem(void)
{
    static int on = -1;
    if (on < 0) {
        const char *e = getenv("GETV_VR_VFXTMEM");
        on = (e != NULL && *e != '\0') ? (atoi(e) != 0) : 0;
    }
    return on;
}

static int ge_vr_corpsekeep(void)
{
    static int on = -1;
    if (on < 0) {
        const char *e = getenv("GETV_VR_CORPSEKEEP");
        on = (e != NULL && *e != '\0') ? (atoi(e) != 0) : 0;
    }
    return on;
}

static int ge_stereo_rebuild(void)
{
    static int on = -1;
    if (on < 0) {
        const char *e = getenv("GETV_STEREO_REBUILD");
        on = (e != NULL && *e != '\0') ? (atoi(e) != 0) : 0;
    }
    return on;
}

static int ge_stereo_hudgate(void)
{
    static int on = -1;
    if (on < 0) {
        const char *e = getenv("GETV_STEREO_HUDGATE");
        on = (e != NULL && *e != '\0') ? (atoi(e) != 0) : 0;
    }
    return on;
}

static int ge_xr_play_srcfbo(void)
{
    static int on = -1;
    if (on < 0) {
        const char *e = getenv("GETV_XR_PLAY_SRCFBO");
        on = (e != NULL && *e != '\0') ? (atoi(e) != 0) : 0;
    }
    return on;
}

static int ge_vr_drawall(void)
{
    static int on = -1;
    if (on < 0) {
        const char *e = getenv("GETV_VR_DRAWALL");
        on = (e != NULL && *e != '\0') ? (atoi(e) != 0) : 0;
    }
    return on;
}
