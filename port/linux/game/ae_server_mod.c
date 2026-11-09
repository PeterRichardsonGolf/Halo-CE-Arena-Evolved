/*
AE_SERVER_MOD.C

See ae_server_mod.h. The files (command_access.txt, whitelist.txt, the hooks' file, the bans by network and by name) are
read at the start, by sv_reload, by the commands that change them and every RELOAD_SECONDS; the door reads bans.txt itself,
fresh, since it runs rarely.
*/

#include "cseries.h"
#include "main/console.h"
#include "main/main.h"
#include "hs/hs.h"
#include "networking/network_game_globals.h"
#include "networking/network_game_manager.h"
#include "networking/network_game_protocol.h"
#include "networking/network_server_manager.h"
#include "networking/network_server_manager_internal.h"
#include "game/game.h"
#include "game/game_engine.h"
#include "game/players.h"
#include "network_chat.h"
#include "network_distributed.h"
#include "network_link.h"
#include "server_director.h"
#include "ae_server_mod.h"
#include "ae_server_ingame.h"
#include "ae_server_bots.h"
#include "ae_lan_bots.h"
#include "ae_net_flow.h"

#include "../src/server_mode.h"
#include "../src/p2p.h"

#if HALO_SERVER_BUILD

#include "ae_server_mod_rules.h"
#include "ae_silent_rules.h"
#include "../src/ae_server_ops.h"
#include "../src/ae_server_admin.h"
#include "../src/posix_server.h"
#include "../src/port_config.h"
#include "../src/server_console.h"
#include "../src/server_events.h"
#include "../src/server_json.h"
#include "../src/server_log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

void platform_translate_path(char const *xbox_path, char *host_path, unsigned long host_path_size);
char *server_commands_run(char const *line, int role, int origin, boolean *succeeded);
unsigned long system_milliseconds(void);

#define BANS_FILE "d:\\bans.txt"

enum
{
	RELOAD_SECONDS = 15,
	MAXIMUM_SLOTS = HALO_PORT_MAXIMUM_NETWORK_PLAYERS,
	MAXIMUM_MACHINES = HALO_PORT_MAXIMUM_NETWORK_MACHINES,
	MAXIMUM_NET_BANS = 512,
	MAXIMUM_BAN_FILE_LINES = 4096,
	EVENT_BATCH = 24,
	EVENT_TEXT_SIZE = 640,
	HOOKS_PER_WINDOW = 10,
	HOOK_WINDOW_MS = 10000,
	FILE_SIZE_MAXIMUM = 1024 * 1024,
	DISK_CHECK_SECONDS = 60,
	DISK_ALERT_GAP = 6 * 3600,
};

static struct
{
	int started;
	unsigned long reload_at;
	unsigned long second_at;
	unsigned long disk_at;
	int rescan;
	/* command_access.txt */
	char access_names[AE_ACCESS_MAXIMUM][AE_COMMAND_NAME_MAXIMUM + 1];
	int access_roles[AE_ACCESS_MAXIMUM];
	int access_count;
	/* whitelist.txt */
	struct ae_whitelist_entry white[AE_WHITELIST_MAXIMUM];
	int white_count;
	/* the bans by network and by name */
	struct ae_ban_record nets[MAXIMUM_NET_BANS];
	int net_count;
	/* the hooks */
	struct ae_hook_rule hooks[AE_HOOK_MAXIMUM];
	int hook_count;
	char hook_file[64];
	unsigned long hook_window;
	int hook_used;
	unsigned long event_after;
	/* each machine's watches */
	struct ae_flood floods[MAXIMUM_MACHINES];
	struct ae_ping_watch pings[MAXIMUM_MACHINES];
	/* the actor of the command being run */
	char pending_by[80];
	int pending_immunity; /* the acting machine's immunity plus one; 0: none given (looked up by the key) */
	int run_role;
	int run_immunity;
	int run_active;
	/* the empty spell */
	unsigned long empty_since;
	int empty_alerted;
	int whitelist_warned;
	/* the hardware ids of machines whose key signed in with a role on this server (this run): the slots kept for
	admins open to them at the door, which cannot see a key */
	char admin_ids[32][2 * P2P_HARDWARE_ID_BYTES + 1];
	int admin_id_count;
} mod;

/* ---------- small things */

static struct network_game *mod_game(void)
{
	struct network_game_server *server = global_network_game_server_get();

	return server ? network_game_server_get_game(server) : NULL;
}

static unsigned long now_seconds(void)
{
	return posix_server_unix_seconds();
}

/* a whole text file of the game's paths (NUL ended, 1 MB at most), or NULL; the caller frees */
static char *read_text(char const *game_path)
{
	FILE *file = fopen(game_path, "rb");
	char *text;
	long length;

	if (!file)
		return NULL;
	fseek(file, 0, SEEK_END);
	length = ftell(file);
	fseek(file, 0, SEEK_SET);
	if (length < 0 || length > FILE_SIZE_MAXIMUM)
	{
		fclose(file);
		return NULL;
	}
	text = malloc((size_t)length + 1);
	if (text && fread(text, 1, (size_t)length, file) != (size_t)length)
	{
		free(text);
		text = NULL;
	}
	if (text)
		text[length] = 0;
	fclose(file);
	return text;
}

/* a plain file name of the data folder: letters, digits, . _ - ; no folder, no leading dot */
static int plain_file_name(char const *name, char const *suffix)
{
	long length = (long)strlen(name), index, tail = suffix ? (long)strlen(suffix) : 0;

	if (length < 1 || length > 48 || name[0] == '.')
		return 0;
	for (index = 0; index < length; index++)
	{
		char c = name[index];

		if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '.' || c == '_' || c == '-'))
			return 0;
	}
	return !tail || (length > tail && !strcmp(name + length - tail, suffix));
}

