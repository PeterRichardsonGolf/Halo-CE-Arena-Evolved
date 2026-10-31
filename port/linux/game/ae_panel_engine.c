/*
AE_PANEL_ENGINE.C

HUD STYLE AE's panels' engine reader (ae_panel_engine.h). Sources (cited as
of this commit):
- the weapon in hand or, in a gunner's seat, the vehicle's: as
  hud_render_weapon_interface (hud_weapon.c:2012) finds it; its state from
  weapon_build_weapon_interface_state (weapons.c:1272), the struct
  weapon_interface_state of hud_weapon.c:223-241;
- a battery weapon: no magazines, or a trigger that ages (weapons.c:1320,
  weapon_set_current_amount's test); heat: a trigger with heat per round
  (weapon_definitions.h:153); overheated: the weapon's flag 0
  (weapons.c:1287);
- the low-ammo test: render_weapon_hud's (hud_weapon.c:1429) on the weapon
  HUD's own flash_cutoffs (its parent's when it says so, :1384), and the
  spec's "under half full" (14.15 part 5);
- grenades: unit grenade_counts and current_grenade_index (units.h), as
  render_grenade_hud (hud_weapon.c:553) reads them;
- shield / health: hud_render_unit_interface's tests (hud_unit.c:968, low
  under 0.25; health 0.25);
- camo: players.h powerup_durations; overshield: damage.c:2259-2293.
*/

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cseries/cseries.h"
#include "math/real_math.h"
#include "game/game.h"
#include "game/game_engine.h"
#include "game/players.h"
#include "interface/hud_draw.h"
#include "interface/weapon_hud_interface_definition.h"
#include "items/weapon_definitions.h"
#include "items/weapons.h"
#include "memory/data.h"
#include "objects/damage.h"
#include "objects/objects.h"
#include "render/render.h"
#include "tag_files/tag_files.h"
#include "tag_files/tag_groups.h"
#include "units/unit_definitions.h"
#include "units/units.h"

#include "ae_burst_rifle.h"
#include "ae_hud_digits.h"
#include "ae_hud_style.h"
#include "ae_panel_engine.h"
#include "ae_score_strip.h"

int config_boolean(const char *name);
const char *config_string(const char *name);
unsigned long config_changes(void);
static boolean reduced_motion_on(void);
boolean network_game_distributed_client(void);
void platform_log(char const *format, ...);
long game_time_get(void);
void hud_item_timers_ae_place(short *inset, long *baseline);
boolean hud_item_timers_ae_corner(short v, rectangle2d *b);
boolean hud_item_timers_ae_meters(short v, rectangle2d *b);

/* the weapon's state for the HUD, as weapons.c fills it (each file that uses it has its own copy: hud_weapon.c:223,
weapons.c:333) */
struct weapon_interface_magazine_state
{
	boolean reloading;
	boolean can_fire;
	short rounds_loaded;
	short rounds_loaded_maximum;
	short rounds_remaining;
	short rounds_remaining_maximum;
};

struct weapon_interface_state
{
	real heat;
	real age;
	boolean overheated;
	byte pad09;
	short magazine_count;
	struct weapon_interface_magazine_state magazines[2];
};

typedef char ae_weapon_interface_state_size[sizeof(struct weapon_interface_state) == 0x20 ? 1 : -1];

#define AE_WEAPON_HUD_INTERFACE_TAG 'wphi'   /* hud_weapon.c's WEAPON_HUD_INTERFACE_TAG */
#define AE_HUD_USE_PARENT_FLASH_BIT 0         /* hud_weapon.c's _hud_use_parent_flash_parameters_bit */
#define AE_BLINK_TICKS 15                     /* the blink's half period: 0.5 s on, 0.5 s off */
#define OVERSHIELD_DECAY 0.00074074074f       /* damage.c:2285 */
#define OVERSHIELD_CHARGE 0.033333335f        /* damage.c:2261 */
#define TICKS_PER_SECOND 30.0f
/* from CE's placements to the spec's anchors (14.15 parts 2, 3), px at a 1080p single view x the view's scale:
measured on a 1920x1080 Blood Gulch frame, the HUD's top-left extent is (139.5, 83.2) where the ammo group's origin
is (150, 83), and the meters' right edge and top (1752.8, 81.0) where the right column is 1758 and the box row 83 */
#define AMMO_ORIGIN_DX 10.5f   /* (the x is then set by the corners' margin: ae_hud_corner_margin) */
#define AMMO_ORIGIN_DY 0.0f
#define SHIELD_RIGHT_DX 5.2f
#define SHIELD_TOP_DY 2.0f

