/*
AE_OBJECTIVE_LOG.C

The objective log (ae_objective_log.h). The host only. Most events are found by watching the snapshot the bots read
(ae_bots_objective.c: the flags and their carriers, the balls, the hill, the bomb), once a tick, so a mode's file needs no
hook for them; the few that carry a cause the snapshot lacks (a capture, how a flag came back, the bomb's grabs) come in
one-line hooks.
*/

#include "cseries.h"
#include "game/game.h"
#include "game/game_engine.h"
#include "game/players.h"
#include "memory/data.h"
#include "objects/objects.h"
#include "items/items.h"
#include "items/weapons.h"
#include "units/units.h"
#include "main/main.h"
#include "scenario/scenario.h"
#include "tag_files/tag_files.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "bots.h"
#include "ae_bomb_assault.h"
#include "ae_bots_objective.h"
#include "ae_fireteam.h"
#include "ae_h2_king.h"
#include "ae_juggernaut.h"
#include "ae_round.h"
#include "ae_strip_engine.h"
#include "ae_strip_model.h"
#include "ae_objective_log.h"

/* port/linux/src/port_config.c, platform */
char const *config_string(char const *name);
boolean config_boolean(char const *name);
void platform_log(char const *format, ...);
boolean network_game_distributed_client(void);

#define AE_OBJ_PLAYERS 64
/* a person's, and the snapshot's, line each second */
#define AE_OBJ_SECOND TICKS_PER_SECOND
/* a hill entry or exit this soon after the last one of that player is not logged */
#define AE_OBJ_HILL_QUIET (2 * TICKS_PER_SECOND)
/* a ball taken this soon after its last holder let go came from that player's hands (a kill or a drop) */
/* a flag last seen at its stand this soon before it was taken was taken from the stand */
#define AE_OBJ_HOME_TICKS 5
#define AE_OBJ_KIT_TICKS (2 * TICKS_PER_SECOND)
#define AE_OBJ_PASS_TICKS (3 * TICKS_PER_SECOND)

static int ae_obj_level_value = -1;

int ae_obj_level(void)
{
	if (ae_obj_level_value < 0)
		ae_obj_level_value = ae_obj_rules_level(config_string("debug.objective_log"), config_boolean("debug.bot_events") ? 1 : 0);
	return ae_obj_level_value;
}

/* ---------- names and numbers */

static long ae_obj_absolute(long player_index)
{
	return player_index == NONE ? -1L : (long)DATUM_INDEX_TO_ABSOLUTE_INDEX(player_index);
}

boolean ae_obj_player_is_bot(long player_index)
{
	struct player_datum *player = player_index != NONE ? player_try_and_get(player_index) : NULL;

	return player && player->local_player_index == NONE && bots_machine_is_bot(player->network_player_data.machine_index);
}

static int ae_obj_mode(void)
{
	struct game_variant *variant;

	if (!game_engine)
		return AE_OBJ_MODE_SLAYER;
	variant = game_engine_get_variant();
	switch (variant->game_engine_index)
	{
	case game_engine_ctf:
		return variant->game_engine_variant.ctf.assault ? AE_OBJ_MODE_ASSAULT :
			variant->game_engine_variant.ctf.single_flag_time > 0 ? AE_OBJ_MODE_CTF1 : AE_OBJ_MODE_CTF;
	case game_engine_oddball:
	{
		struct ae_bots_objective_snapshot const *snapshot = ae_bots_objective_snapshot();

		return !snapshot ? AE_OBJ_MODE_ODDBALL : snapshot->mode == AE_BOTS_MODE_TAG ? AE_OBJ_MODE_TAG :
			snapshot->mode == AE_BOTS_MODE_JUGGERNAUT ? AE_OBJ_MODE_JUGG : AE_OBJ_MODE_ODDBALL;
	}
	case game_engine_king:
		return ae_h2_territories_on() ? AE_OBJ_MODE_TERR : AE_OBJ_MODE_KOTH;
	case game_engine_race:
		return AE_OBJ_MODE_RACE;
	case game_engine_bomb:
		return ae_bomb_snd_on() ? AE_OBJ_MODE_SND : AE_OBJ_MODE_BOMB;
	default:
		return ae_round_on() ? AE_OBJ_MODE_ELIM : AE_OBJ_MODE_SLAYER;
	}
}

char const *ae_obj_mode_word(void)
{
	return ae_obj_rules_mode_name(ae_obj_mode());
}

void ae_obj_emit(char const *ev, struct ae_obj_fields const *fields)
{
	char line[ sizeof(fields->text) + 96 ];

	ae_obj_rules_line(line, sizeof(line), (long)game_time_get(), !network_game_distributed_client(), ae_obj_mode(), ev, fields);
	platform_log("%s", line);
}

static boolean ae_obj_position(long player_index, real_point3d *position)
{
	struct player_datum *player = player_index != NONE ? player_try_and_get(player_index) : NULL;

	if (!player || player->unit_index == NONE)
		return FALSE;
	/* (a rider's position is relative to its seat: where the vehicle is) */
	{
		struct object_datum *unit = object_get(player->unit_index);
		struct object_datum *parent = unit->object.parent_object_index != NONE ?
			object_try_and_get(unit->object.parent_object_index) : NULL;

		*position = parent ? parent->object.position : unit->object.position;
	}
	return TRUE;
}

static void ae_obj_who_at(struct ae_obj_fields *fields, long player_index, real_point3d const *fallback, boolean force)
{
	struct player_datum *player = player_index != NONE ? player_try_and_get(player_index) : NULL;
	real_point3d position = fallback ? *fallback : *global_origin3d;

	if (!force)
		ae_obj_position(player_index, &position);
	ae_obj_fields_long(fields, "p", ae_obj_absolute(player_index));
	ae_obj_fields_long(fields, "b", ae_obj_player_is_bot(player_index) ? 1 : 0);
	ae_obj_fields_long(fields, "team", player ? (long)player->team_index : -1L);
	ae_obj_fields_point(fields, position.x, position.y, position.z);
}

void ae_obj_who(struct ae_obj_fields *fields, long player_index)
{
	ae_obj_who_at(fields, player_index, NULL, FALSE);
}

static boolean ae_obj_dead(long player_index)
{
	struct player_datum *player = player_index != NONE ? player_try_and_get(player_index) : NULL;

	if (!player || player->unit_index == NONE)
		return TRUE;
	return TEST_FLAG(object_get(player->unit_index)->object.damage_flags, _object_dead_bit) ? TRUE : FALSE;
}