/* ---------- the files */

static void load_access(void)
{
	char *text = read_text("d:\\command_access.txt"), *line, *next;

	mod.access_count = 0;
	if (!text)
		return;
	for (line = text; line && *line && mod.access_count < AE_ACCESS_MAXIMUM; line = next)
	{
		int role;
		char command[AE_COMMAND_NAME_MAXIMUM + 1];

		next = strchr(line, '\n');
		if (next)
			*next++ = 0;
		if (ae_access_line_parse(line, command, sizeof(command), &role))
		{
			snprintf(mod.access_names[mod.access_count], sizeof(mod.access_names[0]), "%s", command);
			mod.access_roles[mod.access_count++] = role;
		}
	}
	free(text);
}

static void load_whitelist(void)
{
	char *text = read_text("d:\\whitelist.txt"), *line, *next;

	mod.white_count = 0;
	if (!text)
		return;
	for (line = text; line && *line && mod.white_count < AE_WHITELIST_MAXIMUM; line = next)
	{
		next = strchr(line, '\n');
		if (next)
			*next++ = 0;
		if (ae_whitelist_line_parse(line, &mod.white[mod.white_count]))
			mod.white_count++;
	}
	free(text);
}

static void load_hooks(void)
{
	char const *name = config_string("server.on_event");
	char path[80], *text, *line, *next;

	mod.hook_count = 0;
	if (!name[0])
		return;
	if (!plain_file_name(name, NULL))
	{
		server_log(_server_log_warning, "server", "server.on_event is a file name in the data folder (letters, digits, . _ -)");
		return;
	}
	snprintf(path, sizeof(path), "d:\\%s", name);
	text = read_text(path);
	if (!text)
		return;
	for (line = text; line && *line && mod.hook_count < AE_HOOK_MAXIMUM; line = next)
	{
		next = strchr(line, '\n');
		if (next)
			*next++ = 0;
		if (ae_hook_line_parse(line, &mod.hooks[mod.hook_count]))
			mod.hook_count++;
	}
	free(text);
}

static void load_net_bans(void)
{
	char *text = read_text(BANS_FILE), *line, *next;

	mod.net_count = 0;
	if (!text)
		return;
	for (line = text; line && *line && mod.net_count < MAXIMUM_NET_BANS; line = next)
	{
		struct ae_ban_record record;

		next = strchr(line, '\n');
		if (next)
			*next++ = 0;
		if (ae_ban_line_parse(line, &record) && (record.pattern[0] || strchr(record.ip, '/')))
			mod.nets[mod.net_count++] = record;
	}
	free(text);
}

static void load_all(void)
{
	load_access();
	load_whitelist();
	load_hooks();
	load_net_bans();
	mod.reload_at = now_seconds() + RELOAD_SECONDS;
	if (config_boolean("server.whitelist") && !mod.white_count && !mod.whitelist_warned)
	{
		mod.whitelist_warned = 1;
		server_log(_server_log_warning, "server", "server.whitelist is on and whitelist.txt lists no one: only admin rows join");
	}
}

void ae_server_mod_reload(void)
{
	ae_ops_admin_reload();
	load_all();
	mod.rescan = 1;
}

/* ---------- immunity and access */

int ae_server_mod_command_role(char const *command, int own_role)
{
	int index;

	for (index = 0; index < mod.access_count; index++)
	{
		if (!strcmp(mod.access_names[index], command))
			return mod.access_roles[index];
	}
	return own_role;
}

void ae_server_mod_set_actor(char const *by)
{
	snprintf(mod.pending_by, sizeof(mod.pending_by), "%s", by ? by : "");
}

void ae_server_mod_set_actor_immunity(int immunity)
{
	mod.pending_immunity = immunity;
}

void ae_server_mod_run_begin(void)
{
	int role = 0, immunity = 0;

	mod.run_active = 1;
	mod.run_immunity = 0;
	if (!strncmp(mod.pending_by, "admin:", 6) && ae_ops_admin_by_name(mod.pending_by + 6, &role, &immunity))
		mod.run_immunity = immunity;
	/* (0.7.7: a player's action from the game, by the key they signed in with) */
	else if (!strncmp(mod.pending_by, "key:", 4) && ae_server_ingame_key_immunity(mod.pending_by + 4, &immunity))
		mod.run_immunity = immunity;
	if (!strncmp(mod.pending_by, "key:", 4) && mod.pending_immunity > 0)
		mod.run_immunity = mod.pending_immunity - 1;
	mod.pending_by[0] = 0;
	mod.pending_immunity = 0;
}

void ae_server_mod_run_end(void)
{
	mod.run_active = 0;
	mod.run_immunity = 0;
}

int ae_server_mod_may_act_address(unsigned long address, int actor_role)
{
	int role = -1, immunity = 0;

	if (!ae_ops_admin_by_address(address, &role, &immunity))
		return 1;
	return ae_mod_may_act(actor_role, mod.run_active ? mod.run_immunity : 0, role, immunity);
}

