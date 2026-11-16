/*
AE_SPLIT_QUIT_RULES.H

Arena Evolved: a split-screen player who quits leaves the screen (owner 2026-11-09: "look into a better way to handle when
a splitscreen player quits the game"). When the quit takes effect (game.c's remove_quitting_players_from_game) and other
players of this machine remain, the player comes off the machine's local list, so the others' views grow at once (two
players: the other goes full screen; four: three views). The player stays in the game as a quit player: their row in the
scoreboard and the carnage report stays. Pure (port/linux/tests/ae_split_quit_rules_test.c); the game's side is
ae_split_quit.c.
*/

#ifndef __AE_SPLIT_QUIT_RULES_H
#define __AE_SPLIT_QUIT_RULES_H

/* whether a player who quit leaves the local list: they are on it, at least one other local player is, and the game is a
network multiplayer one's (not co-op, whose QUIT quits every controller; not a film) */
int ae_split_quit_leaves(int is_local, int local_players, int coop, int film);

#endif