/* the overshield's projection per player (by its absolute index): kept from rising unless the vitality rose (a slot
for each of the most players a game has, so no two players share one) */
#define PLAYER_SLOTS HALO_PORT_MAXIMUM_NETWORK_PLAYERS
static struct { real seconds, shield; long tick; } overshield[PLAYER_SLOTS];

static struct unit_datum *player_unit(long player_index, long *unit_index, struct player_datum **player)
{
	struct player_datum *p;

	if (player_index == NONE)
		return NULL;
	p = player_get(player_index);
	if (!p || p->unit_index == NONE)
		return NULL;
	*unit_index = p->unit_index;
	*player = p;
	return unit_try_and_get(p->unit_index);
}

static long view_player_index(short local_player_index)
{
	if (local_player_index < 0 || local_player_index >= MAXIMUM_LOCAL_PLAYERS)
		return NONE;
	return local_player_get_player_index(local_player_index);
}

/* the weapon CE's panel shows: the unit's, or in a gunner's seat the vehicle's (hud_render_weapon_interface) */
static long panel_weapon(long unit_index, boolean *in_seat, long *vehicle_index)
{
	struct unit_datum *unit = unit_get(unit_index);
	long weapon_index = unit_inventory_get_weapon(unit_index, unit->unit.current_weapon_index);

	*in_seat = FALSE;
	*vehicle_index = NONE;
	if (unit->object.parent_object_index != NONE && unit->unit.parent_seat_index != NONE)
	{
		struct unit_datum *parent = unit_try_and_get(unit->object.parent_object_index);

		if (parent && weapon_index == NONE)
		{
			struct unit_definition *parent_definition = unit_definition_get(parent->definition_index);

			*in_seat = TRUE;
			*vehicle_index = unit->object.parent_object_index;
			if (unit->unit.parent_seat_index < parent_definition->unit.seats.count)
			{
				struct unit_seat *seat = TAG_BLOCK_GET_ELEMENT(&parent_definition->unit.seats,
					unit->unit.parent_seat_index, struct unit_seat);

				if (TEST_FLAG(seat->flags, _unit_seat_gunner_bit))
					weapon_index = unit_inventory_get_weapon(unit->object.parent_object_index,
						parent->unit.current_weapon_index);
			}
		}
	}
	return weapon_index;
}

static boolean name_has(char const *name, char const *part)
{
	return (boolean)(name && part && strstr(name, part) != NULL);
}

/* the stock weapons by their tags (and the pack's battle rifle): the art they borrow (14.15 part 2's table) */
static short weapon_hint(long definition_index)
{
	char const *name = tag_get_name(definition_index);

	if (ae_burst_rifle_definition(definition_index))
		return _ae_hint_battle_rifle;
	if (!name)
		return _ae_hint_none;
	if (name_has(name, "assault rifle"))
		return _ae_hint_assault_rifle;
	if (name_has(name, "\\pistol\\pistol"))
		return _ae_hint_pistol;
	if (name_has(name, "shotgun"))
		return _ae_hint_shotgun;
	if (name_has(name, "needler"))
		return _ae_hint_needler;
	if (name_has(name, "sniper rifle"))
		return _ae_hint_sniper;
	if (name_has(name, "rocket launcher"))
		return _ae_hint_rocket;
	if (name_has(name, "flamethrower"))
		return _ae_hint_flamethrower;
	if (name_has(name, "fuel rod gun"))
		return _ae_hint_fuel_rod_rounds;
	return _ae_hint_none;
}