static double ae_obj_distance(real_point3d const *a, real_point3d const *b)
{
	double dx = a->x - b->x, dy = a->y - b->y, dz = a->z - b->z;

	return sqrt(dx * dx + dy * dy + dz * dz);
}

/* ---------- the watchers' memory */

static struct
{
	boolean running;
	long last_tick;
	long next_second;
	/* ctf, by the flag's owner team */
	long carrier[2];
	long take_tick[2];
	real_point3d take_position[2];
	long home_tick[2];
	long capture_tick[2];
	/* balls, by player slot */
	long ball_since[AE_OBJ_PLAYERS];
	long ball_dropper;
	long ball_drop_tick;
	long jugg;
	/* the hill */
	boolean hill;
	real_point3d hill_centre;
	int hill_state;
	int hill_team;
	boolean inside[AE_OBJ_PLAYERS];
	long inside_tick[AE_OBJ_PLAYERS];
	/* the bomb's rounds: the scores at the last round end, and the reason the code that ended it told */
	long scores_logged[2];
	char pending_why[16];
	/* the bomb's arming (the carrier's ticks in the goal) */
	long arm_since;
	long arm_last;
	long arm_player;
	long planted_tick;
	long defuse_done_tick;
	/* a player's vehicle and weapons, by slot */
	long unit_of[AE_OBJ_PLAYERS];
	long unit_born[AE_OBJ_PLAYERS];
	long vehicle_of[AE_OBJ_PLAYERS];
	long vehicle_since[AE_OBJ_PLAYERS];
	long weapons_of[AE_OBJ_PLAYERS][4];
} ae_obj_state;

static void ae_obj_forget(void)
{
	long index;

	memset(&ae_obj_state, 0, sizeof(ae_obj_state));
	ae_obj_state.carrier[0] = ae_obj_state.carrier[1] = NONE;
	ae_obj_state.home_tick[0] = ae_obj_state.home_tick[1] = 0;
	ae_obj_state.capture_tick[0] = ae_obj_state.capture_tick[1] = -1000;
	ae_obj_state.ball_dropper = NONE;
	ae_obj_state.jugg = NONE;
	ae_obj_state.hill_state = -1;
	ae_obj_state.hill_team = -1;
	for (index = 0; index < AE_OBJ_PLAYERS; index++)
	{
		long weapon;

		ae_obj_state.ball_since[index] = -1;
		ae_obj_state.unit_of[index] = NONE;
		ae_obj_state.vehicle_of[index] = NONE;
		for (weapon = 0; weapon < 4; weapon++)
			ae_obj_state.weapons_of[index][weapon] = NONE;
	}
	ae_obj_state.arm_since = -1;
	ae_obj_state.arm_player = NONE;
	ae_obj_state.planted_tick = -1000;
	ae_obj_state.defuse_done_tick = -1000;
	ae_obj_state.last_tick = -1;
}

/* ---------- the engine's hooks */

void ae_obj_ctf_capture(long player_index, long team_index)
{
	struct ae_obj_fields fields;
	real_point3d position = *global_origin3d;
	long flag, now = (long)game_time_get();

	if (ae_obj_level() < AE_OBJ_EVENTS || network_game_distributed_client() || !VALID_INDEX(team_index, 2))
		return;
	flag = 1 - team_index;
	ae_obj_state.capture_tick[flag] = now;
	ae_obj_position(player_index, &position);
	ae_obj_fields_begin(&fields);
	ae_obj_fields_long(&fields, "flag", flag);
	ae_obj_who(&fields, player_index);
	ae_obj_fields_real(&fields, "carry_s", ae_obj_state.take_tick[flag] > 0 ? (now - ae_obj_state.take_tick[flag]) / (double)TICKS_PER_SECOND : -1.0);
	ae_obj_fields_real(&fields, "dist", ae_obj_state.take_tick[flag] > 0 ? ae_obj_distance(&ae_obj_state.take_position[flag], &position) : -1.0);
	ae_obj_emit("flag_capture", &fields);
}

void ae_obj_ctf_return(long weapon_index, long player_index)
{
	struct ae_obj_fields fields;
	struct weapon_datum *weapon;
	real_point3d position = *global_origin3d;

	if (ae_obj_level() < AE_OBJ_EVENTS || network_game_distributed_client() || weapon_index == NONE)
		return;
	weapon = weapon_try_and_get(weapon_index);
	if (!weapon)
		return;
	item_get_position_even_if_in_inventory(weapon_index, &position);
	ae_obj_fields_begin(&fields);
	ae_obj_fields_long(&fields, "flag", (long)weapon->object.owner_team_index);
	ae_obj_fields_text(&fields, "how", player_index != NONE ? "touch" : "timeout");
	ae_obj_fields_long(&fields, "by", ae_obj_absolute(player_index));
	ae_obj_fields_long(&fields, "b", ae_obj_player_is_bot(player_index) ? 1 : 0);
	ae_obj_fields_point(&fields, position.x, position.y, position.z);
	ae_obj_emit("flag_return", &fields);
}

static long ae_obj_team_score(long team)
{
	if (!game_engine || !game_engine_has_teams())
		return 0;
	if (game_engine_get_variant()->game_engine_index == game_engine_bomb)
		return game_engine_bomb_team_score(team);
	return game_engine_get_team_score(team);
}

void ae_obj_bomb_player(char const *what, long player_index)
{
	struct ae_obj_fields fields;
	char const *ev = NULL, *why = NULL;

	if (ae_obj_level() < AE_OBJ_EVENTS || network_game_distributed_client())
		return;
	if (!strcmp(what, "grab"))
		ev = "bomb_take";
	else if (!strcmp(what, "drop"))
		ev = "bomb_drop", why = "dropped";
	else if (!strncmp(what, "drop (the carrier killed", 24))
		ev = "bomb_drop", why = "killed";
	else if (!strcmp(what, "fumble"))
		ev = "bomb_drop", why = "fumble";
	else if (!strcmp(what, "plant"))
		ev = "plant_done", ae_obj_state.planted_tick = (long)game_time_get();
	else if (!strcmp(what, "save"))
	{
		ae_obj_snd_pending("defuse");
		/* (ae_bomb_assault.c logs the same defuse as its own defuse_done: one line a defuse) */
		if ((long)game_time_get() - ae_obj_state.defuse_done_tick <= 1)
			return;
		ev = "defuse_done";
		ae_obj_state.defuse_done_tick = (long)game_time_get();
	}
	else if (!strcmp(what, "score"))
		ev = "bomb_score", ae_obj_state.planted_tick = (long)game_time_get();
	else if (!strcmp(what, "blast kills"))
		ev = "blast_kill";
	else if (!strncmp(what, "out of bounds", 13))
		ev = "oob_kill";
	if (!ev)
		return;
	ae_obj_fields_begin(&fields);
	ae_obj_who(&fields, player_index);
	if (why)
		ae_obj_fields_text(&fields, "why", why);
	if (!strcmp(ev, "bomb_score"))
	{
		ae_obj_fields_long(&fields, "score_r", ae_obj_team_score(0));
		ae_obj_fields_long(&fields, "score_b", ae_obj_team_score(1));
	}
	ae_obj_emit(ev, &fields);
}

