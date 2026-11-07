/*
AE_SERVER_INGAME.C

Running a dedicated server from inside the game (ae_server_ingame.h).
*/

#include "cseries.h"
#include "game/game.h"
#include "interface/ui_widget.h"
#include "networking/network_connection.h"
#include "networking/network_game_globals.h"
#include "networking/network_game_manager.h"
#include "networking/network_server_manager.h"
#include "networking/network_server_manager_internal.h"

#include "network_distributed.h"
#include "ae_moderation.h"
#include "ae_scoreboard_header.h"
#include "ae_server_ingame.h"
#include "../src/server_mode.h"

#ifdef HALO_GAME_BROWSER
#include "../src/delta_moderation.h"
int browser_moderator_key(char *text, int size);
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void platform_log(char const *format, ...);
unsigned long system_milliseconds(void);

enum
{
	MACHINES = HALO_PORT_MAXIMUM_NETWORK_MACHINES,
	SLOTS = HALO_PORT_MAXIMUM_NETWORK_PLAYERS,
	/* the admin message: when it changes, at most this often, and this often anyway */
	SEND_SOONEST_MS = 500,
	SEND_EVERY_MS = 4000,
	/* a client forgets the message this long after the last one (the role taken away, the server gone quiet) */
	INFO_STALE_MS = 15000,
	/* a sign-in tried again after this long, when the server never said the role */
	SIGN_IN_AGAIN_MS = 20000,
};

struct ae_server_admin_message
{
	struct distributed_message_header header;
	unsigned char body[AE_INGAME_WIRE_MAXIMUM];
};

/* ======================================================================================== the server's */

#if HALO_SERVER_BUILD && defined(HALO_GAME_BROWSER)

#include "game/players.h"
#include "server_director.h"
#include "ae_server_bots.h"
#include "ae_server_restart.h"
#include "ae_server_mod.h"
#include "ae_server_mod_rules.h"
#include "ae_team_override.h"
#include "../src/ae_server_ops.h"
#include "../src/port_config.h"
#include "../src/server_console.h"
#include "../src/server_log.h"
#include "../src/server_rotation.h"

char *server_commands_run(char const *line, int role, int origin, boolean *succeeded);
void posix_random_bytes(void *buffer, unsigned long size);

static struct
{
	boolean registered;
	unsigned long started_ms;
	boolean has_key[MACHINES];
	unsigned char keys[MACHINES][32];
	unsigned long sent_at[MACHINES];
	int sent_size[MACHINES];
	unsigned char sent[MACHINES][AE_INGAME_WIRE_MAXIMUM];
	/* a send due at once (an action done) */
	boolean urgent;
	/* the sign-in offer: this server's salt, who has had the offer, and the key rows' revision it was made at */
	unsigned char salt[AE_INGAME_OFFER_SALT];
	boolean salt_made;
	boolean offered[MACHINES];
	unsigned long offer_revision;
} host;

static struct network_game *host_game(void)
{
	struct network_game_server *server = global_network_game_server_get();

	return server ? network_game_server_get_game(server) : NULL;
}

/* HALO_SERVER_OWNER_KEY (HOST A SERVER's: the starting PC's own moderator key) as 64 hex characters, else "" */
static char const *owner_key(void)
{
	static char kept[AE_INGAME_KEY_HEX + 1];
	static boolean read;

	if (!read)
	{
		char const *text = getenv("HALO_SERVER_OWNER_KEY");

		read = TRUE;
		if (!text || !ae_ingame_key_hex(text, kept, (int)sizeof(kept)))
			kept[0] = 0;
	}
	return kept;
}