/* the weapon HUD's flash cutoffs CE uses (its parent's when it says so) */
static struct weapon_flash_state_definition const *flash_cutoffs(struct weapon_definition const *definition)
{
	long hud_index = definition->weapon.interface_definition.hud_interface.index;
	short depth;

	for (depth = 0; hud_index != NONE && depth < 8; depth++)
	{
		struct weapon_hud_interface_definition const *hud =
			(struct weapon_hud_interface_definition const *)tag_get(AE_WEAPON_HUD_INTERFACE_TAG, hud_index);

		if (!hud)
			return NULL;
		if (!TEST_FLAG(hud->flash_cutoffs.flags, AE_HUD_USE_PARENT_FLASH_BIT) || hud->parent_hud.index == NONE)
			return &hud->flash_cutoffs;
		hud_index = hud->parent_hud.index;
	}
	return NULL;
}

/* (hud_weapon.c's weapon HUD element sizes and its heat state; each element starts with its header's state type) */
#define AE_WEAPON_HUD_STATIC_SIZE 0xb4
#define AE_WEAPON_HUD_METER_SIZE 0xb4
#define AE_WEAPON_HUD_NUMBER_SIZE 0xa0
#define AE_WEAPON_HUD_STATE_HEAT 2

static boolean block_declares_state(struct tag_block const *block, long element_size, short state)
{
	long index;

	for (index = 0; index < block->count; index++)
	{
		short const *element = (short const *)tag_block_get_element_with_size(block, index, element_size);

		if (element && element[0] == state)
			return TRUE;
	}
	return FALSE;
}

/* whether a weapon's HUD, or a parent of it, has an element of this state (classic draws only what the tag has:
hud_weapon.c, the elements' state_type) */
static boolean hud_declares_state(struct weapon_definition const *definition, short state)
{
	long hud_index = definition->weapon.interface_definition.hud_interface.index;
	short depth;

	for (depth = 0; hud_index != NONE && depth < 8; depth++)
	{
		struct weapon_hud_interface_definition const *hud =
			(struct weapon_hud_interface_definition const *)tag_get(AE_WEAPON_HUD_INTERFACE_TAG, hud_index);

		if (!hud)
			return FALSE;
		if (block_declares_state(&hud->statics, AE_WEAPON_HUD_STATIC_SIZE, state) ||
			block_declares_state(&hud->meters, AE_WEAPON_HUD_METER_SIZE, state) ||
			block_declares_state(&hud->numbers, AE_WEAPON_HUD_NUMBER_SIZE, state))
		{
			return TRUE;
		}
		hud_index = hud->parent_hud.index;
	}
	return FALSE;
}

static boolean blink_on(void)
{
	return (boolean)((game_time_get() / AE_BLINK_TICKS) % 2 == 0);
}

static void read_weapon(long weapon_index, struct ae_panel_snapshot *out)
{
	struct weapon_datum *weapon = weapon_get(weapon_index);
	struct weapon_definition *definition = weapon_definition_get(weapon->definition_index);
	struct weapon_interface_state state;
	struct weapon_flash_state_definition const *cutoffs = flash_cutoffs(definition);
	short trigger_index;

	weapon_build_weapon_interface_state(weapon_index, &state);
	out->has_weapon = TRUE;
	out->weapon_type_hint = weapon_hint(weapon->definition_index);
	out->weapon_uses_rounds = (boolean)(definition->weapon.magazines.count > 0);
	out->weapon_has_battery = (boolean)(definition->weapon.magazines.count == 0);
	for (trigger_index = 0; trigger_index < definition->weapon.triggers.count; trigger_index++)
	{
		struct weapon_trigger_definition *trigger = TAG_BLOCK_GET_ELEMENT(&definition->weapon.triggers,
			trigger_index, struct weapon_trigger_definition);

		if (trigger->heat_generated_per_round > 0.0f)
			out->weapon_has_heat = TRUE;
		if (trigger->age_generated_per_round > 0.0f)
			out->weapon_has_battery = TRUE;
	}
	out->heat = state.heat;
	out->battery = 1.0f - state.age;
	out->overheated = state.overheated;
	if (state.magazine_count > 0)
	{
		out->rounds_loaded = state.magazines[0].rounds_loaded;
		out->rounds_per_magazine = state.magazines[0].rounds_loaded_maximum;
		out->rounds_reserve = state.magazines[0].rounds_remaining;
		out->tank = state.magazines[0].rounds_loaded_maximum > 0 ?
			(real)state.magazines[0].rounds_loaded / (real)state.magazines[0].rounds_loaded_maximum : 0.0f;
		/* CE's low-ammo test (render_weapon_hud's, on the tag's cutoff) and the spec's "under half full" */
		if (cutoffs && state.magazines[0].rounds_loaded <= cutoffs->loaded_ammo && !state.magazines[0].reloading &&
			state.magazines[0].rounds_loaded * 2 < state.magazines[0].rounds_loaded_maximum)
		{
			out->low_ammo_flash_on = blink_on();
		}
	}
}

