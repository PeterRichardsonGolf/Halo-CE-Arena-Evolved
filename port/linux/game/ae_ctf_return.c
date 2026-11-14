/*
AE_CTF_RETURN.C

FLAG RETURN (ae_ctf_return.h; the rules: ae_ctf_return_rules.h), the gametype's ctf_return. The host counts (a client
is told the flag's place, as before). Each flag keeps its count by its weapon; a new last owned time (someone picked it
up) starts it again; the count moves only while the flag lies on the map. In STAND-BY, the local defenders by it are
told how far it is ("RETURNING FLAG 40%"). Log: "ctf return: <team> flag x defenders, y enemies, nn%" each second it
lies, "home".
*/

#include "cseries.h"
#include "game/game.h"
#include "game/game_engine.h"
#include "game/players.h"
#include "items/weapons.h"
#include "math/real_math.h"
#include "interface/hud_messaging.h"
#include "memory/data.h"
#include "objects/objects.h"
#include "units/units.h"

#include "ae_ctf_return.h"
#include "ae_ctf_return_rules.h"
#include "ae_objective_log.h"
#include "ae_variant_ext.h"

#include <stdio.h>
#include <string.h>

void platform_log(const char *format, ...);

#define AE_CTF_RETURN_FLAGS 4
/* game_engine_ctf.c's _ctf_weapon_handled_bit: the flag has left its stand */
#define AE_CTF_WEAPON_HANDLED_BIT 6

static struct
{
	struct { long weapon_index, last_owned_time, progress, seen_tick; } flags[AE_CTF_RETURN_FLAGS];
} ae_ctf_return = { 0 };

/* whether a slot is free: never used, its flag gone, or not counted for a while (a touch return, a capture, a game's
end leave a slot behind; a new game's game time starts again below the slot's) */
static boolean ae_ctf_return_slot_free(long slot, long now)
{
	long weapon_index = ae_ctf_return.flags[slot].weapon_index;

	return weapon_index == 0 || !object_try_and_get_and_verify_type(weapon_index, _object_mask_weapon) ||
		!weapon_is_flag(weapon_index) || ae_ctf_return.flags[slot].seen_tick > now ||
		now - ae_ctf_return.flags[slot].seen_tick > 2;
}

static int ae_ctf_return_mode(void)
{
	int mode = ae_variant_ext_game()->ctf_return;

	return mode < AE_EXT_CTF_RETURN_COUNT ? mode : AE_EXT_CTF_RETURN_CE;
}

boolean ae_ctf_stand_return(void)
{
	return ae_ctf_return_touch_returns(ae_ctf_return_mode()) ? FALSE : TRUE;
}

static void ae_ctf_return_say(short local_player_index, long percent)
{
	char text[40];
	wchar_t line[40];
	int at;

	/* (the build's wchar_t is two bytes: no swprintf) */
	snprintf(text, sizeof(text), "RETURNING FLAG %ld%%", percent);
	for (at = 0; text[at] && at < (int)NUMBEROF(line) - 1; at++)
		line[at] = (wchar_t)text[at];
	line[at] = 0;
	hud_print_message(local_player_index, line);
}

/* the living players by the flag: its team's on foot (defenders), the others' on foot or in a seat (enemies) */
static void ae_ctf_return_count(struct object_datum const *flag, long *defenders, long *enemies)
{
	struct data_iterator iterator;
	struct player_datum *player;

	*defenders = 0;
	*enemies = 0;
	data_iterator_new(&iterator, player_data);
	while ((player = (struct player_datum *)data_iterator_next(&iterator)) != NULL)
	{
		struct object_datum *unit;

		if (player->unit_index == NONE)
			continue;
		unit = object_try_and_get_and_verify_type(player->unit_index, _object_mask_unit);
		if (!unit || distance3d(&unit->object.position, &flag->object.position) > AE_CTF_RETURN_RADIUS)
			continue;
		if (player->team_index != flag->object.owner_team_index)
			(*enemies)++;
		else if (unit->object.parent_object_index == NONE)
			(*defenders)++;
	}
}

boolean ae_ctf_flag_contested(long weapon_index)
{
	struct object_datum *flag = object_try_and_get_and_verify_type(weapon_index, _object_mask_weapon);
	long defenders, enemies;

	/* (a flag at home, on its stand, is not in play: an enemy by it holds nothing) */
	if (!flag || flag->object.parent_object_index != NONE ||
		!TEST_FLAG(weapon_get(weapon_index)->weapon.flags, AE_CTF_WEAPON_HANDLED_BIT))
	{
		return FALSE;
	}
	ae_ctf_return_count(flag, &defenders, &enemies);
	return enemies > 0 ? TRUE : FALSE;
}

