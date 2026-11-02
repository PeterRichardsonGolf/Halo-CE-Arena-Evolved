/*
AE_MEDALS.C

MEDALS, the game's side (ae_medals_glue.h; the rules are ae_medal_rules.c's, unit tested): reads the kills, the flags,
the ball, the hill, the checkpoints and the game's end as the game has them, hands them to the rules and gives what
comes out to the players who earned it.

- Who detects: the host, and a local game or split screen (no network). A client never decides a medal: the host's
  message 102 is only a picture to show, and a client takes it for a player of its own machine.
- A medal for a player of this machine goes to ae_medal_display_add at once; for a player of another machine, on
  message 102, reliably (the player's byte and the medal's number). Bots earn none on screen: nobody is there to see
  them, but their kills and deaths keep every count true.
- The weapon of a kill is its damage tag's path (ae_medal_rules_weapon): a map with its own tag names gets no weapon
  medals, and no harm.
- What a kill needs from the game (the blow, the victim's unit, the killer's unit, the vehicle's driver, who was
  hurting whom) is read here, at the kill, and handed to the rules in one struct. The flags and the hill are read each
  tick from the players' statistics and the game type's state; a medal is given on the tick the state moves.
- The records (ae_medal_history.h): each round fired, each blow that lands on a player's unit (with the shield and body
  just before it), a step of each living player every tick, and the weapons the map's spawner made and who took them.
  Only the host keeps them, through detecting().
*/

#include <math.h>
#include <string.h>
#include "cseries.h"
#include "memory/data.h"
#include "game/game.h"
#include "game/game_engine.h"
#include "game/game_globals.h"
#include "game/game_engine_place.h"
#include "game/players.h"
#include "objects/damage.h"
#include "objects/objects.h"
#include "objects/object_types.h"
#include "items/projectile_definitions.h"
#include "items/weapon_definitions.h"
#include "items/weapons.h"
#include "math/real_math.h"
#include "objects/damage_effect_definitions.h"
#include "objects/object_definitions.h"
#include "physics/collision_model_definitions.h"
#include "scenario/scenario.h"
#include "scenario/scenario_definitions.h"
#include "tag_files/tag_files.h"
#include "units/bipeds.h"
#include "units/unit_definitions.h"
#include "units/units.h"

#include "ae_announcer.h"
#include "ae_rifle_profiles.h"
#include "ae_burst_rifle.h"
#include "ae_medal_history.h"
#include "ae_medal_pack.h"
#include "ae_medals.h"
#include "ae_medals_glue.h"
#include "ae_medal_rules.h"
#include "ae_strip_engine.h"
#include "ae_weapon_pack.h"
#include "ae_weapon_slots.h"
#include "bots.h"
#include "network_distributed.h"

void platform_log(const char *format, ...) __attribute__((format(printf, 1, 2)));

/* ---------- constants */

enum
{
	/* the game's oddball ball types (game_engine_oddball.c's), the rules' */
	ODDBALL_NORMAL = 0,
	ODDBALL_REVERSE_TAG,
	ODDBALL_JUGGERNAUT,
};

/* (damage.c's _damage_can_cause_headshots_bit: a damage's flag 1) */
#define DAMAGE_CAN_CAUSE_HEADSHOTS_BIT 1
/* the rounds a head kill takes with the AE rifles: 4 bursts of 3, or 5 single rounds (ae_rifle_profiles.c) */
#define PERFECT_BURST_ROUNDS 12
#define PERFECT_SINGLE_ROUNDS 5
#define ROLE_CACHE_SIZE 32

/* ---------- structures */

/* the blow that may kill, as damage.c saw it this tick */
struct blow
{
	boolean set;
	long tick;
	long object_index;
	short category;
	short side_effect;
	boolean headshot;
	boolean killed_instantly;
	int weapon;
	long definition_index;
	/* the blow was to the head and its damage kills outright on one (not a doubling) */
	boolean instant_head;
	/* the victim's shield stood past full before it */
	boolean overshield;
};

/* ---------- globals */

static char const *const medal_keys[AE_MEDAL_COUNT] = {
#define AE_MEDAL(name, key, points) key,
#include "ae_medal_ids.h"
#undef AE_MEDAL
};

static struct
{
	boolean started;
	boolean ended;
	long last_tick;
	struct ae_medal_game game;
	struct ae_medal_out out;
	struct blow blow;
	/* by slot (the player's absolute index) */
	boolean used[AE_MEDAL_RULES_PLAYERS];
	short identifier[AE_MEDAL_RULES_PLAYERS];
	short team[AE_MEDAL_RULES_PLAYERS];
	/* the statistics as they stood last tick, for the flags (known: they have been read once for this player) */
	boolean known[AE_MEDAL_RULES_PLAYERS];
	short flag_grabs[AE_MEDAL_RULES_PLAYERS];
	short flag_returns[AE_MEDAL_RULES_PLAYERS];
	short flag_scores[AE_MEDAL_RULES_PLAYERS];
	/* what the player held, and whether they stood on the hill, last tick */
	boolean carrying[AE_MEDAL_RULES_PLAYERS];
	boolean on_hill[AE_MEDAL_RULES_PLAYERS];
	boolean alive[AE_MEDAL_RULES_PLAYERS];
	long hill_timer;
	/* the records, and the unit each player had last tick (a new one is a new life) */
	struct ae_history history;
	long unit_of[AE_MEDAL_RULES_PLAYERS];
	/* the last kill the rules found perfect: its victim and the tick (the kill feed reads it) */
	long perfect_victim;
	long perfect_tick;
	/* the shield and body of the unit about to be hit, and who hits it */
	struct
	{
		boolean set;
		long tick;
		long object_index;
		int attacker;
		real shield;
		real body;
	} before;
	/* CTF: where each team's stand is, and where its flag lay last tick, off its carrier */
	real_point3d stand[2];
	boolean stand_known[2];
	real_point3d flag_at[2];
	boolean flag_at_known[2];
	/* what a weapon definition is, found once */
	struct
	{
		long definition;
		int role;
	} roles[ROLE_CACHE_SIZE];
	int role_count;
} medals;

/* ---------- private code */

/* whether this machine decides medals: the host, or a game with no network */
static boolean detecting(void)
{
	short connection = game_connection();

	return (connection == _game_connection_local || connection == _game_connection_network_server) &&
		game_engine_running();
}

static int slot_of(long player_index)
{
	long slot;

	if (player_index == NONE)
		return -1;
	slot = DATUM_INDEX_TO_ABSOLUTE_INDEX(player_index);
	return slot >= 0 && slot < AE_MEDAL_RULES_PLAYERS ? (int)slot : -1;
}

/* the slot of the player a unit belongs to, -1 for none */
static int slot_of_unit(struct unit_datum const *unit)
{
	return unit && unit->unit.player_index != NONE && player_try_and_get(unit->unit.player_index) ?
		slot_of(unit->unit.player_index) : -1;
}