void ae_obj_bomb_round(long round, int timed_out, long score_red, long score_blue)
{
	struct ae_obj_fields fields;
	long winner = score_red > ae_obj_state.scores_logged[0] ? 0 : score_blue > ae_obj_state.scores_logged[1] ? 1 : -1;
	char why[16];

	if (ae_obj_level() < AE_OBJ_EVENTS || network_game_distributed_client())
		return;
	snprintf(why, sizeof(why), "%s", timed_out ? "time" : "goal");
	if (ae_obj_mode() == AE_OBJ_MODE_SND)
		snprintf(why, sizeof(why), "%s", ae_obj_state.pending_why[0] ? ae_obj_state.pending_why : "blast");
	ae_obj_state.pending_why[0] = 0;
	ae_obj_state.scores_logged[0] = score_red;
	ae_obj_state.scores_logged[1] = score_blue;
	ae_obj_fields_begin(&fields);
	ae_obj_fields_long(&fields, "round", round);
	ae_obj_fields_long(&fields, "winner", winner);
	ae_obj_fields_text(&fields, "why", why);
	ae_obj_fields_long(&fields, "score_r", score_red);
	ae_obj_fields_long(&fields, "score_b", score_blue);
	ae_obj_emit("round_end", &fields);
}

void ae_obj_defuse(int kind, long player_index, long held_ticks)
{
	struct ae_obj_fields fields;
	struct player_datum *player;

	if (ae_obj_level() < AE_OBJ_EVENTS || network_game_distributed_client())
		return;
	ae_obj_fields_begin(&fields);
	if (kind == AE_OBJ_DEFUSE_CANCEL)
	{
		player = player_try_and_get(player_index);
		ae_obj_fields_long(&fields, "p", ae_obj_absolute(player_index));
		ae_obj_fields_long(&fields, "b", ae_obj_player_is_bot(player_index) ? 1 : 0);
		ae_obj_fields_long(&fields, "team", player ? (long)player->team_index : -1L);
		ae_obj_fields_real(&fields, "held_s", held_ticks / (double)TICKS_PER_SECOND);
		ae_obj_emit("defuse_cancel", &fields);
		return;
	}
	if (kind == AE_OBJ_DEFUSE_DONE)
	{
		if ((long)game_time_get() - ae_obj_state.defuse_done_tick <= 1)
			return;
		ae_obj_state.defuse_done_tick = (long)game_time_get();
	}
	ae_obj_who(&fields, player_index);
	ae_obj_emit(kind == AE_OBJ_DEFUSE_START ? "defuse_start" : "defuse_done", &fields);
}

void ae_obj_snd_round(long round, long attackers, long score_red, long score_blue)
{
	struct ae_obj_fields fields;

	if (ae_obj_level() < AE_OBJ_EVENTS || network_game_distributed_client())
		return;
	ae_obj_fields_begin(&fields);
	ae_obj_fields_long(&fields, "round", round);
	ae_obj_fields_long(&fields, "attackers", attackers);
	ae_obj_fields_long(&fields, "score_r", score_red);
	ae_obj_fields_long(&fields, "score_b", score_blue);
	ae_obj_emit("round_start", &fields);
}

void ae_obj_snd_pending(char const *why)
{
	snprintf(ae_obj_state.pending_why, sizeof(ae_obj_state.pending_why), "%s", why ? why : "");
}

void ae_obj_terr_capture(int territory, long hill, int team, int from_team, long score_red, long score_blue)
{
	struct ae_obj_fields fields;

	if (ae_obj_level() < AE_OBJ_EVENTS)
		return;
	ae_obj_fields_begin(&fields);
	ae_obj_fields_long(&fields, "terr", territory);
	ae_obj_fields_long(&fields, "hill", hill);
	ae_obj_fields_long(&fields, "team", team);
	ae_obj_fields_long(&fields, "from_team", from_team);
	ae_obj_fields_long(&fields, "score_r", score_red);
	ae_obj_fields_long(&fields, "score_b", score_blue);
	ae_obj_emit("terr_capture", &fields);
}

void ae_obj_terr_state(int territory, long hill, int owner, int contested, long occupants_red, long occupants_blue,
	long progress)
{
	struct ae_obj_fields fields;

	if (ae_obj_level() < AE_OBJ_EVENTS)
		return;
	ae_obj_fields_begin(&fields);
	ae_obj_fields_long(&fields, "terr", territory);
	ae_obj_fields_long(&fields, "hill", hill);
	ae_obj_fields_text(&fields, "state", contested ? "contested" : occupants_red + occupants_blue > 0 ? "taking" :
		owner >= 0 ? "held" : "neutral");
	ae_obj_fields_long(&fields, "team", owner);
	ae_obj_fields_long(&fields, "occ_r", occupants_red);
	ae_obj_fields_long(&fields, "occ_b", occupants_blue);
	ae_obj_fields_long(&fields, "progress", progress);
	ae_obj_emit("terr_state", &fields);
}

/* ---------- 0.7.7 follow-ups */

/* (game_engine_bomb.c's playtest log, every line) the bomb's send home as the log words it */
void ae_obj_bomb_log(char const *message)
{
	struct ae_obj_fields fields;
	char why[24];
	double x, y, z;

	if (ae_obj_level() < AE_OBJ_EVENTS || network_game_distributed_client() ||
		!ae_obj_rules_bomb_reset(message, why, sizeof(why), &x, &y, &z))
	{
		return;
	}
	ae_obj_fields_begin(&fields);
	ae_obj_fields_text(&fields, "why", why);
	ae_obj_fields_point(&fields, x, y, z);
	ae_obj_emit("bomb_reset", &fields);
}

