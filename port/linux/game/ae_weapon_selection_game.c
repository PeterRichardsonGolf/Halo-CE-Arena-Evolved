/* What the game in play names of the weapon cache (ae_weapon_selection_game.h). */
#include "cseries.h"
#include "game/game.h"
#include "interface/ui_widget.h"
#include "main/main.h"
#include "networking/network_game_globals.h"
#include "networking/network_server_manager.h"
#include "saved games/playlist_profile.h"
#include "saved games/saved_game_files.h"
#include "tag_files/files.h"
#include "network_distributed.h"
#include "ae_silent_rules.h"
#include "ae_weapon_cache.h"
#include "ae_weapon_selection_block.h"
#include "ae_weapon_selection_game.h"

#include <stdio.h>
#include <string.h>
#include <xtl.h>

long config_integer(char const *name);

static struct ae_weapon_selection_bytes chosen;
static struct ae_weapon_selection_bytes received;
static struct ae_weapon_selection_bytes pending;
/* (debug.network_test_cache_weapon: the test host's block, kept over whatever gametype the lobby then picks) */
static struct ae_weapon_selection_bytes test_block;
static int test_block_valid;
static int pending_valid;
static struct ae_weapon_selection_bytes sent;
static int sent_valid;

struct ae_weapon_cache_message
{
	struct distributed_message_header header;
	unsigned short version;
	unsigned short uses_cache;
	unsigned long generation;
	unsigned char cache_id[AE_CACHE_ID_SIZE];
	unsigned long schema_hash[2];
	unsigned char aecw[AE_WEAPON_SELECTION_AECW_PAYLOAD_SIZE];
};

struct ae_weapon_cache_echo_message
{
	struct distributed_message_header header;
	unsigned short version;
	unsigned short reserved;
	unsigned long generation;
	unsigned char cache_id[AE_CACHE_ID_SIZE];
};

/* 2: the AECW payload of message 109 is the version 3 one (12 bytes); 1 never shipped */
enum { MESSAGE_VERSION = 2 };

void platform_log(char const *format, ...);

/* (a whole read of the gametype's 512 bytes: a valid file only) */
static int read_block(long playlist_profile_index, unsigned char *block)
{
	struct file_reference file;
	int read = 0;

	if (playlist_profile_index == NONE || !TEST_FLAG(playlist_profile_index, _saved_game_file_index_valid_bit))
		return 0;
	playlist_profile_wait_for_write();
	if (saved_game_files_take_mutex())
	{
		if (saved_game_file_open(&file, playlist_profile_index))
		{
			read = file_set_position(&file, 0) && file_read(&file, SAVED_GAME_FILE_BLOCK_SIZE, block);
			saved_game_file_close(&file, playlist_profile_index);
		}
		saved_game_files_release_mutex();
	}
	return read;
}

void ae_weapon_selection_choose_bytes(struct ae_weapon_selection_bytes const *bytes)
{
	memset(&chosen, 0, sizeof(chosen));
	if (bytes && bytes->aecw_present)
	{
		memcpy(chosen.aecw, bytes->aecw, sizeof(chosen.aecw));
		chosen.aecw_present = 1;
	}
}

void ae_weapon_selection_choose_profile(long playlist_profile_index)
{
	unsigned char block[SAVED_GAME_FILE_BLOCK_SIZE];
	struct ae_weapon_selection_bytes bytes;

	memset(&bytes, 0, sizeof(bytes));
	if (read_block(playlist_profile_index, block))
		ae_weapon_selection_block_read(block, &bytes);
	ae_weapon_selection_choose_bytes(&bytes);
}

void ae_weapon_selection_pending_block(unsigned char const *block)
{
	memset(&pending, 0, sizeof(pending));
	pending_valid = block != NULL;
	if (block)
		ae_weapon_selection_block_read(block, &pending);
}

void ae_weapon_selection_pending_apply(void)
{
	if (pending_valid)
		ae_weapon_selection_choose_bytes(&pending);
	pending_valid = 0;
}

void ae_weapon_selection_pending_clear(void)
{
	pending_valid = 0;
}

void ae_weapon_selection_of_game(struct game_variant const *variant, struct game_variant_options const *options,
	struct ae_weapon_selection *selection)
{
	struct ae_weapon_selection_bytes bytes;
	unsigned int ranks[2] = { 0, 0 };
	struct ae_weapon_selection_bytes const *aecw = game_connection() == _game_connection_network_client ? &received :
		test_block_valid ? &test_block : &chosen;

