/*
SKULLS_CUSTOM.C

The skulls of this port's own, not the Master Chief Collection's (skulls.c
has those, and their table and screen; Outbreak is skulls_outbreak.c's):
what each changes, which the game's sources ask where it changes it (each
marked "port: skull").

- Woe Is Me: every team the player's enemy, the marines too.
- Armistice: the player's enemies (the Covenant, the Flood, the sentinels)
  not each other's; the marines still the player's allies.
- M Rating: the effects' blood five times over.
- Need Glasses?: every unit cloaked, vehicles and allies too (not the
  players).
- Scarab: a player's human weapon fires rockets, a Covenant one fuel rods,
  one a shot (copied into the level as it loads: randomizer.c).
- Rip & Tear: a player's triggers and grenades held, a punch twice as hard,
  the shield recharging into an overshield.
- Efficient: skulls.c's Famine's, the other way.
- Floor Is Lava: a player standing on the level's floor burns: the shield
  first, then the body, until death. Anything else stood on (a crate, a
  vehicle) is safe.
- Johnny Ammo: a player's reload throws away the rounds left in the
  magazine.
- Leadhead: no headshots (damage.c, units.c).
*/

#include <stdio.h>
#include <string.h>

#include "cseries.h"
#include "cseries/errors.h"
#include "math/real_math.h"
#include "ai/ai.h"
#include "ai/ai_globals.h"
#include "cutscene/cinematics.h"
#include "game/game.h"
#include "game/game_allegiance.h"
#include "game/game_engine.h"
#include "items/weapons.h"
#include "networking/network_game_globals.h"
#include "networking/network_game_manager.h"
#include "objects/objects.h"
#include "tag_files/tag_groups.h"
#include "units/units.h"

#include "randomizer.h"

/* ---------- Woe Is Me, Armistice

the game asks the teams' enmity far more often than the skulls change: the
two are taken each tick */
static boolean woe_is_me;
static boolean armistice;
/* a cutscene playing: with Woe Is Me, no AI sees the player, which would be
killed before it had the controls */
static boolean cutscene;

/* the teams there are (game_allegiance.c's NUMBER_OF_GAME_TEAMS) */
#define SKULLS_GAME_TEAMS 10

/* the rules taken; changed (a cutscene begun or over, a skull picked up or
lost), every actor's props told again who is an enemy, as a script's
change of allegiance would: an actor keeps what it made of a unit when it
first saw it (a player seen in a cutscene, no enemy, for good) */
static void teams_taken(boolean refresh)
{
	boolean was_woe_is_me = woe_is_me;
	boolean was_armistice = armistice;
	boolean was_cutscene = cutscene;

	woe_is_me = skull_active(_skull_woe_is_me);
	armistice = skull_active(_skull_armistice);
	cutscene = cinematic_in_progress();
	if (refresh && ai_globals->ai_initialized_for_map && !network_game_distributed_client() &&
		(was_woe_is_me != woe_is_me || was_armistice != armistice ||
			((woe_is_me || was_woe_is_me) && was_cutscene != cutscene)))
	{
		ai_update_team_status();
	}
}

/* a team the player fights: any AI's but the marines' (and theirs too, with
Woe Is Me); not the default team, an empty vehicle's */
static boolean team_against_player(short team_index)
{
	if (team_index == _game_team_default || team_index == _game_team_player)
		return FALSE;
	return team_index != _game_team_human || woe_is_me;
}

int skulls_team_enmity(short our_team_index, short other_team_index)
{
	if ((!woe_is_me && !armistice) || our_team_index == other_team_index ||
		!VALID_INDEX(our_team_index, SKULLS_GAME_TEAMS) || !VALID_INDEX(other_team_index, SKULLS_GAME_TEAMS))
	{
		return NONE;
	}
	if (woe_is_me && (our_team_index == _game_team_player) != (other_team_index == _game_team_player) &&
		our_team_index != _game_team_default && other_team_index != _game_team_default)
	{
		/* (in a cutscene, the player no one's enemy: the Covenant's neither) */
		return !cutscene;
	}
	if (armistice && team_against_player(our_team_index) && team_against_player(other_team_index))
		return FALSE;
	return NONE;
}

