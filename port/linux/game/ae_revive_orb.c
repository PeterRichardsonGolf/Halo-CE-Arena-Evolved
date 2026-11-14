/* The downed player's orb in the world (ae_revive_orb.h) */
#include "cseries/cseries.h"

#include "bitmaps/bitmap_color_conversion.h"
#include "cutscene/cinematics.h"
#include "game/game.h"
#include "game/game_engine.h"
#include "game/players.h"
#include "math/real_math.h"
#include "objects/objects.h"
#include "rasterizer/rasterizer.h"
#include "render/render.h"
#include "units/units.h"

#include "ae_revive_orb.h"
#include "ae_revive_orb_rules.h"
#include "ae_round_internal.h"
#include "ae_round_wire.h"

#include <math.h>
#include <string.h>

enum
{
	FACETS = 80,
	SHELLS = 3,
	/* the orb's shells, the marker inside, the floor glow */
	MAXIMUM_VERTICES = SHELLS * FACETS * 3 + 8 * 3 + 16 * 3 + 64,
	MAXIMUM_ORBS = 8,
};
#define PI_F 3.14159265f
#define DRAW_RANGE 120.0f

static real_point3d points[MAXIMUM_VERTICES * MAXIMUM_ORBS];
static pixel32 colors[MAXIMUM_VERTICES * MAXIMUM_ORBS];

/* the unit icosphere, one subdivision: 80 facets */
static real sphere[FACETS][3][3];
static boolean sphere_ready;

static void normalize(real v[3])
{
	real length = (real)sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);

	v[0] /= length; v[1] /= length; v[2] /= length;
}

static void sphere_make(void)
{
	static int const faces[20][3] = {
		{ 0, 11, 5 }, { 0, 5, 1 }, { 0, 1, 7 }, { 0, 7, 10 }, { 0, 10, 11 }, { 1, 5, 9 }, { 5, 11, 4 }, { 11, 10, 2 },
		{ 10, 7, 6 }, { 7, 1, 8 }, { 3, 9, 4 }, { 3, 4, 2 }, { 3, 2, 6 }, { 3, 6, 8 }, { 3, 8, 9 }, { 4, 9, 5 },
		{ 2, 4, 11 }, { 6, 2, 10 }, { 8, 6, 7 }, { 9, 8, 1 } };
	real t = (1.0f + (real)sqrt(5.0)) / 2.0f;
	real v[12][3] = { { -1, t, 0 }, { 1, t, 0 }, { -1, -t, 0 }, { 1, -t, 0 }, { 0, -1, t }, { 0, 1, t }, { 0, -1, -t },
		{ 0, 1, -t }, { t, 0, -1 }, { t, 0, 1 }, { -t, 0, -1 }, { -t, 0, 1 } };
	int face, facet = 0, i, k;

	for (i = 0; i < 12; i++)
		normalize(v[i]);
	for (face = 0; face < 20; face++)
	{
		real const *a = v[faces[face][0]], *b = v[faces[face][1]], *c = v[faces[face][2]];
		real ab[3], bc[3], ca[3];
		real const *corners[4][3];

		for (k = 0; k < 3; k++)
		{
			ab[k] = a[k] + b[k]; bc[k] = b[k] + c[k]; ca[k] = c[k] + a[k];
		}
		normalize(ab); normalize(bc); normalize(ca);
		corners[0][0] = a; corners[0][1] = ab; corners[0][2] = ca;
		corners[1][0] = b; corners[1][1] = bc; corners[1][2] = ab;
		corners[2][0] = c; corners[2][1] = ca; corners[2][2] = bc;
		corners[3][0] = ab; corners[3][1] = bc; corners[3][2] = ca;
		for (i = 0; i < 4; i++)
		{
			for (k = 0; k < 3; k++)
				memcpy(sphere[facet][k], corners[i][k], sizeof(real) * 3);
			facet++;
		}
	}
	sphere_ready = TRUE;
}

static pixel32 colour(real const rgb[3], real alpha)
{
	real_argb_color color;

	color.alpha = alpha < 0.0f ? 0.0f : alpha > 1.0f ? 1.0f : alpha;
	color.red = rgb[0] > 1.0f ? 1.0f : rgb[0];
	color.green = rgb[1] > 1.0f ? 1.0f : rgb[1];
	color.blue = rgb[2] > 1.0f ? 1.0f : rgb[2];
	return real_argb_color_to_pixel32(&color);
}

