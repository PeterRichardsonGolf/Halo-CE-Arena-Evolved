/*
AE_COMPANION.C

See ae_companion.h.
*/

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cseries.h"
#include "cache/cache_files.h"
#include "ai/ai.h"
#include "ai/actor_definitions.h"
#include "ai/actors.h"
#include "ai/ai_globals.h"
#include "ai/ai_scenario_definitions.h"
#include "ai/encounters.h"
#include "ai/ai_script.h"
#include "game/game.h"
#include "game/game_allegiance.h"
#include "game/players.h"
#include "game/game_engine.h"
#include "game/player_queues_new.h"
#include "memory/data.h"
#include "networking/network_game_globals.h"
#include "main/main.h"
#include "objects/damage.h"
#include "objects/objects.h"
#include "physics/collisions.h"
#include "scenario/scenario.h"
#include "scenario/scenario_definitions.h"
#include "tag_files/tag_files.h"
#include "networking/network_client_manager.h"
#include "networking/network_game_manager.h"
#include "networking/network_messages.h"
#include "networking/network_server_message_handler.h"
#include "networking/network_server_manager.h"
#include "networking/network_server_manager_internal.h"
#include "cutscene/cinematics.h"
#include "units/bipeds.h"
#include "units/unit_definitions.h"
#include "../src/port_config.h"
#include "bots.h"
#include "units/units.h"
#include "items/weapons.h"
#include "ae_companion.h"
#include "ae_bots_squad_rules.h"
#include "ae_fireteam.h"
#include "ae_campaign_weapons.h"
#include "ae_companion_rules.h"

void platform_log(char const *format, ...);
boolean main_menu_is_active(void);
int halo_server_mode(void);
static boolean campaign_host(struct scenario const *scenario, boolean need_ai);
char const *config_string(char const *name);

/* (ai.c's actor_default_state values: none, asleep, alert) */
enum { companion_state_none = 0, companion_state_alert = 2 };

/* the leash, in world units */
#define NEAR_DISTANCE 6.0f
#define FAR_DISTANCE 8.0f
#define SETTLE_DISTANCE 3.5f
#define SETTLE_STEP 0.8f
#define FIGHT_DISTANCE 12.0f
/* a destination kept while the player has moved less than this from it */
#define RETARGET_DISTANCE 1.5f
#define SAMPLE_TICKS 30
/* standing closer than this to another companion, or to the player, a companion steps away: how far, and for how long at most */
#define CROWD_DISTANCE 1.3f
#define CROWD_PLAYER_DISTANCE 1.8f
#define SPREAD_STEP 2.5f
#define SPREAD_TICKS 90
/* a companion this close to a vehicle a player drives takes a seat in it */
#define BOARD_DISTANCE 25.0f
/* a member waiting to enter while the human sits in a vehicle with control spawns after this long (5 s), and boards */
#define SEATED_WAIT_TICKS 150
/* a member put out of its seat by anything but the squad's own rule waits this long to take one again (3 s) */
#define BOARD_RETRY_TICKS 90
/* the level's first moments (its intro) pass before the companions come */
#define AUTO_START_TICKS 90
#define AUTO_RETRY_TICKS 60
#define AUTO_RETRY_BOT_TICKS 10
#define VARIANTS 8
#define ENEMIES 256

/* a companion is the player of a bot machine (bots.h): bot number n is companion n */
static struct
{
	boolean active;
	boolean wanted[AE_COMPANION_MOST];
	boolean attached[AE_COMPANION_MOST];
	boolean catching[AE_COMPANION_MOST];
	real_point3d goal[AE_COMPANION_MOST];
	real_point3d spread_goal[AE_COMPANION_MOST];
	long spread_until[AE_COMPANION_MOST];
	boolean has_goal[AE_COMPANION_MOST];
	long variant[AE_COMPANION_MOST];
	long candidates[VARIANTS];
	long candidate_count;
	long wanted_count;
	long next_auto;
	long seated;
	long started;
	long next_sample;
	long enemy_seen[ENEMIES];
	long enemy_seen_count;
	long enemies_gone;
	long moves_ordered;
	long adds_asked;
	long actors_made;
	long fire_ticks;
	long respawns;
	long spread_orders_made;
	long spread_no_surface;
	long spread_refused;
	boolean had_unit[AE_COMPANION_MOST];
	/* the tick a player who left is gone from the game at (its quit is sent ahead): it joins again only after it */
	long leaving_until[AE_COMPANION_MOST];
	boolean join_waits[AE_COMPANION_MOST];
	/* DEV MODE reset: until this tick a body far from the players is put beside the nearest */
	long place_until;
	long placed;
	/* ENTRY: the body last put behind the player (a new one, the level's first or a respawn, is put there once), and the
	ticks a body has waited for clear ground there */
	boolean entered[AE_COMPANION_MOST];
	long entered_unit[AE_COMPANION_MOST];
	long entry_tries[AE_COMPANION_MOST];
	boolean spawn_wait_logged[AE_COMPANION_MOST];
	/* the level (its scenario tag) the squad was asked for or last started in: another is a new level */
	long level_scenario;
	/* the squad's log (always, with a squad): each body's last living state, for its death's line; the catch-up's watch
	(the nearest it came to the player, and when); the teleports; the next position line */
	boolean was_alive[AE_COMPANION_MOST];
	long last_unit[AE_COMPANION_MOST];
	real_point3d last_at[AE_COMPANION_MOST];
	real best_distance[AE_COMPANION_MOST];
	long gain_tick[AE_COMPANION_MOST];
	long teleports;
	/* STUCK INDOORS (ae_companion_stuck_warp): each member's window (its place, its distance and the tick at its start), the
	warps */
	real_point3d stuck_at[AE_COMPANION_MOST];
	real stuck_distance[AE_COMPANION_MOST];
	long stuck_tick[AE_COMPANION_MOST];
	/* the window whose missing spot was logged */
	long stuck_said[AE_COMPANION_MOST];
	/* no spot or teleport at the last try: the next try not before */
	long stuck_retry[AE_COMPANION_MOST];
	long indoor_warps;
	long next_positions;
	/* boarding: the seat a member last took (its vehicle, NONE none) and when; a member put out of it by something else waits
	until board_after to take a seat again */
	long boarded_vehicle[AE_COMPANION_MOST];
	short boarded_seat[AE_COMPANION_MOST];
	long boarded_tick[AE_COMPANION_MOST];
	long board_after[AE_COMPANION_MOST];
	/* MOUNT: the vehicle a member was sent to man (NONE: none) and when */
	long mount_vehicle[AE_COMPANION_MOST];
	long mount_tick[AE_COMPANION_MOST];
	/* the tick the human was first seen seated with control while a member waited to enter (0: not) */
	long seated_wait_since;
	/* THE PLAYER'S TRACK (ae_companion_rules.h): the first human player's walked spots, oldest first */
	float track[AE_COMPANION_TRACK_MOST][3];
	int track_count;
	/* each crumb's cues (ae_companion_track_push), and whether the human was in the air since the last crumb */
	unsigned char track_flags[AE_COMPANION_TRACK_MOST];
	boolean track_air;
} companion;

/* the companions' state cleared: zero, the vehicles a member was sent to none */
static void companion_clear(void)
{
	long slot;

	memset(&companion, 0, sizeof companion);
	for (slot = 0; slot < AE_COMPANION_MOST; slot++)
		companion.mount_vehicle[slot] = NONE;
}

/* the last reason the setting's companions were not asked for, logged once */
static char last_refusal_text[96];

/* DEV MODE (campaign <n> won <seconds>): the level is won this many ticks into it, -1 none */
static long won_after_ticks = -1;
/* the squad's periodic lines (its status each second, its positions every 2 s): every one with debug.bot_events on, else one every
LOG_QUIET_TICKS (30 s), the first at once; the tick each was last written */
#define LOG_QUIET_TICKS 900
static long status_logged = -1, positions_logged = -1;

static boolean periodic_line(long now, long *last)
{
	if (config_boolean("debug.bot_events"))
		return TRUE;
	if (*last >= 0 && now >= *last && now - *last < LOG_QUIET_TICKS)
		return FALSE;
	*last = now;
	return TRUE;
}
/* the level and tick the DEV MODE end was armed in: another level, or the same one started again, drops it */
static long won_scenario = NONE, won_armed_tick = -1;
static long won_at_tick = -1;
static void won_service(long now);

/* a seat's flag: a rider may hold weapons there (the tag's allows weapons) */
#define SEAT_ALLOWS_WEAPONS_BIT 5

/* a reset's time to put the bodies beside the players (2 s), and how near counts as beside them */
#define PLACE_TICKS 60
#define PLACE_NEAR 6.0f
/* the ticks a new body waits for clear ground behind the player before it is left where it spawned (2 s) */
#define ENTRY_TRIES 60
/* the squad's position line, every 2 s */
#define POSITIONS_TICKS 60

/* the DEV MODE count (companion <n>, off), -1 when none is set: the game.companions setting then says */
static long count_override = -1;
/* the count the next level started from the menus asks for (a TEST RUN's campaign test, before its load), -1 none */
static long count_next_level = -1;
/* the menu's AI SQUAD when count_next_level was given: a change of it since makes the menu's count the one */
static long count_next_level_setting = -1;

void get_unique_random_color(struct network_game_server *server, struct network_player *player);

static char const *last_name(char const *name)
{
	char const *last = name ? strrchr(name, '\\') : NULL;

	return last ? last + 1 : name ? name : "";
}

static boolean has_text(char const *text, char const *word)
{
	size_t length = strlen(word), at, index;

	for (at = 0; text[at]; at++)
	{
		for (index = 0; index < length && text[at + index] &&
			tolower((unsigned char)text[at + index]) == tolower((unsigned char)word[index]); index++)
			;
		if (index == length)
			return TRUE;
	}
	return FALSE;
}

/* how well an actor variant's name suits a companion: 0 not at all, higher is better. A plain marine is best;
leaders, wounded and scripted ones are worse. */
static int variant_score(char const *name)
{
	char const *last = last_name(name);

	/* (a level with no marines of its own: its plain elites, the nearest to a soldier of the level's AI that can fight beside the
	players) */
	if (!has_text(name, "marine"))
	{
		if (!has_text(name, "elite") || has_text(name, "flood") || has_text(name, "ultra") || has_text(name, "zealot") ||
			has_text(name, "stealth") || has_text(name, "honor"))
			return 0;
		/* (plain elites first; the spec ops, majors and commanders when a level has no others) */
		return has_text(name, "specops") || has_text(name, "major") || has_text(name, "commander") ? 1 : 2;
	}
	if (has_text(name, "dead") || has_text(name, "wound") || has_text(name, "cinematic") || has_text(name, "cutscene") ||
		has_text(name, "pilot") || has_text(name, "flee") || has_text(name, "cower"))
		return 0;
	if (!strcmp(last, "marine"))
		return 5;
	if (has_text(last, "armored"))
		return 4;
	if (has_text(last, "sergeant") || has_text(last, "major"))
		return 2;
	return 3;
}

/* the actor variants of the level that suit a companion, best first: from the scenario's own palette (what the level places) and
else from any actor variant in its tags. Only the best kind (marines; else plain elites) and the ones near its score, so each
companion can hold another of the level's soldiers' weapons. Returns how many (at most max) */
static long variants_choose(struct scenario *scenario, long *out, long max)
{
	long index, count = 0, best_score = 0;
	long found[64], scores[64], found_count = 0, pass, each, other;
	struct tag_iterator iterator;
	long tag_index;

	for (pass = 0; pass < 2 && !found_count; pass++)
	{
		if (!pass)
		{
			for (index = 0; index < scenario->ai_actor_palette.count && found_count < 64; index++)
			{
				struct tag_reference *entry = TAG_BLOCK_GET_ELEMENT(&scenario->ai_actor_palette, index, struct tag_reference);

				if (entry->index != NONE && tag_get_group_tag(entry->index) == ACTOR_VARIANT_DEFINITION_TAG &&
					variant_score(tag_get_name(entry->index)) > 0)
				{
					found[found_count] = entry->index;
					scores[found_count++] = variant_score(tag_get_name(entry->index));
				}
			}
		}
		else
		{
			tag_iterator_new(&iterator, ACTOR_VARIANT_DEFINITION_TAG);
			while ((tag_index = tag_iterator_next(&iterator)) != NONE && found_count < 64)
			{
				if (variant_score(tag_get_name(tag_index)) > 0)
				{
					found[found_count] = tag_index;
					scores[found_count++] = variant_score(tag_get_name(tag_index));
				}
			}
		}
	}
	for (each = 0; each < found_count; each++)
		if (scores[each] > best_score)
			best_score = scores[each];
	/* (best score first, equal scores by name, so a game's order is the same each time) */
	for (each = 0; each < found_count; each++)
		for (other = each + 1; other < found_count; other++)
			if (scores[other] > scores[each] ||
				(scores[other] == scores[each] && strcmp(tag_get_name(found[other]), tag_get_name(found[each])) < 0))
			{
				long swap = found[each];

				found[each] = found[other];
				found[other] = swap;
				swap = scores[each];
				scores[each] = scores[other];
				scores[other] = swap;
			}
	for (each = 0; each < found_count && count < max; each++)
	{
		boolean again = FALSE;

		if (scores[each] < best_score - 2 || scores[each] < 1)
			continue;
		for (other = 0; other < count; other++)
			if (out[other] == found[each])
				again = TRUE;
		if (!again)
			out[count++] = found[each];
	}
	return count;
}