/* the carrier's arming in the goal: starts when the ticks rise from none, ends with a plant or a score (the bomb's own
lines came a moment before) or is cancelled (the carrier left or died: held_s) */
static void ae_obj_watch_arming(long now)
{
	struct ae_bomb_state state;
	struct ae_obj_fields fields;

	ae_bomb_state_get(&state);
	if (!state.running)
		return;
	if (state.arm_ticks > 0 && ae_obj_state.arm_since < 0 && state.carrier != NONE)
	{
		struct player_datum *carrier = player_try_and_get(state.carrier);

		if (carrier)
		{
			ae_obj_state.arm_since = now;
			ae_obj_state.arm_player = state.carrier;
			ae_obj_fields_begin(&fields);
			ae_obj_who(&fields, state.carrier);
			ae_obj_emit("plant_start", &fields);
		}
	}
	else if (state.arm_ticks <= 0 && ae_obj_state.arm_since >= 0)
	{
		/* (a plant puts the bomb down at once: carried no more with a plant going) */
		if (state.plant_ticks <= 0 && now - ae_obj_state.planted_tick > 3)
		{
			ae_obj_fields_begin(&fields);
			ae_obj_fields_long(&fields, "p", ae_obj_absolute(ae_obj_state.arm_player));
			ae_obj_fields_long(&fields, "b", ae_obj_player_is_bot(ae_obj_state.arm_player) ? 1 : 0);
			ae_obj_fields_long(&fields, "team", player_try_and_get(ae_obj_state.arm_player) ?
				(long)player_try_and_get(ae_obj_state.arm_player)->team_index : -1L);
			ae_obj_fields_real(&fields, "held_s", (now - ae_obj_state.arm_since) / (double)TICKS_PER_SECOND);
			ae_obj_emit("plant_cancel", &fields);
		}
		ae_obj_state.arm_since = -1;
		ae_obj_state.arm_player = NONE;
	}
}

static char const *ae_obj_object_name(long object_index)
{
	struct object_datum *object = object_index != NONE ? object_try_and_get(object_index) : NULL;
	char const *name;

	if (!object || object->definition_index == NONE)
		return "";
	name = tag_name_strip_path(tag_get_name(object->definition_index));
	return name ? name : "";
}

/* each living player's vehicle (boards, exits) and weapons (what is new in the hands is a pickup; a new unit, a spawn,
only sets the memory) */
static void ae_obj_watch_units(long now)
{
	struct data_iterator iterator;
	struct player_datum *player;

	data_iterator_new(&iterator, player_data);
	while ((player = (struct player_datum *)data_iterator_next(&iterator)) != NULL)
	{
		long slot = (long)DATUM_INDEX_TO_ABSOLUTE_INDEX(iterator.datum_index);
		struct unit_datum *unit;
		long vehicle = NONE, weapons[4] = { NONE, NONE, NONE, NONE }, index;
		boolean same_unit;

		if (slot < 0 || slot >= AE_OBJ_PLAYERS)
			continue;
		unit = player->unit_index != NONE ? (struct unit_datum *)object_try_and_get_and_verify_type(player->unit_index, _object_mask_unit) : NULL;
		if (unit && TEST_FLAG(unit->object.damage_flags, _object_dead_bit))
			unit = NULL;
		same_unit = unit && ae_obj_state.unit_of[slot] == player->unit_index;
		if (unit)
		{
			vehicle = unit->object.parent_object_index;
			for (index = 0; index < 4 && index < MAXIMUM_WEAPONS_PER_UNIT; index++)
				weapons[index] = unit->unit.weapon_object_indices[index];
		}
		if (vehicle != ae_obj_state.vehicle_of[slot])
		{
			struct ae_obj_fields fields;

			if (ae_obj_state.vehicle_of[slot] != NONE && same_unit)
			{
				ae_obj_fields_begin(&fields);
				ae_obj_who(&fields, iterator.datum_index);
				ae_obj_fields_text(&fields, "veh", ae_obj_object_name(ae_obj_state.vehicle_of[slot]));
				ae_obj_fields_text(&fields, "seat", "any");
				ae_obj_fields_real(&fields, "held_s", (now - ae_obj_state.vehicle_since[slot]) / (double)TICKS_PER_SECOND);
				ae_obj_emit("veh_exit", &fields);
			}
			if (vehicle != NONE)
			{
				struct unit_datum *car = (struct unit_datum *)object_try_and_get_and_verify_type(vehicle, _object_mask_unit);
				int seat = car && car->unit.driver_object_index == player->unit_index ? 0 :
					car && car->unit.gunner_object_index == player->unit_index ? 1 : 2;

				ae_obj_fields_begin(&fields);
				ae_obj_who(&fields, iterator.datum_index);
				ae_obj_fields_text(&fields, "veh", ae_obj_object_name(vehicle));
				ae_obj_fields_text(&fields, "seat", ae_obj_rules_seat_name(seat));
				ae_obj_emit("veh_board", &fields);
				ae_obj_state.vehicle_since[slot] = now;
			}
			ae_obj_state.vehicle_of[slot] = vehicle;
		}
		if (unit && !same_unit)
			ae_obj_state.unit_born[slot] = now;
		/* (the start kit comes a moment after the spawn) */
		if (same_unit && now - ae_obj_state.unit_born[slot] > AE_OBJ_KIT_TICKS)
		{
			for (index = 0; index < 4 && index < MAXIMUM_WEAPONS_PER_UNIT; index++)
			{
				long other;
				boolean known = weapons[index] == NONE;

				for (other = 0; other < 4 && !known; other++)
					known = weapons[index] == ae_obj_state.weapons_of[slot][other];
				if (!known && ae_obj_rules_item_logged(ae_obj_object_name(weapons[index])))
				{
					struct ae_obj_fields fields;

					ae_obj_fields_begin(&fields);
					ae_obj_who(&fields, iterator.datum_index);
					ae_obj_fields_text(&fields, "item", ae_obj_object_name(weapons[index]));
					ae_obj_emit("item_take", &fields);
				}
			}
		}
		for (index = 0; index < 4; index++)
			ae_obj_state.weapons_of[slot][index] = weapons[index];
		ae_obj_state.unit_of[slot] = unit ? player->unit_index : NONE;
		if (!unit)
			ae_obj_state.vehicle_of[slot] = NONE;
	}
}

