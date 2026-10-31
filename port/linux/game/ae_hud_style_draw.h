/*
AE_HUD_STYLE_DRAW.H

HUD STYLE AE for the rest of what Arena Evolved draws (spec 12.6; plan Task 9):
the PRE-GAME COUNTDOWN, the performance overlay and the capture notice, the
names over players, the voice speakers' list and co-op's texts, each in the
strip's language. Each entry is an AE hook's: TRUE (or a change made) with
HUD STYLE AE, else nothing, and the CLASSIC code runs as before. Waypoint
labels, spawn markers, CAMPAIGN TIMER and CE's own messages are CLASSIC in
both styles.
*/

#ifndef __AE_HUD_STYLE_DRAW_H
#define __AE_HUD_STYLE_DRAW_H

#include <wchar.h>

#include "ae_pure_types.h"

union real_argb_color;
union rectangle2d;

/* the countdown's digit (Overpass 900 upright, 0.24 of the view's height, white, or the viewer's team colour in
team games) and the header's two lines under it (22 / 16 px x the view's scale, 0.17 of the view under its
middle), after the black (game_engine.c) */
boolean ae_hud_style_countdown(long ticks_left, short view_width, short view_height);
/* the performance line: Overpass 900 15 px, the strip's outline, one colour for the whole line (white at 60 FPS
and over, CLASSIC's yellow and red under); its extent for the capture notice and the timers (HUD units) */
boolean ae_hud_style_overlay(short corner, const union real_argb_color *color, const char *text, boolean *drawn,
	union rectangle2d *extent);
/* the last completed frame's AE menu process + draw time, for display.performance full */
void ae_ui_timing_append(char *line, long size, boolean full);
void ae_ui_timing_extent(short *bottom, short top, short line_height, float scale, const char *line);
/* the capture's notice: white, a failure red; under the performance line when it is in the same corner */
boolean ae_hud_style_capture_notice(boolean statistics_drawn, const union rectangle2d *statistics_extent,
	boolean statistics_right);
/* a name's colour over a player: allies in the team's colour, enemies neutral white (ENEMY NAME COLOR RED: red) */
boolean ae_hud_style_name_color(boolean ally, union real_argb_color *color);
/* the strip's outline under a name over a player, drawn as hud.c draws the name (its bounds, centred, scaled about
(x, y)) */
void ae_hud_style_name_outline(long font_index, const union rectangle2d *bounds, short x, short y,
	const wchar_t *name, const union real_argb_color *color, real scale);
/* the voice speakers' list: names as over players at 14 px with the outline, the speaker's icon dimmed */
boolean ae_hud_style_voice_speakers(void);
/* co-op's bottom text: "SPECTATING" grey, the name white, "A NEXT" grey; the hint grey 14 px; the skip vote's
words grey, its count white */
boolean ae_hud_style_bottom_text(const wchar_t *text);

#endif /* __AE_HUD_STYLE_DRAW_H */
