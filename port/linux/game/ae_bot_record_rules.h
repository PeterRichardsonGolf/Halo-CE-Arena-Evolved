/*
AE_BOT_RECORD_RULES.H

(0.9.4) Bot service records for AE TOURNAMENT (notes/specs/2026-11-15-bot-service-records.md, section 2). Each bot name of
the offline career keeps a record of the counted tournament games it played on this PC, and each local profile a record
against each bot it met. Pure code (no engine headers): port/linux/tests/ae_bot_record_rules_test.c runs it;
ae_glue_bot_records.c keeps it in bot_records.txt in the private folder beside career.txt (which it never touches).

A BOT (keyed by its name made lower case, ae_sr_key: the same name always plays the same style, so the name is the bot):
its name as last seen, the record clock of its first and last game (the clock counts the tournament games recorded on
this PC), the playlist of the first meeting, its games, wins and ties (losses are the rest: its side's result; a team
game its team's, a free for all its place in the top half a win), kills, deaths, assists, best spree, its last shooting
and movement steps, the lowest and highest tier it played at, and up to AE_BOT_RECORD_WEAPONS weapons with their kills
(merged by name, the most kills kept). Then, as last seen, its play style, favourite weapon and a bot rank (none earned:
the rank 1 .. 50 is to come).

(rc2, version 2) botx: the key and the numbers of ae_record_stats.h (8 groups); botkinds: the key, then seven groups of games wins
kills deaths assists in the report's kind order (SLAYER FLAG BALL HILL RACE BOMB OTHER); botmedals: the key and id:count pairs
(the medals split over lines of about 400 bytes); botmap: the key, a map, its games, wins, kills and deaths (at most
AE_BOT_RECORD_MAPS). Each follows the bot's own line; a version 1 reader skips them (and, version 2 being newer to it, never
writes the file back). Lines of a bot with no bot line before them are skipped. A version 1 file read here has none: the
numbers stay empty (a dash on the screens) and grow with the games counted from now on.

A VERSUS line (a profile and a bot): games against (a team game on the other team; a free for all always), the profile's
wins and the bot's of those (placed above: the team won / the higher place; level is neither), games with (on the
profile's team) and how many of those that team won, the profile's kills on the bot and the bot's on the profile, the
clock of the last game together, and that LAST GAME (playlist, map, the bot's result, kills, deaths, assists, spree, its
kills on the profile, its free for all place or -1).

The file, plain text, one line for each fact:

	# AE bot records
	version = 2
	clock = 41
	bot = rook7 | Rook7 | 3 41 team-slayer | 23 12 1 187 171 64 9 | 4 5 9 13 1 3 -1 | battle_rifle:61,assault_rifle:22
	versus = chief | rook7 | 14 6 8 9 6 31 23 41
	lastgame = chief | rook7 | team-slayer | prisoner | 0 12 8 3 5 4 -1
	botx = rook7 | 5 11 27660 | 852 340 | ... (ae_rs_stats_write's 8 groups)
	botkinds = rook7 | 14 8 123 107 41 | 6 3 46 44 15 | 3 1 18 20 8 | 0 0 0 0 0 | 0 0 0 0 0 | 0 0 0 0 0 | 0 0 0 0 0
	botmedals = rook7 | 3:2,7:1,40:12
	botmap = rook7 | prisoner | 6 4 55 46
	checksum = 5e0c11a2

bot: key | name | first last first_playlist | games wins ties kills deaths assists best_spree | shooting movement tier_low
tier_high style favourite rank | weapons (name:kills, ","-separated). versus: profile | bot | against you_won it_won with
with_won your_kills its_kills last. lastgame: profile | bot | playlist | map | result kills deaths assists spree its_kills
place. In names "|", ",", ":" and control characters are made "?".

The rules are career.txt's (ae_career_record.h): the checksum (FNV-1a of every byte before its line) is the last line; a file
whose checksum does not match, or that has none, is read as empty and flagged edited (the glue keeps it aside as .bad); a
reader skips lines of a kind it does not know and fields past the ones it knows; a version past its own sets newer (the glue
then never writes the file back); a key twice, or a profile and bot twice, is an edited file. A versus or lastgame line of a
bot with no bot line before it is skipped.

Bounds: AE_BOT_RECORD_BOTS bots (the name pool is 451: a new bot past them replaces the one with the oldest last game, the
fewest games on a tie, and its versus lines go with it) and AE_BOT_RECORD_VERSUS versus lines (a new one past them replaces
the oldest).
*/

#ifndef __AE_BOT_RECORD_RULES_H
#define __AE_BOT_RECORD_RULES_H

#include <stddef.h>

#include "ae_medals.h"
#include "ae_pgcr_rules.h"
#include "ae_record_stats.h"

