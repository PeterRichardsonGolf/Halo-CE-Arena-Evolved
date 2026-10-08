/*
AE_DRAW_STUB.H

A recording ae_draw.h for the unit tests (ae_draw_stub.c): every ae_draw call
is kept as an ae_stub_call (with the view, clip and alpha current at it)
instead of drawn, so a test can check where widgets put things. Text is
measured with AE's real faces (ae_font.c, the TTFs in port/assets/fonts, read
from the repo root, the tests' working directory) exactly as ae_draw.c
measures it; the device fonts' glyphs are square (size wide). Views and clips
work as ae_draw.c documents them, through ae_layout.c.
*/

#ifndef __AE_DRAW_STUB_H
#define __AE_DRAW_STUB_H

#include "ae_draw.h"
#include "ae_style.h"

enum { AE_STUB_RECT, AE_STUB_GRADIENT, AE_STUB_OUTLINE, AE_STUB_TEXT, AE_STUB_BUTTON, AE_STUB_IMAGE, AE_STUB_LINE,
	AE_STUB_CLIP_PUSH, AE_STUB_CLIP_POP, AE_STUB_VIEW };
struct ae_stub_call
{
	/* (BUTTON: font the device font, align the button; IMAGE: align the image's id) */
	int kind, font, align;
	float size, x, y, width, height;   /* text: width from ae_font_measure, height = bottom - top */
	float top, bottom;                 /* text: its ink box relative to y (ae_draw_text_box's) */
	float x1, y1, thickness;           /* line */
	unsigned int rgba;                 /* with the current alpha applied (GRADIENT: the top's) */
	unsigned int rgba_bottom;          /* GRADIENT: the bottom's (others: rgba) */
	char text[160];
	struct ae_view view;               /* the view current at the call (VIEW: the view set) */
	/* the clip current at the call, in the view's units: x0, y0, x1, y1 (the whole view when clipped is 0) */
	float clip[4]; int clipped;
};
/* a frame layout_width x 1080 layout units, window_height pixels tall; loads the TTFs from port/assets/fonts (once);
forgets the calls; the full view, no clips, opaque (as after a Present) */
void ae_stub_reset(float layout_width, float window_height);
int ae_stub_count(void);
/* the index-th call (NULL past the last) */
struct ae_stub_call const *ae_stub_get(int index);
int ae_stub_find_text(const char *text, int from);   /* index of the first TEXT call with exactly this text, -1 */
/* a call's rectangle (text: x .. x + width as aligned, y + top .. y + bottom; line: its ends' box, grown by half
its thickness) in window pixels */
void ae_stub_pixels(struct ae_stub_call const *call, struct ae_rect *pixels);
/* a text call's em in window pixels */
float ae_stub_text_em_pixels(struct ae_stub_call const *call);

#endif
