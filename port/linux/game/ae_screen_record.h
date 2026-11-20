/*
AE_SCREEN_RECORD.H

SERVICE RECORD (0.7.6): the page of what this PC has seen of one player over the games it played (ae_service_record.h):
games, wins and the win rate, kills, deaths, assists and K/D, the same by the kind of game, and the medals by kind. (rc2) Three
pages, OVERVIEW | COMBAT | MODES & MAPS (ae_record_pages.h), gone through with Y (pad), Tab (keyboard) or a click on the chips of the
head, or LB / RB. The D-pad, arrows or stick move an ice-blue focus between the boxes that have a DETAILS page (ae_record_focus.h); A or a click opens its page of the detail window (ae_screen_record_window.h; the MEDALS box opens the MEDALS page), B backs out to the box. On one's own record OVERVIEW lists one's rivals with the kills each way; on a rival's it adds the line of one against them. It is this
PC's view only: another PC may have seen more or less of the same player.

LT / RT (Page Up / Page Down, the "Next player" prompt) step to the next or previous player this PC keeps a record of, the viewer's own first and
then the most recently seen (ae_record_step_rules.h), wrapping; the page and the focused box stay. With one player kept the prompt is not shown.

Two doors: a player's card on the post-game report (SERVICE RECORD, ae_screen_pgcr.c) and the PROFILES list (the right
shoulder on a profile, ae_screen_profiles.c). B leaves.

(0.8.1) The MEDALS pane (on MODES & MAPS) is the page's one focusable part once the player has a medal: a 2 u white ring with a pad or keys,
the hover wash under the mouse, its last line "+N more" and ALL MEDALS. A (Enter) or a click opens the MEDALS page
(ae_screen_record_medals.h), every kind of medal in a grid.

Log: "ae record: page <name> (<n> games)" and "ae record: page closed".
*/

#ifndef __AE_SCREEN_RECORD_H
#define __AE_SCREEN_RECORD_H

/* opens the page of the player called name over the stack; viewer is the local player whose rivals count (a profile's
name; may be empty). 0 when the stack is full */
int ae_screen_record_open(short owner, char const *name, char const *viewer);

/* (DEV MODE record_medals <kinds> [seed] [page 1-3] [counted <n>] [kinds <1-7>]) the page of a made-up player with this many
medal kinds (random counts from the seed), in a record of its own that is never written: the record file is not touched. The new numbers
(time, shots, damage, objectives, maps, weapons, the last game) are made up too, for every game or for `counted` of them (negative:
every one; fewer than the games shows the muted "FROM n GAMES" tails). first_page: 0 OVERVIEW, 1 COMBAT, 2 MODES & MAPS. The games
are spread over the first mode_kinds game kinds (0: the usual three). An open page leaves first. 1 opened; text gets the answer */
int ae_screen_record_dev(int kinds, unsigned long seed, int first_page, long counted, int mode_kinds, char *text, int size);

/* (0.8.1, for the MEDALS page) the page's frame and its whole-frame backdrop: the design units' metrics, the frame and the
density; and the head: SERVICE RECORD (a dot and crumb after it: "MEDALS"), the player's name with its YOU or RIVAL tag,
and the sub line */
struct frame_metrics;
struct ae_frame;
struct ae_density;
struct ae_service_record;
void ae_record_frame_begin(struct frame_metrics *f, struct ae_frame *frame, struct ae_density *density);
void ae_record_head(struct frame_metrics const *f, struct ae_service_record const *record, char const *name, char const *viewer,
	char const *crumb, char const *sub);

#endif
