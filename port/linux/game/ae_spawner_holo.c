/* The weapon spawners' hologram in the world (ae_spawner_holo.h) */
#include "cseries/cseries.h"

#include "bitmaps/bitmap_color_conversion.h"
#include "cutscene/cinematics.h"
#include "game/game.h"
#include "game/game_engine.h"
#include "game/players.h"
#include "items/items.h"
#include "math/real_math.h"
#include "objects/object_definitions.h"
#include "objects/objects.h"
#include "physics/collisions.h"
#include "physics/collision_usage.h"
#include "rasterizer/rasterizer.h"
#include "render/render.h"
#include "scenario/scenario.h"

#include "ae_spawner_holo.h"
#include "ae_spawner_holo_rules.h"
#include "ae_spawner_msg_rules.h"
#include "ae_spawner_rules.h"
#include "ae_spawners.h"
#include "object_mesh.h"

#include <math.h>
#include <string.h>

/* port_config.c's */
int config_boolean(char const *name);
void platform_log(char const *format, ...);

enum
{
	SPOTS_MAXIMUM = AE_SPAWNERS_MAXIMUM, /* (every spawner a spot) */
	SHAPES_MAXIMUM = 12,
	/* (a shape holds any model object_mesh.h can read: no weapon is too detailed for its ghost) */
	SHAPE_VERTICES = OBJECT_MESH_MAXIMUM_VERTICES,
	SHAPE_TRIANGLES = OBJECT_MESH_MAXIMUM_TRIANGLES,
	SHAPE_EDGES = 3 * OBJECT_MESH_MAXIMUM_TRIANGLES, /* (no two triangles sharing an edge: the most there can be) */
	WELD_TABLE = 32768, /* a power of two, twice SHAPE_VERTICES */
	EDGE_TABLE = 131072, /* a power of two, over SHAPE_EDGES */
	MAXIMUM_VERTICES = 150000,
	SCAN_EVERY_TICKS = 3,
	ABSENT_GRACE_TICKS = 10,
};

/* the weapon's spot: a weapon of the map this close to it (horizontally) and this near its height lies on it */
#define SPOT_RADIUS 1.0f
#define SPOT_HEIGHT 1.2f
/* points of the model this close are one (its seams' split vertices joined, so its outline is only the silhouette) */
#define WELD_STEP 0.0005f
/* the outline's and the fill edge's widths, in pixels */
#define OUTLINE_PIXELS 2.0f
#define EDGE_PIXELS 3.0f

static real const ice[3] = { 0.373f, 0.784f, 0.925f };	/* #5FC8EC, the menus' accent */
static real const ice_bright[3] = { 0.80f, 0.94f, 1.0f };
static real const red[3] = { 0.925f, 0.502f, 0.455f };	/* #EC8074 */
static real const white[3] = { 0.941f, 0.965f, 1.0f };	/* #F0F6FF, a POWER pad's outline */

struct shape_edge
{
	unsigned short a, b; /* its points, a < b */
	short t0, t1; /* the triangles on its two sides (t1 -1: an open edge) */
};

/* a weapon's model as the ghost draws it: its points (joined at seams) and triangles in the model's own space, the
edges between the triangles, which way the triangles turn outward, and its length along the model's forward axis */
struct shape
{
	long definition; /* NONE: free */
	boolean ok;
	int vertex_count, triangle_count, edge_count;
	real winding; /* 1 or -1: the sign that points a triangle's normal out of the model */
	real low, high; /* the model's extent along its forward (x) axis: the stock's end and the muzzle's */
	real_point3d centre; /* the middle of its points, and how far the furthest is from it */
	real radius;
	boolean inverse_ok; /* inverse: its root node's default inverse, the model's space to that node's */
	struct ae_holo_frame inverse;
	real_point3d vertices[SHAPE_VERTICES];
	unsigned short triangles[SHAPE_TRIANGLES][3];
	struct shape_edge edges[SHAPE_EDGES];
};

/* why a spot's ghost is or isn't drawn (the log names each change) */
enum
{
	_reason_none,
	_reason_drawn,
	_reason_off,
	_reason_weapon_there,
	_reason_far,
	_reason_behind,
	_reason_no_shape,
	_reason_no_room,
};

static char const *const reason_words[] =
{
	"-", "drawn", "off (merged, dormant or not this game type's)", "its weapon lies there", "too far", "behind the view",
	"no shape (the weapon's model can't be read)", "over the vertex budget",
};

/* a spawner's spot: whether its weapon lies there and the pose that weapon lay in */
struct spot
{
	boolean present; /* its weapon lies on it as of the last look */
	long absent_since; /* the tick the last look found none (NONE while present) */
	long definition; /* the weapon last seen lying there ready (NONE: never) */
	boolean pose_known; /* seen lying there ready: origin, forward and up are its pose */
	boolean placed; /* never seen: origin, forward and up are the item's placement, on its side on the ground */
	long placed_definition;
	real_point3d origin;
	real_vector3d forward, up;
	boolean node_known; /* node: the weapon's root node where the game drew it, as last seen settled there */
	struct ae_holo_frame node;
	boolean ground_known; /* ground: the height of the ground under it, where the ghost's lowest point lies */
	real ground;
	boolean shown[MAXIMUM_NUMBER_OF_LOCAL_PLAYERS]; /* drawn in each view the last frame */
	int reason;
};

