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
#include "ae_weapon_slots.h"

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
/* a pull a tick after the rifle's quickest (0.7.0: each AE rifle's own, ae_burst_rifle_pull_ticks: the BATTLE
RIFLE's bursts 14 ticks apart, the EVO RIFLE's pulls 11, the DMR's 10), as a pull in the burst's recovery is lost;
bots.c adds its skill's few ticks */

/* ---------- the other packs' weapons (ae_weapon_slots.h), by their slot: their tags' names say nothing to bots.c */

/* the SMGs: full-automatic close-range weapons, held down in bursts as the assault rifle is (tap 0, bots.c's own
from the trigger); wanted a little more than the assault rifle (6), less than the pistol (7) at range; kept
closer than bots.c's 7 (the assault rifle's) because their spread opens fast (1 to 2.75 degrees) and
they are strong close up. The silenced one a step further: its spread is tighter (0.25 to 2.5) */
#define AE_BOTS_SMG_VALUE 6
#define AE_BOTS_SMG_RANGE 5.0f
#define AE_BOTS_SILENCED_SMG_RANGE 6.0f
/* the carbine: a precision rifle as the BR, its rounds one a pull: a pull every 11 ticks (its pack's 2.857 rounds a
second, the 0.35 s fire recovery: 10.5 ticks, and a tick more as the BR's), else bots.c holds its trigger
down as an automatic's and a NORMAL bot's held aim sprays it (3% of its rounds hit in the octagon grid) */
#define AE_BOTS_CARBINE_VALUE 8
#define AE_BOTS_CARBINE_RANGE 11.0f
#define AE_BOTS_CARBINE_PULL_TICKS 11
/* the silenced assault rifle: the stock assault rifle's numbers under another name (bots.c values it by the name:
4 for one it does not know), so the assault rifle's value and bots.c's distance for it */
#define AE_BOTS_AR_SILENCED_VALUE 6
#define AE_BOTS_AR_SILENCED_RANGE 7.0f

void ae_bots_weapon_kind(long definition_index, short *value, short *tap_ticks, real *range)
{
	if (ae_burst_rifle_definition(definition_index))
	{
		*value = AE_BOTS_RIFLE_VALUE;
		*tap_ticks = ae_burst_rifle_pull_ticks(definition_index) + 1;
		*range = AE_BOTS_RIFLE_RANGE;
		return;
	}
	switch (ae_weapon_pack_definition_slot(definition_index))
	{
	case AE_WEAPON_SLOT_SMG:
		*value = AE_BOTS_SMG_VALUE;
		*range = AE_BOTS_SMG_RANGE;
		break;
	case AE_WEAPON_SLOT_SMG_SILENCED:
		*value = AE_BOTS_SMG_VALUE;
		*range = AE_BOTS_SILENCED_SMG_RANGE;
		break;
	case AE_WEAPON_SLOT_CARBINE:
		*value = AE_BOTS_CARBINE_VALUE;
		*tap_ticks = AE_BOTS_CARBINE_PULL_TICKS;
		*range = AE_BOTS_CARBINE_RANGE;
		break;
	case AE_WEAPON_SLOT_AR_SILENCED:
		*value = AE_BOTS_AR_SILENCED_VALUE;
		*range = AE_BOTS_AR_SILENCED_RANGE;
		break;
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
