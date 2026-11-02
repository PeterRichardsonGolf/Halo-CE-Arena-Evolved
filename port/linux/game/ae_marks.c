/*
AE_MARKS.C

MARKS (controls.mark; a controller's tap of the flashlight button, whose hold
stays the flashlight's): a press marks what is aimed at, for 5 seconds, seen
through walls, with one sound. One mark a player: a new one replaces the old,
and a press on your own mark clears it. Three kinds: an enemy (where they were:
it does not follow), an item (ends if it is picked up or goes) and a place.
Teammates and your own vehicle cannot be marked: the aim passes through them.

When several are in range the best tier is taken (ae_mark_rules.h): an enemy, a dead body, a marker, an objective or vehicle or
named place, a pickup, the ground. Aimed at a marker, a press answers it (your own: cancel; a teammate's: OK, NEVER MIND, DIBS)
rather than marks.

The looks are DEAD ALLY MARKERS': WAYPOINTS' rows, the same text and sizes, HUD
STYLE AE's outlines (an enemy's kept in CLASSIC too: plain red is lost on light
ground). A mark out of the view stays at the edge of its ellipse, showing only
the distance (the word in TEXT).

The rules are ae_mark_rules.c's (unit tested).
*/

#include <ctype.h>
#include <string.h>
#include "cseries.h"
#include "math/real_math.h"
#include "memory/data.h"
#include "camera/director.h"
#include "camera/observer.h"
#include "cutscene/cinematics.h"
#include "game/game.h"
#include "game/game_allegiance.h"
#include "game/game_engine.h"
#include "game/item_timers.h"
#include "game/players.h"
#include "objects/objects.h"
#include "objects/object_types.h"
#include "physics/collisions.h"
#include "tag_files/tag_files.h"
#include "units/unit_definitions.h"
#include "units/units.h"
#include "items/weapons.h"
#include "interface/hud.h"
#include "interface/hud_messaging.h"
#include "interface/ui_widget.h"
#include "render/render.h"
#include "render/render_camera_projection.h"
#include "rasterizer/rasterizer.h"
#include "bitmaps/bitmaps.h"
#include "bitmaps/bitmap_color_conversion.h"
#include "text/draw_string.h"
#include "text/font_group.h"
#include "text/unicode.h"

#include "ae_hooks.h"
#include "ae_hud_style.h"
#include "ae_key_prompts.h"
#include "ae_mark_calls.h"
#include "ae_mark_prompt.h"
#include "ae_mark_prompt_rules.h"
#include "ae_mark_places.h"
#include "ae_mark_rules.h"
#include "ae_marker_draw.h"
#include "ae_marker_rules.h"
#include "ae_mark_targets.h"
#include "ae_marks.h"
#include "ae_marks_left_game.h"
#include "ae_pause.h"
#include "ae_seats.h"
#include "ae_waypoint_icons.h"
#include "ae_waypoint_ink.h"
#include "ae_wheel.h"
#include "ae_wheel_draw.h"
#include "ae_wheel_rules.h"
#include "ae_zoom.h"
#include "network_chat.h"
#include "network_distributed.h"
#include "network_voice.h"
#include "halo_keyboard.h"

#include <math.h>

void platform_log(const char *format, ...) __attribute__((format(printf, 1, 2)));
void platform_ui_tone_start(short const *samples, unsigned long frames, unsigned long channels,
	unsigned long sample_rate);
struct bitmap_data const *hud_item_timers_ae_glyph(short glyph);
long hud_item_timers_ae_font_index(void);
short hud_item_timers_ae_waypoint_style(void);
real hud_item_timers_ae_waypoint_scale(real world_units);
real hud_item_timers_ae_outline_step(real row_scale);
boolean game_engine_scoreboard_held(short local_player_index);
real game_engine_scoreboard_shown(short local_player_index);

/* ---------- constants */

#define MARK_RANGE 100.0f
#define MARK_ENEMY_REACH 0.35f
#define MARK_ITEM_REACH 0.10f
/* an enemy far off is small and moving: how much wider its reach grows with distance, and how far past where the aim
meets the ground it is still found (a low aim at someone on a rise) */
#define MARK_ENEMY_REACH_SLOPE 0.05f
#define MARK_ENEMY_PAST_HIT 3.0f
/* an item is small: how much wider its reach grows with distance, per world unit (about 1.7 degrees) */
#define MARK_ITEM_REACH_SLOPE 0.03f
/* a driver has no reticle to aim with and looks over the vehicle: wider reaches, and past where the aim meets the ground */
#define MARK_DRIVER_ENEMY_REACH_SLOPE 0.10f
#define MARK_DRIVER_ITEM_REACH 0.30f
#define MARK_DRIVER_ITEM_REACH_SLOPE 0.14f
#define MARK_DRIVER_PAST_HIT 8.0f
/* the widest an item's reach grows: far off, an item you can hardly see is not what you meant */
#define MARK_ITEM_REACH_MOST 0.5f
#define MARK_DRIVER_ITEM_REACH_MOST 3.0f
/* the view reaches are set for: a zoomed view's are narrower by as much, so they stay the same on the screen */
#define MARK_REACH_FIELD_OF_VIEW 1.2217305f
#define MARK_ITEM_ABOVE 0.15f
/* how many allies, and pieces of your own vehicle, the aim can pass through */
#define MARK_THROUGH_STEPS 6

/* the ellipse a mark stays inside, of the view's size */
#define MARK_ELLIPSE_X 0.4375f
#define MARK_ELLIPSE_Y 0.315f
/* a marker behind one of the callout wheel's sectors fades to this */
#define MARK_WHEEL_FADE 0.3f

/* how near a client's spot the host must find what it says it marked */
#define MARK_HOST_ENEMY_NEAR 4.0f
/* how near a call's point is the same one made again */
#define MARK_REPEAT_NEAR 2.0f
/* how far short of a point the host's line to it stops: the wall the aim met would block it */
#define MARK_HOST_POINT_SHORT 0.5f
#define MARK_HOST_ITEM_NEAR 3.0f

/* how near a power item's spawn a place mark is for its timer to be the mark's fact: across, and up or down */
#define MARK_TIMER_NEAR 1.5f
#define MARK_TIMER_NEAR_VERTICAL 2.0f

#define MARK_TONE_RATE 22050
#define MARK_TONE_FRAMES 3300

/* ---------- globals */

/* how near the aim must pass an empty power spawn to take it, world units */
#define MARK_SPAWN_RADIUS 1.5f

static struct mark marks[MARK_PLAYERS];
static struct ae_mark_press pad_press[MAXIMUM_LOCAL_PLAYERS];
static long marks_game = NONE;
static struct ae_mark_spam pad_spam[MAXIMUM_LOCAL_PLAYERS];
static struct ae_mark_spam host_spam[MARK_PLAYERS];
/* (the host) the tick of each player's last counted mark, plus one: what a double tap rides */
static long host_counted[MARK_PLAYERS];
/* the last feed line each local player's view had, for the merging of a call made again */
static struct ae_feed_merge feed_merges[MAXIMUM_LOCAL_PLAYERS];

/* what crosses: the marker's absolute index, the kind (or MARK_CLEAR), the word and the spot */
struct distributed_mark
{
	byte player_index;
	byte kind;
	byte word;
	byte pad;
	float x, y, z;
};

struct mark_message
{
	struct distributed_message_header header;
	struct distributed_mark mark;
};

/* (indexed by MARK_WORD_*: new words go at the end) */
static wchar_t const *const mark_words[] =
	{ L"ENEMY", L"WEAPON", L"VEHICLE", L"ITEM", L"HERE", L"FIGHT", L"HILL", L"TELEPORTER", L"BOMB SITE" };
typedef char verify_mark_words[NUMBEROF(mark_words) == MARK_WORD_COUNT ? 1 : -1];

/* ---------- private code */

/* a short tone a kind, made in code: an enemy's falls and is the highest, a place's the lowest */
static void mark_sound(short kind)
{
	static short const pitch_from[AE_MARK_KIND_COUNT] = { 1046, 784, 587 };
	static short const pitch_to[AE_MARK_KIND_COUNT] = { 698, 784, 587 };
	static short tones[AE_MARK_KIND_COUNT][MARK_TONE_FRAMES], played[AE_MARK_KIND_COUNT][MARK_TONE_FRAMES];
	static boolean made[AE_MARK_KIND_COUNT];
	float volume = ae_settings_menu_volume();
	long at;

	if (!made[kind])
	{
		float phase = 0.0f;

		for (at = 0; at < MARK_TONE_FRAMES; at++)
		{
			float part = (float)at / (float)MARK_TONE_FRAMES;
			float pitch = (float)pitch_from[kind] + (float)(pitch_to[kind] - pitch_from[kind]) * part;
			float envelope = part < 0.05f ? part / 0.05f : (1.0f - part) * (1.0f - part);

			phase += pitch / (float)MARK_TONE_RATE;
			tones[kind][at] = (short)(sinf(phase * 6.2831853f) * envelope * 12000.0f);
		}
		made[kind] = TRUE;
	}
	if (volume > 1.0f)
		volume = 1.0f;
	if (volume <= 0.0f)
		return;
	for (at = 0; at < MARK_TONE_FRAMES; at++)
		played[kind][at] = (short)((float)tones[kind][at] * volume);
	platform_ui_tone_start(played[kind], MARK_TONE_FRAMES, 1, MARK_TONE_RATE);
}

static boolean mark_is_enemy(struct player_datum const *player, struct object_datum const *object)
{
	return !game_engine_has_teams() || game_team_is_enemy(player->team_index, object->object.owner_team_index);
}

/* how far from the aim's line a point is (squared), and how far along it; FALSE when behind the eye */
/* whether an enemy rides this vehicle, alive */
static boolean mark_vehicle_has_enemy(struct player_datum const *player, struct object_datum const *vehicle)
{
	long child;

	for (child = vehicle->object.first_child_object_index; child != NONE; child = object_get(child)->object.next_object_index)
	{
		struct object_datum *rider = object_get(child);

		if (TEST_FLAG(_object_mask_biped, rider->object.type) && !TEST_FLAG(rider->object.damage_flags, _object_dead_bit) &&
			mark_is_enemy(player, rider))
		{
			return TRUE;
		}
	}
	return FALSE;
}

static boolean mark_off_line(real_point3d const *eye, real_vector3d const *forward, real_point3d const *point,
	real *along, real *across_squared)
{
	real_vector3d to;

	to.i = point->x - eye->x;
	to.j = point->y - eye->y;
	to.k = point->z - eye->z;
	*along = to.i * forward->i + to.j * forward->j + to.k * forward->k;
	if (*along <= 0.0f)
		return FALSE;
	*across_squared = to.i * to.i + to.j * to.j + to.k * to.k - *along * *along;
	return TRUE;
}

/* whether nothing but the marker's own vehicle (own, their outermost object) stands between the eye and the object */
static boolean mark_clear_to(real_point3d const *eye, real_point3d const *point, long own, long target)
{
	real_point3d start = *eye;
	long ignore = own;
	short step;

	for (step = 0; step < MARK_THROUGH_STEPS; step++)
	{
		struct collision_result collision;
		real_vector3d ray;
		long hit;

		ray.i = point->x - start.x;
		ray.j = point->y - start.y;
		ray.k = point->z - start.z;
		if (!collision_test_vector(_collision_test_for_projectiles_flags, &start, &ray, ignore, &collision))
			return TRUE;
		if (collision.type != _collision_result_object || collision.object_index == NONE)
			return FALSE;
		hit = object_get_ultimate_parent(collision.object_index);
		if (hit == target)
			return TRUE;
		if (hit != own)
			return FALSE;
		ignore = collision.object_index;
		start = collision.point;
	}
	return FALSE;
}

