/*
AE_POWER_LIST.H

HUD STYLE AE's match clock and timers list (spec 11.10b T1, 12.3; plan Task 7):
the clock is in the score strip, so the corner clock is not drawn; the power
items' list (TIMERS, TRAINING) is drawn in the strip's sub-line style: Overpass
900 14 px, names grey caps, times white tabular right-aligned on the right-hand
column's edge (the shield meter's right edge), rows 20 px apart, the foot row on
the shared bottom line (11.12), "+N MORE" 13 px at 60 % above it, R/B in their
sides' colours; TOP LEFT (display.power_list, one view) keeps its place, a line
in the same style. CLASSIC: nothing here runs (hud_item_timers.c's own).
Sizes are px at 1080p x the view's scale; small text never under 12 px (14 in
quarter views), the times never under 14 px.
*/

#ifndef __AE_POWER_LIST_H
#define __AE_POWER_LIST_H

#include "ae_pure_types.h"

/* (hud_item_timers.c) TRUE: AE style in a game with a score strip: no corner clock */
boolean ae_power_list_hides_clock(short local_player_index);
/* (hud_item_timers.c) TRUE: AE style drew the timers list (or had nothing to draw): CLASSIC's is skipped */
boolean ae_power_list_draw(long font_index);
/* CAMPAIGN TIMER in HUD STYLE AE: the time played (ticks) in the corner in AE's digits; FALSE in HUD STYLE CE */
boolean ae_power_list_campaign_clock(short local_player_index, long ticks);
/* the list's left edge in the view (screen px) this frame, for the strip's gap rule; the view's right edge when no
list is drawn on the right */
real ae_power_list_left_x(short local_player_index);
/* the last frame's list: its foot row's baseline and left edge (screen px); FALSE when none was drawn */
boolean ae_power_list_last(short local_player_index, real *foot_baseline, real *left_x);
/* the list's last drawn extent in the view px: left, top, right, bottom */
boolean ae_power_list_extent(short local_player_index, real extent[4]);
/* the right-hand column's x in the view (screen px): the shield meter's right edge, where the timers list (and the
own-buff rows of the AE panels) end (spec 14.15 part 3) */
real ae_power_list_right_x(short local_player_index);
/* (a map's tags going) the last game's lists forgotten: a campaign never draws one (campaign review 2026-10-18 M6) */
void ae_power_list_forget(void);

#endif /* __AE_POWER_LIST_H */
