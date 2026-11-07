/*
AE_SERVER_LOBBY.C

A dedicated server's lobby page, the engine side (ae_server_lobby.h).
*/

#include "cseries.h"
#include "game/game.h"
#include "networking/network_connection.h"
#include "networking/network_game_globals.h"
#include "networking/network_game_manager.h"
#include "networking/network_server_manager.h"
#include "networking/network_server_manager_internal.h"

#include "network_distributed.h"
#include "ae_server_lobby.h"
#include "../src/server_mode.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void platform_log(char const *format, ...);
unsigned long system_milliseconds(void);

enum
{
	MACHINES = HALO_PORT_MAXIMUM_NETWORK_MACHINES,
	/* the info built again this often while the lobby is up (and sent where it changed) */
	BUILD_EVERY_MS = 500,
	/* a download not heard of for this long is over */
	DOWNLOAD_STALE_MS = 700,
};

struct ae_server_lobby_message
{
	struct distributed_message_header header;
	unsigned char body[AE_SL_WIRE_MAXIMUM];
};

/* ======================================================================================== the server's */

#if HALO_SERVER_BUILD && defined(HALO_GAME_BROWSER)

#include "game/players.h"
#include "game/game_engine.h"
#include "server_director.h"
#include "server_gametypes.h"
#include "team_change.h"
#include "ae_glue_gametype_picker.h"
#include "ae_lan_bots.h"
#include "ae_map_share.h"
#include "ae_server_bots.h"
#include "ae_server_playlist.h"
#include "network_map_vote.h"
#include "../src/port_config.h"
#include "../src/server_rotation.h"

boolean network_distributed_server_send_to_machine_reliably(long machine_index, void *message, word size);
short network_distributed_server_machines(long *machine_indices, short maximum);

/* an entry's words, built once (a gametype's variant takes reading): by its map and gametype text */
enum { DESCRIBED = 32 };
struct described { char key[SERVER_ENTRY_TEXT_SIZE]; char name[AE_SL_NAME]; char facts[AE_SL_FACTS]; };

static struct
{
	unsigned long built_at;
	int in_lobby;
	int size;
	unsigned char bytes[AE_SL_WIRE_MAXIMUM];
	/* what each machine was sent last (0: nothing yet in this lobby) */
	int sent_size[MACHINES];
	unsigned long sent_sum[MACHINES];
	struct described described[DESCRIBED];
	int described_next;
} host;

static unsigned long bytes_sum(unsigned char const *bytes, int size)
{
	unsigned long sum = 5381;
	int index;

	for (index = 0; index < size; index++)
		sum = sum * 33 + bytes[index];
	return sum;
}

static struct network_game *host_game(void)
{
	struct network_game_server *server = global_network_game_server_get();

	return server ? network_game_server_get_game(server) : NULL;
}

static void upper_copy(char const *text, char *out, int size)
{
	int index;

	ae_sl_printable(text, out, size);
	for (index = 0; out[index]; index++)
		if (out[index] >= 'a' && out[index] <= 'z')
			out[index] = (char)(out[index] - 32);
}

/* an entry's gametype name and its line of rules (the variant built as the director builds it) */
static void entry_describe(struct server_entry const *entry, struct ae_sl_entry *out)
{
	struct described *found = NULL;
	long index;

	for (index = 0; index < DESCRIBED; index++)
	{
		if (host.described[index].key[0] && !strcmp(host.described[index].key, entry->text))
		{
			found = &host.described[index];
			break;
		}
	}
	if (!found)
	{
		struct game_variant variant;
		struct game_variant_options options;
		struct ae_gtp_gametype described;
		char reason[96];

		found = &host.described[host.described_next];
		host.described_next = (host.described_next + 1) % DESCRIBED;
		memset(found, 0, sizeof(*found));
		snprintf(found->key, sizeof(found->key), "%s", entry->text);
		if (server_gametype_build(entry->gametype, entry->rules, entry->rule_count, &variant, &options, reason,
			(long)sizeof(reason)))
		{
			ae_gtp_describe_variant(&variant, &options, &described);
			snprintf(found->name, sizeof(found->name), "%s", described.stored);
			ae_sl_facts(described.team, described.start, described.score, described.time, found->facts,
				(int)sizeof(found->facts));
		}
		if (!found->name[0])
			ae_sl_gametype_word(entry->gametype, found->name, (int)sizeof(found->name));
	}
	snprintf(out->name, sizeof(out->name), "%s", found->name);
	snprintf(out->facts, sizeof(out->facts), "%s", found->facts);
}

