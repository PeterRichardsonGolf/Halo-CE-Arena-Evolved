/*
AE_DS5_GAME.C

Arena Evolved: each local player's DualSense wish (ae_ds5_game.h), for a controller port whose pad is a DualSense
the platform side can drive (port/linux/src/ae_ds5.c):
- the lightbar in the player's colour (the team's in a team game, the armour's otherwise; a campaign's Chief green),
  with the powerups (ae_ds5_lightbar), and the player lights the split-screen number;
- R2 by the weapon in hand, its profile (ae_ds5_rules.c's table) from the tag's own numbers (rate of fire, charging,
  heat a round, zoom levels, weapon type; AE's battle rifle by its own check) and the weapon's state each frame
  (heat, overheated, charge, a round in the last 5 ticks, reloading, empty, readying); each new round (the weapon's
  last-fired game time moved) is a shot the platform side turns into the profile's kick. In a vehicle, a gunner's
  seat takes the vehicle's weapon; any other seat has none;
- L2 a light resistance while zoomed, or a powerup's buzz (an overshield charging, becoming the juggernaut).
debug.ds5_log: each weapon tag's numbers and kind once a map, what each player wants as it changes, the first
rounds of each weapon.
*/

#include <stdio.h>
#include <string.h>

#include "cseries.h"
#include "real_math.h"
#include "tag_files/tag_groups.h"
#include "game/game.h"
#include "game/game_engine.h"
#include "game/players.h"
#include "items/weapon_definitions.h"
#include "items/weapons.h"
#include "items/weapon_datum_flags.h"
#include "objects/objects.h"
#include "cache/cache_files.h"
#include "../src/ae_ds5.h"
#include "ae_burst_rifle.h"
#include "ae_juggernaut.h"
#include "units/units.h"
#include "units/unit_definitions.h"
#include "interface/hud_draw.h"
#include "interface/unit_hud_interface_definition.h"
#include "ae_hit_feedback.h"
#include "ae_ds5_game.h"

void platform_log(char const *format, ...);

static char last_wanted[MAXIMUM_LOCAL_PLAYERS][sizeof(((struct ae_ds5_wish *)0)->what)];

static unsigned char channel(real value)
{
	if (value <= 0.0f)
		return 0;
	if (value >= 1.0f)
		return 255;
	return (unsigned char)(value * 255.0f + 0.5f);
}

/* a round in this many ticks counts as firing (the slowest automatic, 6 rounds a second, fires every 5) */
#define FIRING_TICKS 5

/* each controller port's last weapon and its last round's game time (a new round = a kick) */
static struct
{
	long weapon_index;
	long last_fired;
	unsigned long shots;
	int logged;
	/* the charge last frame, and whether the last round was a fully charged one (the plasma pistol's blast: the
	charge is gone by the frame its round is seen) */
	float last_charge;
	int charged_shot;
} rounds[MAXIMUM_LOCAL_PLAYERS];
/* the first rounds of each weapon in hand are logged (to check the counting; the rest are counted by the platform) */
#define ROUNDS_LOGGED 5

/* a weapon definition's kind from its tag's numbers (and its first trigger, NULL: none) */
static enum ae_ds5_weapon_kind definition_kind(long definition_index, float *rate_of_fire,
	struct weapon_trigger_definition **first_trigger)
{
	struct weapon_definition *definition = weapon_definition_get(definition_index);
	struct weapon_trigger_definition *first = NULL;
	float rate = 0.0f, charging = 0.0f;
	int projectiles = 0;
	long trigger;