/* whether any of it can be seen: its middle, or a biped's head over cover */
static boolean mark_object_in_view(real_point3d const *eye, struct object_datum const *object, long own, long object_index)
{
	real_point3d head;

	if (mark_clear_to(eye, &object->object.bounding_sphere_center, own, object_index))
		return TRUE;
	if (!TEST_FLAG(_object_mask_biped, object->object.type))
		return FALSE;
	unit_get_head_position(object_index, &head);
	return mark_clear_to(eye, &head, own, object_index);
}

/* whether the object is a flag or the ball: the game's objectives lying about, which are weapons */
static boolean mark_is_objective(long object_index)
{
	struct object_datum *object = object_get(object_index);
	char const *name;

	if (!TEST_FLAG(_object_mask_weapon, object->object.type))
		return FALSE;
	if (weapon_is_flag(object_index))
		return TRUE;
	name = tag_name_strip_path(tag_get_name(object->definition_index));
	if (!strncmp(name, "mp_", 3))
		name += 3;
	return !strcmp(name, "ball");
}

/* what a scan looks for: these types (which of them: all, the objectives, or all but those), before this far along the aim, and
within this range of it (its slope with distance, the widest it grows, and the view's narrowing) */
enum { MARK_WHICH_ANY, MARK_WHICH_OBJECTIVES, MARK_WHICH_PICKUPS };

struct mark_scan
{
	unsigned long types;
	short which;
	boolean enemy_riders;	/* a vehicle counts only with an enemy aboard */
	real before, range, reach_slope, reach_most, view_scale;
};

/* the part of its range a point is off the aim by, squared (under 1: in range), or 2 when it is behind the eye or past where the
aim ends. radius: a place's own size, within which the aim counts as on it. limited: not past before (a marker is seen through walls) */
static real mark_part_of(struct mark_scan const *scan, real_point3d const *eye, real_vector3d const *forward,
	real_point3d const *point, real radius, boolean limited)
{
	real along, across, allowed;

	if (!mark_off_line(eye, forward, point, &along, &across) || (limited ? along > scan->before + radius : along > MARK_RANGE))
		return 2.0f;
	allowed = (scan->range + scan->reach_slope * along);
	if (allowed > scan->reach_most)
		allowed = scan->reach_most;
	allowed *= scan->view_scale;
	if (radius > 0.0f)
	{
		across = sqrtf(across);
		across = across > radius ? across - radius : 0.0f;
		across *= across;
	}
	return across / (allowed * allowed);
}

/* what a scan found: the nearest to the aim (its part), and whether the object own_target, when it is one of those, is in
range and in view too */
struct mark_found
{
	long object_index;
	real part;
	long own_target;
	boolean own_in_reach;
};

/* the nearest to the aim of these objects, within its reach and before the hit */
static void mark_nearest_on_line(struct player_datum const *player, real_point3d const *eye, real_vector3d const *forward,
	struct mark_scan const *scan, struct mark_found *found)
{
	struct object_iterator iterator;
	struct object_datum *object;
	long own = object_get_ultimate_parent(player->unit_index);

	found->object_index = NONE;
	found->part = 1.0f;
	found->own_in_reach = FALSE;
	object_iterator_new(&iterator, scan->types, 0);
	while ((object = object_iterator_next(&iterator)) != NULL)
	{
		long object_index = iterator.index;
		boolean is_own = object_index == found->own_target;
		real part;

		if (object_index == own || object_index == player->unit_index || object->object.parent_object_index != NONE)
			continue;
		if (TEST_FLAG(_object_mask_biped, object->object.type) &&
			(TEST_FLAG(object->object.damage_flags, _object_dead_bit) || !mark_is_enemy(player, object)))
		{
			continue;
		}
		if (scan->enemy_riders && TEST_FLAG(_object_mask_vehicle, object->object.type) && !mark_vehicle_has_enemy(player, object))
			continue;
		part = mark_part_of(scan, eye, forward, &object->object.bounding_sphere_center, 0.0f, TRUE);
		if (part >= 1.0f || (part >= found->part && !is_own))
			continue;
		if (scan->which != MARK_WHICH_ANY && mark_is_objective(object_index) != (scan->which == MARK_WHICH_OBJECTIVES))
			continue;
		if (!mark_object_in_view(eye, object, own, object_index))
			continue;
		if (is_own)
			found->own_in_reach = TRUE;
		if (part < found->part)
		{
			found->object_index = object_index;
			found->part = part;
		}
	}
}

/* what the item is, for its mark's row: its tag's own name in capitals, without the multiplayer word some have */
static void mark_item_name(struct mark *mark, long object_index)
{
	/* (the tags whose own names are not what players call them) */
	static char const *const called[][2] =
	{
		{ "gravity rifle", "fuel rod gun" },
		{ "over shield", "overshield" },
		{ "active camouflage", "active camo" },
		{ "full-spectrum vision", "night vision" },
		{ "rwarthog", "rocket warthog" },
		{ "ball", "oddball" },
	};
	char const *name = tag_name_strip_path(tag_get_name(object_get(object_index)->definition_index));
	short length = 0;
	short index;

	if (!strncmp(name, "mp_", 3))
		name += 3;
	for (index = 0; index < (short)NUMBEROF(called); index++)
	{
		if (!strcmp(name, called[index][0]))
		{
			name = called[index][1];
			break;
		}
	}
	for (; *name && length < MARK_NAME_SIZE - 1; name++)
	{
		if (!strcmp(name, "_mp") || !strcmp(name, " mp"))
			break;
		mark->name[length++] = *name == '_' ? L' ' : (wchar_t)toupper((unsigned char)*name);
	}
	mark->name[length] = 0;
}

static void mark_set_item(struct mark *mark, long object_index)
{
	struct object_datum *object = object_get(object_index);

	mark->kind = AE_MARK_KIND_ITEM;
	mark->object_index = object_index;
	mark->word = TEST_FLAG(_object_mask_vehicle, object->object.type) ? MARK_WORD_VEHICLE :
		TEST_FLAG(_object_mask_weapon, object->object.type) ? MARK_WORD_WEAPON : MARK_WORD_ITEM;
	mark->spot = object->object.bounding_sphere_center;
	/* (a vehicle's over its roof; a small item's just over it, not to climb the screen when you look down at it near) */
	mark->spot.z += TEST_FLAG(_object_mask_vehicle, object->object.type) ? object->object.bounding_sphere_radius + MARK_ABOVE :
		MARK_ITEM_ABOVE;
	mark_item_name(mark, object_index);
}

static void mark_set_enemy(struct mark *mark, long object_index)
{
	mark->kind = AE_MARK_KIND_ENEMY;
	mark->object_index = object_index;
	mark->word = MARK_WORD_ENEMY;
	/* (an enemy's vehicle: over its roof) */
	if (TEST_FLAG(_object_mask_vehicle, object_get(object_index)->object.type))
	{
		mark->spot = object_get(object_index)->object.bounding_sphere_center;
		mark->spot.z += object_get(object_index)->object.bounding_sphere_radius;
	}
	else
	{
		unit_get_head_position(object_index, &mark->spot);
	}
	mark->spot.z += MARK_ABOVE;
}

/* an enemy at a point, where no one stands to be marked: just over it */
static void mark_set_enemy_point(struct mark *mark, real_point3d const *spot)
{
	mark->kind = AE_MARK_KIND_ENEMY;
	mark->object_index = NONE;
	mark->word = MARK_WORD_ENEMY;
	mark->spot = *spot;
	mark->spot.z += MARK_ABOVE;
}

static void mark_set_place(struct mark *mark, real_point3d const *spot)
{
	mark->kind = AE_MARK_KIND_PLACE;
	mark->object_index = NONE;
	mark->word = MARK_WORD_HERE;
	mark->spot = *spot;
}

/* whether this player sits in a vehicle's driver seat */
static boolean mark_driving(struct player_datum const *player)
{
	long own = object_get_ultimate_parent(player->unit_index);
	short seat = ((struct unit_datum *)object_get(player->unit_index))->unit.parent_seat_index;

	return own != player->unit_index && seat != NONE && TEST_FLAG(_object_mask_unit, object_get(own)->object.type) &&
		unit_seat_is_driver(own, seat);
}

/* ---------- the aim */

/* the aim is looked at again after this many ticks, unless a press asks */
#define MARK_AIM_RECAST_TICKS 3

static struct ae_mark_aim aims[MAXIMUM_LOCAL_PLAYERS];

static boolean mark_player_muted_here(struct player_datum const *player);
static struct player_datum *mark_local_player(short local_player_index, long *player_index);

/* where the aim goes: the first thing in its way that is not the player's own vehicle or an ally */
struct mark_trace
{
	boolean hit;
	real hit_at;
	long hit_object;
	real_point3d point;
};

static void mark_trace_aim(struct player_datum const *player, struct observer_result const *camera, struct mark_trace *trace)
{
	struct collision_result collision;
	real_point3d start = camera->position;
	real_vector3d ray;
	long own = object_get_ultimate_parent(player->unit_index);
	long ignore = own;
	short step;

	trace->hit = FALSE;
	trace->hit_at = MARK_RANGE;
	trace->hit_object = NONE;
	for (step = 0; step < MARK_THROUGH_STEPS; step++)
	{
		real left = MARK_RANGE - distance3d(&camera->position, &start);

		if (left <= 0.0f)
			break;
		scale_vector3d(&camera->forward, left, &ray);
		if (!collision_test_vector(_collision_test_for_projectiles_flags, &start, &ray, ignore, &collision))
			break;
		if (collision.type == _collision_result_object && collision.object_index != NONE)
		{
			struct object_datum *object;

			trace->hit_object = object_get_ultimate_parent(collision.object_index);
			/* (your own vehicle is not marked: the view from a driver's seat looks over it) */
			if (trace->hit_object == own)
			{
				ignore = collision.object_index;
				trace->hit_object = NONE;
				start = collision.point;
				continue;
			}
			object = object_get(trace->hit_object);
			/* (an ally is not marked: the aim goes on past them) */
			if (TEST_FLAG(_object_mask_biped, object->object.type) &&
				!TEST_FLAG(object->object.damage_flags, _object_dead_bit) && !mark_is_enemy(player, object))
			{
				ignore = trace->hit_object;
				trace->hit_object = NONE;
				start = collision.point;
				continue;
			}
		}
		trace->hit = TRUE;
		trace->hit_at = distance3d(&camera->position, &collision.point);
		trace->point = collision.point;
		break;
	}
}

/* ---------- the targets */

enum { MARK_FROM_ENEMY, MARK_FROM_ITEM, MARK_FROM_BODY, MARK_FROM_MARKER, MARK_FROM_PLACE };

/* a thing the aim could take: its tier and how far off the aim it is, what it is (MARK_FROM_*), and where or which */
struct mark_candidate
{
	short tier;
	short source;
	short word;			/* a place's */
	long object_index;	/* the object, or the slot of the marker */
	real part;
	real_point3d point;
};

static void mark_consider(struct mark_candidate *best, short tier, short source, long object_index, real part)
{
	if (!ae_mark_candidate_better(tier, part, best->tier, best->part))
		return;
	best->tier = tier;
	best->source = source;
	best->object_index = object_index;
	best->part = part;
}

static void mark_consider_place(struct mark_candidate *best, short tier, short source, short word, real_point3d const *point, real part)
{
	if (!ae_mark_candidate_better(tier, part, best->tier, best->part))
		return;
	best->tier = tier;
	best->source = source;
	best->object_index = NONE;
	best->part = part;
	best->word = word;
	best->point = *point;
}

/* whether the mark is still there, within its life */
static boolean mark_alive(struct mark const *mark, long now)
{
	return mark->active && !(now >= mark->born && now - mark->born >= (mark->ends ? mark->ends - mark->born : AE_MARK_LIFE_TICKS));
}