	memset(&bytes, 0, sizeof(bytes));
	if (variant)
		bytes.weapon_pack = variant->universal_variant.weapon_pack;
	if (options)
	{
		bytes.weapon_pack_high = options->weapon_pack_high;
		bytes.pack_start[0] = options->pack_start[0];
		bytes.pack_start[1] = options->pack_start[1];
		bytes.grenades = options->reserved[0];
	}
	if (aecw->aecw_present)
	{
		memcpy(bytes.aecw, aecw->aecw, sizeof(bytes.aecw));
		bytes.aecw_present = 1;
	}
	ae_weapon_selection_read(selection, &bytes, ranks);
}

void ae_weapon_selection_network_test_options(struct game_variant_options *options)
{
	struct ae_weapon_selection_bytes bytes;
	unsigned int weapon = (unsigned int)config_integer("debug.network_test_cache_weapon");

	/* (no test weapon now: the one an earlier test set is forgotten, each test starts with its own weapon) */
	if (!weapon || weapon > 0xFFFF)
	{
		test_block_valid = 0;
		return;
	}
	if (!options)
		return;
	memset(&bytes, 0, sizeof(bytes));
	bytes.aecw_present = 1;
	bytes.aecw[0] = AE_WEAPON_SELECTION_AECW_FLAG_AE_WEAPONS;
	bytes.aecw[2] = (unsigned char)(config_integer("debug.network_test_cache_set") & AE_WEAPON_SELECTION_SET_LAST);
	bytes.aecw[4] = (unsigned char)(weapon & 0xFF);
	bytes.aecw[5] = (unsigned char)(weapon >> 8);
	test_block = bytes;
	test_block_valid = 1;
	options->loadout = _loadout_custom;
	platform_log("weapon cache: test host: everyone starts with weapon %u (set %u)", weapon, (unsigned int)bytes.aecw[2]);
}

static unsigned int running_start_said[2];

static void ae_weapon_selection_running_start_forget(short slot)
{
	running_start_said[slot] = 0;
}

unsigned int ae_weapon_selection_running_start(short slot)
{
	struct ae_weapon_selection selection;

	if (slot < 0 || slot > 1 || !game_engine_running())
		return 0;
	ae_weapon_selection_of_game(game_engine_get_variant(), game_variant_options_get(), &selection);
	if (selection.ae_weapons && selection.start[slot].weapon && selection.start[slot].weapon <= 0x7FFF)
	{
		if (running_start_said[slot] != selection.start[slot].weapon)
		{
			running_start_said[slot] = selection.start[slot].weapon;
			platform_log("weapon cache: the game's loadout slot %d starts with weapon %u (its AE WEAPONS block)", (int)slot,
				selection.start[slot].weapon);
		}
		return selection.start[slot].weapon;
	}
	/* (a game with no start weapon of its own: the next one that names a weapon is said again) */
	ae_weapon_selection_running_start_forget(slot);
	return 0;
}

boolean ae_weapon_selection_game_uses_cache(struct game_variant const *variant, struct game_variant_options const *options)
{
	struct ae_weapon_selection selection;

	ae_weapon_selection_of_game(variant, options, &selection);
	return selection.uses_cache != 0;
}

boolean ae_weapon_selection_guard(struct game_variant const *variant, struct game_variant_options const *options,
	char const *what)
{
	int network = game_connection() == _game_connection_network_server || game_connection() == _game_connection_network_client;

	if (!ae_weapon_selection_guard_refuses(network, ae_weapon_selection_game_uses_cache(variant, options)))
		return FALSE;
	platform_log("weapon cache: no-cache guard: this network game does not use the cache; refusing %s", what ? what : "a cache object");
	return TRUE;
}

/* ---------- messages 109 and 110 */

void ae_weapon_selection_net_reset(void)
{
	memset(&received, 0, sizeof(received));
	sent_valid = 0;
}

static void message_make(struct ae_weapon_cache_message *message)
{
	unsigned int high = 0;
	unsigned char const *id = ae_weapon_cache_id();

	memset(message, 0, sizeof(*message));
	message->version = MESSAGE_VERSION;
	message->generation = ae_weapon_cache_serial();
	if (id)
		memcpy(message->cache_id, id, AE_CACHE_ID_SIZE);
	message->schema_hash[0] = ae_weapon_cache_schema_hash(&high);
	message->schema_hash[1] = high;
	message->uses_cache = 1;
	if (chosen.aecw_present)
		memcpy(message->aecw, chosen.aecw, sizeof(message->aecw));
}

