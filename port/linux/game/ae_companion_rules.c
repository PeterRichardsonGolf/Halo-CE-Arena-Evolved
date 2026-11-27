/*
AE_COMPANION_RULES.C

See ae_companion_rules.h.
*/

#include <math.h>
#include <string.h>

#include "ae_companion_rules.h"

/* (the levels whose intro ride the sweep passed: none yet) */
static char const *const intro_ride_levels[] = { NULL };

int ae_companion_entry_candidates(float facing_i, float facing_j, int slot, float offsets[][2])
{
	/* (behind, then behind and to the side, then to the side a little back; the nearest first) */
	static float const back[AE_COMPANION_ENTRY_CANDIDATES] = { 2.5f, 2.5f, 2.5f, 3.5f, 3.5f, 1.5f, 1.5f, 4.5f };
	static float const side[AE_COMPANION_ENTRY_CANDIDATES] = { 1.0f, -1.0f, 0.0f, 1.5f, -1.5f, 2.5f, -2.5f, 0.0f };
	float length = sqrtf(facing_i * facing_i + facing_j * facing_j), mirror = slot % 2 ? 1.0f : -1.0f;
	int index;

	if (length < 1e-4f)
	{
		facing_i = 1.0f;
		facing_j = 0.0f;
	}
	else
	{
		facing_i /= length;
		facing_j /= length;
	}
	for (index = 0; index < AE_COMPANION_ENTRY_CANDIDATES; index++)
	{
		/* (left of the facing is (-j, i)) */
		float across = side[index] * mirror;

		offsets[index][0] = -facing_i * back[index] - facing_j * across;
		offsets[index][1] = -facing_j * back[index] + facing_i * across;
	}
	return AE_COMPANION_ENTRY_CANDIDATES;
}

int ae_companion_entry_unseen(float facing_i, float facing_j, float offset_i, float offset_j)
{
	float facing = sqrtf(facing_i * facing_i + facing_j * facing_j), offset = sqrtf(offset_i * offset_i + offset_j * offset_j);

	if (facing < 1e-4f || offset < 1e-4f)
		return 0;
	/* (cos 100 degrees) */
	return (facing_i * offset_i + facing_j * offset_j) / (facing * offset) < -0.1736f;
}

int ae_companion_entry_floor(float normal_k, float floor_z, float player_z, float headroom)
{
	return normal_k >= AE_COMPANION_ENTRY_FLOOR_UP && fabsf(floor_z - player_z) <= AE_COMPANION_ENTRY_FLOOR_RISE &&
		headroom >= AE_COMPANION_ENTRY_HEADROOM;
}

int ae_companion_catch_up(float distance, long ticks_without_gain, int unseen, int seated)
{
	return !seated && unseen && distance > AE_COMPANION_CATCH_UP_FAR && ticks_without_gain >= AE_COMPANION_CATCH_UP_TICKS;
}

int ae_companion_track_step(float const track[][3], int count, float x, float y, float z)
{
	return ae_companion_track_step_from(track, count, x, y, z, 0);
}

int ae_companion_track_step_from(float const track[][3], int count, float x, float y, float z, int least)
{
	int index, nearest = -1, newer = -1;
	float best = AE_COMPANION_TRACK_ON * AE_COMPANION_TRACK_ON, near;

	for (index = least > 0 ? least : 0; index < count; index++)
	{
		float dx = track[index][0] - x, dy = track[index][1] - y, gap = dx * dx + dy * dy;

		/* (the newer of two as near: a track that loops back is walked on) */
		if (fabsf(track[index][2] - z) <= AE_COMPANION_TRACK_RISE && gap <= best)
		{
			best = gap;
			nearest = index;
		}
	}
	if (nearest < 0)
		return -1;
	/* (a track walked back along the same line: the newest crumb within AE_COMPANION_TRACK_NEWER of the nearest one's distance,
	on a later stretch of the track (the track between the two over twice as long as the gap between them), is walked from, not
	the older one beside it on the way out to the turn) */
	near = sqrtf(best) + AE_COMPANION_TRACK_NEWER;
	for (index = count - 1; index > nearest && newer < 0; index--)
	{
		float dx = track[index][0] - x, dy = track[index][1] - y;

		if (fabsf(track[index][2] - z) <= AE_COMPANION_TRACK_RISE && dx * dx + dy * dy <= near * near &&
			dx * dx + dy * dy <= AE_COMPANION_TRACK_ON * AE_COMPANION_TRACK_ON)
		{
			float along = 0.0f, gap_x = track[index][0] - track[nearest][0], gap_y = track[index][1] - track[nearest][1];
			int step;

			for (step = nearest; step < index; step++)
			{
				float sx = track[step + 1][0] - track[step][0], sy = track[step + 1][1] - track[step][1];

				along += sqrtf(sx * sx + sy * sy);
			}
			if (along > 2.0f * sqrtf(gap_x * gap_x + gap_y * gap_y) + AE_COMPANION_TRACK_STEP)
				newer = index;
		}
	}
	if (newer >= 0)
	{
		float dx = track[newer][0] - x, dy = track[newer][1] - y;

		nearest = newer;
		best = dx * dx + dy * dy;
	}
	if (best <= AE_COMPANION_TRACK_REACH * AE_COMPANION_TRACK_REACH && nearest + 1 < count)
		return nearest + 1;
	return nearest;
}

