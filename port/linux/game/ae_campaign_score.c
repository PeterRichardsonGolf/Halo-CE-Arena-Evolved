/*
AE_CAMPAIGN_SCORE.C

The campaign's score, the engine's side: see ae_campaign_score.h. The rules are ae_campaign_score_rules.c's and
ae_campaign_medals_rules.c's. Derived from AGPL-3.0-or-later code (README credits).

The score is a log of entries, each with the game time it happened at and its player's slot; the totals are summed from it.
Going back to a checkpoint (a death, or Revert to Checkpoint) takes nothing from the log, nor from the level's time, which is
counted on each tick the game time goes forward, not read from it. Restarting the level starts it all again.

Saved games: the saved game file's size is fixed (HALO_PORT_GAME_STATE_SIZE) and checked, so the log is not put in it. It is
written beside it, in a file of the saved game's own name with ".score" after it, each time the saved game is written (Save and
Quit and the checkpoints), bound to the saved game's checksum and the level's name. A saved game taken reads the file back;
a file of another saved game, another level, another version, a damaged or an oversized one is refused with its reason in the
log, and the score starts at 0. With the setting off nothing is written and nothing is read.
*/

#include "cseries.h"
#include <stdio.h>
#include <string.h>
#include <xtl.h>
#include "game/game.h"
#include "game/game_allegiance.h"
#include "game/game_engine.h"
#include "game/players.h"
#include "ai/actors.h"
#include "ai/actor_definitions.h"
#include "ai/actor_types.h"
#include "objects/objects.h"
#include "scenario/scenario.h"
#include "scenario/scenario_definitions.h"
#include "units/units.h"
#include "networking/network_game_globals.h"
#include "networking/network_game_manager.h"

#include "ae_campaign_score.h"
#include "ae_glue_career_xp.h"
#include "ae_campaign_score_rules.h"
#include "ae_campaign_medals_rules.h"
#include "ae_saved_game_file.h"
#include "halo_firefight.h"
#include "ae_test_start.h"
#include "campaign_score_values.h"
#include "network_coop.h"
#include "ae_companion.h"
#include "ae_dev_cmd.h"
#include "ae_dev_mode.h"
#include "ae_script_notice.h"

/* units.c's _unit_damage_part_head (its enum is its own): the body part object_damage_aftermath is told of */
#define HEAD_BODY_PART 2
/* a melee hit from behind: the blow going this nearly the way the body faces */
#define BACKSMACK_DOT 0.5f
/* the units a level's one shot, one kill remembers as hurt */
#define HURT_SLOTS 2048
/* the tags an unknown enemy is named for, each logged once */
#define UNKNOWN_TAGS 64

void platform_log(char const *format, ...);
int config_boolean(char const *name);
unsigned long config_changes(void);
char const *config_string(char const *name);
int config_set(char const *name, char const *value);
char *tag_get_name(long tag_index);
/* player_ui.h's: player 1's profile folder, ending in its separator */
unsigned char player_ui_get_path_to_local_player_profile_directory(short local_player_index, char *path);

static struct ae_campaign_score_log score_log;
static struct ae_campaign_medals_state medals;
/* the last game time seen (-1: none yet; a smaller one is a checkpoint reverted to) */
static long last_time = -1;
static int competitive;
static int won_logged;
/* the level's career XP was paid (a level won is told once) */
static int xp_paid;
/* the units hurt by any blow since the level began (or a checkpoint was reverted to), salted by their index */
static long hurt[HURT_SLOTS];
static long unknown_tags[UNKNOWN_TAGS];
static int unknown_tag_count;
static unsigned char file_bytes[AE_CAMPAIGN_SCORE_FILE_MAXIMUM_SIZE];

static char const *const difficulty_names[AE_CAMPAIGN_SCORE_DIFFICULTIES] = { "EASY", "NORMAL", "HEROIC", "LEGENDARY" };
static char const *const style_words[AE_CAMPAIGN_SCORE_STYLE_COUNT] = { "one_shot", "headshot", "melee", "backsmack" };
static char const *const kind_words[AE_CAMPAIGN_SCORE_ENTRY_COUNT] = { "killed", "killed an ally", "betrayed", "died", "reverted", "medal" };

/* ---------- settings */

static int setting_on(void)
{
	static unsigned long changes = (unsigned long)-1;
	static int on = 0;

	if (changes != config_changes())
	{
		changes = config_changes();
		on = config_boolean("game.campaign_score") != 0;
	}
	return on;
}

static int revert_cost_on(void)
{
	static unsigned long changes = (unsigned long)-1;
	static int on = 1;

	if (changes != config_changes())
	{
		changes = config_changes();
		on = config_boolean("game.campaign_score_revert_cost") != 0;
	}
	return on;
}

/* the score type a resumed game was saved with (0 shared, 1 each player's own), or -1 when this run was not resumed: a score log keeps
the meaning it was made with, whatever the setting is now */
static int resumed_type = -1;