enum
{
	AE_BOT_RECORD_VERSION = 2,
	AE_BOT_RECORD_KEY = 48,
	AE_BOT_RECORD_ID = 32,
	AE_BOT_RECORD_WEAPON = 24,
	AE_BOT_RECORD_BOTS = 512,
	AE_BOT_RECORD_VERSUS = 1024,
	AE_BOT_RECORD_WEAPONS = 5,
	/* (rc2) the maps a bot keeps and the game kinds it counts */
	AE_BOT_RECORD_MAPS = 8,
	AE_BOT_RECORD_KINDS = AE_RS_KINDS,
	/* a game's facts: the weapons a bot brings (the summary's six), the local profiles */
	AE_BOT_GAME_WEAPONS = 6,
	AE_BOT_GAME_PROFILES = 4,
	/* a line of the file, its newline in it, at most (the reader refuses a longer one). Version 1's reader refused one of
	AE_BOT_RECORD_OLD_LINE: the writer keeps its lines under that (the new kinds' too, so an older build skips them rather than
	refusing the file) */
	AE_BOT_RECORD_LINE = 1024,
	AE_BOT_RECORD_OLD_LINE = 448,
	/* (every table full of the longest keys, with every lastgame line and the version 2 lines: about 2.1 MB with every number at ten digits) */
	AE_BOT_RECORD_TEXT = 3 * 1024 * 1024,
	/* the rank: none earned (the bot rank 1 .. 50 is to come) */
	AE_BOT_RECORD_NO_RANK = -1,
	/* a bot's result in a game (and a lastgame line's) */
	AE_BOT_RECORD_WIN = 0,
	AE_BOT_RECORD_LOSS = 1,
	AE_BOT_RECORD_TIE = 2,
	/* the RIVAL tag: this many games against and within this many wins of each other */
	AE_BOT_RIVAL_GAMES = 10,
	AE_BOT_RIVAL_WITHIN = 2,
	/* ae_bot_records_read's answers */
	AE_BOT_RECORD_READ_BAD = 0,
	AE_BOT_RECORD_READ_OK = 1,
	AE_BOT_RECORD_READ_EDITED = 2
};

struct ae_bot_weapon
{
	char name[AE_BOT_RECORD_WEAPON];
	long kills;
};

/* (rc2) a bot's games, wins and kills, deaths, assists by a game kind */
struct ae_bot_kind
{
	long games, wins, kills, deaths, assists;
};

struct ae_bot_record
{
	char key[AE_BOT_RECORD_KEY];
	char name[AE_BOT_RECORD_KEY];
	long first, last;
	char first_playlist[AE_BOT_RECORD_ID];
	long games, wins, ties, kills, deaths, assists, best_spree;
	int shooting, movement, tier_low, tier_high;
	/* as last seen: its play style (AE_STYLE_* preset, -1 none), its favourite weapon (AE_STYLE_WEAPON_*), its rank */
	int style, favourite, rank;
	int weapon_count;
	struct ae_bot_weapon weapons[AE_BOT_RECORD_WEAPONS];
	/* (rc2, file version 2) the shared numbers (ae_record_stats.h) of the games counted since, the games by kind, the medals
	by id and the maps; all empty in a record written by version 1 */
	struct ae_rs_stats x;
	struct ae_bot_kind kind[AE_BOT_RECORD_KINDS];
	unsigned long medals[AE_MEDAL_COUNT];
	int map_count;
	struct ae_rs_map map[AE_BOT_RECORD_MAPS];
};

struct ae_bot_lastgame
{
	int has;
	char playlist[AE_BOT_RECORD_ID];
	char map[AE_BOT_RECORD_KEY];
	int result;
	long kills, deaths, assists, spree, its_kills;
	int place;
};

struct ae_bot_versus
{
	char profile[AE_BOT_RECORD_KEY];
	char bot[AE_BOT_RECORD_KEY];
	long against, you_won, it_won, with, with_won, your_kills, its_kills, last;
	struct ae_bot_lastgame lastgame;
};

struct ae_bot_records
{
	int newer;
	int edited;
	long clock;
	int count;
	struct ae_bot_record bot[AE_BOT_RECORD_BOTS];
	int versus_count;
	struct ae_bot_versus versus[AE_BOT_RECORD_VERSUS];
};

/* ---- one counted game's facts */

/* a local profile of the game */
struct ae_bot_game_profile
{
	char key[AE_BOT_RECORD_KEY];
	int team;       /* 0 red, 1 blue; a free for all: -1 */
	int place;      /* a free for all's zero-based place (a shared place its best), -1 in a team game */
};

