/*
AE_GLUE_BOT_RECORDS.C

The bot service records' store and facts (ae_glue_bot_records.h).
*/

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cseries.h"
#include "game/game.h"
#include "game/game_engine.h"
#include "game/players.h"
#include "memory/data.h"
#include "tag_files/tag_files.h"
#include "bots.h"
#include "ae_bots_seat.h"
#include "ae_bots_style_rules.h"
#include "ae_dev_events.h"
#include "ae_bot_record_demo.h"
#include "ae_dev_cmd_rules.h"
#include "ae_glue_bot_records.h"
#include "ae_glue_social.h"
#include "ae_glue_social_game.h"
#include "ae_pgcr_data.h"
#include "ae_pgcr_screen_rules.h"
#include "ae_pgcr_stats.h"
#include "ae_postgame.h"
#include "ae_scoreboard.h"
#include "ae_screen_bot_record.h"
#include "ae_screen_pgcr.h"
#include "ae_social_names_rules.h"
#include "ae_social_playlists.h"
#include "ae_social_lobby_rules.h"
#include "ae_weapon_ids.h"
#include "ae_weapon_registry.h"
#include "../src/ae_private_files.h"

void platform_log(char const *format, ...);
unsigned long system_milliseconds(void);
/* game_engine.c's ranking (as ae_glue_postgame.c reads it) */
long populate_statistic_buffer(struct ae_scoreboard_stat *statistic_buffer, long statistic, boolean inverse);

#define FILE_NAME "bot_records.txt"
#define RANKING 0
#define PLACE_TIED 0x80000000UL
#define MAXIMUM_RANKED 128

static struct
{
	struct ae_bot_records records;
	char text[AE_BOT_RECORD_TEXT];
	int read;
} store;

/* DEV MODE's made-up store */
static struct
{
	int on;
	struct ae_bot_records records;
} demo;

/* a counted game waiting for the report's summaries */
static struct
{
	int waiting;
	unsigned long since;
	struct ae_social_plan plan;
	char profile[AE_BOT_GAME_PROFILES][AE_BOT_RECORD_KEY];
	int locals;
} pending;

struct ae_bot_records *ae_bot_records_store(void)
{
	long length;
	int result;

	if (store.read)
		return &store.records;
	store.read = 1;
	length = ae_private_file_read(FILE_NAME, store.text, sizeof(store.text));
	if (length < 0 && length != -2)
	{
		ae_bot_records_init(&store.records);
		platform_log("ae botrecord: %s: none yet", FILE_NAME);
		return &store.records;
	}
	result = length == -2 ? AE_BOT_RECORD_READ_BAD : ae_bot_records_read(&store.records, store.text);
	if (result != AE_BOT_RECORD_READ_OK)
	{
		ae_bot_records_init(&store.records);
		if (ae_private_file_set_aside(FILE_NAME))
			platform_log("ae botrecord: %s refused: kept as %s.bad, a fresh one started", FILE_NAME, FILE_NAME);
		else
			platform_log("ae botrecord: %s refused (not kept aside), a fresh one started", FILE_NAME);
	}
	else
		platform_log("ae botrecord: %s: %d bots, %d versus lines, clock %ld%s", FILE_NAME, store.records.count,
			store.records.versus_count, store.records.clock, store.records.newer ? " (a newer version: never written back)" : "");
	return &store.records;
}

int ae_bot_records_store_save(void)
{
	long length;
	struct ae_bot_records *records = ae_bot_records_store();

	if (records->newer)
	{
		platform_log("ae botrecord: the file is of a newer version: not written");
		return 0;
	}
	length = ae_bot_records_write(records, store.text, sizeof(store.text));
	if (length < 0 || !ae_private_file_write(FILE_NAME, store.text, length))
	{
		platform_log("ae botrecord: %s could not be written", FILE_NAME);
		return 0;
	}
	return 1;
}

/* a kill's weapon name for the record: a pack weapon's registry display name ("HEAVY REVOLVER"; spec open question 7: a
   pack weapon's damage tag can carry its base's name), else the report's name from the tag's path (ae_pgr_weapon_name).
   The pack weapon is the registry weapon whose tag's folder holds the damage tag, the deepest such folder */
