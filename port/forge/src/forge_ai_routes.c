/*
FORGE_AI_ROUTES.C

Forge's AI routes (forge_ai_routes.h): their points, numbered and kept in
order; the Object Settings of a point and its route's page; the lines drawn
between them in Forge mode; and their saved lines. The AI walks them as its
squad's move positions (forge_ai.c).
*/

#include "cseries.h"
#include "cseries/errors.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "forge_engine.h"
#include "forge_ai_routes.h"

/* ---------- constants */

/* (a new route's second point, this far ahead of its first) */
#define ROUTE_FIRST_LEG 4.0f

/* ---------- globals */

/* a point's WAIT TIME, seconds, by its index */
static short const s_route_waits[16] = { 0, 1, 2, 3, 4, 5, 6, 8, 10, 12, 15, 20, 25, 30, 45, 60 };
static char const *const s_behavior_names[NUMBER_OF_FORGE_AI_ROUTE_BEHAVIORS] = { "PING PONG", "STAY AT END", "LOOP" };

/* the ROUTE SETTINGS page: open for this point (-1: closed) */
static int32_t s_settings_live_idx = -1;
/* the ADD row's button chosen: 0 BEFORE, 1 AFTER */
static int s_add_side = 1;
/* the panel's title */
static char s_title[48];

/* a new route's second point, following its first while it is held */
static struct
{
	int32_t first;
	int32_t second;
	float x, y, z;
} s_follow = { -1, -1 };

/* ---------- private code */

/* a point asked for at x, y, z, placed from `from`: on the ground nearest it
that `from` sees (forge_ai_route_place_toward) */
static void route_ground_place(struct forge_live_instance const *from, float *x, float *y, float *z)
{
	float start[3], want[3], at[3];

	start[0] = from->x;
	start[1] = from->y;
	start[2] = from->z;
	want[0] = *x;
	want[1] = *y;
	want[2] = *z;
	forge_ai_route_place_toward(start, want, at);
	*x = at[0];
	*y = at[1];
	*z = at[2];
}

static struct forge_live_instance *route_point(int32_t live_idx)
{
	struct forge_live_instance *inst;

	if (live_idx < 0 || live_idx >= g_forge.live_instance_count)
		return NULL;
	inst = &g_forge.live_instances[live_idx];
	return forge_ai_route_is(inst) ? inst : NULL;
}

static int route_number_used(int number)
{
	int32_t i;

	for (i = 0; i < g_forge.live_instance_count; i++)
	{
		struct forge_live_instance const *inst = &g_forge.live_instances[i];

		if (forge_ai_route_is(inst) && FORGE_AI_ROUTE_NUMBER(inst->team_designator) == number)
			return 1;
	}
	return 0;
}

static int route_free_number(void)
{
	int number;

	for (number = 1; number <= FORGE_AI_ROUTE_MAX; number++)
	{
		if (!route_number_used(number))
			return number;
	}
	return 0;
}

/* a point's usage set, and sent to the session's others */
static void route_point_set_usage(int32_t live_idx, int16_t usage)
{
	struct forge_live_instance *inst = route_point(live_idx);

	if (!inst || inst->team_designator == usage)
		return;
	inst->team_designator = usage;
	forge_network_send_marker_usage(live_idx, usage);
}

/* the route's points numbered 0 on, in their order, leaving `gap` (-1: none)
free for a new one */
static void route_renumber(int number, int gap)
{
	int32_t points[FORGE_AI_ROUTE_MAX_POINTS];
	int count = forge_ai_route_points(number, points, FORGE_AI_ROUTE_MAX_POINTS);
	int k;

	for (k = 0; k < count; k++)
	{
		int16_t usage = g_forge.live_instances[points[k]].team_designator;
		int order = gap >= 0 && k >= gap ? k + 1 : k;

		route_point_set_usage(points[k], FORGE_AI_ROUTE_USAGE(number, order, FORGE_AI_ROUTE_BEHAVIOR(usage),
			FORGE_AI_ROUTE_WAIT_INDEX(usage)));
	}
}

static int32_t route_point_spawn(int number, int order, int behavior, float x, float y, float z, float yaw)
{
	return forge_3d_spawn_object_at(FORGE_GROUP_AI_ROUTE, FORGE_AI_ROUTE_PATH, 1, x, y, z, yaw, 0.0f, 0.0f,
		FORGE_AI_ROUTE_USAGE(number, order, behavior, 0), 1);
}

