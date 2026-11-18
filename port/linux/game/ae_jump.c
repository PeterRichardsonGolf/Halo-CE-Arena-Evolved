/*
AE_JUMP.C

The gametype's JUMP (GAME OPTIONS > SPARTAN, game_variant_options.jump_percent: 50 to 200 percent of Halo CE's jump
speed, 0 Halo CE's; AE's seeds 120, owner 2026-11-01) with a map layout's jump line, which replaces it: the map is
tuned to its own (ae_jump_rules.h). A host gives the gametype's only to the players of machines that run Arena
Evolved 0.7.3 or later (their Delta build name); an older build's or another game's players jump as in Halo CE, as
their own machines move them.
*/

#include "cseries.h"
#include "game/game.h"
#include "game/game_engine.h"
#include "memory/data.h"
#include "game/players.h"
#include "networking/network_game_globals.h"

#include "ae_jump.h"
#include "ae_jump_rules.h"
#include "ae_layout.h"
#include "ae_map_share.h"
#include "../src/delta_peer.h"

void platform_log(char const *format, ...);

real ae_jump_multiplier(long player_index)
{
	static int logged = -1;
	struct game_variant_options const *options = game_variant_options_get();
	real layout = ae_layout_jump_multiplier();
	int layout_percent = layout != 1.0f ? (int)(layout * 100.0f + 0.5f) : 0;
	unsigned char stored = options ? options->jump_percent : AE_JUMP_STOCK;
	int has_it = TRUE, percent;

	if (player_index != NONE && !layout_percent && stored != AE_JUMP_STOCK && global_network_game_server_get())
	{
		struct player_datum *player = player_get(player_index);

		/* (the host: another machine's player has it when that machine runs Arena Evolved 0.7.3 or later, as its
		Delta HELLO's build name says; a machine without one, or not yet heard, jumps as in Halo CE) */
		if (player && player->local_player_index == NONE)
		{
			char build[48];

			has_it = ae_peer_is_ae(player->network_player_data.machine_index) &&
				delta_peer_game_machine_build((int)player->network_player_data.machine_index, build, (int)sizeof(build)) > 0 &&
				ae_jump_build_has_it(build);
		}
	}
	percent = ae_jump_player_percent(layout_percent, stored, has_it);
	if (percent != logged && has_it)
	{
		logged = percent;
		platform_log("jump: %d%% (%s)", percent, layout_percent ? "the map's layout" :
			stored != AE_JUMP_STOCK ? "the gametype's JUMP" : "Halo CE's");
	}
	return percent / 100.0f;
}
