/*
AE_STEAM_FRIENDS_RULES.H

STEAM FRIENDS for the online party: which of a player's Steam friends run AE. DEV tests only for now: a hash is the first 12 hex digits
of SHA-256("ae-steam-1" + the Steam id in decimal), which anyone can reverse and claim, so the party service refuses the steam calls
(404) unless it is started with --steam-dev; the verified version (Steam sign-in, keyed codes) replaces this.
Pure (port/linux/tests/ae_steam_friends_rules_test.c).
*/

#ifndef __AE_STEAM_FRIENDS_RULES_H
#define __AE_STEAM_FRIENDS_RULES_H

#include "ae_party_client_rules.h"

enum { AE_STEAM_FRIENDS_MAX = 300, AE_STEAM_HASH = 13, AE_STEAM_CHUNK = 40 };

struct ae_steam_friend
{
	unsigned long long id;
	char name[32];                 /* the Steam name, shown only on this PC */
};

/* a Steam id's short hash (12 hex digits and the end) */
void ae_steam_hash(unsigned long long id, char out[AE_STEAM_HASH]);

/* the comma-separated hashes of friends[from ...] (at most AE_STEAM_CHUNK of them, and as many as fit size): into csv; how many were written */
int ae_steam_hashes_csv(const struct ae_steam_friend *friends, int count, int from, char *csv, int size);

/* the screen's rows: one per friend running AE (the Steam name, the AE name, code and state), in the Steam list's order; others is how
many of the friends are not (a count only: AE cannot tell who plays or who is online without the friend asking too) */
struct ae_steam_row
{
	char steam_name[32];
	char ae_name[AE_PC_NAME];
	char code[AE_FRIEND_CODE_SIZE];
	char state[AE_PC_WORD];
	int verified;
};
int ae_steam_rows(const struct ae_steam_friend *friends, int count, const struct ae_pc_steam *found, int found_count,
	struct ae_steam_row *rows, int maximum, int *others);

/* the state's words as STEAM FRIENDS shows them ("IN YOUR PARTY" needs the party: in_party is the member codes' test result) */
const char *ae_steam_state_word(const char *state, int in_party);

#endif