static struct shape shapes[SHAPES_MAXIMUM];
static long shape_used[SHAPES_MAXIMUM]; /* each slot's last use (-1 free): a full cache gives up its least recent */
static long shape_clock;
static struct spot spots[SPOTS_MAXIMUM];
static struct ae_spawners_holo info[SPOTS_MAXIMUM];
static boolean info_ok[SPOTS_MAXIMUM];
static int spot_count;
static struct scenario *spots_scenario;
static long last_scan_tick = -1000;
static long last_game_tick;
static boolean shown[MAXIMUM_NUMBER_OF_LOCAL_PLAYERS];
static long most_drawn;
static real_point3d points[MAXIMUM_VERTICES];
static pixel32 colors[MAXIMUM_VERTICES];
/* one ghost's points in the world and which of its triangles face the viewer */
static real_point3d world[SHAPE_VERTICES];
static byte facing_viewer[SHAPE_TRIANGLES];

/* ---------- the weapon's shape */

static void frame_of(real_matrix4x3 const *m, struct ae_holo_frame *frame)
{
	frame->origin.x = m->position.x;
	frame->origin.y = m->position.y;
	frame->origin.z = m->position.z;
	frame->forward.x = m->forward.i * m->scale;
	frame->forward.y = m->forward.j * m->scale;
	frame->forward.z = m->forward.k * m->scale;
	frame->left.x = m->left.i * m->scale;
	frame->left.y = m->left.j * m->scale;
	frame->left.z = m->left.k * m->scale;
	frame->up.x = m->up.i * m->scale;
	frame->up.y = m->up.j * m->scale;
	frame->up.z = m->up.k * m->scale;
}

static unsigned long weld_hash(long x, long y, long z)
{
	return ((unsigned long)x * 73856093UL) ^ ((unsigned long)y * 19349663UL) ^ ((unsigned long)z * 83492791UL);
}

static int weld(struct shape *shape, short *table, real_point3d const *p)
{
	long x = (long)floor(p->x / WELD_STEP + 0.5f), y = (long)floor(p->y / WELD_STEP + 0.5f), z = (long)floor(p->z / WELD_STEP + 0.5f);
	unsigned long slot = weld_hash(x, y, z) & (WELD_TABLE - 1);

	for (;;)
	{
		short index = table[slot];

		if (index < 0)
		{
			if (shape->vertex_count >= SHAPE_VERTICES)
				return -1;
			index = (short)shape->vertex_count++;
			shape->vertices[index] = *p;
			table[slot] = index;
			return index;
		}
		{
			real_point3d const *q = &shape->vertices[index];

			if ((long)floor(q->x / WELD_STEP + 0.5f) == x && (long)floor(q->y / WELD_STEP + 0.5f) == y &&
				(long)floor(q->z / WELD_STEP + 0.5f) == z)
				return index;
		}
		slot = (slot + 1) & (WELD_TABLE - 1);
	}
}

static boolean edge_add(struct shape *shape, long *table, int a, int b, int triangle)
{
	unsigned short low = (unsigned short)(a < b ? a : b), high = (unsigned short)(a < b ? b : a);
	unsigned long slot = (((unsigned long)low << 16) | high) * 2654435761UL & (EDGE_TABLE - 1);

	for (;;)
	{
		long index = table[slot];

		if (index < 0)
		{
			struct shape_edge *edge;

			if (shape->edge_count >= SHAPE_EDGES)
				return FALSE;
			index = shape->edge_count++;
			edge = &shape->edges[index];
			edge->a = low;
			edge->b = high;
			edge->t0 = (short)triangle;
			edge->t1 = -1;
			table[slot] = index;
			return TRUE;
		}
		if (shape->edges[index].a == low && shape->edges[index].b == high)
		{
			/* (a third triangle on one edge keeps the first two) */
			if (shape->edges[index].t1 < 0)
				shape->edges[index].t1 = (short)triangle;
			return TRUE;
		}
		slot = (slot + 1) & (EDGE_TABLE - 1);
	}
}

static void normal_of(real_point3d const *a, real_point3d const *b, real_point3d const *c, real_vector3d *n)
{
	real ux = b->x - a->x, uy = b->y - a->y, uz = b->z - a->z, vx = c->x - a->x, vy = c->y - a->y, vz = c->z - a->z;

	n->i = uy * vz - uz * vy;
	n->j = uz * vx - ux * vz;
	n->k = ux * vy - uy * vx;
}