/* a vehicle's seat rows (14.15 part 2: Banshee bolts + bomb; another gun a heat bar when its HUD has a heat element,
as classic shows it: the Warthog chain gun's has none, HUD audit 10-26; the Scorpion's CANNON row is gone, owner
10-26: classic has none), by the vehicle's tag */
static void read_seat(long vehicle_index, long weapon_index, struct ae_panel_snapshot *out)
{
	struct unit_datum *vehicle = unit_get(vehicle_index);
	char const *name = tag_get_name(vehicle->definition_index);
	struct weapon_interface_state state;

	out->in_seat = TRUE;
	if (weapon_index == NONE)
		return;
	weapon_build_weapon_interface_state(weapon_index, &state);
	if (name_has(name, "banshee"))
	{
		out->seat_rows[0].kind = _ae_seat_bolts;
		out->seat_rows[0].fill = state.heat;
		out->seat_rows[0].overheated = state.overheated;
		out->seat_rows[1].kind = _ae_seat_bomb;
		out->seat_rows[1].fill = state.magazine_count > 1 && state.magazines[1].rounds_loaded_maximum > 0 ?
			(real)state.magazines[1].rounds_loaded / (real)state.magazines[1].rounds_loaded_maximum : 1.0f - state.age;
		out->seat_row_count = 2;
	}
	else if (hud_declares_state(weapon_definition_get(weapon_get(weapon_index)->definition_index), AE_WEAPON_HUD_STATE_HEAT))
	{
		out->seat_rows[0].kind = _ae_seat_heat;
		out->seat_rows[0].fill = state.heat;
		out->seat_rows[0].overheated = state.overheated;
		out->seat_row_count = 1;
	}
}

/* each player slot's shield hit trail (ae_panel_trail_step) */
static struct ae_panel_trail trails[PLAYER_SLOTS];

static real overshield_projection(short slot, struct unit_datum *unit)
{
	real v = unit->object.shield_vitality;
	real seconds = 0.0f;
	long now = game_time_get();

	if (TEST_FLAG(unit->object.damage_flags, _object_shield_over_charging_bit))
		seconds = (3.0f - v) / (OVERSHIELD_CHARGE * TICKS_PER_SECOND) + 2.0f / (OVERSHIELD_DECAY * TICKS_PER_SECOND);
	else if (v > 1.0f)
		seconds = (v - 1.0f) / (OVERSHIELD_DECAY * TICKS_PER_SECOND);
	/* never rising unless the vitality rose (a pickup, a recharge): a host correction does not make it run back */
	if (seconds > 0.0f && overshield[slot].seconds > 0.0f &&
		v <= overshield[slot].shield + 0.0001f && now >= overshield[slot].tick &&
		seconds > overshield[slot].seconds)
	{
		seconds = overshield[slot].seconds;
	}
	overshield[slot].seconds = seconds;
	overshield[slot].shield = v;
	overshield[slot].tick = now;
	return seconds;
}

