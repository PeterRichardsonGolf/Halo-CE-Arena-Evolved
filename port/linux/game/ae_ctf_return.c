/*
AE_CTF_RETURN.C

(spike 10-26) FLAG RETURN by standing by the flag (ae_ctf_return.h). The host counts (a client is told the flag's
place, as before). Each flag keeps its count by its weapon; a new last owned time (someone picked it up) starts it
again. A defender within AE_CTF_RETURN_RADIUS adds AE_CTF_RETURN_BOOST ticks a tick; the local defenders by it are told
how far it is ("RETURNING FLAG 40%"). Log: "ctf return: <team> flag x defenders, nn%" each second it moves, "home".
*/

#include "cseries.h"
#include "game/game.h"
#include "game/players.h"
#include "math/real_math.h"
#include "interface/hud_messaging.h"
#include "memory/data.h"
#include "objects/objects.h"
#include "units/units.h"

#include "ae_ctf_return.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void platform_log(const char *format, ...);

#define AE_CTF_RETURN_RADIUS 2.5f
#define AE_CTF_RETURN_BOOST 4
#define AE_CTF_RETURN_FLAGS 4

static struct
{
	int read, on;
	long time;
	struct { long weapon_index, last_owned_time, progress; } flags[AE_CTF_RETURN_FLAGS];
} ae_ctf_return = { 0 };

static void ae_ctf_return_read(void)
{
	char const *rule = getenv("HALO_FLAG_RETURN"), *time = getenv("HALO_FLAG_RETURN_TIME");

	if (ae_ctf_return.read)
		return;
	ae_ctf_return.read = 1;
	ae_ctf_return.on = rule && !strcmp(rule, "stand");
	ae_ctf_return.time = (time && atoi(time) > 0 ? atoi(time) : 30) * TICKS_PER_SECOND;
	if (ae_ctf_return.on)
		platform_log("ctf return: STAND BY THE FLAG, %ld s, defenders x%d", ae_ctf_return.time / TICKS_PER_SECOND,
			AE_CTF_RETURN_BOOST + 1);
}

boolean ae_ctf_stand_return(void)
{
	ae_ctf_return_read();
	return ae_ctf_return.on ? TRUE : FALSE;
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

boolean ae_ctf_flag_return_due(long weapon_index, long last_owned_time, boolean handled, boolean ce_due)
{
	struct object_datum *flag;
	struct data_iterator iterator;
	struct player_datum *player;
	long slot, free_slot = -1, defenders = 0, percent;

	if (!ae_ctf_stand_return() || !handled)
		return ce_due;
	for (slot = 0; slot < AE_CTF_RETURN_FLAGS; slot++)
	{
		if (ae_ctf_return.flags[slot].weapon_index == weapon_index)
			break;
		if (free_slot < 0 && ae_ctf_return.flags[slot].weapon_index == 0)
			free_slot = slot;
	}
	if (slot == AE_CTF_RETURN_FLAGS)
	{
		slot = free_slot >= 0 ? free_slot : 0;
		ae_ctf_return.flags[slot].weapon_index = weapon_index;
		ae_ctf_return.flags[slot].last_owned_time = last_owned_time;
		ae_ctf_return.flags[slot].progress = 0;
	}
	/* (picked up since: the count starts again) */
	if (ae_ctf_return.flags[slot].last_owned_time != last_owned_time)
	{
		ae_ctf_return.flags[slot].last_owned_time = last_owned_time;
		ae_ctf_return.flags[slot].progress = 0;
	}
	flag = object_get(weapon_index);
	data_iterator_new(&iterator, player_data);
	while ((player = (struct player_datum *)data_iterator_next(&iterator)) != NULL)
	{
		struct object_datum *unit;

		if (player->unit_index == NONE || player->team_index != flag->object.owner_team_index)
			continue;
		unit = object_try_and_get_and_verify_type(player->unit_index, _object_mask_unit);
		if (!unit || unit->object.parent_object_index != NONE ||
			distance3d(&unit->object.position, &flag->object.position) > AE_CTF_RETURN_RADIUS)
		{
			continue;
		}
		defenders++;
	}
	ae_ctf_return.flags[slot].progress += 1 + AE_CTF_RETURN_BOOST * defenders;
	percent = MIN(100, ae_ctf_return.flags[slot].progress * 100 / ae_ctf_return.time);
	if (defenders && game_time_get() % (TICKS_PER_SECOND / 2) == 0)
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
		platform_log("ctf return: team %d flag, %ld defenders by it, %ld%%", (int)flag->object.owner_team_index,
			defenders, percent);
	if (ae_ctf_return.flags[slot].progress < ae_ctf_return.time)
		return FALSE;
	platform_log("ctf return: team %d flag home", (int)flag->object.owner_team_index);
	ae_ctf_return.flags[slot].weapon_index = 0;
	return TRUE;
}