	for (trigger = 0; trigger < definition->weapon.triggers.count; trigger++)
	{
		struct weapon_trigger_definition *trigger_definition =
			TAG_BLOCK_GET_ELEMENT(&definition->weapon.triggers, trigger, struct weapon_trigger_definition);

		if (trigger == 0)
		{
			first = trigger_definition;
			rate = trigger_definition->initial_rate_of_fire > trigger_definition->final_rate_of_fire ?
				trigger_definition->initial_rate_of_fire : trigger_definition->final_rate_of_fire;
		}
		if (trigger_definition->charging_time > charging)
			charging = trigger_definition->charging_time;
		if (trigger_definition->projectile.index != NONE)
			projectiles++;
	}
	*rate_of_fire = rate;
	if (first_trigger)
		*first_trigger = first;
	return ae_ds5_weapon_kind(ae_burst_rifle_definition(definition_index),
		definition->weapon.weapon_type == _weapon_type_needler, rate, charging,
		first ? first->heat_generated_per_round : 0.0f, definition->weapon.zoom_level_count, projectiles == 0,
		definition->weapon.weapon_type == _weapon_type_plasma_rifle);
}

/* (once a map) every weapon tag's numbers and the kind they give: the data the kinds are checked against */
static void weapon_catalogue(void)
{
	static long last_time = -1;
	struct tag_iterator iterator;
	long tag_index;
	long now = game_time_get();

	if (last_time >= 0 && now >= last_time)
	{
		last_time = now;
		return;
	}
	last_time = now;
	tag_iterator_new(&iterator, WEAPON_DEFINITION_TAG);
	while ((tag_index = tag_iterator_next(&iterator)) != NONE)
	{
		struct weapon_definition *definition = weapon_definition_get(tag_index);
		struct weapon_trigger_definition *first = NULL;
		float rate = 0.0f;
		enum ae_ds5_weapon_kind kind = definition_kind(tag_index, &rate, &first);
		char const *name = tag_get_name(tag_index);

		int magazine = first && first->magazine_index >= 0 && first->magazine_index < definition->weapon.magazines.count ?
			TAG_BLOCK_GET_ELEMENT(&definition->weapon.magazines, first->magazine_index,
				struct weapon_magazine_definition)->rounds_loaded_maximum : -1;

		platform_log("dualsense: weapon tag %s: triggers %d, rate %.2f/s, charging %.2f s, heat %.3f a round, "
			"zoom levels %d, type %d, magazine holds %d -> %s (%s)", name ? name : "?",
			(int)definition->weapon.triggers.count, (double)rate, first ? (double)first->charging_time : 0.0,
			first ? (double)first->heat_generated_per_round : 0.0, (int)definition->weapon.zoom_level_count,
			(int)definition->weapon.weapon_type, magazine, ae_ds5_weapon_kind_name(kind), ae_ds5_profile_feel(kind));
	}
}