/* (ae_round.c) the elimination's rounds, downs, revives and the zone's phases */
void ae_obj_elim_round_start(long round, long max_rounds, int switched, long wins_red, long wins_blue)
{
	struct ae_obj_fields fields;

	if (ae_obj_level() < AE_OBJ_EVENTS || network_game_distributed_client())
		return;
	ae_obj_fields_begin(&fields);
	ae_obj_fields_long(&fields, "round", round);
	ae_obj_fields_long(&fields, "max_rounds", max_rounds);
	ae_obj_fields_long(&fields, "switched", switched ? 1 : 0);
	ae_obj_fields_long(&fields, "wins_r", wins_red);
	ae_obj_fields_long(&fields, "wins_b", wins_blue);
	ae_obj_emit("elim_round_start", &fields);
}

void ae_obj_elim_round_end(long round, long winner, long wins_red, long wins_blue)
{
	struct ae_obj_fields fields;

	if (ae_obj_level() < AE_OBJ_EVENTS || network_game_distributed_client())
		return;
	ae_obj_fields_begin(&fields);
	ae_obj_fields_long(&fields, "round", round);
	ae_obj_fields_long(&fields, "winner", winner);
	ae_obj_fields_long(&fields, "wins_r", wins_red);
	ae_obj_fields_long(&fields, "wins_b", wins_blue);
	ae_obj_emit("elim_round_end", &fields);
}

void ae_obj_elim_down(long player_index, boolean out, boolean zone, real_point3d const *spot)
{
	struct ae_obj_fields fields;

	if (ae_obj_level() < AE_OBJ_EVENTS || network_game_distributed_client())
		return;
	ae_obj_fields_begin(&fields);
	ae_obj_who_at(&fields, player_index, spot, TRUE);
	ae_obj_fields_long(&fields, "zone", zone ? 1 : 0);
	ae_obj_emit(out ? "elim_out" : "elim_down", &fields);
}

void ae_obj_elim_revive(long player_index, long by, long revives)
{
	struct ae_obj_fields fields;

	if (ae_obj_level() < AE_OBJ_EVENTS || network_game_distributed_client())
		return;
	ae_obj_fields_begin(&fields);
	ae_obj_who(&fields, player_index);
	ae_obj_fields_long(&fields, "by", ae_obj_absolute(by));
	ae_obj_fields_long(&fields, "revives", revives);
	ae_obj_emit("elim_revive", &fields);
}

void ae_obj_zone_phase(long round, int phase, double radius, long left_seconds)
{
	struct ae_obj_fields fields;

	if (ae_obj_level() < AE_OBJ_EVENTS || network_game_distributed_client())
		return;
	ae_obj_fields_begin(&fields);
	ae_obj_fields_long(&fields, "round", round);
	ae_obj_fields_long(&fields, "phase", phase);
	ae_obj_fields_real(&fields, "radius", radius);
	ae_obj_fields_long(&fields, "left_s", left_seconds);
	ae_obj_emit("zone_phase", &fields);
}

/* (game_engine_race.c) a flag touched; a lap done */
void ae_obj_race_flag(long player_index, long flag_team)
{
	struct ae_obj_fields fields;

	if (ae_obj_level() < AE_OBJ_EVENTS || network_game_distributed_client())
		return;
	ae_obj_fields_begin(&fields);
	ae_obj_who(&fields, player_index);
	ae_obj_fields_long(&fields, "flag", flag_team);
	ae_obj_emit("race_flag", &fields);
}

void ae_obj_race_lap(long player_index, long lap, long lap_ticks)
{
	struct ae_obj_fields fields;

	if (ae_obj_level() < AE_OBJ_EVENTS || network_game_distributed_client())
		return;
	ae_obj_fields_begin(&fields);
	ae_obj_who(&fields, player_index);
	ae_obj_fields_long(&fields, "lap", lap);
	ae_obj_fields_real(&fields, "lap_s", lap_ticks / (double)TICKS_PER_SECOND);
	ae_obj_emit("race_lap", &fields);
}

/* ---------- where a player's objective is */

void ae_obj_objective_of(long player_index, char const **name, double *distance)
{
	struct ae_bots_objective_snapshot const *snapshot = ae_bots_objective_snapshot();
	struct player_datum *player = player_index != NONE ? player_try_and_get(player_index) : NULL;
	real_point3d here, target;
	boolean has = FALSE;

	*name = "none";
	*distance = -1.0;
	if (!snapshot || !player || !ae_obj_position(player_index, &here))
		return;
	switch (snapshot->mode)
	{
	case AE_BOTS_MODE_CTF:
		if (VALID_INDEX(player->team_index, 2))
		{
			long own = player->team_index, other = 1 - own;

			if (ae_bots_objective_player_carries(snapshot, player_index) && snapshot->flags[own].exists)
				*name = "stand", target = snapshot->flags[own].stand, has = TRUE;
			else if (snapshot->flags[other].exists)
				*name = other ? "flag1" : "flag0", target = snapshot->flags[other].position, has = TRUE;
		}
		break;
	case AE_BOTS_MODE_ODDBALL:
	case AE_BOTS_MODE_TAG:
	case AE_BOTS_MODE_JUGGERNAUT:
		if (snapshot->ball_count > 0)
		{
			long index, best = 0;

			for (index = 1; index < snapshot->ball_count; index++)
			{
				if (ae_obj_distance(&here, &snapshot->ball_positions[index]) < ae_obj_distance(&here, &snapshot->ball_positions[best]))
					best = index;
			}
			*name = "ball", target = snapshot->ball_positions[best], has = TRUE;
		}
		break;
	case AE_BOTS_MODE_KING:
		if (ae_h2_territories_on())
		{
			int in_target = 0;

			if (ae_h2_territories_bot_target(player->team_index, player_index, &here, &target, &in_target))
				*name = "terr", has = TRUE;
		}
		else if (snapshot->hill)
			*name = "hill", target = snapshot->hill_centre, has = TRUE;
		break;
	case AE_BOTS_MODE_BOMB:
		if (snapshot->bomb_carrier_player_index == player_index && VALID_INDEX(player->team_index, 2) &&
			snapshot->bomb_has_goal[1 - player->team_index])
		{
			*name = "goal", target = snapshot->bomb_goals[1 - player->team_index], has = TRUE;
		}
		else if (snapshot->bomb_planted_goal != NONE && snapshot->bomb_planted_goal >= 0 && snapshot->bomb_planted_goal < 2 &&
			snapshot->bomb_has_goal[snapshot->bomb_planted_goal])
		{
			*name = "planted", target = snapshot->bomb_goals[snapshot->bomb_planted_goal], has = TRUE;
		}
		else if (snapshot->bomb_weapon_index != NONE)
			*name = "bomb", target = snapshot->bomb_position, has = TRUE;
		break;
	default:
		break;
	}
	if (has)
		*distance = ae_obj_distance(&here, &target);
}

