/*
 * Hand-apply into the workshop cinema / hub draw TU (same site as
 * getenv("GETV_XR_PLAY_SCREEN") / gevr_xr hub quad layer).
 * Public tree has no cinema / gevr_xr C — do not push those bodies.
 *
 * Chair FAIL (workshop already Tick, no glass):
 *   hub gate treated PLAY_SCREEN=2 as "not cinema" (ship boot is =2).
 * Use ApplyHubGate. Do not call SetHubActive(play_screen == 1).
 *
 * First grep:
 *   GETV_XR_PLAY_SCREEN / AUTOSCREEN / FOVSCALE_CINEMA
 *   gePortSimShouldTick / GETV_VR_ONESHOT
 *   gevr_xr hub / XrCompositionLayerQuad / PLAY_SCREEN blit
 *
 * GETV_VR_OPT_PANEL unset/empty/0 = OFF.
 * =1 → world-locked dark glass to the wearer's RIGHT of the cinema.
 *
 * Do not parent to HMD / watch / GETV_VR_HUB.
 * Do not touch bondview2 / HEAD_TRANSLATE / PLAYSPACE / GUNREBASE.
 * Face A is the #32 trap — confirm is trigger / poke only.
 */

#ifdef GE_PORT_NATIVE
#include "ge_vr/ge_vr_opt.h"

/* frontend_or_intro: title / intro / file-select / AUTOSCREEN cinema up.
 * play_screen: GETV_XR_PLAY_SCREEN (2 = ship world-lock). Pass 2, not (x==1).
 * gameplay_stereo_eyes: 1 once a mission eye pair exists (same cinema→VR gate). */
static void ge_vr_opt_panel_hub_frame(int frontend_or_intro,
                                      int play_screen,
                                      int gameplay_stereo_eyes,
                                      int first_eye,
                                      const float cinema_center[3],
                                      const float cinema_right[3],
                                      const float cinema_up[3],
                                      const float cinema_normal[3],
                                      float cinema_width,
                                      float cinema_height)
{
    GeVrOptPanelGlassLayer glass;
    GeVrOptPanelQuad drop;
    GeVrOptLaser laser;
    int hand;

    /* SCREEN=2 counts. Do not write: SetHubActive(play_screen == 1). */
    geVrOptPanelApplyHubGate(frontend_or_intro, play_screen,
                             gameplay_stereo_eyes);
    if (!geVrOptPanelVisible())
        return;

    if (first_eye) {
        geVrOptPanelSetCinemaFrame(cinema_center, cinema_right, cinema_up,
                                   cinema_normal, cinema_width, cinema_height);
        geVrOptPanelTick(1);
    } else {
        geVrOptPanelTick(0);
    }

    if (!geVrOptPanelGetGlassLayer(&glass))
        return;

    /*
     * Workshop blit — reuse the gevr_xr hub / PLAY_SCREEN=2 quad layer:
     *   same world space as the cinema billboard (not HMD / view-locked)
     *   XrCompositionLayerQuad (or the live hub-quad helper) from glass.quad
     *   dark translucent fill = glass.chrome.glass_rgba
     *   glow hover outline = glass.chrome.glow_rgb when Hovered()
     *   header "VR SETTINGS" + close (geVrOptPanelCloseHot)
     *   rows: LEFT name+value, RIGHT chevron (GetRowRect)
     *   enum dropdown: GetDropdownQuad — sibling RIGHT, never overlaps
     * Laser: GetLaser origin→hit while the panel is up. Trigger commit.
     */
    {
        /* submit world-locked quad: glass.quad + glass.chrome.glass_rgba */
        (void)glass;
        if (geVrOptPanelGetDropdownQuad(&drop)) {
            /* smaller sibling quad to the RIGHT of the main glass */
            (void)drop;
        }
        for (hand = 0; hand < GE_VR_HAND_COUNT; ++hand) {
            if (geVrOptPanelGetLaser((GeVrHand)hand, &laser)) {
                /* glowing beam laser.origin → laser.hit */
                (void)laser;
            }
        }
    }
}
#endif
