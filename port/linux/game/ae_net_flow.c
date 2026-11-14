/*
AE_NET_FLOW.C

See ae_net_flow.h. The host's side of the table is network_server_manager.c's (ae_net_flow_server.inc), which fills
each slot's address and state before it asks the rules.
*/

#include <stdlib.h>
#include <string.h>

#include "cseries.h"
#include "game/game.h"
#include "main/console.h"
#include "networking/network_game_globals.h"
#include "networking/network_game_manager.h"
#include "networking/network_client_manager.h"
#include "network_distributed.h"
#include "ae_net_flow.h"
#include "../src/ae_lan_broadcast.h"
#include "../src/posix.h"
#include "../src/port_config.h"
#include "../src/server_mode.h"

enum
{
	TELL_MS = 5000,
	BIND_MS = 1000
};

static struct ae_flow_slot ae_flow_slots[HALO_PORT_MAXIMUM_NETWORK_MACHINES];
static unsigned long ae_flow_told_at[HALO_PORT_MAXIMUM_NETWORK_MACHINES];
static unsigned long ae_flow_client_token;
static unsigned long ae_flow_client_sent_at;

unsigned long system_milliseconds(void);
boolean network_distributed_client_send(void *message, word size);
boolean network_distributed_server_send_to_machine_reliably(long machine_index, void *message, word size);
short network_distributed_server_machines(long *machine_indices, short maximum);

struct ae_flow_slot *ae_net_flow_slots(void)
{
	return ae_flow_slots;
}

void ae_net_flow_machine_joined(long machine_index)
{
	if (machine_index < 0 || machine_index >= HALO_PORT_MAXIMUM_NETWORK_MACHINES)
		return;
	ae_flow_slots[machine_index].port = 0;
	ae_flow_slots[machine_index].bound = 0;
	ae_flow_told_at[machine_index] = 0;
	do
	{
		posix_random_bytes(&ae_flow_slots[machine_index].token, sizeof(ae_flow_slots[machine_index].token));
	} while (!ae_flow_slots[machine_index].token);
}

void ae_net_flow_machine_left(long machine_index)
{
	if (machine_index < 0 || machine_index >= HALO_PORT_MAXIMUM_NETWORK_MACHINES)
		return;
	ae_flow_slots[machine_index].port = 0;
	ae_flow_slots[machine_index].bound = 0;
	ae_flow_slots[machine_index].token = 0;
	ae_flow_told_at[machine_index] = 0;
}

static void fill_header(struct distributed_message_header *header, word size)
{
	header->header = 0;
	build_message_header(&header->header, size, 2, 0);
	header->type = _distributed_message_ae_flow;
	header->count = 1;
	header->game_time = game_in_progress() ? game_time_get() : 0;
	header->checkpoint_epoch = 0; /* the lobby's and the connection's messages belong to no world (OpenCE 42) */
}

void ae_net_flow_update(void)
{
	unsigned long now = system_milliseconds();

	if (global_network_game_server_get())
	{
		long machine_indices[HALO_PORT_MAXIMUM_NETWORK_MACHINES];
		short count = network_distributed_server_machines(machine_indices, HALO_PORT_MAXIMUM_NETWORK_MACHINES);
		short index;

		for (index = 0; index < count; index++)
		{
			long machine = machine_indices[index];
			struct
			{
				struct distributed_message_header header;
				struct ae_flow_entry entry;
			} message;

			if (machine < 0 || machine >= HALO_PORT_MAXIMUM_NETWORK_MACHINES)
				continue;
			if (!ae_flow_slots[machine].token)
				ae_net_flow_machine_joined(machine);
			if (ae_flow_told_at[machine] && now - ae_flow_told_at[machine] < TELL_MS)
				continue;
			csmemset(&message, 0, sizeof(message));
			fill_header(&message.header, (word)sizeof(message));
			message.entry.token = ae_flow_slots[machine].token;
			if (network_distributed_server_send_to_machine_reliably(machine, &message, (word)sizeof(message)))
				ae_flow_told_at[machine] = now | 1;
		}
	}
	else if (ae_flow_client_token && !getenv("HALO_NET_FLOW_BIND_OFF") && global_network_game_client_get() && now - ae_flow_client_sent_at >= BIND_MS)
	{
		struct
		{
			struct distributed_message_header header;
			struct ae_flow_entry entry;
		} message;

		csmemset(&message, 0, sizeof(message));
		fill_header(&message.header, (word)sizeof(message));
		message.entry.token = ae_flow_client_token;
		network_distributed_client_send(&message, (word)sizeof(message));
		ae_flow_client_sent_at = now;
	}
}

void ae_net_flow_client_token(void const *entries, long count)
{
	struct ae_flow_entry entry;

	if (count < 1 || global_network_game_server_get())
		return;
	memcpy(&entry, entries, sizeof(entry));
	if (entry.token && entry.token != ae_flow_client_token)
	{
		ae_flow_client_token = entry.token;
		ae_flow_client_sent_at = 0;
	}
}

int ae_net_flow_is_bind(void const *message, unsigned short size, unsigned long *token)
{
	struct distributed_message_header header;
	struct ae_flow_entry entry;

	if (size != sizeof(header) + sizeof(entry))
		return 0;
	memcpy(&header, message, sizeof(header));
	if (header.type != _distributed_message_ae_flow)
		return 0;
	memcpy(&entry, (unsigned char const *)message + sizeof(header), sizeof(entry));
	if (token)
		*token = entry.token;
	return 1;
}

int ae_net_flow_address_is_tunnel(unsigned long address)
{
	static unsigned long own[8];
	static int own_count;
	static unsigned long read_at;
	char const *setting = config_string("server.tunnel_addresses");
	unsigned long now = system_milliseconds();

	if (!address || !setting || !setting[0])
		return 0;
	/* (this computer's addresses, looked at again every ten seconds) */
	if (!read_at || now - read_at >= 10000)
	{
		unsigned long network[8], broadcasts[8];
		int index;

		read_at = now | 1;
		own_count = halo_server_mode() ? ae_lan_networks(network, broadcasts, 8) : 0;
		for (index = 0; index < own_count; index++)
		{
			unsigned long n = network[index];

			own[index] = ((n & 0xFF) << 24) | ((n & 0xFF00) << 8) | ((n >> 8) & 0xFF00) | ((n >> 24) & 0xFF);
		}
	}
	return ae_flow_address_is_tunnel(setting, halo_server_mode(), address, own, own_count);
}