static void vertex(long *count, real x, real y, real z, pixel32 c)
{
	points[*count].x = x; points[*count].y = y; points[*count].z = z;
	colors[*count] = c;
	(*count)++;
}

/* a shell of the sphere about (cx, cy, cz): alpha by facing the camera, brighter at the rim (inner) or in the middle (glow) */
static void shell(long *count, real cx, real cy, real cz, real radius, real const rgb[3], long now, real pulse, int glow,
	real glow_alpha)
{
	int facet, k;

	for (facet = 0; facet < FACETS; facet++)
	{
		real mid[3] = { 0, 0, 0 }, to_camera[3], length, facing, alpha;
		pixel32 c;

		for (k = 0; k < 3; k++)
			mid[k] = (sphere[facet][0][k] + sphere[facet][1][k] + sphere[facet][2][k]) / 3.0f;
		normalize(mid);
		to_camera[0] = render.camera.position.x - cx;
		to_camera[1] = render.camera.position.y - cy;
		to_camera[2] = render.camera.position.z - cz;
		length = (real)sqrt(to_camera[0] * to_camera[0] + to_camera[1] * to_camera[1] + to_camera[2] * to_camera[2]);
		facing = length > 0.01f ? (mid[0] * to_camera[0] + mid[1] * to_camera[1] + mid[2] * to_camera[2]) / length : 1.0f;
		if (glow)
			alpha = glow_alpha * (facing < 0.0f ? 0.0f : facing * facing) * (0.8f + 0.2f * pulse);
		else
			alpha = ae_revive_orb_alpha(facing, facet, now, pulse);
		c = colour(rgb, alpha);
		for (k = 0; k < 3; k++)
			vertex(count, cx + sphere[facet][k][0] * radius, cy + sphere[facet][k][1] * radius,
				cz + sphere[facet][k][2] * radius, c);
	}
}

/* the small marker inside: an octahedron turning slowly, bright in the orb's colour */
static void marker(long *count, real cx, real cy, real cz, real const rgb[3], long now, real pulse)
{
	real angle = (real)(now % 180) * 2.0f * PI_F / 180.0f;
	real bob = 0.013f * (real)sin((double)now * 0.05);
	real size = 0.053f * (0.95f + 0.1f * pulse), ca = (real)cos(angle), sa = (real)sin(angle);
	real bright[3];
	pixel32 top, bottom;
	real ring[4][2];
	int i;

	bright[0] = rgb[0] * 0.4f + 0.6f; bright[1] = rgb[1] * 0.4f + 0.6f; bright[2] = rgb[2] * 0.4f + 0.6f;
	top = colour(bright, 0.85f);
	bottom = colour(bright, 0.5f);
	for (i = 0; i < 4; i++)
	{
		real a = angle + (real)i * PI_F / 2.0f;

		ring[i][0] = cx + size * 0.7f * (real)cos(a);
		ring[i][1] = cy + size * 0.7f * (real)sin(a);
	}
	(void)ca; (void)sa;
	for (i = 0; i < 4; i++)
	{
		int j = (i + 1) % 4;

		vertex(count, ring[i][0], ring[i][1], cz + bob, bottom);
		vertex(count, ring[j][0], ring[j][1], cz + bob, bottom);
		vertex(count, cx, cy, cz + bob + size, top);
		vertex(count, ring[j][0], ring[j][1], cz + bob, bottom);
		vertex(count, ring[i][0], ring[i][1], cz + bob, bottom);
		vertex(count, cx, cy, cz + bob - size, top);
	}
}

/* a soft glow on the floor under it: a fan fading to nothing at its edge */
static void floor_glow(long *count, real cx, real cy, real cz, real const rgb[3], real pulse)
{
	enum { SEGMENTS = 16 };
	real radius = AE_REVIVE_ORB_RADIUS * (1.5f + 0.1f * pulse);
	pixel32 centre = colour(rgb, 0.30f + 0.12f * pulse), edge = colour(rgb, 0.0f);
	int i;

	for (i = 0; i < SEGMENTS; i++)
	{
		real a0 = 2.0f * PI_F * (real)i / SEGMENTS, a1 = 2.0f * PI_F * (real)(i + 1) / SEGMENTS;

		vertex(count, cx, cy, cz, centre);
		vertex(count, cx + radius * (real)cos(a0), cy + radius * (real)sin(a0), cz, edge);
		vertex(count, cx + radius * (real)cos(a1), cy + radius * (real)sin(a1), cz, edge);
	}
}

