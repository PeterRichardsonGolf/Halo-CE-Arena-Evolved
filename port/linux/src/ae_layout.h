/*
AE_LAYOUT.H

The AE menus' layout space and its mappings, pure arithmetic (ae_layout.c):
no GL, no game, so the unit tests build it alone (port/linux/tests/
ae_draw_layout_test.c). ae_draw.c draws with it.

The layout space is the frame's picture, 1080 units tall and as wide as the
picture's aspect makes it. A view (a split-screen player's part, or the whole
frame) is a rectangle of it, inside which a screen lays itself out in a space
of its own that is again 1080 units tall. Plain types only (the game's side
includes this header too).
*/

#ifndef __AE_LAYOUT_H
#define __AE_LAYOUT_H

enum { AE_LAYOUT_HEIGHT = 1080 };

/* the layout space of the frame: 1080 tall, width by the picture's aspect */
struct ae_layout { float width, height; float scale; /* window pixels per unit */ float origin_x, origin_y; };
/* a view: its rectangle in layout units, and layout units per unit of its own 1080-tall space */
struct ae_view { float x, y, width, height; float scale; };

/* the layout of a picture at picture_x, picture_y (its top left, in pixels), picture_width by picture_height */
void ae_draw_layout(int picture_x, int picture_y, int picture_width, int picture_height, struct ae_layout *layout);
/* layout units to pixels and back */
void ae_layout_to_pixels(struct ae_layout const *layout, float x, float y, float *pixel_x, float *pixel_y);
void ae_layout_from_pixels(struct ae_layout const *layout, float pixel_x, float pixel_y, float *x, float *y);

/* a view of the layout (rectangle in layout units); its own space is 1080 tall and width / scale wide */
void ae_layout_view(float x, float y, float width, float height, struct ae_view *view);
float ae_view_width(struct ae_view const *view);
/* a view's own units to layout units and back */
void ae_view_to_layout(struct ae_view const *view, float x, float y, float *layout_x, float *layout_y);
void ae_view_from_layout(struct ae_view const *view, float layout_x, float layout_y, float *x, float *y);

/* the menus' 640x480 pointer coordinates (halo_ui_pointer: whole pixels, x 0 at the left of the 640 columns centred
in a picture screen_width wide) to layout units of a layout layout_width wide (a menu pixel's middle), and back
(fractional menu pixels; layout_width 0, before any picture, as wide as the picture's own aspect) */
void ae_layout_from_menu_point(short menu_x, short menu_y, int screen_width, float layout_width, float *x, float *y);
void ae_layout_to_menu_point(float x, float y, int screen_width, float layout_width, float *menu_x, float *menu_y);

/* a footer's prompts that fit a row: widths[i] is prompt i's whole width (its cap and its label), rank[i] its priority
(0 the highest; ties: the earlier one is the higher), gap between the kept ones, room the row's width. Marks keep[i] 1
for what stays; prompts are dropped lowest priority first until the rest fit (never a label alone shortened, never a
cap alone); returns how many stay (0 when not even the highest fits: the caller then shortens its label) */
short ae_prompts_fit(float const *widths, short const *rank, short count, float gap, float room, unsigned char *keep);

#endif
