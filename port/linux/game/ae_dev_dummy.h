/*
AE_DEV_DUMMY.H

DEV MODE's dummy command: target dummies for weapon tuning (the rules: ae_dev_dummy_rules.h). A dummy is a bot of a local
game: it stands still where it was put, facing the player, and never moves or shoots (a walking dummy walks: the rules
header); it heals to full 3 s after it was last
hurt and respawns at the same spot 1 s after it dies. bots.c lets the dummy command's bots do nothing
(ae_dev_dummy_hold); ae_bots_live.c lets them go first when bots.count falls (ae_dev_dummy_leaver).
*/

#ifndef __AE_DEV_DUMMY_H
#define __AE_DEV_DUMMY_H

#include "cseries.h"

struct player_action;

/* the dummy command: `count` new dummies for the player, `distance` ahead (or `here`), facing you or, with `away`,
the way you face: 1 and bots.count's new value in *bots_wanted, or 0 and why in error */
int ae_dev_dummy_add(long player_index, int count, boolean here, boolean away, float distance, long *bots_wanted,
	char *error, int error_size);
/* the dummy walk command: one walking dummy on the player's team, at `at` (or at the player's feet when `here`), walking
to `goal` when has_goal (else roaming): 1 and bots.count's new value in *bots_wanted, or 0 and why in error */
int ae_dev_dummy_add_walker(long player_index, boolean here, float const *at, boolean has_goal, float const *goal,
	long *bots_wanted, char *error, int error_size);
/* (bots.c's bots_choose_goal, first) TRUE with the goal for a walking dummy's bot that has one */
boolean ae_dev_dummy_walk_goal(long bot_index, float *x, float *y, float *z);
/* (0.9.3, the reach report) count walking dummies at once, the i-th from starts[i] to goals[i]: how many (their bots in
bot_indices, bots.count's new value in *bots_wanted), or 0 and why in error; a walking dummy sent to a new start (put
there on the next tick) and goal */
int ae_dev_dummy_add_walkers(long player_index, int count, float const (*starts)[3], float const (*goals)[3],
	long *bot_indices, long *bots_wanted, char *error, int error_size);
void ae_dev_dummy_walk_retarget(long bot_index, float const *start, float const *goal);
/* whether bot `bot_index` (0 up) is a walking dummy */
boolean ae_dev_dummy_walks(long bot_index);
/* the clear command: every dummy goes (flagged ones that never joined are forgotten): how many, and bots.count's new
value in *bots_wanted */
int ae_dev_dummy_clear(long *bots_wanted);
/* how many dummies there are (in the game or on their way) */
int ae_dev_dummy_count(void);
/* each game tick (ae_dev_cmd_tick), DEV MODE on: placing, healing and respawning */
void ae_dev_dummy_tick(void);
/* whether the player is a dummy (flagged, in the game) */
boolean ae_dev_dummy_player(long player_index);
/* whether bot `bot_index` (0 up) is a dummy: its driver's seat is its own to keep (the vehicle code never gets it out) */
boolean ae_dev_dummy_flagged(long bot_index);
/* whether the player is not a dummy still waiting to be put at its spot (a command naming it waits for that) */
boolean ae_dev_dummy_placed(long player_index);
/* a new game, or DEV MODE off: no dummy is flagged */
void ae_dev_dummy_forget(void);
/* (0.9.4; owner 11-16 melee-mate SKIP, "team mate did not spawn": a mate dummy joined the other team, bots.teams even
puts a lone bot opposite the host) "dummy ... mate": the dummies that join from now are the host's team's, and the bots'
teams are not evened while it holds (cleared by dummy clear and a new game) */
void ae_dev_dummy_mates_set(boolean on);
boolean ae_dev_dummy_mates(void);

/* (bots.c's bots_bot_update, last) TRUE for a dummy's bot: its controls set to nothing, its look to the way it was put;
a walking dummy's bot keeps its walk and loses only its triggers, grenade and blow (FALSE) */
boolean ae_dev_dummy_hold(long bot_index, struct player_action *action, unsigned long *flags);
/* (ae_bots_live.c, when a bot leaves because bots.count fell) the bot that leaves: a dummy in the game first, else
`fallback`, the game's own pick. `in_game[i]` is non-zero for each bot (0 up) in the game */
int ae_dev_dummy_leaver(unsigned char const *in_game, int maximum, int fallback);

#endif