/* any player's panels' state (the host logs its joiners' for the two-machine check) */
static boolean player_snapshot(long player_index, struct ae_panel_snapshot *out)
{
	struct player_datum *player = NULL;
	struct unit_datum *unit;
	long unit_index = NONE, weapon_index, vehicle_index;
	boolean in_seat;
	short i;

	memset(out, 0, sizeof(*out));
	out->weapon_type_hint = _ae_hint_none;
	unit = player_unit(player_index, &unit_index, &player);
	if (!unit || TEST_FLAG(unit->object.damage_flags, _object_dead_bit))
		return FALSE;

	weapon_index = panel_weapon(unit_index, &in_seat, &vehicle_index);
	if (in_seat)
		read_seat(vehicle_index, weapon_index, out);
	else if (weapon_index != NONE)
		read_weapon(weapon_index, out);

	for (i = 0; i < 2; i++)
		out->grenades[i] = unit->unit.grenade_counts[i];
	out->armed_grenade = unit->unit.current_grenade_index;

	out->shield = unit->object.shield_vitality;
	out->health = unit->object.body_vitality;
	out->shield_down = (boolean)(out->shield <= 0.0f);
	out->shield_low = (boolean)(out->shield > 0.0f && out->shield < 0.25f);
	out->shield_blink_on = blink_on();
	out->health_low = (boolean)(out->health < 0.25f);
	out->camo_ticks = player->powerup_durations[_player_powerup_active_camouflage];
	out->overshield_seconds = overshield_projection((short)((player_index & 0xFFFF) % PLAYER_SLOTS), unit);
	ae_panel_trail_step(&trails[(player_index & 0xFFFF) % PLAYER_SLOTS], out->shield, game_time_get(), &out->shield_trail,
		&out->shield_trail_alpha);
	out->reduced_motion = reduced_motion_on();
	return TRUE;
}

boolean ae_panel_engine_snapshot(short local_player_index, struct ae_panel_snapshot *out)
{
	return player_snapshot(view_player_index(local_player_index), out);
}

/* each view's last ammo corner inside its top-left quarter (ae_panel_engine_view) */
static rectangle2d good_corner[MAXIMUM_LOCAL_PLAYERS];
static boolean good_corner_known[MAXIMUM_LOCAL_PLAYERS];