static void shape_fill(struct shape *shape, struct object_mesh const *mesh)
{
	static short weld_table[WELD_TABLE];
	static long edge_table[EDGE_TABLE];
	static short map[OBJECT_MESH_MAXIMUM_VERTICES];
	long index;
	real_point3d center;
	real outward = 0.0f;

	shape->ok = FALSE;
	shape->vertex_count = shape->triangle_count = shape->edge_count = 0;
	if (!mesh || mesh->vertex_count < 3 || mesh->triangle_count < 1 || mesh->vertex_count > OBJECT_MESH_MAXIMUM_VERTICES)
		return;
	memset(weld_table, 0xFF, sizeof(weld_table));
	memset(edge_table, 0xFF, sizeof(edge_table));
	for (index = 0; index < mesh->vertex_count; index++)
		if ((map[index] = (short)weld(shape, weld_table, &mesh->vertices[index].position)) < 0)
			return;
	for (index = 0; index < mesh->triangle_count; index++)
	{
		unsigned short const *t = mesh->triangles[index];
		int a, b, c, n;

		if (t[0] >= mesh->vertex_count || t[1] >= mesh->vertex_count || t[2] >= mesh->vertex_count)
			continue;
		a = map[t[0]];
		b = map[t[1]];
		c = map[t[2]];
		if (a == b || b == c || a == c)
			continue;
		if (shape->triangle_count >= SHAPE_TRIANGLES)
			return;
		n = shape->triangle_count++;
		shape->triangles[n][0] = (unsigned short)a;
		shape->triangles[n][1] = (unsigned short)b;
		shape->triangles[n][2] = (unsigned short)c;
		if (!edge_add(shape, edge_table, a, b, n) || !edge_add(shape, edge_table, b, c, n) || !edge_add(shape, edge_table, c, a, n))
			return;
	}
	if (shape->triangle_count < 1)
		return;
	center.x = center.y = center.z = 0.0f;
	shape->low = 1e30f;
	shape->high = -1e30f;
	for (index = 0; index < shape->vertex_count; index++)
	{
		center.x += shape->vertices[index].x;
		center.y += shape->vertices[index].y;
		center.z += shape->vertices[index].z;
		if (shape->vertices[index].x < shape->low)
			shape->low = shape->vertices[index].x;
		if (shape->vertices[index].x > shape->high)
			shape->high = shape->vertices[index].x;
	}
	center.x /= (real)shape->vertex_count;
	center.y /= (real)shape->vertex_count;
	center.z /= (real)shape->vertex_count;
	shape->centre = center;
	shape->radius = 0.0f;
	for (index = 0; index < shape->vertex_count; index++)
	{
		real dx = shape->vertices[index].x - center.x, dy = shape->vertices[index].y - center.y;
		real dz = shape->vertices[index].z - center.z, d = (real)sqrt(dx * dx + dy * dy + dz * dz);

		if (d > shape->radius)
			shape->radius = d;
	}
	shape->inverse_ok = mesh->node_count > 0;
	if (shape->inverse_ok)
		frame_of(&mesh->inverse[0], &shape->inverse);
	/* (the triangles' normals summed against the way out from the centre: which turn is outward) */
	for (index = 0; index < shape->triangle_count; index++)
	{
		real_point3d const *a = &shape->vertices[shape->triangles[index][0]];
		real_point3d const *b = &shape->vertices[shape->triangles[index][1]];
		real_point3d const *c = &shape->vertices[shape->triangles[index][2]];
		real_vector3d n;

		normal_of(a, b, c, &n);
		outward += n.i * ((a->x + b->x + c->x) / 3.0f - center.x) + n.j * ((a->y + b->y + c->y) / 3.0f - center.y) +
			n.k * ((a->z + b->z + c->z) / 3.0f - center.z);
	}
	shape->winding = outward >= 0.0f ? 1.0f : -1.0f;
	shape->ok = shape->high > shape->low;
}

/* the shape of a weapon definition: kept once read; `object_index` (a weapon of that definition on the map, or NONE) gives
the model as that object shows it */
static struct shape *shape_of(long definition, long object_index)
{
	int index, free_index;

	if (definition == NONE)
		return NULL;
	for (index = 0; index < SHAPES_MAXIMUM; index++)
	{
		if (shapes[index].definition == definition)
		{
			shape_used[index] = ++shape_clock;
			return &shapes[index];
		}
	}
	free_index = ae_holo_cache_slot(shape_used, SHAPES_MAXIMUM);
	shape_used[free_index] = ++shape_clock;
	shapes[free_index].definition = definition;
	shape_fill(&shapes[free_index], object_index != NONE ? object_mesh_get(object_index) :
		object_mesh_get_for_definition(definition));
	if (!shapes[free_index].ok && object_index != NONE)
		shape_fill(&shapes[free_index], object_mesh_get_for_definition(definition));
	platform_log("spawner holo: weapon %ld shape %s (%d points, %d triangles, %d edges)", definition,
		shapes[free_index].ok ? "read" : "not readable (no model, or over the ghost's size)", shapes[free_index].vertex_count,
		shapes[free_index].triangle_count, shapes[free_index].edge_count);
	return &shapes[free_index];
}