/* whether an enemy is the one the player's own mark is on, where it was marked */
static boolean mark_enemy_is_own(struct mark const *own, long object_index)
{
	struct mark enemy;

	if (own->kind != AE_MARK_KIND_ENEMY || own->object_index != object_index)
		return FALSE;
	csmemset(&enemy, 0, sizeof(enemy));
	mark_set_enemy(&enemy, object_index);
	return ae_mark_press_is_own(own->kind, own->object_index, enemy.kind, enemy.object_index,
		distance_squared3d(&own->spot, &enemy.spot)) != 0;
}

/* whether a point is the place the player's own mark is on */
static boolean mark_place_is_own(struct mark const *own, real_point3d const *point)
{
	return own->kind == AE_MARK_KIND_PLACE && ae_mark_press_is_own(own->kind, NONE, AE_MARK_KIND_PLACE, NONE,
		distance_squared3d(&own->spot, point));
}

/* whether more is to be looked at: the player's own mark settles it, once found; with one to look for every tier is, else only
those no worse than the best so far */
static boolean mark_pass_open(struct mark_candidate const *best, struct mark_candidate const *own, boolean own_live, short tier)
{
	if (own->tier)
		return FALSE;
	return own_live || best->tier == 0 || tier <= best->tier;
}

/* where a dead player's body lies, when it is there to be marked */
static struct object_datum *mark_body_of(struct player_datum const *player)
{
	struct object_datum *object;

	if (player->dead_unit_index == NONE ||
		(object = object_try_and_get_and_verify_type(player->dead_unit_index, _object_mask_unit)) == NULL)
	{
		return NULL;
	}
	return TEST_FLAG(object->object.damage_flags, _object_dead_bit) ? object : NULL;
}

/* what the aim is on: FALSE when nothing is in range. answers: the markers there are (a teammate's to answer, your own to
cancel) are targets too, and the target of your own mark is your own mark */
static boolean mark_seen_here(short local_player_index, short index, struct mark const *mark, real_point3d const *eye, long now);

static boolean mark_pick(short local_player_index, struct ae_mark_aim *aim, boolean answers)
{
	struct observer_result const *camera = observer_get_camera(local_player_index);
	long player_index = local_player_get_player_index(local_player_index);
	struct player_datum *player = player_index == NONE ? NULL : player_try_and_get(player_index);
	struct mark_trace trace;
	struct mark_candidate best, own, nearest_body;
	struct mark_scan scan;
	struct mark_found found;
	struct mark const *own_mark;
	long own_absolute, now = game_time_get();
	boolean driving, own_live;
	real past_hit, view_scale;

	csmemset(aim, 0, sizeof(*aim));
	aim->cast_at = now;
	aim->verb = AE_CALL_MARK;
	aim->reply_to = NONE;
	if (!camera || !player || player->unit_index == NONE)
		return FALSE;
	own_absolute = DATUM_INDEX_TO_ABSOLUTE_INDEX(player_index) % MARK_PLAYERS;
	own_mark = &marks[own_absolute];
	own_live = answers && mark_alive(own_mark, now);
	csmemset(&best, 0, sizeof(best));
	csmemset(&own, 0, sizeof(own));
	csmemset(&nearest_body, 0, sizeof(nearest_body));
	driving = mark_driving(player);
	past_hit = driving ? MARK_DRIVER_PAST_HIT : 0.5f;
	view_scale = camera->field_of_view > 0.0f && camera->field_of_view < MARK_REACH_FIELD_OF_VIEW ?
		camera->field_of_view / MARK_REACH_FIELD_OF_VIEW : 1.0f;
	mark_trace_aim(player, camera, &trace);

	/* enemies: the one the aim is on, else the nearest to it (the range grows with distance, and a driver's is wider) */
	scan.types = _object_mask_biped | _object_mask_vehicle;
	scan.enemy_riders = TRUE;
	scan.which = MARK_WHICH_ANY;
	scan.before = trace.hit_at + (driving ? past_hit : MARK_ENEMY_PAST_HIT);
	scan.range = MARK_ENEMY_REACH;
	scan.reach_slope = driving ? MARK_DRIVER_ENEMY_REACH_SLOPE : MARK_ENEMY_REACH_SLOPE;
	scan.reach_most = MARK_RANGE;
	scan.view_scale = view_scale;
	found.own_target = own_live && own_mark->kind == AE_MARK_KIND_ENEMY ? own_mark->object_index : NONE;
	if (trace.hit_object != NONE && ((TEST_FLAG(_object_mask_biped, object_get(trace.hit_object)->object.type) &&
		!TEST_FLAG(object_get(trace.hit_object)->object.damage_flags, _object_dead_bit)) ||
		(TEST_FLAG(_object_mask_vehicle, object_get(trace.hit_object)->object.type) &&
		mark_vehicle_has_enemy(player, object_get(trace.hit_object)))))
	{
		mark_consider(&best, AE_TIER_ENEMY, MARK_FROM_ENEMY, trace.hit_object, 0.0f);
		if (own_live && mark_enemy_is_own(own_mark, trace.hit_object))
			mark_consider(&own, AE_TIER_ENEMY, MARK_FROM_ENEMY, trace.hit_object, 0.0f);
	}
	else
	{
		mark_nearest_on_line(player, &camera->position, &camera->forward, &scan, &found);
		if (found.object_index != NONE)
			mark_consider(&best, AE_TIER_ENEMY, MARK_FROM_ENEMY, found.object_index, found.part);
		if (found.own_in_reach && mark_enemy_is_own(own_mark, found.own_target))
			mark_consider(&own, AE_TIER_ENEMY, MARK_FROM_ENEMY, found.own_target, 0.0f);
	}

	scan.enemy_riders = FALSE;
	/* the item reaches, for bodies, items and named places (and the markers) */
	scan.before = trace.hit_at + past_hit;
	scan.range = driving ? MARK_DRIVER_ITEM_REACH : MARK_ITEM_REACH;
	scan.reach_slope = driving ? MARK_DRIVER_ITEM_REACH_SLOPE : MARK_ITEM_REACH_SLOPE;
	scan.reach_most = driving ? MARK_DRIVER_ITEM_REACH_MOST : MARK_ITEM_REACH_MOST;

	/* dead bodies the player can see, while they last (kept apart: against an objective, a vehicle or a pickup one counts only when
	nearer the aim, and that does not rank the same way in a row) */
	if (mark_pass_open(&best, &own, own_live, AE_TIER_BODY))
	{
		struct data_iterator iterator;
		struct player_datum *other;
		long own_unit = object_get_ultimate_parent(player->unit_index);

		data_iterator_new(&iterator, player_data);
		while ((other = (struct player_datum *)data_iterator_next(&iterator)) != NULL)
		{
			struct object_datum *body;
			real_point3d spot;
			real part;
			boolean is_own;

			if (iterator.datum_index == player_index || other->quit_out_of_game || (body = mark_body_of(other)) == NULL)
				continue;
			part = mark_part_of(&scan, &camera->position, &camera->forward, &body->object.bounding_sphere_center, 0.0f, TRUE);
			if (part >= 1.0f)
				continue;
			object_get_origin(other->dead_unit_index, &spot);
			is_own = own_live && mark_place_is_own(own_mark, &spot);
			if (!is_own && (best.tier == AE_TIER_ENEMY || !ae_mark_candidate_better(AE_TIER_BODY, part, nearest_body.tier, nearest_body.part)))
				continue;
			if (!mark_object_in_view(&camera->position, body, own_unit, other->dead_unit_index))
				continue;
			if (is_own)
				mark_consider_place(&own, AE_TIER_BODY, MARK_FROM_BODY, MARK_WORD_FIGHT, &spot, part);
			mark_consider_place(&nearest_body, AE_TIER_BODY, MARK_FROM_BODY, MARK_WORD_FIGHT, &spot, part);
		}
	}

	/* markers: seen through walls, so only the angle counts */
	if (answers && mark_pass_open(&best, &own, own_live, AE_TIER_MARKER))
	{
		short slot;

		for (slot = 0; slot < MARK_PLAYERS; slot++)
		{
			struct mark const *marker = &marks[slot];
			struct player_datum *marker_player;
			real part;

			if (!mark_alive(marker, now))
				continue;
			if (slot != own_absolute)
			{
				if (!game_engine_has_teams() || marker->team_index != player->team_index)
					continue;
				marker_player = distributed_player(slot);
				if (marker_player && mark_player_muted_here(marker_player))
					continue;
				/* (one this view does not show cannot be answered) */
				if (marker->kind == AE_MARK_KIND_ENEMY && !ae_marks_rule_through_walls() &&
					!mark_seen_here(local_player_index, slot, marker, &camera->position, now))
				{
					continue;
				}
			}
			part = mark_part_of(&scan, &camera->position, &camera->forward, &marker->spot, 0.0f, FALSE);
			if (part >= 1.0f)
				continue;
			if (slot == own_absolute)
				mark_consider(&own, AE_TIER_MARKER, MARK_FROM_MARKER, slot, part);
			else
				mark_consider(&best, AE_TIER_MARKER, MARK_FROM_MARKER, slot, part);
		}
	}

	/* objectives (a flag or the ball not held), vehicles, and the named places of the map */
	if (mark_pass_open(&best, &own, own_live, AE_TIER_NAMED))
	{
		struct ae_mark_place places[AE_MARK_PLACES_MOST];
		short count, index;

		found.own_target = own_live && own_mark->kind == AE_MARK_KIND_ITEM ? own_mark->object_index : NONE;
		scan.types = _object_mask_weapon;
		scan.which = MARK_WHICH_OBJECTIVES;
		mark_nearest_on_line(player, &camera->position, &camera->forward, &scan, &found);
		if (found.object_index != NONE)
			mark_consider(&best, AE_TIER_NAMED, MARK_FROM_ITEM, found.object_index, found.part);
		if (found.own_in_reach)
			mark_consider(&own, AE_TIER_NAMED, MARK_FROM_ITEM, found.own_target, 0.0f);
		/* (a vehicle the aim is on; a driver, who has no reticle, finds the nearest too) */
		if (trace.hit_object != NONE && TEST_FLAG(_object_mask_vehicle, object_get(trace.hit_object)->object.type))
		{
			mark_consider(&best, AE_TIER_NAMED, MARK_FROM_ITEM, trace.hit_object, 0.0f);
			if (own_live && own_mark->kind == AE_MARK_KIND_ITEM && own_mark->object_index == trace.hit_object)
				mark_consider(&own, AE_TIER_NAMED, MARK_FROM_ITEM, trace.hit_object, 0.0f);
		}
		if (driving)
		{
			scan.types = _object_mask_vehicle;
			scan.which = MARK_WHICH_ANY;
			mark_nearest_on_line(player, &camera->position, &camera->forward, &scan, &found);
			if (found.object_index != NONE)
				mark_consider(&best, AE_TIER_NAMED, MARK_FROM_ITEM, found.object_index, found.part);
			if (found.own_in_reach)
				mark_consider(&own, AE_TIER_NAMED, MARK_FROM_ITEM, found.own_target, 0.0f);
		}
		count = ae_mark_targets_places(places, AE_MARK_PLACES_MOST);
		for (index = 0; index < count; index++)
		{
			real part = mark_part_of(&scan, &camera->position, &camera->forward, &places[index].point, places[index].radius, TRUE);

			if (part >= 1.0f)
				continue;
			if (own_live && mark_place_is_own(own_mark, &places[index].point))
				mark_consider_place(&own, AE_TIER_NAMED, MARK_FROM_PLACE, places[index].word, &places[index].point, part);
			mark_consider_place(&best, AE_TIER_NAMED, MARK_FROM_PLACE, places[index].word, &places[index].point, part);
		}
		/* (a power item's empty spawn, with the timers on: nothing stands there to aim at, so the aim near it takes it) */
		if (game_engine_item_timers_active())
		{
			short spawns = item_timers_count();

			for (index = 0; index < spawns; index++)
			{
				struct item_timer const *timer = item_timers_get(index);
				real part;

				if (!timer || timer->timer_class >= NUMBER_OF_ITEM_TIMER_POWER_CLASSES)
					continue;
				part = mark_part_of(&scan, &camera->position, &camera->forward, &timer->position, MARK_SPAWN_RADIUS, TRUE);
				/* (one with its item there is the item's: only looked for when the aim is near) */
				if (part >= 1.0f || ae_marks_find(player, AE_MARK_KIND_ITEM, &timer->position) != NONE)
					continue;
				if (own_live && mark_place_is_own(own_mark, &timer->position))
					mark_consider_place(&own, AE_TIER_NAMED, MARK_FROM_PLACE, MARK_WORD_HERE, &timer->position, part);
				mark_consider_place(&best, AE_TIER_NAMED, MARK_FROM_PLACE, MARK_WORD_HERE, &timer->position, part);
			}
		}
	}

	/* weapons, ammo, grenades, power items, health */
	if (mark_pass_open(&best, &own, own_live, AE_TIER_PICKUP))
	{
		found.own_target = own_live && own_mark->kind == AE_MARK_KIND_ITEM ? own_mark->object_index : NONE;
		scan.types = _object_mask_weapon | _object_mask_equipment;
		scan.which = MARK_WHICH_PICKUPS;
		mark_nearest_on_line(player, &camera->position, &camera->forward, &scan, &found);
		if (found.object_index != NONE)
			mark_consider(&best, AE_TIER_PICKUP, MARK_FROM_ITEM, found.object_index, found.part);
		if (found.own_in_reach)
			mark_consider(&own, AE_TIER_PICKUP, MARK_FROM_ITEM, found.own_target, 0.0f);
	}

	/* the ground the aim meets */
	if (trace.hit && mark_pass_open(&best, &own, own_live, AE_TIER_GROUND))
	{
		if (own_live && mark_place_is_own(own_mark, &trace.point))
			mark_consider_place(&own, AE_TIER_GROUND, MARK_FROM_PLACE, MARK_WORD_HERE, &trace.point, 0.0f);
		mark_consider_place(&best, AE_TIER_GROUND, MARK_FROM_PLACE, MARK_WORD_HERE, &trace.point, 0.0f);
	}

	if (ae_mark_candidate_better(nearest_body.tier, nearest_body.part, best.tier, best.part))
		best = nearest_body;

	/* the target of your own mark is your own mark: a press there takes it back, ahead of any tier */
	if (own.tier)
	{
		best = own;
		aim->verb = AE_CALL_CANCEL;
		aim->reply_to = (short)own_absolute;
	}
	if (!best.tier)
		return FALSE;
	aim->found = TRUE;
	aim->tier = best.tier;
	switch (best.source)
	{
	case MARK_FROM_ENEMY:
		mark_set_enemy(&aim->mark, best.object_index);
		break;
	case MARK_FROM_ITEM:
		mark_set_item(&aim->mark, best.object_index);
		break;
	case MARK_FROM_MARKER:
		aim->mark = marks[best.object_index];
		if (aim->verb != AE_CALL_CANCEL)
		{
			aim->reply_to = (short)best.object_index;
			aim->verb = (short)ae_mark_press_verb(0, (aim->mark.responders >> own_absolute) & 1,
				aim->mark.kind == AE_MARK_KIND_ITEM, aim->mark.dibs != -1);
		}
		break;
	default:
		mark_set_place(&aim->mark, &best.point);
		aim->mark.word = best.word;
		aim->mark.name[0] = 0;
		/* (an empty power spawn says what comes there) */
		if (best.tier == AE_TIER_NAMED && best.word == MARK_WORD_HERE)
		{
			short spawn = ae_marks_timer_for(&best.point);

			if (spawn)
			{
				wcsncpy(aim->mark.name, item_timers_get((short)(spawn - 1))->label, MARK_NAME_SIZE - 1);
				aim->mark.name[MARK_NAME_SIZE - 1] = 0;
			}
		}
		break;
	}
	return TRUE;
}

