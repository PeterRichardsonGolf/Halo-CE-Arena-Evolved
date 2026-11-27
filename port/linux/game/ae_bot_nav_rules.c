/*
AE_BOT_NAV_RULES.C

Arena Evolved: the bots' hand-placed navigation, the file and the plans (ae_bot_nav_rules.h).
*/

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ae_bot_nav_rules.h"

#define AE_NAV_TOKENS 16
#define AE_NAV_LINE 256

static void nav_error(struct ae_bot_nav_map *map, int line, char const *why)
{
	if (map->error_count < AE_BOT_NAV_ERRORS)
	{
		map->errors[map->error_count].line = line;
		snprintf(map->errors[map->error_count].why, sizeof map->errors[0].why, "%s", why);
	}
	map->error_count++;
}

static int nav_number(char const *text, float *value)
{
	char *end;
	double v;

	if (!text || !text[0])
		return 0;
	v = strtod(text, &end);
	if (*end || v != v || v > 1.0e6 || v < -1.0e6)
		return 0;
	*value = (float)v;
	return 1;
}

static int nav_point3(char *const *token, float *out)
{
	return nav_number(token[0], &out[0]) && nav_number(token[1], &out[1]) && nav_number(token[2], &out[2]);
}

static int nav_name_ok(char const *name)
{
	size_t length = strlen(name), index;

	if (length < 1 || length >= AE_BOT_NAV_NAME)
		return 0;
	for (index = 0; index < length; index++)
	{
		char c = name[index];

		if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '-'))
			return 0;
	}
	return 1;
}

int ae_bot_nav_kind_of_name(char const *name)
{
	if (!name)
		return 0;
	if (!strcmp(name, "warthog"))
		return AE_BOT_NAV_WARTHOG;
	if (!strcmp(name, "ghost"))
		return AE_BOT_NAV_GHOST;
	if (!strcmp(name, "scorpion"))
		return AE_BOT_NAV_SCORPION;
	if (!strcmp(name, "any"))
		return AE_BOT_NAV_ANY;
	return 0;
}

static float nav_distance(float const *a, float const *b)
{
	float dx = a[0] - b[0], dy = a[1] - b[1], dz = a[2] - b[2];

	return (float)sqrt(dx * dx + dy * dy + dz * dz);
}

static float nav_point_distance(struct ae_bot_nav_point const *a, struct ae_bot_nav_point const *b)
{
	float pa[3], pb[3];

	pa[0] = a->x; pa[1] = a->y; pa[2] = a->z;
	pb[0] = b->x; pb[1] = b->y; pb[2] = b->z;
	return nav_distance(pa, pb);
}

/* each point's junctions: the nearest AE_BOT_NAV_JOINS points of other routes within AE_BOT_NAV_JUNCTION */
static void nav_join(struct ae_bot_nav_map *map)
{
	int a, b;

	for (a = 0; a < map->point_count; a++)
		map->points[a].join_count = 0;
	for (a = 0; a < map->point_count; a++)
	{
		struct ae_bot_nav_point *point = &map->points[a];
		float near[AE_BOT_NAV_JOINS];

		for (b = 0; b < map->point_count; b++)
		{
			float distance;
			int slot, last;

			if (map->points[b].route == point->route)
				continue;
			distance = nav_point_distance(point, &map->points[b]);
			if (distance > AE_BOT_NAV_JUNCTION)
				continue;
			/* (kept nearest first) */
			last = point->join_count < AE_BOT_NAV_JOINS ? point->join_count : AE_BOT_NAV_JOINS - 1;
			if (point->join_count == AE_BOT_NAV_JOINS && distance >= near[AE_BOT_NAV_JOINS - 1])
				continue;
			for (slot = last; slot > 0 && near[slot - 1] > distance; slot--)
			{
				near[slot] = near[slot - 1];
				point->joins[slot] = point->joins[slot - 1];
			}
			near[slot] = distance;
			point->joins[slot] = (short)b;
			if (point->join_count < AE_BOT_NAV_JOINS)
				point->join_count++;
		}
	}
}