/* ---------- a player's line */

void ae_obj_ps(long player_index, struct ae_obj_ps_words const *words)
{
	struct ae_obj_fields fields;
	struct player_datum *player = player_index != NONE ? player_try_and_get(player_index) : NULL;
	struct object_datum *unit;
	char const *objective;
	double distance;
	char const *state = "alive";

	if (!player || player->unit_index == NONE)
		return;
	unit = object_get(player->unit_index);
	if (TEST_FLAG(unit->object.damage_flags, _object_dead_bit))
		return;
	if (unit->object.parent_object_index != NONE)
		state = "driving";
	ae_obj_objective_of(player_index, &objective, &distance);
	ae_obj_fields_begin(&fields);
	ae_obj_who(&fields, player_index);
	ae_obj_fields_long(&fields, "hp", (long)(unit->object.body_vitality * 100.0f + 0.5f));
	ae_obj_fields_long(&fields, "shield", (long)(unit->object.shield_vitality * 100.0f + 0.5f));
	ae_obj_fields_text(&fields, "job", words ? words->job : "human");
	ae_obj_fields_text(&fields, "role", words ? words->role : ae_bots_objective_player_carries(ae_bots_objective_snapshot(), player_index) ? "carrier" : "none");
	ae_obj_fields_text(&fields, "goal", words ? words->goal : "none");
	ae_obj_fields_text(&fields, "objective", objective);
	ae_obj_fields_real(&fields, "d_obj", distance);
	ae_obj_fields_text(&fields, "state", words && words->state ? words->state : state);
	ae_obj_fields_text(&fields, "seat", words ? words->seat : "none");
	ae_obj_fields_long(&fields, "ft", words ? words->fireteam : -1L);
	ae_obj_fields_text(&fields, "order", words ? words->order : "none");
	ae_obj_emit("ps", &fields);
}

/* ---------- the watchers */

static void ae_obj_watch_flags(struct ae_bots_objective_snapshot const *snapshot, long now)
{
	long team;

	for (team = 0; team < 2; team++)
	{
		struct ae_bots_objective_flag const *flag = &snapshot->flags[team];
		long was = ae_obj_state.carrier[team], carrier = flag->exists ? flag->carrier_player_index : NONE;

		if (was != carrier)
		{
			struct ae_obj_fields fields;

			if (was != NONE)
			{
				ae_obj_fields_begin(&fields);
				ae_obj_fields_long(&fields, "flag", team);
				ae_obj_who_at(&fields, was, &flag->position, TRUE);
				ae_obj_fields_text(&fields, "why", now - ae_obj_state.capture_tick[team] <= 3 ? "capture" :
					ae_obj_dead(was) ? "killed" : "dropped");
				ae_obj_fields_real(&fields, "held_s", ae_obj_state.take_tick[team] > 0 ?
					(now - ae_obj_state.take_tick[team]) / (double)TICKS_PER_SECOND : -1.0);
				ae_obj_emit("flag_drop", &fields);
			}
			if (carrier != NONE)
			{
				ae_obj_fields_begin(&fields);
				ae_obj_fields_long(&fields, "flag", team);
				ae_obj_who_at(&fields, carrier, &flag->position, FALSE);
				ae_obj_fields_text(&fields, "from", now - ae_obj_state.home_tick[team] <= AE_OBJ_HOME_TICKS ? "stand" : "dropped");
				ae_obj_fields_long(&fields, "prev", ae_obj_absolute(was));
				ae_obj_emit("flag_take", &fields);
				ae_obj_state.take_tick[team] = now;
				ae_obj_state.take_position[team] = flag->position;
			}
			ae_obj_state.carrier[team] = carrier;
		}
		/* (the engine marks a flag taken a tick before the unit holds it: still at its stand counts as home) */
		if (flag->exists && carrier == NONE && (flag->home || ae_obj_distance(&flag->position, &flag->stand) < 1.0))
			ae_obj_state.home_tick[team] = now;
	}
}

static void ae_obj_watch_balls(struct ae_bots_objective_snapshot const *snapshot, long now)
{
	struct data_iterator iterator;
	struct player_datum *player;
	boolean seen[AE_OBJ_PLAYERS];
	long slot;

	memset(seen, 0, sizeof(seen));
	data_iterator_new(&iterator, player_data);
	while ((player = (struct player_datum *)data_iterator_next(&iterator)) != NULL)
	{
		struct ae_obj_fields fields;

		slot = (long)DATUM_INDEX_TO_ABSOLUTE_INDEX(iterator.datum_index);
		if (slot < 0 || slot >= AE_OBJ_PLAYERS)
			continue;
		seen[slot] = TRUE;
		{
			boolean holds = player->unit_index != NONE && ae_strip_oddball_ball_count(iterator.datum_index) > 0;

			if (holds && ae_obj_state.ball_since[slot] < 0)
			{
				ae_obj_fields_begin(&fields);
				ae_obj_who(&fields, iterator.datum_index);
				ae_obj_fields_long(&fields, "prev", now - ae_obj_state.ball_drop_tick <= AE_OBJ_PASS_TICKS ? ae_obj_state.ball_dropper : -1L);
				ae_obj_emit("ball_take", &fields);
				ae_obj_state.ball_since[slot] = now;
			}
			else if (!holds && ae_obj_state.ball_since[slot] >= 0)
			{
				real_point3d last = *global_origin3d;

				ae_obj_fields_begin(&fields);
				ae_obj_who_at(&fields, iterator.datum_index, &last, FALSE);
				ae_obj_fields_text(&fields, "why", ae_obj_dead(iterator.datum_index) ? "killed" : "dropped");
				ae_obj_fields_real(&fields, "held_s", (now - ae_obj_state.ball_since[slot]) / (double)TICKS_PER_SECOND);
				ae_obj_emit("ball_drop", &fields);
				ae_obj_state.ball_since[slot] = -1;
				ae_obj_state.ball_dropper = slot;
				ae_obj_state.ball_drop_tick = now;
			}
		}
	}
	for (slot = 0; slot < AE_OBJ_PLAYERS; slot++)
	{
		if (!seen[slot])
			ae_obj_state.ball_since[slot] = -1;
	}
	if (snapshot->mode == AE_BOTS_MODE_JUGGERNAUT && ae_juggernaut_active())
	{
		long holder = NONE;

		data_iterator_new(&iterator, player_data);
		while ((player = (struct player_datum *)data_iterator_next(&iterator)) != NULL)
		{
			if (ae_juggernaut_player_is(iterator.datum_index))
				holder = iterator.datum_index;
		}
		if (holder != ae_obj_state.jugg)
		{
			struct ae_obj_fields fields;

			ae_obj_fields_begin(&fields);
			ae_obj_fields_long(&fields, "from", ae_obj_absolute(ae_obj_state.jugg));
			ae_obj_fields_long(&fields, "to", ae_obj_absolute(holder));
			ae_obj_emit("jugg_change", &fields);
			ae_obj_state.jugg = holder;
		}
	}
}

