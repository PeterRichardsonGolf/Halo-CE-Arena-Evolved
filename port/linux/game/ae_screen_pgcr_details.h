/*
AE_SCREEN_PGCR_DETAILS.H

(rc2) The post-game report's DETAILS page (ae_pgcr_details_rules.h's words): one player's game in full, drawn over the whole
frame in place of the board (the CARNAGE step's Y, ae_screen_pgcr.c). Measures are the report's 1920 x 1080 design units and the
SERVICE RECORD pages' grid (ae_record_pages.h): a head ("POST-GAME REPORT", a dot, DETAILS; the player's colour chip, name and tag;
team or place, gametype, map and time), three panes at 232 (THIS GAME, ACCURACY AND DAMAGE, KILL KINDS) and three at 696
(TEAMS or TOP PLAYERS, the game kind's block, ASSISTS).
*/

#ifndef __AE_SCREEN_PGCR_DETAILS_H
#define __AE_SCREEN_PGCR_DETAILS_H

#include "ae_pgcr_screen_rules.h"

struct frame_metrics;

/* draws the page for the report's row `row` (the report's own order) */
void ae_pgcr_details_draw(struct ae_pgr_report const *report, int row, struct frame_metrics const *f);

#endif
