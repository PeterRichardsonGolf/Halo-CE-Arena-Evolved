/*
AE_SPAWNER_HOLO_RULES.C

The spawner hologram's pure rules (ae_spawner_holo_rules.h).
*/

#include <math.h>

#include "ae_spawner_holo_rules.h"

#define TICKS 30

int ae_holo_clip_triangle(struct ae_holo_point3 const triangle[3], struct ae_holo_point3 axis, float limit,
	struct ae_holo_point3 out[4])
{
	int n = 0, index;

	if (!triangle || !out) return 0;
	for (index = 0; index < 3; index++)
	{
		struct ae_holo_point3 a = triangle[index], b = triangle[(index + 1) % 3];
		float da = a.x * axis.x + a.y * axis.y + a.z * axis.z - limit, db = b.x * axis.x + b.y * axis.y + b.z * axis.z - limit;

		if (da <= 0.0f && n < 4) out[n++] = a;
		if (((da < 0.0f && db > 0.0f) || (da > 0.0f && db < 0.0f)) && n < 4)
		{
			float t = da / (da - db);

			out[n].x = a.x + (b.x - a.x) * t;
			out[n].y = a.y + (b.y - a.y) * t;
			out[n].z = a.z + (b.z - a.z) * t;
			n++;
		}
	}
	return n >= 3 ? n : 0;
}

int ae_holo_cut_triangle(struct ae_holo_point3 const triangle[3], struct ae_holo_point3 axis, float limit,
	struct ae_holo_point3 *from, struct ae_holo_point3 *to)
{
	struct ae_holo_point3 ends[2];
	int n = 0, index;

	if (!triangle || !from || !to) return 0;
	for (index = 0; index < 3 && n < 2; index++)
	{
		struct ae_holo_point3 a = triangle[index], b = triangle[(index + 1) % 3];
		float da = a.x * axis.x + a.y * axis.y + a.z * axis.z - limit, db = b.x * axis.x + b.y * axis.y + b.z * axis.z - limit;

		if ((da < 0.0f && db >= 0.0f) || (da >= 0.0f && db < 0.0f))
		{
			float t = da / (da - db);

			ends[n].x = a.x + (b.x - a.x) * t;
			ends[n].y = a.y + (b.y - a.y) * t;
			ends[n].z = a.z + (b.z - a.z) * t;
			n++;
		}
	}
	if (n < 2) return 0;
	*from = ends[0];
	*to = ends[1];
	return 1;
}

float ae_holo_line_width(float distance, float pixels)
{
	float width = distance * AE_HOLO_RADIANS_PER_PIXEL * pixels;

	return width < AE_HOLO_LINE_WIDTH_MINIMUM ? AE_HOLO_LINE_WIDTH_MINIMUM : width;
}

float ae_holo_fill(long ticks_left, long period)
{
	float fill;

	if (period <= 0 || ticks_left <= 0) return 1.0f;
	fill = 1.0f - (float)ticks_left / (float)period;
	return fill < 0.0f ? 0.0f : fill > 1.0f ? 1.0f : fill;
}

float ae_holo_edge(float low, float high, float fill)
{
	return low + (high - low) * (fill < 0.0f ? 0.0f : fill > 1.0f ? 1.0f : fill);
}

float ae_holo_distance_alpha(float distance)
{
	if (distance <= AE_HOLO_FULL_DISTANCE) return 1.0f;
	if (distance >= AE_HOLO_GONE_DISTANCE) return 0.0f;
	return (AE_HOLO_GONE_DISTANCE - distance) / (AE_HOLO_GONE_DISTANCE - AE_HOLO_FULL_DISTANCE);
}

float ae_holo_pulse(long ticks_left)
{
	long into;

	if (ticks_left <= 0 || ticks_left > AE_HOLO_PULSE_SECONDS * TICKS) return 1.0f;
	into = ticks_left % TICKS; /* 0 .. 29 inside each second */
	/* bright at the second's start, down to 0.55 by its middle, back up */
	{
		float phase = (float)into / (float)TICKS;
		float dip = phase < 0.5f ? phase * 2.0f : (1.0f - phase) * 2.0f;

		return 0.55f + 0.45f * (1.0f - dip);
	}
}

int ae_holo_seen(int spawners_weapon, int ready)
{
	if (!spawners_weapon) return 0;
	return AE_HOLO_SEEN_PRESENT | (ready ? AE_HOLO_SEEN_POSE : 0);
}

void ae_holo_first_pose(int faces_item, float facing, struct ae_holo_point3 *forward, struct ae_holo_point3 *up)
{
	forward->x = faces_item ? (float)cos(facing) : 1.0f;
	forward->y = faces_item ? (float)sin(facing) : 0.0f;
	forward->z = 0.0f;
	up->x = -forward->y;
	up->y = forward->x;
	up->z = 0.0f;
}

int ae_holo_outline_ink(int power, int red)
{
	if (red) return AE_HOLO_INK_RED;
	return power ? AE_HOLO_INK_WHITE : AE_HOLO_INK_ICE;
}

int ae_holo_cache_slot(long const *used, int count)
{
	int index, oldest = 0;

	if (!used || count <= 0) return 0;
	for (index = 0; index < count; index++)
	{
		if (used[index] < 0) return index;
		if (used[index] < used[oldest]) oldest = index;
	}
	return oldest;
}