/* the variant's weapon in the unit's hands (the variant's other changes, its vitality and colours, are not made: the body is
the player's) */
static void give_variant_weapon(long variant, long unit_index)
{
	struct actor_variant_definition *definition = actor_variant_definition_get(variant);
	struct object_placement_data placement;
	long weapon_index;

	if (definition->ranged_combat.reference.index == NONE)
		return;
	object_placement_data_new(&placement, definition->ranged_combat.reference.index, unit_index);
	weapon_index = object_new(&placement);
	if (weapon_index != NONE && !unit_add_weapon_to_inventory(unit_index, weapon_index, _unit_add_weapon_replace))
		object_delete(weapon_index);
}

/* the player's body is in a vehicle's seat: that vehicle, else NONE */
static long vehicle_of(long unit_index)
{
	long parent = object_get(unit_index)->object.parent_object_index;

	if (parent != NONE && object_try_and_get_and_verify_type(parent, _object_mask_vehicle))
		return parent;
	return NONE;
}

/* the nearest living human player's unit to a point, or NONE */
static long nearest_player_unit(real_point3d const *from, real_point3d *at)
{
	struct data_iterator iterator;
	struct player_datum *player;
	long best = NONE;
	real best_distance = 0.0f;

	data_iterator_new(&iterator, player_data);
	while ((player = (struct player_datum *)data_iterator_next(&iterator)) != NULL)
	{
		struct unit_datum *unit;
		real_point3d position;
		real distance;

		if (player->unit_index == NONE || bots_machine_is_bot(player->network_player_data.machine_index))
			continue;
		unit = unit_get(player->unit_index);
		if (TEST_FLAG(unit->object.damage_flags, _object_dead_bit))
			continue;
		object_get_origin(player->unit_index, &position);
		distance = distance_squared3d(from, &position);
		if (best == NONE || distance < best_distance)
		{
			best = player->unit_index;
			best_distance = distance;
			*at = position;
		}
	}
	return best;
}

/* the companion number of a unit that is a companion player's, or NONE */
static long slot_of_unit(long unit_index)
{
	struct unit_datum *unit;
	struct player_datum *player;
	long slot;

	if (!companion.active || unit_index == NONE)
		return NONE;
	unit = unit_get(unit_index);
	if (unit->unit.player_index == NONE)
		return NONE;
	player = player_get(unit->unit.player_index);
	if (player->local_player_index != NONE || !bots_machine_is_bot(player->network_player_data.machine_index))
		return NONE;
	slot = player->network_player_data.machine_index - BOTS_FIRST_MACHINE;
	return slot >= 0 && slot < AE_COMPANION_MOST && companion.wanted[slot] ? slot : NONE;
}

int ae_companion_loadout(long unit_index, int reset_equipment)
{
	long slot = slot_of_unit(unit_index);

	return slot != NONE ? ae_campaign_weapons_give_squad(unit_index, (int)slot, reset_equipment) : 0;
}

int ae_companion_is_driven(long unit_index)
{
	return slot_of_unit(unit_index) != NONE && unit_get(unit_index)->unit.actor_index != NONE;
}

/* whether the MP bots' brain drives the companions' bodies (game.companions_brain bot, the default) and not the level's actor AI
(actor) */
static boolean brain_is_bot(void)
{
	return (boolean)strcmp(config_string("game.companions_brain"), "actor") != 0;
}

/* the level's living AI enemies of the players' team: their units, at most max; the list is made once a tick */
long ae_companion_foes(long *units, long max)
{
	static long cached[ENEMIES];
	static long cached_count = 0, cached_tick = -1;
	struct scenario *scenario = global_scenario_try_and_get();
	long now = game_time_get(), count, encounter;

	if (cached_tick != now && scenario && ai_globals && ai_globals->ai_initialized_for_map)
	{
		struct ai_script_actor_reference_iterator iterator;
		struct actor_datum *actor;

		cached_tick = now;
		cached_count = 0;
		for (encounter = 0; encounter < scenario->ai_encounters.count; encounter++)
		{
			ai_index_actor_iterator_new(encounter, &iterator);
			while ((actor = ai_index_actor_iterator_next(&iterator)) != NULL)
			{
				long unit_index = actor->meta.unit_index;

				if (unit_index == NONE || cached_count >= ENEMIES || !game_team_is_enemy(_game_team_player, actor->meta.team_index))
					continue;
				if (TEST_FLAG(unit_get(unit_index)->object.damage_flags, _object_dead_bit))
					continue;
				cached[cached_count++] = unit_index;
			}
		}
	}
	else if (cached_tick != now)
	{
		cached_tick = now;
		cached_count = 0;
	}
	count = cached_count < max ? cached_count : max;
	memcpy(units, cached, (size_t)count * sizeof(long));
	return count;
}

long ae_companion_leader(real_point3d const *from, real_point3d *at)
{
	return nearest_player_unit(from, at);
}

int ae_companion_action(long absolute_player_index, struct player_action *action)
{
	struct data_iterator iterator;
	struct player_datum *player;

	if (!companion.active || !brain_is_bot() || !campaign_host(global_scenario_try_and_get(), FALSE))
		return 0;
	data_iterator_new(&iterator, player_data);
	while ((player = (struct player_datum *)data_iterator_next(&iterator)) != NULL)
	{
		long slot;

		if (DATUM_INDEX_TO_ABSOLUTE_INDEX(iterator.datum_index) != absolute_player_index)
			continue;
		if (player->local_player_index != NONE || !bots_machine_is_bot(player->network_player_data.machine_index))
			return 0;
		slot = player->network_player_data.machine_index - BOTS_FIRST_MACHINE;
		if (slot < 0 || slot >= AE_COMPANION_MOST || !companion.wanted[slot])
			return 0;
		bots_squad_drive(slot, iterator.datum_index, action);
		return 1;
	}
	return 0;
}

/* how many companions the game is to have: DEV MODE's count, else the setting's */
static long wanted_total(void)
{
	return ae_companion_wanted_count(count_next_level, count_next_level_setting, count_override, config_integer("game.companions"),
		AE_COMPANION_MOST);
}

/* the level's first living human player's body, NONE when none or the level's intro plays */
static long first_human_unit(void)
{
	real_point3d here, at;

	here.x = here.y = here.z = 0.0f;
	(void)here;
	{
		struct data_iterator iterator;
		struct player_datum *player;

		data_iterator_new(&iterator, player_data);
		while ((player = (struct player_datum *)data_iterator_next(&iterator)) != NULL)
		{
			if (player->unit_index == NONE || bots_machine_is_bot(player->network_player_data.machine_index))
				continue;
			if (!TEST_FLAG(unit_get(player->unit_index)->object.damage_flags, _object_dead_bit))
			{
				object_get_origin(player->unit_index, &at);
				return player->unit_index;
			}
		}
	}
	return NONE;
}

/* a campaign game this machine hosts (ae_companion_may_ask): the one place a squad may be. need_ai: the level's AI running too
(an ask); a squad in the game already is kept through a level's load */
static boolean campaign_host(struct scenario const *scenario, boolean need_ai)
{
	return scenario && ae_companion_may_ask(scenario->type == _scenario_type_solo,
		!need_ai || (ai_globals && ai_globals->ai_initialized_for_map),
		game_engine_running(), main_menu_is_active(), network_game_distributed_client(), global_network_game_server_get() != NULL,
		halo_server_mode());
}

/* asks for count companions: they join on the server's next turn. A level of the game's own AI is needed, and a machine that
runs it */
static int ask(int count, char *text, unsigned int text_size)
{
	struct scenario *scenario = global_scenario_try_and_get();
	long slot, asked = 0;

	if (!campaign_host(scenario, TRUE))
	{
		snprintf(text, text_size, "no campaign game of this machine's own with AI running");
		return 0;
	}
	if (!companion.active)
	{
		companion_clear();
		companion.started = game_time_get();
		companion.level_scenario = global_scenario_index;
		companion.next_sample = companion.started + SAMPLE_TICKS;
	}
	companion.candidate_count = variants_choose(scenario, companion.candidates, VARIANTS);
	if (!companion.candidate_count && !brain_is_bot())
	{
		snprintf(text, text_size, "this level has no marine actor variant to give a companion its fighting");
		return 0;
	}
	if (count < 1)
		count = 1;
	if (count > AE_COMPANION_MOST)
		count = AE_COMPANION_MOST;
	companion.active = TRUE;
	companion.wanted_count = count;
	for (slot = 0; slot < AE_COMPANION_MOST; slot++)
	{
		if (companion.candidate_count)
			companion.variant[slot] = companion.candidates[slot % companion.candidate_count];
		if (slot < count && !companion.wanted[slot])
		{
			companion.wanted[slot] = TRUE;
			asked++;
		}
		else if (slot >= count && companion.wanted[slot])
		{
			/* (the count asked for is the squad's: one past it leaves) */
			companion.wanted[slot] = FALSE;
			platform_log("companion: %ld is past the %d asked for and leaves", slot + 1, count);
		}
	}
	companion.adds_asked += asked;
	platform_log("companion: %ld asked (%d wanted in all), %s brain, %ld variant(s), the first %s", asked, count,
		brain_is_bot() ? "bot" : "actor", companion.candidate_count,
		companion.candidate_count ? tag_get_name(companion.candidates[0]) : "none");
	snprintf(text, text_size, "%d companion(s) join as co-op players (%s)", count,
		brain_is_bot() ? "bot brain" : last_name(tag_get_name(companion.candidates[0])));
	return count;
}

void ae_companion_next_level(int count)
{
	count_next_level = count < 0 ? -1 : count > AE_COMPANION_MOST ? AE_COMPANION_MOST : count;
	count_next_level_setting = config_integer("game.companions");
	platform_log("companion: the next level starts with %ld", count_next_level);
}

int ae_companion_spawn(int count, char *text, unsigned int text_size)
{
	count_override = count;
	return ask(count, text, text_size);
}

void ae_companion_stop(void)
{
	long slot;

	count_override = 0;
	if (!companion.active)
		return;
	/* (the players leave on the server's next turn) */
	for (slot = 0; slot < AE_COMPANION_MOST; slot++)
		companion.wanted[slot] = FALSE;
	platform_log("companion: sent off");
}

int ae_companion_reset(void)
{
	long slot;
	int players = 0;

	if (!companion.active)
		return -1;
	companion.place_until = game_time_get() + PLACE_TICKS;
	for (slot = 0; slot < AE_COMPANION_MOST; slot++)
	{
		if (!companion.wanted[slot])
			continue;
		players++;
		companion.has_goal[slot] = FALSE;
		bots_squad_reset(slot);
	}
	platform_log("companion: reset, %d player(s) kept, put beside the players for %d ticks", players, PLACE_TICKS);
	/* (the squad's order of before is let go: FOLLOW again) */
	ae_fireteam_campaign_reset();
	return players;
}