static int cache_host(void)
{
	enum ae_weapon_cache_state state = ae_weapon_cache_state();

	return state == AE_WEAPON_CACHE_LOADED || state == AE_WEAPON_CACHE_DEV;
}

void ae_weapon_selection_net_send(long machine_index)
{
	struct ae_weapon_cache_message message;

	if (game_connection() != _game_connection_network_server || !cache_host())
		return;
	message_make(&message);
	distributed_send_to_machine_reliably(machine_index, &message, AE_CACHE_MESSAGE_HOST_ID, 1, sizeof(message));
}

void ae_weapon_selection_net_update(void)
{
	struct ae_weapon_cache_message message;

	if (!game_in_progress() || main_menu_is_active() || game_connection() != _game_connection_network_server ||
		!cache_host())
		return;
	if (sent_valid && !memcmp(&sent, &chosen, sizeof(sent)))
		return;
	sent = chosen;
	sent_valid = 1;
	message_make(&message);
	distributed_send(&message, AE_CACHE_MESSAGE_HOST_ID, 1, sizeof(message), _distributed_to_clients_reliably);
}

int ae_weapon_selection_net_handles_message(unsigned short const *message, unsigned short size)
{
	struct distributed_message_header header;

	if (size < sizeof(header))
		return 0;
	memcpy(&header, message, sizeof(header));
	return header.type == AE_CACHE_MESSAGE_HOST_ID || header.type == AE_CACHE_MESSAGE_CLIENT_ECHO;
}

void ae_weapon_selection_net_handle_message(long machine_index, unsigned short const *data, unsigned short size)
{
	struct distributed_message_header header;

	memcpy(&header, data, sizeof(header));
	if (GET_MESSAGE_TYPE(header.header) != 2)
		return;
	if (header.type == AE_CACHE_MESSAGE_CLIENT_ECHO)
	{
		/* (the host: a client says its id differs from the one the host has: log it, drop it) */
		struct ae_weapon_cache_echo_message echo;

		if (machine_index == NONE || game_connection() != _game_connection_network_server || size != sizeof(echo))
			return;
		memcpy(&echo, data, sizeof(echo));
		platform_log("weapon cache: machine #%d tells cache %02X%02X%02X%02X%02X%02X%02X%02X (generation %lu); this "
			"host's differs: dropping it", (int)machine_index, echo.cache_id[0], echo.cache_id[1], echo.cache_id[2],
			echo.cache_id[3], echo.cache_id[4], echo.cache_id[5], echo.cache_id[6], echo.cache_id[7], echo.generation);
		network_game_server_drop_machine(machine_index, (short)ae_cache_rejection_word(AE_CACHE_JOIN_DIFFERENT_BUILD));
		return;
	}
	/* (the host's alone, over its stream, whole) */
	{
		struct ae_weapon_cache_message message;
		unsigned char const *own = ae_weapon_cache_id();

		if (machine_index != NONE || game_connection() != _game_connection_network_client ||
			network_connection_last_read_was_unreliable() || size != sizeof(message))
			return;
		memcpy(&message, data, sizeof(message));
		if (message.header.count != 1 || message.version != MESSAGE_VERSION)
			return;
		memset(&received, 0, sizeof(received));
		{
			int index, any = 0;

			for (index = 0; index < AE_WEAPON_SELECTION_AECW_PAYLOAD_SIZE; index++)
				any |= message.aecw[index];
			if (any)
			{
				memcpy(received.aecw, message.aecw, sizeof(received.aecw));
				received.aecw_present = 1;
			}
		}
		/* (the full id against this machine's own: a mismatch past the join's 32-bit digest is a collision or a forged
		block; this client tells the host, which drops it, and logs both) */
		if (own && memcmp(own, message.cache_id, AE_CACHE_ID_SIZE))
		{
			struct ae_weapon_cache_echo_message echo;

			platform_log("weapon cache: the host's cache id differs from this machine's (generation %lu against %u)",
				message.generation, ae_weapon_cache_serial());
			memset(&echo, 0, sizeof(echo));
			echo.version = MESSAGE_VERSION;
			echo.generation = ae_weapon_cache_serial();
			memcpy(echo.cache_id, own, AE_CACHE_ID_SIZE);
			distributed_send(&echo, AE_CACHE_MESSAGE_CLIENT_ECHO, 1, sizeof(echo), _distributed_to_host_reliably);
		}
	}
}
