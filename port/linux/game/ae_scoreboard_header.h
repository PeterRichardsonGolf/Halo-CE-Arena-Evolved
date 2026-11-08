/*
AE_SCOREBOARD_HEADER.H

HUD STYLE AE's scoreboard header (spec 11.9, 14.15 part 6; plan Task 8): over
the scoreboard (Tab held, and after the game), two lines and a rule: "TEAM
CTF . BLOOD GULCH - NHE" and "3 CAPTURES . 15:00" (ae_header_lines.c), in its
place of the scoreboard's gametype-name row (dropped with AE). 1p: above CE's
scoreboard panel, the rule as wide as the panel; split screen: above each
view's title at its x; in quarter views (960x540 of a 1080 screen) the header
and the scoreboard under it move down 18 px together, clear of the ammo's
rows; a split view's board is drawn at the full-screen board's scale (0.75)
about the view's centre, so its rows end above the score strip. CLASSIC:
nothing here draws; the gametype row stays.
*/

#ifndef __AE_SCOREBOARD_HEADER_H
#define __AE_SCOREBOARD_HEADER_H

#include "ae_pure_types.h"

union rectangle2d;
struct ae_header_lines;

/* the running game's header text (ae_header_lines.h): the countdown's lines too */
void ae_scoreboard_header_text(struct ae_header_lines *lines);

/* a pack's map's name (<file>@<pack>: the library's, else the pack's maplist.txt's), into out: FALSE for another
family's map (out untouched) */
boolean ae_pack_map_caption(char const *map_path, wchar_t *out, long count);
/* a map's name for the header and the post-game report, in capitals: a pack's map by its pack's name for it (CE+'s
bloodgulch@cex is ABYSS, not the stock map its file is named for), else ae_scoreboard_header_map_name's */
void ae_scoreboard_map_name(char const *map_path, wchar_t *out, long count);

/* (game_engine.c) the scoreboard's gametype rows: 0 with AE (the header has the name), else as they are */
long ae_scoreboard_header_rows(long gametype_rows);
/* (game_engine.c, the full-screen scoreboard) its panel's place, HUD units: draws the header above it */
void ae_scoreboard_header_full(short x0, short left, short top, short block_width, long line_height, real scale,
	real ping_room, real alpha);
/* (game_engine.c, the split-screen score) a row's bounds before it is drawn (row 0, the title): quarter views move
it down 18 px; above the title, the header; the row's text scale set (0.75 about the view's centre and the title's
top) */
void ae_scoreboard_header_row(long row, union rectangle2d *bounds, real alpha);
/* (game_engine.c) the row drawn: the text scale back to 1 */
void ae_scoreboard_header_row_drawn(void);

#endif /* __AE_SCOREBOARD_HEADER_H */