static int type_competitive(void)
{
	static unsigned long changes = (unsigned long)-1;
	static int each = 0;

	if (resumed_type >= 0)
		return resumed_type;
	if (changes != config_changes())
	{
		changes = config_changes();
		each = ae_campaign_score_type_competitive(config_string("network.coop_score_type"));
	}
	return each;
}

/* a campaign level (a solo scenario) is being played */
static int campaign_level(void)
{
	struct scenario *scenario;

	if (!game_in_progress())
		return 0;
	/* (Firefight scores as a campaign level does, on any map) */
	if (firefight_active())
		return 1;
	if (game_engine_running())
		return 0;
	scenario = global_scenario_try_and_get();
	return scenario != NULL && scenario->type == _scenario_type_solo;
}

int ae_campaign_score_active(void)
{
	return (setting_on() || firefight_active()) && campaign_level() && game_connection() != _game_connection_network_client;
}

static int difficulty(void)
{
	short level = game_difficulty_level_get();

	return level < 0 ? 0 : level >= AE_CAMPAIGN_SCORE_DIFFICULTIES ? AE_CAMPAIGN_SCORE_DIFFICULTIES - 1 : level;
}

float ae_campaign_score_skulls_multiplier(void)
{
	/* (the skulls' multiplier plugs in here: each skull on, its value, combined as the values' skulls say) */
	return 1.0f;
}

/* ---------- time and the hurt */

static void hurt_reset(void)
{
	long index;

	for (index = 0; index < HURT_SLOTS; index++)
		hurt[index] = NONE;
}

static long hurt_slot(long unit_index)
{
	long index = (long)((((unsigned long)unit_index * 2654435761UL) >> 16) % HURT_SLOTS);
	long tries;

	for (tries = 0; tries < HURT_SLOTS; tries++, index = (index + 1) % HURT_SLOTS)
	{
		if (hurt[index] == unit_index || hurt[index] == NONE)
			return index;
	}
	return NONE;
}

static void update(void)
{
	long time = game_time_get();

	if (last_time == -1)
		score_log.ticks = time > 0 ? time : 0;
	else if (time > last_time)
		score_log.ticks += time - last_time;
	else if (time < last_time)
		hurt_reset();
	last_time = time;
}

void ae_campaign_score_tick(void)
{
	/* (a campaign level has no game engine: its tick gives the companions' bodies their actors, and runs the DEV MODE
	commands waiting) */
	ae_companion_tick();
	ae_script_notice_tick();
	if (ae_dev_mode_on() && !game_engine_running())
		ae_dev_cmd_tick();
	if ((setting_on() || firefight_active()) && campaign_level())
		update();
}

void ae_campaign_score_new_map(void)
{
	/* (every level, the setting on or off: the state is only ever read with it on) */
	ae_campaign_score_log_reset(&score_log);
	ae_campaign_medals_reset(&medals);
	hurt_reset();
	unknown_tag_count = 0;
	last_time = -1;
	won_logged = 0;
	xp_paid = 0;
	competitive = 0;
	resumed_type = -1;
}

/* ---------- the log */

static int slot_of(long player_index)
{
	struct player_datum const *player = player_try_and_get(player_index);
	int slot;

	if (!player)
		return -1;
	slot = (int)DATUM_INDEX_TO_ABSOLUTE_INDEX(player_index);
	return slot < AE_CAMPAIGN_SCORE_PLAYERS ? slot : -1;
}

static char const *tag_text(long tag_index)
{
	char const *name = tag_index != NONE ? tag_get_name(tag_index) : NULL;

	return name ? name : "";
}

static void append(int kind, long variant_index, int slot, long points, unsigned char style)
{
	update();
	if (!ae_campaign_score_log_append(&score_log, game_time_get(), kind, variant_index, slot, points, style))
	{
		if (score_log.count >= AE_CAMPAIGN_SCORE_MAXIMUM_ENTRIES)
			platform_log("score: the level's %d entries are logged: no more are scored", (int)AE_CAMPAIGN_SCORE_MAXIMUM_ENTRIES);
		return;
	}
	if (kind == AE_CAMPAIGN_SCORE_ENTRY_MEDAL)
		platform_log("score: player %d, medal %s (AE medal %d), %+ld, total %ld", slot,
			variant_index >= 0 && variant_index < AE_CAMPAIGN_MEDAL_COUNT ? ae_campaign_medal_descriptions[variant_index].label : "?",
			variant_index >= 0 && variant_index < AE_CAMPAIGN_MEDAL_COUNT ? ae_campaign_medal_descriptions[variant_index].ae_medal : -1,
			points, ae_campaign_score_log_points(&score_log, slot, type_competitive()));
	else
		platform_log("score: player %d, %s %s, %+ld (style %d), total %ld", slot, kind_words[kind],
			variant_index == NONE && kind <= AE_CAMPAIGN_SCORE_ENTRY_ALLY ? "(staged)" : tag_text(variant_index), points,
			(int)style, ae_campaign_score_log_points(&score_log, slot, type_competitive()));
}