int ae_server_mod_may_act_slot(long slot, int actor_role)
{
	struct network_game *game = mod_game();
	long machine;
	unsigned long address;
	int role = -1, immunity = 0, row_role, row_immunity;

	if (!game || slot < 0 || slot >= MAXIMUM_SLOTS)
		return 1;
	machine = game->players[slot].machine_index;
	address = distributed_machine_real_address(machine);
	/* (the target's standing: the stronger of its address row and the key it signed in with; behind a tunnel the
	address is 0 for everyone, and the owner's key has no address row) */
	if (address && ae_ops_admin_by_address(address, &row_role, &row_immunity))
		ae_mod_standing_merge(&role, &immunity, row_role, row_immunity);
	row_role = ae_server_ingame_machine_role((int)machine, &row_immunity);
	if (row_role > 0)
		ae_mod_standing_merge(&role, &immunity, row_role, row_immunity);
	if (role < 0)
		return 1;
	return ae_mod_may_act(actor_role, mod.run_active ? mod.run_immunity : 0, role, immunity);
}

/* ---------- the door */

static int bans_hold_network(unsigned long address)
{
	FILE *file = fopen(BANS_FILE, "r");
	char line[512];
	unsigned long now = now_seconds();
	int held = 0;

	if (!file)
		return 0;
	while (!held && fgets(line, sizeof(line), file))
	{
		struct ae_ban_record record;

		if (ae_ban_line_parse(line, &record) && !ae_ban_over(&record, now) && ae_ban_network_holds(&record, address))
			held = 1;
	}
	fclose(file);
	return held;
}

/* the server's own address (network.address, when one is given): its own client machine joins from it, and is never refused */
static unsigned long own_address(void)
{
	unsigned int bytes[4];
	char end;
	char const *text = config_string("network.address");

	if (text[0] && sscanf(text, "%3u.%3u.%3u.%3u%c", &bytes[0], &bytes[1], &bytes[2], &bytes[3], &end) == 4 &&
		bytes[0] < 256 && bytes[1] < 256 && bytes[2] < 256 && bytes[3] < 256)
	{
		return (unsigned long)bytes[0] << 24 | (unsigned long)bytes[1] << 16 | (unsigned long)bytes[2] << 8 | bytes[3];
	}
	return 0;
}

/* (ae_server_ingame.c) a machine's key signed in with a role: its hardware id passes the slots kept for admins from now on */
void ae_server_mod_admin_hardware_id(char const *hardware_id)
{
	int index;

	if (!hardware_id || !hardware_id[0])
		return;
	for (index = 0; index < mod.admin_id_count; index++)
		if (!strcmp(mod.admin_ids[index], hardware_id))
			return;
	if (mod.admin_id_count >= (int)NUMBEROF(mod.admin_ids))
	{
		memmove(mod.admin_ids[0], mod.admin_ids[1], sizeof(mod.admin_ids) - sizeof(mod.admin_ids[0]));
		mod.admin_id_count--;
	}
	snprintf(mod.admin_ids[mod.admin_id_count++], sizeof(mod.admin_ids[0]), "%s", hardware_id);
}

static int admin_hardware_id_known(char const *hardware_id)
{
	int index;

	if (!hardware_id || !hardware_id[0])
		return 0;
	for (index = 0; index < mod.admin_id_count; index++)
		if (!strcmp(mod.admin_ids[index], hardware_id))
			return 1;
	return 0;
}

/* the server's own client machine, which joins its own game from 127.0.0.1 (an address "auto" counts as a tunnel
agent's): its hardware id is this program's own; before it has joined, the first machine from loopback is it. A game
client on this computer (HOST A SERVER's) has this program's hardware id too, and goes through whole like it */
static int own_client_machine(unsigned long address, char const *hardware_id)
{
	char own[2 * P2P_HARDWARE_ID_BYTES + 1];

	if (address != 0x7F000001UL)
		return 0;
	p2p_hardware_id(own, (int)sizeof(own));
	if (own[0] && hardware_id && !strcmp(own, hardware_id))
		return 1;
	return network_game_client_get_local_machine_index() == NONE;
}

int ae_server_mod_door(struct network_game_server *server, unsigned long address, unsigned long real_address,
	char const *hardware_id, char const *discord_id, short *code)
{
	unsigned long who = real_address ? real_address : address;
	struct network_game *game = network_game_server_get_game(server);
	int role = -1, immunity = 0, admin, tunnel;

	/* (the server's own client machine goes through whole; so does a machine from this computer's address, 127.0.0.1
	among them, unless that address is a tunnel agent's (server.tunnel_addresses; "auto" counts 127.0.0.1 and this
	computer's addresses on a dedicated server): a tunnel's address is no one's, every internet player comes from it,
	so its machines are no one's by address, and still meet the whitelist by hardware id or Discord id and the slots
	kept for admins) */
	if (!halo_server_mode() || own_client_machine(address, hardware_id))
		return 0;
	tunnel = !real_address && ae_net_flow_address_is_tunnel(address);
	if (tunnel)
		who = 0;
	else if (!who || who == 0x7F000001UL || who == own_address())
		return 0;
	if (!mod.started)
	{
		load_all();
		mod.started = 1;
	}
	/* (from a tunnel the hardware id is all that tells the machines apart, for bans, kicks and the limit of machines
	from one address: one that told none is refused) */
	if (tunnel && (!hardware_id || !hardware_id[0]))
	{
		server_log(_server_log_info, "moderation", "a machine from a tunnel's address was refused at the door: its join request carries no hardware id");
		*code = AE_NO_ID_REJECTION_CODE;
		return 1;
	}
	admin = who ? ae_ops_admin_by_address(who, &role, &immunity) : 0;
	if (admin && role >= 1)
		return 0;
	if (who && bans_hold_network(who))
	{
		server_log(_server_log_info, "moderation", "a machine of a banned network was refused at the door");
		*code = _rejection_code_blacklisted_machine;
		return 1;
	}
	if (config_boolean("server.whitelist") && !ae_whitelist_door_ok(mod.white, mod.white_count, who, hardware_id, discord_id) &&
		!ae_whitelist_has_names(mod.white, mod.white_count))
	{
		server_log(_server_log_info, "moderation", "a machine not on the whitelist was refused at the door");
		*code = _rejection_code_blacklisted_machine;
		return 1;
	}
	{
		long reserved = config_integer("server.reserved_slots");

		/* (the people count, not the bots: a bot's seat is there to be taken. An admin by the key it signs in with
		once in the game (its MODERATOR KEY row) cannot be told at the door: a machine whose key signed in with a role
		on this server before passes by its hardware id) */
		if (reserved > 0 && game && game->maximum_players > 0 &&
			ae_server_bots_humans(game) + 1 > game->maximum_players - reserved && !admin_hardware_id_known(hardware_id))
		{
			server_log(_server_log_info, "moderation", "a machine was refused: the last %ld slots are kept for admins", reserved);
			*code = AE_ADMIN_SLOTS_REJECTION_CODE; /* (0.7.9: its own words, not "the game is full") */
			return 1;
		}
	}
	return 0;
}

