/*
AE_DS5_GAME.C

Arena Evolved: each local player's DualSense wish (ae_ds5_game.h), for a controller port whose pad is a DualSense
the platform side can drive (port/linux/src/ae_ds5.c):
- the lightbar in the player's colour (the team's in a team game, the armour's otherwise; a campaign's Chief green),
  with the powerups (ae_ds5_lightbar), and the player lights the split-screen number;
- R2 by the weapon in hand, its profile (ae_ds5_rules.c's table) from the tag's own numbers (rate of fire, charging,
  heat a round, zoom levels, weapon type; AE's battle rifle by its own check, the weapon packs' by their pack) and the weapon's state each frame
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
#include "ae_weapon_pack.h"
#include "ae_weapon_slots.h"
#include "ae_juggernaut.h"
#include "units/units.h"
#include "units/unit_definitions.h"
#include "interface/hud_draw.h"
#include "interface/unit_hud_interface_definition.h"
#include "ae_hit_feedback.h"
#include "interface/player_ui.h"
#include "models/model_animation_definitions.h"
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

/* each controller port's memory of last frame: the weapon and its states, the grenade's throw; and each trigger's
last event (its kick and count: the platform kicks on a change of the count) with the last rumble pulse */
static struct
{
	long weapon_index;
	long last_fired;
	int logged;
	/* the charge last frame, and whether the last round was a fully charged one (the plasma pistol's blast: the
	charge is gone by the frame its round is seen) */
	float last_charge;
	int rounds_loaded;
	int reloading, readying, chambering;
	char grenade_throw_state;
	long overcharge_at;
	unsigned char kick_right[AE_DS5_EFFECT_SIZE], kick_left[AE_DS5_EFFECT_SIZE];
	int kick_right_ms, kick_left_ms;
	unsigned long kicks_right, kicks_left, rumbles;
	struct ae_ds5_rumble rumble;
} rounds[MAXIMUM_LOCAL_PLAYERS];
/* the first rounds of each weapon in hand are logged (to check the counting; the rest are counted by the platform) */
#define ROUNDS_LOGGED 20

/* the weapon pack a weapon definition comes from on the loaded map: its pack's slot (ae_weapon_slots.h), the stable
key (the packs' weapons are told apart by the pack, never by their numbers) */
static int definition_pack(long definition_index)
{
	switch (ae_weapon_pack_definition_slot(definition_index))
	{
	case AE_WEAPON_SLOT_BATTLE_RIFLE:
		return AE_DS5_PACK_BATTLE_RIFLE;
	case AE_WEAPON_SLOT_SMG:
		return AE_DS5_PACK_SMG;
	case AE_WEAPON_SLOT_SMG_SILENCED:
		return AE_DS5_PACK_SMG_SILENCED;
	case AE_WEAPON_SLOT_CARBINE:
	/* (0.7.0: the DMR and the EVO RIFLE, single-shot precision rifles: the carbine's trigger) */
	case AE_WEAPON_SLOT_DMR:
	case AE_WEAPON_SLOT_EVO_RIFLE:
		return AE_DS5_PACK_CARBINE;
	case AE_WEAPON_SLOT_AR_SILENCED:
		return AE_DS5_PACK_AR_SILENCED;
	default:
		return AE_DS5_PACK_NONE;
	}
}

