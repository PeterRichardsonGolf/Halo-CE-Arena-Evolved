/*
AE_BOTS_VANTAGE_RULES.C

Arena Evolved: the map's vantage spots for the bots (ae_bots_vantage_rules.h). From halo-ce-vita (GPL-3.0) via Halo Content
Evolved (Watanoge/halo-content-evolved, AGPL-3.0-or-later).
*/

#include "ae_bots_vantage_rules.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

static float distance3d(float ax, float ay, float az, float bx, float by, float bz)
{
	float dx = ax - bx, dy = ay - by, dz = az - bz;

	return (float)sqrt(dx * dx + dy * dy + dz * dz);
}

static float pin(float value, float low, float high)
{
	return value < low ? low : value > high ? high : value;
}

float ae_vantage_range(float distance)
{
	if (distance < 5.0f)
		return 0.2f;
	if (distance < 12.0f)
		return 0.2f + 0.8f * (distance - 5.0f) / 7.0f;
	if (distance < 50.0f)
		return 1.0f;
	if (distance < AE_VANTAGE_SIGHT)
		return 1.0f - 0.7f * (distance - 50.0f) / (AE_VANTAGE_SIGHT - 50.0f);
	return 0.0f;
}

float ae_vantage_first_score(float height, struct ae_vantage_place const *places, int place_count, float x, float y, float z)
{
	float score = pin(height, -2.0f, 8.0f);
	int place;

	for (place = 0; place < place_count; place++)
		score += 0.25f * places[place].weight *
			ae_vantage_range(distance3d(x, y, z, places[place].x, places[place].y, places[place].z));
	return score;
}

void ae_vantage_consider(struct ae_vantage_spot *candidates, int *count, struct ae_vantage_spot const *spot)
{
	int index, worst = -1;

	for (index = 0; index < *count; index++)
	{
		struct ae_vantage_spot *other = &candidates[index];

		if (distance3d(other->x, other->y, other->z, spot->x, spot->y, spot->z) < AE_VANTAGE_CANDIDATE_SPACING)
		{
			if (other->first_score < spot->first_score)
				*other = *spot;
			return;
		}
	}
	if (*count < AE_VANTAGE_CANDIDATES)
	{
		candidates[(*count)++] = *spot;
		return;
	}
	for (index = 0; index < *count; index++)
		if (worst == -1 || candidates[index].first_score < candidates[worst].first_score)
			worst = index;
	if (candidates[worst].first_score < spot->first_score)
		candidates[worst] = *spot;
}

void ae_vantage_score(struct ae_vantage_spot *spot, struct ae_vantage_place const *places, int place_count)
{
	int place;

	spot->score = 0.8f * pin(spot->height, 0.0f, 6.0f) + 2.5f * (float)spot->open / (float)AE_VANTAGE_OPEN_RAYS;
	spot->sees_count = 0;
	for (place = 0; place < place_count; place++)
		if (spot->sees & (1UL << place))
		{
			spot->score += places[place].weight *
				ae_vantage_range(distance3d(spot->x, spot->y, spot->z, places[place].x, places[place].y, places[place].z));
			spot->sees_count++;
		}
}

int ae_vantage_choose(struct ae_vantage_spot const *candidates, int count, struct ae_vantage_spot *points)
{
	int chosen = 0;

	while (chosen < AE_VANTAGE_MAXIMUM)
	{
		int index, other, best = -1;

		for (index = 0; index < count; index++)
		{
			struct ae_vantage_spot const *spot = &candidates[index];
			int near = 0;

			if (spot->sees_count < AE_VANTAGE_MINIMUM_SEEN || (best != -1 && spot->score <= candidates[best].score))
				continue;
			for (other = 0; other < chosen && !near; other++)
				near = distance3d(points[other].x, points[other].y, points[other].z, spot->x, spot->y, spot->z) < AE_VANTAGE_SPACING;
			if (!near)
				best = index;
		}
		if (best == -1)
			break;
		points[chosen++] = candidates[best];
	}
	return chosen;
}

/* ---------- the height grid */

int ae_vantage_grid_begin(struct ae_vantage_grid *grid, float x0, float y0, float x1, float y1, float cell, long maximum)
{
	float width = x1 - x0, height = y1 - y0, side = width > height ? width : height;
	long cells;

	memset(grid, 0, sizeof(*grid));
	grid->x = x0;
	grid->y = y0;
	grid->cell = side / (float)maximum > cell ? side / (float)maximum : cell;
	grid->width = (long)(width / grid->cell) + 1;
	grid->height = (long)(height / grid->cell) + 1;
	cells = grid->width * grid->height;
	grid->area = (float *)calloc((size_t)cells, sizeof(float));
	grid->height_sum = (float *)calloc((size_t)cells, sizeof(float));
	if (!grid->area || !grid->height_sum)
	{
		ae_vantage_grid_end(grid);
		return 0;
	}
	return 1;
}

void ae_vantage_grid_add(struct ae_vantage_grid *grid, float x, float y, float z, float area)
{
	long cx = (long)((x - grid->x) / grid->cell), cy = (long)((y - grid->y) / grid->cell);

	if (x < grid->x || y < grid->y || cx < 0 || cy < 0 || cx >= grid->width || cy >= grid->height)
		return;
	grid->area[cy * grid->width + cx] += area;
	grid->height_sum[cy * grid->width + cx] += area * z;
}