static void start_game(void)
{
	struct game_variant *variant = game_engine_get_variant();
	int mode = AE_MEDAL_MODE_OTHER;
	int assault = 0;
	int ball_type = AE_MEDAL_BALL_NORMAL;

	switch (variant->game_engine_index)
	{
	case game_engine_slayer:
		mode = AE_MEDAL_MODE_SLAYER;
		break;
	case game_engine_ctf:
		mode = AE_MEDAL_MODE_CTF;
		assault = variant->game_engine_variant.ctf.assault != 0;
		break;
	case game_engine_oddball:
		mode = AE_MEDAL_MODE_ODDBALL;
		switch (variant->game_engine_variant.oddball.oddball_ball_type)
		{
		case ODDBALL_REVERSE_TAG: ball_type = AE_MEDAL_BALL_REVERSE_TAG; break;
		case ODDBALL_JUGGERNAUT: ball_type = AE_MEDAL_BALL_JUGGERNAUT; break;
		default: break;
		}
		break;
	case game_engine_king:
		mode = AE_MEDAL_MODE_KING;
		break;
	case game_engine_race:
		mode = AE_MEDAL_MODE_RACE;
		break;
	default:
		break;
	}
	ae_medal_rules_reset(&medals.game, mode, game_engine_has_teams() ? 1 : 0, assault, ball_type);
	memset(medals.used, 0, sizeof(medals.used));
	memset(medals.identifier, 0, sizeof(medals.identifier));
	memset(medals.known, 0, sizeof(medals.known));
	memset(medals.carrying, 0, sizeof(medals.carrying));
	memset(medals.on_hill, 0, sizeof(medals.on_hill));
	memset(medals.alive, 0, sizeof(medals.alive));
	memset(&medals.blow, 0, sizeof(medals.blow));
	memset(&medals.before, 0, sizeof(medals.before));
	memset(medals.stand_known, 0, sizeof(medals.stand_known));
	medals.perfect_victim = NONE;
	medals.perfect_tick = NONE;
	memset(medals.flag_at_known, 0, sizeof(medals.flag_at_known));
	ae_history_reset(&medals.history);
	{
		int slot;

		for (slot = 0; slot < AE_MEDAL_RULES_PLAYERS; slot++)
			medals.unit_of[slot] = NONE;
	}
	if (mode == AE_MEDAL_MODE_CTF && !assault)
	{
		int team;

		for (team = 0; team < 2; team++)
		{
			long flag_index = find_netgame_flag(NULL, 0.0f, 0.0f, _netgame_flag_ctf_flag, (short)team);

			if (flag_index != NONE)
			{
				medals.stand[team] = TAG_BLOCK_GET_ELEMENT(&global_scenario_get()->netgame_flags, flag_index,
					struct scenario_netgame_flag)->position;
				medals.stand_known[team] = TRUE;
			}
		}
	}
	medals.hill_timer = mode == AE_MEDAL_MODE_KING ? ae_strip_king_hill_timer() : 0;
	medals.ended = FALSE;
	medals.started = TRUE;
	medals.last_tick = game_time_get();
}

/* a game is under way for the rules; a new one when the game's time went back */
static void ensure_game(void)
{
	/* (a game restarted on the same map: nothing showing from the last) */
	if (medals.started && game_time_get() < medals.last_tick)
		ae_medal_display_reset();
	if (!medals.started || game_time_get() < medals.last_tick)
		start_game();
	medals.last_tick = game_time_get();
}

/* who is in the game, their team; a slot taken by another player starts from nothing */
static void refresh_players(void)
{
	struct data_iterator iterator;
	struct player_datum *player;
	boolean present[AE_MEDAL_RULES_PLAYERS];
	int slot;

	memset(present, 0, sizeof(present));
	data_iterator_new(&iterator, player_data);
	while ((player = (struct player_datum *)data_iterator_next(&iterator)) != NULL)
	{
		slot = slot_of(iterator.datum_index);
		if (slot < 0)
			continue;
		/* (one who has left is out of the game's medals, the end's too) */
		if (player->quit_out_of_game)
			continue;
		present[slot] = TRUE;
		if (medals.identifier[slot] != player->identifier)
		{
			ae_medal_rules_clear_player(&medals.game, slot);
			ae_history_clear_player(&medals.history, slot);
			medals.unit_of[slot] = NONE;
			medals.identifier[slot] = player->identifier;
			medals.known[slot] = FALSE;
			medals.carrying[slot] = FALSE;
			medals.on_hill[slot] = FALSE;
		}
		medals.team[slot] = (short)player->team_index;
	}
	for (slot = 0; slot < AE_MEDAL_RULES_PLAYERS; slot++)
	{
		medals.used[slot] = present[slot];
		if (!present[slot])
		{
			/* (a team that no player has) */
			medals.team[slot] = -2;
			medals.identifier[slot] = 0;
		}
	}
}

/* whether the unit has a flag in its hands (CTF's flag, the oddball's ball) */
static boolean unit_has_flag(struct unit_datum const *unit)
{
	short index;

	if (!unit)
		return FALSE;
	for (index = 0; index < MAXIMUM_WEAPONS_PER_UNIT; index++)
	{
		long weapon_index = unit->unit.weapon_object_indices[index];

		if (weapon_index != NONE && weapon_try_and_get(weapon_index) && weapon_is_flag(weapon_index))
			return TRUE;
	}
	return FALSE;
}

/* the unit's weapon in hand, NULL for none */
static struct weapon_datum *unit_weapon_in_hand(struct unit_datum const *unit)
{
	short current;

	if (!unit)
		return NULL;
	current = unit->unit.current_weapon_index;
	if (current < 0 || current >= MAXIMUM_WEAPONS_PER_UNIT || unit->unit.weapon_object_indices[current] == NONE)
		return NULL;
	return weapon_try_and_get(unit->unit.weapon_object_indices[current]);
}

/* the unit's vitality as a fraction of its most, shield or body */
static real unit_body_fraction(long unit_index)
{
	real most = object_get_maximum_body_vitality(unit_index, TRUE);

	return most > 0.0f ? object_get_actual_body_vitality(unit_index, TRUE) / most : 1.0f;
}

static real unit_shield_fraction(long unit_index)
{
	real most = object_get_maximum_shield_vitality(unit_index, TRUE);

	return most > 0.0f ? object_get_actual_shield_vitality(unit_index, TRUE) / most : 0.0f;
}

/* the slot of the player who drives the vehicle the killer's unit rides, if not the killer */
static int vehicle_driver_slot(struct unit_datum const *killer_unit, int killer_slot)
{
	struct unit_datum *vehicle, *driver;
	int slot;

	if (!killer_unit || killer_unit->object.parent_object_index == NONE)
		return -1;
	vehicle = (struct unit_datum *)object_try_and_get_and_verify_type(killer_unit->object.parent_object_index,
		_object_mask_vehicle);
	if (!vehicle || vehicle->unit.driver_object_index == NONE)
		return -1;
	driver = unit_try_and_get(vehicle->unit.driver_object_index);
	slot = slot_of_unit(driver);
	return slot == killer_slot ? -1 : slot;
}

