/*
AE_VERSION_LINE.H

The build's version in a game (owner 2026-10-30, the HUD corners: "put the version number as the tail of the top
line, with the other indicators"): with display.version_watermark on, in a game with no menu up, "v0.7.0-beta" ends
the performance line at the top of the screen (main.c main_framerate_render) instead of sitting faint in the bottom
right corner over the match clock and the power column. The main menu's own version box is unchanged.
*/

#ifndef __AE_VERSION_LINE_H
#define __AE_VERSION_LINE_H

union real_argb_color;

/* whether the version goes on the top line now (main.c: the corner watermark not drawn; the line shown) */
int ae_version_line_takes(void);
/* appends "v<version>" to the line, when it takes it; 1 when the line holds the version alone (its colour then
ae_version_line_color's, not the frame rate's) */
int ae_version_line_append(char *line, long size);
/* the version's colour, alone on the line: the corner watermark's faint light grey */
void ae_version_line_color(union real_argb_color *color);

#endif