/* ---------- the look at the map's weapons */

static void reset_spots(void)
{
	int index;

	memset(spots, 0, sizeof(spots));
	for (index = 0; index < SPOTS_MAXIMUM; index++)
	{
		spots[index].definition = NONE;
		spots[index].placed_definition = NONE;
		spots[index].absent_since = NONE;
	}
	for (index = 0; index < SHAPES_MAXIMUM; index++)
	{
		shapes[index].definition = NONE;
		shape_used[index] = -1;
	}
	last_scan_tick = -1000;
	spot_count = 0;
}

static void read_spots(void)
{
	int index, count = ae_spawners_holo_count();

	if (count > SPOTS_MAXIMUM)
		count = SPOTS_MAXIMUM;
	for (index = 0; index < count; index++)
		info_ok[index] = ae_spawners_holo_get(index, &info[index]);
	spot_count = count;
}

static boolean ground_under(real_point3d const *at, real *z);

static boolean lies_on(struct item_datum const *item, struct ae_spawners_holo const *where, real radius, real *distance)
{
	real dx = item->object.position.x - where->position.x, dy = item->object.position.y - where->position.y;
	real dz = item->object.position.z - where->position.z;

	*distance = dx * dx + dy * dy;
	return *distance < radius * radius && dz > -SPOT_HEIGHT - 0.3f && dz < SPOT_HEIGHT;
}

static boolean loose(struct item_datum const *item)
{
	return item && item->object.parent_object_index == NONE && !TEST_FLAG(item->item.flags, _item_attached_to_unit_bit);
}

/* a weapon lies on the spot: it counts as there only when the spawner makes it (a different weapon dropped there does not
hide the ghost), and its pose is learnt while the spawner calls it ready and it has settled: its root node as the game
draws it, and the ground under it */
static void seen(int index, long object_index, struct item_datum const *item)
{
	struct spot *spot = &spots[index];
	/* (the host's: the very weapon the spot made; a guest's: one of the weapons the spot makes) */
	boolean own = info[index].host ? object_index == info[index].object_index :
		ae_spawners_holo_accepts(index, item->definition_index);
	int what = ae_holo_seen(own, info[index].state == AE_SPAWNER_MSG_READY);

	if (what & AE_HOLO_SEEN_PRESENT)
		spot->present = TRUE;
	struct ae_holo_point3 velocity;
	real_matrix4x3 *nodes;
	boolean moved, first;

	if (!(what & AE_HOLO_SEEN_POSE))
		return;
	velocity.x = item->object.translational_velocity.i;
	velocity.y = item->object.translational_velocity.j;
	velocity.z = item->object.translational_velocity.k;
	if (!ae_holo_at_rest(TEST_FLAG(item->object.flags, _object_at_rest_bit), velocity))
		return;
	first = !spot->pose_known || spot->definition != item->definition_index;
	{
		real dx = spot->origin.x - item->object.position.x, dy = spot->origin.y - item->object.position.y;
		real dz = spot->origin.z - item->object.position.z;

		moved = first || dx * dx + dy * dy + dz * dz > 0.02f * 0.02f;
	}
	spot->definition = item->definition_index;
	spot->pose_known = TRUE;
	spot->origin = item->object.position;
	spot->forward = item->object.forward;
	spot->up = item->object.up;
	nodes = object_get_node_matrices(object_index);
	spot->node_known = nodes != NULL;
	if (nodes)
		frame_of(&nodes[0], &spot->node);
	if (moved)
		spot->ground_known = ground_under(&item->object.position, &spot->ground);
	if (first)
		platform_log("spawner holo: spot %d pose learnt from its settled weapon at (%.2f %.2f %.2f), its root node at "
			"(%.2f %.2f %.2f), the ground %s%.2f, the spot at (%.2f %.2f %.2f)", index, item->object.position.x,
			item->object.position.y, item->object.position.z, spot->node.origin.x, spot->node.origin.y, spot->node.origin.z,
			spot->ground_known ? "at " : "not found, ", spot->ground_known ? spot->ground : 0.0f, info[index].position.x,
			info[index].position.y, info[index].position.z);
	(void)shape_of(item->definition_index, object_index);
}

