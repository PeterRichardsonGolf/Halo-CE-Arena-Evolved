/*
AE_SERVER_BOTS.C

See ae_server_bots.h. Rules: ae_server_bots_rules.c.
*/

#include "cseries.h"
#include "game/game.h"
#include "game/game_engine.h"
#include "game/players.h"
#include "memory/data.h"
#include "networking/network_game_manager.h"
#include "saved games/arena_gametypes.h"

#include "ae_lan_bots.h"
#include "ae_server_bots.h"
#include "ae_server_bots_rules.h"
#include "network_distributed.h"
#include "../src/server_json.h"
#include "../src/server_log.h"
#include "../src/server_mode.h"

#include <stdio.h>
#include <string.h>

int config_boolean(const char *name);
int config_integer(const char *name);
const char *config_string(const char *name);
int config_set(const char *name, const char *value);

/* a rejoin held until the human is back: its statistics as they were */
struct server_bots_held
{
	struct game_statistics statistics;
	int have_statistics;
	/* the datum the dropped human played (it stays until the game ends, its quit flag set a moment after the drop) */
	long datum_index;
};

/* a human who came back and waits for its player to exist, to get its statistics back */
struct server_bots_restore
{
	int used;
	char name[AE_SERVER_REJOIN_NAME];
	unsigned long address;
	int tries;
	long old_datum_index;
	struct game_statistics statistics;
};

/* the humans of the game as the last look saw them */
struct server_bots_seen
{
	int used;
	char name[AE_SERVER_REJOIN_NAME];
	unsigned long address;
	int team;
};

static struct
{
	int entry_ae;
	int entry_set;
	double human_since;
	int in_game;
	struct ae_server_rejoin_table table;
	struct server_bots_held held[AE_SERVER_REJOIN_MAXIMUM];
	struct server_bots_restore restore[AE_SERVER_REJOIN_MAXIMUM];
	struct server_bots_seen seen[HALO_PORT_MAXIMUM_NETWORK_PLAYERS];
} sb = { 0, 0, -1.0, 0 };

static int sb_fill(void)
{
	int fill = ae_server_bots_fill_parse(config_string("server.bots_fill"));

	if (fill < 0)
		fill = AE_SERVER_BOTS_FILL_AUTO;
	return ae_server_bots_fill_resolved(fill, sb.entry_ae);
}

boolean ae_server_bots_enabled(void)
{
	return halo_server_mode() && sb_fill() != AE_SERVER_BOTS_FILL_OFF;
}

boolean ae_server_bots_alone(void)
{
	return halo_server_mode() && config_boolean("server.bots_alone") != 0;
}

char const *ae_server_bots_skill_name(void)
{
	char const *name = config_string("server.bots_skill");

	return ae_server_bots_skill_valid(name) ? name : "normal";
}

static void sb_reset_game(void);

void ae_server_bots_entry_applied(char const *gametype)
{
	struct game_variant variant;
	struct game_variant_options options;
	wchar_t wide[64];
	long length;

	for (length = 0; gametype && gametype[length] && length < (long)NUMBEROF(wide) - 1; length++)
		wide[length] = (wchar_t)(unsigned char)(gametype[length] == '_' ? ' ' : gametype[length]);
	wide[length] = 0;
	csmemset(&variant, 0, sizeof(variant));
	sb.entry_ae = gametype && arena_gametype_any_build(wide, &variant, &options) ? 1 : 0;
	sb.entry_set = 1;
	sb_reset_game();
	/* (the skill the brain reads: bots.skill) */
	config_set("bots.skill", ae_server_bots_skill_name());
	server_log(_server_log_info, "bots", "entry %s: fill %s, skill %s", gametype ? gametype : "?",
		sb_fill() == AE_SERVER_BOTS_FILL_OFF ? "off" : sb_fill() == AE_SERVER_BOTS_FILL_MINIMUM ? "minimum" :
		"empty_slots", ae_server_bots_skill_name());
}

