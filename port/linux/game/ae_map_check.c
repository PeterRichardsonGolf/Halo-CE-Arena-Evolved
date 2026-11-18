/*
AE_MAP_CHECK.C

See ae_map_check.h. The rules are ae_playlist_rules.c's; what the server does with an answer is ae_server_playlist.c's.
*/

#include <stdio.h>
#include <string.h>

#include "cseries.h"
#include "game/game.h"
#include "network_distributed.h"
#include "ae_glue_mcc_maps.h"
#include "ae_map_check.h"
#include "ae_server_playlist.h"
#include "../src/server_mode.h"

void platform_log(char const *format, ...);
boolean network_distributed_client_send_reliably(void *message, word size);
boolean network_distributed_server_send_to_machine_reliably(long machine_index, void *message, word size);

enum
{
	MESSAGE_VERSION = 1,
	ASK_CHUNKS_MAXIMUM = 16
};

struct ae_map_check_ask_message
{
	struct distributed_message_header header;
	unsigned short version;
	unsigned short serial;
	unsigned char chunk;
	unsigned char chunks;
	unsigned char total;
	unsigned char count;
	char names[AE_PL_ASK_PER_MESSAGE][AE_PL_MAP_NAME];
};

struct ae_map_check_answer_message
{
	struct distributed_message_header header;
	unsigned short version;
	unsigned short serial;
	unsigned char flags;
	unsigned char count;
	unsigned short reserved;
	unsigned char bits[AE_PL_MAPS / 8];
};

enum { ANSWER_HAS_MCC = 1 };

static void fill_header(
	struct distributed_message_header *header,
	byte type,
	word size)
{
	header->header = 0;
	build_message_header(&header->header, size, 2, 0);
	header->type = type;
	header->count = 1;
	header->game_time = game_in_progress() ? game_time_get() : 0;
	header->checkpoint_epoch = 0; /* the lobby's and the connection's messages belong to no world (OpenCE 42) */
}

void ae_map_check_ask(
	long machine_index,
	struct ae_pl_map_table const *table,
	unsigned serial)
{
	int chunks = ae_pl_ask_messages(table->count), chunk, at;

	if (chunks < 1 || chunks > ASK_CHUNKS_MAXIMUM)
		return;
	for (chunk = 0; chunk < chunks; chunk++)
	{
		struct ae_map_check_ask_message message;

		memset(&message, 0, sizeof(message));
		fill_header(&message.header, _distributed_message_ae_map_ask, (word)sizeof(message));
		message.version = MESSAGE_VERSION;
		message.serial = (unsigned short)serial;
		message.chunk = (unsigned char)chunk;
		message.chunks = (unsigned char)chunks;
		message.total = (unsigned char)table->count;
		for (at = 0; at < AE_PL_ASK_PER_MESSAGE && chunk * AE_PL_ASK_PER_MESSAGE + at < table->count; at++)
		{
			snprintf(message.names[at], AE_PL_MAP_NAME, "%s", table->name[chunk * AE_PL_ASK_PER_MESSAGE + at]);
			message.count++;
		}
		network_distributed_server_send_to_machine_reliably(machine_index, &message, (word)sizeof(message));
	}
}

/* ---------- a client: what it was asked, and its answer */

static struct
{
	int serial;
	int chunks;
	unsigned got;
	int total;
	char names[AE_PL_MAPS][AE_PL_MAP_NAME];
	int answered_serial;
} asked = { -1, 0, 0, 0, { { 0 } }, -1 };

void ae_map_check_reset(void)
{
	memset(&asked, 0, sizeof(asked));
	asked.serial = -1;
	asked.answered_serial = -1;
}

static void client_answer(void)
{
	struct ae_map_check_answer_message answer;
	int slot, held = 0;

	memset(&answer, 0, sizeof(answer));
	fill_header(&answer.header, _distributed_message_ae_map_answer, (word)sizeof(answer));
	answer.version = MESSAGE_VERSION;
	answer.serial = (unsigned short)asked.serial;
	answer.count = (unsigned char)asked.total;
	if (ae_mcc_menu_on())
	{
		answer.flags |= ANSWER_HAS_MCC;
		for (slot = 0; slot < asked.total; slot++)
			if (ae_mcc_map_present(asked.names[slot]))
			{
				ae_pl_bits_set(answer.bits, slot);
				held++;
			}
	}
	asked.answered_serial = asked.serial;
	platform_log("map check: told the server %s MCC's files, %d of %d asked maps held", answer.flags ? "it has" :
		"it has no", held, asked.total);
	network_distributed_client_send_reliably(&answer, (word)sizeof(answer));
}

int ae_map_check_handles_message(
	unsigned short const *message,
	unsigned short size)
{
	struct distributed_message_header header;

	if (size < sizeof(header))
		return 0;
	memcpy(&header, message, sizeof(header));
	return header.type == _distributed_message_ae_map_ask || header.type == _distributed_message_ae_map_answer;
}

void ae_map_check_handle_message(
	long machine_index,
	unsigned short const *data,
	unsigned short size,
	int reliable)
{
	struct distributed_message_header header;

	if (!reliable)
		return;
	memcpy(&header, data, sizeof(header));
	if (GET_MESSAGE_TYPE(header.header) != 2)
		return;
	if (header.type == _distributed_message_ae_map_answer)
	{
		/* (the host: a machine's answer) */
		struct ae_map_check_answer_message answer;

		if (machine_index == NONE || !halo_server_mode() || size != sizeof(answer))
			return;
		memcpy(&answer, data, sizeof(answer));
		if (answer.header.count != 1 || answer.version != MESSAGE_VERSION || answer.count > AE_PL_MAPS)
			return;
		ae_server_playlist_map_answer(machine_index, answer.serial, answer.flags & ANSWER_HAS_MCC ? 1 : 0, answer.bits,
			answer.count);
	}
	else
	{
		/* (a client: the host's question; answered when all of it is here) */
		struct ae_map_check_ask_message ask;
		int at;

		if (machine_index != NONE || halo_server_mode() || size != sizeof(ask))
			return;
		memcpy(&ask, data, sizeof(ask));
		if (ask.header.count != 1 || ask.version != MESSAGE_VERSION || ask.chunks < 1 || ask.chunks > ASK_CHUNKS_MAXIMUM ||
			ask.chunk >= ask.chunks || ask.total < 1 || ask.total > AE_PL_MAPS || ask.count > AE_PL_ASK_PER_MESSAGE ||
			ask.chunk * AE_PL_ASK_PER_MESSAGE + ask.count > ask.total ||
			ask.chunks != ae_pl_ask_messages(ask.total))
			return;
		if (asked.serial != (int)ask.serial)
		{
			memset(&asked, 0, sizeof(asked));
			asked.serial = ask.serial;
			asked.answered_serial = -1;
		}
		asked.chunks = ask.chunks;
		asked.total = ask.total;
		for (at = 0; at < ask.count; at++)
		{
			ask.names[at][AE_PL_MAP_NAME - 1] = 0;
			memcpy(asked.names[ask.chunk * AE_PL_ASK_PER_MESSAGE + at], ask.names[at], AE_PL_MAP_NAME);
		}
		asked.got |= 1u << ask.chunk;
		if (asked.answered_serial != asked.serial && asked.got == (1u << asked.chunks) - 1)
			client_answer();
	}
}