/* the floor flags of the entry and the track: the structure and the objects one stands on */
#define FLOOR_FLAGS (FLAG(_collision_test_structure_bit) | FLAG(_collision_test_front_facing_surfaces_bit) | \
	FLAG(_collision_test_objects_bit) | FLAG(_collision_test_objects_vehicles_bit) | FLAG(_collision_test_objects_scenery_bit) | \
	FLAG(_collision_test_objects_machines_bit) | FLAG(_collision_test_objects_controls_bit))

/* whether a unit stands on an object's collision: the biped physics' object under it, or an object first under its feet */
static boolean on_object(long unit_index)
{
	struct biped_datum *biped = biped_try_and_get(unit_index);
	struct collision_result collision;
	real_point3d start;
	real_vector3d down = { 0.0f, 0.0f, -1.2f };

	if (biped && biped->biped.elevator_object_index != NONE)
		return TRUE;
	object_get_origin(unit_index, &start);
	start.z += 0.3f;
	return (boolean)(collision_test_vector(FLOOR_FLAGS, &start, &down, unit_index, &collision) &&
		collision.type == _collision_result_object);
}

/* NARROW GROUND (ae_companion_rules.h): the floor a step out in the eight directions, falling away on two opposite sides; one
answer a unit a tick */
static boolean narrow_ground(long unit_index)
{
	static struct { long unit, tick; boolean narrow; } known[8];
	static int next;
	long now = game_time_get();
	int drops[8], index;
	real_point3d at;

	for (index = 0; index < 8; index++)
		if (known[index].unit == unit_index && known[index].tick == now)
			return known[index].narrow;
	object_get_origin(unit_index, &at);
	for (index = 0; index < 8; index++)
	{
		struct collision_result collision;
		real angle = 0.7853982f * (real)index;
		real_point3d probe = { at.x + AE_COMPANION_NARROW_PROBE * (real)cos(angle), at.y + AE_COMPANION_NARROW_PROBE * (real)sin(angle),
			at.z + 0.5f };
		real_vector3d down = { 0.0f, 0.0f, -(0.5f + AE_COMPANION_NARROW_DROP) };

		drops[index] = !collision_test_vector(FLOOR_FLAGS, &probe, &down, unit_index, &collision);
	}
	known[next].unit = unit_index;
	known[next].tick = now;
	known[next].narrow = (boolean)ae_companion_narrow(drops);
	next = (next + 1) % 8;
	return known[(next + 7) % 8].narrow;
}

/* ON A DECK: a unit on an object's ground or on narrow ground */
static boolean on_deck(long unit_index)
{
	return (boolean)(vehicle_of(unit_index) == NONE && (on_object(unit_index) || narrow_ground(unit_index)));
}

int ae_companion_on_deck(long unit_index)
{
	return unit_index != NONE && on_deck(unit_index);
}

/* THE PLAYER'S TRACK: a crumb each time the first human player on foot and on the ground is AE_COMPANION_TRACK_STEP from the
last (ae_companion_track_push); a jump or a fall in between (more than AE_COMPANION_TRACK_RISE up or down) starts it again, a
jump over a barrier marks the crumb before it, a crouch its own crumb */
static void track_service(void)
{
	long human = first_human_unit();
	struct biped_datum *biped;
	real_point3d at;

	if (human == NONE || vehicle_of(human) != NONE || !(biped = biped_try_and_get(human)))
		return;
	if (biped->biped.support_surface_index == NONE && biped->biped.elevator_object_index == NONE)
	{
		/* (going up in the air: a jump, not a step down or a fall) */
		if (object_get(human)->object.translational_velocity.k > 0.02f)
			companion.track_air = TRUE;
		return;
	}
	object_get_origin(human, &at);
	ae_companion_track_push(companion.track, companion.track_flags, &companion.track_count, at.x, at.y, at.z, companion.track_air,
		biped->biped.crouch > 0.5f);
	/* (on the ground again: a jump that landed away marked its crumb as it was added; one in place marks none) */
	companion.track_air = FALSE;
}

int ae_companion_track_goal_cue(real_point3d const *from, real_point3d *goal, int *cue)
{
	int index = ae_companion_track_step((float const (*)[3])companion.track, companion.track_count, from->x, from->y, from->z);

	if (cue)
		*cue = 0;
	if (index < 0)
		return 0;
	goal->x = companion.track[index][0];
	goal->y = companion.track[index][1];
	goal->z = companion.track[index][2];
	if (cue)
		*cue = ae_companion_track_cue((float const (*)[3])companion.track, companion.track_flags, companion.track_count, index,
			from->x, from->y);
	return 1;
}

int ae_companion_track_goal(real_point3d const *from, real_point3d *goal)
{
	return ae_companion_track_goal_cue(from, goal, NULL);
}

/* whether another living player's body (the human's, another member's; not self_unit) stands within AE_COMPANION_ENTRY_APART of
a spot */
static boolean spot_taken(real_point3d const *spot, long self_unit)
{
	struct data_iterator iterator;
	struct player_datum *player;

	data_iterator_new(&iterator, player_data);
	while ((player = (struct player_datum *)data_iterator_next(&iterator)) != NULL)
	{
		real_point3d at;

		if (player->unit_index == NONE || player->unit_index == self_unit ||
			TEST_FLAG(unit_get(player->unit_index)->object.damage_flags, _object_dead_bit))
			continue;
		object_get_origin(player->unit_index, &at);
		if (distance_squared3d(&at, spot) < AE_COMPANION_ENTRY_APART * AE_COMPANION_ENTRY_APART)
			return TRUE;
	}
	return FALSE;
}

/* ENTRY (ae_companion_rules.h): a spot behind the human player's unit, out of their view, on walkable ground that a clear line
from them reaches (no wall, no vehicle hull, no scenery between), not on another body (self_unit's own aside): TRUE with the
spot (the floor, a little above it) */
static boolean entry_spot(long human, long slot, long self_unit, real_point3d *spot)
{
	unsigned long const sight = FLAG(_collision_test_structure_bit) | FLAG(_collision_test_front_facing_surfaces_bit) |
		FLAG(_collision_test_back_facing_surfaces_bit) | FLAG(_collision_test_objects_bit) |
		FLAG(_collision_test_objects_vehicles_bit) | FLAG(_collision_test_objects_scenery_bit) |
		FLAG(_collision_test_objects_machines_bit);
	unsigned long const floor = FLOOR_FLAGS;
	struct unit_datum *unit = unit_get(human);
	boolean human_on_object = on_object(human), human_on_deck = on_deck(human);
	float offsets[AE_COMPANION_ENTRY_CANDIDATES][2];
	real_point3d feet, chest;
	int count, index;

	object_get_origin(human, &feet);
	chest = feet;
	chest.z += 0.6f;
	/* (on a deck, a bridge's: a spot of the track they walked behind them, else a deck spot along it; nothing off it) */
	if (human_on_deck)
	{
		float deck[AE_COMPANION_DECK_CANDIDATES][2];
		int taken, total = AE_COMPANION_MOST + 2 + AE_COMPANION_DECK_CANDIDATES;

		count = ae_companion_deck_candidates(unit->unit.looking_vector.i, unit->unit.looking_vector.j, deck);
		for (taken = 0; taken < total; taken++)
		{
			struct collision_result collision;
			int crumb = taken < AE_COMPANION_MOST + 2 ?
				ae_companion_track_behind((float const (*)[3])companion.track, companion.track_count, feet.x, feet.y, taken) : -1;
			real_point3d there, eye = chest;
			real_vector3d down = { 0.0f, 0.0f, -(0.6f + AE_COMPANION_DECK_RISE + 0.3f) }, across;

			if (crumb >= 0)
			{
				there.x = companion.track[crumb][0];
				there.y = companion.track[crumb][1];
			}
			else if (taken >= AE_COMPANION_MOST + 2)
			{
				there.x = feet.x + deck[taken - AE_COMPANION_MOST - 2][0];
				there.y = feet.y + deck[taken - AE_COMPANION_MOST - 2][1];
			}
			else
				continue;
			there.z = chest.z;
			vector_from_points3d(&eye, &there, &across);
			if (collision_test_vector(sight, &eye, &across, human, &collision))
				continue;
			if (!collision_test_vector(floor, &there, &down, human, &collision) ||
				fabsf(collision.point.z - feet.z) > AE_COMPANION_DECK_RISE ||
				!ae_companion_entry_floor_kind(human_on_object, collision.type == _collision_result_object))
				continue;
			there = collision.point;
			there.z += 0.05f;
			if (spot_taken(&there, self_unit))
				continue;
			*spot = there;
			return TRUE;
		}
		return FALSE;
	}
	/* (behind where they look: out of their view) */
	count = ae_companion_entry_candidates(unit->unit.looking_vector.i, unit->unit.looking_vector.j, (int)slot, offsets);
	for (index = 0; index < count; index++)
	{
		struct collision_result collision;
		real_point3d there = { feet.x + offsets[index][0], feet.y + offsets[index][1], chest.z }, ground;
		real_vector3d across, down = { 0.0f, 0.0f, -(0.6f + AE_COMPANION_ENTRY_FLOOR_RISE + 0.5f) }, up;
		real headroom = AE_COMPANION_ENTRY_HEADROOM + 1.0f, floor_up;

		vector_from_points3d(&chest, &there, &across);
		if (collision_test_vector(sight, &chest, &across, human, &collision))
			continue;
		if (!collision_test_vector(floor, &there, &down, human, &collision))
			continue;
		ground = collision.point;
		floor_up = collision.plane.n.k;
		if (!ae_companion_entry_floor_kind(human_on_object, collision.type == _collision_result_object))
			continue;
		{
			real_point3d above = { ground.x, ground.y, ground.z + 0.1f };

			up.i = 0.0f;
			up.j = 0.0f;
			up.k = headroom;
			if (collision_test_vector(sight, &above, &up, human, &collision))
				headroom = collision.t * headroom;
		}
		if (!ae_companion_entry_floor(floor_up, ground.z, feet.z, headroom))
			continue;
		ground.z += 0.05f;
		if (spot_taken(&ground, self_unit))
			continue;
		*spot = ground;
		return TRUE;
	}
	return FALSE;
}

/* (a reset's turn) each living body further than PLACE_NEAR from the nearest human player, or in a vehicle, is put behind them:
the reset is followed by the test's own goto of the player, and the squad goes with it */
static void place_service(long now)
{
	static real const side[AE_COMPANION_MOST] = { 1.2f, -1.2f, 1.2f, -1.2f };
	static real const back[AE_COMPANION_MOST] = { 1.6f, 1.6f, 3.0f, 3.0f };
	struct data_iterator iterator;
	struct player_datum *player;

	if (now >= companion.place_until)
		return;
	data_iterator_new(&iterator, player_data);
	while ((player = (struct player_datum *)data_iterator_next(&iterator)) != NULL)
	{
		long slot, human;
		real_point3d at, human_at, spot;
		real_vector3d const *forward;

		if (player->local_player_index != NONE || !bots_machine_is_bot(player->network_player_data.machine_index) ||
			player->unit_index == NONE || TEST_FLAG(unit_get(player->unit_index)->object.damage_flags, _object_dead_bit))
			continue;
		slot = player->network_player_data.machine_index - BOTS_FIRST_MACHINE;
		if (slot < 0 || slot >= AE_COMPANION_MOST || !companion.wanted[slot])
			continue;
		object_get_origin(player->unit_index, &at);
		human = nearest_player_unit(&at, &human_at);
		if (human == NONE || (object_get(player->unit_index)->object.parent_object_index == NONE &&
			distance_squared3d(&at, &human_at) < PLACE_NEAR * PLACE_NEAR))
			continue;
		if (vehicle_of(human) == NONE && entry_spot(human, slot, player->unit_index, &spot))
			;
		else
		{
			forward = &object_get(object_get_ultimate_parent(human))->object.forward;
			spot.x = human_at.x - forward->i * back[slot] - forward->j * side[slot];
			spot.y = human_at.y - forward->j * back[slot] + forward->i * side[slot];
			spot.z = human_at.z + 0.2f;
		}
		if (player_teleport(iterator.datum_index, NONE, &spot))
		{
			companion.placed++;
			companion.teleports++;
			bots_squad_reset(slot);
			/* (a reset lets go of a vehicle it was sent to) */
			companion.mount_vehicle[slot] = NONE;
			platform_log("companion: %ld put beside the player at %.1f %.1f %.1f (from %.1f %.1f %.1f: a reset)", slot + 1, spot.x, spot.y,
				spot.z, at.x, at.y, at.z);
		}
	}
}

