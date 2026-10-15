/*
AE_BOTS.C

Arena Evolved's side of the bots of local games (ae_bots.h): the pack's
battle rifle in a bot's hands, the lobby's line when the gametype has no
bots, and Delta Stats leaving bots' games alone. bots.c (OpenCE PR #367)
stays upstream's but for the AE hook lines.
*/

#include "cseries.h"
#include "game/game_engine.h"
#include "memory/data.h"
#include "game/players.h"
#include "networking/network_game_globals.h"

#include "bots.h"
#include "ae_burst_rifle.h"
#include "ae_bots.h"

#include <string.h>

/* port/linux/src/port_config.c */
long config_integer(char const *name);
void platform_log(char const *format, ...);

/* ---------- the battle rifle */

/* what a bot makes of the rifle: as much as the sniper rifle and the shotgun (bots.c's table: the pistol 7, the
assault rifle 6), so that it keeps it; a rifle's distance, between the pistol's 9 and the sniper's 18 */
#define AE_BOTS_RIFLE_VALUE 8
#define AE_BOTS_RIFLE_RANGE 11.0f
/* a pull every 13 ticks: a burst's first rounds come 12 apart at the rifle's quickest (ae_burst_rifle.c: the
burst's own 4 ticks, the 0.20 s recovery, the tick of the pull), and a pull in the recovery is lost, so a tick
more; bots.c adds its skill's few ticks */
#define AE_BOTS_RIFLE_PULL_TICKS 13

void ae_bots_weapon_kind(long definition_index, short *value, short *tap_ticks, real *range)
{
	if (!ae_burst_rifle_definition(definition_index))
		return;
	*value = AE_BOTS_RIFLE_VALUE;
	*tap_ticks = AE_BOTS_RIFLE_PULL_TICKS;
	*range = AE_BOTS_RIFLE_RANGE;
}

/* ---------- the lobby */

static struct
{
	/* the gametype last told of (its engine, NONE for none), so that the line is written once */
	long told_engine;
	boolean told;
} lobby = { NONE, FALSE };

void ae_bots_lobby_check(boolean local_pregame, long game_engine_index, wchar_t const *gametype_name)
{
	char name[16];
	short index;

	static char const *const engines[] = { "none", "CTF", "Slayer", "Oddball", "King of the Hill", "Race" };

	/* (none: the lobby before its gametype is set) */
	if (!local_pregame || config_integer("bots.count") <= 0 || game_engine_index == game_engine_slayer ||
		game_engine_index == game_engine_none)
	{
		lobby.told = FALSE;
		return;
	}
	if (lobby.told && lobby.told_engine == game_engine_index)
		return;
	lobby.told = TRUE;
	lobby.told_engine = game_engine_index;
	for (index = 0; gametype_name && index < 11 && gametype_name[index]; index++)
		name[index] = gametype_name[index] < 0x80 ? (char)gametype_name[index] : '?';
	name[index] = 0;
	platform_log("bots: none in this game: %s%s%s is a game of %s, not Slayer (bots play Slayer and Team Slayer only)",
		name[0] ? "\"" : "", name[0] ? name : "the gametype", name[0] ? "\"" : "",
		game_engine_index >= 0 && game_engine_index < (long)NUMBEROF(engines) ? engines[game_engine_index] : "another kind");
}

/* ---------- Delta Stats */

boolean ae_bots_in_game(void)
{
	struct data_iterator iterator;
	struct player_datum *player;

	/* (local games alone have bots: a machine of 113 and up in another game is a person's) */
	if (!player_data || !network_game_is_splitscreen_local())
		return FALSE;
	data_iterator_new(&iterator, player_data);
	while ((player = (struct player_datum *)data_iterator_next(&iterator)) != NULL)
	{
		if (player->local_player_index == NONE && bots_machine_is_bot(player->network_player_data.machine_index))
			return TRUE;
	}
	return FALSE;
}

boolean ae_bots_kept_from_delta(void)
{
	static boolean logged = FALSE;

	if (!ae_bots_in_game())
	{
		logged = FALSE;
		return FALSE;
	}
	if (!logged)
		platform_log("Delta Stats: a game with bots: not recorded");
	logged = TRUE;
	return TRUE;
}
