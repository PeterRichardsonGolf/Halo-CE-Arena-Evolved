/*
AE_SPIKE_GHOST.C

A developer's test option (off by default): debug.spike_ghost (HALO_SPIKE_GHOST) "evolved" or "h1" places one of CMT's multiplayer ghosts
(a test vehicle pack's) wherever the map places its own ghost; unset or "stock", the map's ghost.
*/

#include "cseries.h"
#include "cseries/errors.h"
#include "game/game.h"
#include "game/game_engine.h"
#include "networking/network_server_manager.h"
#include "tag_files/tag_files.h"

#include "ae_spike_ghost.h"
#include "ae_vehicle_pack_rules.h"

#include <string.h>

long tag_loaded(long group_tag, const char *name);
struct network_game_server *global_network_game_server_get(void);
char const *config_string(char const *name);

#define AE_SPIKE_GHOST_VEHICLE_GROUP 0x76656869 /* 'vehi' */

static int ae_spike_ghost_choice = -1; /* -1 unread, 0 stock, 1 evolved, 2 h1 */

long ae_spike_ghost_remap(long definition_index)
{
	static char const *const looks[] = { NULL, "vehicles\\evolved\\ghost\\_ghost_mp\\ghost_mp",
		"vehicles\\h1_spirit\\ghost\\_ghost_mp\\ghost_mp" };
	static long logged_game = -1;
	char name[256];
	char const *own;
	long ghost;

	{
		char const *value = config_string("debug.spike_ghost");
		int choice = !value ? 0 : !strcmp(value, "evolved") ? 1 : !strcmp(value, "h1") ? 2 : 0;

		if (choice != ae_spike_ghost_choice)
			error(_error_silent, "spike ghost: %s", choice == 1 ? "CMT upgraded (evolved)" :
				choice == 2 ? "CMT non-upgraded (h1 spirit)" : "the map's");
		ae_spike_ghost_choice = choice;
	}
	if (!ae_spike_ghost_choice || definition_index == NONE)
		return definition_index;
	/* (this machine's own choice: with another machine in the game the two would place different vehicles and fall
	out of step, so it does nothing as a client, and nothing as a host a machine has joined or is joining) */
	if (game_connection() == _game_connection_network_client)
		return definition_index;
	if (game_connection() == _game_connection_network_server)
	{
		struct network_game_server *server = global_network_game_server_get();

		if (server && network_game_server_port_remote_machine_count(server, FALSE) > 0)
			return definition_index;
	}
	own = tag_get_name(definition_index);
	if (!own || strcmp(own, "vehicles\\ghost\\ghost_mp"))
		return definition_index;
	if (!ae_vehicle_pack_name(looks[ae_spike_ghost_choice], name, sizeof(name)) ||
		(ghost = tag_loaded(AE_SPIKE_GHOST_VEHICLE_GROUP, name)) == NONE)
	{
		if (logged_game != 0)
			error(_error_silent, "spike ghost: %s not on the map (the test pack, VEHICLES DESTRUCTIBLE on)", name);
		logged_game = 0;
		return definition_index;
	}
	if (logged_game != 1)
		error(_error_silent, "spike ghost: the map's ghost placed as %s", name);
	logged_game = 1;
	return ghost;
}