int ae_bot_nav_parse(char const *text, struct ae_bot_nav_map *map)
{
	char line[AE_NAV_LINE];
	int line_number = 0, open_route = -1;
	char const *at = text;

	memset(map, 0, sizeof(*map));
	if (!text)
		return 0;
	while (*at)
	{
		char *token[AE_NAV_TOKENS];
		int tokens = 0;
		size_t length = 0;
		char *cursor, *hash;

		line_number++;
		while (*at && *at != '\n' && length + 1 < sizeof line)
			line[length++] = *at++;
		line[length] = 0;
		/* (a line too long: the rest of it skipped, the line refused) */
		if (*at && *at != '\n')
		{
			while (*at && *at != '\n')
				at++;
			if (*at)
				at++;
			nav_error(map, line_number, "line too long");
			continue;
		}
		if (*at)
			at++;
		hash = strchr(line, '#');
		if (hash)
			*hash = 0;
		for (cursor = strtok(line, " \t\r"); cursor && tokens < AE_NAV_TOKENS; cursor = strtok(NULL, " \t\r"))
			token[tokens++] = cursor;
		if (tokens == 0)
			continue;
		if (!map->version)
		{
			int version = tokens > 1 ? atoi(token[1]) : 0;

			if (tokens != 3 || strcmp(token[0], "nav") || version != AE_BOT_NAV_VERSION || strlen(token[2]) >= AE_BOT_NAV_FILE)
			{
				nav_error(map, line_number, "the first statement must be: nav 1 <file>");
				return map->error_count;
			}
			map->version = version;
			snprintf(map->file, sizeof map->file, "%s", token[2]);
			continue;
		}
		if (!strcmp(token[0], "made"))
		{
			int index;

			for (index = 1; index < tokens; index++)
			{
				unsigned int value;
				char *end;

				value = (unsigned int)strtoul(token[index], &end, 16);
				if (*end || strlen(token[index]) != 8 || map->made_count >= AE_BOT_NAV_MADE)
				{
					nav_error(map, line_number, "made takes 8-hex-digit hash prefixes (8 at most)");
					break;
				}
				map->made[map->made_count++] = value;
			}
			continue;
		}
		if (!strcmp(token[0], "route"))
		{
			struct ae_bot_nav_route *route;
			char kinds[64], *kind;
			int index, bits = 0, bad = 0;

			if (open_route >= 0)
			{
				nav_error(map, line_number, "route inside a route (end missing)");
				/* (its points given back, as an unended route at the file's end gives them: else they stay counted,
				and a later route's points run past the table) */
				map->point_count = map->routes[open_route].first;
				open_route = -1;
			}
			if (tokens < 3 || !nav_name_ok(token[1]) || map->route_count >= AE_BOT_NAV_ROUTES)
			{
				nav_error(map, line_number, map->route_count >= AE_BOT_NAV_ROUTES ? "too many routes" :
					"route NAME KIND[,KIND...] [loop] [oneway]");
				open_route = -2;
				continue;
			}
			snprintf(kinds, sizeof kinds, "%s", token[2]);
			for (kind = strtok(kinds, ","); kind; kind = strtok(NULL, ","))
			{
				int bit = ae_bot_nav_kind_of_name(kind);

				if (!bit)
					bad = 1;
				bits |= bit;
			}
			route = &map->routes[map->route_count];
			memset(route, 0, sizeof(*route));
			for (index = 3; index < tokens; index++)
			{
				if (!strcmp(token[index], "loop"))
					route->loop = 1;
				else if (!strcmp(token[index], "oneway"))
					route->oneway = 1;
				else
					bad = 1;
			}
			if (bad || !bits)
			{
				nav_error(map, line_number, "route NAME KIND[,KIND...] [loop] [oneway]");
				open_route = -2;
				continue;
			}
			snprintf(route->name, sizeof route->name, "%s", token[1]);
			route->kinds = bits;
			route->first = map->point_count;
			route->count = 0;
			route->line = line_number;
			open_route = map->route_count;
			continue;
		}
		if (!strcmp(token[0], "point"))
		{
			struct ae_bot_nav_point point;
			float xyz[3];
			int index, bad = 0;

			if (open_route == -2)
				continue;
			if (open_route < 0)
			{
				nav_error(map, line_number, "point outside a route");
				continue;
			}
			if (tokens < 4 || !nav_point3(&token[1], xyz))
			{
				nav_error(map, line_number, "point X Y Z [speed S] [turn R] [wait W]");
				continue;
			}
			memset(&point, 0, sizeof(point));
			point.x = xyz[0];
			point.y = xyz[1];
			point.z = xyz[2];
			point.speed = 1.0f;
			for (index = 4; index < tokens; index += 2)
			{
				float value;

				if (index + 1 >= tokens || !nav_number(token[index + 1], &value))
				{
					bad = 1;
					break;
				}
				if (!strcmp(token[index], "speed") && value >= 0.1f && value <= 1.0f)
					point.speed = value;
				else if (!strcmp(token[index], "turn") && value >= 0.0f && value <= 30.0f)
					point.turn = value;
				else if (!strcmp(token[index], "wait") && value >= 0.0f && value <= 10.0f)
					point.wait = value;
				else
				{
					bad = 1;
					break;
				}
			}
			if (bad)
			{
				nav_error(map, line_number, "point X Y Z [speed 0.1-1] [turn 0-30] [wait 0-10]");
				continue;
			}
			if (map->routes[open_route].count >= AE_BOT_NAV_ROUTE_POINTS || map->point_count >= AE_BOT_NAV_POINTS)
			{
				nav_error(map, line_number, "too many points");
				continue;
			}
			point.route = (short)open_route;
			map->points[map->point_count++] = point;
			map->routes[open_route].count++;
			continue;
		}
		if (!strcmp(token[0], "end"))
		{
			if (open_route == -2)
			{
				open_route = -1;
				continue;
			}
			if (open_route < 0 || tokens != 1)
			{
				nav_error(map, line_number, "end without a route");
				open_route = -1;
				continue;
			}
			/* (a route of fewer than two points: refused, its points given back) */
			if (map->routes[open_route].count < 2)
			{
				nav_error(map, map->routes[open_route].line, "a route needs 2 points at least");
				map->point_count = map->routes[open_route].first;
			}
			else
				map->route_count++;
			open_route = -1;
			continue;
		}
		if (open_route >= 0)
		{
			nav_error(map, line_number, "only point lines inside a route (end missing)");
			continue;
		}
		if (!strcmp(token[0], "keepout") || !strcmp(token[0], "avoid"))
		{
			int keepout = !strcmp(token[0], "keepout");
			struct ae_bot_nav_circle circle;
			float xyz[3];

			if (tokens != 6 || !nav_point3(&token[1], xyz) || strcmp(token[4], "radius") ||
				!nav_number(token[5], &circle.radius) || circle.radius <= 0.0f || circle.radius > 50.0f)
			{
				nav_error(map, line_number, keepout ? "keepout X Y Z radius R (0-50)" : "avoid X Y Z radius R (0-50)");
				continue;
			}
			circle.x = xyz[0];
			circle.y = xyz[1];
			circle.z = xyz[2];
			circle.line = line_number;
			if (keepout ? map->keepout_count >= AE_BOT_NAV_KEEPOUTS : map->avoid_count >= AE_BOT_NAV_AVOIDS)
			{
				nav_error(map, line_number, keepout ? "too many keepouts" : "too many avoids");
				continue;
			}
			if (keepout)
				map->keepouts[map->keepout_count++] = circle;
			else
				map->avoids[map->avoid_count++] = circle;
			continue;
		}
		if (!strcmp(token[0], "link"))
		{
			struct ae_bot_nav_link link;
			int kind = !strcmp(token[1 < tokens ? 1 : 0], "walk") ? AE_BOT_NAV_LINK_WALK :
				!strcmp(token[1 < tokens ? 1 : 0], "jump") ? AE_BOT_NAV_LINK_JUMP :
				!strcmp(token[1 < tokens ? 1 : 0], "drop") ? AE_BOT_NAV_LINK_DROP : -1;

			memset(&link, 0, sizeof(link));
			if ((tokens != 9 && tokens != 10) || kind < 0 || !nav_point3(&token[2], link.from) || strcmp(token[5], "to") ||
				!nav_point3(&token[6], link.to) || (tokens == 10 && strcmp(token[9], "oneway")))
			{
				nav_error(map, line_number, "link walk|jump|drop X Y Z to X Y Z [oneway]");
				continue;
			}
			if (map->link_count >= AE_BOT_NAV_LINKS)
			{
				nav_error(map, line_number, "too many links");
				continue;
			}
			link.kind = kind;
			link.oneway = tokens == 10;
			link.line = line_number;
			map->links[map->link_count++] = link;
			continue;
		}
		nav_error(map, line_number, "unknown statement");
	}
	if (open_route >= 0)
	{
		nav_error(map, map->routes[open_route].line, "route not ended (end missing)");
		map->point_count = map->routes[open_route].first;
	}
	nav_join(map);
	return map->error_count;
}