/* a medal for a player of this game: on screen for a local player, over the network for another machine's, nothing
for a bot */
static void grant(int slot, int medal)
{
	struct player_datum *player = distributed_player((short)slot);

	if (!player || medal < 0 || medal >= AE_MEDAL_COUNT)
		return;
	if (bots_machine_is_bot(player->network_player_data.machine_index))
		return;
	platform_log("medal: player %d earned %s", slot, medal_keys[medal]);
	if (player->local_player_index != NONE)
	{
		ae_medal_display_add(player->local_player_index, medal);
		return;
	}
	if (game_connection() == _game_connection_network_server)
	{
		long machine = distributed_player_machine((short)slot);

		if (machine != NONE)
		{
			struct
			{
				struct distributed_message_header header;
				struct distributed_medal medal;
			} message;

			memset(&message, 0, sizeof(message));
			message.medal.player_index = (byte)slot;
			message.medal.medal = (byte)medal;
			distributed_send_to_machine_reliably(machine, &message, _distributed_message_medal, 1, (word)sizeof(message));
		}
	}
}

static void grant_all(void)
{
	int index;

	for (index = 0; index < medals.out.count; index++)
		grant(medals.out.items[index].player, medals.out.items[index].medal);
}

/* the stored blow, if it is the one that killed this player's unit this tick */
static struct blow const *blow_of(long dead_player_index)
{
	if (medals.blow.set && medals.blow.tick == game_time_get() &&
		player_index_from_unit_index(medals.blow.object_index) == dead_player_index)
	{
		return &medals.blow;
	}
	return NULL;
}

/* whether a living teammate of the killer was hurt by the victim lately */
static boolean teammate_was_hurt_by(long dead_player_index, int killer_slot)
{
	struct data_iterator iterator;
	struct player_datum *player;

	data_iterator_new(&iterator, player_data);
	while ((player = (struct player_datum *)data_iterator_next(&iterator)) != NULL)
	{
		int slot = slot_of(iterator.datum_index);
		struct unit_datum *unit;
		short index;

		if (slot < 0 || slot == killer_slot || medals.team[slot] != medals.team[killer_slot] ||
			distributed_living_unit(player) == NONE)
		{
			continue;
		}
		unit = unit_try_and_get(player->unit_index);
		if (!unit)
			continue;
		for (index = 0; index < MAXIMUM_ATTACKERS_PER_UNIT; index++)
		{
			struct unit_attacker const *attacker = &unit->unit.attackers[index];

			if (attacker->player_index == dead_player_index &&
				(long)attacker->game_time_stamp > game_time_get() - AE_MEDAL_ASSIST_TICKS)
			{
				return TRUE;
			}
		}
	}
	return FALSE;
}


/* whether a name has a word in it, case aside */
static boolean name_has(char const *name, char const *word)
{
	size_t length = strlen(word), index;

	if (!name)
		return FALSE;
	for (; *name; name++)
	{
		for (index = 0; index < length; index++)
		{
			char c = name[index];

			if (!c)
				return FALSE;
			if (c >= 'A' && c <= 'Z')
				c = (char)(c - 'A' + 'a');
			if (c != word[index])
				break;
		}
		if (index == length)
			return TRUE;
	}
	return FALSE;
}

/* what a weapon definition is: an AE rifle by its profile, a pack weapon by its slot, any other by its tag's path */
static int weapon_role(long definition_index)
{
	struct ae_rifle_profile const *profile;
	int index, role;

	if (definition_index == NONE)
		return AE_MEDAL_ROLE_NONE;
	for (index = 0; index < medals.role_count; index++)
		if (medals.roles[index].definition == definition_index)
			return medals.roles[index].role;
	profile = ae_burst_rifle_profile_of(definition_index);
	if (profile)
	{
		role = profile == ae_rifle_profile(AE_RIFLE_BATTLE_RIFLE) ? AE_MEDAL_ROLE_BATTLE_RIFLE :
			profile == ae_rifle_profile(AE_RIFLE_EVO_RIFLE) ? AE_MEDAL_ROLE_EVO_RIFLE : AE_MEDAL_ROLE_DMR;
	}
	else
	{
		switch (ae_weapon_pack_definition_slot(definition_index))
		{
		case AE_WEAPON_SLOT_BATTLE_RIFLE: role = AE_MEDAL_ROLE_BATTLE_RIFLE; break;
		case AE_WEAPON_SLOT_SMG:
		case AE_WEAPON_SLOT_SMG_SILENCED: role = AE_MEDAL_ROLE_SMG; break;
		case AE_WEAPON_SLOT_CARBINE: role = AE_MEDAL_ROLE_CARBINE; break;
		case AE_WEAPON_SLOT_AR_SILENCED: role = AE_MEDAL_ROLE_ASSAULT_RIFLE; break;
		case AE_WEAPON_SLOT_DMR: role = AE_MEDAL_ROLE_DMR; break;
		case AE_WEAPON_SLOT_EVO_RIFLE: role = AE_MEDAL_ROLE_EVO_RIFLE; break;
		default: role = ae_medal_rules_role(tag_get_name(definition_index)); break;
		}
	}
	if (medals.role_count < ROLE_CACHE_SIZE)
	{
		medals.roles[medals.role_count].definition = definition_index;
		medals.roles[medals.role_count].role = role;
		medals.role_count++;
	}
	return role;
}

/* the victim's weapons: whether one is a power weapon, and whether they held as many as a player may */
static void victim_inventory(long unit_index, struct unit_datum const *unit, struct ae_medal_kill *kill)
{
	short index;

	for (index = 0; index < MAXIMUM_WEAPONS_PER_UNIT; index++)
	{
		long weapon_index = unit->unit.weapon_object_indices[index];
		struct weapon_datum const *weapon = weapon_index != NONE ? weapon_try_and_get(weapon_index) : NULL;

		if (weapon && ae_medal_rules_role_is_power(weapon_role(weapon->definition_index)))
			kill->victim_power_weapon = 1;
	}
	kill->victim_inventory_full = unit_get_weapon_count(unit_index) >= 2;
}

/* the vehicle a unit rides, NULL for none */
static struct unit_datum *vehicle_of(struct unit_datum const *unit)
{
	if (!unit || unit->object.parent_object_index == NONE)
		return NULL;
	return (struct unit_datum *)object_try_and_get_and_verify_type(unit->object.parent_object_index, _object_mask_vehicle);
}

/* the victim drove a vehicle: how fast it went, in world units a second */
static void victim_vehicle(long unit_index, struct unit_datum const *unit, struct ae_medal_kill *kill)
{
	struct unit_datum const *vehicle = vehicle_of(unit);

	if (vehicle && vehicle->unit.driver_object_index == unit_index)
	{
		kill->victim_driving = 1;
		kill->victim_vehicle_speed = (real)sqrt(vehicle->object.translational_velocity.i * vehicle->object.translational_velocity.i +
			vehicle->object.translational_velocity.j * vehicle->object.translational_velocity.j +
			vehicle->object.translational_velocity.k * vehicle->object.translational_velocity.k) * TICKS_PER_SECOND;
	}
}