/* ---------- the chat flood */

int ae_server_mod_chat_flood(long machine_index, char const *name)
{
	long limit = config_integer("server.chat_flood_lines");
	long minutes = config_integer("server.chat_flood_mute_minutes");

	if (!halo_server_mode() || limit <= 0 || machine_index < 0 || machine_index >= MAXIMUM_MACHINES)
		return 0;
	if (!ae_flood_note(&mod.floods[machine_index], system_milliseconds(), (int)limit))
		return 0;
	if (minutes < 1)
		minutes = 2;
	server_log(_server_log_info, "moderation", "%s muted for %ld minutes: chat flood (more than %ld lines in 5 seconds)", name, minutes, limit);
	network_chat_host_mute_named(name, (short)minutes, TRUE);
	return 1;
}

/* ---------- bans, whitelist: the commands' work */

void ae_server_mod_rescan(void)
{
	mod.rescan = 1;
	load_net_bans();
}

static int append_ban(struct ae_ban_record const *record)
{
	char line[640];
	char path[1024];
	FILE *file;
	int written;

	if (!ae_ban_line_format(record, line, sizeof(line)))
		return 0;
	platform_translate_path(BANS_FILE, path, sizeof(path));
	ae_server_backup_file(path);
	file = fopen(BANS_FILE, "a");
	if (!file)
		return 0;
	written = fputs(line, file) >= 0;
	written = fclose(file) == 0 && written;
	return written;
}

static void when_text(char *when, int size)
{
	time_t now = time(NULL);
	struct tm *local = localtime(&now);

	when[0] = 0;
	if (local)
		strftime(when, (size_t)size, "%Y-%m-%d %H:%M:%S", local);
}

int ae_server_mod_ban_write(char const *network, char const *pattern, long seconds, char const *reason, int role, char *why,
	int why_size)
{
	struct ae_ban_record record;
	char kept[40];

	memset(&record, 0, sizeof(record));
	if (network && network[0])
	{
		unsigned long low;

		if (!ae_cidr_normalise(network, kept, sizeof(kept)))
		{
			snprintf(why, (size_t)why_size, "%s is not an address or a network (a.b.c.d/n, n 1 to 32)", network);
			return 0;
		}
		snprintf(record.ip, sizeof(record.ip), "%s", kept);
		/* (not a network that holds an admin row's address the caller outranks not) */
		{
			unsigned int bytes[4];

			if (sscanf(kept, "%u.%u.%u.%u", &bytes[0], &bytes[1], &bytes[2], &bytes[3]) == 4)
			{
				low = (unsigned long)bytes[0] << 24 | (unsigned long)bytes[1] << 16 | (unsigned long)bytes[2] << 8 | bytes[3];
				if (!ae_server_mod_may_act_address(low, role))
				{
					snprintf(why, (size_t)why_size, "that address is an admin's: the caller may not ban it");
					return 0;
				}
			}
		}
	}
	if (pattern && pattern[0])
	{
		if (!ae_pattern_valid(pattern))
		{
			snprintf(why, (size_t)why_size, "a name pattern is 1 to %d printable characters, * for any run, ? for one, and a letter or digit", AE_PATTERN_MAXIMUM);
			return 0;
		}
		snprintf(record.pattern, sizeof(record.pattern), "%s", pattern);
		snprintf(record.players, sizeof(record.players), "%s", pattern);
	}
	if (!record.ip[0] && !record.pattern[0])
	{
		snprintf(why, (size_t)why_size, "give a network or a name pattern");
		return 0;
	}
	if (record.ip[0])
		snprintf(record.players, sizeof(record.players), "%s", record.ip);
	snprintf(record.reason, sizeof(record.reason), "%s%s", reason && reason[0] ? reason : "banned by the server",
		role < 3 ? (role == 2 ? " (by the admin)" : " (by the moderator)") : "");
	if (seconds > 0)
		record.until = now_seconds() + (unsigned long)seconds;
	when_text(record.when, sizeof(record.when));
	if (!append_ban(&record))
	{
		snprintf(why, (size_t)why_size, "bans.txt cannot be written");
		return 0;
	}
	ae_server_mod_rescan();
	return 1;
}

static void whitelist_path(char *path, int size)
{
	platform_translate_path("d:\\whitelist.txt", path, (unsigned long)size);
}