static void weapon_name(long damage_tag, char *out, size_t size)
{
	char const *path = damage_tag != NONE ? tag_get_name(damage_tag) : NULL;
	struct ae_weapon_registry_row const *best = NULL, *pack = NULL;
	size_t best_length = 0;
	unsigned int index;
	int packs = 0;

	out[0] = 0;
	if (!path || !size)
		return;
	for (index = 0; index < ae_weapon_registry_count(); index++)
	{
		struct ae_weapon_registry_row const *row = ae_weapon_registry_at(index);
		long definition = row ? ae_weapon_id_definition(row->id) : NONE;
		char const *weapon = definition != NONE ? tag_get_name(definition) : NULL;
		char const *slash = weapon ? strrchr(weapon, '\\') : NULL;
		size_t folder = slash ? (size_t)(slash - weapon) + 1 : 0;
		char const *root_end = weapon && !strncmp(weapon, "ae\\packs\\", 9) ? strchr(weapon + 9, '\\') : NULL;

		if (!root_end || !row->display || !row->display[0])
			continue;
		/* (its pack's folder, ae/packs/<pack>/: the weapon when it is the only one of its pack the tag is in) */
		if (!strncmp(path, weapon, (size_t)(root_end - weapon) + 1) && (!pack || pack->id != row->id))
		{
			pack = row;
			packs++;
		}
		if (strncmp(path, weapon, folder) || folder <= best_length)
			continue;
		best = row;
		best_length = folder;
	}
	if (!best && packs == 1)
		best = pack;
	if (best)
	{
		size_t at;

		for (at = 0; best->display[at] && at + 1 < size; at++)
			out[at] = (char)toupper((unsigned char)best->display[at]);
		out[at] = 0;
		return;
	}
	ae_pgr_weapon_name(path, out, size);
}

/* the ranking's place of a player (zero based, a shared place its best); -1 when not ranked */
static int place_of(struct ae_scoreboard_stat const *ranked, long count, long player_index)
{
	long index;

	for (index = 0; index < count; index++)
		if (ranked[index].player_index == player_index)
			return (int)(ranked[index].place & ~PLACE_TIED);
	return -1;
}

