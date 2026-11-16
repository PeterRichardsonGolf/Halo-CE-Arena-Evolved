/*
AE_ROTATION_SHUFFLE.H

The order a dedicated server plays its rotation in when server.rotation_shuffle is on (server_director.c): each
pass over the rotation is a new random order of its entries, every entry exactly once. Never the same map twice
in a row, the boundary between passes included (the first entry of a pass against the entry played last), and,
where the pool allows it, not the same mode twice in a row either. Pure: no engine calls
(port/linux/tests/ae_rotation_shuffle_test.c).
*/

#ifndef __AE_ROTATION_SHUFFLE_H
#define __AE_ROTATION_SHUFFLE_H

#define AE_ROTATION_SHUFFLE_MAXIMUM 64

/* the mode a gametype name stands for, for telling two entries apart by mode: the name without its family marks
(ae, ev, br, comp, btb, pro) and its "team" mark, "ts" and "sly" read as slayer: ae_team_sly, ae_br_ts and
ev_team_sly are all "slayer". Any case. */
void ae_rotation_shuffle_mode_key(
	char const *gametype,
	char *key,
	int key_size);

/* a new order of count entries (at most AE_ROTATION_SHUFFLE_MAXIMUM): order[i] is the entry played i-th, each
entry once. map[i] and gametype[i] are entry i's map and gametype names (compared in any case; a map named by its
file); previous_map and previous_gametype are the entry played just before the pass (NULL: none). seed picks the
order. Returns 1 if no two neighbours (and not the first against the previous entry) share a map, 0 if the pool
cannot do that (one map alone, or one map in more than half of the entries): the order is then the best found. */
int ae_rotation_shuffle_order(
	int count,
	char const *const *map,
	char const *const *gametype,
	char const *previous_map,
	char const *previous_gametype,
	unsigned long seed,
	int *order);

/* the same with each entry's weight (1 to 5; weights may be NULL): at each place the entry is drawn with a chance in
proportion to its weight among the entries the rules leave (a heavier entry tends to come earlier in a pass; every entry
is still played once a pass) */
int ae_rotation_shuffle_order_weighted(
	int count,
	char const *const *map,
	char const *const *gametype,
	int const *weights,
	char const *previous_map,
	char const *previous_gametype,
	unsigned long seed,
	int *order);

/* the map vote's rules (server_director.c): the entry just played is not one of the choices when there are
two or more entries to choose from (1 yes, 0 no: a single one stays), and a vote nobody took part in is won by the
first choice offered that is not the entry just played (the choice's place; the first if every choice is it) */
int ae_rotation_vote_drop_played(
	int candidate_count);
int ae_rotation_vote_default(
	long const *entries,
	int count,
	long played);

/* the rotation word of an AE gametype's stored name: lower case, a '_' for each space ("AE TEAM SLY" -> "ae_team_sly").
Returns the length, 0 if it does not fit in size (key size at least 2) */
int ae_rotation_word_of(
	char const *stored_name,
	char *word,
	int word_size);

#endif
