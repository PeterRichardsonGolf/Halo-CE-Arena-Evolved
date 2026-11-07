/*
AE_DEV_DUMMY.H

DEV MODE's dummy command: target dummies for weapon tuning (the rules: ae_dev_dummy_rules.h). A dummy is a bot of a local
game: it stands still where it was put, facing the player, and never moves or shoots; it heals to full 3 s after it was last
hurt and respawns at the same spot 1 s after it dies. bots.c lets the dummy command's bots do nothing
(ae_dev_dummy_hold); ae_bots_live.c lets them go first when bots.count falls (ae_dev_dummy_leaver).
*/

#ifndef __AE_DEV_DUMMY_H
#define __AE_DEV_DUMMY_H

#include "cseries.h"

struct player_action;

/* the dummy command: `count` new dummies for the player, `distance` ahead (or `here`): 1 and bots.count's new value
in *bots_wanted, or 0 and why in error */
int ae_dev_dummy_add(long player_index, int count, boolean here, float distance, long *bots_wanted, char *error,
	int error_size);
/* the clear command: every dummy goes (flagged ones that never joined are forgotten): how many, and bots.count's new
value in *bots_wanted */
int ae_dev_dummy_clear(long *bots_wanted);
/* how many dummies there are (in the game or on their way) */
int ae_dev_dummy_count(void);
/* each game tick (ae_dev_cmd_tick), DEV MODE on: placing, healing and respawning */
void ae_dev_dummy_tick(void);
/* a new game, or DEV MODE off: no dummy is flagged */
void ae_dev_dummy_forget(void);

/* (bots.c's bots_bot_update, last) TRUE for a dummy's bot: its controls set to nothing, its look to the way it was put */
boolean ae_dev_dummy_hold(long bot_index, struct player_action *action, unsigned long *flags);
/* (ae_bots_live.c, when a bot leaves because bots.count fell) the bot that leaves: a dummy in the game first, else
`fallback`, the game's own pick. `in_game[i]` is non-zero for each bot (0 up) in the game */
int ae_dev_dummy_leaver(unsigned char const *in_game, int maximum, int fallback);

#endif