boolean ae_panel_engine_view(short local_player_index, struct ae_panel_view *out)
{
	real ppu = ae_hud_pixels_per_unit();
	short width = (short)(render.camera.viewport_bounds.x1 - render.camera.viewport_bounds.x0);
	short height = (short)(render.camera.viewport_bounds.y1 - render.camera.viewport_bounds.y0);
	rectangle2d corner, meters;
	boolean placed = TRUE;

	out->scale = ae_score_strip_view_scale();
	out->view_width = (real)width * ppu;
	out->view_height = (real)height * ppu;
	/* the ammo group's origin: where CE's weapon HUD put its top-left elements in this view (hud.c's
	hud_top_left_extent, kept by hud_item_timers.c) */
	/* (a corner outside the view's top-left quarter is not the ammo's: after an active camouflage pickup one of CE's
	weapon HUD pieces sat thousands of pixels above the view and the ammo group went with it (owner 10-15, "ammo
	disappeared"); the last good corner of this view stands in) */
	if (hud_item_timers_ae_corner(local_player_index, &corner) &&
		corner.x0 >= 0 && corner.x0 < width / 2 && corner.y0 >= 0 && corner.y0 < height / 2)
	{
		if (local_player_index >= 0 && local_player_index < MAXIMUM_LOCAL_PLAYERS)
		{
			good_corner[local_player_index] = corner;
			good_corner_known[local_player_index] = TRUE;
		}
	}
	else if (local_player_index >= 0 && local_player_index < MAXIMUM_LOCAL_PLAYERS &&
		good_corner_known[local_player_index] &&
		good_corner[local_player_index].x0 < width / 2 && good_corner[local_player_index].y0 < height / 2)
	{
		corner = good_corner[local_player_index];
	}
	else
	{
		corner.x0 = NONE;
	}
	if (corner.x0 != NONE)
	{
		out->ammo_origin_x = (real)corner.x0 * ppu + AMMO_ORIGIN_DX * out->scale;
		out->ammo_origin_y = (real)corner.y0 * ppu + AMMO_ORIGIN_DY * out->scale;
	}
	else
	{
		out->ammo_origin_x = 130.0f * out->scale;
		out->ammo_origin_y = 83.0f * out->scale;
		placed = FALSE;
	}
	/* the right column: CE's shield and health meters' right edge and top (hud_unit.c's meters_bounds) */
	if (hud_item_timers_ae_meters(local_player_index, &meters) && meters.x1 > width / 2)
	{
		out->shield_right_x = (real)meters.x1 * ppu + SHIELD_RIGHT_DX * out->scale;
		out->shield_top_y = (real)meters.y0 * ppu + SHIELD_TOP_DY * out->scale;
	}
	else
	{
		short inset = 0;
		long baseline = 0;

		hud_item_timers_ae_place(&inset, &baseline);
		out->shield_right_x = (real)(width - inset) * ppu;
		out->shield_top_y = out->ammo_origin_y;
		placed = FALSE;
	}
	/* the corners square: the same margin from the view's left and right edges (CE's placements keep the heights),
	and with HUD AREA (display.hud_area) from its sides, as CE's elements, the sensor and the strip keep to them */
	{
		short area_left = 0, area_right = 0;

		hud_area_insets(&area_left, &area_right);
		out->ammo_origin_x = (real)area_left * ppu + ae_hud_corner_margin(out->scale) - AE_PANEL_LEFT_LINE_X * out->scale;
		out->shield_right_x = out->view_width - (real)area_right * ppu - ae_hud_corner_margin(out->scale);
	}
	return placed;
}

static const char *player_weapon_name(long player_index)
{
	struct player_datum *player = NULL;
	long unit_index = NONE, weapon_index, vehicle_index;
	boolean in_seat;
	char const *name;
	char const *last;

	if (!player_unit(player_index, &unit_index, &player))
		return "none";
	weapon_index = panel_weapon(unit_index, &in_seat, &vehicle_index);
	if (weapon_index == NONE)
		return "none";
	name = tag_get_name(weapon_get(weapon_index)->definition_index);
	if (!name)
		return "?";
	last = strrchr(name, '\\');
	return last ? last + 1 : name;
}

const char *ae_panel_engine_weapon_name(short local_player_index)
{
	return player_weapon_name(view_player_index(local_player_index));
}

/* a player's name in ASCII for the log */
static void player_name(long player_index, char *out, long size)
{
	struct player_datum *player = player_index == NONE ? NULL : player_get(player_index);
	long i;

	for (i = 0; player && i < size - 1 && i < (long)NUMBEROF(player->name) && player->name[i]; i++)
		out[i] = player->name[i] >= 32 && player->name[i] < 127 && player->name[i] != ' ' ? (char)player->name[i] : '_';
	out[i] = 0;
}

/* debug.ae_panel_powerups ("overshield:10,camo:20", seconds of game time): the host gives its first local player
the overshield (object_double_charge_shield, as the powerup does) and 37 s of camo (player_handle_powerup) */
boolean player_handle_powerup(long player_index, short powerup_type, short duration);
static void debug_powerups(void)
{
	static long given_through = -1;
	char const *setting = config_string("debug.ae_panel_powerups");
	long now = game_time_get(), player_index = view_player_index(0), unit_index = NONE;
	struct player_datum *player = NULL;
	char const *walk;

	if (!setting || !setting[0] || network_game_distributed_client() || now < given_through)
		return;
	if (!player_unit(player_index, &unit_index, &player))
		return;
	for (walk = setting; walk && *walk; walk = strchr(walk, ','), walk = walk ? walk + 1 : NULL)
	{
		double seconds = 0.0;
		long tick;
		boolean camo = (boolean)(strncmp(walk, "camo:", 5) == 0);

		if (!camo && strncmp(walk, "overshield:", 11) != 0)
			continue;
		seconds = atof(walk + (camo ? 5 : 11));
		tick = (long)(seconds * TICKS_PER_SECOND);
		if (tick <= given_through || tick > now)
			continue;
		if (camo)
			player_handle_powerup(player_index, _player_powerup_active_camouflage, (short)(37 * TICKS_PER_SECOND));
		else
			object_double_charge_shield(unit_index);
		platform_log("ae panel: debug %s given at tick %ld", camo ? "camo" : "overshield", now);
	}
	given_through = now;
}