/* the place of a point on its route (0 first), and how many it has */
static int route_rank(int32_t live_idx, int *count)
{
	struct forge_live_instance const *inst = route_point(live_idx);
	int32_t points[FORGE_AI_ROUTE_MAX_POINTS];
	int rank = 0;

	*count = 0;
	if (!inst)
		return 0;
	*count = forge_ai_route_points(FORGE_AI_ROUTE_NUMBER(inst->team_designator), points, FORGE_AI_ROUTE_MAX_POINTS);
	while (rank < *count && points[rank] != live_idx)
		rank++;
	return rank;
}

/* a new point beside this one: halfway to its neighbor on that side, or on
past the end the way the route was going there */
static void route_insert(int32_t live_idx, int after)
{
	struct forge_live_instance const *inst = route_point(live_idx);
	int32_t points[FORGE_AI_ROUTE_MAX_POINTS];
	int number, count, rank, neighbor, new_rank;
	float x, y, z;
	int32_t made;

	if (!inst)
		return;
	number = FORGE_AI_ROUTE_NUMBER(inst->team_designator);
	count = forge_ai_route_points(number, points, FORGE_AI_ROUTE_MAX_POINTS);
	if (count >= FORGE_AI_ROUTE_MAX_POINTS)
	{
		forge_set_status("This AI Route Is Full (%d Points)", FORGE_AI_ROUTE_MAX_POINTS);
		return;
	}
	for (rank = 0; rank < count && points[rank] != live_idx; rank++)
	{
	}
	if (rank == count)
		return;
	neighbor = after ? rank + 1 : rank - 1;
	if (neighbor >= 0 && neighbor < count)
	{
		struct forge_live_instance const *other = &g_forge.live_instances[points[neighbor]];

		x = (inst->x + other->x) * 0.5f;
		y = (inst->y + other->y) * 0.5f;
		z = (inst->z + other->z) * 0.5f;
	}
	else
	{
		/* (on past the end: as far again as the last leg, or ahead of it) */
		int inner = after ? rank - 1 : rank + 1;

		if (inner >= 0 && inner < count)
		{
			struct forge_live_instance const *other = &g_forge.live_instances[points[inner]];

			x = inst->x + (inst->x - other->x);
			y = inst->y + (inst->y - other->y);
			z = inst->z + (inst->z - other->z);
		}
		else
		{
			float sign = after ? 1.0f : -1.0f;

			x = inst->x + (float)cos((double)inst->yaw) * ROUTE_FIRST_LEG * sign;
			y = inst->y + (float)sin((double)inst->yaw) * ROUTE_FIRST_LEG * sign;
			z = inst->z;
		}
	}
	/* (on ground the point it is added from sees: not behind a wall, out of
	the map, under it or in the air) */
	route_ground_place(inst, &x, &y, &z);
	new_rank = after ? rank + 1 : rank;
	route_renumber(number, new_rank);
	made = route_point_spawn(number, new_rank, FORGE_AI_ROUTE_BEHAVIOR(inst->team_designator), x, y, z, inst->yaw);
	if (made >= 0)
		forge_set_status("Added Point %d to AI Route #%d", new_rank + 1, number);
}

static void route_delete(int number)
{
	int32_t i;

	for (i = 0; i < g_forge.live_instance_count; i++)
	{
		if (forge_ai_route_is(&g_forge.live_instances[i]) &&
			FORGE_AI_ROUTE_NUMBER(g_forge.live_instances[i].team_designator) == number)
		{
			forge_3d_delete_instance(i, 1);
		}
	}
}

/* ---------- public code */

int forge_ai_route_is(struct forge_live_instance const *inst)
{
	return inst && inst->active && inst->group_tag == FORGE_GROUP_AI_ROUTE;
}

void forge_ai_route_spawned(int32_t live_idx, int broadcast_net)
{
	struct forge_live_instance *inst = route_point(live_idx);
	int number;

	if (!inst)
		return;
	/* (a map's or a peer's point is already of a route) */
	if (FORGE_AI_ROUTE_NUMBER(inst->team_designator) != 0 || !broadcast_net)
		return;
	number = route_free_number();
	if (number == 0)
	{
		forge_set_status("No Free AI Route Numbers (%d in this map)", FORGE_AI_ROUTE_MAX);
		inst->active = 0;
		return;
	}
	inst->team_designator = FORGE_AI_ROUTE_USAGE(number, 0, _forge_ai_route_ping_pong, 0);
}