long ae_server_bots_target(long humans, long max_players, boolean played, long bots_now, boolean lobby, double now)
{
	int fill = sb_fill();
	long minimum = config_integer("server.min_players");
	long delay = config_integer("server.bots_delay");
	int alone = config_boolean("server.bots_alone") != 0;

	if (minimum < 1)
		minimum = 1;
	if (minimum > 16)
		minimum = 16;
	if (delay < 0)
		delay = 0;
	/* (the lobby waits for others a moment before bots fill it: the delay from the first person in, when no bot is
	seated yet) */
	if (humans <= 0)
		sb.human_since = -1.0;
	else if (sb.human_since < 0.0)
		sb.human_since = now;
	if (lobby && humans > 0 && bots_now == 0 && now - sb.human_since < (double)delay)
		return 0;
	return ae_server_bots_wanted(fill, (int)minimum, (int)humans, ae_server_rejoin_held(&sb.table, 1),
		!AE_SERVER_BOTS_RATED_DEFAULT, (int)max_players, alone, played != 0);
}

long ae_server_bots_humans(struct network_game const *game)
{
	long slot, count = 0;

	for (slot = 0; game && slot < (long)NUMBEROF(game->players); slot++)
	{
		if (network_player_is_valid(&game->players[slot]) && !ae_lan_bots_network_player(&game->players[slot]))
			count++;
	}
	return count;
}

long ae_server_bots_count(struct network_game const *game)
{
	long slot, count = 0;

	for (slot = 0; game && slot < (long)NUMBEROF(game->players); slot++)
	{
		if (network_player_is_valid(&game->players[slot]) && ae_lan_bots_network_player(&game->players[slot]))
			count++;
	}
	return count;
}

static void sb_name(wchar_t const *wide, char *name)
{
	long index;

	for (index = 0; wide[index] && index < AE_SERVER_REJOIN_NAME - 1; index++)
		name[index] = (unsigned)wide[index] >= 0x20 && (unsigned)wide[index] < 0x7F ? (char)wide[index] : '?';
	name[index] = 0;
}

static unsigned long sb_address(struct network_player const *player)
{
	return player->machine_index >= 0 ? distributed_machine_real_address(player->machine_index) : 0;
}

/* the datum of a human by its name that is not the datum `except` (NONE for any): a human who played the game under
that name and is gone (quit true: its quit flag is set), or the one who plays it now (quit false). The newest datum wins
when a human came back under the same name */
static struct player_datum *sb_datum(char const *name, boolean quit, long except, long *datum_index)
{
	struct data_iterator iterator;
	struct player_datum *player, *found = NULL;

	if (!player_data)
		return NULL;
	data_iterator_new(&iterator, player_data);
	while ((player = (struct player_datum *)data_iterator_next(&iterator)) != NULL)
	{
		char text[AE_SERVER_REJOIN_NAME];

		sb_name(player->name, text);
		if (!strcmp(text, name) && iterator.datum_index != except && (!quit || player->quit_out_of_game) &&
			(quit || !player->quit_out_of_game) && !ae_lan_bots_network_player(&player->network_player_data))
		{
			found = player;
			if (datum_index)
				*datum_index = iterator.datum_index;
		}
	}
	return found;
}

/* the datum at a datum index, NULL when it is gone */
static struct player_datum *sb_datum_at(long datum_index)
{
	struct data_iterator iterator;
	struct player_datum *player;

	if (!player_data || datum_index == NONE)
		return NULL;
	data_iterator_new(&iterator, player_data);
	while ((player = (struct player_datum *)data_iterator_next(&iterator)) != NULL)
	{
		if (iterator.datum_index == datum_index)
			return player;
	}
	return NULL;
}

/* the datum of a human who has just dropped: its quit flag is set a moment after the player left the game's list, so
the newest datum of that name not marked as quit by a former drop */
static struct player_datum *sb_datum_dropped(char const *name, long *datum_index)
{
	struct data_iterator iterator;
	struct player_datum *player, *found = NULL;