static void record_now(struct ae_social_plan const *plan, char profiles[][AE_BOT_RECORD_KEY], int locals)
{
	static struct ae_scoreboard_stat ranked[MAXIMUM_RANKED];
	static struct ae_bot_game_bot bots[BOTS_MAXIMUM];
	char names[AE_SOCIAL_SEATS][AE_SOCIAL_LOBBY_NAME];
	long profile_slot[AE_BOT_GAME_PROFILES];
	struct ae_bot_game game;
	struct ae_bot_records *records;
	struct data_iterator iterator;
	struct player_datum *player;
	long count;
	int seats = plan->roster.count - plan->humans, bot_count = 0, local, added;

	if (seats <= 0 || !ae_social_lobby_names(plan->seed, seats, names))
		return;
	memset(&game, 0, sizeof(game));
	snprintf(game.playlist, sizeof(game.playlist), "%s", plan->playlist);
	snprintf(game.map, sizeof(game.map), "%s", plan->map);
	game.teams = game_engine_has_teams() ? 1 : 0;
	game.red_score = game.teams ? game_engine_get_team_score(0) : 0;
	game.blue_score = game.teams ? game_engine_get_team_score(1) : 0;
	count = populate_statistic_buffer(ranked, RANKING, FALSE);
	game.players = (int)count;
	/* (each local player whose game was saved: a profile of the game) */
	for (local = 0; local < locals && local < AE_BOT_GAME_PROFILES; local++)
	{
		long index = profiles[local][0] && local_player_exists((short)local) ? local_player_get_player_index((short)local) :
			NONE;
		struct player_datum *human = index != NONE ? player_try_and_get(index) : NULL;
		struct ae_bot_game_profile *who;

		if (!human)
			continue;
		who = &game.profile[game.profile_count];
		profile_slot[game.profile_count] = DATUM_INDEX_TO_ABSOLUTE_INDEX(index);
		game.profile_count++;
		snprintf(who->key, sizeof(who->key), "%s", profiles[local]);
		who->team = game.teams ? (int)PIN(human->team_index, 0, 1) : -1;
		who->place = game.teams ? -1 : place_of(ranked, count, index);
	}
	data_iterator_new(&iterator, player_data);
	while ((player = (struct player_datum *)data_iterator_next(&iterator)) != NULL && bot_count < BOTS_MAXIMUM)
	{
		long slot = DATUM_INDEX_TO_ABSOLUTE_INDEX(iterator.datum_index);
		long seat = (long)player->network_player_data.machine_index - BOTS_FIRST_MACHINE;
		struct ae_pgcr_summary const *summary = ae_pgcr_data_get((int)slot);
		struct ae_social_seat const *roster;
		struct ae_bot_game_bot *bot;
		struct ae_bots_style style;
		int index;

		if (seat < 0 || seat >= BOTS_MAXIMUM || seat >= seats || !names[seat][0] || player->quit_out_of_game)
			continue;
		roster = &plan->roster.seat[plan->humans + seat];
		bot = &bots[bot_count++];
		memset(bot, 0, sizeof(*bot));
		snprintf(bot->name, sizeof(bot->name), "%s", names[seat]);
		bot->tier = roster->tier;
		bot->shooting = roster->shooting;
		bot->movement = roster->movement;
		bot->style = -1;
		if (ae_bots_seat_style_get(seat, &style))
		{
			bot->style = style.preset;
			bot->favourite = style.weapon;
		}
		bot->team = game.teams ? (int)PIN(player->team_index, 0, 1) : -1;
		bot->place = game.teams ? -1 : place_of(ranked, count, iterator.datum_index);
		bot->kills = player->statistics.kills[0];
		bot->deaths = player->statistics.deaths;
		bot->assists = player->statistics.assists[0];
		bot->spree = ae_postgame_best_spree(iterator.datum_index);
		if (bot->spree < 0)
			bot->spree = summary ? summary->best_spree : 0;
		for (index = 0; summary && index < summary->weapon_count && index < AE_BOT_GAME_WEAPONS; index++)
		{
			struct ae_bot_weapon *weapon = &bot->weapons[bot->weapon_count];

			weapon_name(summary->weapons[index].tag, weapon->name, sizeof(weapon->name));
			weapon->kills = summary->weapons[index].kills;
			if (weapon->name[0] && weapon->kills > 0)
				bot->weapon_count++;
		}
		for (index = 0; index < game.profile_count; index++)
		{
			bot->kills_on[index] = ae_pgcr_stats_kills((int)slot, (int)profile_slot[index]);
			bot->killed_by[index] = ae_pgcr_stats_kills((int)profile_slot[index], (int)slot);
		}
	}
	records = ae_bot_records_store();
	added = ae_bot_records_add_game(records, &game, bots, bot_count);
	if (!added)
		return;
	if (!ae_bot_records_store_save())
		platform_log("ae botrecord: the game stays in memory: the next game writes it");
	platform_log("ae botrecord: game added (%s, %d bots, %d profile%s)%s", game.playlist, added, game.profile_count,
		game.profile_count == 1 ? "" : "s", ae_pgcr_data_have_summary() ? "" : "; no report summaries: weapons and kills 0");
	ae_dev_event("bot_record_game", "\"playlist\":\"%s\",\"bots\":%d,\"profiles\":%d", game.playlist, added,
		game.profile_count);
}

void ae_bot_records_game_counted(struct ae_social_plan const *plan, char const *const *profiles, int locals)
{
	int local;

	/* (a real game is the file's: its post-game pages read it, never the made-up store; record_now writes the file's
	records only, the made-up store is never written) */
	ae_bot_records_demo_end("a real game counted");
	memset(&pending, 0, sizeof(pending));
	pending.plan = *plan;
	pending.locals = locals < AE_BOT_GAME_PROFILES ? locals : AE_BOT_GAME_PROFILES;
	for (local = 0; local < pending.locals; local++)
		if (profiles[local])
			snprintf(pending.profile[local], sizeof(pending.profile[local]), "%s", profiles[local]);
	pending.waiting = 1;
	pending.since = system_milliseconds();
	ae_bot_records_tick();
}