static struct ae_round_wire const *live_view(void)
{
	struct ae_round_wire const *view;

	if (!game_engine_running() || cinematic_in_progress() || !ae_round_on() || !(view = ae_round_view()) ||
		view->phase != AE_ROUND_PHASE_LIVE || !(view->flags & AE_ROUND_WIRE_REVIVE))
	{
		return NULL;
	}
	return view;
}

static struct player_datum *player_of_slot(long slot)
{
	struct data_iterator iterator;
	struct player_datum *player;

	data_iterator_new(&iterator, player_data);
	while ((player = (struct player_datum *)data_iterator_next(&iterator)) != NULL)
	{
		if (DATUM_INDEX_TO_ABSOLUTE_INDEX(iterator.datum_index) == slot)
			return player;
	}
	return NULL;
}

void ae_revive_orb_draw(short local_player_index)
{
	struct ae_round_wire const *round;
	long now = game_time_get(), count = 0, drawn = 0;
	real pulse;
	int index;

	if (local_player_index < 0 || !(round = live_view()))
		return;
	if (!sphere_ready)
		sphere_make();
	pulse = ae_revive_orb_pulse(now);
	for (index = 0; index < round->count && drawn < MAXIMUM_ORBS; index++)
	{
		struct ae_round_wire_player const *entry = &round->players[index];
		struct player_datum const *downed;
		real rgb[3], x, y, z, dx, dy, pulse_scale;
		real_point3d floor_point;

		if (entry->state != AE_ROUND_PLAYER_DOWN || !(downed = player_of_slot(entry->player)))
			continue;
		x = ae_round_wire_real(entry->spot[0]);
		y = ae_round_wire_real(entry->spot[1]);
		z = ae_round_wire_real(entry->spot[2]);
		dx = x - render.camera.position.x;
		dy = y - render.camera.position.y;
		if (dx * dx + dy * dy > DRAW_RANGE * DRAW_RANGE)
			continue;
		ae_revive_orb_team_rgb(downed->team_index, rgb);
		pulse_scale = 1.0f + 0.04f * (pulse - 0.5f);
		floor_point.x = x; floor_point.y = y; floor_point.z = z + 0.04f;
		floor_glow(&count, x, y, floor_point.z, rgb, pulse);
		/* (outer glow shells first, then the surface, then the marker inside) */
		shell(&count, x, y, z + AE_REVIVE_ORB_LIFT, AE_REVIVE_ORB_RADIUS * 1.55f * pulse_scale, rgb, now, pulse, TRUE, 0.07f);
		shell(&count, x, y, z + AE_REVIVE_ORB_LIFT, AE_REVIVE_ORB_RADIUS * 1.25f * pulse_scale, rgb, now, pulse, TRUE, 0.10f);
		marker(&count, x, y, z + AE_REVIVE_ORB_LIFT, rgb, now, pulse);
		shell(&count, x, y, z + AE_REVIVE_ORB_LIFT, AE_REVIVE_ORB_RADIUS * pulse_scale, rgb, now, pulse, FALSE, 0.0f);
		drawn++;
	}
	if (count > 0)
		rasterizer_debug_draw_triangles_now(points, colors, count / NUMBER_OF_VERTICES_PER_TRIANGLE);
}

boolean ae_revive_hides_body(long object_index)
{
	struct ae_round_wire const *round;
	struct object_datum const *object;
	int index;

	if (object_index == NONE || !(round = live_view()))
		return FALSE;
	object = object_try_and_get_and_verify_type(object_index, _object_mask_biped);
	if (!object || !TEST_FLAG(object->object.damage_flags, _object_dead_bit))
		return FALSE;
	for (index = 0; index < round->count; index++)
	{
		struct ae_round_wire_player const *entry = &round->players[index];

		if (entry->state == AE_ROUND_PLAYER_DOWN)
		{
			struct player_datum const *downed = player_of_slot(entry->player);

			/* the downed player's own body when it is named, else a body at the spot */
			if (downed && (downed->dead_unit_index != NONE || downed->unit_index != NONE))
			{
				if (object_index == downed->dead_unit_index || object_index == downed->unit_index)
					return TRUE;
			}
			else if (ae_revive_body_is_at_spot(object->object.position.x - ae_round_wire_real(entry->spot[0]),
				object->object.position.y - ae_round_wire_real(entry->spot[1]),
				object->object.position.z - ae_round_wire_real(entry->spot[2])))
			{
				return TRUE;
			}
		}
	}
	return FALSE;
}
