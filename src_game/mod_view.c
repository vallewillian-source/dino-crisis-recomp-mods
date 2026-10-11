/*
 * Game-owned display-view plugin for Dino Crisis.
 *
 * On PSX the display-view controls (aspect/widescreen, frame interpolation)
 * are owned by trusted mods (main.cpp: "Widescreen/View mode ... are
 * mod-owned"). This registers the activation plugins referenced by
 * mods/packages/dc.enhancement.widescreen; enabling a feature runs its
 * activation callback after the launcher commits the mod plan.
 *
 * dc.view-widescreen  -> fixed 16:9 display aspect (GTE squash / wide present;
 *                        DC rooms classify as 2D-only, so the net effect is
 *                        the classic stretched-backdrop widescreen look).
 * dc.view-motion-interp -> presentation-only temporal blending at display
 *                        refresh, motion-adaptive (guest timing stays stock).
 *
 * Disabling a feature simply never calls its callback: 4:3 / stock pacing.
 */
#include "mod_plugins.h"

static void dc_widescreen_activate(void) {
    (void)psx_mod_set_fixed_display_aspect(16u, 9u);
}

static void dc_motion_interp_activate(void) {
    (void)psx_mod_set_frame_interpolation(0u);  /* 0 = display refresh rate */
    (void)psx_mod_set_frame_interpolation_blend(
        PSX_MOD_FRAME_INTERPOLATION_MOTION_ADAPTIVE);
}

PSX_MOD_CONSTRUCTOR(dc_register_view_plugins) {
    (void)psx_mod_register_activation_plugin("dc.view-widescreen",
                                             dc_widescreen_activate);
    (void)psx_mod_register_activation_plugin("dc.view-motion-interp",
                                             dc_motion_interp_activate);
}