/* a key's role here: the owner's key, its key row, the address its machine joined from (the highest) */
static int key_role(unsigned char const *key, int machine_index, int *immunity)
{
	char hex[AE_INGAME_KEY_HEX + 1];
	int role = AE_INGAME_ROLE_NONE, best_immunity = 0, row_role, row_immunity;

	ae_ingame_key_text(key, hex);
	if (owner_key()[0] && !strcmp(owner_key(), hex))
	{
		role = AE_INGAME_ROLE_OWNER;
		best_immunity = 99;
	}
	if (ae_ops_admin_by_key(hex, &row_role, &row_immunity))
	{
		/* (equal roles: the higher immunity) */
		int merged_role = role > AE_INGAME_ROLE_NONE ? role : -1;

		ae_mod_standing_merge(&merged_role, &best_immunity, row_role, row_immunity);
		role = merged_role < 0 ? AE_INGAME_ROLE_NONE : merged_role;
	}
	if (machine_index >= 0 && machine_index < MACHINES)
	{
		unsigned long address = distributed_machine_real_address(machine_index);

		if (address && ae_ops_admin_by_address((unsigned int)address, &row_role, &row_immunity))
		{
			int merged_role = role > AE_INGAME_ROLE_NONE ? role : -1;

			ae_mod_standing_merge(&merged_role, &best_immunity, row_role, row_immunity);
			role = merged_role < 0 ? AE_INGAME_ROLE_NONE : merged_role;
		}
	}
	if (immunity)
		*immunity = best_immunity;
	return role;
}

int ae_server_ingame_key_immunity(char const *key, int *immunity)
{
	unsigned char bytes[32];
	short machine;

	if (!ae_ingame_key_bytes(key, bytes))
		return 0;
	for (machine = 0; machine < MACHINES; machine++)
	{
		if (host.has_key[machine] && !memcmp(host.keys[machine], bytes, 32))
			return key_role(bytes, machine, immunity) > AE_INGAME_ROLE_NONE;
	}
	return key_role(bytes, NONE, immunity) > AE_INGAME_ROLE_NONE;
}

int ae_server_ingame_machine_role(int machine_index, int *immunity)
{
	if (immunity)
		*immunity = 0;
	if (machine_index < 0 || machine_index >= MACHINES || !host.has_key[machine_index])
		return AE_INGAME_ROLE_NONE;
	return key_role(host.keys[machine_index], machine_index, immunity);
}

static int key_machine(unsigned char const *key)
{
	short machine;

	for (machine = 0; machine < MACHINES; machine++)
	{
		if (host.has_key[machine] && !memcmp(host.keys[machine], key, 32))
			return machine;
	}
	return NONE;
}

/* (ae_ingame_allowed's: the role a command needs on this server, -1 for none it has) */
static int command_role(void *context, char const *command)
{
	static struct { char const *name; int role; } const own[] =
	{
		{ "sv_team", _server_role_moderator }, { "mute", _server_role_moderator }, { "kick", _server_role_moderator },
		{ "ban", _server_role_moderator }, { "sv_rotation_begin", _server_role_moderator },
		{ "sv_end_game", _server_role_moderator }, { "say", _server_role_moderator },
		{ "sv_restart", _server_role_owner },
	};
	long index;

	(void)context;
	for (index = 0; index < (long)NUMBEROF(own); index++)
	{
		if (!strcmp(own[index].name, command))
			return ae_server_mod_command_role(command, own[index].role);
	}
	return -1;
}

/* the moderation's permission bits for a role (Delta Peer's MOD_STATE; this server's actions are checked again as
commands) */
static delta_u32 role_permissions(int role)
{
	delta_u32 bits = 0;

	if (role >= AE_INGAME_ROLE_MODERATOR)
		bits |= DELTA_MODERATION_VIEW | DELTA_MODERATION_WARN | DELTA_MODERATION_KICK | DELTA_MODERATION_MAP;
	if (role >= AE_INGAME_ROLE_ADMIN)
		bits |= DELTA_MODERATION_BAN_TIMED | DELTA_MODERATION_BAN | DELTA_MODERATION_UNBAN;
	if (role >= AE_INGAME_ROLE_OWNER)
		bits |= DELTA_MODERATION_SETTINGS | DELTA_MODERATION_ROLES;
	return bits;
}

/* ---------- Delta Peer's hooks */