static void give_medals(struct ae_campaign_medal_award const *awards, int count)
{
	int index;

	for (index = 0; index < count; index++)
	{
		long points = ae_campaign_score_medal_points(&campaign_score_values, ae_campaign_medal_descriptions[awards[index].medal].key,
			difficulty(), ae_campaign_score_skulls_multiplier());

		append(AE_CAMPAIGN_SCORE_ENTRY_MEDAL, awards[index].medal, awards[index].slot, points, 0);
	}
}

/* the medals a kill earns, with the facts of it the engine knows */
static void kill_medals(long killing_player_index, long killer_object_index, long victim_unit_index, long damage_definition_index,
	unsigned char style)
{
	struct ae_campaign_medal_award awards[AE_CAMPAIGN_MEDALS_MOST_PER_KILL];
	struct ae_campaign_kill kill;
	struct player_datum const *player = player_try_and_get(killing_player_index);
	struct object_datum *killer = killer_object_index != NONE ? object_try_and_get(killer_object_index) : NULL;
	int slot = slot_of(killing_player_index);

	if (slot < 0 || !player)
		return;
	memset(&kill, 0, sizeof(kill));
	kill.killer_slot = slot;
	kill.victim_unit = victim_unit_index;
	kill.killer_object = killer_object_index;
	kill.ticks = score_log.ticks;
	kill.weapon = ae_campaign_medals_weapon(&campaign_score_values,
		damage_definition_index != NONE ? tag_get_name(damage_definition_index) : NULL);
	kill.headshot = (style >> AE_CAMPAIGN_SCORE_STYLE_HEADSHOT) & 1;
	kill.melee = (style >> AE_CAMPAIGN_SCORE_STYLE_MELEE) & 1;
	kill.backsmack = (style >> AE_CAMPAIGN_SCORE_STYLE_BACKSMACK) & 1;
	kill.wheelman_slot = -1;
	/* Wheelman: the driver of the vehicle the killer rides, a player, another than the killer */
	if (killer)
	{
		long vehicle_index = NONE;
		struct unit_datum *vehicle;

		if (killer->object.type == _object_type_vehicle)
			vehicle_index = killer_object_index;
		else if (killer->object.parent_object_index != NONE)
			vehicle_index = killer->object.parent_object_index;
		vehicle = vehicle_index != NONE ? (struct unit_datum *)object_try_and_get_and_verify_type(vehicle_index, _object_mask_vehicle) : NULL;
		if (vehicle && vehicle->unit.driver_object_index != NONE && vehicle->unit.driver_object_index != killer_object_index)
		{
			struct unit_datum *driver = (struct unit_datum *)object_try_and_get_and_verify_type(vehicle->unit.driver_object_index,
				_object_mask_unit);

			if (driver && driver->unit.player_index != NONE)
				kill.wheelman_slot = slot_of(driver->unit.player_index);
		}
	}
	{
		struct unit_datum *unit = player->unit_index != NONE ?
			(struct unit_datum *)object_try_and_get_and_verify_type(player->unit_index, _object_mask_unit) : NULL;

		if (!unit || TEST_FLAG(unit->object.damage_flags, _object_dead_bit))
			kill.killer_dead = 1;
		else if (unit->object.shield_vitality <= 0.0f && unit->object.body_vitality > 0.0f &&
			unit->object.body_vitality <= campaign_score_values.medal_close_call_health)
			kill.killer_nearly_dead = 1;
	}
	give_medals(awards, ae_campaign_medals_kill(&medals, &campaign_score_values, &kill, awards, AE_CAMPAIGN_MEDALS_MOST_PER_KILL));
}

/* an unknown enemy's tag, named once in the log */
static void unknown_enemy_logged(long variant_index)
{
	int index;

	for (index = 0; index < unknown_tag_count; index++)
	{
		if (unknown_tags[index] == variant_index)
			return;
	}
	if (unknown_tag_count < UNKNOWN_TAGS)
		unknown_tags[unknown_tag_count++] = variant_index;
	platform_log("score: %s is not in the value table: it scores %d points", tag_text(variant_index),
		(int)AE_CAMPAIGN_SCORE_UNKNOWN_POINTS);
}

