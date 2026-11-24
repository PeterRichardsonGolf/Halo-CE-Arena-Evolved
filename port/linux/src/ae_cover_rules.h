/*
AE_COVER_RULES.H

A picture drawn to fill a box (CSS "cover"): the part of the picture that shows, as a rectangle of texture
coordinates, when the picture keeps its proportions and the overflow is cut evenly from the centre. The pure part:
no drawing, no game; port/linux/tests/ae_cover_rules_test.c.
*/

#ifndef __AE_COVER_RULES_H
#define __AE_COVER_RULES_H

/* the texture coordinates u0, v0, u1, v1 (in uv, each 0 to 1) of a picture of picture_w by picture_h pixels that
fill a box of box_w by box_h: the whole picture when the proportions match, else the middle of the longer side. A
size of zero or less (picture or box) gives the whole picture (0, 0, 1, 1) */
void ae_cover_uv(int picture_w, int picture_h, float box_w, float box_h, float uv[4]);

/* whether a picture of picture_w by picture_h already has the proportions of a box of box_w by box_h (within one
percent), so that it needs no cutting; 1 for a size of zero or less (nothing to cut) */
int ae_cover_matches(int picture_w, int picture_h, int box_w, int box_h);

/* where a picture of picture_w by picture_h shows whole inside a box of box_w by box_h with its proportions kept
(CSS "contain"): x, y, width, height in rect, in the box's units from its top left corner, centred. A size of zero or
less (picture or box) gives the whole box */
void ae_cover_fit_rect(int picture_w, int picture_h, float box_w, float box_h, float rect[4]);

/* (a map's picture frame) the shape every map picture is shown in: 16:9. A picture whose proportions are between 1.2 and
2.0 fills the frame (cover: ae_cover_uv cuts the longer side); any other is shown whole inside it with plain bars (ae_cover_fit_rect),
so an odd picture's logo is never cut. A size of zero or less counts as one that fills */
float ae_cover_frame_height(float frame_w);
int ae_cover_fills(int picture_w, int picture_h);
/* the largest 16:9 rectangle inside a box of box_w by box_h, centred: x, y, width, height from the box's top left corner */
void ae_cover_frame(float box_w, float box_h, float rect[4]);

#endif
