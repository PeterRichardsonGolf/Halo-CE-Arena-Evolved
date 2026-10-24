/*
AE_CHUPA_LINK.C

Arena Evolved and ChupathingyCE 0.8 in one Delta game: the game's side
(ae_chupa_link.h; the rules: ae_chupa_link_rules.h).
*/

#include "ae_chupa_link.h"
#include "ae_chupa_link_rules.h"

#include "game/game.h"
#include "game/game_engine.h"
#include "networking/network_client_manager.h"
#include "networking/network_game_protocol.h"
#include "networking/network_messages.h"
#include "networking/network_server_manager.h"
#include "halo_port_limits.h"

#include <stddef.h>
#include <stdio.h>

/* network_client_manager.c's (ChupathingyCE 0.8.0f's) */
void network_game_client_note_host_network_version(unsigned int version);
/* port/linux/game/game_modes.c's */
void game_modes_options_default(struct game_variant_options *options);
#ifdef HALO_GAME_BROWSER
/* port/linux/src/delta_peer_game.c's */
int delta_peer_game_machine_legacy_version(int machine_index);
#endif
/* network_distributed.c's */
void network_distributed_host_notice(char const *text);
/* (the network's machines, network_game_manager.h's MAXIMUM_NETWORK_MACHINE_COUNT) */
#define AE_CHUPA_LINK_MACHINES HALO_PORT_MAXIMUM_NETWORK_MACHINES

boolean ae_chupa_link_record_size_taken(
	unsigned long total_size)
{
	unsigned int version = network_game_client_host_network_version();

	if (!ae_chupa_link_settings_size_taken(version, total_size))
		return FALSE;
	if (version == AE_CHUPA_LINK_VERSION_UNKNOWN && total_size == AE_CHUPA_LINK_SETTINGS_SIZE)
		network_game_client_note_host_network_version(AE_CHUPA_LINK_OLDER_RECORD_VERSION);
	return TRUE;
}

void ae_chupa_link_record_arrived(
	byte *record,
	unsigned long total_size)
{
	struct game_variant_options options;
	static int told;

	if (total_size != AE_CHUPA_LINK_SETTINGS_SIZE)
		return;
	/* (OpenCE's further options, 0x1C to 0x2B, at their defaults: no mode) */
	csmemset(&options, 0, sizeof(options));
	game_modes_options_default(&options);
	ae_chupa_link_settings_widen(record, (unsigned char const *)&options + GAME_VARIANT_OPTIONS_V1_SIZE);
	if (told++ < 3)
	{
		network_event("cross-play: the host's settings record is network %u's (%d bytes): widened, OpenCE's game types off",
			network_game_client_host_network_version(), AE_CHUPA_LINK_SETTINGS_SIZE);
	}
}

boolean ae_chupa_link_plays_version(
	unsigned int version)
{
	return ae_chupa_link_rules_of_version(version, game_connection() == _game_connection_network_client,
		network_game_client_host_network_version(), (unsigned)delta_legacy_announce()) != 0;
}

boolean ae_chupa_link_static_shields(
	void)
{
	return ae_chupa_link_plays_version(AE_CHUPA_LINK_STATIC_SHIELDS_VERSION);
}

void ae_chupa_link_host_frame(
	void)
{
#ifdef HALO_GAME_BROWSER
	static unsigned short dropped[AE_CHUPA_LINK_MACHINES];
	unsigned int announce = (unsigned)delta_legacy_announce();
	long machine_index;

	for (machine_index = 0; machine_index < AE_CHUPA_LINK_MACHINES; machine_index++)
	{
		int hello = delta_peer_game_machine_legacy_version((int)machine_index);
		char notice[160];

		if (!hello || ae_chupa_link_host_keeps(announce, (unsigned)hello))
		{
			dropped[machine_index] = 0;
			continue;
		}
		/* (while it is joined: the drop takes effect when the server next
		looks at its machines; told once a session) */
		if (!network_game_server_drop_machine(machine_index, _rejection_code_version_too_old) ||
			dropped[machine_index] == (unsigned short)hello)
		{
			continue;
		}
		dropped[machine_index] = (unsigned short)hello;
		network_event("cross-play: client machine #%d plays network %d, which can't read this host's (%u) game settings: dropped",
			(int)machine_index, hello, announce);
		snprintf(notice, sizeof(notice),
			"A player's game plays network %d and can't join this game (network %u): ChupathingyCE 0.8.0f or Arena Evolved 0.3.9 or later joins",
			hello, announce);
		network_distributed_host_notice(notice);
	}
#endif
}