/* debug.ae_panel_seat ("ghost:driver:8", "warthog:gunner:8"): the host seats its first local player in the first
vehicle whose tag has the name in it, in its first driver's (or gunner's) seat, at that second of game time; the
vehicles the map has are logged */
static void debug_seat(void)
{
	static boolean done = FALSE;
	char const *setting = config_string("debug.ae_panel_seat");
	char name[32];
	char role[16];
	double seconds = 0.0;
	long player_index = view_player_index(0), unit_index = NONE;
	struct player_datum *player = NULL;
	struct object_iterator vehicles;

	if (done || !setting || !setting[0] || network_game_distributed_client())
		return;
	if (sscanf(setting, "%31[^:]:%15[^:]:%lf", name, role, &seconds) != 3 ||
		game_time_get() < (long)(seconds * TICKS_PER_SECOND) || !player_unit(player_index, &unit_index, &player))
	{
		return;
	}
	done = TRUE;
	object_iterator_new(&vehicles, _object_mask_vehicle, 0);
	while (object_iterator_next(&vehicles))
	{
		long vehicle_index = vehicles.index;
		struct unit_definition *definition = unit_definition_get(object_get(vehicle_index)->definition_index);
		char const *tag = tag_get_name(object_get(vehicle_index)->definition_index);
		short seat_index;

		platform_log("ae panel: debug vehicle %s", tag ? tag : "?");
		if (!name_has(tag, name))
			continue;
		for (seat_index = 0; seat_index < definition->unit.seats.count; seat_index++)
		{
			struct unit_seat *seat = TAG_BLOCK_GET_ELEMENT(&definition->unit.seats, seat_index, struct unit_seat);
			boolean wanted = (boolean)(strcmp(role, "gunner") ? TEST_FLAG(seat->flags, _unit_seat_driver_bit) :
				TEST_FLAG(seat->flags, _unit_seat_gunner_bit) && !TEST_FLAG(seat->flags, _unit_seat_driver_bit));

			if (wanted && unit_enter_seat(unit_index, vehicle_index, seat_index))
			{
				platform_log("ae panel: debug seated in %s seat %d (%s)", tag, seat_index, role);
				return;
			}
		}
	}
	platform_log("ae panel: debug no %s seat in a %s", role, name);
}