void forge_ai_route_begin(int32_t live_idx)
{
	struct forge_live_instance const *inst = route_point(live_idx);
	int32_t second;
	int number;
	float x, y, z;

	if (!inst)
		return;
	number = FORGE_AI_ROUTE_NUMBER(inst->team_designator);
	x = inst->x + (float)cos((double)inst->yaw) * ROUTE_FIRST_LEG;
	y = inst->y + (float)sin((double)inst->yaw) * ROUTE_FIRST_LEG;
	z = inst->z;
	route_ground_place(inst, &x, &y, &z);
	second = route_point_spawn(number, 1, FORGE_AI_ROUTE_BEHAVIOR(inst->team_designator), x, y, z, inst->yaw);
	if (second < 0)
		return;
	s_follow.first = live_idx;
	s_follow.second = second;
	s_follow.x = inst->x;
	s_follow.y = inst->y;
	s_follow.z = inst->z;
	forge_set_status("AI Route #%d: place its start, then move its points (ADD POINT in their settings)", number);
}

void forge_ai_route_point_deleted(int number)
{
	if (number > 0)
		route_renumber(number, -1);
	if (s_settings_live_idx >= 0 && !route_point(s_settings_live_idx))
		s_settings_live_idx = -1;
}

int16_t forge_ai_route_copy_usage(int32_t live_idx)
{
	struct forge_live_instance const *inst = route_point(live_idx);
	int count = 0, rank, number;

	if (!inst)
		return 0;
	number = FORGE_AI_ROUTE_NUMBER(inst->team_designator);
	rank = route_rank(live_idx, &count);
	if (count >= FORGE_AI_ROUTE_MAX_POINTS)
	{
		forge_set_status("This AI Route Is Full (%d Points)", FORGE_AI_ROUTE_MAX_POINTS);
		return 0;
	}
	route_renumber(number, rank + 1);
	return FORGE_AI_ROUTE_USAGE(number, rank + 1, FORGE_AI_ROUTE_BEHAVIOR(inst->team_designator), 0);
}

void forge_ai_route_usage_apply(int32_t live_idx, int16_t usage)
{
	struct forge_live_instance *inst = route_point(live_idx);

	if (inst)
		inst->team_designator = usage;
}

int forge_ai_route_numbers(int *numbers, int max)
{
	int count = 0, number;

	for (number = 1; number <= FORGE_AI_ROUTE_MAX && count < max; number++)
	{
		if (route_number_used(number))
			numbers[count++] = number;
	}
	return count;
}

int forge_ai_route_points(int number, int32_t *points, int max)
{
	int count = 0;
	int32_t i;

	if (number <= 0)
		return 0;
	for (i = 0; i < g_forge.live_instance_count && count < max; i++)
	{
		struct forge_live_instance const *inst = &g_forge.live_instances[i];
		int order, slot;

		if (!forge_ai_route_is(inst) || FORGE_AI_ROUTE_NUMBER(inst->team_designator) != number)
			continue;
		order = FORGE_AI_ROUTE_ORDER(inst->team_designator);
		for (slot = count;
			slot > 0 && FORGE_AI_ROUTE_ORDER(g_forge.live_instances[points[slot - 1]].team_designator) > order;
			slot--)
		{
			points[slot] = points[slot - 1];
		}
		points[slot] = i;
		count++;
	}
	return count;
}

int forge_ai_route_behavior(int number)
{
	int32_t points[1];

	if (forge_ai_route_points(number, points, 1) == 0)
		return _forge_ai_route_ping_pong;
	return FORGE_AI_ROUTE_BEHAVIOR(g_forge.live_instances[points[0]].team_designator);
}

int forge_ai_route_wait_seconds(int16_t usage)
{
	return s_route_waits[FORGE_AI_ROUTE_WAIT_INDEX(usage)];
}

char const *forge_ai_route_behavior_name(int behavior)
{
	return behavior >= 0 && behavior < NUMBER_OF_FORGE_AI_ROUTE_BEHAVIORS ? s_behavior_names[behavior] : "?";
}

