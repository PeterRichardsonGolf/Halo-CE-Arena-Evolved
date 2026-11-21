/*
AE_SCOPE_MASK_RULES.H

A weapon HUD's scope mask drawn to fill the whole view (ae_scope_mask_rules.c). No engine includes: the unit test in
port/linux/tests/ae_scope_mask_rules_test.c compiles it alone.
*/

#ifndef __AE_SCOPE_MASK_RULES_H
#define __AE_SCOPE_MASK_RULES_H

/* how many times its own size a mask's picture is drawn, about its middle: its edge carried out (the HUD's pictures are
drawn clamped at their edges, never repeated) */
#define AE_SCOPE_MASK_SPAN 5

/* whether a crosshair item is a zoom mask that fills the view: an item of the zoom state, a whole picture (not a sprite),
at its own size (no width or height scale of its own: 0 or 1) and at least twice as wide as it is tall (the packs' masks
are 2048 x 512 and 1024 x 512; a scope's marks and reticles are square). Drawn at its own size, a split-screen view of a
wide screen (its top or bottom half) is wider than it and showed the world in open strips beside its dark surround */
int ae_scope_mask_fills_view(int zoom_state, int not_a_sprite, float scale_x, float scale_y, int width, int height);

/* the picture's part to draw for such a mask: AE_SCOPE_MASK_SPAN times its whole about its middle, in texels for an
interface bitmap, else in the picture's 0-1. Its window keeps its size and place; past the picture its edge's dark
texels reach every side of any view */
void ae_scope_mask_clip(int interface_bitmap, int width, int height, float *x0, float *x1, float *y0, float *y1);

#endif