/* the seat the killer sat in and what it rode (a flying vehicle by its tag's name) */
static void killer_seat(struct unit_datum const *unit, struct ae_medal_kill *kill)
{
	struct unit_datum const *vehicle = vehicle_of(unit);

	if (vehicle && unit->unit.parent_seat_index != NONE)
	{
		struct unit_definition const *definition = unit_definition_get(vehicle->definition_index);

		if (unit->unit.parent_seat_index >= 0 && unit->unit.parent_seat_index < definition->unit.seats.count)
		{
			struct unit_seat const *seat = TAG_BLOCK_GET_ELEMENT(&definition->unit.seats, unit->unit.parent_seat_index,
				struct unit_seat);

			kill->killer_seat = TEST_FLAG(seat->flags, _unit_seat_driver_bit) ? AE_MEDAL_SEAT_DRIVER :
				TEST_FLAG(seat->flags, _unit_seat_gunner_bit) ? AE_MEDAL_SEAT_GUNNER : AE_MEDAL_SEAT_PASSENGER;
			kill->killer_vehicle = name_has(tag_get_name(vehicle->definition_index), "banshee") ? AE_MEDAL_VEHICLE_AIR :
				AE_MEDAL_VEHICLE_GROUND;
		}
	}
}

/* the directory of a tag's path (up to its last slash), for telling a damage from the weapon it belongs to */
static boolean same_directory(char const *a, char const *b)
{
	char const *a_end = a ? strrchr(a, '\\') : NULL;
	char const *b_end = b ? strrchr(b, '\\') : NULL;
	size_t length;

	if (!a_end || !b_end || a_end - a != b_end - b)
		return FALSE;
	for (length = 0; length < (size_t)(a_end - a); length++)
	{
		char x = a[length], y = b[length];

		if (x >= 'A' && x <= 'Z')
			x = (char)(x - 'A' + 'a');
		if (y >= 'A' && y <= 'Z')
			y = (char)(y - 'A' + 'a');
		if (x != y)
			return FALSE;
	}
	return TRUE;
}

/* the weapon of the unit's whose damage this is: the one in hand first, then the others it carries. A weapon owns a damage when
its rounds' impact or attached damage, or its melee blow, is that damage, or the damage's tag lies in the weapon's folder (an
explosion). NONE for none: a grenade, a vehicle, a weapon since dropped */
static long killing_weapon(struct unit_datum const *unit, long damage_index)
{
	short pass, slot;

	if (!unit || damage_index == NONE)
		return NONE;
	for (pass = 0; pass < 2; pass++)
	{
		for (slot = 0; slot < MAXIMUM_WEAPONS_PER_UNIT; slot++)
		{
			long weapon_index = unit->unit.weapon_object_indices[slot];
			struct weapon_datum const *weapon = weapon_index != NONE ? weapon_try_and_get(weapon_index) : NULL;
			struct weapon_definition const *definition;
			short trigger_index;

			if (!weapon)
				continue;
			definition = weapon_definition_get(weapon->definition_index);
			if (definition->weapon.melee_attack_damage.index == damage_index)
				return weapon_index;
			for (trigger_index = 0; trigger_index < definition->weapon.triggers.count; trigger_index++)
			{
				struct weapon_trigger_definition const *trigger = TAG_BLOCK_GET_ELEMENT(&definition->weapon.triggers,
					trigger_index, struct weapon_trigger_definition);

				if (trigger->projectile.index != NONE)
				{
					struct projectile_definition const *projectile = projectile_definition_get(trigger->projectile.index);

					if (projectile->projectile.impact_damage.index == damage_index ||
						projectile->projectile.attached_detonation_damage.index == damage_index)
					{
						return weapon_index;
					}
				}
			}
			if (pass == 1 && same_directory(tag_get_name(weapon->definition_index), tag_get_name(damage_index)))
				return weapon_index;
		}
	}
	return NONE;
}

long ae_medals_killing_weapon_definition(long killing_player_index, long damage_index)
{
	struct player_datum const *killer = killing_player_index != NONE ? player_try_and_get(killing_player_index) : NULL;
	struct unit_datum const *unit = killer && killer->unit_index != NONE ? unit_try_and_get(killer->unit_index) : NULL;
	long weapon_index = killing_weapon(unit, damage_index);
	struct weapon_datum const *weapon = weapon_index != NONE ? weapon_try_and_get(weapon_index) : NULL;

	return weapon ? weapon->definition_index : NONE;
}

/* the rounds a head kill takes with a weapon that is not an AE rifle, from the damage tag and the head they hit, and the least
time they take; 0 where the rule cannot say (a damage that varies, a shield that fails by degrees) */
static int perfect_hits_by_tag(long damage_index, long victim_unit_index, struct weapon_datum const *weapon, long *fastest)
{
	struct damage_effect_definition const *effect = damage_effect_definition_get(damage_index);
	struct object_definition const *definition = object_definition_get(object_get(victim_unit_index)->definition_index);
	struct weapon_definition const *weapon_definition = weapon_definition_get(weapon->definition_index);
	struct collision_model const *model;
	struct damage_resistance_material const *material;
	struct ae_perfect_damage low, high;
	int needed, other;
	short head = ae_damage_head_material_index(victim_unit_index);

	if (definition->object.collision_model.index == NONE || head == NONE || weapon_definition->weapon.triggers.count < 1)
		return 0;
	model = collision_model_definition_get(definition->object.collision_model.index);
	material = TAG_BLOCK_GET_ELEMENT(&model->resistance.materials, head, struct damage_resistance_material);
	if ((model->resistance.shield_failure_threshold > 0.0f && model->resistance.maximum_shield_failure < 1.0f) ||
		model->resistance.shield_material_type < 0 || model->resistance.shield_material_type >= MAXIMUM_NUMBER_OF_MATERIAL_TYPES ||
		material->material_type < 0 || material->material_type >= MAXIMUM_NUMBER_OF_MATERIAL_TYPES)
	{
		return 0;
	}
	low.damage = effect->damage.damage_lower_bound;
	low.shield_max = object_get_maximum_shield_vitality(victim_unit_index, TRUE);
	low.shield_multiplier = material->shield_damage_multiplier;
	low.shield_modifier = effect->damage.material_modifiers[model->resistance.shield_material_type];
	low.leak = material->shield_leak_fraction;
	low.body_multiplier = material->body_damage_multiplier * effect->damage.material_modifiers[material->material_type];
	high = low;
	high.damage = effect->damage.damage_upper_bound;
	needed = ae_perfect_required_hits(&low);
	other = ae_perfect_required_hits(&high);
	if (needed <= 0 || needed != other)
		return 0;
	{
		struct weapon_trigger_definition const *trigger = TAG_BLOCK_GET_ELEMENT(&weapon_definition->weapon.triggers, 0,
			struct weapon_trigger_definition);
		real rate = trigger->initial_rate_of_fire > trigger->final_rate_of_fire ? trigger->initial_rate_of_fire :
			trigger->final_rate_of_fire;

		*fastest = rate > 0.0001f ? (long)ceil((real)(needed - 1) * TICKS_PER_SECOND / rate) : -1;
	}
	return needed;
}