static void hook_machine_key(void *context, int machine_index, unsigned char const *key)
{
	(void)context;
	if (machine_index < 0 || machine_index >= MACHINES)
		return;
	host.sent_size[machine_index] = 0;
	if (!key)
	{
		host.has_key[machine_index] = FALSE;
		return;
	}
	memcpy(host.keys[machine_index], key, 32);
	host.has_key[machine_index] = TRUE;
	{
		char hex[AE_INGAME_KEY_HEX + 1];
		int role = key_role(key, machine_index, NULL);

		ae_ingame_key_text(key, hex);
		server_log(_server_log_info, "moderation", "machine %d signed in from the game, key %.8s: %s", machine_index,
			hex, role ? server_role_name(role) : "no role");
		/* (the slots kept for admins open to this machine from now on: ae_server_mod.c's door) */
		if (role >= AE_INGAME_ROLE_MODERATOR)
			ae_server_mod_admin_hardware_id(network_game_server_machine_hardware_id(machine_index));
	}
	host.urgent = TRUE;
}

static int hook_key_role(void *context, unsigned char const *key, delta_u32 *permissions, delta_u32 *ban_minutes)
{
	int role = key_role(key, key_machine(key), NULL);

	(void)context;
	if (permissions)
		*permissions = role_permissions(role);
	if (ban_minutes)
		*ban_minutes = 7 * 24 * 60;
	return role;
}

/* the slot of the machine's player: by name when given, else its first */
static long machine_slot(long machine_index, char const *name)
{
	struct network_game *game = host_game();
	long slot, slots[SLOTS];
	char names[SLOTS][16];
	char const *pointers[SLOTS];
	int count = 0, pick;

	if (!game)
		return NONE;
	for (slot = 0; slot < SLOTS; slot++)
	{
		if (!network_player_is_valid(&game->players[slot]) || game->players[slot].machine_index != machine_index)
			continue;
		if (!server_status_player_name(slot, names[count], sizeof(names[count])))
			names[count][0] = 0;
		pointers[count] = names[count];
		slots[count++] = slot;
	}
	/* (a card names its player: when that player has left, nobody, not the machine's first player or a new joiner) */
	pick = ae_ingame_card_pick(count, pointers, name);
	return pick < 0 ? NONE : slots[pick];
}

/* the first line of a command's output, for the player's result */
static void first_line(char const *output, char *result, int size)
{
	int length;

	if (!output || !output[0])
	{
		result[0] = 0;
		return;
	}
	length = (int)strcspn(output, "\n");
	snprintf(result, (size_t)size, "%.*s", length, output);
}

