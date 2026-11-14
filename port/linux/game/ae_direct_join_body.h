/*
AE_DIRECT_JOIN_BODY.H

JOIN BY ADDRESS's connection (ae_glue_lobby.c ae_lobby_join_host), included at the end of
source/networking/network_client_manager.c by one AE hook line, because the advertised game and the join's
parameters it needs are that file's own. The searching client connects to the host at an address and port as it
connects to the game its own machine hosts (network_game_client_idle_searching): a temporary advertisement of this
platform, no entry of the game list.

Not a header for anything else: it defines a function, and only that file includes it.
*/

/* 0: this machine's client is not searching yet (try again next frame); 1: the connection is started; -1: it
could not be (the game's error is shown by the caller) */
long ae_network_game_client_join_address(
	unsigned long address,
	unsigned short port)
{
	struct network_game_client *client = network_game_client_searching();
	struct network_advertised_game direct = {0};
	struct network_join_parameters join_parameters;
	struct transport_address server_address;

	if (!client)
		return 0;
	csmemset(&server_address, 0, sizeof(server_address));
	server_address.address.long_words[0] = address;
	server_address.port = port;
	server_address.address_length = IPV4_ADDRESS_LENGTH;
	server_address.address_type = 0;
	direct.platform = network_game_get_local_platform();
	transport_get_nonce(direct.nonce, sizeof(direct.nonce));
	csmemset(&join_parameters, 0, sizeof(join_parameters));
	network_game_generate_join_game_token(join_parameters.join_token);
	return network_game_client_initiate_join_game(client, &direct, &join_parameters, &server_address) ? 1 : -1;
}
