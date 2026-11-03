/*
AE_HOLD_ACT.C

HOLD TO ACT (ae_hold_act.h). Halo CE acts on the action button's press: next to a vehicle a tap gets in (or takes the
seat, or flips it), a tap in a seat gets out, and only a weapon on the ground waits for a hold. The second game holds
for all of them, so a tap next to a vehicle or in a passenger's seat reloads, and nobody gets in or out by accident
while reloading in a fight (owner 2026-11-01: "port in the action contextual stuff like hold x to get in"; "same for
all actions when you are in a vehicle").

The hold is the game's own weapon pickup hold (the swap flag, set while the button is held past the map's
minimum_weapon_swap_ticks), so a vehicle and a weapon ask for the same hold, and its prompt "Hold X to enter" follows
the pickup's "Hold X to pick up".

Where it applies: a gametype with VEHICLES DESTRUCTIBLE ON, the rule AE's other vehicle changes follow (ae_ghost_tune,
ae_vehicle_death; seeds have it ON in every AE gametype with vehicles, stock gametypes OFF). Every machine reads it from
the gametype; a bot keeps the tap (its seat logic presses once, ae_bots_vehicle.inc).
*/

#include "cseries.h"
#include "game/game.h"
#include "game/game_engine.h"
#include "game/players.h"
#include "objects/objects.h"
#include "units/units.h"
#include "ae_hold_act.h"
#include "ae_vehicle_death_rules.h"
#include "bots.h"
#include "halo_port_limits.h"

/* hud.c's state messages: get in, flip (the HUD messages' order) */
#define HUD_MESSAGE_ENTER_VEHICLE 6
#define HUD_MESSAGE_FLIP_VEHICLE 8

static boolean held_before[HALO_PORT_MAXIMUM_NETWORK_PLAYERS];

static boolean ae_hold_act_on(void)
{
	struct game_variant const *variant = game_engine_running() ? game_engine_get_variant() : NULL;

	return variant && ae_vehicle_death_get(variant->universal_variant.flags) == _ae_vehicle_death_on;
}

static boolean vehicle_action(short action_result)
{
	return action_result == _player_action_result_enter_vehicle || action_result == _player_action_result_evict_from_vehicle ||
		action_result == _player_action_result_flip_vehicle;
}

void ae_hold_act_control(long player_index, unsigned long *control_flags)
{
	struct player_datum *player = player_get(player_index);
	short absolute = DATUM_INDEX_TO_ABSOLUTE_INDEX(player_index);
	boolean held = TEST_FLAG(*control_flags, _unit_control_swap_weapons_bit) != 0;
	boolean first = held && !held_before[absolute];
	boolean seated;

	held_before[absolute] = held;
	if (!ae_hold_act_on() || bots_machine_is_bot(player->network_player_data.machine_index) || player->unit_index == NONE)
		return;
	/* (in a seat every seat asks for the hold to get out, and a tap reloads a passenger's gun; on foot only a vehicle
	next to the player does) */
	seated = unit_get(player->unit_index)->unit.parent_seat_index != NONE &&
		object_get(player->unit_index)->object.parent_object_index != NONE;
	if (!seated && (!vehicle_action(player->action_result) || object_get(player->unit_index)->object.parent_object_index != NONE))
		return;
	if (TEST_FLAG(*control_flags, _unit_control_action_bit))
	{
		/* (the press: a reload, as away from anything; the keyboard's action key, which never reloads, nothing) */
		SET_FLAG(*control_flags, _unit_control_action_bit, FALSE);
		if (!TEST_FLAG(*control_flags, UNIT_CONTROL_PORT_ACTION_ONLY_BIT))
			SET_FLAG(*control_flags, _unit_control_weapon_reload_bit, TRUE);
	}
	if (first)
		SET_FLAG(*control_flags, _unit_control_action_bit, TRUE);
}

int ae_hold_act_active(void)
{
	return ae_hold_act_on() ? 1 : 0;
}

wchar_t const *ae_hold_act_prompt(long message_index, wchar_t const *text)
{
	static wchar_t const hold[] = L"Hold ";

	if ((message_index == HUD_MESSAGE_ENTER_VEHICLE || message_index == HUD_MESSAGE_FLIP_VEHICLE) && text &&
		text[0] == L'P' && text[1] == L'r' && text[2] == L'e' && text[3] == L's' && text[4] == L's' && text[5] == L' ' &&
		text[6] == 0 && ae_hold_act_on())
	{
		return hold;
	}
	return text;
}