/* what the aim is on, as a mark would be made of it: FALSE when nothing is in range. place_only: the spot it meets, whatever
stands there. The markers are not looked at: a call of the wheel makes a mark */
static boolean mark_cast(short local_player_index, struct player_datum const *player, struct mark *mark, boolean place_only)
{
	struct observer_result const *camera = observer_get_camera(local_player_index);

	if (!camera)
		return FALSE;
	if (place_only)
	{
		struct mark_trace trace;

		mark_trace_aim(player, camera, &trace);
		if (!trace.hit)
			return FALSE;
		mark_set_place(mark, &trace.point);
		return TRUE;
	}
	else
	{
		struct ae_mark_aim aim;

		if (!mark_pick(local_player_index, &aim, FALSE))
			return FALSE;
		*mark = aim.mark;
		return TRUE;
	}
}

int config_boolean(char const *name);

/* the host's network.marks: this machine's in a game it hosts, on a client as the host sent it; always on in a game with no network */
boolean ae_marks_rule_on(void)
{
	int on = 1;

	if (game_connection() == _game_connection_network_server)
		return config_boolean("network.marks") != 0;
	if (game_connection() == _game_connection_network_client)
		ae_mark_rule_read(network_chat_marks_byte(), &on);
	return on != 0;
}

/* the gametype's ENEMY MARKS THROUGH WALLS: the variant's, the host's on a client */
boolean ae_marks_rule_through_walls(void)
{
	return game_engine_running() && ae_marks_walls_of(game_engine_get_variant()->universal_variant.ball_melee);
}

struct ae_mark_aim const *ae_marks_aim(short local_player_index)
{
	static struct ae_mark_aim none;
	struct ae_mark_aim *aim;
	long now = game_time_get();

	if (!VALID_INDEX(local_player_index, MAXIMUM_LOCAL_PLAYERS) || !game_engine_running() || !ae_marks_rule_on())
		return &none;
	aim = &aims[local_player_index];
	if (aim->cast_at != NONE && now >= aim->cast_at && now - aim->cast_at < MARK_AIM_RECAST_TICKS)
		return aim;
	if (cinematic_in_progress())
	{
		csmemset(aim, 0, sizeof(*aim));
		aim->cast_at = now;
		return aim;
	}
	mark_pick(local_player_index, aim, TRUE);
	return aim;
}

static boolean mark_network(void)
{
	short connection = game_connection();

	return connection == _game_connection_network_client || connection == _game_connection_network_server;
}

static void mark_entry(struct distributed_mark *entry, long absolute, short kind, short word,
	real_point3d const *spot)
{
	csmemset(entry, 0, sizeof(*entry));
	entry->player_index = distributed_player_to_byte(absolute);
	entry->kind = (byte)kind;
	/* (a build before the newer words drops a word it does not know: its mark is HERE) */
	entry->word = (byte)(word > MARK_WORD_HERE ? MARK_WORD_HERE : word);
	if (spot)
	{
		entry->x = spot->x;
		entry->y = spot->y;
		entry->z = spot->z;
	}
}

/* (the host) the client machines with a player on the team */
short ae_marks_team_machines(long team_index, long *machines, short maximum)
{
	long all[HALO_PORT_MAXIMUM_NETWORK_MACHINES];
	short count = distributed_client_machines(all, HALO_PORT_MAXIMUM_NETWORK_MACHINES);
	short index, found = 0;

	if (!game_engine_has_teams())
		return 0;
	for (index = 0; index < count && found < maximum; index++)
	{
		struct data_iterator iterator;
		struct player_datum *player;
		boolean hears = FALSE;

		data_iterator_new(&iterator, player_data);
		while (!hears && (player = (struct player_datum *)data_iterator_next(&iterator)) != NULL)
		{
			hears = !player->quit_out_of_game && player->team_index == team_index &&
				distributed_machine_has_player(all[index], (short)DATUM_INDEX_TO_ABSOLUTE_INDEX(iterator.datum_index));
		}
		if (hears)
			machines[found++] = all[index];
	}
	return found;
}

/* (the host) to every client machine with a player on the marker's team, and only those */
static void mark_host_send(struct distributed_mark const *entry, long team_index)
{
	long machines[HALO_PORT_MAXIMUM_NETWORK_MACHINES];
	short count = ae_marks_team_machines(team_index, machines, HALO_PORT_MAXIMUM_NETWORK_MACHINES);
	short index;

	for (index = 0; index < count; index++)
	{
		struct mark_message message;

		csmemset(&message, 0, sizeof(message));
		message.mark = *entry;
		distributed_send_to_machine_reliably(machines[index], &message, _distributed_message_mark_down, 1,
			(word)sizeof(message));
	}
}

/* this mark, or its clearing, goes out */
static void mark_share(struct distributed_mark const *entry, long team_index)
{
	if (game_connection() == _game_connection_network_server)
		mark_host_send(entry, team_index);
	else if (game_connection() == _game_connection_network_client)
	{
		struct mark_message message;

		csmemset(&message, 0, sizeof(message));
		message.mark = *entry;
		distributed_send(&message, _distributed_message_mark_up, 1, (word)sizeof(message),
			_distributed_to_host_reliably);
	}
}

/* the plain mark, as the older builds have it */
void ae_marks_share_plain(long absolute, short kind, short word, real_point3d const *spot, long team_index)
{
	struct distributed_mark entry;

	mark_entry(&entry, absolute, kind, word, spot);
	mark_share(&entry, team_index);
}

void ae_marks_share_clear(long absolute, long team_index)
{
	struct distributed_mark entry;

	mark_entry(&entry, absolute, MARK_CLEAR, 0, NULL);
	mark_share(&entry, team_index);
}

/* whether a player here hears a teammate's mark */
static boolean mark_heard_here(long team_index)
{
	short local;

	for (local = local_player_get_next(NONE); local != NONE; local = local_player_get_next(local))
	{
		long player_index = local_player_get_player_index(local);

		if (player_index != NONE && player_get(player_index)->team_index == team_index)
			return TRUE;
	}
	return FALSE;
}

/* whether a player's machine is muted here: its marks are not shown or heard on this machine */
static boolean mark_player_muted_here(struct player_datum const *player)
{
	return network_voice_machine_muted(player->network_player_data.machine_index);
}

/* a press: the guard's word, on the marker's HUD */
static boolean mark_guard(short local_player_index)
{
	int said = ae_mark_spam_take(&pad_spam[local_player_index], game_time_get());

	if (said != AE_MARK_SPAM_OK)
		platform_log("marks: view %d: guard says %d at tick %ld", local_player_index, said, (long)game_time_get());
	switch (said)
	{
	case AE_MARK_SPAM_WARN:
		hud_print_message(local_player_index, L"Too many marks: marking stops soon");
		return TRUE;
	case AE_MARK_SPAM_NOW_BLOCKED:
		hud_print_message(local_player_index, L"Too many marks: blocked for 30 seconds");
		return FALSE;
	case AE_MARK_SPAM_STILL_BLOCKED:
		return FALSE;
	}
	return TRUE;
}

/* ae_bots_marks.inc (the host) */
void ae_bots_marks_heard(struct player_datum const *marker, short mark_kind, long object_index, real_point3d const *spot);
boolean ae_bots_marks_ride(struct player_datum const *marker);

/* whether this mark is the player's own one again: the same call at the same thing, or near the same point */
static boolean mark_repeat_of(struct mark const *own, struct mark const *next)
{
	if (!own->active || own->entry != next->entry || own->kind != next->kind)
		return FALSE;
	if (next->object_index != NONE)
		return own->object_index == next->object_index;
	return own->object_index == NONE && distance_squared3d(&own->spot, &next->spot) < MARK_REPEAT_NEAR * MARK_REPEAT_NEAR;
}