static int whitelist_save(void)
{
	char path[1024];
	char *text = malloc((size_t)(mod.white_count + 1) * 96 + 128);
	size_t length;
	int index, ok;

	if (!text)
		return 0;
	strcpy(text, "# Arena Evolved whitelist: ip:<a.b.c.d[/n]>, hwid:<hex>, discord:<id> or name:<player name>, a line each.\n");
	length = strlen(text);
	for (index = 0; index < mod.white_count; index++)
	{
		char line[96];

		if (ae_whitelist_line_format(&mod.white[index], line, sizeof(line)))
		{
			strcpy(text + length, line);
			length += strlen(line);
		}
	}
	whitelist_path(path, sizeof(path));
	ae_server_backup_file(path);
	ok = posix_server_write_file(path, text, (unsigned long)length) != 0;
	free(text);
	return ok;
}

int ae_server_mod_whitelist_add(char const *text, char *why, int why_size)
{
	struct ae_whitelist_entry entry;
	int index;

	load_whitelist();
	if (!ae_whitelist_line_parse(text, &entry))
	{
		snprintf(why, (size_t)why_size, "an entry is ip:<a.b.c.d[/n]>, hwid:<hex>, discord:<digits> or name:<player name>");
		return 0;
	}
	for (index = 0; index < mod.white_count; index++)
	{
		if (mod.white[index].kind == entry.kind && !strcmp(mod.white[index].value, entry.value))
		{
			snprintf(why, (size_t)why_size, "that entry is listed already");
			return 0;
		}
	}
	if (mod.white_count >= AE_WHITELIST_MAXIMUM)
	{
		snprintf(why, (size_t)why_size, "%d entries at most", AE_WHITELIST_MAXIMUM);
		return 0;
	}
	mod.white[mod.white_count++] = entry;
	if (!whitelist_save())
	{
		mod.white_count--;
		snprintf(why, (size_t)why_size, "whitelist.txt cannot be written");
		return 0;
	}
	return 1;
}

int ae_server_mod_whitelist_remove(char const *text)
{
	struct ae_whitelist_entry entry;
	int index, found = 0;

	load_whitelist();
	if (!ae_whitelist_line_parse(text, &entry))
		return 0;
	for (index = 0; index < mod.white_count; index++)
	{
		if (mod.white[index].kind == entry.kind && !strcmp(mod.white[index].value, entry.value))
		{
			memmove(&mod.white[index], &mod.white[index + 1], sizeof(mod.white[0]) * (size_t)(mod.white_count - index - 1));
			mod.white_count--;
			found = 1;
			break;
		}
	}
	if (found)
		whitelist_save();
	return found;
}

int ae_server_mod_whitelist_text(char *text, int size)
{
	int index, length = 0;

	load_whitelist();
	text[0] = 0;
	for (index = 0; index < mod.white_count && length + 100 < size; index++)
	{
		char line[96];

		if (ae_whitelist_line_format(&mod.white[index], line, sizeof(line)))
			length += snprintf(text + length, (size_t)(size - length), "%s", line);
	}
	if (!length)
		snprintf(text, (size_t)size, "the whitelist is empty\n");
	return length > 0;
}

int ae_server_mod_access_text(char *text, int size)
{
	int index, length = 0;

	text[0] = 0;
	for (index = 0; index < mod.access_count && length + 80 < size; index++)
		length += snprintf(text + length, (size_t)(size - length), "%s needs the %s role\n", mod.access_names[index],
			ae_mod_role_text(mod.access_roles[index]));
	if (!length)
		snprintf(text, (size_t)size, "no command's role is changed (command_access.txt)\n");
	return length > 0;
}

/* GET /api/access: the admin rows (without their secrets), command_access.txt's changes, the whitelist (on or off, its
entries as whitelist.txt writes them) and the reserved slots */
void ae_server_mod_access_json(struct server_json *json)
{
	int index;

	ae_ops_admins_json(json);
	server_json_array_begin(json, "access");
	for (index = 0; index < mod.access_count; index++)
	{
		server_json_object_begin(json, NULL);
		server_json_string(json, "command", mod.access_names[index]);
		server_json_string(json, "role", ae_mod_role_text(mod.access_roles[index]));
		server_json_object_end(json);
	}
	server_json_array_end(json);
	load_whitelist();
	server_json_object_begin(json, "whitelist");
	server_json_boolean(json, "on", config_boolean("server.whitelist") != 0);
	server_json_array_begin(json, "entries");
	for (index = 0; index < mod.white_count; index++)
	{
		char line[96];

		if (ae_whitelist_line_format(&mod.white[index], line, sizeof(line)))
		{
			line[strcspn(line, "\n")] = 0;
			server_json_string(json, NULL, line);
		}
	}
	server_json_array_end(json);
	server_json_object_end(json);
	server_json_integer(json, "reserved_slots", config_integer("server.reserved_slots"));
}

/* ---------- bans in and out as JSON */

void ae_server_mod_bans_export(struct server_json *json)
{
	char *text = read_text(BANS_FILE), *line, *next;
	int count = 0;

	server_json_array_begin(json, "bans");
	for (line = text; line && *line && count < MAXIMUM_BAN_FILE_LINES; line = next)
	{
		struct ae_ban_record record;

		next = strchr(line, '\n');
		if (next)
			*next++ = 0;
		line[strcspn(line, "\r")] = 0;
		if (!ae_ban_line_parse(line, &record))
			continue;
		server_json_object_begin(json, NULL);
		server_json_string(json, "when", record.when);
		server_json_string(json, "ip", record.ip);
		server_json_string(json, "hwid", record.hwid);
		server_json_string(json, "discord_id", record.discord_id);
		server_json_string(json, "name_pattern", record.pattern);
		server_json_string(json, "players", record.players);
		server_json_string(json, "reason", record.reason);
		if (record.until)
			server_json_integer(json, "until", (long long)record.until);
		else
			server_json_null(json, "until");
		server_json_object_end(json);
		count++;
	}
	server_json_array_end(json);
	/* (no bans.txt yet: text is NULL, and the game's free does not take NULL) */
	if (text)
		free(text);
}