	if (!player_data)
		return NULL;
	data_iterator_new(&iterator, player_data);
	while ((player = (struct player_datum *)data_iterator_next(&iterator)) != NULL)
	{
		char text[AE_SERVER_REJOIN_NAME];

		sb_name(player->name, text);
		if (!strcmp(text, name) && !ae_lan_bots_network_player(&player->network_player_data))
		{
			found = player;
			if (datum_index)
				*datum_index = iterator.datum_index;
		}
	}
	return found;
}

static void sb_reset_game(void)
{
	csmemset(sb.seen, 0, sizeof(sb.seen));
	csmemset(sb.restore, 0, sizeof(sb.restore));
	csmemset(sb.held, 0, sizeof(sb.held));
	ae_server_rejoin_clear(&sb.table);
	sb.in_game = 0;
}

void ae_server_bots_update(struct network_game *game)
{
	struct server_bots_seen now[HALO_PORT_MAXIMUM_NETWORK_PLAYERS];
	long slot, count = 0, index;

	if (!halo_server_mode() || !game)
		return;
	if (!game_engine_running() || game->variant.game_engine_index == game_engine_none)
	{
		if (sb.in_game)
			sb_reset_game();
		return;
	}
	/* (a game over: people who leave its postgame are not dropped) */
	if (!game_engine_can_score())
		return;
	csmemset(now, 0, sizeof(now));
	for (slot = 0; slot < (long)NUMBEROF(game->players); slot++)
	{
		struct network_player const *player = &game->players[slot];

		if (!network_player_is_valid(player) || ae_lan_bots_network_player(player) || count >= HALO_PORT_MAXIMUM_NETWORK_PLAYERS)
			continue;
		now[count].used = 1;
		sb_name(player->name, now[count].name);
		now[count].address = sb_address(player);
		now[count].team = VALID_INDEX(player->team_index, 2) ? player->team_index : -1;
		count++;
	}
	if (sb.in_game)
	{
		/* those gone since the last look: held under their name and address, with their statistics */
		for (index = 0; index < HALO_PORT_MAXIMUM_NETWORK_PLAYERS; index++)
		{
			long other;
			int here = 0;

			if (!sb.seen[index].used)
				continue;
			for (other = 0; other < count; other++)
				here |= now[other].address == sb.seen[index].address && !strcmp(now[other].name, sb.seen[index].name);
			if (!here)
			{
				int entry = ae_server_rejoin_drop(&sb.table, sb.seen[index].name, sb.seen[index].address,
					sb.seen[index].team, !AE_SERVER_BOTS_RATED_DEFAULT);
				long datum_index = NONE;
				struct player_datum *datum = sb_datum_dropped(sb.seen[index].name, &datum_index);

				if (entry >= 0)
				{
					sb.held[entry].have_statistics = datum != NULL;
					sb.held[entry].datum_index = datum_index;
					if (datum)
						sb.held[entry].statistics = datum->statistics;
				}
				server_log(_server_log_info, "bots", "%s dropped from the game (team %d): the slot is held%s", sb.seen[index].name,
					sb.seen[index].team, AE_SERVER_BOTS_RATED_DEFAULT ? " empty" : " by a bot until they come back");
			}
		}
		/* those new since the last look: a human who comes back gets the statistics back once its player exists */
		for (index = 0; index < count; index++)
		{
			long other;
			int known = 0, entry;

			for (other = 0; other < HALO_PORT_MAXIMUM_NETWORK_PLAYERS; other++)
				known |= sb.seen[other].used && sb.seen[other].address == now[index].address &&
					!strcmp(sb.seen[other].name, now[index].name);
			if (known)
				continue;
			entry = ae_server_rejoin_find(&sb.table, now[index].name, now[index].address);
			if (entry >= 0)
			{
				if (sb.held[entry].have_statistics)
				{
					for (other = 0; other < AE_SERVER_REJOIN_MAXIMUM && sb.restore[other].used; other++)
						;
					if (other < AE_SERVER_REJOIN_MAXIMUM)
					{
						sb.restore[other].used = 1;
						strncpy(sb.restore[other].name, now[index].name, AE_SERVER_REJOIN_NAME - 1);
						sb.restore[other].address = now[index].address;
						sb.restore[other].tries = 0;
						sb.restore[other].old_datum_index = sb.held[entry].datum_index;
						sb.restore[other].statistics = sb.held[entry].statistics;
					}
				}
				server_log(_server_log_info, "bots", "%s is back (team %d, they left team %d)", now[index].name, now[index].team,
					sb.table.entries[entry].team);
				ae_server_rejoin_take(&sb.table, entry);
			}
		}
	}
	/* (the statistics given back once the returning human's player exists: each look until then, a few seconds) */
	for (index = 0; index < AE_SERVER_REJOIN_MAXIMUM; index++)
	{
		struct player_datum *live, *old;

		if (!sb.restore[index].used)
			continue;
		live = sb_datum(sb.restore[index].name, FALSE, sb.restore[index].old_datum_index, NULL);
		if (!live && ++sb.restore[index].tries < 600)
			continue;
		if (live)
		{
			old = sb_datum_at(sb.restore[index].old_datum_index);
			live->statistics = sb.restore[index].statistics;
			if (old)
				csmemset(&old->statistics, 0, sizeof(old->statistics));
			server_log(_server_log_info, "bots", "%s: their score is back (%d kills, %d deaths)", sb.restore[index].name,
				(int)sb.restore[index].statistics.kills[0], (int)sb.restore[index].statistics.deaths);
		}
		sb.restore[index].used = 0;
	}
	csmemcpy(sb.seen, now, sizeof(now));
	sb.in_game = 1;
}