/* ENTRY: each new squad body (the level's first, a respawn) not in a seat is put behind the human player once they have
control and are on foot, at entry_spot's ground; none found in ENTRY_TRIES ticks, it stays where it spawned */
static void entry_service(void)
{
	struct data_iterator iterator;
	struct player_datum *player;
	long human = first_human_unit();

	if (ae_companion_intro_holds() || human == NONE || vehicle_of(human) != NONE)
		return;
	data_iterator_new(&iterator, player_data);
	while ((player = (struct player_datum *)data_iterator_next(&iterator)) != NULL)
	{
		long slot;
		real_point3d spot, from;
		boolean placed = FALSE;

		if (player->local_player_index != NONE || !bots_machine_is_bot(player->network_player_data.machine_index) ||
			player->unit_index == NONE || TEST_FLAG(unit_get(player->unit_index)->object.damage_flags, _object_dead_bit))
			continue;
		slot = player->network_player_data.machine_index - BOTS_FIRST_MACHINE;
		if (slot < 0 || slot >= AE_COMPANION_MOST || !companion.wanted[slot] ||
			(companion.entered[slot] && companion.entered_unit[slot] == player->unit_index))
			continue;
		/* (a body already in the level, one that boarded while the human sat: not an entry) */
		if (companion.was_alive[slot] && companion.last_unit[slot] == player->unit_index)
		{
			companion.entered[slot] = TRUE;
			companion.entered_unit[slot] = player->unit_index;
			continue;
		}
		object_get_origin(player->unit_index, &from);
		if (!companion.entry_tries[slot])
			platform_log("companion: %ld %s at %.1f %.1f %.1f", slot + 1, companion.entered[slot] ? "respawns" : "spawns", from.x, from.y,
				from.z);
		/* (one seated for the intro's ride gets out with the player: vehicles_follow) */
		if (vehicle_of(player->unit_index) == NONE)
		{
			if (entry_spot(human, slot, player->unit_index, &spot) && player_teleport(iterator.datum_index, NONE, &spot))
			{
				placed = TRUE;
				companion.teleports++;
				platform_log("companion: %ld enters behind the player at %.1f %.1f %.1f (from %.1f %.1f %.1f)", slot + 1, spot.x, spot.y,
					spot.z, from.x, from.y, from.z);
				bots_squad_reset(slot);
			}
			else if (++companion.entry_tries[slot] < ENTRY_TRIES)
				continue;
			if (!placed)
				platform_log("companion: %ld found no clear ground behind the player, stays where it spawned", slot + 1);
		}
		companion.entered[slot] = TRUE;
		companion.entered_unit[slot] = player->unit_index;
		companion.entry_tries[slot] = 0;
	}
}

/* the cause of a body's death for its line: the last attacker that hurt it (a player's number, or its object's tag), "none" */
static void death_cause(long unit_index, char *text, size_t size)
{
	struct unit_datum *unit = (struct unit_datum *)object_try_and_get(unit_index);
	long index, best = -1;

	snprintf(text, size, "none");
	if (!unit || !TEST_FLAG(_object_mask_unit, unit->object.type))
		return;
	for (index = 0; index < MAXIMUM_ATTACKERS_PER_UNIT; index++)
		if (unit->unit.attackers[index].damage_inflicted > 0.0f &&
			(best < 0 || unit->unit.attackers[index].game_time_stamp > unit->unit.attackers[best].game_time_stamp))
			best = index;
	if (best < 0)
		return;
	if (unit->unit.attackers[best].player_index != NONE)
		snprintf(text, size, "player %ld", (long)DATUM_INDEX_TO_ABSOLUTE_INDEX(unit->unit.attackers[best].player_index));
	else
	{
		struct object_datum *attacker = object_try_and_get(unit->unit.attackers[best].object_index);

		snprintf(text, size, "%s", attacker ? last_name(tag_get_name(attacker->definition_index)) : "an object gone");
	}
}

/* what a unit stands on, for the position line: "on <object tag>" (the biped physics' object under it), "bsp <surface>", "seated",
"air"; and the first thing a ray down from it meets with objects tested ("ray <object tag|bsp> <z>") */
static void ground_text(long unit_index, char *text, size_t size)
{
	struct biped_datum *biped = biped_try_and_get(unit_index);
	struct collision_result collision;
	real_point3d start;
	real_vector3d down = { 0.0f, 0.0f, -3.0f };
	char stand[80], ray[96];

	if (vehicle_of(unit_index) != NONE)
	{
		snprintf(text, size, "seated");
		return;
	}
	if (biped && biped->biped.elevator_object_index != NONE && object_try_and_get(biped->biped.elevator_object_index))
		snprintf(stand, sizeof stand, "on %s", last_name(tag_get_name(object_get(biped->biped.elevator_object_index)->definition_index)));
	else if (biped && biped->biped.support_surface_index != NONE)
		snprintf(stand, sizeof stand, "bsp %ld", (long)biped->biped.support_surface_index);
	else
		snprintf(stand, sizeof stand, "air");
	object_get_origin(unit_index, &start);
	start.z += 0.3f;
	if (!collision_test_vector(FLAG(_collision_test_structure_bit) | FLAG(_collision_test_front_facing_surfaces_bit) |
		FLAG(_collision_test_objects_bit) | FLAG(_collision_test_objects_scenery_bit) | FLAG(_collision_test_objects_machines_bit) |
		FLAG(_collision_test_objects_vehicles_bit) | FLAG(_collision_test_objects_controls_bit), &start, &down, unit_index, &collision))
		snprintf(ray, sizeof ray, "ray none");
	else if (collision.type == _collision_result_object && object_try_and_get(collision.object_index))
		snprintf(ray, sizeof ray, "ray %s %.2f", last_name(tag_get_name(object_get(collision.object_index)->definition_index)),
			collision.point.z);
	else
		snprintf(ray, sizeof ray, "ray bsp %.2f", collision.point.z);
	snprintf(text, size, "%s, %s", stand, ray);
}

/* the squad's log, every tick with a squad: a member's death (where, by what), and every 2 s a position line (the human, each
member: its spot, seated or dead) */
static void log_service(long now)
{
	struct data_iterator iterator;
	struct player_datum *player;
	char line[900];
	int used = 0;
	boolean positions = now >= companion.next_positions;

	if (positions)
	{
		long human = first_human_unit();
		real_point3d at;

		companion.next_positions = now + POSITIONS_TICKS;
		if (human != NONE)
		{
			char ground[180];

			object_get_origin(human, &at);
			ground_text(human, ground, sizeof ground);
			used = snprintf(line, sizeof line, "human %.1f %.1f %.1f (%s)", at.x, at.y, at.z, ground);
		}
		else
			used = snprintf(line, sizeof line, "human none");
	}
	data_iterator_new(&iterator, player_data);
	while ((player = (struct player_datum *)data_iterator_next(&iterator)) != NULL)
	{
		long slot;
		boolean alive;

		if (player->local_player_index != NONE || !bots_machine_is_bot(player->network_player_data.machine_index))
			continue;
		slot = player->network_player_data.machine_index - BOTS_FIRST_MACHINE;
		if (slot < 0 || slot >= AE_COMPANION_MOST || !companion.wanted[slot])
			continue;
		alive = player->unit_index != NONE && !TEST_FLAG(unit_get(player->unit_index)->object.damage_flags, _object_dead_bit);
		if (alive)
		{
			companion.last_unit[slot] = player->unit_index;
			object_get_origin(player->unit_index, &companion.last_at[slot]);
		}
		else if (companion.was_alive[slot] && !object_try_and_get(companion.last_unit[slot]))
		{
			/* (its body is gone, not dead: a checkpoint revert or a level change took it; no death) */
			platform_log("companion: %ld's body was taken away (a revert or a level change), at %.1f %.1f %.1f", slot + 1,
				companion.last_at[slot].x, companion.last_at[slot].y, companion.last_at[slot].z);
		}
		else if (companion.was_alive[slot])
		{
			char cause[96];

			death_cause(companion.last_unit[slot], cause, sizeof cause);
			platform_log("companion: %ld died at %.1f %.1f %.1f, by %s", slot + 1, companion.last_at[slot].x, companion.last_at[slot].y,
				companion.last_at[slot].z, cause);
		}
		companion.was_alive[slot] = alive;
		if (positions && used > 0 && used < (int)sizeof line && alive)
		{
			char ground[180];

			ground_text(player->unit_index, ground, sizeof ground);
			used += snprintf(line + used, sizeof line - used, ", %ld %.1f %.1f %.1f (%s)", slot + 1, companion.last_at[slot].x,
				companion.last_at[slot].y, companion.last_at[slot].z, ground);
		}
		else if (positions && used > 0 && used < (int)sizeof line)
			used += snprintf(line + used, sizeof line - used, ", %ld %s", slot + 1, player->unit_index == NONE ? "not in" : "dead");
	}
	if (positions && periodic_line(now, &positions_logged))
		platform_log("companion: positions t+%lds %s", (now - companion.started) / 30, line);
}

/* whether the human player (unit human at human_at) does not see a member's body at at: out of their view, or a wall between */
static boolean member_unseen(long human, real_point3d const *human_at, real_point3d const *at)
{
	struct unit_datum *looker = unit_get(human);
	struct collision_result collision;
	real_point3d eye = *human_at, chest = *at;
	real_vector3d line;

	eye.z += 0.6f;
	chest.z += 0.6f;
	vector_from_points3d(&eye, &chest, &line);
	return (boolean)(ae_companion_entry_unseen(looker->unit.looking_vector.i, looker->unit.looking_vector.j, at->x - human_at->x,
		at->y - human_at->y) || collision_test_vector(FLAG(_collision_test_structure_bit) | FLAG(_collision_test_front_facing_surfaces_bit),
		&eye, &line, human, &collision));
}

/* STUCK INDOORS: no entry spot behind the player (a corridor, every spot in their view or off the floor): a crumb of the player's
own track behind them with room for a body, not on another one, else a spot straight behind them; TRUE with the spot */
static boolean stuck_track_spot(long human, long unit, real_point3d *spot)
{
	real_point3d feet;
	int taken;

	object_get_origin(human, &feet);
	for (taken = 0; taken < AE_COMPANION_MOST + 2; taken++)
	{
		int crumb = ae_companion_track_behind((float const (*)[3])companion.track, companion.track_count, feet.x, feet.y, taken);
		real_point3d there, fixed;

		if (crumb < 0)
			break;
		there.x = companion.track[crumb][0];
		there.y = companion.track[crumb][1];
		there.z = companion.track[crumb][2] + 0.1f;
		if (spot_taken(&there, unit) || !biped_fix_position(unit, NONE, &there, &fixed, 2.0f, FALSE, TRUE, TRUE))
			continue;
		*spot = fixed;
		return TRUE;
	}
	/* (no crumb free: straight behind the player, 1.5 then 2.5 units, where a body has room) */
	{
		struct unit_datum *looker = unit_get(human);
		real i = looker->unit.looking_vector.i, j = looker->unit.looking_vector.j, length = (real)sqrt(i * i + j * j);
		real back;

		if (length < 0.01f)
			return FALSE;
		for (back = 1.5f; back <= 2.5f; back += 1.0f)
		{
			real_point3d there = feet, fixed;

			there.x -= i / length * back;
			there.y -= j / length * back;
			there.z += 0.1f;
			if (spot_taken(&there, unit) || !biped_fix_position(unit, NONE, &there, &fixed, 2.0f, FALSE, TRUE, TRUE))
				continue;
			*spot = fixed;
			return TRUE;
		}
	}
	return FALSE;
}