static int hook_action(void *context, int machine_index, unsigned char const *key, int action, int target_machine,
	int minutes, char const *reason, char *result, int result_size)
{
	char line[256], text[96], length[16], hex[AE_INGAME_KEY_HEX + 1], actor[80];
	char *output;
	boolean succeeded = FALSE;
	long slot = NONE;
	int immunity = 0, role = key_role(key, machine_index, &immunity);
	unsigned long held_address = 0;
	char held_id[48] = "";

	(void)context;
	if (role <= AE_INGAME_ROLE_NONE)
	{
		snprintf(result, (size_t)result_size, "You have no role on this server.");
		return 0;
	}
	if (action == _delta_moderation_action_kick || action == _delta_moderation_action_ban ||
		action == _delta_moderation_action_mute || action == _delta_moderation_action_unmute ||
		action == _delta_moderation_action_ae_move || action == _delta_moderation_action_warn)
	{
		slot = machine_slot(target_machine, reason);
		if (slot == NONE)
		{
			snprintf(result, (size_t)result_size, "That player is no longer in the game.");
			return 0;
		}
	}
	switch (action)
	{
	case _delta_moderation_action_kick:
		ae_moderation_hold_machine((short)target_machine, &held_address, held_id, (int)sizeof(held_id));
		snprintf(line, sizeof(line), "kick %ld", slot);
		break;
	case _delta_moderation_action_ban:
		ae_ingame_ban_command_length(minutes, length, (int)sizeof(length));
		snprintf(line, sizeof(line), "ban %ld %s \"banned from the game by an admin\"", slot, length);
		break;
	case _delta_moderation_action_mute:
		if (minutes > 0)
			snprintf(line, sizeof(line), "mute %ld %d", slot, minutes > 24 * 60 ? 24 * 60 : minutes);
		else
			snprintf(line, sizeof(line), "mute %ld", slot);
		break;
	case _delta_moderation_action_unmute:
		snprintf(line, sizeof(line), "unmute %ld", slot);
		break;
	case _delta_moderation_action_ae_move:
		snprintf(line, sizeof(line), "sv_team %ld %s", slot, minutes ? "blue" : "red");
		break;
	case _delta_moderation_action_end_game:
		snprintf(line, sizeof(line), "sv_end_game");
		break;
	case _delta_moderation_action_ae_next_entry:
	{
		struct server_rotation const *rotation = server_director_rotation();

		if (!rotation || minutes < 0 || minutes >= rotation->count || rotation->entries[minutes].rejected[0])
		{
			snprintf(result, (size_t)result_size, "That game is not in the playlist now.");
			return 0;
		}
		/* (the next game, after this one: never the one played now) */
		snprintf(line, sizeof(line), "sv_rotation_begin %d", minutes);
		break;
	}
	case _delta_moderation_action_ae_say:
		if (!ae_ingame_clean_text(reason, text, (int)sizeof(text)))
		{
			snprintf(result, (size_t)result_size, "Nothing to say.");
			return 0;
		}
		snprintf(line, sizeof(line), "say %s", text);
		break;
	case _delta_moderation_action_ae_restart:
		snprintf(line, sizeof(line), "sv_restart %d \"An admin restarts the server.\"", minutes > 0 && minutes <= 600 ?
			minutes : AE_INGAME_RESTART_SECONDS);
		break;
	default:
		snprintf(result, (size_t)result_size, "This server does not take that action from the game.");
		return 0;
	}
	ae_ingame_key_text(key, hex);
	snprintf(actor, sizeof(actor), "key:%s", hex);
	ae_server_mod_set_actor(actor);
	ae_server_mod_set_actor_immunity(immunity + 1);
	server_log(_server_log_info, "console", "game %.8s (%s): %s", hex, server_role_name(role), line);
	output = server_commands_run(line, role, _server_origin_web, &succeeded);
	first_line(output, result, result_size);
	if (output)
		server_console_free(output);
	if (succeeded && action == _delta_moderation_action_kick)
		ae_moderation_hold(held_address, held_id);
	if (!result[0])
		snprintf(result, (size_t)result_size, succeeded ? "Done." : "The server did not do it.");
	host.urgent = TRUE;
	return succeeded ? 1 : 0;
}

static void hook_bind_answer(void *context, int machine_index, delta_u32 request, int accepted, unsigned char const *key)
{
	(void)context;
	(void)machine_index;
	(void)request;
	(void)accepted;
	(void)key;
}

/* ---------- the admin message */

static void info_make(struct ae_ingame_wire *info, int role)
{
	struct network_game *game = host_game();
	struct server_rotation const *rotation = server_director_rotation();
	struct server_entry const *next = server_director_next();
	long current = server_director_index(), next_index = NONE, index;

	memset(info, 0, sizeof(*info));
	info->role = (unsigned char)role;
	info->allowed = (unsigned short)ae_ingame_allowed(role, command_role, NULL);
	snprintf(info->name, sizeof(info->name), "%s", config_string("server.name"));
	if (game)
	{
		long humans = ae_server_bots_humans(game), bots = ae_server_bots_count(game);

		info->players = (unsigned char)PIN(humans, 0, 255);
		info->bots = (unsigned char)PIN(bots, 0, 255);
	}
	info->max_players = (unsigned char)PIN(config_integer("server.max_players"), 0, 255);
	info->up_minutes = (unsigned short)MIN((system_milliseconds() - host.started_ms) / 60000UL, 0xFFFFUL);
	if (server_director_refuses_joins())
		info->flags |= AE_INGAME_WIRE_HOLDING;
	if (ae_restart_active())
	{
		info->flags |= AE_INGAME_WIRE_RESTARTING;
		info->restart_seconds = (unsigned short)PIN(ae_restart_seconds_left(), 0, 0xFFFF);
	}
	info->now = AE_INGAME_WIRE_NO_ENTRY;
	info->next = AE_INGAME_WIRE_NO_ENTRY;
	if (!rotation || !rotation->count)
		return;
	info->total = (unsigned char)MIN(rotation->count, 255);
	if (current != NONE && current < info->total)
		info->now = (unsigned char)current;
	if (next && next >= rotation->entries && next < rotation->entries + rotation->count)
		next_index = (long)(next - rotation->entries);
	if (next_index != NONE && next_index < info->total)
		info->next = (unsigned char)next_index;
	/* (the games to come, in the rotation's order from the next one; from the first without one) */
	for (index = 0; index < rotation->count && info->count < AE_INGAME_WIRE_ENTRIES; index++)
	{
		long at = ((next_index != NONE ? next_index : 0) + index) % rotation->count;
		struct server_entry const *entry = &rotation->entries[at];
		struct ae_ingame_entry *out = &info->entries[info->count];

		if (at >= 255)
			continue;
		snprintf(out->map, sizeof(out->map), "%s", entry->map_name[0] ? entry->map_name : entry->map);
		snprintf(out->gametype, sizeof(out->gametype), "%s", entry->gametype);
		out->flags = entry->rejected[0] ? AE_INGAME_ENTRY_REJECTED : 0;
		out->index = (unsigned char)at;
		info->count++;
	}
}