static void ae_obj_watch_hill(struct ae_bots_objective_snapshot const *snapshot, long now)
{
	struct data_iterator iterator;
	struct player_datum *player;
	long occupants[2] = { 0, 0 };
	int state, team = -1;
	struct ae_obj_fields fields;

	if (ae_h2_territories_on())
		return;
	if (snapshot->hill && (!ae_obj_state.hill || ae_obj_distance(&ae_obj_state.hill_centre, &snapshot->hill_centre) > 0.5))
	{
		ae_obj_fields_begin(&fields);
		if (!ae_obj_state.hill)
		{
			ae_obj_fields_point(&fields, snapshot->hill_centre.x, snapshot->hill_centre.y, snapshot->hill_centre.z);
			ae_obj_emit("hill_new", &fields);
		}
		else
		{
			ae_obj_fields_real(&fields, "px", ae_obj_state.hill_centre.x);
			ae_obj_fields_real(&fields, "py", ae_obj_state.hill_centre.y);
			ae_obj_fields_real(&fields, "pz", ae_obj_state.hill_centre.z);
			ae_obj_fields_point(&fields, snapshot->hill_centre.x, snapshot->hill_centre.y, snapshot->hill_centre.z);
			ae_obj_emit("hill_move", &fields);
		}
		ae_obj_state.hill = TRUE;
		ae_obj_state.hill_centre = snapshot->hill_centre;
		ae_obj_state.hill_state = -1;
	}
	else if (!snapshot->hill)
	{
		ae_obj_state.hill = FALSE;
		return;
	}
	data_iterator_new(&iterator, player_data);
	while ((player = (struct player_datum *)data_iterator_next(&iterator)) != NULL)
	{
		long slot = (long)DATUM_INDEX_TO_ABSOLUTE_INDEX(iterator.datum_index);
		boolean inside = player->unit_index != NONE && ae_bots_objective_player_in_hill(iterator.datum_index);

		if (inside && VALID_INDEX(player->team_index, 2))
			occupants[player->team_index]++;
		if (slot < 0 || slot >= AE_OBJ_PLAYERS || inside == ae_obj_state.inside[slot] ||
			now - ae_obj_state.inside_tick[slot] < AE_OBJ_HILL_QUIET)
		{
			continue;
		}
		ae_obj_state.inside[slot] = inside;
		ae_obj_state.inside_tick[slot] = now;
		ae_obj_fields_begin(&fields);
		ae_obj_who(&fields, iterator.datum_index);
		ae_obj_emit(inside ? "hill_enter" : "hill_leave", &fields);
	}
	state = occupants[0] && occupants[1] ? 2 : occupants[0] || occupants[1] ? 1 : 0;
	team = state == 1 ? (occupants[0] ? 0 : 1) : -1;
	if (state != ae_obj_state.hill_state || team != ae_obj_state.hill_team)
	{
		ae_obj_state.hill_state = state;
		ae_obj_state.hill_team = team;
		ae_obj_fields_begin(&fields);
		ae_obj_fields_text(&fields, "state", state == 2 ? "contested" : state == 1 ? "held" : "neutral");
		ae_obj_fields_long(&fields, "team", team);
		ae_obj_fields_long(&fields, "occ_r", occupants[0]);
		ae_obj_fields_long(&fields, "occ_b", occupants[1]);
		ae_obj_emit("hill_state", &fields);
	}
}

/* the mode's objective as two compact words (the snap line's o1 and o2) */
static void ae_obj_snapshot_words(struct ae_bots_objective_snapshot const *snapshot, char *o1, size_t size1, char *o2, size_t size2)
{
	snprintf(o1, size1, "-");
	snprintf(o2, size2, "-");
	switch (snapshot->mode)
	{
	case AE_BOTS_MODE_CTF:
	{
		char *out[2] = { o1, o2 };
		size_t sizes[2] = { size1, size2 };
		long team;

		for (team = 0; team < 2; team++)
		{
			struct ae_bots_objective_flag const *flag = &snapshot->flags[team];

			if (!flag->exists)
				snprintf(out[team], sizes[team], "none");
			else if (flag->carrier_player_index != NONE)
				snprintf(out[team], sizes[team], "carried:%ld", ae_obj_absolute(flag->carrier_player_index));
			else if (flag->home)
				snprintf(out[team], sizes[team], "home");
			else
				snprintf(out[team], sizes[team], "dropped:%.1f,%.1f,%.1f", flag->position.x, flag->position.y, flag->position.z);
		}
		break;
	}
	case AE_BOTS_MODE_ODDBALL:
	case AE_BOTS_MODE_TAG:
	case AE_BOTS_MODE_JUGGERNAUT:
	{
		long slot, length = 0;

		o1[0] = 0;
		for (slot = 0; slot < AE_OBJ_PLAYERS && length < (long)size1 - 6; slot++)
		{
			if (ae_obj_state.ball_since[slot] >= 0)
				length += snprintf(o1 + length, size1 - (size_t)length, "%s%ld", length ? ";" : "", slot);
		}
		if (!length && snapshot->ball_count > 0)
			snprintf(o1, size1, "free:%.1f,%.1f,%.1f", snapshot->ball_positions[0].x, snapshot->ball_positions[0].y,
				snapshot->ball_positions[0].z);
		else if (!length)
			snprintf(o1, size1, "none");
		break;
	}
	case AE_BOTS_MODE_KING:
		if (ae_h2_territories_on())
		{
			long red, blue, neutral, total;

			ae_h2_territories_held(&red, &blue, &neutral, &total);
			snprintf(o1, size1, "%ld/%ld/%ld", red, blue, neutral);
			snprintf(o2, size2, "%ld", total);
		}
		else if (snapshot->hill)
		{
			snprintf(o1, size1, "%.1f,%.1f,%.1f", snapshot->hill_centre.x, snapshot->hill_centre.y, snapshot->hill_centre.z);
			snprintf(o2, size2, "%s", ae_obj_state.hill_state == 2 ? "contested" : ae_obj_state.hill_state == 1 ?
				(ae_obj_state.hill_team ? "held_b" : "held_r") : "neutral");
		}
		else
			snprintf(o1, size1, "none");
		break;
	case AE_BOTS_MODE_BOMB:
		if (snapshot->bomb_carrier_player_index != NONE)
			snprintf(o1, size1, "carried:%ld", ae_obj_absolute(snapshot->bomb_carrier_player_index));
		else if (snapshot->bomb_planted_goal != NONE)
			snprintf(o1, size1, "planted:%ld", (long)snapshot->bomb_planted_goal);
		else if (snapshot->bomb_weapon_index != NONE)
			snprintf(o1, size1, "loose:%.1f,%.1f,%.1f", snapshot->bomb_position.x, snapshot->bomb_position.y,
				snapshot->bomb_position.z);
		else
			snprintf(o1, size1, "none");
		snprintf(o2, size2, "%ld", (long)game_engine_bomb_round());
		break;
	default:
		break;
	}
}