/* STUCK INDOORS (ae_companion_rules.h): a member under a roof cut off by a barrier or past the follow radius that has stayed where
it is is put behind the player (entry_spot); TRUE when it was */
static boolean stuck_indoors(long now, long slot, long player_index, long unit, long human, real_point3d const *human_at,
	real_point3d const *at, real distance)
{
	int indoors = bots_squad_indoors(), cut_off = bots_squad_member_cut_off(slot);
	real_point3d spot;
	long ticks;

	if (!indoors || distance < AE_COMPANION_STUCK_NEAR || !(cut_off || distance > AE_COMPANION_STUCK_FAR) ||
		!companion.stuck_tick[slot] || now < companion.stuck_tick[slot] ||
		distance < companion.stuck_distance[slot] - AE_COMPANION_STUCK_GAIN ||
		distance_squared3d(at, &companion.stuck_at[slot]) > AE_COMPANION_STUCK_GAIN * AE_COMPANION_STUCK_GAIN)
	{
		companion.stuck_tick[slot] = now > 0 ? now : 1;
		companion.stuck_at[slot] = *at;
		companion.stuck_distance[slot] = distance;
		return FALSE;
	}
	ticks = now - companion.stuck_tick[slot];
	if (ticks < AE_COMPANION_STUCK_TICKS || (now < companion.stuck_retry[slot] && now >= companion.stuck_retry[slot] - TICKS_PER_SECOND) ||
		!ae_companion_stuck_warp(indoors, cut_off, (float)distance, ticks, member_unseen(human, human_at, at), vehicle_of(unit) != NONE,
			bots_squad_member_fighting(slot)))
		return FALSE;
	/* (no entry spot: a crumb of the player's track behind them; neither: said once a window, tried again next tick) */
	if (!entry_spot(human, slot, unit, &spot) && !stuck_track_spot(human, unit, &spot))
	{
		if (companion.stuck_said[slot] != companion.stuck_tick[slot])
		{
			companion.stuck_said[slot] = companion.stuck_tick[slot];
			platform_log("companion: %ld is stuck indoors %.1f away, but no spot behind the player has room", slot + 1, (double)distance);
		}
		companion.stuck_retry[slot] = now + TICKS_PER_SECOND;
		return FALSE;
	}
	if (!player_teleport(player_index, NONE, &spot))
	{
		companion.stuck_retry[slot] = now + TICKS_PER_SECOND;
		return FALSE;
	}
	companion.teleports++;
	companion.indoor_warps++;
	companion.stuck_tick[slot] = 0;
	companion.best_distance[slot] = 0.0f;
	companion.gain_tick[slot] = now > 0 ? now : 1;
	bots_squad_reset(slot);
	platform_log("companion: %ld warps to the player at %.1f %.1f %.1f (stuck indoors %s, %.1f away for %ld s)", slot + 1, spot.x, spot.y,
		spot.z, cut_off ? "behind a barrier" : "out of reach", (double)distance, ticks / 30);
	return TRUE;
}

/* CATCH-UP (ae_companion_rules.h): the last resort, a member far from the player that has come no nearer for 8 s and that the
player does not see is put behind them (entry_spot); counted in the 1 Hz line's teleports */
static void catch_up_service(long now)
{
	struct data_iterator iterator;
	struct player_datum *player;
	long human = first_human_unit();
	real_point3d human_at;

	if (ae_companion_intro_holds() || human == NONE || vehicle_of(human) != NONE)
		return;
	object_get_origin(human, &human_at);
	data_iterator_new(&iterator, player_data);
	while ((player = (struct player_datum *)data_iterator_next(&iterator)) != NULL)
	{
		struct unit_datum *looker = unit_get(human);
		struct collision_result collision;
		long slot;
		real_point3d at, spot, eye, chest;
		real_vector3d line;
		real distance;
		boolean unseen;

		if (player->local_player_index != NONE || !bots_machine_is_bot(player->network_player_data.machine_index) ||
			player->unit_index == NONE || TEST_FLAG(unit_get(player->unit_index)->object.damage_flags, _object_dead_bit))
			continue;
		slot = player->network_player_data.machine_index - BOTS_FIRST_MACHINE;
		if (slot < 0 || slot >= AE_COMPANION_MOST || !companion.wanted[slot])
			continue;
		object_get_origin(player->unit_index, &at);
		distance = (real)sqrt(distance_squared3d(&at, &human_at));
		if (stuck_indoors(now, slot, iterator.datum_index, player->unit_index, human, &human_at, &at, distance))
			continue;
		if (!companion.gain_tick[slot] || distance < companion.best_distance[slot] - AE_COMPANION_CATCH_UP_GAIN ||
			distance <= AE_COMPANION_CATCH_UP_FAR || now < companion.gain_tick[slot])
		{
			companion.best_distance[slot] = distance;
			companion.gain_tick[slot] = now > 0 ? now : 1;
			continue;
		}
		eye = human_at;
		eye.z += 0.6f;
		chest = at;
		chest.z += 0.6f;
		vector_from_points3d(&eye, &chest, &line);
		unseen = (boolean)(ae_companion_entry_unseen(looker->unit.looking_vector.i, looker->unit.looking_vector.j, at.x - human_at.x,
			at.y - human_at.y) || collision_test_vector(FLAG(_collision_test_structure_bit) | FLAG(_collision_test_front_facing_surfaces_bit),
			&eye, &line, human, &collision));
		if (!ae_companion_catch_up((float)distance, now - companion.gain_tick[slot], unseen, vehicle_of(player->unit_index) != NONE) ||
			!entry_spot(human, slot, player->unit_index, &spot) || !player_teleport(iterator.datum_index, NONE, &spot))
			continue;
		companion.teleports++;
		companion.best_distance[slot] = 0.0f;
		companion.gain_tick[slot] = now > 0 ? now : 1;
		bots_squad_reset(slot);
		platform_log("companion: %ld teleports behind the player at %.1f %.1f %.1f (from %.1f %.1f %.1f, %.0f away: no way to them in "
			"%d s, out of their view)", slot + 1, spot.x, spot.y, spot.z, at.x, at.y, at.z, (double)distance,
			AE_COMPANION_CATCH_UP_TICKS / 30);
	}
}

/* a squad body with no weapon at all (its spawn's equipment lost: a seat taken as it spawned, a level's script) is given the level's
first starting profile, once a body, through player_add_equipment as a spawn's equipment is */
static void arm_service(void)
{
	static long armed[AE_COMPANION_MOST];
	struct data_iterator iterator;
	struct player_datum *player;

	data_iterator_new(&iterator, player_data);
	while ((player = (struct player_datum *)data_iterator_next(&iterator)) != NULL)
	{
		struct unit_datum *unit;
		long slot;
		short weapon;
		boolean any = FALSE;

		if (player->local_player_index != NONE || !bots_machine_is_bot(player->network_player_data.machine_index) ||
			player->unit_index == NONE)
			continue;
		slot = player->network_player_data.machine_index - BOTS_FIRST_MACHINE;
		if (slot < 0 || slot >= AE_COMPANION_MOST || armed[slot] == player->unit_index)
			continue;
		unit = unit_get(player->unit_index);
		if (TEST_FLAG(unit->object.damage_flags, _object_dead_bit))
			continue;
		for (weapon = 0; weapon < MAXIMUM_WEAPONS_PER_UNIT; weapon++)
			if (unit->unit.weapon_object_indices[weapon] != NONE)
				any = TRUE;
		armed[slot] = player->unit_index;
		if (!any && global_scenario_get()->starting_profiles.count > 0)
		{
			player_add_equipment(player->unit_index, 0, FALSE);
			platform_log("companion: %ld had no weapon: the level's starting profile given", slot + 1);
		}
	}
}

/* DEV MODE: the companions' bodies die (they come back as a co-op player's does) */
int ae_companion_bodies(void)
{
	struct data_iterator iterator;
	struct player_datum *player;
	int bodies = 0;

	if (!companion.active)
		return -1;
	data_iterator_new(&iterator, player_data);
	while ((player = (struct player_datum *)data_iterator_next(&iterator)) != NULL)
	{
		if (player->local_player_index == NONE && bots_machine_is_bot(player->network_player_data.machine_index) &&
			player->unit_index != NONE && !TEST_FLAG(unit_get(player->unit_index)->object.damage_flags, _object_dead_bit))
			bodies++;
	}
	return bodies;
}

real ae_companion_farthest(void)
{
	struct data_iterator iterator;
	struct player_datum *player;
	long human = first_human_unit();
	real farthest = -1.0f;
	real_point3d human_at;

	if (!companion.active || human == NONE)
		return -1.0f;
	object_get_origin(human, &human_at);
	data_iterator_new(&iterator, player_data);
	while ((player = (struct player_datum *)data_iterator_next(&iterator)) != NULL)
	{
		real_point3d at;
		real distance;

		if (player->local_player_index != NONE || !bots_machine_is_bot(player->network_player_data.machine_index) ||
			player->unit_index == NONE || TEST_FLAG(unit_get(player->unit_index)->object.damage_flags, _object_dead_bit))
			continue;
		object_get_origin(player->unit_index, &at);
		distance = (real)sqrt(distance_squared3d(&at, &human_at));
		if (distance > farthest)
			farthest = distance;
	}
	return farthest;
}

int ae_companion_kill(void)
{
	struct data_iterator iterator;
	struct player_datum *player;
	long killed = 0;

	if (!companion.active)
		return 0;
	data_iterator_new(&iterator, player_data);
	while ((player = (struct player_datum *)data_iterator_next(&iterator)) != NULL)
	{
		if (player->local_player_index != NONE || !bots_machine_is_bot(player->network_player_data.machine_index) ||
			player->unit_index == NONE || TEST_FLAG(unit_get(player->unit_index)->object.damage_flags, _object_dead_bit))
			continue;
		unit_kill(player->unit_index);
		killed++;
	}
	platform_log("companion: %ld killed", killed);
	return (int)killed;
}

int ae_companion_down_humans(void)
{
	struct data_iterator iterator;
	struct player_datum *player;
	long downed = 0;

	data_iterator_new(&iterator, player_data);
	while ((player = (struct player_datum *)data_iterator_next(&iterator)) != NULL)
	{
		if (player->local_player_index == NONE || player->unit_index == NONE ||
			TEST_FLAG(unit_get(player->unit_index)->object.damage_flags, _object_dead_bit))
			continue;
		unit_kill(player->unit_index);
		downed++;
	}
	platform_log("companion: %ld human player(s) killed", downed);
	return (int)downed;
}

static short free_seat(long vehicle);

int ae_companion_mount(long slot, long vehicle)
{
	if (slot < 0 || slot >= AE_COMPANION_MOST || !companion.active)
		return 0;
	if (vehicle != NONE && (!object_try_and_get_and_verify_type(vehicle, _object_mask_vehicle) || free_seat(vehicle) == NONE))
		return 0;
	companion.mount_vehicle[slot] = vehicle;
	companion.mount_tick[slot] = game_time_get();
	return 1;
}

long ae_companion_mount_of(long slot)
{
	if (slot < 0 || slot >= AE_COMPANION_MOST || companion.mount_vehicle[slot] == NONE)
		return NONE;
	/* (none, or a vehicle gone: the cleared state holds no vehicle) */
	if (!object_try_and_get_and_verify_type(companion.mount_vehicle[slot], _object_mask_vehicle))
	{
		companion.mount_vehicle[slot] = NONE;
		return NONE;
	}
	/* (not reached in time: given up) */
	if (game_time_get() - companion.mount_tick[slot] > AE_SQUAD_MOUNT_GIVE_UP_TICKS &&
		companion.boarded_vehicle[slot] != companion.mount_vehicle[slot])
	{
		platform_log("companion: %ld gave up on the vehicle it was sent to", slot + 1);
		companion.mount_vehicle[slot] = NONE;
		return NONE;
	}
	return companion.mount_vehicle[slot];
}

int ae_companion_has_free_seat(long vehicle)
{
	return vehicle != NONE && object_try_and_get_and_verify_type(vehicle, _object_mask_vehicle) && free_seat(vehicle) != NONE;
}

int ae_companion_dry(void)
{
	struct data_iterator iterator;
	struct player_datum *player;
	long emptied = 0;

	if (!companion.active)
		return 0;
	data_iterator_new(&iterator, player_data);
	while ((player = (struct player_datum *)data_iterator_next(&iterator)) != NULL)
	{
		struct unit_datum *unit;
		short each;

		if (player->local_player_index != NONE || !bots_machine_is_bot(player->network_player_data.machine_index) ||
			player->unit_index == NONE || TEST_FLAG(unit_get(player->unit_index)->object.damage_flags, _object_dead_bit))
			continue;
		unit = unit_get(player->unit_index);
		for (each = 0; each < MAXIMUM_WEAPONS_PER_UNIT; each++)
		{
			long weapon_index = unit->unit.weapon_object_indices[each];
			struct weapon_datum *weapon;
			short magazine;

			if (weapon_index == NONE)
				continue;
			weapon = weapon_get(weapon_index);
			for (magazine = 0; magazine < 2; magazine++)
			{
				weapon->weapon.magazines[magazine].rounds_loaded = 0;
				weapon->weapon.magazines[magazine].rounds_total = 0;
			}
			weapon->weapon.age = 1.0f;
		}
		emptied++;
	}
	platform_log("companion: %ld with no ammunition", emptied);
	return (int)emptied;
}