void ae_campaign_score_unit_damaged(long unit_index, long killing_player_index, short body_part, int melee, void const *direction,
	int lethal, int no_statistics, long damage_definition_index, long killer_object_index)
{
	struct campaign_score_values const *values = &campaign_score_values;
	struct unit_datum *unit;
	long slot, variant_index = NONE, base = 0;
	unsigned char style = 0;
	char lower[128];
	int hostile = 0;

	if (!ae_campaign_score_active() || !object_try_and_get_and_verify_type(unit_index, _object_mask_unit))
		return;
	update();
	slot = hurt_slot(unit_index);
	if (!lethal)
	{
		if (slot != NONE)
			hurt[slot] = unit_index;
		return;
	}
	if (no_statistics)
		return;
	unit = unit_get(unit_index);

	/* a player's death, and another player's betrayal */
	if (unit->object.type == _object_type_biped && unit->unit.player_index != NONE)
	{
		int dead = slot_of(unit->unit.player_index), killer = slot_of(killing_player_index);

		if (dead >= 0 && values->penalty_death)
			append(AE_CAMPAIGN_SCORE_ENTRY_DEATH, NONE, dead, values->penalty_death, 0);
		if (killer >= 0 && killer != dead && values->penalty_betrayal)
			append(AE_CAMPAIGN_SCORE_ENTRY_BETRAYAL, NONE, killer, values->penalty_betrayal, 0);
		ae_campaign_medals_player_died(&medals, dead, killer_object_index);
		return;
	}

	/* the enemy's (or the ally's) points: a vehicle's by its name, else its actor variant's */
	if (unit->object.type == _object_type_vehicle)
	{
		variant_index = unit->definition_index;
		ae_campaign_score_lower(tag_text(variant_index), lower, sizeof(lower));
		base = ae_campaign_score_match_points(values->vehicles, values->vehicle_count, lower);
		if (!base)
			base = ae_campaign_score_match_points(values->ally_vehicles, values->ally_vehicle_count, lower);
	}
	else
	{
		long actor_index = unit->unit.swarm_actor_index != NONE ? unit->unit.swarm_actor_index : unit->unit.actor_index;
		struct actor_variant_definition *variant;
		short actor_type;

		if (actor_index == NONE)
			return;
		variant_index = actor_get(actor_index)->meta.variant_definition_index;
		if (variant_index == NONE)
			return;
		variant = actor_variant_definition_get(variant_index);
		if (variant->actor_reference.index == NONE)
			return;
		actor_type = actor_definition_get(variant->actor_reference.index)->type;
		ae_campaign_score_lower(tag_text(variant_index), lower, sizeof(lower));
		base = ae_campaign_score_actor_points(values->enemies, values->enemy_count, actor_type, lower);
		if (!base)
			base = ae_campaign_score_actor_points(values->allies, values->ally_count, actor_type, lower);
		if (!base && killing_player_index != NONE)
		{
			/* (a type the values have nothing for, a custom campaign's: scored as the stated default when it is an enemy
			of the player that killed it) */
			struct player_datum const *killer = player_try_and_get(killing_player_index);

			hostile = killer && actor_type != _actor_player && game_team_is_enemy(killer->team_index, unit->object.owner_team_index);
			base = ae_campaign_score_unknown_points(hostile);
			if (base)
				unknown_enemy_logged(variant_index);
		}
	}
	if (base < 0)
	{
		if (slot_of(killing_player_index) >= 0)
			append(AE_CAMPAIGN_SCORE_ENTRY_ALLY, variant_index, slot_of(killing_player_index), base, 0);
		return;
	}
	if (base == 0)
		return;
	/* (an enemy no player killed scores nothing) */
	if (killing_player_index == NONE || slot_of(killing_player_index) < 0)
		return;

	/* the kill's style: hurt by nothing before, a hit to the head, a melee hit, from behind */
	if (slot != NONE && hurt[slot] != unit_index)
		style |= (unsigned char)(1 << AE_CAMPAIGN_SCORE_STYLE_ONE_SHOT);
	if (body_part == HEAD_BODY_PART)
		style |= (unsigned char)(1 << AE_CAMPAIGN_SCORE_STYLE_HEADSHOT);
	if (melee)
	{
		real_vector3d blow = { 0.0f, 0.0f, 0.0f };
		int behind;

		if (direction)
			blow = *(real_vector3d const *)direction;
		behind = direction && normalize3d(&blow) > 0.0f && dot_product3d(&blow, &unit->object.forward) > BACKSMACK_DOT;
		style |= (unsigned char)(1 << (behind ? AE_CAMPAIGN_SCORE_STYLE_BACKSMACK : AE_CAMPAIGN_SCORE_STYLE_MELEE));
	}
	if (slot != NONE)
		hurt[slot] = unit_index;
	append(AE_CAMPAIGN_SCORE_ENTRY_KILL, variant_index, slot_of(killing_player_index),
		ae_campaign_score_kill_points(values, base, style, difficulty(), ae_campaign_score_skulls_multiplier()), style);
	kill_medals(killing_player_index, killer_object_index, unit_index, damage_definition_index, style);
}

/* points a Firefight objective awards a player (no kill, no medals) */
void ae_campaign_score_award(long player_index, long points)
{
	int slot = slot_of(player_index);

	if (!ae_campaign_score_active() || slot < 0 || !points)
		return;
	append(AE_CAMPAIGN_SCORE_ENTRY_KILL, NONE, slot, points, 0);
}

void ae_campaign_score_checkpoint_reverted(void)
{
	long cost = ae_campaign_score_revert_points(&campaign_score_values, revert_cost_on());
	short local_player_index;

	if (!ae_campaign_score_active() || !cost)
		return;
	/* (each of this machine's players: the game goes back for all of them) */
	for (local_player_index = local_player_get_next(NONE); local_player_index != NONE;
		local_player_index = local_player_get_next(local_player_index))
	{
		int slot = slot_of(local_player_get_player_index(local_player_index));

		if (slot >= 0)
			append(AE_CAMPAIGN_SCORE_ENTRY_REVERT, NONE, slot, cost, 0);
	}
}