/* the bans of a JSON text imported: how many added, skipped (already there or over) and refused; 0 and why if the text is no
list of bans */
int ae_server_mod_bans_import(char const *body, int length, int *added, int *skipped, int *refused, char *why, int why_size)
{
	struct server_json_document document;
	struct ae_ban_record *known = malloc(sizeof(*known) * MAXIMUM_BAN_FILE_LINES);
	int known_count = 0, list, index;
	char *text = read_text(BANS_FILE), *line, *next;
	unsigned long now = now_seconds();

	*added = *skipped = *refused = 0;
	if (!known)
	{
		snprintf(why, (size_t)why_size, "out of memory");
		if (text)
			free(text);
		return 0;
	}
	for (line = text; line && *line && known_count < MAXIMUM_BAN_FILE_LINES; line = next)
	{
		next = strchr(line, '\n');
		if (next)
			*next++ = 0;
		if (ae_ban_line_parse(line, &known[known_count]))
			known_count++;
	}
	/* (no bans.txt yet: nothing known, and the game's free does not take NULL) */
	if (text)
		free(text);
	if (!server_json_parse(&document, body, length, 8))
	{
		snprintf(why, (size_t)why_size, "the text is not JSON");
		free(known);
		return 0;
	}
	list = document.tokens[0].type == _server_json_object ? server_json_member(&document, 0, "bans") : -1;
	if (list < 0 || document.tokens[list].type != _server_json_array)
	{
		snprintf(why, (size_t)why_size, "the JSON is {\"bans\": [ {ip, hwid, discord_id, name_pattern, players, reason, until}, ... ]}");
		server_json_document_free(&document);
		free(known);
		return 0;
	}
	for (index = 0; index < document.tokens[list].size && index < MAXIMUM_BAN_FILE_LINES; index++)
	{
		int item = server_json_element(&document, list, index), member, other, duplicate = 0;
		struct ae_ban_record record;
		double until = 0;

		memset(&record, 0, sizeof(record));
		if (item < 0 || document.tokens[item].type != _server_json_object)
		{
			(*refused)++;
			continue;
		}
		server_json_get_string(&document, server_json_member(&document, item, "ip"), record.ip, sizeof(record.ip));
		server_json_get_string(&document, server_json_member(&document, item, "hwid"), record.hwid, sizeof(record.hwid));
		server_json_get_string(&document, server_json_member(&document, item, "discord_id"), record.discord_id, sizeof(record.discord_id));
		server_json_get_string(&document, server_json_member(&document, item, "name_pattern"), record.pattern, sizeof(record.pattern));
		server_json_get_string(&document, server_json_member(&document, item, "players"), record.players, sizeof(record.players));
		server_json_get_string(&document, server_json_member(&document, item, "reason"), record.reason, sizeof(record.reason));
		server_json_get_string(&document, server_json_member(&document, item, "when"), record.when, sizeof(record.when));
		member = server_json_member(&document, item, "until");
		if (member >= 0 && !server_json_is_null(&document, member))
		{
			if (!server_json_get_number(&document, member, &until) || until < 0 || until > 4000000000.0)
			{
				(*refused)++;
				continue;
			}
			record.until = (unsigned long)until;
		}
		if (!ae_ban_record_clean(&record))
		{
			(*refused)++;
			continue;
		}
		if (ae_ban_over(&record, now))
		{
			(*skipped)++;
			continue;
		}
		for (other = 0; other < known_count; other++)
		{
			if (ae_ban_same(&record, &known[other]))
			{
				duplicate = 1;
				break;
			}
		}
		if (duplicate)
		{
			(*skipped)++;
			continue;
		}
		if (!append_ban(&record))
		{
			snprintf(why, (size_t)why_size, "bans.txt cannot be written");
			server_json_document_free(&document);
			free(known);
			return 0;
		}
		if (known_count < MAXIMUM_BAN_FILE_LINES)
			known[known_count++] = record;
		(*added)++;
	}
	server_json_document_free(&document);
	free(known);
	if (*added)
		ae_server_mod_rescan();
	return 1;
}

int ae_server_mod_bans_export_file(char const *name, char *why, int why_size)
{
	struct server_json json;
	char path[1024], game_path[80];
	int ok;

	if (!plain_file_name(name, ".json"))
	{
		snprintf(why, (size_t)why_size, "the file is a name ending in .json, in the data folder");
		return 0;
	}
	snprintf(game_path, sizeof(game_path), "d:\\%s", name);
	platform_translate_path(game_path, path, sizeof(path));
	server_json_begin(&json);
	server_json_object_begin(&json, NULL);
	server_json_boolean(&json, "ok", 1);
	ae_server_mod_bans_export(&json);
	server_json_object_end(&json);
	ok = posix_server_write_file(path, server_json_text(&json), (unsigned long)strlen(server_json_text(&json))) != 0;
	server_json_free(&json);
	if (!ok)
		snprintf(why, (size_t)why_size, "%s cannot be written", name);
	return ok;
}

int ae_server_mod_bans_import_file(char const *name, int *added, int *skipped, int *refused, char *why, int why_size)
{
	char game_path[80], *text;
	int ok;

	if (!plain_file_name(name, ".json"))
	{
		snprintf(why, (size_t)why_size, "the file is a name ending in .json, in the data folder");
		return 0;
	}
	snprintf(game_path, sizeof(game_path), "d:\\%s", name);
	text = read_text(game_path);
	if (!text)
	{
		snprintf(why, (size_t)why_size, "%s cannot be read (1 MB at most)", name);
		return 0;
	}
	ok = ae_server_mod_bans_import(text, (int)strlen(text), added, skipped, refused, why, why_size);
	free(text);
	return ok;
}

