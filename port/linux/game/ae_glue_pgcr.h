/*
AE_GLUE_PGCR.H

The post-game report screen's contact with the game (ae_glue_pgcr.c): a report for the board and the pane
(ae_pgcr_screen_rules.h) from the game that just ended (the engine's ranking, the players, the synced statistics, the
host's summaries of ae_pgcr_data.h), and the last report kept for the lobby until the next game starts.
*/

#ifndef __AE_GLUE_PGCR_H
#define __AE_GLUE_PGCR_H

#include "ae_pgcr_screen_rules.h"

/* the report now: the game's own numbers, and the host's summaries where this machine has them. Cheap enough to ask each
frame; report->valid is 0 with no game */
void ae_glue_pgcr_collect(struct ae_pgr_report *report);
/* the last report kept, NULL for none (a new game's start clears it) */
struct ae_pgr_report const *ae_glue_pgcr_kept(void);
/* keep this report (a whole one: its game's scores are up) */
void ae_glue_pgcr_keep(struct ae_pgr_report const *report);
/* a new game starts: the kept report goes */
void ae_glue_pgcr_forget(void);

#endif