void ae_campaign_score_level_won(void)
{
	struct data_iterator iterator;
	struct player_datum *player;
	long par, points;
	float time_multiplier;

	/* (the career XP of the level, with the score setting on or off: once for a level won, never for a client of a network game) */
	if (campaign_level() && game_connection() != _game_connection_network_client && !xp_paid)
	{
		xp_paid = 1;
		ae_career_xp_campaign_won(difficulty());
	}
	if (!ae_campaign_score_active() || won_logged)
		return;
	won_logged = 1;
	update();
	competitive = type_competitive();
	/* Perfection: won without dying, with enough kills */
	data_iterator_new(&iterator, player_data);
	while ((player = (struct player_datum *)data_iterator_next(&iterator)) != NULL)
	{
		int slot = slot_of(iterator.datum_index);

		if (slot >= 0 && !player->quit_out_of_game && ae_campaign_medals_perfection(&campaign_score_values,
			(int)ae_campaign_score_log_count(&score_log, slot, AE_CAMPAIGN_SCORE_ENTRY_DEATH),
			(int)ae_campaign_score_log_count(&score_log, slot, AE_CAMPAIGN_SCORE_ENTRY_KILL)))
		{
			struct ae_campaign_medal_award award;

			award.slot = slot;
			award.medal = AE_CAMPAIGN_MEDAL_PERFECTION;
			give_medals(&award, 1);
		}
	}
	par = ae_campaign_score_par_seconds();
	time_multiplier = ae_campaign_score_time_multiplier();
	points = ae_campaign_score_log_team_total(&score_log);
	platform_log("score: level won on %s: %ld entries, time %ld s, par %ld s, points %ld, time x%.2f, score %ld",
		difficulty_names[difficulty()], score_log.count, score_log.ticks / AE_CAMPAIGN_SCORE_TICKS_PER_SECOND, par, points,
		(double)time_multiplier, ae_campaign_score_final(points, time_multiplier));
	{
		int slot;

		for (slot = 0; slot < AE_CAMPAIGN_SCORE_PLAYERS; slot++)
		{
			if (ae_campaign_score_log_count(&score_log, slot, AE_CAMPAIGN_SCORE_ENTRY_KILL) ||
				ae_campaign_score_log_total(&score_log, slot))
				platform_log("score: player %d: %ld points, %ld kills, %ld deaths, %s", slot,
					ae_campaign_score_final(ae_campaign_score_log_points(&score_log, slot, competitive), time_multiplier),
					ae_campaign_score_log_count(&score_log, slot, AE_CAMPAIGN_SCORE_ENTRY_KILL),
					ae_campaign_score_log_count(&score_log, slot, AE_CAMPAIGN_SCORE_ENTRY_DEATH), competitive ? "own score" : "shared score");
		}
	}
}

/* ---------- the numbers */

long ae_campaign_score_player_points(long player_index)
{
	update();
	return ae_campaign_score_log_points(&score_log, slot_of(player_index), type_competitive());
}

long ae_campaign_score_player_kills(long player_index)
{
	return ae_campaign_score_log_count(&score_log, slot_of(player_index), AE_CAMPAIGN_SCORE_ENTRY_KILL);
}

long ae_campaign_score_player_deaths(long player_index)
{
	return ae_campaign_score_log_count(&score_log, slot_of(player_index), AE_CAMPAIGN_SCORE_ENTRY_DEATH);
}

long ae_campaign_score_team_points(void)
{
	update();
	return ae_campaign_score_log_team_total(&score_log);
}

int ae_campaign_score_is_competitive(void)
{
	return type_competitive();
}

long ae_campaign_score_medal_count(long player_index, int medal)
{
	int slot = player_index == NONE ? -1 : slot_of(player_index);
	long total = 0, index;

	for (index = 0; index < score_log.count; index++)
	{
		if ((slot < 0 || score_log.entries[index].player_slot == slot) && score_log.entries[index].kind == AE_CAMPAIGN_SCORE_ENTRY_MEDAL &&
			score_log.entries[index].variant_index == medal)
			total++;
	}
	return total;
}

long ae_campaign_score_level_ticks(void)
{
	if ((setting_on() || firefight_active()) && campaign_level())
		update();
	return score_log.ticks;
}

void ae_campaign_score_time_text(wchar_t *text, long count)
{
	long seconds = ae_campaign_score_level_ticks() / AE_CAMPAIGN_SCORE_TICKS_PER_SECOND;
	char narrow[24];
	long index;

	snprintf(narrow, sizeof(narrow), "%ld:%02ld:%02ld", seconds / 3600, (seconds / 60) % 60, seconds % 60);
	for (index = 0; index < count - 1 && narrow[index]; index++)
		text[index] = (wchar_t)narrow[index];
	if (count > 0)
		text[index] = 0;
}