/* a weapon definition's kind from its tag's numbers and its pack (and its first trigger, NULL: none) */
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
	return ae_ds5_weapon_kind(ae_burst_rifle_definition(definition_index), definition_pack(definition_index),
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

		struct weapon_magazine_definition const *magazine_definition = first && first->magazine_index >= 0 &&
			first->magazine_index < definition->weapon.magazines.count ? TAG_BLOCK_GET_ELEMENT(
				&definition->weapon.magazines, first->magazine_index, struct weapon_magazine_definition) : NULL;
		int magazine = magazine_definition ? magazine_definition->rounds_loaded_maximum : -1;

		platform_log("dualsense: weapon tag %s: triggers %d, rate %.2f/s, charging %.2f s, heat %.3f a round, "
			"zoom levels %d, type %d, magazine holds %d (chamber %.2f s, reload %.2f s), pack slot %d -> %s (%s)",
			name ? name : "?",
			(int)definition->weapon.triggers.count, (double)rate, first ? (double)first->charging_time : 0.0,
			first ? (double)first->heat_generated_per_round : 0.0, (int)definition->weapon.zoom_level_count,
			(int)definition->weapon.weapon_type, magazine,
			magazine_definition ? (double)magazine_definition->chamber_time : 0.0,
			magazine_definition ? (double)magazine_definition->reload_time : 0.0,
			(int)ae_weapon_pack_definition_slot(tag_index), ae_ds5_weapon_kind_name(kind),
			ae_ds5_profile_feel(kind));
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

/* the weapon the player fires: in a vehicle's seat, the vehicle's weapon when the vehicle says this unit is its
gunner (its gunner_object_index: the ghost's driver, the warthog's gunner), none in any other seat; on foot, the one
in hand */
static long seated_weapon(long unit_index, long weapon_index)
{
	struct unit_datum *unit;
	struct unit_datum *parent;

	if (unit_index == NONE)
		return weapon_index;
	unit = unit_get(unit_index);
	if (unit->object.parent_object_index == NONE || unit->unit.parent_seat_index == NONE)
		return weapon_index;
	parent = unit_try_and_get(unit->object.parent_object_index);
	if (!parent || parent->unit.gunner_object_index != unit_index)
		return NONE;
	return unit_inventory_get_weapon(unit->object.parent_object_index, parent->unit.current_weapon_index);
}

/* the shotgun's pump: no chambering state of its own (its magazine's chamber time is 0), it is in the recoil animation;
the pump is that animation's key frame (the frame the game's own key frame events fire on), or its end if the tag has
no key frame. Only the shotgun's weapon type: the CE sniper rifle is no bolt rifle, and its tag has no chambering */
static int pump_frame(struct weapon_datum const *weapon)
{
	struct weapon_definition *definition = weapon_definition_get(weapon->definition_index);
	struct animation_graph *graph;
	struct animation const *animation;
	short key;

	if (definition->weapon.weapon_type != _weapon_type_shotgun || weapon->weapon.state != _weapon_state_primary_recoil ||
		definition->object.animation_graph.index == NONE || weapon->object.animation.state.index == NONE)
		return 0;
	graph = animation_graph_definition_get(definition->object.animation_graph.index);
	if (weapon->object.animation.state.index < 0 || weapon->object.animation.state.index >= graph->animations.count)
		return 0;
	animation = TAG_BLOCK_GET_ELEMENT(&graph->animations, weapon->object.animation.state.index, struct animation);
	key = animation->private_key_frame_index;
	if (key <= 0 || key >= animation->frame_count)
		key = animation->frame_count - 1;
	return weapon->object.animation.state.frame_index >= key;
}

/* an event on a trigger (right 1, left 0): its kick and rumble kept until the next */
static void event(short gamepad_index, int right, enum ae_ds5_weapon_kind kind, enum ae_ds5_event what, int strength,
	int rumble_on, int log, char const *name)
{
	unsigned char effect[AE_DS5_EFFECT_SIZE];
	struct ae_ds5_rumble rumble;
	int ms = ae_ds5_event(effect, &rumble, kind, what, strength);

	if (ms > 0)
	{
		if (right)
		{
			memcpy(rounds[gamepad_index].kick_right, effect, sizeof(effect));
			rounds[gamepad_index].kick_right_ms = ms;
			rounds[gamepad_index].kicks_right++;
		}
		else
		{
			memcpy(rounds[gamepad_index].kick_left, effect, sizeof(effect));
			rounds[gamepad_index].kick_left_ms = ms;
			rounds[gamepad_index].kicks_left++;
		}
	}
	if (rumble_on && rumble.ms > 0 && (rumble.left || rumble.right))
	{
		rounds[gamepad_index].rumble = rumble;
		rounds[gamepad_index].rumbles++;
	}
	if (log && rounds[gamepad_index].logged < ROUNDS_LOGGED)
	{
		rounds[gamepad_index].logged++;
		platform_log("dualsense: port %d %s (%s): %s kick %d ms, rumble left %d right %d for %d ms", gamepad_index + 1,
			name, ae_ds5_weapon_kind_name(kind), right ? "R2" : "L2", ms, rumble_on ? rumble.left : 0,
			rumble_on ? rumble.right : 0, rumble_on ? rumble.ms : 0);
	}
}

/* a pad that is not a DualSense this machine drives (an Xbox pad, an 8BitDo, a Switch Pro): the battle rifle's
round pulse on its motors through the game's rumble, the one a DualSense gets (the rifle's own trigger damage effect
rumbles only the light motor, at a quarter: owner 10-17, "br doesn't have haptics on xinput devices"). Every other
weapon keeps its tag's rumble alone. Its own memory of the rounds, apart from the DualSense ports' */
static void other_pad_battle_rifle(short local_player_index, long weapon_index)
{
	static long seen_weapon[MAXIMUM_LOCAL_PLAYERS] = { NONE, NONE, NONE, NONE };
	static long seen_fired[MAXIMUM_LOCAL_PLAYERS];
	long player_index = local_player_get_player_index(local_player_index);
	short gamepad_index;
	long last_fired;

	if (player_index == NONE)
		return;
	gamepad_index = player_get(player_index)->local_player_index;
	if (gamepad_index < 0 || gamepad_index >= MAXIMUM_LOCAL_PLAYERS || ae_ds5_port_active(gamepad_index))
		return;
	weapon_index = seated_weapon(player_get(player_index)->unit_index, weapon_index);
	if (weapon_index == NONE)
	{
		seen_weapon[gamepad_index] = NONE;
		return;
	}
	last_fired = weapon_get(weapon_index)->weapon.game_time_last_fired;
	if (seen_weapon[gamepad_index] == weapon_index && seen_fired[gamepad_index] != last_fired &&
		!player_ui_rumble_disabled(local_player_index))
	{
		struct ae_ds5_weapon_state state;
		struct ae_ds5_rumble rumble;
		unsigned char effect[AE_DS5_EFFECT_SIZE];
		char const *name;

		weapon_state(weapon_index, &state, &name);
		memset(&rumble, 0, sizeof(rumble));
		if (state.kind == AE_DS5_WEAPON_BURST && ae_ds5_event(effect, &rumble, state.kind, AE_DS5_EVENT_SHOT, 100) >= 0 &&
			rumble.ms > 0)
		{
			ae_ds5_pulse(gamepad_index, rumble.left, rumble.right, rumble.ms);
		}
	}
	seen_weapon[gamepad_index] = weapon_index;
	seen_fired[gamepad_index] = last_fired;
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
	long now = game_time_get();
	int rumble_on, winding, grenades;

	other_pad_battle_rifle(local_player_index, weapon_index);
	if (!ae_ds5_on())
		return;
	/* (the player's own DUALSENSE settings: their controller's port is their local player's) */
	settings = ae_ds5_settings_port(local_player_index);
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
	/* (black armour: the shield's colours, as LIGHTBAR SHIELD; a dim white read as stuck, owner 10-15) */
	if (settings.shield || ae_ds5_lightbar_black(base))
		shield_lightbar(gamepad_index, player_index, lightbar, lightbar_text, sizeof(lightbar_text));
	else
		ae_ds5_lightbar(lightbar, base, &powerups, lightbar_text, sizeof(lightbar_text));
	wish.red = lightbar[0];
	wish.green = lightbar[1];
	wish.blue = lightbar[2];
	weapon_state(weapon_index, &state, &name);
	state.ticks = now;
	ae_ds5_right_held(wish.right, &state, settings.strength, right_text, sizeof(right_text));
	/* the events, each at the game's own moment: a round (the weapon's last-fired time moved), the reload's magazine
	(or shell) going in (rounds loaded rising while reloading), the weapon readied (its readying over), the post-shot
	chambering (the shotgun's pump: the weapon's or its magazine's chambering state begun), a grenade leaving the hand
	(its throw's ending begun). The same weapon as last frame for the weapon's; a switch is only a settle */
	rumble_on = settings.triggers && !player_ui_rumble_disabled(local_player_index);
	if (weapon_index != NONE)
	{
		struct weapon_datum *weapon = weapon_get(weapon_index);
		long last_fired = weapon->weapon.game_time_last_fired;
		int chambering = weapon->weapon.state == _weapon_state_primary_chamber ||
			weapon->weapon.state == _weapon_state_secondary_chamber ||
			weapon->weapon.magazines[0].state == _magazine_chambering || pump_frame(weapon);
		int rounds_loaded = weapon->weapon.magazines[0].rounds_loaded;

		if (rounds[gamepad_index].weapon_index != weapon_index)
		{
			rounds[gamepad_index].logged = 0;
			rounds[gamepad_index].readying = 1;
		}
		else
		{
			if (rounds[gamepad_index].last_fired != last_fired)
			{
				int charged = rounds[gamepad_index].last_charge >= 0.95f;

				event(gamepad_index, 1, state.kind, charged ? AE_DS5_EVENT_SHOT_CHARGED : AE_DS5_EVENT_SHOT,
					settings.strength, rumble_on, settings.log, charged ? "charged round" : "round");
				if (charged && ae_ds5_kind_covenant(state.kind))
					rounds[gamepad_index].overcharge_at = now;
			}
			if ((state.reloading || rounds[gamepad_index].reloading) && rounds_loaded > rounds[gamepad_index].rounds_loaded)
				event(gamepad_index, 1, state.kind, AE_DS5_EVENT_MAG_IN, settings.strength, rumble_on, settings.log,
					"magazine in");
			if (chambering && !rounds[gamepad_index].chambering)
				event(gamepad_index, 1, state.kind, AE_DS5_EVENT_CHAMBER, settings.strength, rumble_on, settings.log,
					"chambered");
			if (rounds[gamepad_index].readying && !state.readying)
				event(gamepad_index, 1, state.kind, AE_DS5_EVENT_SETTLE, settings.strength, rumble_on, settings.log,
					"readied");
		}
		rounds[gamepad_index].last_fired = last_fired;
		rounds[gamepad_index].rounds_loaded = rounds_loaded;
		rounds[gamepad_index].chambering = chambering;
		rounds[gamepad_index].reloading = state.reloading;
		rounds[gamepad_index].readying = state.readying;
	}
	rounds[gamepad_index].weapon_index = weapon_index;
	rounds[gamepad_index].last_charge = state.charge;
	/* (a charge weapon charging: the motors rumble with it) */
	{
		struct ae_ds5_rumble charge_rumble;

		if (rumble_on && ae_ds5_charge_rumble(state.kind, state.charge, now, &charge_rumble))
		{
			ae_ds5_rumble_scaled(&charge_rumble, settings.strength);
			rounds[gamepad_index].rumble = charge_rumble;
			rounds[gamepad_index].rumbles++;
		}
	}
	/* L2: a grenade's throw, and whether there are grenades to throw */
	{
		long unit_index = player_get(player_index)->unit_index;
		struct unit_datum *unit = unit_index != NONE ? unit_get(unit_index) : NULL;
		char throw_state = unit ? unit->unit.grenade_throw_state : (char)_unit_grenade_throw_idle;
		short grenade_type = unit ? (short)unit->unit.current_grenade_index : 0;

		winding = throw_state == _unit_grenade_throw_wind_up || throw_state == _unit_grenade_throw_in_hand;
		grenades = unit && grenade_type >= 0 ? unit_get_grenade_count(unit_index, grenade_type) : 0;
		if (throw_state == _unit_grenade_throw_ending && rounds[gamepad_index].grenade_throw_state != throw_state)
			event(gamepad_index, 0, state.kind, grenade_type == _unit_grenade_covenant_plasma ?
				AE_DS5_EVENT_GRENADE_PLASMA : AE_DS5_EVENT_GRENADE_FRAG, settings.strength, rumble_on, settings.log,
				"grenade thrown");
		rounds[gamepad_index].grenade_throw_state = throw_state;
	}
	memcpy(wish.kick_right, rounds[gamepad_index].kick_right, sizeof(wish.kick_right));
	wish.kick_right_ms = rounds[gamepad_index].kick_right_ms;
	wish.kicks_right = rounds[gamepad_index].kicks_right;
	memcpy(wish.kick_left, rounds[gamepad_index].kick_left, sizeof(wish.kick_left));
	wish.kick_left_ms = rounds[gamepad_index].kick_left_ms;
	wish.kicks_left = rounds[gamepad_index].kicks_left;
	wish.rumble = rounds[gamepad_index].rumble;
	wish.rumbles = rounds[gamepad_index].rumbles;
	/* (the plasma pistol's overcharged release: the lightbar's green flash, its only Covenant colour) */
	if (settings.lights && rounds[gamepad_index].overcharge_at > 0 &&
		ae_ds5_overcharge_flash(lightbar, now - rounds[gamepad_index].overcharge_at))
	{
		wish.red = lightbar[0];
		wish.green = lightbar[1];
		wish.blue = lightbar[2];
		snprintf(lightbar_text, sizeof(lightbar_text), "overcharge flash (green)");
	}
	if (ae_ds5_left_powerup(wish.left, &powerups, settings.strength, left_power, sizeof(left_power)))
		left_text = left_power;
	else
		left_text = ae_ds5_left_effect(wish.left, zoom_level != NONE, grenades, winding, settings.strength);
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