/* whether the kill was perfect (ae_medal_history.h): a head kill with the rounds a head kill takes and none missed. A line
for the log says what was found, for each kill that could have been */
static boolean perfect_kill(struct blow const *blow, struct player_datum const *killer, struct weapon_datum const *weapon,
	long killing_player_index, long dead_player_index, long victim_unit_index, int killer_slot, int victim_slot)
{
	struct ae_rifle_profile const *profile;
	struct ae_perfect perfect;
	int hits = 0, shots = 0;
	boolean result;

	if (blow && weapon && blow->headshot && !blow->instant_head && blow->definition_index != NONE)
	{
		platform_log("medal: perfect check, player %d on %d with %s: the head shot's damage does not kill outright: none",
			killer_slot, victim_slot, tag_get_name(weapon->definition_index));
	}
	if (!blow || !weapon || !blow->instant_head || blow->category == AE_MEDAL_CATEGORY_SNIPER || blow->definition_index == NONE)
		return FALSE;
	memset(&perfect, 0, sizeof(perfect));
	perfect.killer = killer_slot;
	perfect.victim = victim_slot;
	perfect.tick = game_time_get();
	perfect.weapon = weapon->definition_index;
	perfect.fastest_ticks = -1;
	perfect.flight_ticks = AE_HISTORY_PERFECT_FLIGHT_TICKS +
		(killer->local_player_index == NONE ? AE_HISTORY_PERFECT_REMOTE_TICKS : 0);
	perfect.multiplier_ok = game_engine_get_damage_multiplier(killing_player_index, dead_player_index) == 1.0f;
	perfect.head_kill = 1;
	profile = ae_burst_rifle_damage_profile(blow->definition_index);
	if (profile)
	{
		int kind;

		for (kind = 0; kind < AE_RIFLE_KINDS && ae_rifle_profile((enum ae_rifle_kind)kind) != profile; kind++)
			;
		perfect.required = profile->rounds_a_pull > 1 ? PERFECT_BURST_ROUNDS : PERFECT_SINGLE_ROUNDS;
		perfect.fastest_ticks = ae_rifle_perfect_kill_ticks((enum ae_rifle_kind)kind);
	}
	else
		perfect.required = perfect_hits_by_tag(blow->definition_index, victim_unit_index, weapon, &perfect.fastest_ticks);
	result = (boolean)ae_history_perfect(&medals.history, &perfect, &hits, &shots);
	platform_log("medal: perfect check, player %d on %d with %s: %d hits needed, %d found, %d rounds fired: %s", killer_slot,
		victim_slot, tag_get_name(weapon->definition_index), perfect.required, hits, shots, result ? "perfect" : "no");
	return result;
}

/* ---------- public code */

char const *ae_medal_key(int medal)
{
	return medal >= 0 && medal < AE_MEDAL_COUNT ? medal_keys[medal] : NULL;
}

boolean ae_medals_kill_was_perfect(long dead_player_index)
{
	return (boolean)(medals.perfect_victim == dead_player_index && medals.perfect_tick == game_time_get() && dead_player_index != NONE);
}

void ae_medals_new_map(void)
{
	medals.started = FALSE;
	medals.role_count = 0;
	ae_medal_display_reset();
	/* (the pictures now, in the map's load: not a stall at the first kill) */
	ae_medal_pack_preload();
	ae_announcer_new_map();
}

/* the share of a player's whole vitality (shield and body weighed by their sizes) that went since the shield and body stood
at these */
static real vitality_taken(long object_index, real before_shield, real before_body, real *shield, real *body)
{
	struct object_datum const *object = object_get(object_index);
	real shield_max = object_get_maximum_shield_vitality(object_index, TRUE);
	real body_max = object_get_maximum_body_vitality(object_index, TRUE);
	/* (a lethal blow leaves the body below nothing: what was not there to take is not taken) */
	real shield_after = object->object.shield_vitality > 0.0f ? object->object.shield_vitality : 0.0f;
	real body_after = object->object.body_vitality > 0.0f ? object->object.body_vitality : 0.0f;
	real shield_gone = before_shield - shield_after;
	real body_gone = before_body - body_after;

	if (shield_gone < 0.0f)
		shield_gone = 0.0f;
	if (body_gone < 0.0f)
		body_gone = 0.0f;
	*shield = shield_gone;
	*body = body_gone;
	return shield_max + body_max > 0.0f ? (shield_gone * shield_max + body_gone * body_max) / (shield_max + body_max) : 0.0f;
}

void ae_medals_damage_before(
	long object_index,
	long owner_player_index)
{
	struct unit_datum *unit;

	medals.before.set = FALSE;
	if (!detecting() || !game_engine_can_score())
		return;
	unit = unit_try_and_get(object_index);
	if (!unit || slot_of_unit(unit) < 0)
		return;
	ensure_game();
	medals.before.set = TRUE;
	medals.before.tick = game_time_get();
	medals.before.object_index = object_index;
	medals.before.attacker = owner_player_index != NONE && player_try_and_get(owner_player_index) ?
		slot_of(owner_player_index) : -1;
	medals.before.shield = unit->object.shield_vitality;
	medals.before.body = unit->object.body_vitality;
}

void ae_medals_damage(
	long object_index,
	long definition_index,
	short category,
	short side_effect,
	boolean headshot,
	unsigned long being_damaged_flags)
{
	boolean known = FALSE;
	boolean overshield = FALSE;

	if (!detecting())
		return;
	if (medals.before.set && medals.before.object_index == object_index && medals.before.tick == game_time_get())
	{
		struct unit_datum *unit = unit_try_and_get(object_index);
		int victim_slot = slot_of_unit(unit);

		known = TRUE;
		overshield = medals.before.shield > 1.0f;
		if (victim_slot >= 0)
		{
			struct ae_history_hit hit;
			real shield, body;

			memset(&hit, 0, sizeof(hit));
			hit.taken = vitality_taken(object_index, medals.before.shield, medals.before.body, &shield, &body);
			hit.shield = shield;
			hit.body = body;
			hit.tick = game_time_get();
			hit.attacker = medals.before.attacker;
			hit.tag = definition_index;
			hit.category = category;
			hit.kind = (short)(definition_index != NONE ? ae_medal_rules_weapon(tag_get_name(definition_index)) :
				AE_MEDAL_WEAPON_NONE);
			hit.head = headshot ? 1 : 0;
			/* (a blow that took nothing is no hit) */
			if (shield > 0.0f || body > 0.0f)
				ae_history_hit(&medals.history, victim_slot, &hit, medals.before.shield, medals.before.body);
		}
	}
	medals.before.set = FALSE;
	if (!TEST_FLAG(being_damaged_flags, _object_being_damaged_body_depleted_bit))
		return;
	medals.blow.set = TRUE;
	medals.blow.tick = game_time_get();
	medals.blow.object_index = object_index;
	medals.blow.category = category;
	medals.blow.side_effect = side_effect;
	medals.blow.headshot = headshot;
	medals.blow.killed_instantly = TEST_FLAG(being_damaged_flags, _object_being_damaged_killed_instantly_bit) != 0;
	medals.blow.weapon = definition_index != NONE ? ae_medal_rules_weapon(tag_get_name(definition_index)) :
		AE_MEDAL_WEAPON_NONE;
	medals.blow.definition_index = definition_index;
	medals.blow.instant_head = (boolean)(headshot && definition_index != NONE &&
		TEST_FLAG(damage_effect_definition_get(definition_index)->damage.flags, DAMAGE_CAN_CAUSE_HEADSHOTS_BIT));
	medals.blow.overshield = known && overshield;
}