/* the entry added with a role (or the role added to the same entry already there) */
static void entry_add(struct ae_sl_wire *info, struct server_entry const *entry, unsigned char role)
{
	char const *map = entry->map_name[0] ? entry->map_name : entry->map;
	struct ae_sl_entry *out;
	int index;

	for (index = 0; index < info->count; index++)
	{
		if (!strcmp(info->entries[index].map, map) && !strcmp(info->entries[index].gametype, entry->gametype))
		{
			info->entries[index].roles |= role;
			return;
		}
	}
	if (info->count >= AE_SL_ENTRIES || !map[0])
		return;
	out = &info->entries[info->count++];
	memset(out, 0, sizeof(*out));
	out->roles = role;
	ae_sl_printable(map, out->map, (int)sizeof(out->map));
	ae_sl_printable(entry->gametype, out->gametype, (int)sizeof(out->gametype));
	entry_describe(entry, out);
}

static void info_make(struct ae_sl_wire *info)
{
	struct network_game *game = host_game();
	struct server_rotation const *rotation = server_director_rotation();
	struct server_entry const *now = server_director_current(), *next = server_director_next();
	long vote_entries[NETWORK_MAP_VOTE_MAXIMUM_CHOICES], vote_count, index, slot;
	char const *join_word = NULL;
	int range = 0, policy = team_change_host_policy();

	memset(info, 0, sizeof(*info));
	if (config_boolean("server.map_vote"))
	{
		info->flags |= AE_SL_FLAG_MAP_VOTE;
		if (config_boolean("server.vote_nominate"))
			info->flags |= AE_SL_FLAG_NOMINATE;
	}
	if (config_boolean("server.vote_rtv"))
		info->flags |= AE_SL_FLAG_RTV;
	if (config_integer("server.vote_extend_minutes") > 0)
		info->flags |= AE_SL_FLAG_EXTEND;
	info->team_rule = policy == _team_change_off ? AE_SL_TEAMS_LOCKED : policy == _team_change_allow ? AE_SL_TEAMS_OPEN :
		AE_SL_TEAMS_BALANCED;
	info->join = AE_SL_JOIN_ANYONE;
	if (now)
		info->join = (unsigned char)PIN(ae_server_playlist_audience(now), AE_SL_JOIN_ANYONE, AE_SL_JOIN_AE_MCC);
	else if (ae_server_playlist_listing(&join_word, &range) && join_word)
		info->join = !strcmp(join_word, "ae+mcc") ? AE_SL_JOIN_AE_MCC : !strcmp(join_word, "ae") ? AE_SL_JOIN_AE :
			AE_SL_JOIN_ANYONE;
	/* (the players the lobby waits for before it counts down: people and bots) */
	info->min_players = (unsigned char)PIN(config_integer("server.min_players"), 0, 255);
	info->max_players = (unsigned char)PIN(config_integer("server.max_players"), 0, 255);
	info->vote_seconds = (unsigned char)PIN(config_integer("server.map_vote_seconds"), 0, 255);
	upper_copy(config_string("server.region"), info->region, (int)sizeof(info->region));
	ae_sl_printable(config_string("server.motd"), info->motd, (int)sizeof(info->motd));
	for (slot = 0; game && slot < (long)NUMBEROF(game->players); slot++)
	{
		struct network_player const *player = &game->players[slot];

		if (!network_player_is_valid(player) || !ae_lan_bots_network_player(player))
			continue;
		info->bots = (unsigned char)MIN(info->bots + 1, 255);
		if (player->team_index == 0)
			info->bots_red = (unsigned char)MIN(info->bots_red + 1, 255);
		else if (player->team_index == 1)
			info->bots_blue = (unsigned char)MIN(info->bots_blue + 1, 255);
	}
	if (now)
		entry_add(info, now, AE_SL_ENTRY_NOW);
	/* (the games to come: the next, then the rotation's order from it, none someone present cannot play) */
	if (rotation && rotation->count > 0)
	{
		long start = next && next >= rotation->entries && next < rotation->entries + rotation->count ?
			(long)(next - rotation->entries) : 0, listed = 0;

		for (index = 0; index < rotation->count && listed < AE_SL_NEXT; index++)
		{
			struct server_entry const *entry = &rotation->entries[(start + index) % rotation->count];

			if (entry->rejected[0] || entry == now || ae_server_playlist_blocks(entry))
				continue;
			entry_add(info, entry, AE_SL_ENTRY_NEXT);
			listed++;
		}
	}
	/* (the vote's choices, while one runs or was just held) */
	vote_count = server_director_vote_entries(vote_entries, NETWORK_MAP_VOTE_MAXIMUM_CHOICES);
	for (index = 0; rotation && index < vote_count; index++)
	{
		if (vote_entries[index] >= 0 && vote_entries[index] < rotation->count)
			entry_add(info, &rotation->entries[vote_entries[index]], AE_SL_ENTRY_VOTE);
	}
}