static void info_send(long machine_index, int role, unsigned long now)
{
	struct ae_server_admin_message message;
	struct ae_ingame_wire info;
	int size;

	info_make(&info, role);
	memset(&message, 0, sizeof(message));
	size = ae_ingame_wire_encode(&info, message.body, (int)sizeof(message.body));
	if (!size)
		return;
	/* (unchanged: not again before SEND_EVERY_MS) */
	if (size == host.sent_size[machine_index] && !memcmp(message.body, host.sent[machine_index], (size_t)size) &&
		now - host.sent_at[machine_index] < SEND_EVERY_MS)
	{
		return;
	}
	if (now - host.sent_at[machine_index] < SEND_SOONEST_MS && !host.urgent && host.sent_size[machine_index])
		return;
	distributed_send_to_machine_reliably(machine_index, &message, _distributed_message_ae_server_admin, 1,
		(word)(sizeof(message.header) + size));
	memcpy(host.sent[machine_index], message.body, (size_t)size);
	host.sent_size[machine_index] = size;
	host.sent_at[machine_index] = now;
}

/* ---------- the sign-in offer (message 114) */

static void offer_send(short machine)
{
	struct
	{
		struct distributed_message_header header;
		unsigned char body[AE_INGAME_OFFER_MAXIMUM];
	} message;
	struct ae_ingame_offer offer;
	unsigned char rows[AE_INGAME_OFFER_HASHES][32], owner[32];
	int count, index, size, role, immunity;
	unsigned long address = distributed_machine_real_address(machine);

	if (!host.salt_made)
	{
		posix_random_bytes(host.salt, sizeof(host.salt));
		host.salt_made = TRUE;
	}
	memset(&offer, 0, sizeof(offer));
	memcpy(offer.salt, host.salt, sizeof(offer.salt));
	if (owner_key()[0] && ae_ingame_key_bytes(owner_key(), owner))
		ae_ingame_offer_add(&offer, owner);
	count = ae_ops_admin_key_rows(rows, AE_INGAME_OFFER_HASHES);
	for (index = 0; index < count; index++)
		ae_ingame_offer_add(&offer, rows[index]);
	if (address && ae_ops_admin_by_address((unsigned int)address, &role, &immunity))
		offer.flags |= AE_INGAME_OFFER_BY_ADDRESS;
	memset(&message, 0, sizeof(message));
	size = ae_ingame_offer_encode(&offer, message.body, (int)sizeof(message.body));
	if (!size)
		return;
	distributed_send_to_machine_reliably(machine, &message, _distributed_message_ae_server_offer, 1,
		(word)(sizeof(message.header) + size));
	host.offered[machine] = TRUE;
}

