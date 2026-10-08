/*
AE_DRAW.H

The AE menus' renderer (ae_draw.c, AE's fork of Chupa's ui_overlay.c): the
shapes, text, button glyphs and pictures AE's screens draw, gathered while
the game draws a frame and drawn at that frame's Present into the back
buffer (so screenshots and recordings show them), at its resolution.

Coordinates are layout units (ae_layout.h): the frame's picture is 1080 units
tall. ae_draw_view puts the drawing into a view (a split-screen player's part)
whose own space is again 1080 units tall; ae_draw_view_full draws over the
whole frame again.

Called from the game's thread while it draws a frame. Plain types only: the
game's units (64-bit builds rewrite their long) and the platform's both call
it. Without the game browser (configure.py --no-game-browser) everything is
a stub and ae_draw_available() is 0.
*/

#ifndef __AE_DRAW_H
#define __AE_DRAW_H

#include "ae_layout.h"

/* fonts: AE_FONT_TITLE OpenCE, AE_FONT_ROW Overpass 900, AE_FONT_BODY Overpass 750 (the look's own faces,
ae_font.h; the overlay's Noto stands in for one that will not load); the device fonts are Kenney's Input Prompts */
enum { AE_FONT_TITLE, AE_FONT_ROW, AE_FONT_BODY, AE_FONT_XBOX, AE_FONT_PLAYSTATION, AE_FONT_NINTENDO,
	AE_FONT_KEYBOARD, AE_NUMBER_OF_FONTS };
enum { AE_ALIGN_LEFT, AE_ALIGN_CENTER, AE_ALIGN_RIGHT };
/* a controller's button for ae_draw_button, drawn as its device names it (Kenney's Input Prompts) */
enum { AE_BUTTON_A, AE_BUTTON_B, AE_BUTTON_X, AE_BUTTON_Y, AE_BUTTON_START, AE_BUTTON_LEFT_TRIGGER,
	AE_BUTTON_RIGHT_TRIGGER, AE_BUTTON_LEFT_SHOULDER, AE_BUTTON_RIGHT_SHOULDER, AE_BUTTON_DPAD_LEFT,
	AE_BUTTON_DPAD_RIGHT, AE_BUTTON_BACK, AE_NUMBER_OF_BUTTONS };
/* ae_draw_button's device_font: the device the player last used */
enum { AE_FONT_LAST_DEVICE = -1 };

/* colors are 0xRRGGBBAA */

/* the frame's layout (as of the last Present; before the first, from the game's screen width) */
void ae_draw_current_layout(struct ae_layout *layout);
/* a view (split screen): its rectangle in layout units; drawing goes inside it, scaled and clipped to it. Setting a
view (or the full one) empties the clip stack: push clips after it */
void ae_draw_view(float x, float y, float width, float height);
void ae_draw_view_full(void);
/* the current view's own width (its height is 1080) */
float ae_draw_view_width(void);
/* clipping (in the current view's units; nested clips intersect; at most 8 deep) */
void ae_draw_clip_push(float x, float y, float width, float height);
void ae_draw_clip_pop(void);
/* a filled rectangle (radius: rounded corners); with two colors, from the top's to the bottom's */
void ae_draw_rect(float x, float y, float width, float height, float radius, unsigned int rgba);
void ae_draw_gradient(float x, float y, float width, float height, float radius, unsigned int top, unsigned int bottom);
/* a rectangle's outline, thickness wide, inside it */
void ae_draw_outline(float x, float y, float width, float height, float radius, float thickness, unsigned int rgba);
/* text (UTF-8) on one line; returns its width. AE_FONT_TITLE / ROW / BODY: size is the em size (layout or view
units) and y the top of the capitals (baseline = y + cap height); the device fonts (AE_FONT_XBOX..KEYBOARD) keep M1's
meaning (size = pixel height, y the line's top) */
float ae_draw_text(int font, float size, float x, float y, int align, unsigned int rgba, const char *utf8);
float ae_draw_text_width(int font, float size, const char *utf8);
/* the same with tracking: tracking_em x size after every glyph but the last */
float ae_draw_text_tracked(int font, float size, float tracking_em, float x, float y, int align, unsigned int rgba,
	const char *utf8);
float ae_draw_text_tracked_width(int font, float size, float tracking_em, const char *utf8);
/* a text's ink box relative to y, each edge padded by 2 window pixels: *top <= 0 when ink rises above the capitals,
*bottom >= the cap height; returns the width. Clips and boxes around text are made from this, never from metrics */
float ae_draw_text_box(int font, float size, const char *utf8, float *top, float *bottom);
/* a straight stroke (check marks): a thickness-wide rectangle from x0, y0 to x1, y1, round ends */
void ae_draw_line(float x0, float y0, float x1, float y1, float thickness, unsigned int rgba);
/* every following colour's alpha multiplied by alpha (0..1) until changed; ae_draw_view_full and each Present reset
it to 1 */
void ae_draw_set_alpha(float alpha);
/* the current view, and the current view's own units per window pixel (for pixel floors) */
void ae_draw_current_view(struct ae_view *view);
float ae_draw_units_per_pixel(void);
/* a button's glyph (device_font AE_FONT_XBOX..AE_FONT_KEYBOARD, or AE_FONT_LAST_DEVICE), size high, x its left,
y its top; returns its width */
float ae_draw_button(int device_font, int button, float size, float x, float y, unsigned int rgba);
float ae_draw_button_width(int device_font, int button, float size);
/* an RGBA image (map pictures): id from ae_draw_image_load (0: none: too large, or out of memory), drawn scaled
into the rectangle; at most 64 live and 256 MB of them (64 MB on 32-bit), the least recently drawn freed for a new
one (its id then draws nothing, even if already drawn this frame) */
int ae_draw_image_load(const unsigned char *rgba, int width, int height);
void ae_draw_image(int id, float x, float y, float width, float height, unsigned int tint);
/* pointer: from the menus' 640x480 coordinates (halo_ui_pointer) to layout units */
void ae_draw_pointer_to_layout(short menu_x, short menu_y, float *x, float *y);
/* whether AE can draw (a window, GL set up, the game browser built in) */
int ae_draw_available(void);
/* the number of Presents AE has seen (the game's side draws its screens once per frame) */
unsigned int ae_draw_frame(void);

/* (d3d8_gl.c, at Present, before the screenshot: the frame's drawing into the back buffer's framebuffer,
width by height pixels, row 0 the picture's top) */
void ae_draw_present(unsigned int framebuffer, int width, int height);

#endif