/* ---------- the hooks and the alerts: the events read */

struct event_context
{
	struct server_json_document document;
	char *text;
};

static int event_lookup(void *context, char const *field, char *value, int size)
{
	struct event_context *event = context;
	int token = server_json_member(&event->document, 0, field);
	double number;
	int flag;

	if (token < 0)
		return 0;
	if (server_json_get_string(&event->document, token, value, size))
		return 1;
	if (server_json_get_number(&event->document, token, &number))
	{
		snprintf(value, (size_t)size, "%ld", (long)number);
		return 1;
	}
	if (server_json_get_boolean(&event->document, token, &flag))
	{
		snprintf(value, (size_t)size, "%s", flag ? "true" : "false");
		return 1;
	}
	return 0;
}

struct event_batch
{
	char group[EVENT_BATCH][SERVER_EVENT_GROUP_SIZE];
	char text[EVENT_BATCH][EVENT_TEXT_SIZE];
	int count;
};

static void event_collect(void *context, unsigned long sequence, char const *group, char const *text)
{
	struct event_batch *batch = context;

	(void)sequence;
	if (batch->count >= EVENT_BATCH || strlen(text) >= EVENT_TEXT_SIZE)
		return;
	snprintf(batch->group[batch->count], sizeof(batch->group[0]), "%s", group);
	snprintf(batch->text[batch->count], sizeof(batch->text[0]), "%s", text);
	batch->count++;
}

static void run_hook_command(char const *command)
{
	boolean succeeded = FALSE;
	char *output;

	server_log(_server_log_info, "console", "hook: %s", command);
	output = server_commands_run(command, _server_role_admin, _server_origin_init_file, &succeeded);
	if (output)
		server_console_free(output);
}

static void alert_for_event(char const *event, struct event_context *context)
{
	char text[AE_ALERT_TEXT_SIZE], url[96] = "", code[48] = "", message[120] = "";
	int listed = 1, token;

	if (!strcmp(event, "master"))
	{
		event_lookup(context, "url", url, sizeof(url));
		event_lookup(context, "code", code, sizeof(code));
		event_lookup(context, "message", message, sizeof(message));
		token = server_json_member(&context->document, 0, "listed");
		if (token >= 0)
			server_json_get_boolean(&context->document, token, &listed);
		if (!listed && code[0])
		{
			snprintf(text, sizeof(text), "not listed at %s: %s %s", url, code, message);
			ae_ops_alert("listing", text, 3600);
		}
	}
	else if (!strcmp(event, "warning"))
	{
		event_lookup(context, "text", message, sizeof(message));
		if (strstr(message, "installed now"))
		{
			snprintf(text, sizeof(text), "%s", message);
			ae_ops_alert("update_installed", text, 600);
		}
	}
}

static void events_update(void)
{
	struct event_batch *batch;
	unsigned long next = 0, missed = 0;
	int index, rule;

	if (!mod.event_after)
		mod.event_after = server_events_latest();
	batch = malloc(sizeof(*batch));
	if (!batch)
		return;
	batch->count = 0;
	server_events_read(mod.event_after, EVENT_BATCH, 0, event_collect, batch, &next, &missed);
	if (next)
		mod.event_after = next;
	for (index = 0; index < batch->count; index++)
	{
		struct event_context context;
		char event[AE_KEY_NAME_MAXIMUM + 1] = "";

		memset(&context, 0, sizeof(context));
		if (!server_json_parse(&context.document, batch->text[index], (int)strlen(batch->text[index]), 8) ||
			context.document.tokens[0].type != _server_json_object)
		{
			continue;
		}
		if (!server_json_get_string(&context.document, server_json_member(&context.document, 0, "event"), event, sizeof(event)))
		{
			server_json_document_free(&context.document);
			continue;
		}
		alert_for_event(event, &context);
		for (rule = 0; rule < mod.hook_count; rule++)
		{
			char commands[AE_HOOK_COMMAND_MAXIMUM][AE_HOOK_COMMAND_SIZE];
			int number, which;

			if (!ae_hook_fires(&mod.hooks[rule], event, event_lookup, &context))
				continue;
			number = ae_hook_split(&mod.hooks[rule], commands);
			for (which = 0; which < number; which++)
			{
				char expanded[AE_HOOK_COMMAND_SIZE * 2];

				if (!ae_hook_allow(&mod.hook_window, &mod.hook_used, system_milliseconds(), HOOKS_PER_WINDOW, HOOK_WINDOW_MS))
				{
					server_log(_server_log_warning, "server", "hooks: more than %d commands in 10 seconds: the rest are dropped", HOOKS_PER_WINDOW);
					break;
				}
				if (ae_hook_expand(commands[which], event_lookup, &context, expanded, sizeof(expanded)))
					run_hook_command(expanded);
			}
		}
		server_json_document_free(&context.document);
	}
	free(batch);
}

/* ---------- the players, once a second */

static void kick_machine(long machine_index, char const *name, char const *why)
{
	server_log(_server_log_info, "moderation", "%s kicked: %s", name, why);
	server_director_player_removed(machine_index, "kicked");
	network_game_server_kick_machine_of_player(machine_index, FALSE);
}