/* the squad added this slot's player to the network game itself: only such a player is ever removed (a multiplayer bot on the
same bot machine slot never is). Kept through a level change, which keeps the game's players; let go when the player is gone */
static boolean squad_added[AE_COMPANION_MOST];
/* the tick a squad player of the network game was first seen with no player of the game (0: it has one) */
static long squad_missing_since[AE_COMPANION_MOST];

/* whether the game has a player of this bot machine */
static boolean squad_has_player(long machine_index)
{
	struct data_iterator iterator;
	struct player_datum *player;

	data_iterator_new(&iterator, player_data);
	while ((player = (struct player_datum *)data_iterator_next(&iterator)) != NULL)
		if (player->network_player_data.machine_index == machine_index && !player->quit_out_of_game)
			return TRUE;
	return FALSE;
}

/* this machine's game player of a bot machine is made again, as a join in progress makes it: a checkpoint revert brought back
the players of a time before it joined */
static boolean respawn_reverted_player(long machine_index)
{
	struct network_game_client *client = global_network_game_client_get();
	struct network_game *game = client ? network_game_client_get_game(client) : NULL;
	long slot;

	if (!game || !game->local_data.game_objects_loaded)
		return FALSE;
	for (slot = 0; slot < HALO_PORT_MAXIMUM_NETWORK_PLAYERS; slot++)
	{
		struct network_player *player = &game->players[slot];
		long player_index;

		if (!network_player_is_valid(player) || player->machine_index != machine_index)
			continue;
		if (!network_game_spawn_player(player))
			return FALSE;
		player_index = unstrip_player_index(player->player_list_index);
		game_engine_player_added(player_index);
		update_client_add_player(player_index);
		if (global_network_game_server_get())
			update_server_add_player(player_index);
		return TRUE;
	}
	return FALSE;
}

/* the server's turn (ae_bots_live's, once an idle): the companion players wanted join the game in progress, and the ones no longer
wanted leave it, as a bot of a local game does */
void ae_companion_server_tick(struct network_game_server *server, struct network_game *game)
{
	long slot, found[AE_COMPANION_MOST], index;

	/* (only in a game being played: in the lobby or the menus the players would be added to no game, again and again) */
	if (game_engine_running())
		memset(squad_added, 0, sizeof squad_added);
	if (!companion.active || !server || !game || !game_in_progress() ||
		main_menu_is_active() || game_engine_running())
		return;
	for (slot = 0; slot < AE_COMPANION_MOST; slot++)
		found[slot] = NONE;
	for (index = 0; index < HALO_PORT_MAXIMUM_NETWORK_PLAYERS; index++)
	{
		struct network_player *player = &game->players[index];

		if (network_player_is_valid(player) && bots_machine_is_bot(player->machine_index) &&
			player->machine_index - BOTS_FIRST_MACHINE < AE_COMPANION_MOST &&
			game->machines[(long)player->machine_index].machine_index != player->machine_index)
			found[player->machine_index - BOTS_FIRST_MACHINE] = index;
	}
	for (slot = 0; slot < AE_COMPANION_MOST; slot++)
	{
		if (found[slot] == NONE)
			squad_added[slot] = FALSE;
		/* (in the network game with no player of the game, a while: a checkpoint revert taken before it joined (a cinematic's
		skip, a level's own revert) brought back the players of then. Its player is made again in its slot of the game, as a
		join in progress makes it (network_game_client_add_player_to_game); a leave would look for the player that is gone) */
		if (found[slot] != NONE && squad_added[slot])
		{
			long now = game_time_get();
			boolean has = squad_has_player(BOTS_FIRST_MACHINE + slot);

			if (has || now < squad_missing_since[slot])
				squad_missing_since[slot] = 0;
			else if (!squad_missing_since[slot])
				squad_missing_since[slot] = now > 0 ? now : 1;
			if (ae_companion_rejoin_wanted(!has, squad_missing_since[slot] ? now - squad_missing_since[slot] : 0))
			{
				squad_missing_since[slot] = 0;
				if (respawn_reverted_player(BOTS_FIRST_MACHINE + slot))
				{
					companion.attached[slot] = FALSE;
					companion.was_alive[slot] = FALSE;
					platform_log("companion: %ld was taken away by a checkpoint revert: its player is made again", slot + 1);
				}
				else
					platform_log("companion: %ld was taken away by a checkpoint revert and could not be made again", slot + 1);
				return;
			}
		}
		if (companion.wanted[slot] && found[slot] == NONE && companion.leaving_until[slot] > 0 &&
			game_time_get() <= companion.leaving_until[slot])
		{
			/* (the player of this slot that left is in the game until its quit's tick: a join before it is refused, and the quit
			then takes the new player too) */
			if (!companion.join_waits[slot])
				platform_log("companion: %ld joins again after its leave, at tick %ld", slot + 1, companion.leaving_until[slot] + 1);
			companion.join_waits[slot] = TRUE;
			continue;
		}
		if (companion.wanted[slot] && found[slot] == NONE && network_game_has_free_player_slot(game))
		{
			struct network_player joining;

			companion.join_waits[slot] = FALSE;

			bots_network_player(slot, &joining);
			joining.team_index = 0;
			joining.primary_color_index = NONE;
			get_unique_random_color(server, &joining);
			joining.player_list_index = NONE;
			if (!network_game_add_player(game, &joining))
			{
				platform_log("companion: %ld could not join", slot + 1);
				companion.wanted[slot] = FALSE;
				continue;
			}
			if (!network_game_server_send_player_joined_info_ingame(server, &joining))
				platform_log("companion: the machines missed %ld joining", slot + 1);
			squad_added[slot] = TRUE;
			platform_log("companion: %ld joined the game in progress", slot + 1);
			return;
		}
		if (found[slot] != NONE && ae_companion_may_remove(squad_added[slot], companion.wanted[slot]))
		{
			struct { struct network_player player; long quit_time; } leaving;
			void *message;

			leaving.player = game->players[found[slot]];
			if (!network_game_remove_player(game, &leaving.player))
				continue;
			leaving.quit_time = game_time_get() + 33;
			companion.leaving_until[slot] = leaving.quit_time + 1;
			message = create_network_game_message(_message_server_remove_player_ingame, &leaving, sizeof leaving);
			if (!message || !network_game_server_send_message_to_all_machines(server, message))
				platform_log("companion: the machines missed %ld leaving", slot + 1);
			platform_log("companion: %ld left the game", slot + 1);
			companion.attached[slot] = FALSE;
			squad_added[slot] = FALSE;
			return;
		}
	}
}

/* every tick: a companion player whose unit is alive and has no actor is given one, so the engine's AI drives it. A body that is
new (the first, or a respawn's) takes the weapon of its variant */
static void attach_actors(void)
{
	struct data_iterator iterator;
	struct player_datum *player;

	if (brain_is_bot())
		return;

	data_iterator_new(&iterator, player_data);
	while ((player = (struct player_datum *)data_iterator_next(&iterator)) != NULL)
	{
		struct unit_datum *unit;
		long slot, actor_index;

		if (player->local_player_index != NONE || !bots_machine_is_bot(player->network_player_data.machine_index))
			continue;
		slot = player->network_player_data.machine_index - BOTS_FIRST_MACHINE;
		if (slot < 0 || slot >= AE_COMPANION_MOST || !companion.wanted[slot])
			continue;
		if (player->unit_index == NONE)
		{
			companion.had_unit[slot] = FALSE;
			continue;
		}
		unit = unit_get(player->unit_index);
		if (TEST_FLAG(unit->object.damage_flags, _object_dead_bit))
		{
			companion.had_unit[slot] = FALSE;
			continue;
		}
		if (unit->unit.actor_index != NONE)
		{
			if (TEST_FLAG(actor_get(unit->unit.actor_index)->output.control_flags, _unit_control_weapon_primary_trigger_bit))
				companion.fire_ticks++;
			continue;
		}
		if (!companion.had_unit[slot] && unit->object.parent_object_index == NONE)
			give_variant_weapon(companion.variant[slot], player->unit_index);
		actor_index = actor_create_for_unit(FALSE, player->unit_index, companion.variant[slot], NONE, NONE, FALSE, NONE, FALSE,
			companion_state_alert, companion_state_none, NONE, 0);
		if (actor_index == NONE)
		{
			platform_log("companion: %ld got no actor on its body", slot + 1);
			continue;
		}
		ai_scripting_force_active_by_unit(player->unit_index, TRUE);
		if (companion.had_unit[slot] == FALSE && companion.attached[slot])
			companion.respawns++;
		companion.attached[slot] = TRUE;
		companion.had_unit[slot] = TRUE;
		companion.actors_made++;
		companion.has_goal[slot] = FALSE;
		platform_log("companion: %ld is driven by the %s actor, team %d", slot + 1, last_name(tag_get_name(companion.variant[slot])),
			(int)actor_get(actor_index)->meta.team_index);
	}
}

/* a free seat of the vehicle that is not the driver's: a gunner's first, else a passenger's; NONE when none */
/* a passenger's seat a squad member may sit in: it holds weapons and is not hidden (a dropship's cargo hook, which carries a
vehicle under it, is neither) */
static boolean seat_for_a_rider(long vehicle, short seat)
{
	struct unit_definition *definition = unit_definition_get(object_get(vehicle)->definition_index);
	unsigned long flags = TAG_BLOCK_GET_ELEMENT(&definition->unit.seats, seat, struct unit_seat)->flags;

	return (boolean)(TEST_FLAG(flags, SEAT_ALLOWS_WEAPONS_BIT) && !TEST_FLAG(flags, _unit_seat_invisible_bit));
}

static short free_seat(long vehicle)
{
	short seat, seats = unit_definition_get(object_get(vehicle)->definition_index)->unit.seats.count, passenger = NONE;

	for (seat = 0; seat < seats; seat++)
	{
		if (unit_seat_is_driver(vehicle, seat) || unit_seat_filled(vehicle, seat))
			continue;
		if (unit_seat_is_gunner(vehicle, seat))
			return seat;
		if (passenger == NONE && seat_for_a_rider(vehicle, seat))
			passenger = seat;
	}
	return passenger;
}

/* a free passenger seat of the vehicle (never a driver's, a gunner's or one seat_for_a_rider refuses); NONE when none */
static short free_passenger_seat(long vehicle)
{
	short seat, seats = unit_definition_get(object_get(vehicle)->definition_index)->unit.seats.count;

	for (seat = 0; seat < seats; seat++)
	{
		if (!unit_seat_is_driver(vehicle, seat) && !unit_seat_is_gunner(vehicle, seat) && !unit_seat_filled(vehicle, seat) &&
			seat_for_a_rider(vehicle, seat))
			return seat;
	}
	return NONE;
}

/* whether the squad rides this level's intro (ae_companion_rules.h's list; HALO_COMPANION_INTRO_RIDE=1 rides every level's,
for the intro sweep) */
static boolean intro_ride(void)
{
	char const *forced = getenv("HALO_COMPANION_INTRO_RIDE");

	return (boolean)ae_companion_intro_ride(global_scenario_index != NONE ? last_name(tag_get_name(global_scenario_index)) : NULL,
		forced && forced[0] == '1');
}

int ae_companion_intro_hold_reason(void)
{
	if (cinematic_in_progress())
		return AE_COMPANION_HOLD_CINEMATIC;
	if (cinematic_globals && cinematic_globals->show_letterbox)
		return AE_COMPANION_HOLD_LETTERBOX;
	return !player_input_enabled() ? AE_COMPANION_HOLD_INPUT : AE_COMPANION_HOLD_NONE;
}

int ae_companion_intro_holds(void)
{
	return ae_companion_intro_hold_reason() != AE_COMPANION_HOLD_NONE;
}