int ae_companion_track_behind(float const track[][3], int count, float px, float py, int taken)
{
	int index;

	for (index = count - 1; index >= 0; index--)
	{
		float dx = track[index][0] - px, dy = track[index][1] - py;

		if (dx * dx + dy * dy < AE_COMPANION_TRACK_BEHIND * AE_COMPANION_TRACK_BEHIND)
			continue;
		if (taken-- <= 0)
			return index;
	}
	return -1;
}

int ae_companion_narrow(int const drops[8])
{
	int index;

	for (index = 0; index < 4; index++)
		if (drops[index] && drops[index + 4])
			return 1;
	return 0;
}

int ae_companion_deck_candidates(float facing_i, float facing_j, float offsets[][2])
{
	static float const reach[2] = { 1.5f, 2.5f };
	float length = sqrtf(facing_i * facing_i + facing_j * facing_j), behind[AE_COMPANION_DECK_CANDIDATES];
	int index, count = 0;

	if (length < 1e-4f)
	{
		facing_i = 1.0f;
		facing_j = 0.0f;
		length = 1.0f;
	}
	for (index = 0; index < 8; index++)
	{
		float angle = 0.7853982f * (float)index, i = cosf(angle), j = sinf(angle);
		int far;

		for (far = 0; far < 2; far++)
		{
			offsets[count][0] = i * reach[far];
			offsets[count][1] = j * reach[far];
			/* (how far behind: the facing's opposite, the nearer of a direction's two first) */
			behind[count] = -(i * facing_i + j * facing_j) / length - 0.01f * (float)far;
			count++;
		}
	}
	/* (an insertion sort, the furthest behind first) */
	for (index = 1; index < count; index++)
	{
		float key = behind[index], i = offsets[index][0], j = offsets[index][1];
		int at = index - 1;

		while (at >= 0 && behind[at] < key)
		{
			behind[at + 1] = behind[at];
			offsets[at + 1][0] = offsets[at][0];
			offsets[at + 1][1] = offsets[at][1];
			at--;
		}
		behind[at + 1] = key;
		offsets[at + 1][0] = i;
		offsets[at + 1][1] = j;
	}
	return count;
}

int ae_companion_entry_floor_kind(int player_on_object, int floor_is_object)
{
	return !player_on_object || floor_is_object;
}

int ae_companion_intro_ride(char const *level, int forced)
{
	int index;

	if (forced)
		return 1;
	if (!level || !level[0])
		return 0;
	for (index = 0; intro_ride_levels[index]; index++)
		if (!strcmp(level, intro_ride_levels[index]))
			return 1;
	return 0;
}

int ae_companion_may_ask(int scenario_solo, int ai_ready, int engine_running, int menu_active, int network_client, int has_server,
	int dedicated_server)
{
	return scenario_solo && ai_ready && !engine_running && !menu_active && !network_client && has_server && !dedicated_server;
}

long ae_companion_wanted_count(long next_level, long next_level_setting, long override, long setting, long most)
{
	long count = next_level >= 0 && next_level_setting == setting ? next_level : override >= 0 ? override : setting;

	return count < 0 ? 0 : count > most ? most : count;
}

int ae_companion_player_down_counts(int is_squad_member, int respawn_mode)
{
	return !is_squad_member || respawn_mode;
}

int ae_companion_player_down_respawn(char const *word)
{
	return word && !strcmp(word, "respawn");
}

int ae_companion_rejoin_wanted(int no_player, long missing_ticks)
{
	return no_player && missing_ticks >= AE_COMPANION_REJOIN_TICKS;
}

int ae_companion_may_remove(int squad_added_it, int wanted)
{
	return squad_added_it && !wanted;
}

int ae_companion_track_push(float track[][3], unsigned char flags[], int *count, float x, float y, float z, int airborne_since,
	int crouched)
{
	if (*count > 0)
	{
		float const *last = track[*count - 1];
		float dx = x - last[0], dy = y - last[1], dz = z - last[2];

		if (dx * dx + dy * dy < AE_COMPANION_TRACK_STEP * AE_COMPANION_TRACK_STEP && fabsf(dz) < AE_COMPANION_TRACK_RISE)
			return 0;
		if (dx * dx + dy * dy > AE_COMPANION_TRACK_GAP * AE_COMPANION_TRACK_GAP || dz >= AE_COMPANION_TRACK_RISE ||
			dz < -AE_COMPANION_TRACK_DROP_KEPT)
			*count = 0;
		else
		{
			if (airborne_since)
				flags[*count - 1] |= AE_COMPANION_TRACK_JUMPED;
			if (dz <= -AE_COMPANION_TRACK_RISE)
				flags[*count - 1] |= AE_COMPANION_TRACK_DROPPED;
		}
	}
	if (*count == AE_COMPANION_TRACK_MOST)
	{
		memmove(track[0], track[1], sizeof track[0] * (AE_COMPANION_TRACK_MOST - 1));
		memmove(&flags[0], &flags[1], sizeof flags[0] * (AE_COMPANION_TRACK_MOST - 1));
		(*count)--;
	}
	track[*count][0] = x;
	track[*count][1] = y;
	track[*count][2] = z;
	flags[*count] = (unsigned char)(crouched ? AE_COMPANION_TRACK_CROUCHED : 0);
	(*count)++;
	return 1;
}