void ae_medals_shot(
	long weapon_index,
	short trigger_index,
	long owner_object_index)
{
	struct unit_datum *owner;
	struct weapon_datum *weapon;
	struct weapon_definition *definition;
	struct weapon_trigger_definition const *trigger;
	int slot, pellets;

	if (!detecting() || !game_engine_can_score())
		return;
	owner = unit_try_and_get(owner_object_index);
	slot = slot_of_unit(owner);
	weapon = slot >= 0 ? weapon_try_and_get(weapon_index) : NULL;
	if (!weapon)
		return;
	definition = weapon_definition_get(weapon->definition_index);
	if (trigger_index < 0 || trigger_index >= definition->weapon.triggers.count)
		return;
	trigger = TAG_BLOCK_GET_ELEMENT(&definition->weapon.triggers, trigger_index, struct weapon_trigger_definition);
	pellets = trigger->projectiles_per_shot > 0 ? trigger->projectiles_per_shot : 1;
	ensure_game();
	ae_history_shot(&medals.history, slot, game_time_get(), weapon->definition_index, weapon_role(weapon->definition_index),
		owner->unit.current_zoom_level != NONE, pellets);
}

void ae_medals_flag_reset(
	long weapon_index)
{
	struct weapon_datum *flag;

	if (!detecting())
		return;
	flag = weapon_try_and_get(weapon_index);
	if (flag && flag->object.owner_team_index >= 0 && flag->object.owner_team_index < 2)
	{
		/* (where it lay as it went home: the update reads a return after it, in the same tick) */
		medals.flag_at[flag->object.owner_team_index] = flag->object.position;
		medals.flag_at_known[flag->object.owner_team_index] = TRUE;
	}
}

void ae_medals_weapon_taken(
	long unit_index,
	long weapon_index)
{
	int slot;

	if (!detecting())
		return;
	slot = slot_of_unit(unit_try_and_get(unit_index));
	if (slot < 0)
		return;
	ensure_game();
	ae_history_picked(&medals.history, slot, weapon_index, game_time_get());
}

void ae_medals_item_spawned(
	long object_index)
{
	if (!detecting() || !weapon_try_and_get(object_index))
		return;
	ensure_game();
	ae_history_spawned(&medals.history, object_index);
}