int skulls_team_alliance(short our_team_index, short other_team_index)
{
	int enmity = skulls_team_enmity(our_team_index, other_team_index);

	return enmity == NONE ? NONE : !enmity;
}

/* ---------- M Rating */

#define M_RATING_BLOOD 5.0f
#define BLOOD_KINDS 64

/* whether a particle is blood, by its tag's name, remembered for the level */
static struct
{
	long definition_index;
	boolean blood;
} blood_kinds[BLOOD_KINDS];
static short blood_kind_count;

float skulls_particle_count_scale(long particle_definition_index)
{
	char const *name;
	boolean blood;
	short index;

	if (particle_definition_index == NONE || !skull_active(_skull_m_rating))
		return 1.0f;
	for (index = 0; index < blood_kind_count; index++)
	{
		if (blood_kinds[index].definition_index == particle_definition_index)
			return blood_kinds[index].blood ? M_RATING_BLOOD : 1.0f;
	}
	name = tag_get_name(particle_definition_index);
	blood = name && strstr(name, "blood") != NULL;
	if (blood_kind_count < BLOOD_KINDS)
	{
		blood_kinds[blood_kind_count].definition_index = particle_definition_index;
		blood_kinds[blood_kind_count].blood = blood;
		blood_kind_count++;
	}
	return blood ? M_RATING_BLOOD : 1.0f;
}

/* ---------- Need Glasses? (and Assassins) */

int skulls_unit_cloaked(long unit_index)
{
	struct unit_datum *unit = unit_index == NONE ? NULL :
		(struct unit_datum *)object_try_and_get_and_verify_type(unit_index, _object_mask_unit);

	if (!unit || TEST_FLAG(unit->object.damage_flags, _object_dead_bit) || unit->unit.player_index != NONE)
		return FALSE;
	if (skull_active(_skull_need_glasses))
		return TRUE;
	/* (an enemy's body: not a vehicle, whose riders would vanish with it) */
	return skull_active(_skull_assassins) && unit->object.type == _object_type_biped &&
		skulls_unit_is_enemy(unit_index);
}

/* ---------- Scarab */

static long scarab_rocket = NONE;
static long scarab_fuel_rod = NONE;

/* whether the unit's shots are a player's: its own, or its gunner's (a
vehicle's gun) */
static boolean shots_of_player(long unit_index)
{
	struct unit_datum *unit = unit_index == NONE ? NULL :
		(struct unit_datum *)object_try_and_get_and_verify_type(unit_index, _object_mask_unit);

	if (!unit)
		return FALSE;
	if (unit->unit.player_index != NONE)
		return TRUE;
	return unit->unit.gunner_object_index != NONE && skulls_unit_is_player(unit->unit.gunner_object_index);
}

/* a Covenant weapon: a hand weapon of theirs (randomizer.c's), or a gun of
their vehicles' */
static boolean weapon_is_covenant(long weapon_definition_index)
{
	char const *name = tag_get_name(weapon_definition_index);

	if (!name)
		return FALSE;
	return randomizer_weapon_is_covenant(name) || strstr(name, "banshee") || strstr(name, "ghost") ||
		strstr(name, "wraith") || strstr(name, "shade") || strstr(name, "c gun turret") ||
		strstr(name, "plasma") || strstr(name, "needler");
}

long skulls_scarab_weapon_projectile(long weapon_definition_index)
{
	if (weapon_definition_index == NONE || !skull_active(_skull_scarab))
		return NONE;
	return weapon_is_covenant(weapon_definition_index) ? scarab_fuel_rod : scarab_rocket;
}

long skulls_scarab_projectile(long weapon_definition_index, long unit_index)
{
	if (weapon_definition_index == NONE || !skull_active(_skull_scarab) || !shots_of_player(unit_index))
		return NONE;
	return weapon_is_covenant(weapon_definition_index) ? scarab_fuel_rod : scarab_rocket;
}

/* ---------- Rip & Tear */

#define RIP_AND_TEAR_SHIELD 3.0f

int skulls_rip_and_tear(long unit_index)
{
	return skull_active(_skull_rip_and_tear) && skulls_unit_is_player(unit_index);
}

float skulls_shield_cap(long object_index)
{
	return skulls_rip_and_tear(object_index) ? RIP_AND_TEAR_SHIELD : 1.0f;
}