/* ---------- the timer fact and the feed */

/* the power item's spawn a place mark is made at: its place in the item timers, plus one; 0 for none, and in a game with no
timers (the timer is the mark's fact only where they run) */
short ae_marks_timer_for(real_point3d const *spot)
{
	short count, index, best = 0;
	real best_distance = MARK_TIMER_NEAR * MARK_TIMER_NEAR;

	if (!game_engine_item_timers_active())
		return 0;
	count = item_timers_count();
	for (index = 0; index < count; index++)
	{
		struct item_timer const *timer = item_timers_get(index);
		real dx, dy, distance;

		if (!timer || timer->timer_class >= NUMBER_OF_ITEM_TIMER_POWER_CLASSES)
			continue;
		dx = timer->position.x - spot->x;
		dy = timer->position.y - spot->y;
		distance = dx * dx + dy * dy;
		if ((real)fabs(timer->position.z - spot->z) <= MARK_TIMER_NEAR_VERTICAL && distance < best_distance)
		{
			best_distance = distance;
			best = (short)(index + 1);
		}
	}
	return best;
}

/* the item timer a mark's timer is, as the viewer reads it now: NULL with no timer on the mark, or none this game has (the number
came over the network: it is bound here), or while the power item is on the map */
static struct item_timer const *mark_timer_of(struct mark const *mark)
{
	struct item_timer const *timer;
	short index = mark->timer_plus_one;

	if (index < 1 || !game_engine_item_timers_active() || index > item_timers_count())
		return NULL;
	timer = item_timers_get((short)(index - 1));
	if (!timer || timer->timer_class >= NUMBER_OF_ITEM_TIMER_POWER_CLASSES || timer->period_ticks <= 0 || item_timer_on_map(timer))
		return NULL;
	return timer;
}

/* its power item's name (ROCKETS) and the seconds to its next spawn */
static boolean mark_timer_fact(struct mark const *mark, wchar_t *label, int size, long *seconds)
{
	struct item_timer const *timer = mark_timer_of(mark);

	if (!timer)
		return FALSE;
	ustrncpy_terminated(label, timer->label, (unsigned long)size);
	*seconds = (item_timer_ticks_left(timer) + TICKS_PER_SECOND - 1) / TICKS_PER_SECOND;
	return TRUE;
}

/* what a mark is called in the feed: an item's short name, a place's own word, the power item of its timer; else nothing (a
bare place) */
static void mark_feed_noun(struct mark const *mark, wchar_t *noun, int size)
{
	wchar_t label[16];
	long seconds;

	noun[0] = 0;
	if (mark->kind == AE_MARK_KIND_ENEMY)
		return;
	if (mark->kind == AE_MARK_KIND_ITEM)
	{
		wchar_t const *word = ae_prompt_word(AE_TIER_PICKUP, AE_CALL_MARK, mark->name);

		ae_feed_noun(noun, size, word ? word : L"Item");
		return;
	}
	if (mark->word == MARK_WORD_HERE)
	{
		if (mark_timer_fact(mark, label, NUMBEROF(label), &seconds))
			ae_feed_noun(noun, size, label);
		return;
	}
	if (mark->word >= 0 && mark->word < (short)NUMBEROF(mark_words))
		ae_feed_noun(noun, size, mark_words[mark->word]);
}

/* the same words, one number: the call a merged line counts */
static int mark_feed_call(struct mark const *mark, wchar_t const *noun, boolean dead)
{
	unsigned long hash = 2166136261u;

	for (; *noun; noun++)
		hash = (hash ^ (unsigned long)*noun) * 16777619u;
	hash = (hash ^ (unsigned long)(mark->entry < 0 ? 0 : mark->entry)) * 16777619u;
	hash = (hash ^ (unsigned long)(mark->kind + 1)) * 16777619u;
	hash = (hash ^ (unsigned long)(dead ? 1 : 0)) * 16777619u;
	return (int)(hash & 0x7fffffffu);
}

/* a mark has come to this machine (made here, or sent by the team): each local player on its team gets the feed's line, "Jenkins:
Rockets here", the same call from the same caller within 6 seconds counting on one line. None for a muted machine's, and none
for a call made again at the same place (its tone says it). The list may have no room for it: the kills keep their rows */
void ae_marks_feed(long absolute, struct mark const *mark)
{
	struct player_datum const *caller;
	wchar_t noun[24], words[40], line[64], label[16];
	long seconds = -1;
	boolean dead;
	int call;
	short local;

	if (!mark || !mark->active || mark->repeats > 0 || !VALID_INDEX(absolute, MARK_PLAYERS))
		return;
	caller = distributed_player((short)absolute);
	if (!caller || caller->quit_out_of_game || mark_player_muted_here(caller))
		return;
	dead = caller->unit_index == NONE;
	mark_feed_noun(mark, noun, NUMBEROF(noun));
	if (dead || mark->kind != AE_MARK_KIND_PLACE || mark->word != MARK_WORD_HERE || !noun[0] ||
		!mark_timer_fact(mark, label, NUMBEROF(label), &seconds))
	{
		seconds = -1;
	}
	if (!ae_feed_words(words, NUMBEROF(words), mark->entry < 0 ? AE_WHEEL_SMART : mark->entry, mark->kind, dead, noun,
		(int)seconds))
	{
		return;
	}
	call = mark_feed_call(mark, noun, dead);
	for (local = local_player_get_next(NONE); local != NONE; local = local_player_get_next(local))
	{
		long player_index = local_player_get_player_index(local);
		struct player_datum const *viewer = player_index == NONE ? NULL : player_try_and_get(player_index);
		struct ae_feed_merge trial;
		int count;

		if (!viewer || !VALID_INDEX(local, MAXIMUM_LOCAL_PLAYERS))
			continue;
		if (DATUM_INDEX_TO_ABSOLUTE_INDEX(player_index) != absolute &&
			(!game_engine_has_teams() || viewer->team_index != mark->team_index))
		{
			continue;
		}
		/* (the merge moves on only when the line had a slot: else the next call is a first one, not an x2 of nothing) */
		trial = feed_merges[local];
		count = ae_feed_merge(&trial, (int)absolute, call, game_time_get());
		if (ae_feed_line(line, NUMBEROF(line), caller->name, words, count) && hud_print_feed_message(local, line, count))
			feed_merges[local] = trial;
	}
}

/* the player's own mark, now: first, a new mark (counted by the guard, with its tone), else the replacing of one just made.
verb: AE_CALL_MARK for a press (entry AE_WHEEL_SMART for a tap, AE_WHEEL_ENEMY for a double tap), AE_CALL_WHEEL for the wheel's */
static boolean mark_commit(short local_player_index, long player_index, struct player_datum const *player, struct mark *next,
	boolean first, int verb, int entry)
{
	long absolute = DATUM_INDEX_TO_ABSOLUTE_INDEX(player_index);
	struct mark *own = &marks[absolute % MARK_PLAYERS];

	if (first && !mark_guard(local_player_index))
		return FALSE;
	next->active = TRUE;
	next->team_index = player->team_index;
	next->born = game_time_get();
	next->ends = next->born + AE_MARK_LIFE_TICKS;
	next->entry = (short)entry;
	next->responders = 0;
	next->dibs = -1;
	next->repeats = (short)ae_mark_repeats_after(verb == AE_CALL_WHEEL && mark_repeat_of(own, next), own->repeats);
	next->serial = (short)ae_mark_serial_next(own->serial);
	/* (a tap at an empty power spawn carries its timer) */
	next->timer_plus_one = player->unit_index != NONE && entry == AE_WHEEL_SMART && next->kind == AE_MARK_KIND_PLACE &&
		next->word == MARK_WORD_HERE ? ae_marks_timer_for(&next->spot) : 0;
	/* (a place the map names, not one with a word of its own as HILL; a fact's row has already named a power spawn) */
	next->place = !next->timer_plus_one && next->kind == AE_MARK_KIND_PLACE && next->word == MARK_WORD_HERE ?
		(short)ae_mark_place_at(&next->spot, next->name, MARK_NAME_SIZE) : 0;
	if (next->timer_plus_one)
		next->name[0] = 0;
	*own = *next;
	if (first)
		mark_sound(next->kind);
	platform_log("marks: view %d: kind %d entry %d at %.2f %.2f %.2f team %ld", local_player_index, next->kind, entry, next->spot.x,
		next->spot.y, next->spot.z, (long)player->team_index);
	/* (a tap, a double tap and the wheel's calls: the bots of the team answer on the host) */
	if (game_connection() != _game_connection_network_client && (entry != AE_WHEEL_RIDE || !ae_bots_marks_ride(player)))
		ae_bots_marks_heard(player, own->kind, own->object_index, &own->spot);
	if (mark_network())
		ae_mark_calls_share(absolute, verb, own);
	ae_marks_feed(absolute, own);
	return TRUE;
}

/* the local player's datum, or NULL when they have none (not yet in the game, or gone) */
static struct player_datum *mark_local_player(short local_player_index, long *player_index)
{
	*player_index = local_player_get_player_index(local_player_index);
	return *player_index == NONE ? NULL : player_try_and_get(*player_index);
}

static void mark_press(short local_player_index)
{
	long player_index;
	struct player_datum *player = mark_local_player(local_player_index, &player_index);
	struct ae_mark_aim aim;

	if (!player || player->unit_index == NONE || !ae_marks_rule_on())
		return;
	/* (always a fresh look: the cached aim is up to 3 ticks old) */
	mark_pick(local_player_index, &aim, TRUE);
	aims[local_player_index].cast_at = NONE;
	if (!aim.found)
	{
		platform_log("marks: view %d: nothing in range", local_player_index);
		return;
	}
	if (aim.verb != AE_CALL_MARK)
	{
		platform_log("marks: view %d: tier %d: answer %d on player %d's mark", local_player_index, (int)aim.tier, (int)aim.verb,
			(int)aim.reply_to);
		ae_marks_answer(local_player_index, aim.verb, aim.reply_to);
		return;
	}
	mark_commit(local_player_index, player_index, player, &aim.mark, TRUE, AE_CALL_MARK, AE_WHEEL_SMART);
}

/* what the aim is on, as an enemy: the enemy there, else the point it meets, marked as one */
static boolean mark_cast_enemy(short local_player_index, struct player_datum const *player, struct mark *next)
{
	csmemset(next, 0, sizeof(*next));
	if (!mark_cast(local_player_index, player, next, FALSE))
		return FALSE;
	if (next->kind != AE_MARK_KIND_ENEMY)
	{
		struct mark spot;

		csmemset(&spot, 0, sizeof(spot));
		if (!mark_cast(local_player_index, player, &spot, TRUE))
			return FALSE;
		csmemset(next, 0, sizeof(*next));
		mark_set_enemy_point(next, &spot.spot);
	}
	return TRUE;
}

/* a double tap: the tap before it has marked; the mark is replaced at the aim point as an enemy. No second count for
the guard and no second tone: it is the same call, made again */
static void mark_double(short local_player_index)
{
	long player_index;
	struct player_datum *player = mark_local_player(local_player_index, &player_index);
	struct mark next;

	if (!player || player->unit_index == NONE || !ae_marks_rule_on())
		return;
	if (!mark_cast_enemy(local_player_index, player, &next))
	{
		platform_log("marks: view %d: double: nothing in range", local_player_index);
		return;
	}
	mark_commit(local_player_index, player_index, player, &next, FALSE, AE_CALL_MARK, AE_WHEEL_ENEMY);
}