static void log_line(long tick, char const *who, long player_index, struct ae_panel_snapshot const *snapshot,
	char const *tail)
{
	static const char *kinds[NUMBER_OF_AE_PANEL_WEAPON_KINDS] = { "none", "rounds", "battery", "battery_no_heat",
		"tank", "seat" };
	static const char *seat_kinds[NUMBER_OF_AE_SEAT_ROW_KINDS] = { "heat", "bolts", "bomb", "cannon", "mg" };
	struct ae_panel_model model;
	char seat[64], name[16];

	ae_panel_model_build(snapshot, &model);
	player_name(player_index, name, sizeof(name));
	if (snapshot->in_seat && snapshot->seat_row_count > 0)
		snprintf(seat, sizeof(seat), "%s:%.2f%s%s", seat_kinds[snapshot->seat_rows[0].kind],
			snapshot->seat_rows[0].fill, snapshot->seat_row_count > 1 ? "+" : "",
			snapshot->seat_row_count > 1 ? seat_kinds[snapshot->seat_rows[1].kind] : "");
	else
		snprintf(seat, sizeof(seat), "%s", snapshot->in_seat ? "empty" : "none");
	platform_log("ae panel: tick %ld %s name=%s weapon=%s kind=%s loaded=%d/%d reserve=%d heat=%.2f battery=%.2f "
		"overheated=%d low=%d grenades=%d/%d armed=%s shield=%.2f health=%.2f camo=%ld overshield=%.1f seat=%s%s",
		tick, who, name, player_weapon_name(player_index), kinds[model.weapon_kind], snapshot->rounds_loaded,
		snapshot->rounds_per_magazine, snapshot->rounds_reserve, snapshot->heat, snapshot->battery,
		snapshot->overheated, snapshot->low_ammo_flash_on || (snapshot->has_weapon && model.rounds_red),
		snapshot->grenades[0], snapshot->grenades[1],
		snapshot->armed_grenade == 0 ? "frag" : snapshot->armed_grenade == 1 ? "plasma" : "none", snapshot->shield,
		snapshot->health, snapshot->camo_ticks, snapshot->overshield_seconds, seat, tail);
}

void ae_panel_engine_debug_tick(void)
{
	debug_powerups();
	debug_seat();
}

void ae_panel_engine_debug_log(short local_player_index)
{
	static long logged_tick[MAXIMUM_LOCAL_PLAYERS] = { -1, -1, -1, -1 };
	struct ae_panel_snapshot snapshot;
	struct ae_panel_view view;
	long tick = game_time_get();
	long player_index = view_player_index(local_player_index);
	boolean placed;
	char who[16], tail[160];

	if (player_index == NONE)
		return;

	/* (once in each 2 s of game time: the first frame drawn in it, as frames skip ticks) */
	if (!ae_panel_engine_log_on() || tick < 0 || tick / 60 == logged_tick[local_player_index])
		return;
	logged_tick[local_player_index] = tick / 60;
	tick = tick / 60 * 60;
	snprintf(who, sizeof(who), "view %d", local_player_index);
	if (!player_snapshot(player_index, &snapshot))
	{
		platform_log("ae panel: tick %ld %s dead", tick, who);
	}
	else
	{
		placed = ae_panel_engine_view(local_player_index, &view);
		snprintf(tail, sizeof(tail), " origin=%.1f,%.1f right=%.1f,%.1f scale=%.2f placed=%d", view.ammo_origin_x,
			view.ammo_origin_y, view.shield_right_x, view.shield_top_y, view.scale, placed);
		log_line(tick, who, player_index, &snapshot, tail);
	}
	/* (the host: its joiners' too, for the two-machine check) */
	if (local_player_index == 0 && !network_game_distributed_client())
	{
		struct data_iterator iterator;
		struct player_datum *player;

		data_iterator_new(&iterator, player_data);
		while ((player = (struct player_datum *)data_iterator_next(&iterator)) != NULL)
		{
			if (player->local_player_index != NONE || !player_snapshot(iterator.datum_index, &snapshot))
				continue;
			snprintf(who, sizeof(who), "player %ld", (long)(iterator.datum_index & 0xFFFF));
			log_line(tick, who, iterator.datum_index, &snapshot, "");
		}
	}
}

/* the settings the views read every frame, read again only when a setting changed */
static struct { unsigned long seen; boolean log, reduced_motion; } cached_settings = { (unsigned long)-1, FALSE, FALSE };

static void settings_read(void)
{
	if (cached_settings.seen == config_changes())
		return;
	cached_settings.seen = config_changes();
	cached_settings.log = (boolean)config_boolean("debug.ae_panel_log");
	cached_settings.reduced_motion = (boolean)config_boolean("display.arena_menus_reduce_motion");
}

boolean ae_panel_engine_log_on(void)
{
	settings_read();
	return cached_settings.log;
}

static boolean reduced_motion_on(void)
{
	settings_read();
	return cached_settings.reduced_motion;
}

boolean ae_panel_engine_reduced_motion(void)
{
	return reduced_motion_on();
}