/* the weapon in hand now: its kind from the tag's numbers, its heat, charge, firing and magazine */
static void weapon_state(long weapon_index, struct ae_ds5_weapon_state *state, char const **name)
{
	long definition_index;
	struct weapon_datum *weapon;
	struct weapon_definition *definition;
	struct weapon_trigger_definition *first = NULL;
	float rate = 0.0f;
	long trigger;
	long now = game_time_get();

	memset(state, 0, sizeof(*state));
	*name = "none";
	if (weapon_index == NONE)
		return;
	weapon = weapon_get(weapon_index);
	definition_index = weapon->definition_index;
	definition = weapon_definition_get(definition_index);
	*name = tag_get_name(definition_index);
	if (!*name)
		*name = "?";
	state->kind = definition_kind(definition_index, &rate, &first);
	state->rate_of_fire = rate;
	/* (the charge: a trigger charging counts its timer down from charging_time) */
	for (trigger = 0; trigger < definition->weapon.triggers.count && trigger < 2; trigger++)
	{
		struct weapon_trigger_definition *trigger_definition =
			TAG_BLOCK_GET_ELEMENT(&definition->weapon.triggers, trigger, struct weapon_trigger_definition);

		if (weapon->weapon.triggers[trigger].state == _trigger_charged)
			state->charge = 1.0f;
		else if (weapon->weapon.triggers[trigger].state == _trigger_charging && trigger_definition->charging_time > 0.0f &&
			state->charge < 1.0f)
		{
			float left = (float)weapon->weapon.triggers[trigger].state_timer /
				(trigger_definition->charging_time * (float)TICKS_PER_SECOND);
			float charge = 1.0f - (left < 0.0f ? 0.0f : left > 1.0f ? 1.0f : left);

			if (charge > state->charge)
				state->charge = charge;
		}
	}
	state->heat = weapon->weapon.heat;
	/* (the game's own overheated state: its flag, which weapons.c clears as the heat falls under the recovery
	threshold, only while there is heat; never a heat level of ours) */
	state->overheated = TEST_FLAG(weapon->weapon.flags, _weapon_overheated_bit) && weapon->weapon.heat > 0.0f ? 1 : 0;
	state->firing = weapon->weapon.game_time_last_fired > 0 && now - weapon->weapon.game_time_last_fired <= FIRING_TICKS;
	/* (switching weapons: the readying and putting-away states, or the owner asked to switch) */
	state->readying = weapon->weapon.state == _weapon_state_ready || weapon->weapon.state == _weapon_state_put_away ||
		TEST_FLAG(weapon->weapon.control_flags, _weapon_control_user_switching_weapons_bit);
	state->reloading = weapon->weapon.state == _weapon_state_primary_reload ||
		weapon->weapon.state == _weapon_state_secondary_reload;
	/* (empty: a battery spent, as the game's weapon_useful reads it, or a magazine that holds rounds with none
	loaded; the energy weapons' magazines hold none (rounds_loaded_maximum 0), as weapon_can_be_fired skips them) */
	state->empty = weapon->weapon.age >= 1.0f;
	if (first && first->magazine_index >= 0 && first->magazine_index < 2 &&
		first->magazine_index < definition->weapon.magazines.count)
	{
		struct weapon_magazine const *magazine = &weapon->weapon.magazines[first->magazine_index];
		struct weapon_magazine_definition const *magazine_definition = TAG_BLOCK_GET_ELEMENT(
			&definition->weapon.magazines, first->magazine_index, struct weapon_magazine_definition);

		if (magazine_definition->rounds_loaded_maximum > 0)
		{
			if (magazine->state == _magazine_reloading)
				state->reloading = 1;
			if (magazine->rounds_loaded <= 0)
				state->empty = 1;
		}
	}
}

/* each controller port's powerup memory: the shield's last value and when it last rose, the juggernaut's start */
static struct
{
	float shield;
	long rose_at;
	int juggernaut;
	long juggernaut_since;
} powers[MAXIMUM_LOCAL_PLAYERS];
/* an overshield that rose in this many ticks is charging */
#define SHIELD_RISING_TICKS 10

static void powerup_state(short gamepad_index, long player_index, struct ae_ds5_powerups *powerups)
{
	struct player_datum *player = player_get(player_index);
	long now = game_time_get();
	int juggernaut = ae_juggernaut_player_is(player_index) ? 1 : 0;

	memset(powerups, 0, sizeof(*powerups));
	powerups->ticks = now;
	powerups->juggernaut_ticks = -1;
	if (player->unit_index != NONE)
	{
		struct unit_datum *unit = unit_get(player->unit_index);

		powerups->shield = unit->object.shield_vitality;
		powerups->camo = unit->unit.active_camouflage;
	}
	if (powerups->shield > powers[gamepad_index].shield + 0.0001f || now < powers[gamepad_index].rose_at)
		powers[gamepad_index].rose_at = now;
	powers[gamepad_index].shield = powerups->shield;
	powerups->shield_rising = powerups->shield > 1.0f && now - powers[gamepad_index].rose_at <= SHIELD_RISING_TICKS;
	if (juggernaut && (!powers[gamepad_index].juggernaut || now < powers[gamepad_index].juggernaut_since))
		powers[gamepad_index].juggernaut_since = now;
	powers[gamepad_index].juggernaut = juggernaut;
	if (juggernaut)
		powerups->juggernaut_ticks = now - powers[gamepad_index].juggernaut_since;
}

/* each controller port's shield memory: last frame's shield, when it last broke */
static struct
{
	float vitality;
	long popped_at;
	int known;
} shields[MAXIMUM_LOCAL_PLAYERS];