/* where the player's body lies, or FALSE when it is not to be found */
boolean ae_marks_death_spot(struct player_datum const *player, real_point3d *spot)
{
	if (player->dead_unit_index == NONE || object_try_and_get_and_verify_type(player->dead_unit_index, _object_mask_unit) == NULL)
		return FALSE;
	object_get_origin(player->dead_unit_index, spot);
	return TRUE;
}

/* a call of the wheel: the smart call marks what the aim is on, the enemy call an enemy (the point the aim meets when none
stands there), the place calls the spot it meets, the calls that ask for something your feet. Dead, the calls the wheel shows
are placed where the player died (the death camera's aim could see through the world). The host makes the same of it, and
sets the spot itself for a dead player and for the feet */
void ae_marks_wheel_send(short local_player_index, int entry)
{
	long player_index;
	struct player_datum *player = mark_local_player(local_player_index, &player_index);
	struct mark next;
	real_point3d spot;
	boolean dead;

	if (!player || !VALID_INDEX(local_player_index, MAXIMUM_LOCAL_PLAYERS) || !game_engine_running() || !ae_marks_rule_on())
		return;
	dead = player->unit_index == NONE;
	if (!ae_wheel_entry_shown(entry, dead))
		return;
	csmemset(&next, 0, sizeof(next));
	if (dead)
	{
		if (!ae_marks_death_spot(player, &spot))
		{
			platform_log("marks: view %d: call %d: no body to place it at", local_player_index, entry);
			return;
		}
		if (entry == AE_WHEEL_ENEMY)
			mark_set_enemy_point(&next, &spot);
		else
			mark_set_place(&next, &spot);
	}
	else if (entry == AE_WHEEL_WEAPON || entry == AE_WHEEL_RIDE)
	{
		/* (asking for something to come to you: at your feet) */
		object_get_origin(object_get_ultimate_parent(player->unit_index), &spot);
		mark_set_place(&next, &spot);
	}
	else if (entry == AE_WHEEL_ENEMY)
	{
		if (!mark_cast_enemy(local_player_index, player, &next))
		{
			platform_log("marks: view %d: call %d: nothing in range", local_player_index, entry);
			return;
		}
	}
	else if (!mark_cast(local_player_index, player, &next, entry != AE_WHEEL_SMART))
	{
		platform_log("marks: view %d: call %d: nothing in range", local_player_index, entry);
		return;
	}
	platform_log("marks: view %d: call %d sent", local_player_index, entry);
	mark_commit(local_player_index, player_index, player, &next, TRUE, AE_CALL_WHEEL, entry);
}

/* ---------- the markers drawn */

boolean hud_item_timers_ae_stock_sprite(struct item_timer const *timer, struct bitmap_data const **bitmap, real_rectangle2d *clip);
/* (game_engine_ctf.c) 1 when a position is at a flag's stand, 0 when not, -1 when the game has no stands */
long game_engine_ctf_flag_at_stand(real_point3d const *position);

/* the item a mark of an item is of, here: the host's object, else the one nearest its spot (a client has no object index) */
static long mark_item_object(struct mark const *mark, struct player_datum const *viewer)
{
	if (mark->object_index != NONE &&
		object_try_and_get_and_verify_type(mark->object_index, _object_mask_weapon | _object_mask_equipment | _object_mask_vehicle))
	{
		return mark->object_index;
	}
	return ae_marks_find(viewer, AE_MARK_KIND_ITEM, &mark->spot);
}

/* whether a vehicle has a gunner's seat the viewer could take now */
static boolean mark_gunner_open(long vehicle_index, struct player_datum const *viewer)
{
	struct unit_definition const *definition;
	short seat;

	if (viewer->unit_index == NONE || !object_try_and_get_and_verify_type(vehicle_index, _object_mask_unit))
		return FALSE;
	definition = unit_definition_get(object_get(vehicle_index)->definition_index);
	for (seat = 0; seat < definition->unit.seats.count; seat++)
	{
		if (unit_seat_is_gunner(vehicle_index, seat) && unit_can_enter_seat(viewer->unit_index, vehicle_index, seat, NULL))
			return TRUE;
	}
	return FALSE;
}

/* a mark's fact, in capitals: a power item's spawn timer (with the item's picture), a flag dropped or at home (by where it lies
against its stand and whether anyone carries it, never by the game's handled bit), a vehicle's open gunner seat. None for the rest */
static void mark_fact(struct mark const *mark, struct player_datum const *viewer, struct ae_marker *marker, boolean *flag)
{
	marker->fact[0] = 0;
	*flag = FALSE;
	if (mark->kind == AE_MARK_KIND_PLACE && mark->word == MARK_WORD_HERE)
	{
		struct item_timer const *timer = mark_timer_of(mark);

		if (timer)
		{
			wchar_t tail[12];
			long seconds = (item_timer_ticks_left(timer) + TICKS_PER_SECOND - 1) / TICKS_PER_SECOND;
			real_rectangle2d clip;

			if (seconds > 5999)
				seconds = 5999;
			ustrncpy_terminated(marker->fact, timer->label, (unsigned long)(NUMBEROF(marker->fact) - 8));
			usnprintf(tail, NUMBEROF(tail), L" %d:%d%d", (int)(seconds / 60), (int)(seconds % 60 / 10), (int)(seconds % 10));
			ustrncpy_terminated(marker->fact + ustrlen(marker->fact), tail, (unsigned long)(NUMBEROF(marker->fact) - ustrlen(marker->fact)));
			if (hud_item_timers_ae_stock_sprite(timer, &marker->fact_sprite, &clip))
			{
				marker->fact_clip[0] = clip.x0;
				marker->fact_clip[1] = clip.y0;
				marker->fact_clip[2] = clip.x1;
				marker->fact_clip[3] = clip.y1;
			}
			else
				marker->fact_sprite = NULL;
		}
	}
	else if (mark->kind == AE_MARK_KIND_ITEM)
	{
		long object_index = mark_item_object(mark, viewer);
		struct object_datum *object = object_index == NONE ? NULL : object_get(object_index);

		if (!object)
			return;
		if (TEST_FLAG(_object_mask_vehicle, object->object.type))
		{
			if (mark_gunner_open(object_index, viewer))
				ustrncpy_terminated(marker->fact, L"GUNNER OPEN", NUMBEROF(marker->fact));
		}
		else if (TEST_FLAG(_object_mask_weapon, object->object.type) && weapon_is_flag(object_index))
		{
			*flag = TRUE;
			if (object->object.parent_object_index == NONE)
			{
				long at_stand = game_engine_ctf_flag_at_stand(&object->object.position);

				if (at_stand >= 0)
					ustrncpy_terminated(marker->fact, at_stand ? L"FLAG AT HOME" : L"FLAG DROPPED", NUMBEROF(marker->fact));
			}
		}
	}
}

/* the glyph a mark is drawn with, and whether it is the rifle's picture, an enemy's red, a pin on its tip */
static void mark_look(struct mark const *mark, boolean caller_dead, boolean flag, struct ae_marker *marker)
{
	static short const kind_glyphs[AE_MARK_KIND_COUNT] =
		{ AE_WAYPOINT_GLYPH_MARK_ENEMY, AE_WAYPOINT_GLYPH_MARK_ITEM, AE_WAYPOINT_GLYPH_MARK_PLACE };
	short entry = mark->entry < 0 ? AE_WHEEL_SMART : mark->entry;

	marker->glyph = kind_glyphs[mark->kind];
	switch (entry)
	{
	case AE_WHEEL_ATTACK: marker->glyph = AE_WAYPOINT_GLYPH_CALL_ATTACK; break;
	case AE_WHEEL_DEFEND: marker->glyph = AE_WAYPOINT_GLYPH_CALL_DEFEND; break;
	case AE_WHEEL_WATCH: marker->glyph = AE_WAYPOINT_GLYPH_CALL_WATCH; break;
	case AE_WHEEL_RIDE: marker->glyph = AE_WAYPOINT_GLYPH_CALL_RIDE; break;
	case AE_WHEEL_WEAPON:
		/* (the map's rifle picture; the item's glyph where the map has none) */
		marker->rifle = TRUE;
		marker->glyph = AE_WAYPOINT_GLYPH_MARK_ITEM;
		break;
	case AE_WHEEL_SMART:
		if (caller_dead && mark->kind == AE_MARK_KIND_PLACE)
			marker->glyph = AE_WAYPOINT_GLYPH_CALL_HELP;
		else if (flag && mark->kind == AE_MARK_KIND_ITEM)
			marker->glyph = AE_WAYPOINT_GLYPH_CALL_FLAG;
		break;
	}
	marker->enemy = (boolean)(mark->kind == AE_MARK_KIND_ENEMY);
	marker->pin = (boolean)(marker->glyph == AE_WAYPOINT_GLYPH_MARK_PLACE);
}

/* the name row's word: what the call is, else what the mark is on */
static void mark_row_word(struct mark const *mark, boolean caller_dead, struct ae_marker *marker)
{
	wchar_t const *word;

	switch (mark->entry)
	{
	case AE_WHEEL_ATTACK: word = L"ATTACK"; break;
	case AE_WHEEL_DEFEND: word = L"DEFEND"; break;
	case AE_WHEEL_WATCH: word = L"WATCH"; break;
	case AE_WHEEL_WEAPON: word = L"NEED WEAPON"; break;
	case AE_WHEEL_RIDE: word = L"NEED A RIDE"; break;
	default:
		if (caller_dead && mark->kind == AE_MARK_KIND_PLACE && mark->entry == AE_WHEEL_SMART)
			word = L"HELP";
		else if (mark->name[0])
			word = mark->name;
		else
			word = mark->word >= 0 && mark->word < (short)NUMBEROF(mark_words) ? mark_words[mark->word] : L"HERE";
		break;
	}
	ustrncpy_terminated(marker->name, word, NUMBEROF(marker->name));
	/* (a timer's fact names the item: the row is its distance alone) */
	if (marker->fact[0] && mark->kind == AE_MARK_KIND_PLACE && mark->word == MARK_WORD_HERE)
		marker->name[0] = 0;
}

/* the answers to a mark: the names of those who said OK and are not muted here, and how many */
static void mark_answers(struct mark const *mark, long viewer_absolute, struct ae_marker *marker)
{
	short bit;

	marker->responders = mark->responders;
	marker->ok_count = 0;
	for (bit = 0; bit < MARK_PLAYERS; bit++)
	{
		struct player_datum const *replier;

		if (!((mark->responders >> bit) & 1))
			continue;
		replier = distributed_player(bit);
		if (!replier || replier->quit_out_of_game || (bit != viewer_absolute && mark_player_muted_here(replier)))
			continue;
		if (marker->ok_count < AE_MARKER_OK_NAMES)
			ustrncpy_terminated(marker->ok_names[marker->ok_count], replier->name, NUMBEROF(marker->ok_names[0]));
		marker->ok_count++;
	}
}

/* an item taken or gone takes its mark with it: here, and from the host for everyone's. A mark is over at its end, an answer
extending it (a dedicated server draws none, so its marks end here) */
static void marks_tick(void)
{
	short index;

	for (index = 0; index < MARK_PLAYERS; index++)
	{
		struct mark *mark = &marks[index];
		struct object_datum *object;

		if (!mark->active || mark->kind != AE_MARK_KIND_ITEM || mark->object_index == NONE)
			continue;
		object = object_try_and_get_and_verify_type(mark->object_index,
			_object_mask_weapon | _object_mask_equipment | _object_mask_vehicle);
		if (object && object->object.parent_object_index == NONE)
			continue;
		mark->active = FALSE;
		if (game_connection() == _game_connection_network_server)
		{
			struct distributed_mark entry;

			mark_entry(&entry, index, MARK_CLEAR, 0, NULL);
			mark_host_send(&entry, mark->team_index);
		}
	}
	ae_mark_calls_tick();
}