long ae_campaign_score_par_seconds(void)
{
	return ae_campaign_score_par_seconds_of_level(&campaign_score_values, global_scenario_index != NONE ? tag_get_name(global_scenario_index) : NULL,
		difficulty());
}

float ae_campaign_score_par_fraction(void)
{
	return ae_campaign_score_par_fraction_of(ae_campaign_score_par_seconds(), ae_campaign_score_level_ticks());
}

float ae_campaign_score_time_multiplier(void)
{
	return ae_campaign_score_time_multiplier_of(&campaign_score_values, ae_campaign_score_par_seconds(), ae_campaign_score_level_ticks());
}

float ae_campaign_score_difficulty_multiplier(void)
{
	return campaign_score_values.difficulty[difficulty()];
}

float ae_campaign_score_total_multiplier(void)
{
	return ae_campaign_score_difficulty_multiplier() * ae_campaign_score_skulls_multiplier() * ae_campaign_score_time_multiplier();
}

void ae_campaign_score_multiplier_text(float multiplier, wchar_t *text, long count)
{
	char narrow[16];
	long index;

	snprintf(narrow, sizeof(narrow), multiplier == (float)(long)multiplier ? "x%.0f" : "x%.2f", (double)multiplier);
	/* (x0.50 as x0.5) */
	index = (long)strlen(narrow);
	while (strchr(narrow, '.') && index > 0 && narrow[index - 1] == '0')
		narrow[--index] = 0;
	for (index = 0; index < count - 1 && narrow[index]; index++)
		text[index] = (wchar_t)narrow[index];
	if (count > 0)
		text[index] = 0;
}

long ae_campaign_score_player_final(long player_index)
{
	return ae_campaign_score_final(ae_campaign_score_player_points(player_index), ae_campaign_score_time_multiplier());
}

long ae_campaign_score_recent_medals(long player_index, short *medals_out, long count)
{
	int slot = slot_of(player_index);
	long shown = 0, index;

	if (slot < 0)
		return 0;
	for (index = score_log.count - 1; index >= 0 && shown < count; index--)
	{
		if (score_log.entries[index].kind == AE_CAMPAIGN_SCORE_ENTRY_MEDAL && score_log.entries[index].player_slot == slot)
			medals_out[shown++] = (short)score_log.entries[index].variant_index;
	}
	/* (newest last) */
	for (index = 0; index < shown / 2; index++)
	{
		short swap = medals_out[index];

		medals_out[index] = medals_out[shown - 1 - index];
		medals_out[shown - 1 - index] = swap;
	}
	return shown;
}

int ae_campaign_score_player_place(long player_index)
{
	return ae_campaign_score_place(&score_log, slot_of(player_index));
}

char const *ae_campaign_score_difficulty_name(void)
{
	return difficulty_names[difficulty()];
}

long ae_campaign_score_par_ticks(void)
{
	return ae_campaign_score_par_ticks_of(ae_campaign_score_par_seconds());
}

int ae_campaign_score_next_drop(long *drop_tick, float *next_multiplier)
{
	return ae_campaign_score_next_drop_of(&campaign_score_values, ae_campaign_score_par_seconds(), ae_campaign_score_level_ticks(),
		drop_tick, next_multiplier);
}

long ae_campaign_score_event_count(void)
{
	return score_log.count;
}

int ae_campaign_score_event_get(long index, struct ae_campaign_score_event *event)
{
	struct ae_campaign_score_entry const *entry;
	char const *name, *last;
	long at;

	if (index < 0 || index >= score_log.count)
		return 0;
	entry = &score_log.entries[index];
	event->tick = entry->game_time;
	event->player_slot = entry->player_slot;
	event->kind = entry->kind;
	event->style = entry->style;
	event->points = entry->points;
	event->variant = entry->variant_index;
	event->name[0] = 0;
	if (entry->kind == AE_CAMPAIGN_SCORE_ENTRY_MEDAL)
	{
		if (entry->variant_index >= 0 && entry->variant_index < AE_CAMPAIGN_MEDAL_COUNT)
			snprintf(event->name, sizeof(event->name), "%s", ae_campaign_medal_descriptions[entry->variant_index].label);
	}
	else if (entry->variant_index != NONE && entry->variant_index >= 0)
	{
		name = tag_get_name(entry->variant_index);
		last = name ? name : "";
		for (at = 0; name && name[at]; at++)
		{
			if (name[at] == '\\' || name[at] == '/')
				last = name + at + 1;
		}
		snprintf(event->name, sizeof(event->name), "%s", last);
	}
	return 1;
}

int ae_campaign_score_summary_get(long player_index, struct ae_campaign_score_summary *summary)
{
	int slot = player_index == NONE ? -1 : slot_of(player_index);

	if (player_index != NONE && slot < 0)
		return 0;
	ae_campaign_score_summarize(&score_log, slot, summary);
	return 1;
}