void forge_ai_routes_update(void)
{
	struct forge_live_instance const *first;
	struct forge_live_instance const *second;

	if (s_follow.first < 0)
		return;
	first = route_point(s_follow.first);
	second = route_point(s_follow.second);
	if (!first || !second || g_forge.grabbed_live_index != s_follow.first)
	{
		s_follow.first = -1;
		s_follow.second = -1;
		return;
	}
	if (first->x != s_follow.x || first->y != s_follow.y || first->z != s_follow.z)
	{
		/* (its leg ahead of the first, kept to the ground the first sees) */
		float x = first->x + (float)cos((double)first->yaw) * ROUTE_FIRST_LEG;
		float y = first->y + (float)sin((double)first->yaw) * ROUTE_FIRST_LEG;
		float z = first->z;

		route_ground_place(first, &x, &y, &z);
		forge_3d_move_instance_ext(s_follow.second, x, y, z, first->yaw, 0.0f, 0.0f, 0, 1);
		s_follow.x = first->x;
		s_follow.y = first->y;
		s_follow.z = first->z;
	}
}

void forge_ai_routes_level_reset(void)
{
	s_settings_live_idx = -1;
	s_follow.first = -1;
	s_follow.second = -1;
}

void forge_ai_routes_render(void (*draw_line)(float const *from, float const *to, int color, int highlighted))
{
	int numbers[FORGE_AI_ROUTE_MAX];
	int route_count, r;

	if (!draw_line)
		return;
	route_count = forge_ai_route_numbers(numbers, FORGE_AI_ROUTE_MAX);
	for (r = 0; r < route_count; r++)
	{
		int32_t points[FORGE_AI_ROUTE_MAX_POINTS];
		int count = forge_ai_route_points(numbers[r], points, FORGE_AI_ROUTE_MAX_POINTS);
		int highlighted = 0, k;

		for (k = 0; k < count; k++)
		{
			highlighted = highlighted || points[k] == g_forge.grabbed_live_index ||
				points[k] == g_forge.hovered_live_index;
		}
		for (k = 0; k + 1 < count; k++)
		{
			struct forge_live_instance const *a = &g_forge.live_instances[points[k]];
			struct forge_live_instance const *b = &g_forge.live_instances[points[k + 1]];
			float from[3], to[3];

			from[0] = a->x;
			from[1] = a->y;
			from[2] = a->z + 0.15f;
			to[0] = b->x;
			to[1] = b->y;
			to[2] = b->z + 0.15f;
			draw_line(from, to, 0, highlighted);
		}
		if (count > 2 && forge_ai_route_behavior(numbers[r]) == _forge_ai_route_loop)
		{
			struct forge_live_instance const *a = &g_forge.live_instances[points[count - 1]];
			struct forge_live_instance const *b = &g_forge.live_instances[points[0]];
			float from[3], to[3];

			from[0] = a->x;
			from[1] = a->y;
			from[2] = a->z + 0.15f;
			to[0] = b->x;
			to[1] = b->y;
			to[2] = b->z + 0.15f;
			draw_line(from, to, 1, highlighted);
		}
	}
}

int forge_ai_route_rows(int32_t live_idx, int *rows)
{
	int count = 0;

	if (!route_point(live_idx))
		return 0;
	if (s_settings_live_idx >= 0 && s_settings_live_idx != live_idx)
		s_settings_live_idx = -1;
	if (s_settings_live_idx == live_idx)
	{
		rows[count++] = _forge_ai_route_row_number;
		rows[count++] = _forge_ai_route_row_behavior;
		rows[count++] = _forge_ai_route_row_delete_route;
		rows[count++] = _forge_ai_route_row_back;
		return count;
	}
	rows[count++] = _forge_ai_route_row_settings;
	rows[count++] = _forge_ai_route_row_wait;
	rows[count++] = _forge_ai_route_row_rot_snap;
	rows[count++] = _forge_ai_route_row_floor;
	rows[count++] = _forge_ai_route_row_delete;
	rows[count++] = _forge_ai_route_row_add;
	return count;
}

char const *forge_ai_route_panel_title(int32_t live_idx)
{
	struct forge_live_instance const *inst = route_point(live_idx);
	int count = 0, rank;

	if (!inst)
		return "AI ROUTE POINT";
	rank = route_rank(live_idx, &count);
	if (s_settings_live_idx >= 0 && s_settings_live_idx == live_idx)
		snprintf(s_title, sizeof(s_title), "AI ROUTE #%d SETTINGS", FORGE_AI_ROUTE_NUMBER(inst->team_designator));
	else
		snprintf(s_title, sizeof(s_title), "AI ROUTE #%d POINT %d OF %d", FORGE_AI_ROUTE_NUMBER(inst->team_designator),
			rank + 1, count);
	return s_title;
}

