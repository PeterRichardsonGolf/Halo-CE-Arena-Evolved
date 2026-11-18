/*
AE_NET_FLOW_SERVER.INC

Included at the end of source/networking/network_server_manager.c: the host's lookup of the machine a datagram is from
by its address and the port its datagrams come from (ae_net_flow_rules.h), which sees the manager's own tables.
*/

/* the slots as the manager holds the machines now (address, in the game, the host's own), the rules' lookup over them */
static void ae_net_flow_fill(struct network_game_server *server)
{
	struct ae_flow_slot *slots = ae_net_flow_slots();
	long i;

	for (i = 0; i < MAXIMUM_NETWORK_MACHINE_COUNT; i++)
	{
		struct network_game_server_client_machine *machine = &server->client_machines[i];

		slots[i].address = network_game_server_client_machine_addresses[i];
		slots[i].joined = network_game_server_client_machine_is_joined_to_game(server, machine);
		slots[i].local = slots[i].joined && network_game_server_client_machine_is_local(server, machine);
		slots[i].loaded = slots[i].joined && network_game_server_client_machine_is_loaded(server, machine);
	}
}

static long ae_net_flow_pick_machine(
	struct network_game_server *server,
	unsigned long address,
	unsigned short port,
	boolean prefer_remote)
{
	unsigned short before[MAXIMUM_NETWORK_MACHINE_COUNT];
	struct ae_flow_slot *slots = ae_net_flow_slots();
	long i, picked;

	ae_net_flow_fill(server);
	for (i = 0; i < MAXIMUM_NETWORK_MACHINE_COUNT; i++)
		before[i] = slots[i].port;
	picked = ae_flow_pick(slots, MAXIMUM_NETWORK_MACHINE_COUNT, address, port, prefer_remote);
	/* (a machine that sent no token: its port found by elimination, once; or, alone at its address, followed) */
	if (picked != AE_FLOW_NONE && slots[picked].port != before[picked])
	{
		network_event("machine #%ld's datagrams come from port %u of its address (by elimination%s)", picked,
			(unsigned)slots[picked].port, before[picked] ? ", moved" : "");
	}
	return picked;
}

boolean network_game_server_flow_bind(
	struct network_game_server *server,
	struct transport_address const *source_address,
	void const *message,
	short size)
{
	unsigned long token;
	long machine_index;

	if (size <= 0 || !ae_net_flow_is_bind(message, (unsigned short)size, &token))
		return FALSE;
	ae_net_flow_fill(server);
	machine_index = ae_flow_bind(ae_net_flow_slots(), MAXIMUM_NETWORK_MACHINE_COUNT,
		source_address->address.long_words[0], source_address->port, token);
	if (machine_index != AE_FLOW_NONE)
	{
		/* (logged once a machine: its port as the host sees it) */
		static unsigned short logged[MAXIMUM_NETWORK_MACHINE_COUNT];

		if (logged[machine_index] != source_address->port)
		{
			logged[machine_index] = source_address->port;
			network_event("machine #%ld's datagrams come from port %u of its address", machine_index,
				(unsigned)source_address->port);
		}
	}
	return TRUE;
}