void ae_medals_player_killed(
	long killing_player_index,
	long dead_player_index,
	boolean friendly_fire)
{
	struct ae_medal_kill kill;
	struct blow const *blow;
	struct player_datum *dead, *killer;
	struct unit_datum *victim_unit, *killer_unit = NULL;
	long victim_unit_index, killing_index = NONE;
	struct weapon_datum *killing = NULL;
	int killer_slot, victim_slot, index;

	if (!detecting() || dead_player_index == NONE || !game_engine_can_score())
		return;
	ensure_game();
	refresh_players();
	dead = player_try_and_get(dead_player_index);
	killer = killing_player_index != NONE ? player_try_and_get(killing_player_index) : NULL;
	victim_slot = slot_of(dead_player_index);
	killer_slot = killer ? slot_of(killing_player_index) : -1;
	if (!dead || victim_slot < 0)
		return;

	memset(&kill, 0, sizeof(kill));
	kill.tick = game_time_get();
	kill.killer = killer_slot;
	kill.victim = victim_slot;
	kill.team_of = medals.team;
	kill.driver = -1;
	kill.victim_stand_distance = -1.0f;
	kill.betrayal = killer_slot >= 0 && killer_slot != victim_slot && friendly_fire;

	blow = blow_of(dead_player_index);
	if (blow)
	{
		kill.category = blow->category;
		kill.weapon = blow->weapon;
		kill.headshot = blow->headshot;
		kill.killed_instantly = blow->killed_instantly;
		kill.side_effect = blow->side_effect;
	}
	victim_unit_index = blow ? blow->object_index : dead->unit_index != NONE ? dead->unit_index : dead->dead_unit_index;
	victim_unit = unit_try_and_get(victim_unit_index);
	/* (behind: the victim faced away from the killer) */
	if (victim_unit && killer && killer->unit_index != NONE && unit_try_and_get(killer->unit_index))
	{
		real_point3d const *at = &victim_unit->object.bounding_sphere_center;
		real_point3d const *from = &object_get(killer->unit_index)->object.bounding_sphere_center;

		kill.from_behind = victim_unit->object.forward.i * (from->x - at->x) + victim_unit->object.forward.j * (from->y - at->y) < 0.0f;
	}
	if (victim_unit)
	{
		for (index = 0; index < MAXIMUM_ATTACKERS_PER_UNIT; index++)
		{
			struct unit_attacker const *attacker = &victim_unit->unit.attackers[index];
			int slot = attacker->player_index != NONE && player_try_and_get(attacker->player_index) ?
				slot_of(attacker->player_index) : -1;

			if (slot < 0)
				continue;
			kill.attackers[kill.attacker_count].player = slot;
			kill.attackers[kill.attacker_count].tick = (long)attacker->game_time_stamp;
			kill.attackers[kill.attacker_count].damage = attacker->damage_inflicted;
			kill.attacker_count++;
		}
		kill.victim_invisible = TEST_FLAG(victim_unit->unit.flags, _unit_active_camouflaged_bit) ||
			TEST_FLAG(victim_unit->unit.flags, _unit_super_camouflaged_bit);
		{
			struct weapon_datum const *weapon = unit_weapon_in_hand(victim_unit);

			kill.victim_reloading = weapon && (weapon->weapon.state == _weapon_state_primary_reload ||
				weapon->weapon.state == _weapon_state_secondary_reload ||
				weapon->weapon.magazines[0].state == _magazine_reloading ||
				weapon->weapon.magazines[1].state == _magazine_reloading);
		}
	}
	if (victim_unit)
	{
		victim_inventory(victim_unit_index, victim_unit, &kill);
		victim_vehicle(victim_unit_index, victim_unit, &kill);
	}
	kill.victim_overshield = blow && blow->overshield;
	/* what the victim carried: the flag or the ball, in the hands now or last tick */
	if (medals.game.mode == AE_MEDAL_MODE_CTF)
		kill.victim_carried_flag = medals.carrying[victim_slot] || unit_has_flag(victim_unit);
	if (medals.game.mode == AE_MEDAL_MODE_CTF && kill.victim_carried_flag && victim_unit && dead->team_index >= 0 &&
		dead->team_index < 2 && medals.stand_known[dead->team_index])
	{
		/* (the stand the carrier scores at: their own team's) */
		kill.victim_stand_distance = distance3d(&victim_unit->object.position, &medals.stand[dead->team_index]);
	}
	if (medals.game.mode == AE_MEDAL_MODE_ODDBALL)
		kill.victim_carried_ball = medals.carrying[victim_slot] || ae_strip_oddball_ball_count(dead_player_index) > 0;
	if (medals.game.mode == AE_MEDAL_MODE_ODDBALL && kill.victim_carried_ball && game_engine && game_engine->get_player_score)
	{
		struct game_variant const *variant = game_engine_get_variant();

		/* (the ball's time, in ticks: the score to win is minutes) */
		kill.victim_score = game_engine->get_player_score(dead_player_index, medals.game.teams ? _get_score_team : _get_score_individual);
		kill.victim_score_to_win = variant->universal_variant.score_to_win * 60L * TICKS_PER_SECOND;
	}
	if (medals.game.mode == AE_MEDAL_MODE_KING)
		kill.victim_on_hill = medals.on_hill[victim_slot] || ae_strip_king_player_on_hill(dead_player_index);

	if (killer_slot >= 0 && killer_slot != victim_slot)
	{
		boolean alive = distributed_living_unit(killer) != NONE;

		kill.killer_dead = !alive;
		if (alive)
		{
			killer_unit = unit_try_and_get(killer->unit_index);
			if (killer_unit)
			{
				struct weapon_datum const *weapon = unit_weapon_in_hand(killer_unit);

				kill.killer_snapshot = killer_unit->object.type == _object_type_biped &&
					killer_unit->object.parent_object_index == NONE && weapon &&
					killer_unit->unit.current_zoom_level == NONE;
				/* a health under 60 percent with an empty shield, and still alive */
				{
					real body = unit_body_fraction(killer->unit_index);

					kill.killer_close_call = body > 0.0001f && body < 0.6f &&
						unit_shield_fraction(killer->unit_index) < 0.0001f;
				}
				kill.driver = vehicle_driver_slot(killer_unit, killer_slot);
				killer_seat(killer_unit, &kill);
				kill.killer_carried_flag = medals.game.mode == AE_MEDAL_MODE_CTF &&
					(medals.carrying[killer_slot] || unit_has_flag(killer_unit));
				if (victim_unit)
				{
					kill.distance = distance3d(&victim_unit->object.bounding_sphere_center,
						&killer_unit->object.bounding_sphere_center);
				}
				/* (the weapon that fired the killing damage, not the one in hand now: a plasma bolt or a rocket lands after
				a switch, and a grenade is no weapon's) */
				killing_index = killing_weapon(killer_unit, blow ? blow->definition_index : NONE);
				killing = killing_index != NONE ? weapon_try_and_get(killing_index) : NULL;
				if (killing)
				{
					kill.killer_role = weapon_role(killing->definition_index);
					kill.killer_magazine_empty = killing->weapon.magazines[0].rounds_loaded == 0;
				}
				kill.killer_carried_ball = medals.game.mode == AE_MEDAL_MODE_ODDBALL &&
					ae_strip_oddball_ball_count(killing_player_index) > 0;
				kill.killer_on_hill = medals.game.mode == AE_MEDAL_MODE_KING &&
					ae_strip_king_player_on_hill(killing_player_index);
				if (medals.game.teams)
					kill.protected_teammate = teammate_was_hurt_by(dead_player_index, killer_slot);
			}
		}
	}
	/* the victim's team: the others of it still alive */
	if (medals.game.teams)
	{
		struct data_iterator iterator;
		struct player_datum *player;

		data_iterator_new(&iterator, player_data);
		while ((player = (struct player_datum *)data_iterator_next(&iterator)) != NULL)
		{
			if (iterator.datum_index != dead_player_index && player->team_index == dead->team_index &&
				!player->quit_out_of_game && distributed_living_unit(player) != NONE)
			{
				kill.victim_team_survivors++;
			}
		}
	}

	/* what the records find, for a kill by an enemy of a killer who is there */
	if (killer_unit && killer_slot >= 0 && killer_slot != victim_slot && !kill.betrayal)
	{
		boolean grenade = kill.weapon == AE_MEDAL_WEAPON_FRAG_GRENADE || kill.weapon == AE_MEDAL_WEAPON_PLASMA_GRENADE ||
			kill.weapon == AE_MEDAL_WEAPON_PLASMA_GRENADE_STUCK;
		long killing_tag = blow ? blow->definition_index : NONE;
		long weapon_index = killing_index;

		if (kill.category == AE_MEDAL_CATEGORY_SNIPER)
			kill.no_scope = ae_history_no_scope(&medals.history, killer_slot, kill.tick);
		kill.quick_draw = ae_history_quick_draw(&medals.history, killer_slot, victim_slot, kill.tick, killing_tag,
			kill.killer_role, kill.category);
		/* (a comeback against a rival: not with a power weapon or a vehicle, and alive) */
		if (!kill.killer_dead && !ae_medal_rules_role_is_power(kill.killer_role) && kill.category != AE_MEDAL_CATEGORY_VEHICLE &&
			kill.category != AE_MEDAL_CATEGORY_MOUNTED)
		{
			kill.reversal = ae_history_reversal(&medals.history, killer_slot, victim_slot);
		}
		kill.nade_shot = ae_history_nade_shot(&medals.history, killer_slot, victim_slot, kill.headshot);
		if (kill.category != AE_MEDAL_CATEGORY_MELEE && !grenade)
		{
			kill.turned_360 = ae_history_turned(&medals.history, killer_slot, kill.tick);
			if (weapon_index != NONE)
				kill.off_the_rack = ae_history_off_the_rack(&medals.history, killer_slot, weapon_index, kill.tick);
		}
		if (kill.category == AE_MEDAL_CATEGORY_MELEE && kill.from_behind && victim_unit)
		{
			kill.ninja = ae_history_ninja(&medals.history, killer_slot, kill.tick, victim_unit->object.position.x,
				victim_unit->object.position.y, victim_unit->object.position.z, victim_unit->object.forward.i,
				victim_unit->object.forward.j);
		}
		if (kill.headshot && killer)
		{
			kill.perfect = perfect_kill(blow, killer, killing, killing_player_index, dead_player_index, victim_unit_index,
				killer_slot, victim_slot);
			if (kill.perfect)
			{
				medals.perfect_victim = dead_player_index;
				medals.perfect_tick = game_time_get();
			}
		}
	}

	ae_medal_rules_kill(&medals.game, &kill, &medals.out);
	ae_history_life_end(&medals.history, victim_slot);
	medals.blow.set = FALSE;
	grant_all();
}