void ae_server_ingame_host_update(void)
{
	unsigned long now = system_milliseconds();
	short machine;

	if (!halo_server_mode())
		return;
	if (!host.registered)
	{
		static struct delta_peer_moderation_host const moderation =
		{
			NULL, hook_machine_key, hook_key_role, hook_action, hook_bind_answer
		};

		host.registered = TRUE;
		host.started_ms = now;
		delta_peer_game_set_moderation_host(&moderation);
		server_log(_server_log_info, "moderation", "players with a role run the server from the game%s",
			owner_key()[0] ? " (the owner's key set by its starter)" : "");
	}
	/* (0.7.7: the lobby too: its page's RUN THE SERVER row, ae_screen_server_lobby.c) */
	if (game_connection() != _game_connection_network_server || !host_game())
		return;
	/* (admins.txt changed: every machine is offered again with the new list) */
	if (ae_ops_admin_revision() != host.offer_revision)
	{
		host.offer_revision = ae_ops_admin_revision();
		memset(host.offered, 0, sizeof(host.offered));
	}
	for (machine = 0; machine < MACHINES; machine++)
	{
		int role;

		if (!delta_peer_game_moderation_capable(machine))
			host.offered[machine] = FALSE;
		else if (!host.offered[machine])
			offer_send(machine);
		if (!host.has_key[machine])
			continue;
		role = key_role(host.keys[machine], machine, NULL);
		if (role > AE_INGAME_ROLE_NONE)
			info_send(machine, role, now);
	}
	host.urgent = FALSE;
}

unsigned char ae_server_ingame_command_team(char const *line, long count, char **words)
{
	struct network_game *game = host_game();
	char name[16], text[96];
	long slot = NONE, number;
	int team, result;
	char *end;

	(void)line;
	if (count < 3 || !game)
	{
		server_console_output("sv_team: give a player's slot (sv_players) and red or blue");
		return FALSE;
	}
	team = !strcmp(words[2], "red") || !strcmp(words[2], "0") ? 0 : !strcmp(words[2], "blue") ||
		!strcmp(words[2], "1") ? 1 : -1;
	number = strtol(words[1], &end, 10);
	if (!*end && number >= 0 && number < SLOTS && network_player_is_valid(&game->players[number]))
		slot = number;
	if (slot == NONE || team < 0)
	{
		server_console_output("sv_team: give a player's slot (sv_players) and red or blue");
		return FALSE;
	}
	if (!ae_server_mod_may_act_slot(slot, server_commands_role()))
	{
		server_console_output("sv_team: that player's role is above yours");
		return FALSE;
	}
	server_status_player_name(slot, name, sizeof(name));
	result = ae_team_override_game(game->players[slot].machine_index, game->players[slot].controller_index, (short)team,
		name, "An admin", text, (int)sizeof(text));
	if (result < 0 || result > 1)
	{
		char said[160];

		snprintf(said, sizeof(said), "sv_team: %s", text[0] ? text : "that player cannot be moved now");
		server_console_output(said);
		return FALSE;
	}
	{
		char said[160];

		snprintf(said, sizeof(said), "%s %s the %s team", name, result == 1 ? "joins, at their next death," : "is on",
			team ? "blue" : "red");
		server_console_output(said);
	}
	server_log(_server_log_info, "moderation", "%s moved to the %s team", name, team ? "blue" : "red");
	return TRUE;
}

#else

void ae_server_ingame_host_update(void)
{
}

int ae_server_ingame_key_immunity(char const *key, int *immunity)
{
	(void)key;
	(void)immunity;
	return 0;
}

int ae_server_ingame_machine_role(int machine_index, int *immunity)
{
	(void)machine_index;
	if (immunity)
		*immunity = 0;
	return 0;
}

unsigned char ae_server_ingame_command_team(char const *line, long count, char **words)
{
	(void)line;
	(void)count;
	(void)words;
	return FALSE;
}

#endif

/* ======================================================================================== the client's */

static struct
{
	boolean valid;
	struct ae_ingame_wire info;
	unsigned long heard_at;
	unsigned long sign_in_at;
	boolean signed_in_tried;
	/* the server's sign-in offer (message 114): the game signs in only where it matches this PC's key */
	boolean offer_valid;
	struct ae_ingame_offer offer;
	/* the results: the count seen, the last one kept for its reader */
	unsigned long result_count;
	boolean result_new;
	boolean result_done;
	char result[128];
	boolean waiting;
} client;