void ae_campaign_score_level_name(char *text, long size)
{
	char const *name = global_scenario_index != NONE ? tag_get_name(global_scenario_index) : NULL;
	char const *last = name ? name : "";
	long index;

	for (index = 0; name && name[index]; index++)
	{
		if (name[index] == '\\' || name[index] == '/')
			last = name + index + 1;
	}
	snprintf(text, (size_t)size, "%s", last);
}

/* ---------- the score file beside the saved game */

/* a file put in another's place in one step, the other replaced (xbox_files.c) */
int platform_replace_file(char const *path, char const *new_path);

static int score_file_path(char *path, long size)
{
	char directory[512];
	char const *file = ae_saved_game_file();

	if (!file || !player_ui_get_path_to_local_player_profile_directory(0, directory))
		return 0;
	snprintf(path, (size_t)size, "%s%s.score", directory, file);
	return 1;
}

void ae_campaign_score_saved_game_written(unsigned long checksum)
{
	char path[640], temporary[660];
	long size;
	HANDLE file;
	DWORD written = 0;
	int good = 0;

	if (!ae_campaign_score_active() || !score_file_path(path, sizeof(path)))
		return;
	update();
	size = ae_campaign_score_file_encode(&score_log, checksum, ae_campaign_score_name_hash(tag_text(global_scenario_index)),
		type_competitive(), file_bytes, (long)sizeof(file_bytes));
	/* (written whole to a file beside it and closed, then put in the score file's place: a crash or a short write
	leaves the score file there as it was) */
	snprintf(temporary, sizeof(temporary), "%s.new", path);
	file = size ? CreateFileA(temporary, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL) : INVALID_HANDLE_VALUE;
	if (file != INVALID_HANDLE_VALUE)
	{
		good = WriteFile(file, file_bytes, (DWORD)size, &written, NULL) && written == (DWORD)size;
		CloseHandle(file);
		if (good)
			good = platform_replace_file(temporary, path);
		if (!good)
			DeleteFileA(temporary);
	}
	if (!good)
		platform_log("score: the score file could not be written");
	else
		platform_log("score: saved %ld entries (%ld bytes) beside the saved game", score_log.count, size);
}

static unsigned long header_checksum_read;

void ae_campaign_score_saved_game_header(unsigned long checksum)
{
	header_checksum_read = checksum;
}