/* once a game tick at the most, and only every few: the spawners' state, then their weapons. A spawner that names its
weapon (the host's base spawners) is looked at by that weapon alone; the rest (a guest's, a POWER pad's) by the map's loose
weapons, each set against its nearest spot */
static void scan(long now)
{
	struct scenario *scenario = global_scenario_get();
	struct object_iterator iterator;
	int index;
	boolean any_guest = FALSE;

	if (now < last_game_tick || spots_scenario != scenario)
	{
		reset_spots();
		spots_scenario = scenario;
	}
	last_game_tick = now;
	if (now - last_scan_tick < SCAN_EVERY_TICKS && now >= last_scan_tick)
		return;
	last_scan_tick = now;
	read_spots();
	for (index = 0; index < spot_count; index++)
	{
		struct item_datum *item;
		real distance;

		if (!info_ok[index])
			continue;
		spots[index].present = FALSE;
		any_guest |= !info[index].host;
		if (!info[index].host || info[index].object_index == NONE)
			continue;
		item = item_try_and_get(info[index].object_index);
		if (loose(item) && lies_on(item, &info[index], SPOT_RADIUS, &distance))
			seen(index, info[index].object_index, item);
	}
	/* (a guest's spots: every loose weapon against its nearest spot, one of the spot's weapons counting as its own) */
	if (any_guest)
	{
		object_iterator_new(&iterator, _object_mask_weapon, 0);
		while (object_iterator_next(&iterator))
		{
			struct item_datum *item = item_try_and_get(iterator.index);
			real nearest = SPOT_RADIUS * SPOT_RADIUS, distance;
			int found = -1;

			if (!loose(item))
				continue;
			for (index = 0; index < spot_count; index++)
			{
				if (!info_ok[index] || info[index].host)
					continue;
				if (lies_on(item, &info[index], SPOT_RADIUS, &distance) && distance < nearest)
				{
					nearest = distance;
					found = index;
				}
			}
			if (found >= 0)
				seen(found, iterator.index, item);
		}
	}
	for (index = 0; index < spot_count; index++)
	{
		if (!info_ok[index])
			continue;
		if (spots[index].present)
			spots[index].absent_since = NONE;
		else if (spots[index].absent_since == NONE)
			spots[index].absent_since = now;
	}
}

static boolean lies_now(int index, long now)
{
	return spots[index].present || (spots[index].absent_since != NONE && now - spots[index].absent_since < ABSENT_GRACE_TICKS);
}

boolean ae_spawner_holo_weapon_lies(long equipment_index)
{
	long now = game_time_get();
	int index;

	scan(now);
	for (index = 0; index < spot_count; index++)
		if (info_ok[index] && info[index].equipment_index == equipment_index)
			return lies_now(index, now);
	return TRUE;
}

/* ---------- the ghost's pose */

/* the height of the ground under a point (from a little over it): FALSE when there is none within reach */
static boolean ground_under(real_point3d const *at, real *z)
{
	struct collision_result result;
	real_point3d start;
	real_vector3d vector;
	boolean pushed = FALSE, hit;

	start = *at;
	start.z += 0.5f;
	vector.i = 0.0f;
	vector.j = 0.0f;
	vector.k = -3.0f;
	if (global_current_collision_user_depth < MAXIMUM_COLLISION_USER_STACK_DEPTH)
	{
		global_current_collision_users[global_current_collision_user_depth++] = 20;
		pushed = TRUE;
	}
	hit = collision_test_vector(FLAG(_collision_test_front_facing_surfaces_bit) | FLAG(_collision_test_structure_bit) |
		FLAG(_collision_test_objects_scenery_bit) | FLAG(_collision_test_objects_machines_bit), &start, &vector, NONE, &result);
	if (pushed)
		--global_current_collision_user_depth;
	if (!hit || result.plane.n.k <= 0.3f)
		return FALSE;
	*z = result.point.z;
	return TRUE;
}

static void left_of(real_vector3d const *forward, real_vector3d const *up, real_vector3d *left)
{
	/* left = up x forward */
	left->i = up->j * forward->k - up->k * forward->j;
	left->j = up->k * forward->i - up->i * forward->k;
	left->k = up->i * forward->j - up->j * forward->i;
}

/* the spot's pose: the one its weapon lay in, else (never seen there) the spawn's placement (ae_holo_first_pose) with the
weapon on its side, where the spawn drops it: the ground under the placement (worked out once a weapon) */
static boolean pose_of(struct spot *spot, struct ae_spawners_holo const *where, struct shape const *shape)
{
	if (spot->pose_known)
		return TRUE;
	if (spot->placed && spot->placed_definition == shape->definition)
		return TRUE;
	spot->origin = where->position;
	spot->node_known = FALSE;
	{
		/* (the way the spawn will face the weapon: the item's facing only where the game faces the map's items) */
		struct ae_holo_point3 forward, up;

		ae_holo_first_pose(where->faces_item, where->facing, &forward, &up);
		spot->forward.i = forward.x;
		spot->forward.j = forward.y;
		spot->forward.k = forward.z;
		spot->up.i = up.x;
		spot->up.j = up.y;
		spot->up.k = up.z;
	}
	spot->ground_known = ground_under(&where->position, &spot->ground);
	spot->placed = TRUE;
	spot->placed_definition = shape->definition;
	platform_log("spawner holo: a spot's pose from its placement at (%.2f %.2f %.2f), the ground %s%.2f",
		where->position.x, where->position.y, where->position.z, spot->ground_known ? "at " : "not found, ",
		spot->ground_known ? spot->ground : 0.0f);
	return TRUE;
}

