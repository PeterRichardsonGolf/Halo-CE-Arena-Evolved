/*
AE_CAMO_VIEWMODEL.C

The first-person weapon and arms under active camouflage, against a nearby
wall. Uncamouflaged, they are drawn before the world with its own near and
far planes (rasterizer_globals.first_person_weapon_*_clip_distance) and
mark the stencil, so the world never covers them. Camouflaged, the opaque
pass only queues them as transparent groups: the world is then drawn where
they are, its depths stay, and rasterizer_active_camouflage_draw tests the
weapon's depths, from the first person's projection (near plane 0.0117
against the world's 0.0625, so deeper for the same distance), against the
world's. A wall within about five times a part's distance hid that part, and
standing at a wall most of the weapon went "into" it (upstream's code: OpenCE
and ChupathingyCE draw it the same way).

Around each first-person group's camouflage passes the viewport's depth
range is narrowed to its front sixty-fourth: the weapon's and arms' own
depths keep their order among themselves (the depth-only pass, the model
drawn under a fading camouflage and the distortion's EQUAL test all see the
same range), and only world geometry closer than the world's near plane
(nothing drawn) could be in front of them. After the passes the window's
viewport is put back as rasterizer_set_target sets it for the primary
target (the only one camouflage draws in). The first-person effects drawn
after (flashes, glows) do not test depth on the weapon's pixels: their
first pass is stenciled to the opaque weapon, which a camouflaged one
never marks, and their second has no depth test.
*/

#include "cseries.h"
#include "rasterizer/rasterizer.h"

#include <xtl.h>

#include "rasterizer/xbox/rasterizer_xbox.h"
#include "ae_camo_viewmodel.h"

/* the front of the depth buffer the camouflaged first person is drawn in:
the world's depth there is 1 - near / distance, below 1/64 only closer than
1.016 times its near plane (0.0625 world units) */
#define CAMO_VIEWMODEL_DEPTH_RANGE (1.0f / 64.0f)

static void camo_viewmodel_viewport(real maximum_z)
{
	D3DVIEWPORT8 viewport;

	viewport.X = global_window_parameters.camera.viewport_bounds.x0;
	viewport.Y = global_window_parameters.camera.viewport_bounds.y0;
	viewport.Width = global_window_parameters.camera.viewport_bounds.x1 -
		global_window_parameters.camera.viewport_bounds.x0;
	viewport.Height = global_window_parameters.camera.viewport_bounds.y1 -
		global_window_parameters.camera.viewport_bounds.y0;
	viewport.MinZ = 0.0f;
	viewport.MaxZ = maximum_z;
	IDirect3DDevice8_SetViewport(global_d3d_device, &viewport);
}

void ae_camo_viewmodel_depth_begin(void)
{
	camo_viewmodel_viewport(CAMO_VIEWMODEL_DEPTH_RANGE);
}

void ae_camo_viewmodel_depth_end(void)
{
	camo_viewmodel_viewport(1.0f);
}
