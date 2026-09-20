/*
 * Hand-apply into workshop vendor/ge-decomp/src/game/propobj.c
 * if 001-propobj-monframe.patch hunks fail on GEVR drift.
 *
 * Site: process_monitor_animation_microcode (PROPDEF_MONITOR / MULTI_MONITOR
 * draw in sub_GAME_7F04AC20 already calls this).
 *
 * First grep: gePortSimShouldTick / GETV_VR_ONESHOT. Retarget the extern
 * if the live helper name differs.
 *
 * GETV_VR_MONFRAME unset/empty/0 = OFF (retail per-eye tick).
 * =1 → once per sim frame. Banner once: [getv][monframe] GETV_VR_MONFRAME=1
 *
 * Do not flip TEXINVAL / TEXDLRETAG / VFXTMEM / VFXSHIFT.
 * Do not merge #55.
 */

#ifdef GE_PORT_NATIVE
#include <stdlib.h>
extern int gePortSimShouldTick(void);

static int ge_vr_monframe(void)
{
    static int on = -1;
    if (on < 0) {
        const char *e = getenv("GETV_VR_MONFRAME");
        on = (e != NULL && *e != '\0') ? (atoi(e) != 0) : 0; /* chair A/B; not KEEP-ON */
        if (on) {
            osSyncPrintf("[getv][monframe] GETV_VR_MONFRAME=1\n");
        }
    }
    return on;
}

static int ge_vr_monframe_should_tick(void)
{
    /* Same gate as GETV_VR_ONESHOT: sim-owner / first eye only. */
    return gePortSimShouldTick();
}
#endif

/*
 * Inside process_monitor_animation_microcode, after `bool yielding = FALSE;`
 * and before `while (!yielding)`, wrap the cmdlist loop AND the
 * xscale/yscale/xmid/ymid/col increment blocks (everything that writes
 * MonitorRecord). Leave vertex setup + texSelect + triangles outside.
 *
 *     bool yielding = FALSE;
 * #ifdef GE_PORT_NATIVE
 *     if (!ge_vr_monframe() || ge_vr_monframe_should_tick())
 *     {
 * #endif
 *     while (!yielding) { ... }
 *     // Increment X scale ... Increment colour change
 * #ifdef GE_PORT_NATIVE
 *     }
 * #endif
 *     // Set up everything for rendering
 */