int ae_bot_nav_fits(struct ae_bot_nav_map const *map, char const *file, int hash_known, unsigned int hash)
{
	int index;

	if (!map || map->version != AE_BOT_NAV_VERSION || !file || strcmp(map->file, file))
		return 0;
	if (map->made_count == 0 || !hash_known)
		return 1;
	for (index = 0; index < map->made_count; index++)
	{
		if (map->made[index] == hash)
			return 1;
	}
	return 0;
}

/* ---------- plans */

static void nav_xyz(struct ae_bot_nav_point const *point, float *out)
{
	out[0] = point->x;
	out[1] = point->y;
	out[2] = point->z;
}

/* a point's neighbours along its route (oneway: the next only) and across its junctions */
static int nav_neighbours(struct ae_bot_nav_map const *map, int index, int *out)
{
	struct ae_bot_nav_point const *point = &map->points[index];
	struct ae_bot_nav_route const *route = &map->routes[point->route];
	int at = index - route->first, count = 0, join;

	if (at + 1 < route->count)
		out[count++] = index + 1;
	else if (route->loop)
		out[count++] = route->first;
	if (!route->oneway)
	{
		if (at > 0)
			out[count++] = index - 1;
		else if (route->loop)
			out[count++] = route->first + route->count - 1;
	}
	for (join = 0; join < point->join_count; join++)
		out[count++] = point->joins[join];
	return count;
}

