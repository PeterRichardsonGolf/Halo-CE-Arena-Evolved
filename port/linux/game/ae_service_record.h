/*
AE_SERVICE_RECORD.H

The service record (0.7.6): what this machine has seen of each player over the games it played, and the rivals a local
profile has set. Pure code (no engine headers): port/linux/tests/ae_service_record_test.c runs it; ae_glue_service_record.c
keeps it in a file and feeds it from the post-game report, ae_screen_record.c shows it.

- It is this machine's view only. A game adds to every player of its report, by that player's key (until the player key of
  the social layer exists, the key is the name made lower case and cut to the same shape: Goat and goat are one player),
  from the numbers of that game's report: games, wins, kills, deaths, assists, each medal's count, and the same five
  numbers split by the game's kind (slayer, flag, ball, hill, race, bomb, other). Nothing is shared.
- A game is added once, at its post-game (ae_sr_record_game), never again.
- Rivals belong to a local profile (its key): up to AE_SR_RIVALS_PER_OWNER players. For each rival the record counts the
  games the two played together and the kills of the owner on the rival and the rival's on the owner, from the owner's
  summary (the host's eight most traded with: a rival outside them counts the game and no kills).
- At most AE_SR_PLAYERS players are kept; a new one past that takes the place of the one seen longest ago that is not an
  owner or a rival.

The file (one small text file in the save root's private folder), lines in this order:

	# AE service record
	version = 1
	clock = 17
	player = goat | Goat | 17 | 12 5 40 31 9 | 12 5 40 31 9 0 0 0 0 0 ... | 3:2,7:1
	rival = goat | tiger | Tiger
	versus = goat | tiger | 4 11 6

player: key, name as last seen, the game it was last seen in (the clock), games wins kills deaths assists, the same five for
each of the AE_SR_MODES kinds in order, then the medals (id:count). rival: the owner's key, the rival's key and name.
versus: the owner's key, the rival's key, games, the owner's kills of the rival, the rival's of the owner.

A reader reads what it knows of any version: a line of a kind it does not know is skipped, a field past the ones it knows is
ignored, and a version past its own sets newer so the glue does not write the file back over what it could not read. A file
without the header, or with a line that is not "key = value", is refused whole (the glue keeps it aside as <file>.bad).
Names are written with "|" and control characters turned to "?".
*/

#ifndef __AE_SERVICE_RECORD_H
#define __AE_SERVICE_RECORD_H

#include <stddef.h>

#include "ae_medals.h"
#include "ae_pgcr_rules.h"

enum
{
	AE_SR_VERSION = 1,
	AE_SR_PLAYERS = 256,
	AE_SR_KEY = 48,
	AE_SR_NAME = 48,
	/* the game kinds (ae_pgr_mode's order) */
	AE_SR_MODES = 7,
	AE_SR_RIVALS_PER_OWNER = 8,
	AE_SR_OWNERS = 4,
	AE_SR_RIVALS = AE_SR_OWNERS * AE_SR_RIVALS_PER_OWNER,
	/* the file's size, at most */
	AE_SR_TEXT = 256 * 1024
};

struct ae_sr_line
{
	long games, wins, kills, deaths, assists;
};

struct ae_sr_player
{
	char key[AE_SR_KEY];
	char name[AE_SR_NAME];
	unsigned long seen;
	struct ae_sr_line total;
	struct ae_sr_line mode[AE_SR_MODES];
	unsigned long medals[AE_MEDAL_COUNT];
};

struct ae_sr_rival
{
	char owner[AE_SR_KEY];
	char key[AE_SR_KEY];
	char name[AE_SR_NAME];
};

struct ae_sr_versus
{
	char owner[AE_SR_KEY];
	char rival[AE_SR_KEY];
	long games, kills, deaths;
};

struct ae_service_record
{
	unsigned long clock;
	int newer;
	int player_count;
	struct ae_sr_player player[AE_SR_PLAYERS];
	int rival_count;
	struct ae_sr_rival rival[AE_SR_RIVALS];
	int versus_count;
	struct ae_sr_versus versus[AE_SR_RIVALS];
};

/* one player of a finished game, as the report holds him */
struct ae_sr_game_player
{
	char name[AE_SR_NAME];
	int slot;
	int me;         /* a player of this machine (an owner of rivals) */
	int won;
	int quit;
	long kills, deaths, assists;
	int medal_count;
	struct ae_pgcr_medal medals[AE_PGCR_SUMMARY_MEDALS];
	/* the summary's kills against others, by slot (none: the host sent no summary) */
	int versus_count;
	struct ae_pgcr_versus versus[AE_PGCR_SUMMARY_VERSUS];
};

struct ae_sr_game
{
	int mode;       /* 0 .. AE_SR_MODES - 1 */
	int count;
	struct ae_sr_game_player player[AE_PGCR_PLAYERS];
};

void ae_sr_init(struct ae_service_record *record);
/* a name's key: lower case, "|" and control characters gone, cut to the key's size; "" for no name */
void ae_sr_key(char const *name, char *key, size_t size);
/* a game's kind's word: "SLAYER" ... */
char const *ae_sr_mode_name(int mode);

/* the player of this key, NULL for none */
struct ae_sr_player const *ae_sr_find(struct ae_service_record const *record, char const *key);
/* a finished game added: 1, or 0 for no player in it */
int ae_sr_record_game(struct ae_service_record *record, struct ae_sr_game const *game);

/* the owner's rival of that name (compared by key): 1 on, 0 off. Setting one on the full list refuses (0) */
int ae_sr_is_rival(struct ae_service_record const *record, char const *owner, char const *name);
int ae_sr_rival_set(struct ae_service_record *record, char const *owner, char const *name, int on);
/* the owner's versus of a rival, NULL for none */
struct ae_sr_versus const *ae_sr_versus_of(struct ae_service_record const *record, char const *owner, char const *rival);

/* the file's text into buffer: its length, -1 when it does not fit */
long ae_sr_write(struct ae_service_record const *record, char *buffer, long size);
/* a file's text read (the record made empty first): 1 taken, 0 refused */
int ae_sr_read(struct ae_service_record *record, char const *text);

#endif