void ae_server_lobby_host_update(void)
{
	struct network_game_server *server = global_network_game_server_get();
	struct ae_server_lobby_message message;
	struct ae_sl_wire info;
	long machines[MACHINES];
	unsigned char present[MACHINES] = { 0 };
	unsigned long now = system_milliseconds(), sum;
	short count, index;
	int lobby, size;

	if (!halo_server_mode())
		return;
	lobby = server && network_game_server_get_state(server, NULL) == 0 && host_game();
	if (!lobby)
	{
		/* (out of the lobby: every machine sent the info again as it comes back) */
		if (host.in_lobby)
		{
			host.in_lobby = 0;
			memset(host.sent_size, 0, sizeof(host.sent_size));
		}
		return;
	}
	host.in_lobby = 1;
	if (host.built_at && now - host.built_at < BUILD_EVERY_MS)
		return;
	host.built_at = now ? now : 1;
	info_make(&info);
	memset(&message, 0, sizeof(message));
	size = ae_sl_wire_encode(&info, message.body, (int)sizeof(message.body));
	if (!size)
		return;
	sum = bytes_sum(message.body, size);
	message.header.header = 0;
	build_message_header(&message.header.header, (word)(sizeof(message.header) + size), 2, 0);
	message.header.type = _distributed_message_ae_server_lobby;
	message.header.count = 1;
	message.header.game_time = 0;
	count = network_distributed_server_machines(machines, MACHINES);
	for (index = 0; index < count; index++)
	{
		long machine = machines[index];

		if (machine < 0 || machine >= MACHINES || !ae_peer_is_ae(machine))
			continue;
		present[machine] = 1;
		if (host.sent_size[machine] == size && host.sent_sum[machine] == sum)
			continue;
		network_distributed_server_send_to_machine_reliably(machine, &message, (word)(sizeof(message.header) + size));
		if (!host.sent_size[machine])
			platform_log("server lobby: info sent to machine #%d (%d games, %d bots)", (int)machine, info.count,
				info.bots);
		host.sent_size[machine] = size;
		host.sent_sum[machine] = sum;
	}
	for (index = 0; index < MACHINES; index++)
		if (!present[index])
			host.sent_size[index] = 0;
}

int ae_server_lobby_team_refused(struct network_game const *game, long player_list_index, short wanted_team)
{
	long slot, red = 0, blue = 0, margin;
	int policy;
	short current;

	if (!halo_server_mode() || !game || player_list_index < 0 || player_list_index >= (long)NUMBEROF(game->players) ||
		!game->variant.universal_variant.teams)
	{
		return 0;
	}
	current = game->players[player_list_index].team_index;
	if (wanted_team == current || (wanted_team != 0 && wanted_team != 1))
		return 0;
	policy = team_change_host_policy();
	if (policy == _team_change_allow)
		return 0;
	if (policy == _team_change_off)
	{
		platform_log("server lobby: team change refused (the server's rule: off)");
		return 1;
	}
	/* (balanced: the people on the teams after it within the margin, or no worse than before) */
	for (slot = 0; slot < (long)NUMBEROF(game->players); slot++)
	{
		struct network_player const *player = &game->players[slot];

		if (!network_player_is_valid(player) || ae_lan_bots_network_player(player))
			continue;
		red += player->team_index == 0;
		blue += player->team_index == 1;
	}
	margin = team_change_host_margin();
	{
		long before = labs(red - blue), after;

		if (current == 0)
			red--, blue++;
		else
			blue--, red++;
		after = labs(red - blue);
		if (after > margin && after >= before)
		{
			platform_log("server lobby: team change refused (balanced: %ld red, %ld blue after it)", red, blue);
			return 1;
		}
	}
	return 0;
}