float ae_vantage_grid_height(struct ae_vantage_grid const *grid, float x, float y, float z)
{
	long cx = (long)((x - grid->x) / grid->cell), cy = (long)((y - grid->y) / grid->cell);
	float area = 0.0f, sum = 0.0f;
	long ix, iy;

	for (iy = cy - 1; iy <= cy + 1; iy++)
		for (ix = cx - 1; ix <= cx + 1; ix++)
		{
			if (ix < 0 || iy < 0 || ix >= grid->width || iy >= grid->height)
				continue;
			area += grid->area[iy * grid->width + ix];
			sum += grid->height_sum[iy * grid->width + ix];
		}
	return area > 0.0f ? z - sum / area : 0.0f;
}

void ae_vantage_grid_end(struct ae_vantage_grid *grid)
{
	free(grid->area);
	free(grid->height_sum);
	grid->area = NULL;
	grid->height_sum = NULL;
}

/* ---------- holding a spot */

int ae_vantage_weapon_class(int sniper_rifle, int precision_rifle, int rocket_launcher)
{
	if (rocket_launcher)
		return AE_VANTAGE_WEAPON_HEAVY;
	if (sniper_rifle)
		return AE_VANTAGE_WEAPON_LONG;
	return precision_rifle ? AE_VANTAGE_WEAPON_RIFLE : AE_VANTAGE_WEAPON_NONE;
}

int ae_vantage_free_class(int weapon_class, int has_weapon, int high_ground)
{
	return weapon_class == AE_VANTAGE_WEAPON_NONE && has_weapon && high_ground ? AE_VANTAGE_WEAPON_ANY : weapon_class;
}

int ae_vantage_cap(int team_bots, int weapon)
{
	int cap = weapon == AE_VANTAGE_WEAPON_LONG ? team_bots / 2 : team_bots / 3;

	return cap < 1 ? 1 : cap;
}

int ae_vantage_may_take(int holders, int team_bots, int weapon)
{
	return weapon != AE_VANTAGE_WEAPON_NONE && team_bots > 0 && holders < ae_vantage_cap(team_bots, weapon);
}

int ae_vantage_pick(int count, float const *scores, float const *distances, unsigned long excluded, int last, unsigned long failed,
	float maximum_distance, float const *noise)
{
	int index, best = -1;
	float best_score = 0.0f;

	for (index = 0; index < count && index < 32; index++)
	{
		float score;

		if ((excluded & (1UL << index)) || (failed & (1UL << index)) || (index == last && count > 1) || distances[index] > maximum_distance)
			continue;
		score = scores[index] - distances[index] / 10.0f + noise[index];
		if (best == -1 || score > best_score)
		{
			best_score = score;
			best = index;
		}
	}
	return best;
}

long ae_vantage_hold_ticks(float unit_random, long ticks_per_second)
{
	float seconds;

	unit_random = pin(unit_random, 0.0f, 1.0f);
	seconds = (float)AE_VANTAGE_HOLD_SECONDS_LEAST + unit_random * (float)(AE_VANTAGE_HOLD_SECONDS_MOST - AE_VANTAGE_HOLD_SECONDS_LEAST);
	return (long)(seconds * (float)ticks_per_second);
}

long ae_vantage_hold_ticks_for(int weapon, float unit_random, long ticks_per_second)
{
	float seconds;

	if (weapon != AE_VANTAGE_WEAPON_ANY)
		return ae_vantage_hold_ticks(unit_random, ticks_per_second);
	unit_random = pin(unit_random, 0.0f, 1.0f);
	seconds = (float)AE_VANTAGE_ANY_HOLD_SECONDS_LEAST +
		unit_random * (float)(AE_VANTAGE_ANY_HOLD_SECONDS_MOST - AE_VANTAGE_ANY_HOLD_SECONDS_LEAST);
	return (long)(seconds * (float)ticks_per_second);
}

int ae_vantage_ammunition_low(int rounds_left, int rounds_most)
{
	return rounds_most > 0 && rounds_left * 4 < rounds_most;
}

int ae_vantage_leave(int weapon_now, int weapon_held, int flanked, float enemy_distance, float vitality, int ammunition_low,
	long held_ticks, long hold_ticks)
{
	if (weapon_now != weapon_held)
		return AE_VANTAGE_LEFT_WEAPON;
	if (flanked)
		return AE_VANTAGE_LEFT_FLANKED;
	/* (0.9.0: a bot holding high ground with any weapon fights an enemy who comes near from there, as it fights anywhere) */
	if (enemy_distance >= 0.0f && enemy_distance < AE_VANTAGE_ENEMY_NEAR && weapon_held != AE_VANTAGE_WEAPON_ANY)
		return AE_VANTAGE_LEFT_ENEMY;
	if (vitality < 1.0f)
		return AE_VANTAGE_LEFT_HURT;
	if (ammunition_low)
		return AE_VANTAGE_LEFT_AMMO;
	if (held_ticks >= hold_ticks)
		return AE_VANTAGE_LEFT_TIME;
	return AE_VANTAGE_STAY;
}

char const *ae_vantage_leave_name(int reason)
{
	static char const *const names[AE_VANTAGE_LEFT_REASONS] =
		{ "stay", "weapon", "flanked", "enemy", "hurt", "ammo", "time", "order", "objective", "power", "died" };

	return reason >= 0 && reason < AE_VANTAGE_LEFT_REASONS ? names[reason] : "?";
}

long ae_vantage_rest_ticks(int reason, long ticks_per_second)
{
	return (reason == AE_VANTAGE_LEFT_POWER ? AE_VANTAGE_POWER_REST_SECONDS : AE_VANTAGE_REST_SECONDS) * ticks_per_second;
}

float ae_vantage_max_distance(int weapon)
{
	return weapon == AE_VANTAGE_WEAPON_ANY ? AE_VANTAGE_ANY_MAX_DISTANCE : AE_VANTAGE_MAX_DISTANCE;
}