/* ---------- drawing */

static long vertex_cap(void)
{
	short views = local_player_count();

	return views <= 1 ? MAXIMUM_VERTICES : views == 2 ? MAXIMUM_VERTICES / 2 : MAXIMUM_VERTICES / 4;
}

static pixel32 colour(real const rgb[3], real alpha)
{
	real_argb_color color;

	color.alpha = alpha < 0.0f ? 0.0f : alpha > 1.0f ? 1.0f : alpha;
	color.red = rgb[0];
	color.green = rgb[1];
	color.blue = rgb[2];
	return real_argb_color_to_pixel32(&color);
}

static void vertex(long *count, real_point3d const *p, pixel32 c)
{
	points[*count] = *p;
	colors[(*count)++] = c;
}

static void triangle(long *count, real_point3d const *a, real_point3d const *b, real_point3d const *c, pixel32 colour_)
{
	vertex(count, a, colour_);
	vertex(count, b, colour_);
	vertex(count, c, colour_);
}

/* a line as a band `width` wide turned to the viewer (two triangles) */
static void line(long *count, real_point3d const *a, real_point3d const *b, real width, real_point3d const *camera, pixel32 c)
{
	real dx = b->x - a->x, dy = b->y - a->y, dz = b->z - a->z;
	real vx = camera->x - (a->x + b->x) * 0.5f, vy = camera->y - (a->y + b->y) * 0.5f, vz = camera->z - (a->z + b->z) * 0.5f;
	real sx = dy * vz - dz * vy, sy = dz * vx - dx * vz, sz = dx * vy - dy * vx;
	real length = (real)sqrt(sx * sx + sy * sy + sz * sz);
	real_point3d p0, p1, p2, p3;

	if (length < 1e-9f)
		return;
	sx *= width * 0.5f / length;
	sy *= width * 0.5f / length;
	sz *= width * 0.5f / length;
	p0.x = a->x - sx; p0.y = a->y - sy; p0.z = a->z - sz;
	p1.x = a->x + sx; p1.y = a->y + sy; p1.z = a->z + sz;
	p2.x = b->x + sx; p2.y = b->y + sy; p2.z = b->z + sz;
	p3.x = b->x - sx; p3.y = b->y - sy; p3.z = b->z - sz;
	triangle(count, &p0, &p1, &p2, c);
	triangle(count, &p0, &p2, &p3, c);
}

static struct ae_holo_point3 as_holo(real_point3d const *p)
{
	struct ae_holo_point3 h;

	h.x = p->x;
	h.y = p->y;
	h.z = p->z;
	return h;
}

static real_point3d as_point(struct ae_holo_point3 const *h)
{
	real_point3d p;

	p.x = h->x;
	p.y = h->y;
	p.z = h->z;
	return p;
}

/* the frame the ghost's model is drawn in: the weapon's root node as the game drew it settled there, else the pose */
static void ghost_frame(struct spot const *spot, struct shape const *shape, struct ae_holo_frame *frame)
{
	real_vector3d left;

	if (spot->pose_known && spot->node_known && shape->inverse_ok)
	{
		ae_holo_skin(&spot->node, &shape->inverse, frame);
		return;
	}
	left_of(&spot->forward, &spot->up, &left);
	frame->origin = as_holo(&spot->origin);
	frame->forward.x = spot->forward.i;
	frame->forward.y = spot->forward.j;
	frame->forward.z = spot->forward.k;
	frame->left.x = left.i;
	frame->left.y = left.j;
	frame->left.z = left.k;
	frame->up.x = spot->up.i;
	frame->up.y = spot->up.j;
	frame->up.z = spot->up.k;
}

static real_point3d as_point_value(struct ae_holo_point3 h)
{
	return as_point(&h);
}

static boolean silhouette(struct shape_edge const *edge)
{
	if (edge->t1 < 0)
		return facing_viewer[edge->t0];
	return facing_viewer[edge->t0] != facing_viewer[edge->t1];
}

/* one spot's ghost: the weapon's shape in the pose, the faces toward the viewer filled from the stock up to the fill edge,
a bright line where the edge cuts them, and the silhouette's outline over it all. The outline is kept whole first: the
fill is what a tight vertex budget drops. FALSE when not even the outline fits */
static boolean draw_one(long *count, long budget, struct spot const *spot, struct shape const *shape,
	struct ae_spawners_holo const *where, struct ae_holo_frame const *frame, real alpha, real distance, real_point3d const *camera)
{
	real_vector3d toward, offset;
	struct ae_holo_point3 axis, centre;
	boolean red_state = where->state == AE_SPAWNER_MSG_RED;
	real fill = red_state ? 1.0f : ae_holo_fill(where->ticks_left, where->period);
	real edge, base, length, lowest = 1e30f, lift = ae_holo_lift(distance), outline_width = ae_holo_line_width(distance, OUTLINE_PIXELS);
	real const *ink = red_state ? red : ice;
	long needed = 0;
	int index;

