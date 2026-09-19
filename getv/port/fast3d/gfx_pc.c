/* Reference port fragment: bool KEEP gates (explosion path, stereo, SrcFbo). */
#include <stdlib.h>

static int ge_vr_texinval(void)
{
    static int on = -1;
    if (on < 0) {
        const char *e = getenv("GETV_VR_TEXINVAL");
        on = (e != NULL && *e != '\0') ? (atoi(e) != 0) : 1 /* ship default ON; dig sets 0 */;
    }
    return on;
}

static int ge_vr_texdlretag(void)
{
    static int on = -1;
    if (on < 0) {
        const char *e = getenv("GETV_VR_TEXDLRETAG");
        on = (e != NULL && *e != '\0') ? (atoi(e) != 0) : 1 /* ship default ON; dig sets 0 */;
    }
    return on;
}

static int ge_vr_vfxtmem(void)
{
    static int on = -1;
    if (on < 0) {
        const char *e = getenv("GETV_VR_VFXTMEM");
        on = (e != NULL && *e != '\0') ? (atoi(e) != 0) : 1 /* ship default ON; dig sets 0 */;
    }
    return on;
}

static int ge_vr_vfxshift(void)
{
    static int on = -1;
    if (on < 0) {
        const char *e = getenv("GETV_VR_VFXSHIFT");
        on = (e != NULL && *e != '\0') ? (atoi(e) != 0) : 1 /* ship default ON; dig sets 0 */;
    }
    return on;
}

static int ge_tex16be(void)
{
    static int on = -1;
    if (on < 0) {
        const char *e = getenv("GETV_TEX16BE");
        on = (e != NULL && *e != '\0') ? (atoi(e) != 0) : 1 /* ship default ON; dig sets 0 */;
    }
    return on;
}

static int ge_tex32be(void)
{
    static int on = -1;
    if (on < 0) {
        const char *e = getenv("GETV_TEX32BE");
        on = (e != NULL && *e != '\0') ? (atoi(e) != 0) : 0;
    }
    return on;
}

static int ge_vr_corpsekeep(void)
{
    static int on = -1;
    if (on < 0) {
        const char *e = getenv("GETV_VR_CORPSEKEEP");
        on = (e != NULL && *e != '\0') ? (atoi(e) != 0) : 1 /* ship default ON; dig sets 0 */;
    }
    return on;
}

static int ge_stereo_rebuild(void)
{
    static int on = -1;
    if (on < 0) {
        const char *e = getenv("GETV_STEREO_REBUILD");
        on = (e != NULL && *e != '\0') ? (atoi(e) != 0) : 1 /* ship default ON; dig sets 0 */;
    }
    return on;
}

static int ge_stereo_hudgate(void)
{
    static int on = -1;
    if (on < 0) {
        const char *e = getenv("GETV_STEREO_HUDGATE");
        on = (e != NULL && *e != '\0') ? (atoi(e) != 0) : 1 /* ship default ON; dig sets 0 */;
    }
    return on;
}

static int ge_xr_play_srcfbo(void)
{
    static int on = -1;
    if (on < 0) {
        const char *e = getenv("GETV_XR_PLAY_SRCFBO");
        on = (e != NULL && *e != '\0') ? (atoi(e) != 0) : 1 /* ship default ON; dig sets 0 */;
    }
    return on;
}

static int ge_vr_drawall(void)
{
    static int on = -1;
    if (on < 0) {
        const char *e = getenv("GETV_VR_DRAWALL");
        on = (e != NULL && *e != '\0') ? (atoi(e) != 0) : 1 /* ship default ON; dig sets 0 */;
    }
    return on;
}

static int ge_vr_skymesh(void)
{
    static int on = -1;
    if (on < 0) {
        const char *e = getenv("GETV_VR_SKYMESH");
        on = (e != NULL && *e != '\0') ? (atoi(e) != 0) : 0;
    }
    return on;
}

static int ge_vr(void)
{
    static int on = -1;
    if (on < 0) {
        const char *e = getenv("GETV_VR");
        on = (e != NULL && *e != '\0') ? (atoi(e) != 0) : 1 /* ship default ON; dig sets 0 */;
    }
    return on;
}

static int ge_xr_play(void)
{
    static int on = -1;
    if (on < 0) {
        const char *e = getenv("GETV_XR_PLAY");
        on = (e != NULL && *e != '\0') ? (atoi(e) != 0) : 1 /* ship default ON; dig sets 0 */;
    }
    return on;
}