static void ae_obj_snap(struct ae_bots_objective_snapshot const *snapshot, long now)
{
	struct ae_obj_fields fields;
	struct ae_strip_snapshot strip;
	char o1[96], o2[96];
	long left = -1;

	if (ae_strip_engine_snapshot(0, &strip) && !strip.clock_counts_up)
		left = strip.clock_seconds;
	ae_obj_snapshot_words(snapshot, o1, sizeof(o1), o2, sizeof(o2));
	ae_obj_fields_begin(&fields);
	ae_obj_fields_long(&fields, "score_r", ae_obj_team_score(0));
	ae_obj_fields_long(&fields, "score_b", ae_obj_team_score(1));
	ae_obj_fields_long(&fields, "time_s", now / TICKS_PER_SECOND);
	ae_obj_fields_long(&fields, "time_left_s", left);
	ae_obj_fields_text(&fields, "o1", o1);
	ae_obj_fields_text(&fields, "o2", o2);
	ae_obj_emit("snap", &fields);
}

void ae_obj_tick(void)
{
	struct ae_bots_objective_snapshot const *snapshot;
	boolean running;
	long now;
	int level;

	level = ae_obj_level();
	if (level < AE_OBJ_EVENTS || network_game_distributed_client())
		return;
	now = (long)game_time_get();
	running = game_engine && game_engine_running() && !game_engine_game_over();
	/* (a new game: its clock starts again) */
	if (ae_obj_state.running && now < ae_obj_state.last_tick)
		ae_obj_state.running = FALSE;
	if (running && !ae_obj_state.running)
	{
		struct ae_obj_fields fields;
		struct data_iterator iterator;
		struct player_datum *player;
		long players = 0, bots = 0;
		char map[96];
		char const *name = global_scenario_index != NONE ? tag_name_strip_path(tag_get_name(global_scenario_index)) : "none";
		size_t index = 0;

		ae_obj_forget();
		data_iterator_new(&iterator, player_data);
		while ((player = (struct player_datum *)data_iterator_next(&iterator)) != NULL)
		{
			players++;
			bots += ae_obj_player_is_bot(iterator.datum_index) ? 1 : 0;
		}
		for (; name && name[index] && index + 1 < sizeof(map); index++)
			map[index] = name[index] == ' ' ? '_' : name[index];
		map[index] = 0;
		ae_obj_fields_begin(&fields);
		ae_obj_fields_text(&fields, "map", map);
		ae_obj_fields_long(&fields, "teams", game_engine_has_teams() ? 1 : 0);
		ae_obj_fields_long(&fields, "players", players);
		ae_obj_fields_long(&fields, "bots", bots);
		ae_obj_emit("game_start", &fields);
		ae_obj_state.running = TRUE;
		ae_obj_state.next_second = now;
	}
	else if (!running && ae_obj_state.running)
	{
		struct ae_obj_fields fields;
		long red = ae_obj_team_score(0), blue = ae_obj_team_score(1);

		ae_obj_fields_begin(&fields);
		ae_obj_fields_long(&fields, "winner", !game_engine_has_teams() || red == blue ? -1L : red > blue ? 0L : 1L);
		ae_obj_fields_long(&fields, "score_r", red);
		ae_obj_fields_long(&fields, "score_b", blue);
		ae_obj_emit("game_end", &fields);
		ae_obj_state.running = FALSE;
	}
	ae_obj_state.last_tick = now;
	if (!running)
		return;
	ae_obj_watch_units(now);
	if (ae_obj_mode() == AE_OBJ_MODE_BOMB || ae_obj_mode() == AE_OBJ_MODE_SND)
		ae_obj_watch_arming(now);
	snapshot = ae_bots_objective_snapshot();
	if (!snapshot)
		return;
	switch (snapshot->mode)
	{
	case AE_BOTS_MODE_CTF:
		ae_obj_watch_flags(snapshot, now);
		break;
	case AE_BOTS_MODE_ODDBALL:
	case AE_BOTS_MODE_TAG:
	case AE_BOTS_MODE_JUGGERNAUT:
		ae_obj_watch_balls(snapshot, now);
		break;
	case AE_BOTS_MODE_KING:
		ae_obj_watch_hill(snapshot, now);
		break;
	default:
		break;
	}
	if (now >= ae_obj_state.next_second)
	{
		struct data_iterator iterator;
		struct player_datum *player;

		ae_obj_state.next_second = now + AE_OBJ_SECOND;
		if (level >= AE_OBJ_SNAP)
			ae_obj_snap(snapshot, now);
		if (level >= AE_OBJ_FULL)
		{
			/* (the people's lines; a bot's comes from its own tick, with what it is doing) */
			data_iterator_new(&iterator, player_data);
			while ((player = (struct player_datum *)data_iterator_next(&iterator)) != NULL)
			{
				if (!ae_obj_player_is_bot(iterator.datum_index))
					ae_obj_ps(iterator.datum_index, NULL);
			}
		}
	}
}