#else

void ae_server_lobby_host_update(void)
{
}

int ae_server_lobby_team_refused(struct network_game const *game, long player_list_index, short wanted_team)
{
	(void)game;
	(void)player_list_index;
	(void)wanted_team;
	return 0;
}

#endif

/* ======================================================================================== the client's */

static struct
{
	int valid;
	struct ae_sl_wire info;
	/* the download, as map_share.c's line had it last */
	unsigned long download_at;
	char level[128];
	int asking;
	unsigned long done, total;
} client;

int ae_server_lobby_dedicated(void)
{
	struct network_game *game;

	if (game_connection() != _game_connection_network_client || !(game = network_game_get_game()))
		return 0;
	return TEST_FLAG(game->cooperative_flags, _network_game_cooperative_dedicated_bit) != 0;
}

struct ae_sl_wire const *ae_server_lobby_info(void)
{
	return client.valid && ae_server_lobby_dedicated() ? &client.info : NULL;
}

void ae_server_lobby_client_tick(unsigned long now)
{
	(void)now;
	if (client.valid && game_connection() != _game_connection_network_client)
	{
		client.valid = 0;
		platform_log("server lobby: the server's lobby info forgotten (no longer its client)");
	}
}

void ae_server_lobby_download_seen(char const *level, int asking, int transferring, unsigned long done,
	unsigned long total)
{
	if (!level || !level[0] || (!asking && !transferring))
		return;
	client.download_at = system_milliseconds() | 1;
	snprintf(client.level, sizeof(client.level), "%s", level);
	client.asking = asking;
	client.done = done;
	client.total = asking ? 0 : total;
}

int ae_server_lobby_download(char *level, int size, unsigned long *done, unsigned long *total)
{
	if (!client.download_at || system_milliseconds() - client.download_at > DOWNLOAD_STALE_MS)
		return 0;
	if (level && size > 0)
		snprintf(level, (size_t)size, "%s", client.level);
	*done = client.done;
	*total = client.total;
	return 1;
}

int ae_server_lobby_handles_message(unsigned short const *message, unsigned short size)
{
	struct distributed_message_header header;

	if (size < sizeof(header))
		return 0;
	memcpy(&header, message, sizeof(header));
	return header.type == _distributed_message_ae_server_lobby;
}

/* (the server's alone, over its stream, whole) */
void ae_server_lobby_handle_message(long machine_index, unsigned short const *data, unsigned short size)
{
	struct ae_server_lobby_message message;
	struct ae_sl_wire read;
	int was_valid = client.valid;

	if (machine_index != NONE || game_connection() != _game_connection_network_client ||
		network_connection_last_read_was_unreliable() || size < sizeof(message.header) + AE_SL_HEAD ||
		size > sizeof(message))
	{
		return;
	}
	memcpy(&message, data, size);
	if (GET_MESSAGE_TYPE(message.header.header) != 2 || message.header.type != _distributed_message_ae_server_lobby ||
		message.header.count != 1)
	{
		return;
	}
	if (!ae_sl_wire_decode(message.body, (int)(size - sizeof(message.header)), &read))
	{
		platform_log("server lobby: an info message that does not read: dropped");
		return;
	}
	client.info = read;
	client.valid = 1;
	if (!was_valid)
		platform_log("server lobby: the server's lobby info (%s, joins %s, waits for %d, %d games, bots %d red %d blue "
			"%d in all, teams %d, chat 0x%x)", read.region[0] ? read.region : "no region", ae_sl_join_word(read.join),
			read.min_players, read.count, read.bots_red, read.bots_blue, read.bots, read.team_rule, read.flags);
}
