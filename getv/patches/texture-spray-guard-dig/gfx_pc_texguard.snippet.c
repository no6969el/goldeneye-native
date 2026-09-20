/*
 * APPLY READY sketch — workshop getv/port/fast3d/gfx_pc.c only.
 * DIG ONLY on this public tree. Do not land until the class chair (§6 RESULT).
 *
 * Site: next to ge_vr_texinval() / ge_vr_texdlretag().
 * First grep (workshop): gfx_texture_cache_lookup, import_texture_rgba16,
 * gfx_dp_set_texture_image, geStereoEyeViewport / eye begin-end.
 *
 * GETV_VR_TEXGUARD unset/empty/0 = OFF (retail shared cache).
 * =1 → per-eye snapshot + eye-boundary unbind/scissor + deferred inval.
 * Banner once: [getv][texguard] GETV_VR_TEXGUARD=1
 *
 * GETV_VR_SCRAPDROP unset/empty/0 = OFF.
 * =1 → discard NaN / Inf / sat / already-converted tris (VERTS).
 * Banner once: [getv][scrapdrop] GETV_VR_SCRAPDROP=1
 * Two knobs. Do not pack scrapdrop into TEXGUARD=2.
 *
 * KEEP ON: TEXINVAL / TEXDLRETAG / VFXTMEM / VFXSHIFT / TEX16BE.
 * Explosion texSelect arg2==4 still invals on the sim tick.
 * Do not wrap propobj.c cmdlists (that is MONFRAME — REJECT).
 * Do not Dam MTXGUARD=2. HT0 + SKYMESH=0 stay.
 * Do not sit GETV_VR_VTXGUARD=0 (heap poison, not this filter).
 */

#ifdef GE_PORT_NATIVE
#include <stdlib.h>

static int ge_vr_texguard(void)
{
    static int on = -1;
    if (on < 0) {
        const char *e = getenv("GETV_VR_TEXGUARD");
        on = (e != NULL && *e != '\0') ? (atoi(e) != 0) : 0; /* chair A/B; not KEEP-ON */
        if (on) {
            osSyncPrintf("[getv][texguard] GETV_VR_TEXGUARD=1\n");
        }
    }
    return on;
}

static int ge_vr_scrapdrop(void)
{
    static int on = -1;
    if (on < 0) {
        const char *e = getenv("GETV_VR_SCRAPDROP");
        on = (e != NULL && *e != '\0') ? (atoi(e) != 0) : 0; /* chair A/B; not KEEP-ON */
        if (on) {
            osSyncPrintf("[getv][scrapdrop] GETV_VR_SCRAPDROP=1\n");
        }
    }
    return on;
}

/* Optional subset — skip inval on monitor texSelect only (arg2 1/2, arg3 8). */
static int ge_vr_moninval(void)
{
    static int on = -1;
    if (on < 0) {
        const char *e = getenv("GETV_VR_MONINVAL");
        on = (e != NULL && *e != '\0') ? (atoi(e) != 0) : 0; /* chair A/B; not KEEP-ON */
        if (on) {
            osSyncPrintf("[getv][moninval] GETV_VR_MONINVAL=1\n");
        }
    }
    return on;
}

/*
 * Call from the TEXINVAL evict site (workshop; body not on this remote):
 *
 *   int should_inval = ge_vr_texinval();
 *   if (ge_vr_texguard()) {
 *       // Once per sim tick, not per eye. Mode 4 (explosions) still invals.
 *       if (!gePortSimShouldTick() && texselect_arg2 != 4)
 *           should_inval = 0;
 *   }
 *   if (ge_vr_moninval() && (texselect_arg2 == 1 || texselect_arg2 == 2)
 *       && texselect_arg3 == 8)
 *       should_inval = 0;
 *
 * Eye begin (LEFT then RIGHT):
 *   snapshot current texture id + tile + combiner + scissor
 *   set scissor to this eye's rect (HMD half / flat 320)
 *
 * Eye end:
 *   unbind current GL texture (no leftover tile)
 *   restore scissor; do not leave the last texrect live
 *
 * Cache lookup when TEXGUARD=1:
 *   include geVrCurrentEye() in the hashmap key  OR
 *   restore the snapshot so eye 1 cannot evict eye 0 names
 *   while SrcFbo still presents them.
 *
 * SCRAPDROP=1 at gfx_sp_tri1 / gfx_draw_rectangle (not propobj.c):
 *   drop if any clip/screen is NaN or Inf
 *   drop if already-converted (huge |clip| / |screen|, or w<=0 filled)
 *   drop if all verts are outside ~8× the current eye viewport
 *   do NOT drop tiny corner AABB (HUD 5x12 / Dam vista)
 *   do NOT drop large s/t (monitor MONVERTSCROLL is legal)
 */
#endif