void forge_ai_route_row_text(int32_t live_idx, int row, char *label, unsigned long label_size, char *value,
	unsigned long value_size)
{
	struct forge_live_instance const *inst = route_point(live_idx);
	int count = 0, rank;

	label[0] = 0;
	value[0] = 0;
	if (!inst)
		return;
	rank = route_rank(live_idx, &count);
	switch (row)
	{
	case _forge_ai_route_row_point:
		snprintf(label, label_size, "POINT:");
		snprintf(value, value_size, "%d OF %d", rank + 1, count);
		break;
	case _forge_ai_route_row_wait:
		snprintf(label, label_size, "POINT WAIT TIME:");
		snprintf(value, value_size, "%d SECONDS", forge_ai_route_wait_seconds(inst->team_designator));
		break;
	case _forge_ai_route_row_add_before:
		snprintf(label, label_size, "ADD POINT BEFORE");
		break;
	case _forge_ai_route_row_add_after:
		snprintf(label, label_size, "ADD POINT AFTER");
		break;
	case _forge_ai_route_row_settings:
		snprintf(label, label_size, "ROUTE #%d SETTINGS", FORGE_AI_ROUTE_NUMBER(inst->team_designator));
		snprintf(value, value_size, "%s", forge_ai_route_behavior_name(FORGE_AI_ROUTE_BEHAVIOR(inst->team_designator)));
		break;
	case _forge_ai_route_row_add:
		snprintf(label, label_size, "ADD POINT BEFORE");
		snprintf(value, value_size, "ADD POINT AFTER");
		break;
	case _forge_ai_route_row_number:
		snprintf(label, label_size, "ROUTE NUMBER:");
		snprintf(value, value_size, "#%d (%d POINTS)", FORGE_AI_ROUTE_NUMBER(inst->team_designator), count);
		break;
	case _forge_ai_route_row_behavior:
		snprintf(label, label_size, "ROUTE BEHAVIOR:");
		snprintf(value, value_size, "%s", forge_ai_route_behavior_name(FORGE_AI_ROUTE_BEHAVIOR(inst->team_designator)));
		break;
	case _forge_ai_route_row_delete_route:
		snprintf(label, label_size, "DELETE ROUTE");
		break;
	case _forge_ai_route_row_back:
		snprintf(label, label_size, "< BACK");
		break;
	default:
		break;
	}
}

int forge_ai_route_row_press(int32_t live_idx, int row, int dir, int enter)
{
	struct forge_live_instance const *inst = route_point(live_idx);
	int number, behavior, wait_index;
	int32_t points[FORGE_AI_ROUTE_MAX_POINTS];
	int count, k;

	if (!inst || dir == 0)
		return FORGE_AI_ROUTE_PRESS_NONE;
	number = FORGE_AI_ROUTE_NUMBER(inst->team_designator);
	behavior = FORGE_AI_ROUTE_BEHAVIOR(inst->team_designator);
	wait_index = FORGE_AI_ROUTE_WAIT_INDEX(inst->team_designator);
	switch (row)
	{
	case _forge_ai_route_row_wait:
		wait_index = (wait_index + 16 + dir) % 16;
		route_point_set_usage(live_idx, FORGE_AI_ROUTE_USAGE(number, FORGE_AI_ROUTE_ORDER(inst->team_designator),
			behavior, wait_index));
		forge_set_status("The AI walking it waits %d seconds here", s_route_waits[wait_index]);
		return FORGE_AI_ROUTE_PRESS_CHANGED;
	case _forge_ai_route_row_add_before:
	case _forge_ai_route_row_add_after:
		if (!enter)
			return FORGE_AI_ROUTE_PRESS_NONE;
		route_insert(live_idx, row == _forge_ai_route_row_add_after);
		return FORGE_AI_ROUTE_PRESS_CHANGED;
	case _forge_ai_route_row_add:
		/* (left and right choose the button, a press adds there) */
		if (!enter)
		{
			s_add_side = dir < 0 ? 0 : 1;
			return FORGE_AI_ROUTE_PRESS_NONE;
		}
		route_insert(live_idx, s_add_side);
		return FORGE_AI_ROUTE_PRESS_CHANGED;
	case _forge_ai_route_row_settings:
		if (!enter)
			return FORGE_AI_ROUTE_PRESS_NONE;
		s_settings_live_idx = live_idx;
		return FORGE_AI_ROUTE_PRESS_PAGE_OPENED;
	case _forge_ai_route_row_number:
		forge_set_status("A route's number is its own: AI spawns' GOAL picks it");
		return FORGE_AI_ROUTE_PRESS_NONE;
	case _forge_ai_route_row_behavior:
		behavior = (behavior + NUMBER_OF_FORGE_AI_ROUTE_BEHAVIORS + dir) % NUMBER_OF_FORGE_AI_ROUTE_BEHAVIORS;
		count = forge_ai_route_points(number, points, FORGE_AI_ROUTE_MAX_POINTS);
		for (k = 0; k < count; k++)
		{
			int16_t usage = g_forge.live_instances[points[k]].team_designator;

			route_point_set_usage(points[k], FORGE_AI_ROUTE_USAGE(number, FORGE_AI_ROUTE_ORDER(usage), behavior,
				FORGE_AI_ROUTE_WAIT_INDEX(usage)));
		}
		forge_set_status("%s", behavior == _forge_ai_route_ping_pong ? "Ping pong: back the way it came at either end" :
			behavior == _forge_ai_route_stay_at_end ? "Stay at end: it stops at the last point" :
			"Loop: from the last point on to the first");
		return FORGE_AI_ROUTE_PRESS_CHANGED;
	case _forge_ai_route_row_delete_route:
		if (!enter)
			return FORGE_AI_ROUTE_PRESS_NONE;
		s_settings_live_idx = -1;
		route_delete(number);
		forge_set_status("Deleted AI Route #%d", number);
		return FORGE_AI_ROUTE_PRESS_DELETED;
	case _forge_ai_route_row_back:
		if (!enter)
			return FORGE_AI_ROUTE_PRESS_NONE;
		s_settings_live_idx = -1;
		return FORGE_AI_ROUTE_PRESS_PAGE_CLOSED;
	default:
		return FORGE_AI_ROUTE_PRESS_NONE;
	}
}