/* ---------- Floor Is Lava

each player's footing, as its biped last moved: on the level's floor or
not, and when (its physics may not run: a seat's, a cutscene's) */
#define LAVA_PLAYERS 16
/* a full shield burnt away in six seconds, then a full body in twelve */
#define LAVA_SHIELD_TICKS (TICKS_PER_SECOND * 6)
#define LAVA_BODY_TICKS (TICKS_PER_SECOND * 12)
/* the shield kept from recharging, while on the floor and after */
#define LAVA_SHIELD_STUN_TICKS (TICKS_PER_SECOND / 2)

static struct
{
	long unit_index;
	long game_time;
	boolean on_floor;
} lava_footing[LAVA_PLAYERS];

static void lava_reset(void)
{
	short slot;

	for (slot = 0; slot < LAVA_PLAYERS; slot++)
	{
		lava_footing[slot].unit_index = NONE;
		lava_footing[slot].game_time = NONE;
		lava_footing[slot].on_floor = FALSE;
	}
}

void skulls_biped_footing(long biped_index, int airborne, long support_object_index)
{
	struct unit_datum *unit = (struct unit_datum *)object_try_and_get_and_verify_type(biped_index, _object_mask_unit);
	short slot;

	if (!unit || unit->unit.player_index == NONE)
		return;
	slot = (short)(DATUM_INDEX_TO_ABSOLUTE_INDEX(unit->unit.player_index) % LAVA_PLAYERS);
	lava_footing[slot].unit_index = biped_index;
	lava_footing[slot].game_time = game_time_get();
	lava_footing[slot].on_floor = !airborne && support_object_index == NONE;
}

static void lava_tick(void)
{
	long now = game_time_get();
	short slot;

	if (!skull_active(_skull_floor_is_lava) || network_game_distributed_client() || cinematic_in_progress())
		return;
	for (slot = 0; slot < LAVA_PLAYERS; slot++)
	{
		struct unit_datum *unit;

		if (!lava_footing[slot].on_floor || lava_footing[slot].game_time < now - 1 ||
			lava_footing[slot].game_time > now)
		{
			continue;
		}
		unit = (struct unit_datum *)object_try_and_get_and_verify_type(lava_footing[slot].unit_index,
			_object_mask_unit);
		if (!unit || unit->unit.player_index == NONE || TEST_FLAG(unit->object.damage_flags, _object_dead_bit) ||
			TEST_FLAG(unit->object.damage_flags, _object_cannot_take_damage_bit))
		{
			continue;
		}
		if (unit->object.shield_stun_ticks < LAVA_SHIELD_STUN_TICKS)
			unit->object.shield_stun_ticks = LAVA_SHIELD_STUN_TICKS;
		if (unit->object.shield_vitality > 0.0f)
		{
			unit->object.shield_vitality -= 1.0f / LAVA_SHIELD_TICKS;
			if (unit->object.shield_vitality < 0.0f)
				unit->object.shield_vitality = 0.0f;
			continue;
		}
		unit->object.body_vitality -= 1.0f / LAVA_BODY_TICKS;
		if (unit->object.body_vitality <= 0.0f)
		{
			unit->object.body_vitality = 0.0f;
			unit_kill(lava_footing[slot].unit_index);
			lava_footing[slot].on_floor = FALSE;
		}
	}
}

/* ---------- Johnny Ammo */

int skulls_johnny_ammo(long owner_object_index, int reloads_whole)
{
	return reloads_whole && skull_active(_skull_johnny_ammo) && skulls_unit_is_player(owner_object_index);
}

/* ---------- the level's, and each tick */

void skulls_custom_level_loaded(void)
{
	blood_kind_count = 0;
	scarab_rocket = tag_loaded('proj', SKULLS_SCARAB_ROCKET_TAG);
	scarab_fuel_rod = tag_loaded('proj', SKULLS_SCARAB_FUEL_ROD_TAG);
	lava_reset();
	teams_taken(FALSE);
	if (skull_active(_skull_scarab) || skulls_scarab_wanted())
	{
		error(_error_silent, "skulls: scarab's rocket %s, fuel rod %s", scarab_rocket != NONE ? "yes" : "no",
			scarab_fuel_rod != NONE ? "yes" : "no");
	}
}

void skulls_custom_tick(void)
{
	teams_taken(TRUE);
	lava_tick();
}