void ae_server_ingame_reset(void)
{
	memset(&client, 0, sizeof(client));
}

#ifdef HALO_GAME_BROWSER
/* (the server's offer holds this PC's key, or the address it joined from: no other server is sent the key) */
static boolean offer_matches_this_pc(void)
{
	char text[AE_INGAME_KEY_HEX + 8];
	unsigned char key[32];
	boolean have = browser_moderator_key(text, (int)sizeof(text)) && ae_ingame_key_bytes(text, key);

	return ae_ingame_offer_matches(&client.offer, have ? key : NULL) != 0;
}
#endif

void ae_server_ingame_client_tick(unsigned long now)
{
#ifdef HALO_GAME_BROWSER
	struct delta_peer_game_moderation moderation;

	if (game_connection() != _game_connection_network_client)
	{
		if (client.valid || client.signed_in_tried)
			ae_server_ingame_reset();
		return;
	}
	if (client.valid && now - client.heard_at > INFO_STALE_MS)
	{
		client.valid = FALSE;
		platform_log("ae server admin: the server's admin message stopped: no role shown");
	}
	delta_peer_game_client_moderation(&moderation);
	if (!moderation.available)
		return;
	/* (a server that offers moderation: signed in by itself, once, and again later if it never said a role) */
	if (!moderation.signed_in && client.offer_valid && offer_matches_this_pc() &&
		(!client.signed_in_tried || (!client.valid && now - client.sign_in_at > SIGN_IN_AGAIN_MS)))
	{
		client.signed_in_tried = TRUE;
		client.sign_in_at = now;
		if (delta_peer_game_moderation_sign_in())
		{
			char key[AE_INGAME_KEY_HEX + 8];

			/* (the key a server's owner lists in its admins.txt: "key:<this>"; its public half, as the server sees it) */
			if (!browser_moderator_key(key, (int)sizeof(key)))
				key[0] = 0;
			platform_log("ae server admin: signed in to the server's moderation (this PC's moderator key: %s)", key);
		}
	}
	if (moderation.result_count != client.result_count)
	{
		client.result_count = moderation.result_count;
		client.result_new = client.waiting;
		client.waiting = FALSE;
		client.result_done = moderation.result_ok != 0;
		snprintf(client.result, sizeof(client.result), "%s", moderation.result);
		platform_log("ae server admin: the server answered: %s (%s)", client.result, client.result_done ? "done" :
			"not done");
	}
#else
	(void)now;
#endif
}

void ae_server_ingame_map_caption(char const *map, char *text, int size)
{
	wchar_t wide[48];
	int index, length = 0;

	if (size <= 0)
		return;
	wide[0] = 0;
	if (map && map[0])
		ae_scoreboard_map_name(map, wide, (long)NUMBEROF(wide));
	for (index = 0; wide[index] && length < size - 1; index++)
		text[length++] = wide[index] >= 0x20 && wide[index] < 0x7F ? (char)wide[index] : '?';
	text[length] = 0;
	if (!length)
		ae_ingame_map_word(map, text, size);
}

int ae_server_ingame_role(void)
{
	return client.valid ? client.info.role : AE_INGAME_ROLE_NONE;
}

struct ae_ingame_wire const *ae_server_ingame_info(void)
{
	return client.valid ? &client.info : NULL;
}

int ae_server_ingame_act(int action, short machine, int value, char const *text)
{
#ifdef HALO_GAME_BROWSER
	static int const delta_actions[AE_INGAME_ACTIONS] =
	{
		AE_INGAME_DELTA_MOVE, AE_INGAME_DELTA_MUTE, AE_INGAME_DELTA_KICK, AE_INGAME_DELTA_BAN, AE_INGAME_DELTA_NEXT,
		AE_INGAME_DELTA_END, AE_INGAME_DELTA_SAY, AE_INGAME_DELTA_RESTART,
	};
	unsigned int sequence;

	if (!client.valid || action < 0 || action >= AE_INGAME_ACTIONS || !ae_ingame_allows(client.info.allowed, action))
		return 0;
	sequence = delta_peer_game_moderation_action(delta_actions[action], machine, value < 0 ? 0 : value, text ? text : "");
	platform_log("ae server admin: %s sent%s (machine %d, %d)", ae_ingame_command(action), sequence ? "" : " NOT", machine,
		value);
	if (sequence)
		client.waiting = TRUE;
	return sequence != 0;
#else
	(void)action;
	(void)machine;
	(void)value;
	(void)text;
	return 0;
#endif
}