int forge_ai_route_add_side(void)
{
	return s_add_side;
}

void forge_ai_route_add_side_set(int after)
{
	s_add_side = after ? 1 : 0;
}

int forge_ai_route_page_back(int32_t live_idx)
{
	if (s_settings_live_idx < 0 || s_settings_live_idx != live_idx)
		return 0;
	s_settings_live_idx = -1;
	return 1;
}

void forge_ai_route_describe(int32_t live_idx, char *text, unsigned long size)
{
	struct forge_live_instance const *inst = route_point(live_idx);
	int count = 0, rank;

	if (!inst)
	{
		snprintf(text, size, "AI Route");
		return;
	}
	rank = route_rank(live_idx, &count);
	snprintf(text, size, "AI Route #%d: Point %d of %d (%s)", FORGE_AI_ROUTE_NUMBER(inst->team_designator), rank + 1,
		count, forge_ai_route_behavior_name(FORGE_AI_ROUTE_BEHAVIOR(inst->team_designator)));
}

int forge_ai_routes_save_lines(char *text, unsigned long size)
{
	int length = 0;
	int32_t i;

	if (!text || !size)
		return 0;
	text[0] = 0;
	for (i = 0; i < g_forge.live_instance_count; i++)
	{
		struct forge_live_instance const *inst = &g_forge.live_instances[i];
		int written;

		if (!forge_ai_route_is(inst) || FORGE_AI_ROUTE_NUMBER(inst->team_designator) == 0)
			continue;
		written = snprintf(text + length, size - (unsigned long)length, "AI_ROUTE|%.4f|%.4f|%.4f|%.4f|%u\r\n",
			inst->x, inst->y, inst->z, inst->yaw, (unsigned)(uint16_t)inst->team_designator);
		if (written <= 0 || length + written >= (int)size)
			break;
		length += written;
	}
	return length;
}

void forge_ai_routes_load_line(char const *line)
{
	float x = 0.0f, y = 0.0f, z = 0.0f, yaw = 0.0f;
	unsigned usage = 0;

	if (!line || strncmp(line, "AI_ROUTE|", 9))
		return;
	if (sscanf(line + 9, "%f|%f|%f|%f|%u", &x, &y, &z, &yaw, &usage) != 5 || FORGE_AI_ROUTE_NUMBER(usage) == 0)
		return;
	forge_3d_spawn_object_at(FORGE_GROUP_AI_ROUTE, FORGE_AI_ROUTE_PATH, 1, x, y, z, yaw, 0.0f, 0.0f,
		(int16_t)(uint16_t)usage, 0);
}
