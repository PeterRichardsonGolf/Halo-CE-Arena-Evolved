/*
AE_BOTS.C

Arena Evolved's side of the bots of local games (ae_bots.h): the pack's
battle rifle in a bot's hands, an energy weapon's battery as its rounds,
the lobby's line when the gametype has no bots (the objective modes' play:
ae_bots_objective.c), and Delta Stats leaving bots' games alone. bots.c (OpenCE PR #367)
stays upstream's but for the AE hook lines.
*/

#include "cseries.h"
#include "game/game_engine.h"
#include "memory/data.h"
#include "game/players.h"
#include "items/weapons.h"
#include "items/weapon_definitions.h"
#include "networking/network_game_globals.h"

#include "bots.h"
#include "ae_burst_rifle.h"
#include "ae_bots.h"
#include "ae_lan_bots.h"
#include "ae_bots_combat_rules.h"
#include "ae_weapon_pack.h"
#include "ae_weapon_ids.h"

#include <string.h>

/* port/linux/src/port_config.c */
long config_integer(char const *name);
int config_boolean(char const *name);
char const *config_string(char const *name);
unsigned long config_changes(void);
void platform_log(char const *format, ...);

/* ---------- the battle rifle */

/* what a bot makes of the rifle: as much as the sniper rifle and the shotgun (bots.c's table: the pistol 7, the
assault rifle 6), so that it keeps it; a rifle's distance, between the pistol's 9 and the sniper's 18 */
#define AE_BOTS_RIFLE_VALUE 8
#define AE_BOTS_RIFLE_RANGE 11.0f
/* (0.7.6) the burst PDW (ae_rifle_profiles.c): the driver's bursts as the rifle's, a pull a tick after its quickest
(16 ticks); a close-range weapon (3 bursts kill inside about 5 wu, 4 past 6.5), wanted as the pistol and kept at the
needler pack's distance */
#define AE_BOTS_PDW_VALUE 7
#define AE_BOTS_PDW_RANGE 6.5f
/* a pull a tick after the rifle's quickest (0.7.0: each AE rifle's own, ae_burst_rifle_pull_ticks: the BATTLE
RIFLE's bursts 14 ticks apart, the EVO RIFLE's pulls 11, the DMR's 10), as a pull in the burst's recovery is lost;
bots.c adds its skill's few ticks */

/* ---------- the other weapons of the cache (ae_weapon_registry.h): their bot columns are the registry's. Their tags' names say
nothing to bots.c, so each row carries the value (the SMGs a little above the assault rifle, the pistol's 7 for the needler
and the magnum, a power weapon's 8), the distance kept (close for the SMGs and the needler, the rifles' 11, a rocket's
launcher's for an explosive so its splash stays off the bot) and the ticks between presses (0: held down; the carbine's 11,
the magnum's 14, the brute shot's 12). tools/weapon_cache/registry.json holds them; port/linux/tests/ae_weapon_registry_test.c
pins the 0.7.5 packs' numbers. */

void ae_bots_weapon_kind(long definition_index, short *value, short *tap_ticks, real *range)
{
	/* (the RAVAGER, whose tags' rounds the driver only paces: no rifle and no PDW, played as its tags and slot make it) */
	if (ae_burst_rifle_definition(definition_index) && !ae_burst_rifle_tags_rounds(definition_index))
	{
		boolean rifle = ae_burst_rifle_rifle(definition_index);

		*value = rifle ? AE_BOTS_RIFLE_VALUE : AE_BOTS_PDW_VALUE;
		*tap_ticks = ae_burst_rifle_pull_ticks(definition_index) + 1;
		*range = rifle ? AE_BOTS_RIFLE_RANGE : AE_BOTS_PDW_RANGE;
		return;
	}
	/* (a cache weapon by its registry row, ae_weapon_registry.h: the value, the distance kept and the ticks between presses
	are its bot columns, which are what the slot switch gave each of the 0.7.5 packs; a tap of -1 leaves bots.c's own
	from the trigger. A weapon the registry does not know keeps bots.c's name search) */
	{
		struct ae_weapon_registry_row const *row = ae_weapon_id_row_of_definition(definition_index);

		if (row)
		{
			*value = row->bot_value;
			*range = row->bot_range;
			if (row->bot_tap_ticks >= 0)
				*tap_ticks = row->bot_tap_ticks;
		}
	}
}

/* a spent weapon (ae_bots_combat_weapon_spent): 1 spent, 0 not, -1 bots.c's own test (rounds) */
int ae_bots_weapon_spent(long weapon_index)
{
	struct weapon_datum *weapon = weapon_get(weapon_index);
	struct weapon_definition *definition = weapon_definition_get(weapon->definition_index);
	struct weapon_magazine_definition *magazine = definition->weapon.magazines.count > 0 ?
		TAG_BLOCK_GET_ELEMENT(&definition->weapon.magazines, 0, struct weapon_magazine_definition) : NULL;

	return ae_bots_combat_weapon_spent(magazine ? (int)definition->weapon.magazines.count : 0,
		magazine ? magazine->rounds_loaded_maximum : 0, magazine ? magazine->rounds_total_maximum : 0, weapon->weapon.age);
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

	static char const *const engines[] = { "none", "CTF", "Slayer", "Oddball", "King of the Hill", "Race", "Bomb" };

	/* (none: the lobby before its gametype is set) */
	if (!local_pregame || config_integer("bots.count") <= 0 || ae_bots_engine_played(game_engine_index) ||
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
	platform_log("bots: none in this game: %s%s%s is a game of %s (bots play Slayer, CTF, Oddball, King, Race and Bomb)",
		name[0] ? "\"" : "", name[0] ? name : "the gametype", name[0] ? "\"" : "",
		game_engine_index >= 0 && game_engine_index < (long)NUMBEROF(engines) ? engines[game_engine_index] : "another kind");
}

/* ---------- Delta Stats */

boolean ae_bots_in_game(void)
{
	struct data_iterator iterator;
	struct player_datum *player;

	/* (a local game's bots are on machines of 113 and up, which in another game are people's; a LAN game's are
	the host's players on its free controller slots: ae_lan_bots.c) */
	if (!player_data)
		return FALSE;
	if (!network_game_is_splitscreen_local())
		return ae_lan_bots_in_game();
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

/* ---------- PLAYFIGHT */

boolean ae_bots_playfight(void)
{
	static unsigned long read_at = (unsigned long)-1;
	static boolean playfight;

	if (read_at != config_changes())
	{
		read_at = config_changes();
		playfight = config_boolean("bots.playfight") != 0;
	}
	return playfight;
}

boolean ae_bots_standing(void)
{
	static unsigned long read_at = (unsigned long)-1;
	static boolean standing;

	if (read_at != config_changes())
	{
		char const *name = config_string("bots.movement_skill");

		read_at = config_changes();
		standing = name && !strcmp(name, "standing");
	}
	return standing;
}