int ae_bot_nav_plan(struct ae_bot_nav_map const *map, int kind, float const start[3], float const goal[3], int *plan,
	int maximum)
{
	return ae_bot_nav_plan_any(map, kind, start, goal, plan, maximum, 0);
}

int ae_bot_nav_plan_any(struct ae_bot_nav_map const *map, int kind, float const start[3], float const goal[3], int *plan,
	int maximum, int any_length)
{
	static float cost[AE_BOT_NAV_POINTS];
	static int from[AE_BOT_NAV_POINTS];
	static unsigned char done[AE_BOT_NAV_POINTS];
	static int heap[AE_BOT_NAV_POINTS * (AE_BOT_NAV_JOINS + 2) + AE_BOT_NAV_POINTS];
	static float heap_cost[AE_BOT_NAV_POINTS * (AE_BOT_NAV_JOINS + 2) + AE_BOT_NAV_POINTS];
	int heap_count = 0, index, best = -1, count, steps;
	float best_total = 0.0f, straight, xyz[3];

	if (!map || map->point_count <= 0 || maximum <= 0)
		return 0;
	straight = nav_distance(start, goal);
	for (index = 0; index < map->point_count; index++)
	{
		cost[index] = -1.0f;
		from[index] = -1;
		done[index] = 0;
	}
	/* (the entries: a usable route's points within reach of the start) */
	for (index = 0; index < map->point_count; index++)
	{
		float distance;

		if (!(map->routes[map->points[index].route].kinds & kind))
			continue;
		nav_xyz(&map->points[index], xyz);
		distance = nav_distance(start, xyz);
		if (distance > AE_BOT_NAV_ENTRY)
			continue;
		cost[index] = distance;
		heap[heap_count] = index;
		heap_cost[heap_count] = distance;
		heap_count++;
	}
	/* (Dijkstra; the heap a plain binary one, entries pushed again on a better cost) */
	{
		int a, b;

		for (a = heap_count / 2 - 1; a >= 0; a--)
		{
			b = a;
			for (;;)
			{
				int left = 2 * b + 1, right = left + 1, small = b;

				if (left < heap_count && heap_cost[left] < heap_cost[small])
					small = left;
				if (right < heap_count && heap_cost[right] < heap_cost[small])
					small = right;
				if (small == b)
					break;
				{
					int t = heap[b];
					float tc = heap_cost[b];

					heap[b] = heap[small];
					heap_cost[b] = heap_cost[small];
					heap[small] = t;
					heap_cost[small] = tc;
				}
				b = small;
			}
		}
	}
	while (heap_count > 0)
	{
		int current = heap[0], neighbours[AE_BOT_NAV_JOINS + 2], neighbour_count, n, b;
		float current_cost = heap_cost[0];

		/* (pop) */
		heap_count--;
		heap[0] = heap[heap_count];
		heap_cost[0] = heap_cost[heap_count];
		b = 0;
		for (;;)
		{
			int left = 2 * b + 1, right = left + 1, small = b;

			if (left < heap_count && heap_cost[left] < heap_cost[small])
				small = left;
			if (right < heap_count && heap_cost[right] < heap_cost[small])
				small = right;
			if (small == b)
				break;
			{
				int t = heap[b];
				float tc = heap_cost[b];

				heap[b] = heap[small];
				heap_cost[b] = heap_cost[small];
				heap[small] = t;
				heap_cost[small] = tc;
			}
			b = small;
		}
		if (done[current] || current_cost > cost[current] + 0.0001f)
			continue;
		done[current] = 1;
		/* (the exit: the reached point nearest the goal, by the way there and on) */
		{
			float to_goal, total, best_to_goal = 0.0f;

			nav_xyz(&map->points[current], xyz);
			to_goal = nav_distance(xyz, goal);
			total = current_cost + to_goal;

			if (best >= 0)
			{
				float best_xyz[3];

				nav_xyz(&map->points[best], best_xyz);
				best_to_goal = nav_distance(best_xyz, goal);
			}
			if (best < 0 || to_goal < best_to_goal - 0.001f || (to_goal < best_to_goal + 0.001f && total < best_total))
			{
				best = current;
				best_total = total;
			}
		}
		neighbour_count = nav_neighbours(map, current, neighbours);
		for (n = 0; n < neighbour_count; n++)
		{
			int next = neighbours[n];
			float next_cost;

			if (done[next] || !(map->routes[map->points[next].route].kinds & kind))
				continue;
			next_cost = current_cost + nav_point_distance(&map->points[current], &map->points[next]);
			if (cost[next] >= 0.0f && next_cost >= cost[next])
				continue;
			cost[next] = next_cost;
			from[next] = current;
			if (heap_count < (int)(sizeof heap / sizeof heap[0]))
			{
				int c = heap_count++;

				heap[c] = next;
				heap_cost[c] = next_cost;
				while (c > 0 && heap_cost[(c - 1) / 2] > heap_cost[c])
				{
					int p = (c - 1) / 2, t = heap[p];
					float tc = heap_cost[p];

					heap[p] = heap[c];
					heap_cost[p] = heap_cost[c];
					heap[c] = t;
					heap_cost[c] = tc;
					c = p;
				}
			}
		}
	}
	if (best < 0)
		return 0;
	/* (worth it: nearer the goal by enough, and not too long a way round) */
	nav_xyz(&map->points[best], xyz);
	if (nav_distance(xyz, goal) > straight - AE_BOT_NAV_GAIN ||
		(!any_length && best_total > straight * AE_BOT_NAV_DETOUR + AE_BOT_NAV_DETOUR_ADD))
	{
		return 0;
	}
	/* (back from the exit; the plan reversed into place) */
	steps = 0;
	for (index = best; index >= 0 && steps < AE_BOT_NAV_POINTS; index = from[index])
		steps++;
	count = steps < maximum ? steps : maximum;
	{
		int at = steps;

		for (index = best; index >= 0; index = from[index])
		{
			at--;
			if (at < count)
				plan[at] = index;
		}
	}
	/* (a plan longer than the room: its first points, the driver plans again on its way) */
	return count;
}