static void players_update(void)
{
	struct network_game *game = mod_game();
	int whitelist = config_boolean("server.whitelist");
	long ping_limit = config_integer("server.ping_kick_ms"), ping_seconds = config_integer("server.ping_kick_seconds");
	int seen[MAXIMUM_MACHINES];
	long slot, humans = 0;
	unsigned long now_ms = system_milliseconds();

	memset(seen, 0, sizeof(seen));
	for (slot = 0; game && slot < MAXIMUM_SLOTS; slot++)
	{
		struct network_player const *player = &game->players[slot];
		char name[16];
		long machine = player->machine_index, index, ping = -1;
		unsigned long address;
		char const *hardware_id;
		char discord_id[32] = "";
		int role = -1, immunity = 0, admin, kicked = 0;
		struct player_datum *datum;

		if (!server_status_player_name(slot, name, sizeof(name)))
			continue;
		if (machine < 0 || machine >= MAXIMUM_MACHINES)
			continue;
		/* (a bot is not a person: no whitelist, name ban or ping rule, and it never counts toward "empty") */
		if (ae_lan_bots_network_player(player))
			continue;
		humans++;
		seen[machine] = 1;
		address = distributed_machine_real_address(machine);
		hardware_id = network_game_server_machine_hardware_id(machine);
		admin = ae_ops_admin_by_address(address, &role, &immunity) && role >= 1;
		if (admin)
			continue;
		network_link_machine_verified(machine, discord_id, sizeof(discord_id), NULL, 0);
		if (whitelist && !ae_whitelist_door_ok(mod.white, mod.white_count, address, hardware_id, discord_id) &&
			!ae_whitelist_name_ok(mod.white, mod.white_count, name))
		{
			kick_machine(machine, name, "not on the whitelist");
			continue;
		}
		for (index = 0; index < mod.net_count && !kicked; index++)
		{
			struct ae_ban_record const *ban = &mod.nets[index];

			if (ae_ban_over(ban, now_seconds()))
				continue;
			if ((ban->pattern[0] && ae_glob_match(ban->pattern, name)) || ae_ban_network_holds(ban, address))
			{
				kick_machine(machine, name, ban->pattern[0] ? "a banned name" : "a banned network");
				kicked = 1;
			}
		}
		if (kicked)
			continue;
		datum = game_engine_running() ? distributed_player(player->player_list_index) : NULL;
		if (datum)
			ping = distributed_player_ping(player->player_list_index);
		if (ae_ping_watch_note(&mod.pings[machine], ping, ping_limit, ping_seconds, now_ms))
		{
			char why[64];

			snprintf(why, sizeof(why), "ping over %ld ms for %ld seconds", ping_limit, ping_seconds);
			kick_machine(machine, name, why);
			memset(&mod.pings[machine], 0, sizeof(mod.pings[machine]));
		}
	}
	for (slot = 0; slot < MAXIMUM_MACHINES; slot++)
	{
		if (!seen[slot])
			memset(&mod.pings[slot], 0, sizeof(mod.pings[slot]));
	}
	/* (the empty spell: nobody for server.alert_empty_hours) */
	if (humans)
	{
		mod.empty_since = 0;
		mod.empty_alerted = 0;
	}
	else
	{
		if (!mod.empty_since)
			mod.empty_since = now_seconds();
		if (ae_empty_alert_due(mod.empty_since, now_seconds(), config_integer("server.alert_empty_hours"), mod.empty_alerted))
		{
			char text[AE_ALERT_TEXT_SIZE];

			snprintf(text, sizeof(text), "nobody has played for %ld hours", config_integer("server.alert_empty_hours"));
			ae_ops_alert("empty", text, 0);
			mod.empty_alerted = 1;
		}
	}
}

void ae_server_mod_update(void)
{
	unsigned long now = now_seconds();

	if (!halo_server_mode())
		return;
	if (!mod.started)
	{
		load_all();
		mod.started = 1;
	}
	if (now >= mod.reload_at)
		load_all();
	events_update();
	if (now != mod.second_at || mod.rescan)
	{
		mod.second_at = now;
		mod.rescan = 0;
		players_update();
	}
	if (now >= mod.disk_at)
	{
		long free_mb = ae_ops_disk_free_mb();
		long threshold = config_integer("server.alert_disk_mb");

		mod.disk_at = now + DISK_CHECK_SECONDS;
		if (free_mb >= 0 && threshold > 0 && ae_disk_low((unsigned long long)free_mb * 1024ULL * 1024ULL, threshold))
		{
			char text[AE_ALERT_TEXT_SIZE];

			snprintf(text, sizeof(text), "the data folder's disk has %ld MB free (alert below %ld MB)", free_mb, threshold);
			ae_ops_alert("disk_low", text, DISK_ALERT_GAP);
		}
	}
	/* (0.7.7: the players with a role, signed in from the game) */
	ae_server_ingame_host_update();
}

#else

void ae_server_mod_update(void) {}
void ae_server_mod_reload(void) {}
int ae_server_mod_door(struct network_game_server *server, unsigned long address, unsigned long real_address,
	const char *hardware_id, const char *discord_id, short *code)
{
	(void)server; (void)address; (void)real_address; (void)hardware_id; (void)discord_id; (void)code;
	return 0;
}
int ae_server_mod_chat_flood(long machine_index, const char *name) { (void)machine_index; (void)name; return 0; }
int ae_server_mod_command_role(const char *command, int own_role) { (void)command; return own_role; }
void ae_server_mod_set_actor(const char *by) { (void)by; }
void ae_server_mod_set_actor_immunity(int immunity) { (void)immunity; }
void ae_server_mod_run_begin(void) {}
void ae_server_mod_run_end(void) {}
int ae_server_mod_may_act_slot(long slot, int actor_role) { (void)slot; (void)actor_role; return 1; }
int ae_server_mod_may_act_address(unsigned long address, int actor_role) { (void)address; (void)actor_role; return 1; }

#endif
