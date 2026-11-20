/*
AE_VIEW_BOTS.H

(0.7.9) DEV MODE's bot view, view bots (owner pick 11-07, direction D round 2: spec 2026-11-07-view-bots-spec.md section
12). A see-through board of the team's bots under the DEV MODE chip, a squad a column: each bot's number, name, the system
that chose its goal, the goal in words and metres and its state (FIGHT, STUCK, NO PATH, TETHER); the WHY strip as its last
line (the focused bot's last WHY, else the team's newest under 30 s); numbered tags by the names over the bots in view,
pips at the screen's edge for those out of it, and the focused bot's goal line. Each split-screen view shows its own
player's team. The focus is the pinned bot (view bots <n>), else the one aimed at (within 6 degrees, held 1 s).

Off by default; for the session only. DEV MODE only, and on the host only: the bots' state lives there, nothing is sent.
Drawn with AE's renderer (ae_draw.h) at the frame's Present, over the HUD. The rules are ae_view_bots_rules.c's.
*/

#ifndef __AE_VIEW_BOTS_H
#define __AE_VIEW_BOTS_H

/* the console's view bots: word on, off, aim (unpin), enemy, own, or a number (pin bot <number>); the answer in text.
FALSE when it did nothing */
int ae_view_bots_command(char const *word, long number, char *text, int size);
int ae_view_bots_on(void);

/* (hud.c's hud_draw_player_name, AE hook) a name over a head was drawn in this view: its box, for the tags */
void ae_view_bots_name_noted(long player_index);
/* (hud.c's hud_draw_screen, AE hook) this view's pass: its camera, for the tags, pips, goal line and the aim */
void ae_view_bots_view(short local_player_index);
/* (main.c, AE hook) once a frame: the boards, tags and pips of every view */
void ae_view_bots_draw(void);

/* (ae_panels.c's label fade) the board's box in a view, in that view's pixels (x0, y0, x1, y1), as last drawn */
int ae_view_bots_extent(short local_player_index, float extent[4]);

#endif /* __AE_VIEW_BOTS_H */
