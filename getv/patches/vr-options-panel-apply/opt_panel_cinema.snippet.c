/*
 * Hand-apply into the workshop cinema / hub draw TU (same site as
 * getenv("GETV_XR_PLAY_SCREEN")). Public tree has no cinema C.
 *
 * First grep:
 *   GETV_XR_PLAY_SCREEN / AUTOSCREEN / FOVSCALE_CINEMA
 *   gePortSimShouldTick / GETV_VR_ONESHOT
 *
 * GETV_VR_OPT_PANEL unset/empty/0 = OFF.
 * =1 → empty world-locked quad to the wearer's RIGHT of the cinema.
 *
 * Do not parent to HMD / watch / GETV_VR_HUB.
 * Do not touch HEAD_TRANSLATE / PLAYSPACE / GUNREBASE.
 * Do not register GETV_XR_TURN_SCALE here (Phase 3).
 * Face A is the #32 trap — confirm is trigger / poke only.
 */

#ifdef GE_PORT_NATIVE
#include "ge_vr/ge_vr_opt.h"

/* cinema_or_hub_this_frame: AUTOSCREEN cinema / frontend / intro / file-select.
 * 0 once gameplay stereo eyes exist (same gate as cinema → VR). */
static void ge_vr_opt_panel_hub_frame(int cinema_or_hub_this_frame,
                                      int first_eye,
                                      const float cinema_center[3],
                                      const float cinema_right[3],
                                      const float cinema_up[3],
                                      const float cinema_normal[3],
                                      float cinema_width,
                                      float cinema_height)
{
    GeVrOptPanelQuad q;

    geVrOptPanelSetHubActive(cinema_or_hub_this_frame);
    if (!geVrOptPanelVisible())
        return;

    if (first_eye) {
        geVrOptPanelSetCinemaFrame(cinema_center, cinema_right, cinema_up,
                                   cinema_normal, cinema_width, cinema_height);
        geVrOptPanelTick(1);
    } else {
        geVrOptPanelTick(0);
    }

    if (!geVrOptPanelGetQuad(&q))
        return;

    /*
     * Chrome (look/feel — not a Hand/6DoF/Haptic dump):
     *   dark translucent glass + glowing border (geVrOptPanelGetChrome)
     *   header "VR SETTINGS" + close (geVrOptPanelCloseHot)
     *   later rows: LEFT name+value, RIGHT chevron; hover = glow outline
     *   enum dropdown: GetDropdownQuad — RIGHT of main, never overlaps
     * Laser: for each hand, GetLaser → draw origin→hit while panel is up.
     * Select = trigger. Face A is the #32 trap. TOUCHUSE = fallback only.
     * Phase 2: zero ship rows.
     */
    {
        GeVrOptChrome chrome;
        GeVrOptLaser laser;
        int hand;
        geVrOptPanelGetChrome(&chrome);
        (void)chrome;
        (void)q;
        for (hand = 0; hand < GE_VR_HAND_COUNT; ++hand) {
            if (geVrOptPanelGetLaser((GeVrHand)hand, &laser)) {
                /* glowing beam laser.origin → laser.hit */
                (void)laser;
            }
        }
    }
}
#endif