void ae_marks_left_game(void)
{
	/* (now, not at the next game's first tick: a new host's first messages come before that) */
	marks_game = NONE;
	ae_mark_calls_reset();
}

/* a new game starts with none */
static void marks_new_game(void)
{
	long game = game_time_get();

	/* (and everything counted in the last game's ticks: the guards, a press held across) */
	if (marks_game == NONE || game < marks_game)
	{
		short index;

		csmemset(marks, 0, sizeof(marks));
		for (index = 0; index < MARK_PLAYERS; index++)
			ae_marks_clear_slot(&marks[index]);
		csmemset(pad_spam, 0, sizeof(pad_spam));
		csmemset(host_spam, 0, sizeof(host_spam));
		csmemset(host_counted, 0, sizeof(host_counted));
		csmemset(feed_merges, 0, sizeof(feed_merges));
		ae_mark_calls_reset();
		csmemset(pad_press, 0, sizeof(pad_press));
		csmemset(aims, 0, sizeof(aims));
		for (index = 0; index < MAXIMUM_LOCAL_PLAYERS; index++)
			aims[index].cast_at = NONE;
		ae_wheel_reset();
	}
	marks_game = game;
}

/* ---------- public code */

/* whether the wheel cannot be open for this player now: their menu, the scoreboard, a cinematic or a held-back input
(each of which empties the input they play with) */
static boolean mark_wheel_interrupted(short local_player_index, struct player_datum const *player)
{
	return !ae_marks_rule_on() || ae_pause_player_open(local_player_index) || game_engine_scoreboard_held(local_player_index) ||
		cinematic_in_progress() || director_inhibited_input(local_player_index) ||
		(player && VALID_INDEX(player->local_player_index, MAXIMUM_LOCAL_PLAYERS) &&
		ui_widgets_active_for_local_player(player->local_player_index));
}

void ae_marks_input(short local_player_index, unsigned long player_control_flags, unsigned long *unit_control_flags,
	int light_bit)
{
	struct ae_mark_press *press;
	struct player_datum *player;
	long player_index;
	int down, light = 1;
	int pad_owned, keyboard, key_down = 0;
	int entry, result;
	long now;

	if (!VALID_INDEX(local_player_index, MAXIMUM_LOCAL_PLAYERS) || !game_engine_running())
		return;
	marks_new_game();
	marks_tick();
	/* (what a press would take, kept up to date for the prompt) */
	ae_marks_aim(local_player_index);
	now = game_time_get();
	press = &pad_press[local_player_index];
	player = mark_local_player(local_player_index, &player_index);
	pad_owned = ae_wheel_pad_owns(local_player_index);
	/* (the keyboard's controller is not always the local player of the same number: the seats, the players' controllers) */
	keyboard = local_player_index == ae_key_prompts_keyboard_player();
	if (keyboard)
		key_down = (halo_keyboard_actions((short)ae_seats_keyboard_controller()) & (1ULL << HALO_KEYBOARD_MARK)) != 0;
	if (pad_owned)
	{
		/* (the left bumper is the mark's and the wheel's: the flashlight button no longer marks) */
		csmemset(press, 0, sizeof(*press));
		down = (int)((player_control_flags >> AE_MARK_PAD_BIT) & 1);
	}
	else
	{
		down = (int)((player_control_flags >> AE_ZOOM_FLASH_BIT) & 1);
		if (down != press->down)
			platform_log("marks: view %d: button %s at tick %ld (zoom has it %d)", local_player_index, down ? "down" : "up", now,
				(int)((player_control_flags >> AE_ZOOM_HELD_BIT) & 1));
		if (ae_mark_press_step(press, down, (int)((player_control_flags >> AE_ZOOM_HELD_BIT) & 1), now, &light))
			mark_press(local_player_index);
		if (!light)
			*unit_control_flags &= ~(1ul << light_bit);
		down = 0;
	}
	/* (every player's tick, so a wheel left open by a layout changed under it is let go of) */
	result = ae_wheel_step(local_player_index, down, key_down, keyboard, mark_wheel_interrupted(local_player_index, player),
		player == NULL || player->unit_index == NONE, now, &entry);
	switch (result)
	{
	case AE_PRESS_TAP:
		mark_press(local_player_index);
		break;
	case AE_PRESS_DOUBLE:
		mark_double(local_player_index);
		break;
	case AE_PRESS_OPEN:
		platform_log("marks: view %d: wheel opened at tick %ld", local_player_index, now);
		break;
	case AE_PRESS_SEND:
		ae_marks_wheel_send(local_player_index, entry);
		break;
	case AE_PRESS_CANCEL:
		platform_log("marks: view %d: wheel closed with nothing sent at tick %ld", local_player_index, now);
		break;
	}
}

void ae_marks_update(void)
{
	/* (a dedicated server has no local player, so ae_marks_input never runs there: the host's ticks come from here) */
	if (!game_engine_running())
		return;
	marks_new_game();
	marks_tick();
}

/* through walls off: whether this view could see a teammate's enemy mark: the world only between the camera and the spot, the
enemy standing there not in the way. Each mark has its own ray, each AE_MARK_SIGHT_TEST_TICKS */
static boolean mark_seen_here(short local_player_index, short index, struct mark const *mark, real_point3d const *eye, long now)
{
	static struct
	{
		long born;
		long tested_at;
		long seen_at;
	} sights[MAXIMUM_LOCAL_PLAYERS][MARK_PLAYERS];
	short local = (short)PIN(local_player_index, 0, MAXIMUM_LOCAL_PLAYERS - 1);
	long born = mark->born;

	if (sights[local][index].born != born || sights[local][index].tested_at == 0)
	{
		sights[local][index].born = born;
		sights[local][index].tested_at = NONE;
		sights[local][index].seen_at = NONE;
	}
	if (ae_mark_sight_due(sights[local][index].tested_at, now))
	{
		struct collision_result collision;
		real_vector3d ray;

		ray.i = mark->spot.x - eye->x;
		ray.j = mark->spot.y - eye->y;
		ray.k = mark->spot.z - eye->z;
		sights[local][index].tested_at = now;
		/* (the structure alone: with the objects bit the ray would meet the enemy standing there, and the viewer's own body) */
		if (!collision_test_vector(FLAG(_collision_test_structure_bit), eye, &ray, NONE, &collision))
			sights[local][index].seen_at = now;
	}
	return ae_mark_sight_held(sights[local][index].seen_at, now) != 0;
}

void ae_marks_draw(short local_player_index)
{
	long player_index;
	struct player_datum const *viewer;
	long now;
	short index;
	struct ae_mark_aim const *aim;
	wchar_t action[AE_MARKER_NAME_SIZE];
	boolean aiming;

	if (local_player_index == NONE || !ae_marks_rule_on())
		return;
	if (game_engine_scoreboard_shown(local_player_index) > 0.0f || game_engine_scoreboard_held(local_player_index))
		return;
	player_index = local_player_get_player_index(local_player_index);
	if (player_index == NONE)
		return;
	viewer = player_get(player_index);
	now = game_time_get();
	/* (the aim prompt's word, for the marker the player aims at: its block takes the prompt's row) */
	aim = ae_marks_aim(local_player_index);
	aiming = (boolean)(aim && aim->found && aim->tier == AE_TIER_MARKER && aim->reply_to >= 0 &&
		ae_mark_prompt_row(local_player_index, action, NUMBEROF(action)));
	for (index = 0; index < MARK_PLAYERS; index++)
	{
		struct mark *mark = &marks[index];
		struct ae_marker marker;
		struct player_datum const *caller;
		long age;
		real_point3d view_position, spot;
		real_point2d screen_position;
		real view_width, view_height, x, y;
		boolean off_screen, own, caller_dead, flag;

		if (!mark->active)
			continue;
		age = ae_mark_effective_age(now - mark->born, mark->ends ? mark->ends - mark->born : AE_MARK_LIFE_TICKS);
		if (ae_mark_alpha(age) <= 0.0f && age >= 0)
		{
			mark->active = FALSE;
			continue;
		}
		own = (boolean)(index == DATUM_INDEX_TO_ABSOLUTE_INDEX(player_index));
		if (!own && game_engine_has_teams() && mark->team_index != viewer->team_index)
			continue;
		if (!own && !game_engine_has_teams())
			continue;
		caller = distributed_player(index);
		/* (a muted machine's marks are kept for the team, and not drawn on this one) */
		if (!own && caller && mark_player_muted_here(caller))
			continue;
		caller_dead = (boolean)(caller && caller->unit_index == NONE);
		/* (a call that asks for something to come is on the caller, where they stand now) */
		spot = mark->spot;
		if ((mark->entry == AE_WHEEL_WEAPON || mark->entry == AE_WHEEL_RIDE) && !own && caller && !caller_dead)
		{
			unit_get_head_position(caller->unit_index, &spot);
			spot.z += MARK_ABOVE;
		}
		/* (the tone has played: only the marker's own line let the mark be made) */
		if (!own && mark->kind == AE_MARK_KIND_ENEMY && !ae_marks_rule_through_walls() &&
			!mark_seen_here(local_player_index, index, mark, &render.camera.position, now))
		{
			continue;
		}
		matrix4x3_transform_point(&render.frustum.world_to_view, &spot, &view_position);
		view_width = (real)(render.camera.viewport_bounds.x1 - render.camera.viewport_bounds.x0);
		view_height = (real)(render.camera.viewport_bounds.y1 - render.camera.viewport_bounds.y0);
		off_screen = !render_camera_view_to_screen(&render.camera, &render.frustum, &view_position, &screen_position);
		if (off_screen)
		{
			x = view_position.x;
			y = -view_position.y;
		}
		else
		{
			x = screen_position.x - (real)render.camera.viewport_bounds.x0 - view_width * 0.5f;
			y = screen_position.y - (real)render.camera.viewport_bounds.y0 - view_height * 0.5f;
		}
		memset(&marker, 0, sizeof(marker));
		marker.clamped = (boolean)(ae_mark_clamp(&x, &y, view_width * MARK_ELLIPSE_X, view_height * MARK_ELLIPSE_Y, off_screen) != 0);
		marker.slot = index;
		marker.born = mark->born;
		marker.x = x + view_width * 0.5f;
		marker.y = y + view_height * 0.5f;
		marker.distance = distance3d(&render.camera.position, &spot);
		marker.alpha = ae_mark_alpha(age);
		marker.appear = ae_mark_appear_scale(age);
		/* (the callout wheel is drawn over the markers: one behind a sector fades) */
		marker.fade = ae_wheel_covers(local_player_index, marker.x, marker.y) ? MARK_WHEEL_FADE : 1.0f;
		/* (a number off the network: held to what there is) */
		marker.repeats = mark->repeats < 0 ? 0 : mark->repeats > 2 ? 2 : mark->repeats;
		mark_fact(mark, viewer, &marker, &flag);
		mark_look(mark, caller_dead, flag, &marker);
		mark_row_word(mark, caller_dead, &marker);
		mark_answers(mark, DATUM_INDEX_TO_ABSOLUTE_INDEX(player_index), &marker);
		/* (the owner's name, under the rows: a teammate's mark; once the viewer said OK, that) */
		if (!own && caller)
		{
			if ((mark->responders >> DATUM_INDEX_TO_ABSOLUTE_INDEX(player_index)) & 1)
				ustrncpy_terminated(marker.owner, L"You said OK", NUMBEROF(marker.owner));
			else
				ustrncpy_terminated(marker.owner, caller->name, NUMBEROF(marker.owner));
		}
		if (aiming && aim->reply_to == index)
			ustrncpy_terminated(marker.action, action, NUMBEROF(marker.action));
		ae_marker_draw(local_player_index, &marker);
	}
}
unsigned short ae_marks_entry_size(void)
{
	return (unsigned short)sizeof(struct distributed_mark);
}

