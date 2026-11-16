/* AE's extra gametype settings in play (see the header). The host's settings travel to every client reliably,
when the game is ready and when they change (network_shield_effects.c's way); a build without message 105 drops
it, and plays Halo CE's rules, which are what the default is. */
#include "cseries.h"
#include "game/game.h"
#include "interface/ui_widget.h"
#include "main/main.h"
#include "networking/network_game_globals.h"
#include "saved games/playlist_profile.h"
#include "saved games/saved_game_files.h"
#include "tag_files/files.h"
#include "interface/player_ui.h"
#include "network_distributed.h"
#include "ae_variant_ext.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <xtl.h>

struct ae_variant_ext_header
{
	unsigned long magic;
	unsigned short version;
	unsigned short size;
};

typedef char ae_variant_ext_header_size[sizeof(struct ae_variant_ext_header) == 8 ? 1 : -1];
typedef char ae_variant_ext_fits[AE_VARIANT_EXT_OFFSET >= 0x1AC &&
	AE_VARIANT_EXT_OFFSET + AE_VARIANT_EXT_BLOCK_SIZE <= SAVED_GAME_FILE_BLOCK_SIZE ? 1 : -1];
typedef char ae_variant_ext_signature_size[sizeof(XCALCSIG_SIGNATURE) == 20 ? 1 : -1];

static struct ae_variant_ext chosen;
static struct ae_variant_ext received;
static struct ae_variant_ext sent;
static int sent_valid;

struct ae_variant_ext_message
{
	struct distributed_message_header header;
	unsigned short version;
	unsigned short reserved;
	struct ae_variant_ext ext;
};

int ae_variant_ext_from_block(unsigned char const *block, struct ae_variant_ext *ext)
{
	unsigned char const *data = block + AE_VARIANT_EXT_OFFSET;
	struct ae_variant_ext_header header;
	XCALCSIG_SIGNATURE checksum;
	unsigned long size = sizeof(header) + AE_VARIANT_EXT_SIZE;

	ae_variant_ext_default(ext);
	memcpy(&header, data, sizeof(header));
	if (header.magic != AE_VARIANT_EXT_MAGIC || header.version != AE_VARIANT_EXT_VERSION ||
		header.size != AE_VARIANT_EXT_SIZE)
	{
		return 0;
	}
	saved_game_file_generate_checksum((byte *)data, (word)size, &checksum);
	if (memcmp(&checksum, data + size, sizeof(checksum)))
		return 0;
	memcpy(ext, data + sizeof(header), AE_VARIANT_EXT_SIZE);
	ae_variant_ext_sanitize(ext);
	return 1;
}

void ae_variant_ext_to_block(unsigned char *block, struct ae_variant_ext const *ext)
{
	unsigned char *data = block + AE_VARIANT_EXT_OFFSET;
	struct ae_variant_ext_header header;
	unsigned long size = sizeof(header) + AE_VARIANT_EXT_SIZE;

	memset(data, 0, AE_VARIANT_EXT_BLOCK_SIZE);
	if (!ext || ae_variant_ext_is_default(ext))
		return;
	header.magic = AE_VARIANT_EXT_MAGIC;
	header.version = AE_VARIANT_EXT_VERSION;
	header.size = AE_VARIANT_EXT_SIZE;
	memcpy(data, &header, sizeof(header));
	memcpy(data + sizeof(header), ext, AE_VARIANT_EXT_SIZE);
	saved_game_file_generate_checksum(data, (word)size, (XCALCSIG_SIGNATURE *)(data + size));
}

void ae_variant_ext_carry(unsigned char *block, unsigned char const *old_block)
{
	struct ae_variant_ext ext;

	if (ae_variant_ext_from_block(old_block, &ext))
		ae_variant_ext_to_block(block, &ext);
}