int ae_companion_script_skips(long player_index)
{
	struct player_datum *player = player_index != NONE ? player_try_and_get(player_index) : NULL;

	return player && !game_engine_running() && player->local_player_index == NONE &&
		bots_machine_is_bot(player->network_player_data.machine_index);
}

int ae_companion_player_down_keeps(long player_index)
{
	static char const *last_word;
	static int last_mode = -1;
	char const *word = config_string("game.companions_player_down");
	int respawn = ae_companion_player_down_respawn(word);

	if (respawn != last_mode || word != last_word)
	{
		if (last_mode >= 0 && respawn != last_mode)
			platform_log("companion: PLAYER DOWN is %s", respawn ? "RESPAWN (the squad keeps the game going)" :
				"CHECKPOINT (with every human down the level goes back to its checkpoint)");
		last_mode = respawn;
		last_word = word;
	}
	return ae_companion_player_down_counts(ae_companion_script_skips(player_index), respawn);
}

int ae_companion_squad_row(long player_index)
{
	struct player_datum *player = player_index != NONE ? player_try_and_get(player_index) : NULL;

	return player && !game_engine_running() && player->local_player_index == NONE &&
		bots_machine_is_bot(player->network_player_data.machine_index);
}

int ae_companion_squad_only(void)
{
	struct data_iterator iterator;
	struct player_datum *player;

	data_iterator_new(&iterator, player_data);
	while ((player = (struct player_datum *)data_iterator_next(&iterator)) != NULL)
		if (player->local_player_index == NONE && !ae_companion_squad_row(iterator.datum_index))
			return 0;
	return 1;
}

int ae_companion_spawn_waits(long player_index)
{
	struct player_datum *player = player_try_and_get(player_index);
	long slot, human;
	boolean waits;

	if (!companion.active || !player || player->unit_index != NONE || player->statistics.deaths != 0 ||
		player->local_player_index != NONE || !bots_machine_is_bot(player->network_player_data.machine_index) ||
		game_engine_running())
		return 0;
	slot = player->network_player_data.machine_index - BOTS_FIRST_MACHINE;
	if (slot < 0 || slot >= AE_COMPANION_MOST || !companion.wanted[slot])
		return 0;
	/* (a level whose intro the squad rides: it spawns into a seat, ae_companion_intro_seat) */
	if (intro_ride() && cinematic_in_progress())
		return 0;
	human = first_human_unit();
	waits = ae_companion_intro_holds() || human == NONE;
	/* (the human seated with control: a few seconds for them to get out, then the member spawns and boards) */
	if (!waits && vehicle_of(human) != NONE)
	{
		long now = game_time_get();

		if (!companion.seated_wait_since)
			companion.seated_wait_since = now > 0 ? now : 1;
		waits = (boolean)(now - companion.seated_wait_since < SEATED_WAIT_TICKS);
	}
	else if (!waits)
		companion.seated_wait_since = 0;
	if (waits && !companion.spawn_wait_logged[slot])
	{
		companion.spawn_wait_logged[slot] = TRUE;
		platform_log("companion: %ld waits to enter until the player has control on foot", slot + 1);
	}
	return waits;
}

int ae_companion_intro_seat(long player_index, long *vehicle, short *seat)
{
	struct player_datum *player = player_try_and_get(player_index);
	long slot, human, ride;
	real_point3d at;

	if (!companion.active || !brain_is_bot() || !player || player->unit_index != NONE || player->statistics.deaths != 0 ||
		player->local_player_index != NONE || !bots_machine_is_bot(player->network_player_data.machine_index) ||
		game_engine_running() || !cinematic_in_progress() || !intro_ride())
		return 0;
	slot = player->network_player_data.machine_index - BOTS_FIRST_MACHINE;
	if (slot < 0 || slot >= AE_COMPANION_MOST || !companion.wanted[slot])
		return 0;
	human = first_human_unit();
	if (human == NONE)
		return 0;
	ride = vehicle_of(human);
	if (ride == NONE)
		return 0;
	*seat = free_passenger_seat(ride);
	if (*seat == NONE)
		return 0;
	*vehicle = ride;
	(void)at;
	return 1;
}

void ae_companion_intro_board(long unit_index, long vehicle, short seat)
{
	if (unit_enter_seat(unit_index, vehicle, seat))
	{
		companion.seated++;
		platform_log("companion: spawned into seat %d of %s during the intro", (int)seat,
			last_name(tag_get_name(object_get(vehicle)->definition_index)));
	}
	else
		platform_log("companion: could not take seat %d of the intro's vehicle", (int)seat);
}

/* a vehicle a human player sits in: the companions near it take its free seats; a companion in a vehicle no human sits in gets out */
static void vehicles_follow(void)
{
	struct data_iterator iterator;
	struct player_datum *player;

	/* (the level's intro has the players' seats: the squad gets in and out only after it lets the players go) */
	if (ae_companion_intro_holds())
		return;
	data_iterator_new(&iterator, player_data);
	while ((player = (struct player_datum *)data_iterator_next(&iterator)) != NULL)
	{
		struct unit_datum *unit;
		long slot, vehicle, human_vehicle = NONE;
		real_point3d player_at, at;

		if (player->local_player_index != NONE || !bots_machine_is_bot(player->network_player_data.machine_index) ||
			player->unit_index == NONE)
			continue;
		slot = player->network_player_data.machine_index - BOTS_FIRST_MACHINE;
		if (slot < 0 || slot >= AE_COMPANION_MOST || !companion.wanted[slot])
			continue;
		unit = unit_get(player->unit_index);
		if (TEST_FLAG(unit->object.damage_flags, _object_dead_bit))
			continue;
		object_get_origin(player->unit_index, &at);
		{
			long human = nearest_player_unit(&at, &player_at);

			if (human != NONE)
				human_vehicle = vehicle_of(human);
		}
		vehicle = vehicle_of(player->unit_index);
		/* (out of the seat it took, and not by the rule below: whatever put it out is logged, and it waits to board again) */
		if (vehicle == NONE && companion.boarded_vehicle[slot] != NONE && companion.boarded_tick[slot])
		{
			long now = game_time_get();

			platform_log("companion: %ld was put out of seat %d of %s after %ld ticks, boards again in %d s", slot + 1,
				(int)companion.boarded_seat[slot], object_try_and_get(companion.boarded_vehicle[slot]) ?
				last_name(tag_get_name(object_get(companion.boarded_vehicle[slot])->definition_index)) : "a vehicle gone",
				now - companion.boarded_tick[slot], BOARD_RETRY_TICKS / 30);
			companion.boarded_vehicle[slot] = NONE;
			companion.boarded_tick[slot] = 0;
			companion.board_after[slot] = now + BOARD_RETRY_TICKS;
		}
		/* (MOUNT: the vehicle it was sent to, reached: in, the gunner's seat first; one it is in is kept) */
		if (companion.mount_vehicle[slot] != NONE && !object_try_and_get_and_verify_type(companion.mount_vehicle[slot], _object_mask_vehicle))
			companion.mount_vehicle[slot] = NONE;
		if (vehicle == NONE && companion.mount_vehicle[slot] != NONE &&
			distance_squared3d(&at, &object_get(companion.mount_vehicle[slot])->object.position) <
				AE_SQUAD_MOUNT_BOARD * AE_SQUAD_MOUNT_BOARD)
		{
			short seat = free_seat(companion.mount_vehicle[slot]);

			if (seat == NONE)
			{
				platform_log("companion: %ld finds no free seat in the vehicle it was sent to", slot + 1);
				companion.mount_vehicle[slot] = NONE;
			}
			else if (unit_enter_seat(player->unit_index, companion.mount_vehicle[slot], seat))
			{
				companion.seated++;
				companion.boarded_vehicle[slot] = companion.mount_vehicle[slot];
				companion.boarded_seat[slot] = seat;
				companion.boarded_tick[slot] = game_time_get() > 0 ? game_time_get() : 1;
				platform_log("companion: %ld mans seat %d (%s) of %s, as ordered", slot + 1, (int)seat,
					unit_seat_is_gunner(companion.mount_vehicle[slot], seat) ? "gunner" : "passenger",
					last_name(tag_get_name(object_get(companion.mount_vehicle[slot])->definition_index)));
				continue;
			}
		}
		if (vehicle != NONE && vehicle != human_vehicle && vehicle != companion.mount_vehicle[slot])
		{
			unit_exit_seat_end(player->unit_index);
			companion.has_goal[slot] = FALSE;
			companion.boarded_vehicle[slot] = NONE;
			companion.boarded_tick[slot] = 0;
			platform_log("companion: %ld gets out", slot + 1);
		}
		else if (vehicle == NONE && human_vehicle != NONE && game_time_get() >= companion.board_after[slot] &&
			distance_squared3d(&at, &player_at) < BOARD_DISTANCE * BOARD_DISTANCE)
		{
			short seat = free_seat(human_vehicle);

			if (seat != NONE && unit_enter_seat(player->unit_index, human_vehicle, seat))
			{
				companion.seated++;
				companion.boarded_vehicle[slot] = human_vehicle;
				companion.boarded_seat[slot] = seat;
				companion.boarded_tick[slot] = game_time_get() > 0 ? game_time_get() : 1;
				if (unit_seat_is_gunner(human_vehicle, seat))
					platform_log("companion: %ld takes the gunner seat (%d) of %s", slot + 1, (int)seat,
						last_name(tag_get_name(object_get(human_vehicle)->definition_index)));
				else
					platform_log("companion: %ld takes seat %d of %s", slot + 1, (int)seat,
						last_name(tag_get_name(object_get(human_vehicle)->definition_index)));
			}
		}
	}
}

/* a companion standing on another or on the player steps away from the one nearest: TRUE when it is moving to do so */
static boolean spread_orders(long actor_index, long slot, real_point3d const *player_at)
{
	struct actor_datum *actor = actor_get(actor_index);
	real_point3d const *body = &actor->input.position.body_position;
	long now = game_time_get();
	struct data_iterator iterator;
	struct player_datum *player;
	real_vector3d push;
	real nearest = 1000.0f, length;

	real_point3d ground;
	long surface = biped_find_pathfinding_surface_index(actor->meta.unit_index, &ground);

	if (companion.spread_until[slot] > now && surface != NONE &&
		distance_squared3d(body, &companion.spread_goal[slot]) > 0.6f * 0.6f)
	{
		actor_move_to_point(actor_index, &companion.spread_goal[slot], surface, NONE);
		return TRUE;
	}
	push.i = push.j = push.k = 0.0f;
	data_iterator_new(&iterator, player_data);
	while ((player = (struct player_datum *)data_iterator_next(&iterator)) != NULL)
	{
		real_point3d at;
		real gap, limit;

		if (player->unit_index == NONE || player->unit_index == actor->meta.unit_index ||
			TEST_FLAG(unit_get(player->unit_index)->object.damage_flags, _object_dead_bit))
			continue;
		limit = bots_machine_is_bot(player->network_player_data.machine_index) ? CROWD_DISTANCE : CROWD_PLAYER_DISTANCE;
		object_get_origin(player->unit_index, &at);
		gap = (real)sqrt(distance_squared3d(body, &at));
		if (gap < limit && gap < nearest)
		{
			nearest = gap;
			push.i = body->x - at.x;
			push.j = body->y - at.y;
		}
	}
	if (nearest > 999.0f)
		return FALSE;
	if (surface == NONE)
	{
		companion.spread_no_surface++;
		return FALSE;
	}
	length = (real)sqrt(push.i * push.i + push.j * push.j);
	if (length < 0.05f)
	{
		/* (exactly on it: each companion goes its own way, by number) */
		push.i = (real)cos(1.7 * (double)slot + 0.6);
		push.j = (real)sin(1.7 * (double)slot + 0.6);
		length = 1.0f;
	}
	companion.spread_goal[slot].x = body->x + push.i / length * SPREAD_STEP;
	companion.spread_goal[slot].y = body->y + push.j / length * SPREAD_STEP;
	companion.spread_goal[slot].z = body->z;
	companion.spread_until[slot] = now + SPREAD_TICKS;
	(void)player_at;
	companion.spread_orders_made++;
	if (!actor_move_to_point(actor_index, &companion.spread_goal[slot], surface, NONE))
	{
		companion.spread_refused++;
		return FALSE;
	}
	return TRUE;
}