int ae_bot_nav_follow(struct ae_bot_nav_map const *map, int const *plan, int count, int current, float const at[3],
	float reach, float *off)
{
	int moved = 1;
	float xyz[3];

	*off = 0.0f;
	if (current < 0)
		current = 0;
	while (moved && current < count)
	{
		moved = 0;
		/* (a point within reach: passed) */
		nav_xyz(&map->points[plan[current]], xyz);
		if (nav_distance(at, xyz) <= reach)
		{
			current++;
			moved = 1;
			continue;
		}
		/* (along the segment after it already: passed too) */
		if (current + 1 < count)
		{
			float a[3], b[3], ab[3], ap[3], length2;

			nav_xyz(&map->points[plan[current]], a);
			nav_xyz(&map->points[plan[current + 1]], b);
			ab[0] = b[0] - a[0]; ab[1] = b[1] - a[1]; ab[2] = b[2] - a[2];
			ap[0] = at[0] - a[0]; ap[1] = at[1] - a[1]; ap[2] = at[2] - a[2];
			length2 = ab[0] * ab[0] + ab[1] * ab[1] + ab[2] * ab[2];
			if (length2 > 0.0f && ap[0] * ab[0] + ap[1] * ab[1] + ap[2] * ab[2] > 0.0f)
			{
				current++;
				moved = 1;
			}
		}
	}
	if (current >= count)
		return count;
	/* (how far off the segment into the point it makes for) */
	if (current > 0)
	{
		float a[3], b[3], ab[3], ap[3], length2, t, p[3];

		nav_xyz(&map->points[plan[current - 1]], a);
		nav_xyz(&map->points[plan[current]], b);
		ab[0] = b[0] - a[0]; ab[1] = b[1] - a[1]; ab[2] = b[2] - a[2];
		ap[0] = at[0] - a[0]; ap[1] = at[1] - a[1]; ap[2] = at[2] - a[2];
		length2 = ab[0] * ab[0] + ab[1] * ab[1] + ab[2] * ab[2];
		t = length2 > 0.0f ? (ap[0] * ab[0] + ap[1] * ab[1] + ap[2] * ab[2]) / length2 : 0.0f;
		t = t < 0.0f ? 0.0f : t > 1.0f ? 1.0f : t;
		p[0] = a[0] + ab[0] * t; p[1] = a[1] + ab[1] * t; p[2] = a[2] + ab[2] * t;
		*off = nav_distance(at, p);
	}
	return current;
}
