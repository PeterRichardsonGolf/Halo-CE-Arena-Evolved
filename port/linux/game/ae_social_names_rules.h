/*
AE_SOCIAL_NAMES_RULES.H

The offline social career's bot names (owner 2026-11-15: "we will need to have a bunch more bot names"), as pure rules: a
built-in pool of invented handles (ae_social_names_pool.inc, the owner's list first), a seeded pick of a lobby's names
with no name twice and none a player in the lobby already has, and a stable hash of a name (the same name always plays
the same way: its play style is chosen from the hash). No engine includes: port/linux/tests/ae_social_names_rules_test.c
compiles these alone.
*/

#ifndef __AE_SOCIAL_NAMES_RULES_H
#define __AE_SOCIAL_NAMES_RULES_H

/* a player name's characters (the network player's name holds 11 and a NUL) */
#define AE_SOCIAL_NAME_LENGTH 11

/* the pool: how many names, and the index-th (NULL out of range) */
int ae_social_names_count(void);
char const *ae_social_names_get(int index);

/* `wanted` names for a lobby, from `seed`: their pool indices into out (all different; none equal, in any case, to one
of the avoid names, e.g. the players'): how many (fewer only when the pool runs out). The same seed and avoid list
pick the same names */
int ae_social_names_pick(unsigned long seed, int wanted, char const *const *avoid, int avoid_count, int *out);

/* a name's stable hash (FNV-1a over its lower-case letters, 32 bits): the same on every machine and build */
unsigned long ae_social_name_hash(char const *name);

#endif
