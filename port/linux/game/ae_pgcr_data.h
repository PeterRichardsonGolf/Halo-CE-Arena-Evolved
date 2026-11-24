/*
AE_PGCR_DATA.H

What the post-game report screen reads: the host's summary of each player (ae_pgcr_rules.h), kept from the end of a game
until the next game starts (so the lobby can reopen the last report). A machine that is not the host holds the summaries the
host sent; with a host that is not Arena Evolved, none: the screen falls back to the synced statistics
(player->statistics: score, kills, deaths, assists, shots fired and hit) and shows what it lacks as "--".

Pure code (no engine headers): port/linux/tests/ae_pgcr_rules_test.c runs it with the rules.
*/

#ifndef __AE_PGCR_DATA_H
#define __AE_PGCR_DATA_H

#include "ae_pgcr_rules.h"

/* a new game starts: the last report goes (ae_medals_new_map) */
void ae_pgcr_data_reset(void);
/* a summary arrived (or was made, on the host): kept for its slot. 0 when it is not a valid one */
int ae_pgcr_data_set(struct ae_pgcr_summary const *summary);
/* the slot's summary, NULL when this machine has none for it */
struct ae_pgcr_summary const *ae_pgcr_data_get(int slot);
/* whether this machine holds any summary (the host sent some, or it is the host): the screen's "have summary" */
int ae_pgcr_data_have_summary(void);
/* how many games have been reported since the program started: 0 none, else the last report's number */
long ae_pgcr_data_report_number(void);

#endif