/* LIGHTBAR SHIELD: the player's shield meter as the HUD draws it (the unit's own HUD interface's meter colours;
a guest reads the shield the host sent) */
static void shield_lightbar(short gamepad_index, long player_index, unsigned char rgb[3], char *text, size_t text_size)
{
	struct player_datum *player = player_get(player_index);
	struct ae_ds5_shield shield;
	long now = game_time_get();

	memset(&shield, 0, sizeof(shield));
	shield.ticks = now;
	shield.popped_ticks = -1;
	shield.pop_flash = ae_hit_feedback_active();
	if (player->unit_index == NONE)
		shield.dead = 1;
	else
	{
		struct unit_datum *unit = unit_get(player->unit_index);
		long hud_index = unit_definition_get_active_hud_index(unit_definition_get(unit->definition_index),
			hud_split_screen_layout());

		shield.dead = TEST_FLAG(unit->object.damage_flags, _object_dead_bit) ? 1 : 0;
		shield.vitality = unit->object.shield_vitality;
		if (hud_index != NONE)
		{
			struct meter_hud_element_definition const *meter =
				&unit_hud_interface_definition_get(hud_index)->shield_meter.meter;

			shield.min_color = meter->min_color;
			shield.max_color = meter->max_color;
			shield.flash_color = meter->flash_color;
			shield.empty_color = meter->empty_color;
		}
	}
	if (!shields[gamepad_index].known || now < shields[gamepad_index].popped_at)
		shields[gamepad_index].popped_at = -1000;
	if (shields[gamepad_index].known && shields[gamepad_index].vitality > 0.0f && shield.vitality <= 0.0f &&
		!shield.dead)
		shields[gamepad_index].popped_at = now;
	shields[gamepad_index].vitality = shield.vitality;
	shields[gamepad_index].known = 1;
	if (now - shields[gamepad_index].popped_at < 1000)
		shield.popped_ticks = now - shields[gamepad_index].popped_at;
	ae_ds5_lightbar_shield(rgb, &shield, text, text_size);
}

/* the weapon the player fires: in a vehicle's seat, the vehicle's weapon for a gunner's seat (as the HUD's,
hud_weapon.c), none for any other; on foot, the one in hand */
static long seated_weapon(long unit_index, long weapon_index)
{
	struct unit_datum *unit;
	struct unit_datum *parent;
	struct unit_seat *seat;

	if (unit_index == NONE)
		return weapon_index;
	unit = unit_get(unit_index);
	if (unit->object.parent_object_index == NONE || unit->unit.parent_seat_index == NONE)
		return weapon_index;
	parent = unit_get(unit->object.parent_object_index);
	if (unit->unit.parent_seat_index >= unit_definition_get(parent->definition_index)->unit.seats.count)
		return NONE;
	seat = TAG_BLOCK_GET_ELEMENT(&unit_definition_get(parent->definition_index)->unit.seats,
		unit->unit.parent_seat_index, struct unit_seat);
	if (!TEST_FLAG(seat->flags, _unit_seat_gunner_bit))
		return NONE;
	return unit_inventory_get_weapon(unit->object.parent_object_index, parent->unit.current_weapon_index);
}