void ae_medals_update(void)
{
	struct data_iterator iterator;
	struct player_datum *player;
	boolean hill_moved = FALSE;
	int mode;

	if (!detecting() || !game_engine_can_score())
		return;
	ensure_game();
	refresh_players();
	mode = medals.game.mode;
	if (mode == AE_MEDAL_MODE_KING)
	{
		long timer = ae_strip_king_hill_timer();

		/* (the hill's move sets its timer back up) */
		hill_moved = timer > medals.hill_timer;
		medals.hill_timer = timer;
		if (hill_moved)
		{
			ae_medal_rules_hill_moved(&medals.game, &medals.out);
			grant_all();
		}
	}
	data_iterator_new(&iterator, player_data);
	while ((player = (struct player_datum *)data_iterator_next(&iterator)) != NULL)
	{
		int slot = slot_of(iterator.datum_index);
		boolean alive = distributed_living_unit(player) != NONE;
		struct unit_datum *unit = alive ? unit_try_and_get(player->unit_index) : NULL;

		if (slot < 0)
			continue;
		medals.alive[slot] = (boolean)(unit != NULL);
		/* (a new unit is a new life: what hurt the last is not counted against this one) */
		if ((alive ? player->unit_index : NONE) != medals.unit_of[slot])
		{
			medals.unit_of[slot] = alive ? player->unit_index : NONE;
			ae_history_life_end(&medals.history, slot);
			ae_history_steps_clear(&medals.history, slot);
		}
		if (unit)
		{
			ae_history_step(&medals.history, slot, game_time_get(), atan2f(unit->unit.aiming_vector.j, unit->unit.aiming_vector.i),
				unit->object.position.x, unit->object.position.y, unit->object.position.z,
				unit->object.type == _object_type_biped && TEST_FLAG(((struct biped_datum *)unit)->biped.flags, _biped_airborne_bit));
		}
		switch (mode)
		{
		case AE_MEDAL_MODE_CTF:
		{
			union multiplayer_statistics const *statistics = &player->statistics.multiplayer_statistics;
			short value;

			medals.carrying[slot] = unit_has_flag(unit);
			if (medals.known[slot] && !medals.game.assault)
			{
				for (value = medals.flag_grabs[slot]; value < statistics->ctf_statistics.flag_grabs; value++)
				{
					ae_medal_rules_flag(&medals.game, slot, AE_MEDAL_FLAG_GRAB, &medals.out);
					grant_all();
				}
				for (value = medals.flag_returns[slot]; value < statistics->ctf_statistics.flag_returns; value++)
				{
					float distance = -1.0f;
					short team = player->team_index;

					/* (where the flag lay when it was taken back, against the stand the other team scores at: where
					it lay as the return put it home, noted by ae_medals_flag_reset) */
					if (team >= 0 && team < 2 && medals.flag_at_known[team] && medals.stand_known[1 - team])
						distance = distance3d(&medals.flag_at[team], &medals.stand[1 - team]);
					ae_medal_rules_flag_at(&medals.game, slot, AE_MEDAL_FLAG_RETURN, distance, &medals.out);
					grant_all();
				}
				for (value = medals.flag_scores[slot]; value < statistics->ctf_statistics.flag_scores; value++)
				{
					ae_medal_rules_flag(&medals.game, slot, AE_MEDAL_FLAG_CAPTURE, &medals.out);
					grant_all();
				}
			}
			medals.flag_grabs[slot] = statistics->ctf_statistics.flag_grabs;
			medals.flag_returns[slot] = statistics->ctf_statistics.flag_returns;
			medals.flag_scores[slot] = statistics->ctf_statistics.flag_scores;
			medals.known[slot] = TRUE;
			break;
		}
		case AE_MEDAL_MODE_ODDBALL:
			medals.carrying[slot] = alive && ae_strip_oddball_ball_count(iterator.datum_index) > 0;
			ae_medal_rules_ball_tick(&medals.game, slot, medals.carrying[slot], &medals.out);
			grant_all();
			break;
		case AE_MEDAL_MODE_KING:
			medals.on_hill[slot] = alive && ae_strip_king_player_on_hill(iterator.datum_index);
			ae_medal_rules_hill_tick(&medals.game, slot, alive, medals.on_hill[slot],
				unit && unit->object.parent_object_index != NONE, &medals.out);
			grant_all();
			break;
		default:
			break;
		}
	}
	if (mode == AE_MEDAL_MODE_KING)
	{
		int slot, other;

		for (slot = 0; slot < AE_MEDAL_RULES_PLAYERS; slot++)
		{
			boolean contested = FALSE;

			if (!medals.used[slot])
				continue;
			for (other = 0; other < AE_MEDAL_RULES_PLAYERS; other++)
			{
				contested |= other != slot && medals.used[other] && medals.alive[other] && medals.on_hill[other] &&
					(!medals.game.teams || medals.team[other] != medals.team[slot]);
			}
			ae_medal_rules_hill_guardian_tick(&medals.game, slot, medals.alive[slot], medals.on_hill[slot], contested,
				&medals.out);
			grant_all();
		}
	}
}

void ae_medals_race_checkpoint(long player_index)
{
	int slot = slot_of(player_index);

	if (!detecting() || slot < 0 || !game_engine_can_score())
		return;
	ensure_game();
	ae_medal_rules_checkpoint(&medals.game, slot, game_time_get(), &medals.out);
	grant_all();
}

void ae_medals_game_end(void)
{
	struct ae_medal_end end;
	struct data_iterator iterator;
	struct player_datum *player;
	boolean teams;

	if (!detecting() || medals.ended)
		return;
	ensure_game();
	medals.ended = TRUE;
	refresh_players();
	teams = medals.game.teams != 0;
	memset(&end, 0, sizeof(end));
	data_iterator_new(&iterator, player_data);
	while ((player = (struct player_datum *)data_iterator_next(&iterator)) != NULL)
	{
		int slot = slot_of(iterator.datum_index);
		struct game_engine_place place;

		if (slot < 0 || player->quit_out_of_game)
			continue;
		end.used[slot] = TRUE;
		end.team[slot] = (short)player->team_index;
		place = game_engine_get_place(iterator.datum_index, teams ? _get_score_team : _get_score_individual);
		end.winner[slot] = place.place == 0 && place.flags != _place_all_tied;
	}
	if (teams)
	{
		long red = game_engine_get_team_score(0);
		long blue = game_engine_get_team_score(1);

		end.leading_team = red >= blue ? 0 : 1;
		end.team_lead = (int)(red >= blue ? red - blue : blue - red);
	}
	ae_medal_rules_game_end(&medals.game, &end, &medals.out);
	grant_all();
}

unsigned short ae_medals_entry_size(void)
{
	return (unsigned short)sizeof(struct distributed_medal);
}

/* message 102, from the host over its connection's stream: a medal for a player of this machine to show */
void ae_medals_handle_down(void const *entries, int from_stream)
{
	struct distributed_medal medal;
	struct player_datum *player;

	if (!from_stream || !game_engine_running())
		return;
	csmemcpy(&medal, entries, sizeof(medal));
	if (medal.medal >= AE_MEDAL_COUNT)
		return;
	player = distributed_player(medal.player_index);
	if (!player || player->local_player_index < 0 || player->local_player_index >= MAXIMUM_LOCAL_PLAYERS)
		return;
	ae_medal_display_add(player->local_player_index, medal.medal);
}