void ae_companion_orders(long actor_index)
{
	struct actor_datum *actor;
	struct unit_datum *unit;
	long slot, player_unit, surface;
	real_point3d player_at, ground, goal;
	real distance;
	boolean engaged, go;
	real settle;

	if (!companion.active)
		return;
	actor = actor_get(actor_index);
	slot = actor->meta.unit_index == NONE ? NONE : slot_of_unit(actor->meta.unit_index);
	if (slot == NONE)
		return;
	unit = unit_get(actor->meta.unit_index);
	/* (a companion in a vehicle's seat is driven by the vehicle's own rules) */
	if (unit->object.parent_object_index != NONE)
		return;
	player_unit = nearest_player_unit(&actor->input.position.body_position, &player_at);
	if (player_unit == NONE)
		return;
	distance = (real)sqrt(distance_squared3d(&actor->input.position.body_position, &player_at));
	/* (each companion stops a step further from the player than the one before: they stand spread along the way they came) */
	settle = SETTLE_DISTANCE + SETTLE_STEP * (real)slot;
	engaged = actor->situation.visible_reachable_enemies > 0 || actor->target.target_type >= _actor_target_acknowledged_enemy;

	if (distance > FIGHT_DISTANCE)
		go = TRUE;
	else if (engaged)
		go = FALSE;
	else if (distance > FAR_DISTANCE)
		go = TRUE;
	else if (distance > NEAR_DISTANCE)
		go = companion.catching[slot];
	else
		go = companion.catching[slot] && distance > settle;
	if (distance <= settle)
		companion.catching[slot] = FALSE;
	else if (go)
		companion.catching[slot] = TRUE;

	if (go)
	{
		surface = biped_find_pathfinding_surface_index(player_unit, &ground);
		if (surface == NONE)
			return;
		/* (the destination only moves when the player has moved off it: a new path each tick costs more than it gains) */
		if (companion.has_goal[slot] && distance_squared3d(&companion.goal[slot], &ground) < RETARGET_DISTANCE * RETARGET_DISTANCE)
			goal = companion.goal[slot];
		else
		{
			goal = ground;
			companion.goal[slot] = goal;
			companion.has_goal[slot] = TRUE;
			companion.moves_ordered++;
		}
		actor_move_to_point(actor_index, &goal, surface, NONE);
	}
	else if (!engaged)
	{
		companion.has_goal[slot] = FALSE;
		if (!spread_orders(actor_index, slot, &player_at))
			actor_move_halt(actor_index);
	}
}

/* a campaign game with the setting (or DEV MODE) asking for companions and none yet: they are asked for, once the level's intro
has passed and a player is in */
static void auto_ask(long now)
{
	char text[160];

	if (companion.next_auto > now || wanted_total() < 1)
		return;
	if (brain_is_bot())
	{
		/* (the bots' brain: the squad is in the game with the level, as co-op players are, and takes its place when co-op lets
		the players after the first spawn: after the level's intro) */
		companion.next_auto = now + AUTO_RETRY_BOT_TICKS;
	}
	else
	{
		if (now < AUTO_START_TICKS)
			return;
		companion.next_auto = now + AUTO_RETRY_TICKS;
		if (cinematic_in_progress() || first_human_unit() == NONE)
		{
			if (now % 600 < AUTO_RETRY_TICKS)
				platform_log("companion: waiting for the intro (cinematic %d, a human on foot %d)", (int)cinematic_in_progress(),
					first_human_unit() != NONE);
			return;
		}
	}
	/* (the count asked for this level is its DEV MODE count from now on) */
	if (count_next_level >= 0)
	{
		if (count_next_level_setting == config_integer("game.companions"))
			count_override = count_next_level;
		else
			platform_log("companion: AI SQUAD was changed to %ld after the count %ld was given: the menu's count", (long)config_integer("game.companions"),
				count_next_level);
		count_next_level = -1;
	}
	if (!ask((int)wanted_total(), text, sizeof text) && strcmp(text, last_refusal_text))
	{
		snprintf(last_refusal_text, sizeof last_refusal_text, "%s", text);
		platform_log("companion: not asked for: %s", text);
	}
}

void ae_companion_tick(void)
{
	struct scenario *scenario = global_scenario_try_and_get();
	struct ai_script_actor_reference_iterator iterator;
	struct actor_datum *actor;
	struct data_iterator players;
	struct player_datum *player;
	long encounter, now = game_time_get(), alive[ENEMIES], alive_count = 0, each, near_count = 0, engaged = 0, moving = 0;
	real_point3d player_at, at, bodies[AE_COMPANION_MOST];
	boolean seated_bodies[AE_COMPANION_MOST];
	real farthest = 0.0f, closest = 1000.0f;

	if (!game_in_progress() || !campaign_host(scenario, FALSE))
	{
		if (companion.active)
		{
			companion_clear();
			platform_log("companion: the squad is off (not a campaign game this machine hosts)");
		}
		/* (a multiplayer game: a test's next-level count is not that game's) */
		if (game_engine_running())
			count_next_level = -1;
		/* (a DEV MODE count is the game's it was given in: the next level started from the menus asks the setting, or the
		count asked for it) */
		count_override = -1;
		return;
	}
	won_service(now);
	/* (PLAYER DOWN CHECKPOINT: every human down with members standing ends in the level's loss, back to its checkpoint) */
	{
		static boolean told;
		boolean down = players_are_all_dead() && companion.active && ae_companion_bodies() > 0;

		if (down && !told)
			platform_log("companion: every human is down: PLAYER DOWN CHECKPOINT, the level goes back to its checkpoint");
		told = down;
	}
	if (!companion.active)
	{
		auto_ask(now);
		return;
	}
	/* (a new level: its time starts again at 0, under the last level's start tick; another scenario as well, for a level
	that runs past the last one's start, which kept the last one's state, its reset window among it, otherwise) */
	if (now < companion.started || companion.level_scenario != global_scenario_index)
	{
		/* a new level: the companions asked for stay asked for, and join the new game */
		boolean wanted[AE_COMPANION_MOST];
		long count = companion.wanted_count;

		memcpy(wanted, companion.wanted, sizeof wanted);
		companion_clear();
		memcpy(companion.wanted, wanted, sizeof wanted);
		companion.wanted_count = count;
		companion.active = TRUE;
		companion.started = now;
		companion.level_scenario = global_scenario_index;
		companion.next_sample = now + SAMPLE_TICKS;
		companion.candidate_count = variants_choose(scenario, companion.candidates, VARIANTS);
		platform_log("companion: a new level, %ld wanted in all", count);
		for (each = 0; each < AE_COMPANION_MOST && companion.candidate_count; each++)
			companion.variant[each] = companion.candidates[each % companion.candidate_count];
		if (!companion.candidate_count && !brain_is_bot())
			companion.active = FALSE;
		platform_log("companion: a new level, %ld companion(s) stay asked for, %ld variant(s)", count, companion.candidate_count);
		return;
	}
	attach_actors();
	arm_service();
	track_service();
	entry_service();
	place_service(now);
	catch_up_service(now);
	log_service(now);
	vehicles_follow();
	if (now < companion.next_sample)
		return;
	companion.next_sample = now + SAMPLE_TICKS;
	data_iterator_new(&players, player_data);
	while ((player = (struct player_datum *)data_iterator_next(&players)) != NULL)
	{
		struct unit_datum *unit;

		if (player->local_player_index != NONE || !bots_machine_is_bot(player->network_player_data.machine_index) ||
			player->unit_index == NONE)
			continue;
		unit = unit_get(player->unit_index);
		if (TEST_FLAG(unit->object.damage_flags, _object_dead_bit))
			continue;
		object_get_origin(player->unit_index, &at);
		/* (the closest pair on foot: seated ones share the vehicle's spot) */
		if (near_count < AE_COMPANION_MOST)
		{
			bodies[near_count] = at;
			seated_bodies[near_count] = (boolean)(vehicle_of(player->unit_index) != NONE);
		}
		near_count++;
		if (nearest_player_unit(&at, &player_at) != NONE)
		{
			real distance = (real)sqrt(distance_squared3d(&at, &player_at));

			if (distance > farthest)
				farthest = distance;
		}
		if (unit->unit.actor_index != NONE)
		{
			struct actor_datum *own = actor_get(unit->unit.actor_index);

			if (own->situation.visible_reachable_enemies > 0)
				engaged++;
			if (own->control.moving)
				moving++;
		}
	}
	for (each = 0; each < near_count && each < AE_COMPANION_MOST; each++)
	{
		long other;

		for (other = each + 1; other < near_count && other < AE_COMPANION_MOST; other++)
		{
			real gap;

			if (seated_bodies[each] || seated_bodies[other])
				continue;
			gap = (real)sqrt(distance_squared3d(&bodies[each], &bodies[other]));

			if (gap < closest)
				closest = gap;
		}
	}
	for (encounter = 0; encounter < scenario->ai_encounters.count; encounter++)
	{
		ai_index_actor_iterator_new(encounter, &iterator);
		while ((actor = ai_index_actor_iterator_next(&iterator)) != NULL)
		{
			long unit_index = actor->meta.unit_index;

			if (unit_index == NONE || alive_count >= ENEMIES || !game_team_is_enemy(_game_team_player, actor->meta.team_index))
				continue;
			if (TEST_FLAG(unit_get(unit_index)->object.damage_flags, _object_dead_bit))
				continue;
			alive[alive_count++] = unit_index;
		}
	}
	for (each = 0; each < companion.enemy_seen_count; each++)
	{
		long other, found = FALSE;

		for (other = 0; other < alive_count; other++)
			if (alive[other] == companion.enemy_seen[each])
				found = TRUE;
		if (!found)
			companion.enemies_gone++;
	}
	companion.enemy_seen_count = 0;
	for (each = 0; each < alive_count; each++)
		companion.enemy_seen[companion.enemy_seen_count++] = alive[each];
	if (brain_is_bot())
	{
		char brain[512];

		bots_squad_report(brain, sizeof brain);
		if (periodic_line(now, &status_logged))
			platform_log("companion: t+%lds %ld bodies, farthest %.1f closest pair %.1f, enemies alive %ld gone %ld, seated %ld, teleports %ld (indoor warps %ld), %s",
			(now - companion.started) / 30, near_count, (double)farthest, (double)(closest > 999.0f ? 0.0f : closest), alive_count,
			companion.enemies_gone, companion.seated, companion.teleports, companion.indoor_warps, brain);
		return;
	}
	if (periodic_line(now, &status_logged))
		platform_log("companion: t+%lds %ld bodies, %ld driven, farthest %.1f closest pair %.1f, %ld see an enemy, %ld moving, enemies alive %ld gone %ld, moves %ld, trigger %ld ticks, respawns %ld, seated %ld, spread orders %ld no-surface %ld refused %ld",
		(now - companion.started) / 30, near_count, companion.actors_made, (double)farthest, (double)(closest > 999.0f ? 0.0f : closest),
		engaged, moving, alive_count, companion.enemies_gone, companion.moves_ordered, companion.fire_ticks, companion.respawns,
		companion.seated, companion.spread_orders_made, companion.spread_no_surface, companion.spread_refused);
}

/* whether a level started from the CAMPAIGN screen is to have companions (the setting, or DEV MODE's count) */
int ae_companion_asked_for_start(void)
{
	return wanted_total() > 0;
}

void ae_companion_won_after(int seconds)
{
	won_after_ticks = (long)seconds * 30;
	won_at_tick = -1;
	won_armed_tick = -1;
}

/* DEV MODE: the level's end at the time asked (main_won_map, as the level's own end does) */
static void won_service(long now)
{
	if (won_after_ticks < 0)
		return;
	/* (armed in a level that is over: a later level, or the same one started again, is not ended by it) */
	if (won_at_tick >= 0 && (global_scenario_index != won_scenario || now < won_armed_tick))
	{
		won_after_ticks = won_at_tick = -1;
		platform_log("companion: the DEV MODE level end was for a level that is over: dropped");
		return;
	}
	if (won_at_tick < 0)
	{
		won_at_tick = now + won_after_ticks;
		won_scenario = global_scenario_index;
		won_armed_tick = now;
	}
	if (now >= won_at_tick)
	{
		won_after_ticks = -1;
		platform_log("companion: the level is won now (DEV MODE)");
		main_won_map();
	}
}
