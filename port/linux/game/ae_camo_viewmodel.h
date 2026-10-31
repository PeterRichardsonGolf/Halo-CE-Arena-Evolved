/*
AE_CAMO_VIEWMODEL.H

The camouflaged first-person weapon drawn in front of the world, as the
uncamouflaged one is (ae_camo_viewmodel.c): what the AE hook lines in
rasterizer_xbox_active_camouflage.c call (port/linux/game/ae_hooks.txt).
*/

#ifndef __AE_CAMO_VIEWMODEL_H
#define __AE_CAMO_VIEWMODEL_H

/* around one first-person group's active camouflage passes (the window's own
view, the primary target): its depths go to the front of the depth buffer */
void ae_camo_viewmodel_depth_begin(void);
void ae_camo_viewmodel_depth_end(void);

#endif