void ae_ds5_player(short local_player_index, long weapon_index, short zoom_level)
{
	struct ae_ds5_wish wish;
	struct ae_ds5_weapon_state state;
	struct ae_ds5_powerups powerups;
	unsigned char base[3];
	unsigned char lightbar[3];
	char lightbar_text[64];
	char left_power[80];
	long player_index;
	short gamepad_index;
	real_rgb_color color;
	char const *name;
	char const *slash;
	char right_text[96];
	char const *left_text;

	struct ae_ds5_settings settings;

	if (!ae_ds5_on())
		return;
	settings = ae_ds5_settings_get();
	if (settings.log)
		weapon_catalogue();
	player_index = local_player_get_player_index(local_player_index);
	if (player_index == NONE)
		return;
	gamepad_index = player_get(player_index)->local_player_index;
	/* (only a port whose pad is a DualSense the platform can drive: nothing to do for any other pad; with
	debug.ds5_log, any port, so a machine without one, the build box, logs what a DualSense would be sent) */
	if (gamepad_index < 0 || gamepad_index >= MAXIMUM_LOCAL_PLAYERS ||
		(!ae_ds5_port_active(gamepad_index) && !settings.log))
		return;
	weapon_index = seated_weapon(player_get(player_index)->unit_index, weapon_index);
	memset(&wish, 0, sizeof(wish));
	wish.player_lights = ae_ds5_player_lights(local_player_index + 1);
	if (game_engine_running())
		game_engine_player_get_change_color(&color, player_index);
	else
	{
		/* (a campaign: the Chief's armour green) */
		color.red = 0.25f;
		color.green = 0.45f;
		color.blue = 0.15f;
	}
	base[0] = channel(color.red);
	base[1] = channel(color.green);
	base[2] = channel(color.blue);
	powerup_state(gamepad_index, player_index, &powerups);
	if (settings.shield)
		shield_lightbar(gamepad_index, player_index, lightbar, lightbar_text, sizeof(lightbar_text));
	else
		ae_ds5_lightbar(lightbar, base, &powerups, lightbar_text, sizeof(lightbar_text));
	wish.red = lightbar[0];
	wish.green = lightbar[1];
	wish.blue = lightbar[2];
	weapon_state(weapon_index, &state, &name);
	ae_ds5_right_held(wish.right, &state, settings.strength, right_text, sizeof(right_text));
	/* (a round: the weapon's last-fired time moved, the weapon the same as last frame) */
	if (weapon_index != NONE)
	{
		long last_fired = weapon_get(weapon_index)->weapon.game_time_last_fired;

		if (rounds[gamepad_index].weapon_index != weapon_index)
		{
			rounds[gamepad_index].logged = 0;
			rounds[gamepad_index].charged_shot = 0;
		}
		else if (rounds[gamepad_index].last_fired != last_fired)
		{
			rounds[gamepad_index].shots++;
			rounds[gamepad_index].charged_shot = rounds[gamepad_index].last_charge >= 0.95f;
			if (settings.log && rounds[gamepad_index].logged < ROUNDS_LOGGED)
			{
				rounds[gamepad_index].logged++;
				platform_log("dualsense: port %d round at tick %d (%s%s)", gamepad_index + 1, (int)last_fired,
					ae_ds5_weapon_kind_name(state.kind), rounds[gamepad_index].charged_shot ? ", charged" : "");
			}
		}
		rounds[gamepad_index].last_fired = last_fired;
	}
	rounds[gamepad_index].weapon_index = weapon_index;
	rounds[gamepad_index].last_charge = state.charge;
	wish.shots = rounds[gamepad_index].shots;
	wish.kick_ms = ae_ds5_right_kick(wish.kick, state.kind, rounds[gamepad_index].charged_shot, settings.strength);
	if (ae_ds5_left_powerup(wish.left, &powerups, settings.strength, left_power, sizeof(left_power)))
		left_text = left_power;
	else
		left_text = ae_ds5_left_effect(wish.left, zoom_level != NONE, settings.strength);
	if (!settings.log)
	{
		ae_ds5_wish_set(gamepad_index, &wish);
		return;
	}
	slash = strrchr(name, '\\');
	snprintf(wish.what, sizeof(wish.what), "%s (%s, %.1f/s, %s): R2 %s, L2 %s; lightbar %s", slash ? slash + 1 : name,
		ae_ds5_weapon_kind_name(state.kind), (double)state.rate_of_fire, ae_ds5_profile_feel(state.kind), right_text,
		left_text, lightbar_text);
	if (strcmp(last_wanted[gamepad_index], wish.what))
	{
		strcpy(last_wanted[gamepad_index], wish.what);
		platform_log("dualsense: player on port %d wants %s", gamepad_index + 1, wish.what);
	}
	ae_ds5_wish_set(gamepad_index, &wish);
}