boolean ae_ctf_flag_return_due(long weapon_index, long last_owned_time, boolean handled, boolean ce_due)
{
	struct object_datum *flag;
	struct data_iterator iterator;
	struct player_datum *player;
	long slot, free_slot = -1, defenders, enemies, percent, time, now = game_time_get();
	int mode = ae_ctf_return_mode();

	if (mode == AE_EXT_CTF_RETURN_CE || !handled)
		return ce_due;
	flag = object_get(weapon_index);
	/* (carried, or not yet at rest: no count) */
	if (flag->object.parent_object_index != NONE || !TEST_FLAG(flag->object.flags, _object_connected_to_map_bit))
		return FALSE;
	for (slot = 0; slot < AE_CTF_RETURN_FLAGS; slot++)
	{
		if (ae_ctf_return.flags[slot].weapon_index == weapon_index && !ae_ctf_return_slot_free(slot, now))
			break;
		if (free_slot < 0 && ae_ctf_return_slot_free(slot, now))
			free_slot = slot;
	}
	if (slot == AE_CTF_RETURN_FLAGS)
	{
		slot = free_slot >= 0 ? free_slot : 0;
		ae_ctf_return.flags[slot].weapon_index = weapon_index;
		ae_ctf_return.flags[slot].last_owned_time = last_owned_time;
		ae_ctf_return.flags[slot].progress = 0;
	}
	ae_ctf_return.flags[slot].seen_tick = now;
	/* (picked up since: the count starts again) */
	if (ae_ctf_return.flags[slot].last_owned_time != last_owned_time)
	{
		ae_ctf_return.flags[slot].last_owned_time = last_owned_time;
		ae_ctf_return.flags[slot].progress = 0;
	}
	time = ae_ctf_return_seconds(mode, ae_variant_ext_game()->ctf_reset_time) * TICKS_PER_SECOND;
	ae_ctf_return_count(flag, &defenders, &enemies);
	ae_ctf_return.flags[slot].progress += ae_ctf_return_step(mode, (int)defenders, (int)enemies);
	percent = MIN(100, ae_ctf_return.flags[slot].progress * 100 / time);
	if (mode == AE_EXT_CTF_RETURN_STAND && defenders && game_time_get() % (TICKS_PER_SECOND / 2) == 0)
	{
		data_iterator_new(&iterator, player_data);
		while ((player = (struct player_datum *)data_iterator_next(&iterator)) != NULL)
		{
			struct object_datum *unit;

			if (player->local_player_index == NONE || player->unit_index == NONE ||
				player->team_index != flag->object.owner_team_index)
			{
				continue;
			}
			unit = object_try_and_get_and_verify_type(player->unit_index, _object_mask_unit);
			if (unit && distance3d(&unit->object.position, &flag->object.position) <= AE_CTF_RETURN_RADIUS)
				ae_ctf_return_say(player->local_player_index, percent);
		}
	}
	if (game_time_get() % TICKS_PER_SECOND == 0)
		platform_log("ctf return: team %d flag, %ld defenders, %ld enemies by it, %ld%%", (int)flag->object.owner_team_index,
			defenders, enemies, percent);
	if (ae_ctf_return.flags[slot].progress < time)
		return FALSE;
	platform_log("ctf return: team %d flag home", (int)flag->object.owner_team_index);
	ae_ctf_return.flags[slot].weapon_index = 0;
	return TRUE;
}

boolean ae_ctf_flag_hold(long weapon_index, long player_index)
{
	struct player_datum *player;
	struct object_datum *flag;

	if (!game_engine_running() || game_engine_get_variant()->game_engine_index != game_engine_ctf ||
		!weapon_is_flag(weapon_index))
	{
		return FALSE;
	}
	player = player_try_and_get(player_index);
	flag = object_try_and_get_and_verify_type(weapon_index, _object_mask_weapon);
	/* (bots take it with a touch, as before: the hold is for people; owner 11-08) */
	if (!player || !flag || ae_obj_player_is_bot(player_index))
		return FALSE;
	return ae_ctf_hold_pickup(ae_variant_ext_game()->ctf_flags, flag->object.owner_team_index, player->team_index) ?
		TRUE : FALSE;
}