	/* (the model in its frame, its lowest point set on the ground under it, then lifted a little toward the viewer) */
	for (index = 0; index < shape->vertex_count; index++)
	{
		world[index] = as_point_value(ae_holo_frame_point(frame, as_holo(&shape->vertices[index])));
		if (world[index].z < lowest)
			lowest = world[index].z;
	}
	centre = ae_holo_frame_point(frame, as_holo(&shape->centre));
	toward.i = camera->x - centre.x;
	toward.j = camera->y - centre.y;
	toward.k = camera->z - centre.z;
	offset.i = offset.j = 0.0f;
	offset.k = spot->ground_known && lowest < 1e29f && fabsf(spot->ground - lowest) < 1.0f ? spot->ground - lowest : 0.0f;
	if (distance > 1e-3f)
	{
		offset.i += toward.i / distance * lift;
		offset.j += toward.j / distance * lift;
		offset.k += toward.k / distance * lift;
	}
	for (index = 0; index < shape->vertex_count; index++)
	{
		world[index].x += offset.i;
		world[index].y += offset.j;
		world[index].z += offset.k;
	}
	for (index = 0; index < shape->triangle_count; index++)
	{
		real_point3d const *a = &world[shape->triangles[index][0]];
		real_vector3d n;

		normal_of(a, &world[shape->triangles[index][1]], &world[shape->triangles[index][2]], &n);
		facing_viewer[index] = (byte)((n.i * (camera->x - a->x) + n.j * (camera->y - a->y) + n.k * (camera->z - a->z)) *
			shape->winding > 0.0f);
	}
	for (index = 0; index < shape->edge_count; index++)
		if (silhouette(&shape->edges[index]))
			needed += 6;
	if (*count + needed > budget)
		return FALSE;
	if (!red_state)
		alpha *= ae_holo_pulse(where->ticks_left);
	length = (real)sqrt(frame->forward.x * frame->forward.x + frame->forward.y * frame->forward.y +
		frame->forward.z * frame->forward.z);
	if (length < 1e-6f)
		length = 1.0f;
	axis.x = frame->forward.x / length;
	axis.y = frame->forward.y / length;
	axis.z = frame->forward.z / length;
	base = (frame->origin.x + offset.i) * axis.x + (frame->origin.y + offset.j) * axis.y + (frame->origin.z + offset.k) * axis.z;
	edge = base + ae_holo_edge(shape->low, shape->high, fill) * length;
	/* the filled part: the faces toward the viewer up to the edge (from the stock, the model's low end, to the muzzle) */
	if (fill > 0.0f)
	{
		pixel32 body = colour(ink, (red_state ? 0.18f : 0.30f) * alpha);
		pixel32 bright = colour(ice_bright, 0.95f * alpha);
		real edge_width = ae_holo_line_width(distance, EDGE_PIXELS);

		for (index = 0; index < shape->triangle_count; index++)
		{
			struct ae_holo_point3 corners[3], part[4];
			real_point3d p0, p1, p2, p3;
			int n;

			if (!facing_viewer[index])
				continue;
			if (*count + needed + 12 > budget)
				break;
			corners[0] = as_holo(&world[shape->triangles[index][0]]);
			corners[1] = as_holo(&world[shape->triangles[index][1]]);
			corners[2] = as_holo(&world[shape->triangles[index][2]]);
			n = fill >= 0.999f ? 3 : ae_holo_clip_triangle(corners, axis, edge, part);
			if (n == 0)
				continue;
			if (fill >= 0.999f)
			{
				part[0] = corners[0];
				part[1] = corners[1];
				part[2] = corners[2];
			}
			p0 = as_point(&part[0]);
			p1 = as_point(&part[1]);
			p2 = as_point(&part[2]);
			triangle(count, &p0, &p1, &p2, body);
			if (n == 4)
			{
				p3 = as_point(&part[3]);
				triangle(count, &p0, &p2, &p3, body);
			}
			/* (the fill edge's bright line across this face) */
			if (!red_state && fill < 0.999f)
			{
				struct ae_holo_point3 from, to;

				if (ae_holo_cut_triangle(corners, axis, edge, &from, &to) && *count + needed + 6 <= budget)
				{
					p0 = as_point(&from);
					p1 = as_point(&to);
					line(count, &p0, &p1, edge_width, camera, bright);
				}
			}
		}
	}
	/* the outline: the silhouette's edges (white round a POWER pad's weapon) */
	{
		int outline = ae_holo_outline_ink(where->power, red_state);
		pixel32 rim = colour(outline == AE_HOLO_INK_RED ? red : outline == AE_HOLO_INK_WHITE ? white : ice, 0.90f * alpha);

		for (index = 0; index < shape->edge_count; index++)
		{
			struct shape_edge const *e = &shape->edges[index];

			if (silhouette(e))
				line(count, &world[e->a], &world[e->b], outline_width, camera, rim);
		}
	}
	return TRUE;
}

