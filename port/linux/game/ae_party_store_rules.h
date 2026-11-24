/*
AE_PARTY_STORE_RULES.H

The people (and bots) a player has partied or played with, kept on this PC: RECENT PLAYERS (the last AE_STORE_RECENT, never uploaded),
FAVOURITES (a saved bot, a friend: at most AE_STORE_SAVED), blocked players, and the record with each (games and wins together and
against). Pure code (no engine headers): port/linux/tests/ae_party_store_rules_test.c runs it. The file is text, one line a record.

- A record is touched when it plays or is partied with: it goes first among the recent ones; the oldest unsaved record is dropped past
  AE_STORE_RECENT. A saved record is never dropped.
- A bot that has played AE_STORE_AUTO_FAVOURITE games with the player in a party becomes a favourite when there is room.
- A blocked record is never offered as a recent player.
*/

#ifndef __AE_PARTY_STORE_RULES_H
#define __AE_PARTY_STORE_RULES_H

#include "ae_party_model_rules.h"

enum
{
	AE_STORE_RECENT = 100,
	AE_STORE_SAVED = 8,
	AE_STORE_AUTO_FAVOURITE = 2,
	AE_STORE_RECORDS = AE_STORE_RECENT + AE_STORE_SAVED,
	AE_STORE_TEXT = 24576,
	AE_STORE_CLOCK_MAX = 0x3FFFFFFF      /* the clock stays inside a 32-bit long: renumbered before it gets here */
};

struct ae_party_record
{
	char id[AE_PARTY_ID];
	char name[AE_PARTY_NAME];
	int kind;                   /* AE_MEMBER_BOT or AE_MEMBER_FRIEND (a person) */
	int saved, blocked, tier;
	long seen;                  /* the store's clock when it last played: larger is newer */
	int together_games, together_wins, against_games, against_wins;
};

struct ae_party_store
{
	int count;
	long clock;
	struct ae_party_record record[AE_STORE_RECORDS];
};

void ae_party_store_clear(struct ae_party_store *store);
/* the record of id, NULL none */
struct ae_party_record *ae_party_store_find(struct ae_party_store *store, char const *id);
/* the record of id touched (made when new): newest now. NULL only for an empty id */
struct ae_party_record *ae_party_store_touch(struct ae_party_store *store, char const *id, char const *name, int kind, int tier);
/* a game with the player: together (on one team) or against; won: 1 the player's side won. A bot that reaches
   AE_STORE_AUTO_FAVOURITE games together becomes a favourite when there is room */
void ae_party_store_game(struct ae_party_store *store, char const *id, int together, int won);
/* favourite on or off: 0 when the saved list is full (or no such record) */
int ae_party_store_set_saved(struct ae_party_store *store, char const *id, int saved);
/* blocked on or off; a block also takes the favourite off */
int ae_party_store_set_blocked(struct ae_party_store *store, char const *id, int blocked);
/* the records to offer as recent players: newest first, not blocked, not in `exclude` (a party), up to max; the count */
int ae_party_store_recent(struct ae_party_store const *store, struct ae_party const *exclude, struct ae_party_record const **out, int max);
/* the favourites, newest first */
int ae_party_store_saved(struct ae_party_store const *store, struct ae_party_record const **out, int max);
/* a record as a party member */
void ae_party_store_member(struct ae_party_record const *record, struct ae_party_member *member);

/* the store as text and back; the length (-1: too small) and the records read (a bad line is skipped; the newest AE_STORE_RECORDS) */
int ae_party_store_write(struct ae_party_store const *store, char *text, int size);
int ae_party_store_read(struct ae_party_store *store, char const *text);

#endif