int ae_server_bots_rejoin_team(struct network_player const *player, int team)
{
	char name[AE_SERVER_REJOIN_NAME];
	int entry;

	if (!halo_server_mode() || !player || !sb.in_game)
		return team;
	sb_name(player->name, name);
	entry = ae_server_rejoin_find(&sb.table, name, sb_address(player));
	if (entry < 0 || !VALID_INDEX(sb.table.entries[entry].team, 2))
		return team;
	return sb.table.entries[entry].team;
}

char const *ae_server_bots_fill_name(void)
{
	switch (sb_fill())
	{
		case AE_SERVER_BOTS_FILL_MINIMUM: return "minimum";
		case AE_SERVER_BOTS_FILL_EMPTY_SLOTS: return "empty_slots";
		default: return "off";
	}
}

void ae_server_bots_status(struct server_json *json, struct network_game const *game)
{
	server_json_object_begin(json, "bots");
	server_json_integer(json, "count", ae_server_bots_count(game));
	server_json_string(json, "skill", ae_server_bots_skill_name());
	server_json_string(json, "fill", ae_server_bots_fill_name());
	server_json_boolean(json, "alone", config_boolean("server.bots_alone") != 0);
	server_json_integer(json, "held", ae_server_rejoin_held(&sb.table, 1) + ae_server_rejoin_held(&sb.table, 0));
	server_json_object_end(json);
}

void ae_server_bots_summary(void)
{
	char const *setting = config_string("server.bots_fill");
	int fill = ae_server_bots_fill_parse(setting);

	if (fill < 0)
		fill = AE_SERVER_BOTS_FILL_AUTO;
	server_log(_server_log_info, "server", "bots: fill %s (%s: minimum on AE gametypes, off on the others), skill %s, "
		"bot-only games %s, start delay %ld s", fill == AE_SERVER_BOTS_FILL_AUTO ? "auto" :
		fill == AE_SERVER_BOTS_FILL_OFF ? "off" : fill == AE_SERVER_BOTS_FILL_MINIMUM ? "minimum" : "empty_slots",
		"server.bots_fill", ae_server_bots_skill_name(), config_boolean("server.bots_alone") ? "on" : "off",
		(long)config_integer("server.bots_delay"));
}