static void reason(int index, int why, long now)
{
	struct spot *spot = &spots[index];

	if (spot->reason == why)
		return;
	spot->reason = why;
	platform_log("spawner holo: spot %d (item %ld, %s, %ld s left) at tick %ld: %s (ghost at %.2f %.2f %.2f forward %.2f %.2f "
		"%.2f up %.2f %.2f %.2f; view at %.2f %.2f %.2f looking %.2f %.2f %.2f)", index, info[index].equipment_index,
		info[index].state == AE_SPAWNER_MSG_RED ? "red" : info[index].state == AE_SPAWNER_MSG_READY ? "ready" : "waiting",
		info[index].ticks_left / TICKS_PER_SECOND, now, reason_words[why], spot->origin.x, spot->origin.y, spot->origin.z,
		spot->forward.i, spot->forward.j, spot->forward.k, spot->up.i, spot->up.j, spot->up.k, render.camera.position.x,
		render.camera.position.y, render.camera.position.z, render.camera.forward.i, render.camera.forward.j,
		render.camera.forward.k);
}

void ae_spawner_holo_draw(short local_player_index)
{
	long now = game_time_get(), count = 0, budget, index;
	boolean on;
	real_point3d camera;
	real_vector3d forward;

	if (local_player_index < 0 || local_player_index >= MAXIMUM_NUMBER_OF_LOCAL_PLAYERS)
		return;
	on = (boolean)(game_engine_running() && !cinematic_in_progress() && ae_spawners_holo_count() > 0 &&
		!config_boolean("debug.spawner_holo_off"));
	if (on != shown[local_player_index])
	{
		shown[local_player_index] = on;
		platform_log("spawner holo: view %d %s at tick %ld", (int)local_player_index, on ? "on" : "off", now);
	}
	if (!on)
		return;
	scan(now);
	budget = vertex_cap();
	camera = render.camera.position;
	forward.i = render.camera.forward.i;
	forward.j = render.camera.forward.j;
	forward.k = render.camera.forward.k;
	for (index = 0; index < spot_count; index++)
	{
		struct spot *spot = &spots[index];
		struct ae_spawners_holo const *where = &info[index];
		struct shape *shape;
		struct ae_holo_frame frame;
		struct ae_holo_point3 centre;
		real dx, dy, dz, distance, alpha;
		boolean was_shown = spot->shown[local_player_index];

		spot->shown[local_player_index] = FALSE;
		if (!info_ok[index])
		{
			reason((int)index, _reason_off, now);
			continue;
		}
		/* (the spawner's weapon on its spot: the real one shows. A spot that calls itself ready with no weapon on it, a POWER
		pad's weapon taken inside its window, shows the ghost counting to the next) */
		if (lies_now((int)index, now))
		{
			reason((int)index, _reason_weapon_there, now);
			continue;
		}
		/* (the host's ghost is the weapon drawn for this count; a guest's, the one last seen lying there) */
		shape = shape_of(where->definition_next || spot->definition == NONE ? where->definition : spot->definition, NONE);
		if (!shape || !shape->ok)
		{
			reason((int)index, _reason_no_shape, now);
			continue;
		}
		/* (range and view from the ghost itself, as drawn: a ghost shown keeps showing a margin past either edge) */
		pose_of(spot, where, shape);
		ghost_frame(spot, shape, &frame);
		centre = ae_holo_frame_point(&frame, as_holo(&shape->centre));
		dx = centre.x - camera.x;
		dy = centre.y - camera.y;
		dz = centre.z - camera.z;
		distance = (real)sqrt(dx * dx + dy * dy + dz * dz);
		if (!ae_holo_in_range(was_shown, distance))
		{
			reason((int)index, _reason_far, now);
			continue;
		}
		if (!ae_holo_in_view(was_shown, dx * forward.i + dy * forward.j + dz * forward.k, shape->radius))
		{
			reason((int)index, _reason_behind, now);
			continue;
		}
		spot->shown[local_player_index] = TRUE;
		alpha = ae_holo_distance_alpha(distance);
		if (alpha <= 0.0f)
			continue;
		if (!draw_one(&count, budget, spot, shape, where, &frame, alpha, distance, &camera))
		{
			reason((int)index, _reason_no_room, now);
			continue;
		}
		reason((int)index, _reason_drawn, now);
	}
	if (count > most_drawn + most_drawn / 8 + 30)
	{
		most_drawn = count;
		platform_log("spawner holo: %ld vertices in view %d (most so far; at most %ld)", count, (int)local_player_index, budget);
	}
	if (count > 0)
		rasterizer_debug_draw_triangles_now(points, colors, count / NUMBER_OF_VERTICES_PER_TRIANGLE);
}
