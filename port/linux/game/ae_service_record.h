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
	version = 2
	clock = 17
	player = goat | Goat | 17 | 12 5 40 31 9 | 12 5 40 31 9 0 0 0 0 0 ... | 3:2,7:1
	playerx = goat | 2 14 5880 | 1200 480 | ... (ae_rs_stats_write's 8 groups) | battle rifle:41,sniper rifle:22
	map = goat | blood gulch | 3 1 60 7
	lastgame = goat | flag | prisoner | 1 11 7 3 5 31 | battle rifle
	rival = goat | tiger | Tiger
	versus = goat | tiger | 4 11 6

player: key, name as last seen, the game it was last seen in (the clock), games wins kills deaths assists, the same five for
each of the AE_SR_MODES kinds in order, then the medals (id:count). rival: the owner's key, the rival's key and name.
versus: the owner's key, the rival's key, games, the owner's kills of the rival, the rival's of the owner.
(rc2, version 2) playerx: the key, the numbers of ae_record_stats.h (8 groups) and the top weapons (name:kills); map: the key,
a map, its games, wins, kills and deaths (at most AE_SR_MAPS a player); lastgame: the key, the kind's word, the map, then the
result (0 win, 1 loss, 2 tie), kills, deaths, assists, best spree and medals earned, then the weapon of most kills. These lines follow their player's. A
version 1 reader skips them (they are lines of a kind it does not know) and never writes the file back (version 2 is newer
to it), so nothing is lost; a version 1 file read by this build has none of them: those numbers stay empty and show a dash
until a game is counted.

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
#include "ae_record_stats.h"

enum
{
	AE_SR_VERSION = 2,
	AE_SR_PLAYERS = 256,
	AE_SR_KEY = 48,
	AE_SR_NAME = 48,
	/* the game kinds (ae_pgr_mode's order) */
	AE_SR_MODES = 7,
	AE_SR_RIVALS_PER_OWNER = 8,
	AE_SR_OWNERS = 4,
	AE_SR_RIVALS = AE_SR_OWNERS * AE_SR_RIVALS_PER_OWNER,
	/* (rc2) the maps and top weapons a player keeps (ae_record_stats.h's rules) */
	AE_SR_MAPS = AE_RS_MAPS,
	AE_SR_WEAPONS = AE_RS_WEAPONS,
	/* the file's size, at most (256 players of 16 maps, 5 weapons and the numbers: about 300 KB) */
	AE_SR_TEXT = 1024 * 1024
};

struct ae_sr_line
{
	long games, wins, kills, deaths, assists;
};

/* (rc2) a player's last game as the record keeps it: its kind, map, result (0 win, 1 loss, 2 tie) and numbers */
struct ae_sr_last
{
	int has;
	int mode;
	char map[AE_RS_MAP_NAME];
	int result;
	long kills, deaths, assists, spree, medals;
	/* the weapon of its most kills ("" for none) */
	char weapon[AE_RS_WEAPON_NAME];
};

struct ae_sr_player
{
	char key[AE_SR_KEY];
	char name[AE_SR_NAME];
	unsigned long seen;
	struct ae_sr_line total;
	struct ae_sr_line mode[AE_SR_MODES];
	unsigned long medals[AE_MEDAL_COUNT];
	/* (rc2, file version 2) the numbers of the games since: time, shots, damage, objectives (ae_record_stats.h), the top
	weapons, the maps and the last game; all empty in a record written by version 1 */
	struct ae_rs_stats x;
	int weapon_count;
	struct ae_rs_weapon weapons[AE_SR_WEAPONS];
	int map_count;
	struct ae_rs_map map[AE_SR_MAPS];
	struct ae_sr_last last;
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
	/* (rc2) the game's new numbers (ae_rs_game_make): has_x 0 for a caller with none (the record's counted games stay as
	they were), its top weapons by name and the medals it earned */
	int has_x;
	struct ae_rs_game x;
	int weapon_count;
	struct ae_rs_weapon weapons[AE_PGCR_SUMMARY_WEAPONS];
	long medals_earned;
};

struct ae_sr_game
{
	int mode;       /* 0 .. AE_SR_MODES - 1 */
	char map[AE_RS_MAP_NAME];
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