void ae_campaign_score_saved_game_taken(void)
{
	unsigned long checksum = header_checksum_read;
	char path[640];
	HANDLE file;
	DWORD size, read = 0;
	char const *why = "";
	int each = 0;

	/* (every saved game the engine takes, whether the score is on or not) */
	ae_test_start_note_loaded();
	if (!ae_campaign_score_active() || !score_file_path(path, sizeof(path)))
		return;
	ae_campaign_score_log_reset(&score_log);
	file = CreateFileA(path, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (file == INVALID_HANDLE_VALUE)
	{
		platform_log("score: the saved game has no score file: the score starts at 0");
		return;
	}
	size = GetFileSize(file, NULL);
	if (size == INVALID_FILE_SIZE || size > (DWORD)AE_CAMPAIGN_SCORE_FILE_MAXIMUM_SIZE)
		why = "it is bigger than a score file can be";
	else if (!ReadFile(file, file_bytes, size, &read, NULL) || read != size)
		why = "it could not be read";
	CloseHandle(file);
	if (!*why && ae_campaign_score_file_decode(file_bytes, (long)size, checksum,
		ae_campaign_score_name_hash(tag_text(global_scenario_index)), &score_log, &each, &why))
	{
		platform_log("score: the score file is not taken (%s): the score starts at 0", why);
		return;
	}
	if (*why)
	{
		ae_campaign_score_log_reset(&score_log);
		platform_log("score: the score file is not taken (%s): the score starts at 0", why);
		return;
	}
	/* (the score type the game was saved with goes on) */
	resumed_type = each ? 1 : 0;
	/* (the level's time goes on from the saved game's, the checkpoint's) */
	last_time = game_time_get();
	hurt_reset();
	platform_log("score: taken %ld entries from the score file: %ld points, time %ld s", score_log.count,
		ae_campaign_score_log_team_total(&score_log), score_log.ticks / AE_CAMPAIGN_SCORE_TICKS_PER_SECOND);
}

/* ---------- DEV MODE */

static int enemy_actor_type(char const *word, short *actor_type)
{
	static struct { char const *name; short type; } const types[] =
	{
		{ "elite", _actor_elite }, { "jackal", _actor_jackal }, { "grunt", _actor_grunt }, { "hunter", _actor_hunter },
		{ "engineer", _actor_engineer }, { "assassin", _actor_assassin }, { "marine", _actor_marine }, { "crew", _actor_crew },
		{ "combat_form", _actor_combat_form }, { "infection_form", _actor_infection_form }, { "carrier_form", _actor_carrier_form },
		{ "sentinel", _actor_sentinel },
	};
	size_t index;

	for (index = 0; index < sizeof(types) / sizeof(types[0]); index++)
	{
		if (!strcmp(word, types[index].name))
		{
			*actor_type = types[index].type;
			return 1;
		}
	}
	return 0;
}

/* a style word: regular, or any of one_shot, headshot, melee, backsmack joined by '+' */
static int style_of_word(char const *word, unsigned char *style)
{
	char part[16];
	long length = 0;

	*style = 0;
	for (;; word++)
	{
		if (*word == '+' || !*word)
		{
			int bit, found = 0;

			part[length] = 0;
			if (strcmp(part, "regular"))
			{
				for (bit = 0; bit < AE_CAMPAIGN_SCORE_STYLE_COUNT; bit++)
				{
					if (!strcmp(part, style_words[bit]))
					{
						*style |= (unsigned char)(1 << bit);
						found = 1;
					}
				}
				if (!found)
					return 0;
			}
			length = 0;
			if (!*word)
				return 1;
		}
		else if (length < (long)sizeof(part) - 1)
			part[length++] = *word;
		else
			return 0;
	}
}

int ae_campaign_score_dev(char const *verb, char const *enemy, char const *style_word, char *text, long size)
{
	long local_player = local_player_get_player_index(local_player_get_next(NONE));

	if (!strcmp(verb, "on") || !strcmp(verb, "off"))
	{
		config_set("game.campaign_score", !strcmp(verb, "on") ? "true" : "false");
		snprintf(text, (size_t)size, "campaign_score: %s", verb);
		return 1;
	}
	if (!strcmp(verb, "status"))
	{
		wchar_t clock[24];
		char name[64], clock_narrow[24];
		int index;

		if (!ae_campaign_score_active())
		{
			snprintf(text, (size_t)size, "campaign_score: %s, %s", setting_on() ? "on, no campaign level on this machine" : "off",
				"nothing is scored");
			return 1;
		}
		ae_campaign_score_time_text(clock, 24);
		for (index = 0; index < 23 && clock[index]; index++)
			clock_narrow[index] = (char)clock[index];
		clock_narrow[index] = 0;
		ae_campaign_score_level_name(name, sizeof(name));
		snprintf(text, (size_t)size, "campaign_score: on, %s, %s, %ld points (%s), %ld kills, %ld deaths, %ld entries, time %s, par %ld s, time x%.2f",
			name, difficulty_names[difficulty()], local_player != NONE ? ae_campaign_score_player_points(local_player) : 0L,
			type_competitive() ? "own" : "shared", local_player != NONE ? ae_campaign_score_player_kills(local_player) : 0L,
			local_player != NONE ? ae_campaign_score_player_deaths(local_player) : 0L, score_log.count, clock_narrow,
			ae_campaign_score_par_seconds(), (double)ae_campaign_score_time_multiplier());
		return 1;
	}
	if (!strcmp(verb, "reset"))
	{
		if (!ae_campaign_score_active())
		{
			snprintf(text, (size_t)size, "campaign_score: reset needs a campaign level with campaign_score on");
			return 0;
		}
		ae_campaign_score_new_map();
		last_time = game_time_get();
		snprintf(text, (size_t)size, "campaign_score: reset");
		return 1;
	}
	if (!strcmp(verb, "add"))
	{
		short actor_type;
		unsigned char style;
		long base, points;
		char lower[4] = "";
		int slot = slot_of(local_player);

		if (!ae_campaign_score_active() || slot < 0)
		{
			snprintf(text, (size_t)size, "campaign_score: add needs a campaign level with campaign_score on");
			return 0;
		}
		if (!enemy || !style_word || !enemy_actor_type(enemy, &actor_type) || !style_of_word(style_word, &style))
		{
			snprintf(text, (size_t)size, "campaign_score: add <elite|jackal|grunt|hunter|engineer|assassin|combat_form|infection_form|carrier_form|sentinel|marine|crew> <regular|one_shot|headshot|melee|backsmack, joined with +>");
			return 0;
		}
		update();
		base = ae_campaign_score_actor_points(campaign_score_values.enemies, campaign_score_values.enemy_count, actor_type, lower);
		if (!base)
			base = ae_campaign_score_actor_points(campaign_score_values.allies, campaign_score_values.ally_count, actor_type, lower);
		if (base < 0)
		{
			points = base;
			append(AE_CAMPAIGN_SCORE_ENTRY_ALLY, NONE, slot, points, 0);
		}
		else
		{
			points = ae_campaign_score_kill_points(&campaign_score_values, base, style, difficulty(), ae_campaign_score_skulls_multiplier());
			append(AE_CAMPAIGN_SCORE_ENTRY_KILL, NONE, slot, points, style);
			/* (and the medals it wins, as a real kill's: no killer unit, no weapon) */
			kill_medals(local_player, NONE, NONE, NONE, style);
		}
		snprintf(text, (size_t)size, "campaign_score: added %s %s: %+ld points, total %ld", enemy, style_word, points,
			ae_campaign_score_log_points(&score_log, slot, type_competitive()));
		return 1;
	}
	snprintf(text, (size_t)size, "campaign_score: status | on | off | reset | add <enemy> <style>");
	return 0;
}