void ae_bot_records_tick(void)
{
	if (!pending.waiting)
		return;
	if (!ae_pgcr_data_have_summary() && system_milliseconds() - pending.since < AE_BOT_RECORDS_WAIT_MS)
		return;
	pending.waiting = 0;
	if (!game_engine_running())
	{
		platform_log("ae botrecord: the game was gone before its report: not recorded");
		return;
	}
	record_now(&pending.plan, pending.profile, pending.locals);
}

int ae_bot_records_waiting(void)
{
	return pending.waiting;
}

struct ae_bot_records *ae_bot_records_active(void)
{
	return demo.on ? &demo.records : ae_bot_records_store();
}

int ae_bot_records_demo_on(void)
{
	return demo.on;
}

void ae_bot_records_demo_end(char const *why)
{
	if (!demo.on)
		return;
	demo.on = 0;
	platform_log("ae botrecord: the made-up store is off (%s): the screens read %s again", why, FILE_NAME);
}

/* the profile key of local player `local` (0 .. 3) as the career keeps it: "" for a guest */
static void local_profile(int local, char *key, size_t size)
{
	key[0] = 0;
	if (!ae_social_screen_key((short)local, key, size))
		key[0] = 0;
}

/* a DEV command's name word (the parser lower-cases words) as the name pool spells it, else with a capital first */
static void dev_name(char const *word, char *out, size_t size)
{
	char key[AE_BOT_RECORD_KEY], each_key[AE_BOT_RECORD_KEY];
	int index, count = ae_social_names_count();

	ae_bot_record_key(word, key, sizeof(key));
	for (index = 0; index < count; index++)
	{
		ae_bot_record_key(ae_social_names_get(index), each_key, sizeof(each_key));
		if (key[0] && !strcmp(key, each_key))
		{
			snprintf(out, size, "%s", ae_social_names_get(index));
			return;
		}
	}
	snprintf(out, size, "%s", word);
	if (out[0])
		out[0] = (char)toupper((unsigned char)out[0]);
}

/* the local players with a profile, player 1 first: VS YOU's lines (a profile's emblem colour is its player's chip) */
static void page_profiles(struct ae_bot_page_source *source)
{
	static unsigned int const chips[4] = { 0x5FC8ECFFu, 0xE0B04CFFu, 0x8FD18AFFu, 0xC79BF2FFu };
	int local;

	for (local = 0; local < 4 && source->profiles < AE_BOT_PAGE_PROFILES; local++)
	{
		int index = source->profiles;

		local_profile(local, source->profile_key[index], sizeof(source->profile_key[index]));
		if (!source->profile_key[index][0])
			continue;
		if (!ae_social_profile_name((short)local, source->profile_name[index], sizeof(source->profile_name[index])))
			snprintf(source->profile_name[index], sizeof(source->profile_name[index]), "PLAYER %d", local + 1);
		source->profile_chip[index] = chips[local];
		source->profile_player[index] = local + 1;
		source->profiles++;
	}
}