int ae_server_ingame_result(char *text, int size, int *done)
{
	if (!client.result_new)
		return 0;
	client.result_new = FALSE;
	if (text && size > 0)
		snprintf(text, (size_t)size, "%s", client.result);
	if (done)
		*done = client.result_done;
	return 1;
}

int ae_server_ingame_handles_message(unsigned short const *message, unsigned short size)
{
	struct distributed_message_header header;

	if (size < sizeof(header))
		return 0;
	memcpy(&header, message, sizeof(header));
	return header.type == _distributed_message_ae_server_admin || header.type == _distributed_message_ae_server_offer;
}

/* (the offer: the same rules as the admin message's; kept for the sign-in's decision) */
static void offer_handle(long machine_index, unsigned short const *data, unsigned short size)
{
	struct
	{
		struct distributed_message_header header;
		unsigned char body[AE_INGAME_OFFER_MAXIMUM];
	} message;
	struct ae_ingame_offer read;

	if (machine_index != NONE || game_connection() != _game_connection_network_client ||
		network_connection_last_read_was_unreliable() || size < sizeof(message.header) + AE_INGAME_OFFER_HEAD ||
		size > sizeof(message))
	{
		return;
	}
	memcpy(&message, data, size);
	if (GET_MESSAGE_TYPE(message.header.header) != 2 || message.header.type != _distributed_message_ae_server_offer ||
		message.header.count != 1 || !ae_ingame_offer_decode(message.body, (int)(size - sizeof(message.header)), &read))
	{
		return;
	}
	client.offer = read;
	client.offer_valid = TRUE;
	{
		char key[AE_INGAME_KEY_HEX + 8];

#ifdef HALO_GAME_BROWSER
		if (!browser_moderator_key(key, (int)sizeof(key)))
			key[0] = 0;
#else
		key[0] = 0;
#endif
		/* (the key is only ever shown here and in PROFILES: it is sent only where the offer holds it) */
		platform_log("ae server admin: the server's sign-in offer: %d keys listed%s (this PC's moderator key: %s)",
			read.count, (read.flags & AE_INGAME_OFFER_BY_ADDRESS) ? ", this address holds a role" : "", key);
	}
}

/* (the server's alone, over its stream, whole) */
void ae_server_ingame_handle_message(long machine_index, unsigned short const *data, unsigned short size)
{
	struct ae_server_admin_message message;
	struct ae_ingame_wire read;
	boolean was_valid = client.valid;
	int role_before = client.info.role;

	if (size >= sizeof(message.header))
	{
		struct distributed_message_header offer_header;

		memcpy(&offer_header, data, sizeof(offer_header));
		if (offer_header.type == _distributed_message_ae_server_offer)
		{
			offer_handle(machine_index, data, size);
			return;
		}
	}
	if (machine_index != NONE || game_connection() != _game_connection_network_client ||
		network_connection_last_read_was_unreliable() || size < sizeof(message.header) + AE_INGAME_WIRE_HEAD ||
		size > sizeof(message))
	{
		return;
	}
	memcpy(&message, data, size);
	if (GET_MESSAGE_TYPE(message.header.header) != 2 || message.header.type != _distributed_message_ae_server_admin ||
		message.header.count != 1)
	{
		return;
	}
	if (!ae_ingame_wire_decode(message.body, (int)(size - sizeof(message.header)), &read))
		return;
	client.info = read;
	client.valid = TRUE;
	client.heard_at = system_milliseconds();
	if (!was_valid || role_before != read.role)
		platform_log("ae server admin: the server says this player is %s here (actions 0x%x, %d games listed)",
			ae_ingame_role_word(read.role), read.allowed, read.count);
}
