/*
AE_ANNOUNCER_RULES.H

The announcer's pure rules (ae_announcer.c). Nothing here is sent: each machine decides for its own players. The draws
are handed in, so a test can fix them.
*/

#ifndef __AE_ANNOUNCER_RULES_H
#define __AE_ANNOUNCER_RULES_H

/* game.announcer "auto": the fewest of Arena Evolved's announcer lines an MCC game's sound banks must hold to announce */
#define AE_ANNOUNCER_AUTO_LEAST 20

/* "auto"'s pick among the MCC game folders that have a spoken-lines bank, given how many of Arena Evolved's announcer
lines each folder's banks hold (count folders, in the order listed): the one that holds the most (the first listed on
a tie), or -1 when none holds AE_ANNOUNCER_AUTO_LEAST */
int ae_announcer_auto_pick(int const *lines, int count);

#define AE_ANNOUNCER_ODDS 100

/* which take of a key plays: 0 up to clean - 1 a take of the key's own list, clean up to clean + others - 1 one of the
more takes, -1 none */
int ae_announcer_take(int clean, int others, unsigned long draw);

#define AE_ANNOUNCER_RUN_KILLS 8

int ae_announcer_run_kill(int *run);
void ae_announcer_run_end(int *run);

#define AE_ANNOUNCER_BURST_KILLS 3

struct ae_announcer_burst
{
	long tick;
	int who;
	long source;
	int count;
};

int ae_announcer_burst_kill(struct ae_announcer_burst *burst, long tick, int who, long source);

/* whether a pack speaks a medal (by its key): only the medals an announcer says aloud in play (multikills, sprees,
killjoy, revenge, first strike, perfection, extermination and a few more); a pack's files for the others (a headshot,
an assist, a plain kill...) stay quiet, as an MCC game's announcer is for them. 0 for a NULL key */
int ae_announcer_pack_speaks_medal(char const *key);

#endif