/* `botrecord page <name> [game]`: the store's bots in its order; game: THIS GAME made up from its totals */
static int dev_page(char const *word, int game, char *text, int size)
{
	struct ae_bot_records *records = ae_bot_records_active();
	struct ae_bot_page_source *source;
	char key[AE_BOT_RECORD_KEY], name[AE_BOT_RECORD_KEY];
	int index, at = -1;

	dev_name(word, name, sizeof(name));

	ae_screen_bot_record_remove();
	source = ae_screen_bot_record_source();
	if (!source)
		return 0;
	ae_bot_record_key(name, key, sizeof(key));
	source->from_career = !game;
	page_profiles(source);
	if (game)
	{
		snprintf(source->map, sizeof(source->map), "PRISONER");
		snprintf(source->playlist, sizeof(source->playlist), "TEAM SLAYER");
	}
	for (index = 0; index < records->count && source->count < AE_BOT_RECORD_BOTS; index++)
	{
		struct ae_bot_record const *bot = &records->bot[index];
		int at_source = source->count++, profile;

		snprintf(source->name[at_source], sizeof(source->name[at_source]), "%s", bot->name);
		if (!strcmp(bot->key, key))
			at = at_source;
		if (!game || at_source >= AE_BOT_PAGE_GAMES)
			continue;
		source->game[at_source].has = 1;
		snprintf(source->game[at_source].result, sizeof(source->game[at_source].result), "%s", index % 3 ? "WIN" : "LOSS");
		source->game[at_source].kills = bot->games ? bot->kills / bot->games : 0;
		source->game[at_source].deaths = bot->games ? bot->deaths / bot->games : 0;
		source->game[at_source].assists = bot->games ? bot->assists / bot->games : 0;
		source->game[at_source].spree = bot->best_spree > 1 ? bot->best_spree - 1 : bot->best_spree;
		if (bot->weapon_count > 0)
			snprintf(source->game[at_source].weapon, sizeof(source->game[at_source].weapon), "%s", bot->weapons[0].name);
		for (profile = 0; profile < source->profiles; profile++)
			source->game[at_source].its_kills_on[profile] = 2 + (index + profile * 3) % 5;
	}
	if (at < 0)
	{
		/* (a name the store has not: its page alone, no record) */
		if (source->count >= AE_BOT_RECORD_BOTS)
			source->count = AE_BOT_RECORD_BOTS - 1;
		at = source->count++;
		snprintf(source->name[at], sizeof(source->name[at]), "%s", name);
	}
	if (!ae_screen_bot_record_open(AE_OWNER_PARENT, at))
	{
		snprintf(text, (size_t)size, "the BOT RECORD page could not open (the menus' stack is full)");
		return 0;
	}
	snprintf(text, (size_t)size, "BOT RECORD of %s (%s, %d profile%s)", name, game ? "THIS GAME made up" : "as from CAREER",
		source->profiles, source->profiles == 1 ? "" : "s");
	return 1;
}

int ae_bot_records_dev_carnage(char const *playlist, unsigned long seed, int players, char const *cursor, char *text,
	int size)
{
	static struct ae_pgr_report report;
	struct ae_social_playlist const *list = NULL;
	char kind[16];
	int picked[AE_PGR_ROWS], index, count, picks, at = -1, next_bot = 0;

	count = ae_social_playlist_count();
	for (index = 0; index < count && !list; index++)
	{
		struct ae_social_playlist const *each = ae_social_playlist_at(index);

		if (each && !strcmp(each->id, playlist))
			list = each;
	}
	if (!list || list->teams > 2 || list->size < 1)
	{
		snprintf(text, (size_t)size, "social carnage: %s is not a playlist of two teams or a free for all", playlist);
		return 0;
	}
	if (list->teams)
		snprintf(kind, sizeof(kind), "%dv%d", list->size, list->size);
	else
		snprintf(kind, sizeof(kind), "ffa%d", list->size);
	if (!ae_pgr_demo_report(&report, kind))
	{
		snprintf(text, (size_t)size, "social carnage: no made-up board of %s", kind);
		return 0;
	}
	report.demo = 1;
	report.tournament = 1;
	for (index = 0; list->label[index] && index + 1 < (int)sizeof(report.gametype); index++)
		report.gametype[index] = (char)toupper((unsigned char)list->label[index]);
	report.gametype[index] = 0;
	if (players < 1)
		players = 1;
	if (players > 4 || players > report.rows - 1)
		players = report.rows - 1 < 4 ? report.rows - 1 : 4;
	picks = ae_social_names_pick(seed, report.rows - players, NULL, 0, picked);
	for (index = 0; index < report.rows; index++)
	{
		struct ae_pgr_row *row = &report.row[index];

		row->rival = 0;
		/* (the players first on the board, player 1 at the top) */
		if (index < players)
		{
			row->me = 1;
			row->bot = 0;
			row->local = (short)index;
			if (!ae_social_profile_name((short)index, row->name, sizeof(row->name)))
				snprintf(row->name, sizeof(row->name), "Player %d", index + 1);
			continue;
		}
		row->me = 0;
		row->local = -1;
		row->bot = 1;
		snprintf(row->name, sizeof(row->name), "%s", next_bot < picks ? ae_social_names_get(picked[next_bot]) : "Bot");
		next_bot++;
		if (cursor && at < 0)
		{
			char row_key[AE_BOT_RECORD_KEY], cursor_key[AE_BOT_RECORD_KEY];

			ae_bot_record_key(row->name, row_key, sizeof(row_key));
			ae_bot_record_key(cursor, cursor_key, sizeof(cursor_key));
			if (!strcmp(row_key, cursor_key))
				at = index;
		}
	}
	if (cursor && cursor[0] && at < 0)
	{
		/* (a name no row has: the first bot takes it, e.g. a bot met for the first time) */
		at = players;
		dev_name(cursor, report.row[at].name, sizeof(report.row[at].name));
	}
	if (!ae_screen_pgcr_open_report(AE_OWNER_PARENT, &report, at >= 0 ? at : players))
	{
		snprintf(text, (size_t)size, "social carnage: the screen could not open (the menus' stack is full)");
		return 0;
	}
	platform_log("ae botrecord: carnage %s (%s, seed %lu, %d player%s)", list->id, kind, seed, players, players == 1 ? "" : "s");
	snprintf(text, (size_t)size, "social carnage: a made-up %s board (%s, seed %lu, %d player%s, never recorded)%s%s",
		list->id, kind, seed, players, players == 1 ? "" : "s", at >= 0 ? ", cursor on " : "", at >= 0 ? report.row[at].name : "");
	return 1;
}