/* (a whole read of the gametype's 512 bytes: a valid file only) */
static int read_block(long playlist_profile_index, unsigned char *block, int writable)
{
	struct file_reference file;
	int read = 0;

	if (playlist_profile_index == NONE ||
		!TEST_FLAG(playlist_profile_index, _saved_game_file_index_valid_bit) ||
		(writable && TEST_FLAG(playlist_profile_index, _saved_game_file_index_read_only_bit)))
	{
		return 0;
	}
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

int playlist_profile_get_ext(long playlist_profile_index, struct ae_variant_ext *ext)
{
	unsigned char block[SAVED_GAME_FILE_BLOCK_SIZE];

	ae_variant_ext_default(ext);
	return read_block(playlist_profile_index, block, 0) && ae_variant_ext_from_block(block, ext);
}

int playlist_profile_set_ext(long playlist_profile_index, struct ae_variant_ext const *ext)
{
	unsigned char block[SAVED_GAME_FILE_BLOCK_SIZE];
	struct file_reference file;
	int written = 0;

	if (!read_block(playlist_profile_index, block, 1))
		return 0;
	ae_variant_ext_to_block(block, ext);
	if (saved_game_files_take_mutex())
	{
		if (saved_game_file_open(&file, playlist_profile_index))
		{
			written = file_set_position(&file, 0) && file_write(&file, SAVED_GAME_FILE_BLOCK_SIZE, block);
			saved_game_file_close(&file, playlist_profile_index);
		}
		saved_game_files_release_mutex();
	}
	playlist_profile_content_changed();
	return written;
}

void ae_variant_ext_choose(struct ae_variant_ext const *ext)
{
	if (ext)
	{
		chosen = *ext;
		ae_variant_ext_sanitize(&chosen);
	}
	else
		ae_variant_ext_default(&chosen);
}

void ae_variant_ext_choose_profile(long playlist_profile_index)
{
	struct ae_variant_ext ext;

	playlist_profile_get_ext(playlist_profile_index, &ext);
	ae_variant_ext_choose(&ext);
}

static struct ae_variant_ext pending;
static int pending_valid;

void ae_variant_ext_pending_block(unsigned char const *block)
{
	/* (a file without a block plays Halo CE's rules, as ae_variant_ext_choose_profile has it) */
	pending_valid = block != NULL;
	if (block)
		ae_variant_ext_from_block(block, &pending);
}

void ae_variant_ext_pending_profile(long playlist_profile_index)
{
	playlist_profile_get_ext(playlist_profile_index, &pending);
	pending_valid = playlist_profile_index != NONE;
}

void ae_variant_ext_pending_apply(void)
{
	if (pending_valid)
		ae_variant_ext_choose(&pending);
	pending_valid = 0;
}

void ae_variant_ext_pending_clear(void)
{
	pending_valid = 0;
}

void ae_variant_ext_set_variant_keeping(struct game_variant *variant)
{
	struct ae_variant_ext keep = chosen;

	player_ui_set_game_variant(variant);
	ae_variant_ext_choose(&keep);
}

/* HALO_AE_EXT=<hex of the block's 48 bytes>: settings for a headless game without a gametype file that holds them
(host and offline games only; a client plays the host's). Read once. */
static struct ae_variant_ext const *env_override(void)
{
	static int read;
	static int have;
	static struct ae_variant_ext env;

	if (!read)
	{
		char const *text = getenv("HALO_AE_EXT");
		size_t length = text ? strlen(text) : 0;
		size_t index;

		read = 1;
		if (length == 2 * AE_VARIANT_EXT_SIZE)
		{
			unsigned char bytes[AE_VARIANT_EXT_SIZE];

			for (index = 0; index < AE_VARIANT_EXT_SIZE; index++)
			{
				unsigned int value;

				if (sscanf(text + 2 * index, "%2x", &value) != 1)
					break;
				bytes[index] = (unsigned char)value;
			}
			if (index == AE_VARIANT_EXT_SIZE)
			{
				memcpy(&env, bytes, sizeof(env));
				ae_variant_ext_sanitize(&env);
				have = 1;
			}
		}
	}
	return have ? &env : NULL;
}

struct ae_variant_ext const *ae_variant_ext_game(void)
{
	struct ae_variant_ext const *env;

	if (game_connection() == _game_connection_network_client)
		return &received;
	env = env_override();
	return env ? env : &chosen;
}

void ae_variant_ext_reset(void)
{
	ae_variant_ext_default(&received);
	sent_valid = 0;
}

static void message_make(struct ae_variant_ext_message *message)
{
	memset(message, 0, sizeof(*message));
	message->version = AE_VARIANT_EXT_VERSION;
	message->ext = chosen;
}

void ae_variant_ext_send(long machine_index)
{
	struct ae_variant_ext_message message;

	if (game_connection() != _game_connection_network_server)
		return;
	message_make(&message);
	distributed_send_to_machine_reliably(machine_index, &message, _distributed_message_ae_variant, 1,
		sizeof(message));
}

void ae_variant_ext_update(void)
{
	struct ae_variant_ext_message message;

	if (!game_in_progress() || main_menu_is_active() || game_connection() != _game_connection_network_server)
		return;
	if (sent_valid && !memcmp(&sent, &chosen, sizeof(sent)))
		return;
	sent = chosen;
	sent_valid = 1;
	message_make(&message);
	distributed_send(&message, _distributed_message_ae_variant, 1, sizeof(message),
		_distributed_to_clients_reliably);
}

int ae_variant_ext_handles_message(unsigned short const *message, unsigned short size)
{
	struct distributed_message_header header;

	if (size < sizeof(header))
		return 0;
	memcpy(&header, message, sizeof(header));
	return header.type == _distributed_message_ae_variant;
}

/* (the host's alone, over its stream, whole) */
void ae_variant_ext_handle_message(long machine_index, unsigned short const *data, unsigned short size)
{
	struct ae_variant_ext_message message;

	if (machine_index != NONE || game_connection() != _game_connection_network_client ||
		network_connection_last_read_was_unreliable() || size != sizeof(message))
		return;
	memcpy(&message, data, sizeof(message));
	if (GET_MESSAGE_TYPE(message.header.header) != 2 || message.header.type != _distributed_message_ae_variant ||
		message.header.count != 1 || message.version != AE_VARIANT_EXT_VERSION)
		return;
	ae_variant_ext_sanitize(&message.ext);
	received = message.ext;
}
