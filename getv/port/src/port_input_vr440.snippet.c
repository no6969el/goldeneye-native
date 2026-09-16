/*
 * vr440 Beta — XR B-button default (merge into getv/port/src/port_input.c).
 *
 * vr439 shipped geXrActBtnB() with geXrParseAct(..., GE_XRACT_WEAPON) (enum value 2).
 * vr440 defaults to GE_XRACT_START (5) so B/Y opens pause/options in headset.
 * GETV_XR_BTN_B=weapon restores the old mapping.
 *
 * geXrPadAct() already maps GE_XRACT_START to out->start (CONT_START / pause path).
 */

/* --- comment block near GETV_XR_BUTTONS / [getv][xrin] banner --- */
/*
 * GETV_XR_BUTTONS=1 on the walk hand:
 *   A  -> use / reload
 *   B  -> start / pause  (GETV_XR_BTN_B=weapon for vr439 weapon-on-B)
 *   squeeze -> aim, trigger -> fire
 *   both thumbstick clicks -> recenter (unchanged)
 */

static s32 geXrActBtnB(void)
{
    if (ge_xr_act_btn_b < 0) {
        const char *e = getenv("GETV_XR_BTN_B");
        ge_xr_act_btn_b = geXrParseAct(e, GE_XRACT_START);
    }
    return ge_xr_act_btn_b;
}
