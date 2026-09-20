/*
 * Hand-apply into workshop getv/port/src/port_input.c.
 * Public tree has no port_input.c body.
 *
 * First grep:
 *   getenv("GETV_XR_TURN_SCALE")
 *   getenv("GETV_XR_TURN")
 *   getenv("GETV_XR_TURN_DEAD")
 *
 * Replace the U-04 static latch with the public cache the menu writes.
 * Do not mint GETV_XR_TURNSPEED / GETV_XR_SNAP / a second integrator.
 * Do not flip GETV_XR_TURN to 0 as the scale control.
 */

#ifdef GE_PORT_NATIVE
#include "ge_vr/ge_vr_opt.h"

/* WAS (latch trap — _putenv / menu write after first read is a no-op):
 *   static int v = -1;
 *   if (v < 0) {
 *       const char *e = getenv("GETV_XR_TURN_SCALE");
 *       v = (e != NULL && *e != '\0') ? atoi(e) : 60;
 *   }
 *   return v;
 */
static int ge_xr_turn_scale(void)
{
    return geVrTurnScaleGet(); /* default 60; setter unlatches */
}

/* GETV_XR_TURN=1 KEEP: right-stick yaw armed. Smooth vs snap is a mode
 * on this same path — geVrTurnModeGet() — not GETV_XR_SNAP. */
static int ge_xr_turn_armed(void)
{
    return geVrTurnArmed();
}

static int ge_xr_turn_snap(void)
{
    return geVrTurnModeGet() == GE_VR_TURN_MODE_SNAP;
}

/*
 * Yaw apply (same site as today's GETV_XR_TURN * TURN_SCALE):
 *   if (!ge_xr_turn_armed()) return;
 *   scale = ge_xr_turn_scale();
 *   if (ge_xr_turn_snap()) apply discrete step * scale
 *   else                   apply continuous stick * scale
 */
#endif
