/* The match rules of the second-generation gametype settings (ae_variant_ext_rules.h): rounds, ties, the number of
players alive at once and slayer's bonus points, as pure decisions. No game dependency: unit-tested
(port/linux/tests/ae_h2_match_rules_test.c). The game side: ae_h2_match.c. */
#ifndef AE_H2_MATCH_RULES_H
#define AE_H2_MATCH_RULES_H

/* after a round has been played: 1 when another is played, 0 when the game is over. rounds is the game's number of
rounds (ae_ext_rounds_count), to_win the wins that end a best-of game early (0 for a fixed number of rounds), played
the rounds played so far, best_wins and runner_up_wins the wins of the two leading sides. A fixed number of rounds
ends when they are played; a tie in wins plays on while RESOLVE TIES is on */
int ae_h2_rounds_go_on(int rounds, int to_win, int resolve_ties, int played, int best_wins, int runner_up_wins);

/* the index of the one highest score, -1 when two or more share it or there are none */
int ae_h2_unique_leader(long const *scores, int count);

/* slayer's BONUS POINTS for a kill: one for killing the leader, one more for each fifth kill in a row (spree is the
killer's kills since their last death, this one included) */
int ae_h2_bonus_points(int victim_led, int spree);

/* MAX ACTIVE PLAYERS: whether a waiting player may spawn now (max 0: every player may); ahead_waiting is the number
of players who have waited longer */
int ae_h2_may_spawn(int max_active, int alive, int ahead_waiting);

/* whether an end the game asked for is a round's end (1) or the game's (0): only the engines' own ends are rounds' ends,
and only while the game plays more than one round. final_end is set for an end the host decided */
int ae_h2_end_is_round(int final_end, int rounds);

/* the slot of a per-player array of the given size for a player's absolute index: the index itself, -1 when it has none
(never a slot shared with another player) */
int ae_h2_slot_of(long absolute_index, int slots);

#endif