/* (the host) what a client says it marked exists here: the nearest such object to the spot, or NONE */
long ae_marks_find(struct player_datum const *player, short kind, real_point3d const *spot)
{
	struct object_iterator iterator;
	struct object_datum *object;
	unsigned long types = kind == AE_MARK_KIND_ENEMY ? _object_mask_biped | _object_mask_vehicle :
		_object_mask_weapon | _object_mask_equipment | _object_mask_vehicle;
	real best = kind == AE_MARK_KIND_ENEMY ? MARK_HOST_ENEMY_NEAR * MARK_HOST_ENEMY_NEAR : MARK_HOST_ITEM_NEAR * MARK_HOST_ITEM_NEAR;
	long found = NONE;

	object_iterator_new(&iterator, types, 0);
	while ((object = object_iterator_next(&iterator)) != NULL)
	{
		real distance = distance_squared3d(&object->object.bounding_sphere_center, spot);
		boolean vehicle = (boolean)TEST_FLAG(_object_mask_vehicle, object->object.type);

		/* (an enemy's vehicle: the mark stands over its roof) */
		if (kind == AE_MARK_KIND_ENEMY && vehicle)
		{
			real past = sqrtf(distance) - object->object.bounding_sphere_radius - MARK_ABOVE;

			distance = past > 0.0f ? past * past : 0.0f;
		}
		if (distance >= best || object->object.parent_object_index != NONE)
			continue;
		if (kind == AE_MARK_KIND_ENEMY && (vehicle ? !mark_vehicle_has_enemy(player, object) :
			TEST_FLAG(object->object.damage_flags, _object_dead_bit) || !mark_is_enemy(player, object)))
		{
			continue;
		}
		best = distance;
		found = iterator.index;
	}
	return found;
}

/* (the host) whether the marker's head reaches the spot. Range is tested first: no line is cast to a spot out of it. An enemy
needs a line: to the one found, or else to the point (the last half unit short of it, where a wall the aim met begins) */
boolean ae_marks_host_reach(struct player_datum const *player, short kind, real_point3d const *spot, long found)
{
	real_point3d head;
	real distance;
	boolean line_clear = TRUE;
	long own = object_get_ultimate_parent(player->unit_index);

	unit_get_head_position(player->unit_index, &head);
	distance = distance_squared3d(&head, spot);
	if (!ae_mark_host_allows(kind, distance, TRUE))
		return FALSE;
	if (ae_mark_host_line_wanted(kind, distance))
	{
		real_vector3d across;
		real length;

		line_clear = found != NONE && mark_object_in_view(&head, object_get(found), own, found);
		if (!line_clear)
		{
			real_point3d short_of;

			/* (a spot at the head itself has nothing between) */
			length = sqrtf(distance);
			line_clear = length <= MARK_HOST_POINT_SHORT;
			if (!line_clear)
			{
				across.i = spot->x - head.x;
				across.j = spot->y - head.y;
				across.k = spot->z - head.z;
				short_of.x = spot->x - across.i / length * MARK_HOST_POINT_SHORT;
				short_of.y = spot->y - across.j / length * MARK_HOST_POINT_SHORT;
				short_of.z = spot->z - across.k / length * MARK_HOST_POINT_SHORT;
				line_clear = mark_clear_to(&head, &short_of, own, NONE);
			}
		}
	}
	return ae_mark_host_allows(kind, distance, line_clear) != 0;
}

/* (the host) the guard's word on a mark: only the limit stops it. A double tap rides the tap before it, once */
boolean ae_marks_host_guard(long absolute, boolean double_tap)
{
	long now = game_time_get();
	int said;

	if (!VALID_INDEX(absolute, MARK_PLAYERS))
		return FALSE;
	if (double_tap && ae_mark_double_free(host_counted[absolute], now))
	{
		host_counted[absolute] = 0;
		return TRUE;
	}
	said = ae_mark_spam_take(&host_spam[absolute], now);
	if (said == AE_MARK_SPAM_NOW_BLOCKED || said == AE_MARK_SPAM_STILL_BLOCKED)
		return FALSE;
	host_counted[absolute] = now + 1;
	return TRUE;
}

struct mark *ae_marks_slot(long absolute)
{
	return &marks[absolute % MARK_PLAYERS];
}

void ae_marks_clear_slot(struct mark *mark)
{
	csmemset(mark, 0, sizeof(*mark));
	mark->dibs = -1;
	mark->entry = NONE;
}

void ae_marks_item_name(struct mark *mark, long object_index)
{
	mark_item_name(mark, object_index);
}

void ae_marks_tone(short kind)
{
	mark_sound(kind);
}

boolean ae_marks_heard_here(long team_index)
{
	return mark_heard_here(team_index);
}

boolean ae_marks_muted_here(struct player_datum const *player)
{
	return mark_player_muted_here(player);
}

int ae_marks_get_detail(long mark_player, struct ae_mark_detail *detail)
{
	struct mark const *mark;

	if (mark_player < 0 || mark_player >= MARK_PLAYERS || !marks[mark_player].active)
		return 0;
	mark = &marks[mark_player];
	detail->kind = mark->kind;
	detail->entry = mark->entry < 0 ? AE_WHEEL_SMART : mark->entry;
	detail->repeats = mark->repeats;
	detail->timer_plus_one = mark->timer_plus_one;
	detail->place = mark->place;
	detail->dibs = mark->dibs;
	detail->responders = mark->responders;
	detail->team_index = mark->team_index;
	detail->born = mark->born;
	detail->muted_here = distributed_player((short)mark_player) && mark_player_muted_here(distributed_player((short)mark_player));
	detail->ends = mark->ends ? mark->ends : mark->born + AE_MARK_LIFE_TICKS;
	detail->x = mark->spot.x;
	detail->y = mark->spot.y;
	detail->z = mark->spot.z;
	return 1;
}

void ae_marks_handle_up(long machine_index, void const *entries, int from_stream)
{
	struct distributed_mark entry;
	struct player_datum *player;
	struct mark *mark;
	struct mark made;
	real_point3d spot;
	long found = NONE;

	if (machine_index == NONE || !from_stream || !game_engine_running() || !ae_marks_rule_on())
		return;
	csmemcpy(&entry, entries, sizeof(entry));
	if (entry.player_index >= MARK_PLAYERS || !distributed_machine_has_player(machine_index, entry.player_index) ||
		(player = distributed_player(entry.player_index)) == NULL || player->unit_index == NONE)
	{
		return;
	}
	mark = &marks[entry.player_index];
	if (entry.kind == MARK_CLEAR)
	{
		/* (a clear of no mark is dropped: a client sending them over and over is not passed on to the team) */
		if (!mark->active)
			return;
		mark->active = FALSE;
		mark_host_send(&entry, player->team_index);
		return;
	}
	if (network_chat_host_name_muted(player->name))
	{
		platform_log("marks: host: player %d is muted", (int)entry.player_index);
		return;
	}
	spot.x = entry.x;
	spot.y = entry.y;
	spot.z = entry.z;
	/* (a spot in the world: no NaN, no infinity, inside the world's bound) */
	if (entry.kind >= AE_MARK_KIND_COUNT || entry.word >= MARK_WORD_COUNT || !distributed_point_valid(&spot, UNIT_WORLD_BOUND))
		return;
	if (entry.kind != AE_MARK_KIND_PLACE && (found = ae_marks_find(player, entry.kind, &spot)) == NONE)
	{
		platform_log("marks: host: player %d's kind %d has nothing near its spot", (int)entry.player_index, (int)entry.kind);
		return;
	}
	if (!ae_marks_host_reach(player, entry.kind, &spot, found))
	{
		platform_log("marks: host: player %d's kind %d is out of range or sight", (int)entry.player_index, (int)entry.kind);
		return;
	}
	if (!ae_marks_host_guard(entry.player_index, FALSE))
		return;
	ae_marks_clear_slot(&made);
	made.active = TRUE;
	made.kind = entry.kind;
	made.word = entry.word;
	made.object_index = entry.kind == AE_MARK_KIND_ITEM ? found : NONE;
	made.team_index = player->team_index;
	made.born = game_time_get();
	made.ends = made.born + AE_MARK_LIFE_TICKS;
	made.entry = AE_WHEEL_SMART;
	made.serial = (short)ae_mark_serial_next(mark->serial);
	made.spot = spot;
	if (entry.kind == AE_MARK_KIND_ITEM)
		mark_item_name(&made, found);
	made.timer_plus_one = made.kind == AE_MARK_KIND_PLACE && made.word == MARK_WORD_HERE ? ae_marks_timer_for(&spot) : 0;
	/* (muted on this machine: the mark is kept, as the team's state, and only its tone and its drawing are left out here) */
	*mark = made;
	platform_log("marks: from player %d kind %d team %ld, heard here %d", (int)entry.player_index, (int)entry.kind,
		(long)player->team_index, (int)mark_heard_here(player->team_index));
	if (!mark_player_muted_here(player) && mark_heard_here(player->team_index))
		mark_sound(mark->kind);
	mark_host_send(&entry, player->team_index);
	ae_mark_calls_send_detail(entry.player_index, mark);
	ae_marks_feed(entry.player_index, mark);
	ae_bots_marks_heard(player, entry.kind, found, &spot);
}

void ae_marks_handle_down(void const *entries, int from_stream)
{
	struct distributed_mark entry;
	struct player_datum *player;
	struct mark *mark;
	boolean own = FALSE;
	short local;

	if (!from_stream || !game_engine_running())
		return;
	csmemcpy(&entry, entries, sizeof(entry));
	if (entry.player_index >= MARK_PLAYERS || (player = distributed_player(entry.player_index)) == NULL)
		return;
	mark = &marks[entry.player_index];
	if (entry.kind == MARK_CLEAR)
	{
		mark->active = FALSE;
		return;
	}
	if (entry.kind >= AE_MARK_KIND_COUNT || entry.word >= MARK_WORD_COUNT || !ae_mark_spot_finite(entry.x, entry.y, entry.z))
		return;
	/* (a host that sends the detail on 101 sends this same mark there: it is made from that one) */
	if (ae_mark_calls_detail_seen())
		return;
	{
		real_point3d spot;

		spot.x = entry.x;
		spot.y = entry.y;
		spot.z = entry.z;
		if (!distributed_point_valid(&spot, UNIT_WORLD_BOUND))
			return;
	}
	/* (the marker's own machine has it from the press, and heard it then) */
	for (local = local_player_get_next(NONE); local != NONE; local = local_player_get_next(local))
		own = own || local_player_get_player_index(local) != NONE &&
			DATUM_INDEX_TO_ABSOLUTE_INDEX(local_player_get_player_index(local)) == entry.player_index;
	if (own)
		return;
	ae_marks_clear_slot(mark);
	mark->active = TRUE;
	mark->kind = entry.kind;
	mark->word = entry.word;
	mark->object_index = NONE;
	mark->team_index = player->team_index;
	mark->born = game_time_get();
	mark->ends = mark->born + AE_MARK_LIFE_TICKS;
	mark->spot.x = entry.x;
	mark->spot.y = entry.y;
	mark->spot.z = entry.z;
	if (entry.kind == AE_MARK_KIND_ITEM)
	{
		/* (the name is not sent: this machine has the item too) */
		long found = ae_marks_find(player, AE_MARK_KIND_ITEM, &mark->spot);

		if (found != NONE)
			mark_item_name(mark, found);
	}
	platform_log("marks: from player %d kind %d team %ld, heard here %d", (int)entry.player_index, (int)entry.kind,
		(long)player->team_index, (int)mark_heard_here(player->team_index));
	if (!mark_player_muted_here(player) && mark_heard_here(player->team_index))
		mark_sound(mark->kind);
}