int ae_companion_track_takeoff(float track[][3], unsigned char flags[], int *count, float x, float y, float z)
{
	if (*count > 0)
	{
		float const *last = track[*count - 1];
		float dx = x - last[0], dy = y - last[1], dz = z - last[2];

		if (dx * dx + dy * dy <= AE_COMPANION_TRACK_TAKEOFF_NEAR * AE_COMPANION_TRACK_TAKEOFF_NEAR && fabsf(dz) < AE_COMPANION_TRACK_RISE)
			return 0;
		if (dx * dx + dy * dy >= AE_COMPANION_TRACK_STEP * AE_COMPANION_TRACK_STEP || fabsf(dz) >= AE_COMPANION_TRACK_RISE)
			return ae_companion_track_push(track, flags, count, x, y, z, 0, 0);
	}
	if (*count == AE_COMPANION_TRACK_MOST)
	{
		memmove(track[0], track[1], sizeof track[0] * (AE_COMPANION_TRACK_MOST - 1));
		memmove(&flags[0], &flags[1], sizeof flags[0] * (AE_COMPANION_TRACK_MOST - 1));
		(*count)--;
	}
	track[*count][0] = x;
	track[*count][1] = y;
	track[*count][2] = z;
	flags[*count] = 0;
	(*count)++;
	return 1;
}

int ae_companion_track_cue(float const track[][3], unsigned char const flags[], int count, int goal, float x, float y)
{
	int cue = 0;

	if (goal < 1 || goal >= count)
		return 0;
	{
		float dx = track[goal - 1][0] - x, dy = track[goal - 1][1] - y;

		if (dx * dx + dy * dy <= AE_COMPANION_TRACK_CUE * AE_COMPANION_TRACK_CUE)
			cue = flags[goal - 1];
	}
	return cue | (flags[goal] & AE_COMPANION_TRACK_CROUCHED);
}

int ae_companion_stuck_restarts(int indoors, int cut_off_seen, float distance, float window_nearest)
{
	return !indoors || distance < AE_COMPANION_STUCK_NEAR || !(cut_off_seen || distance > AE_COMPANION_STUCK_FAR) ||
		distance < window_nearest - AE_COMPANION_STUCK_GAIN;
}

int ae_companion_stuck_warp(int indoors, int cut_off, float distance, long ticks_stuck, int unseen, int seated, int fighting)
{
	if (!indoors || seated || fighting || distance < AE_COMPANION_STUCK_NEAR || !(cut_off || distance > AE_COMPANION_STUCK_FAR))
		return 0;
	return ticks_stuck >= (unseen ? AE_COMPANION_STUCK_TICKS : AE_COMPANION_STUCK_SEEN_TICKS);
}

int ae_companion_progress(float path_left_was, float path_left_now, int crumb_was, int crumb_now)
{
	return crumb_now != crumb_was || path_left_was - path_left_now >= AE_COMPANION_PROGRESS;
}

int ae_companion_warp_due(int path_failed, int track_progress, long ticks_no_progress, int seen)
{
	return path_failed && !track_progress &&
		ticks_no_progress >= (seen ? AE_COMPANION_WARP_SEEN_TICKS : AE_COMPANION_WARP_UNSEEN_TICKS);
}

int ae_companion_warp_now(int teleported, int lift_gone, int door_locked_between, int drop_over_rule)
{
	return teleported ? AE_COMPANION_WARP_TELEPORT : lift_gone ? AE_COMPANION_WARP_LIFT :
		door_locked_between ? AE_COMPANION_WARP_LOCKED : drop_over_rule ? AE_COMPANION_WARP_DROP : AE_COMPANION_WARP_NONE;
}

int ae_companion_link_kind(float across, float up, int in_vehicle)
{
	if (across > AE_COMPANION_TRACK_GAP)
		return in_vehicle ? AE_COMPANION_LINK_VEHICLE : AE_COMPANION_LINK_TELEPORT;
	if (up <= -AE_COMPANION_TRACK_RISE)
		return AE_COMPANION_LINK_DROP;
	if (up >= AE_COMPANION_TRACK_RISE)
		return AE_COMPANION_LINK_RISE;
	return AE_COMPANION_LINK_NONE;
}

int ae_companion_drop_allowed(float link_drop, int player_hurt, float drop)
{
	return !player_hurt && drop <= link_drop + AE_COMPANION_DROP_SPARE;
}

int ae_companion_teleported(float moved_in_a_tick)
{
	return moved_in_a_tick > AE_COMPANION_TELEPORT;
}

int ae_companion_warp_far(float distance)
{
	return distance > AE_COMPANION_WARP_NEAR;
}

int ae_companion_walked(float moved)
{
	return moved >= AE_COMPANION_WALKED;
}