/* a bot of the game, and its kills against each profile (by the profile's index in the game) */
struct ae_bot_game_bot
{
	char name[AE_BOT_RECORD_KEY];
	int tier, shooting, movement, style, favourite;
	int team;       /* 0 red, 1 blue; a free for all: -1 */
	int place;      /* a free for all's place, -1 in a team game */
	long kills, deaths, assists, spree;
	int weapon_count;
	struct ae_bot_weapon weapons[AE_BOT_GAME_WEAPONS];
	/* (rc2) the game's new numbers (ae_rs_game_make; has_x 0: none, the record's counted games stay) and its medals */
	int has_x;
	struct ae_rs_game x;
	int medal_count;
	struct ae_pgcr_medal medals[AE_PGCR_SUMMARY_MEDALS];
	long kills_on[AE_BOT_GAME_PROFILES];    /* its kills on profile n */
	long killed_by[AE_BOT_GAME_PROFILES];   /* profile n's kills on it */
};

struct ae_bot_game
{
	char playlist[AE_BOT_RECORD_ID];
	char map[AE_BOT_RECORD_KEY];
	int mode;       /* (rc2) the game's kind: AE_RS_KIND_ */
	int teams;      /* 1 a team game, 0 a free for all */
	int players;    /* a free for all's ranked players (the top half wins) */
	long red_score, blue_score;
	int profile_count;
	struct ae_bot_game_profile profile[AE_BOT_GAME_PROFILES];
};

void ae_bot_records_init(struct ae_bot_records *records);
/* a bot's or profile's key from a name: ae_sr_key's (lower case, "|" and control characters gone), "" for no name */
void ae_bot_record_key(char const *name, char *key, size_t size);
/* a bot's record by name (any letter case), NULL for none */
struct ae_bot_record *ae_bot_record_find(struct ae_bot_records *records, char const *name);
/* a profile's versus line with a bot (names or keys), NULL for none */
struct ae_bot_versus *ae_bot_record_versus(struct ae_bot_records *records, char const *profile, char const *bot);
/* a bot's result in a game: its team's (the higher score wins, the same is a tie), or a free for all's top half a win */
int ae_bot_record_result(struct ae_bot_game const *game, int team, int place);
/* one counted game into the records: the clock moved once, each bot's totals, tiers, weapons and identity, and each profile's
   versus line with it (made when there is none, the oldest replaced in a full table). The bots added (0: none had a name) */
int ae_bot_records_add_game(struct ae_bot_records *records, struct ae_bot_game const *game, struct ae_bot_game_bot const *bots,
	int bot_count);
/* a bot's record and its versus lines taken away (1 when there was one); every record when name is NULL (the count) */
int ae_bot_records_forget(struct ae_bot_records *records, char const *name);

/* kills per death x 100 (the kills x 100 for no deaths) */
long ae_bot_record_kd100(long kills, long deaths);
/* whether a versus line earns the RIVAL tag (AE_BOT_RIVAL_GAMES against, within AE_BOT_RIVAL_WITHIN wins) */
int ae_bot_record_is_rival(struct ae_bot_versus const *versus);
/* the rival line (spec 4.3): its head words ("FIRST MEETING", "TEAMMATE", "YOU LEAD 7-3", "IT LEADS 8-6", "EVEN 5-5") and its
   sentence; the kills sentence alone (the post-game pane's) into kills_sentence. Any of the three may be NULL */
void ae_bot_record_rival_line(struct ae_bot_versus const *versus, char *head, size_t head_size, char *sentence,
	size_t sentence_size, char *kills_sentence, size_t kills_size);

/* the file's text into buffer: its length, -1 when it does not fit */
long ae_bot_records_write(struct ae_bot_records const *records, char *buffer, long size);
/* a file's text read (the records made empty first): AE_BOT_RECORD_READ_OK, _EDITED (checksum wrong or missing, a line twice:
   empty) or _BAD (no header, a line too long: empty) */
int ae_bot_records_read(struct ae_bot_records *records, char const *text);

/* (CAREER's BOTS list) the bots profile has met (games against or with it), sorted: GAMES (games together, most first,
   then the last met), LAST (the last met first), RECORD (the bot's lead over the profile, biggest first, then games
   together); then by name. Their indices into records->bot, at most `most`: how many */
enum { AE_BOT_RECORD_SORT_GAMES, AE_BOT_RECORD_SORT_LAST, AE_BOT_RECORD_SORT_RECORD, AE_BOT_RECORD_SORTS };
int ae_bot_records_list(struct ae_bot_records const *records, char const *profile, int sort, int *out, int most);
/* a sort's word for the list's head: "GAMES", "LAST MET", "RECORD" */
char const *ae_bot_record_sort_name(int sort);

#endif