int ae_bot_records_dev(struct ae_dev_cmd const *cmd, char *text, int size)
{
	char profile[AE_BOT_RECORD_KEY];
	int words = cmd->words;

	local_profile((int)cmd->number, profile, sizeof(profile));
	/* (a closing "player <n>" is the parser's: not a word of the subcommand) */
	if (words >= 2 && !strcmp(cmd->word[words - 2], "player"))
		words -= 2;
	if (words == 0)
	{
		struct ae_bot_records *records = ae_bot_records_active();

		snprintf(text, (size_t)size, "%s: %d bots, %d versus lines, clock %ld", demo.on ? "the made-up store" : FILE_NAME,
			records->count, records->versus_count, records->clock);
		return 1;
	}
	if (!strcmp(cmd->word[0], "demo"))
	{
		long seed = (long)cmd->seed, bots = (long)cmd->position[0];

		if (words == 2 && !strcmp(cmd->word[1], "off"))
		{
			demo.on = 0;
			snprintf(text, (size_t)size, "the bot records are %s's again", FILE_NAME);
			return 1;
		}
		demo.on = 1;
		bots = ae_bot_records_demo(&demo.records, (unsigned long)seed, (int)bots, profile);
		snprintf(text, (size_t)size, "a made-up store (never written): %ld bots from seed %ld, versus lines for %s", bots, seed,
			profile[0] ? profile : "no profile (a guest)");
		return 1;
	}
	if (!strcmp(cmd->word[0], "page"))
		return dev_page(cmd->word[1], words >= 3 && !strcmp(cmd->word[2], "game"), text, size);
	if (!strcmp(cmd->word[0], "forget"))
	{
		if (!demo.on)
		{
			snprintf(text, (size_t)size, "botrecord: demo store only");
			return 0;
		}
		ae_bot_records_forget(&demo.records, NULL);
		snprintf(text, (size_t)size, "the made-up store is empty");
		return 1;
	}
	if (!strcmp(cmd->word[0], "set"))
	{
		int at, changed = 0;
		char name[AE_BOT_RECORD_KEY];

		if (!demo.on)
		{
			snprintf(text, (size_t)size, "botrecord: demo store only");
			return 0;
		}
		dev_name(cmd->word[1], name, sizeof(name));
		/* (set <name> then pairs: games|wins|ties|kills|deaths|with <n>, vs <you>-<it>; the parser checked them) */
		for (at = 2; at + 1 < words; at += 2)
		{
			long value = 0, value2 = 0;
			char const *dash;

			if (!strcmp(cmd->word[at], "player"))
				continue;
			value = strtol(cmd->word[at + 1], NULL, 10);
			dash = strchr(cmd->word[at + 1], '-');
			if (dash)
				value2 = strtol(dash + 1, NULL, 10);
			changed += ae_bot_records_demo_set(&demo.records, name, profile, cmd->word[at], value, value2);
		}
		snprintf(text, (size_t)size, "%s: %d number%s set", name, changed, changed == 1 ? "" : "s");
		return changed > 0;
	}
	snprintf(text, (size_t)size, "botrecord: %s is not a botrecord command", cmd->word[0]);
	return 0;
}
