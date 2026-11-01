/*
AE_MARKS.C

MARKS (controls.mark; a controller's tap of the flashlight button, whose hold
stays the flashlight's): a press marks what is aimed at, for 5 seconds, seen
through walls, with one sound. One mark a player: a new one replaces the old,
and a press on your own mark clears it. Three kinds: an enemy (where they were:
it does not follow), an item (ends if it is picked up or goes) and a place.
Teammates and your own vehicle cannot be marked: the aim passes through them.

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
#include "camera/observer.h"
#include "game/game.h"
#include "game/game_allegiance.h"
#include "game/game_engine.h"
#include "game/players.h"
#include "objects/objects.h"
#include "objects/object_types.h"
#include "physics/collisions.h"
#include "tag_files/tag_files.h"
#include "units/units.h"
#include "interface/hud.h"
#include "interface/hud_messaging.h"
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
#include "ae_mark_rules.h"
#include "ae_marks.h"
#include "ae_seats.h"
#include "ae_waypoint_icons.h"
#include "ae_waypoint_ink.h"
#include "ae_zoom.h"
#include "network_distributed.h"
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

#define MARK_PLAYERS 16
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
#define MARK_ABOVE 0.3f
#define MARK_ITEM_ABOVE 0.15f
/* how many allies, and pieces of your own vehicle, the aim can pass through */
#define MARK_THROUGH_STEPS 6

/* the glyph's height, in the text scale's units, and its smallest (about 20 pixels at 1080p) */
#define MARK_GLYPH_SIZE 36.0f
#define MARK_GLYPH_SMALLEST 9.0f
#define MARK_ROW_GAP 1.0f
#define MARK_DISTANCE_SCALE 0.8f
#define MARK_DISTANCE_WHITE 0.96f
/* the ellipse a mark stays inside, of the view's size */
#define MARK_ELLIPSE_X 0.4375f
#define MARK_ELLIPSE_Y 0.315f
/* a place's pin stands on its tip, 15/32 of the glyph under its middle */
#define MARK_PIN_TIP 0.46875f

/* how near a client's spot the host must find what it says it marked */
#define MARK_HOST_ENEMY_NEAR 4.0f
#define MARK_HOST_ITEM_NEAR 3.0f
#define MARK_CLEAR AE_MARK_KIND_COUNT

#define MARK_TONE_RATE 22050
#define MARK_TONE_FRAMES 3300

/* ---------- globals */

#define MARK_NAME_SIZE 24

struct mark
{
	boolean active;
	short kind;
	short word;
	long object_index;
	long team_index;
	long born;
	real_point3d spot;
	wchar_t name[MARK_NAME_SIZE];
};

static struct mark marks[MARK_PLAYERS];
static struct ae_mark_press pad_press[MAXIMUM_LOCAL_PLAYERS];
static boolean key_down[MAXIMUM_LOCAL_PLAYERS];
static long marks_game = NONE;
static struct ae_mark_spam pad_spam[MAXIMUM_LOCAL_PLAYERS];
static struct ae_mark_spam host_spam[MARK_PLAYERS];

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

enum { MARK_WORD_ENEMY, MARK_WORD_WEAPON, MARK_WORD_VEHICLE, MARK_WORD_ITEM, MARK_WORD_HERE };
static wchar_t const *const mark_words[] = { L"ENEMY", L"WEAPON", L"VEHICLE", L"ITEM", L"HERE" };

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

/* the nearest to the aim of these objects, within its reach and before the hit */
static long mark_nearest_on_line(struct player_datum const *player, real_point3d const *eye,
	real_vector3d const *forward, real before, unsigned long types, real reach, real reach_slope, real reach_most, real view_scale)
{
	struct object_iterator iterator;
	struct object_datum *object;
	long own = object_get_ultimate_parent(player->unit_index);
	long best = NONE;
	real best_part = 1.0f;

	object_iterator_new(&iterator, types, 0);
	while ((object = object_iterator_next(&iterator)) != NULL)
	{
		long object_index = iterator.index;
		real along, across, allowed, part;

		if (object_index == own || object_index == player->unit_index || object->object.parent_object_index != NONE)
			continue;
		if (TEST_FLAG(_object_mask_biped, object->object.type) &&
			(TEST_FLAG(object->object.damage_flags, _object_dead_bit) || !mark_is_enemy(player, object)))
		{
			continue;
		}
		if (!mark_off_line(eye, forward, &object->object.bounding_sphere_center, &along, &across) || along > before)
			continue;
		allowed = (reach + reach_slope * along);
		if (allowed > reach_most)
			allowed = reach_most;
		allowed *= view_scale;
		part = across / (allowed * allowed);
		if (part >= best_part ||
			!mark_object_in_view(eye, object, own, object_index))
		{
			continue;
		}
		best = object_index;
		best_part = part;
	}
	return best;
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
	unit_get_head_position(object_index, &mark->spot);
	mark->spot.z += MARK_ABOVE;
}

/* whether this player sits in a vehicle's driver seat */
static boolean mark_driving(struct player_datum const *player)
{
	long own = object_get_ultimate_parent(player->unit_index);
	short seat = ((struct unit_datum *)object_get(player->unit_index))->unit.parent_seat_index;

	return own != player->unit_index && seat != NONE && TEST_FLAG(_object_mask_unit, object_get(own)->object.type) &&
		unit_seat_is_driver(own, seat);
}

/* what the aim is on: FALSE when nothing is in reach */
static boolean mark_cast(short local_player_index, struct player_datum const *player, struct mark *mark)
{
	struct observer_result const *camera = observer_get_camera(local_player_index);
	struct collision_result collision;
	real_point3d start;
	real_vector3d ray;
	real hit_at = MARK_RANGE;
	long own = object_get_ultimate_parent(player->unit_index);
	long ignore = own;
	long hit_object = NONE;
	long object_index;
	boolean hit = FALSE;
	boolean driving = mark_driving(player);
	real past_hit = driving ? MARK_DRIVER_PAST_HIT : 0.5f;
	real view_scale = camera && camera->field_of_view > 0.0f && camera->field_of_view < MARK_REACH_FIELD_OF_VIEW ?
		camera->field_of_view / MARK_REACH_FIELD_OF_VIEW : 1.0f;
	short step;

	if (!camera)
		return FALSE;
	start = camera->position;
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

			hit_object = object_get_ultimate_parent(collision.object_index);
			/* (your own vehicle is not marked: the view from a driver's seat looks over it) */
			if (hit_object == own)
			{
				ignore = collision.object_index;
				hit_object = NONE;
				start = collision.point;
				continue;
			}
			object = object_get(hit_object);
			/* (an ally is not marked: the aim goes on past them) */
			if (TEST_FLAG(_object_mask_biped, object->object.type) &&
				!TEST_FLAG(object->object.damage_flags, _object_dead_bit) && !mark_is_enemy(player, object))
			{
				ignore = hit_object;
				hit_object = NONE;
				start = collision.point;
				continue;
			}
		}
		hit = TRUE;
		hit_at = distance3d(&camera->position, &collision.point);
		break;
	}
	if (hit_object != NONE)
	{
		struct object_datum *object = object_get(hit_object);

		if (TEST_FLAG(_object_mask_biped, object->object.type) && !TEST_FLAG(object->object.damage_flags, _object_dead_bit))
		{
			mark_set_enemy(mark, hit_object);
			return TRUE;
		}
	}
	object_index = mark_nearest_on_line(player, &camera->position, &camera->forward, hit_at + (driving ? past_hit : MARK_ENEMY_PAST_HIT),
		_object_mask_biped, MARK_ENEMY_REACH, driving ? MARK_DRIVER_ENEMY_REACH_SLOPE : MARK_ENEMY_REACH_SLOPE, MARK_RANGE, view_scale);
	if (object_index != NONE)
	{
		mark_set_enemy(mark, object_index);
		return TRUE;
	}
	if (hit_object != NONE && TEST_FLAG(_object_mask_vehicle, object_get(hit_object)->object.type))
	{
		mark_set_item(mark, hit_object);
		return TRUE;
	}
	object_index = mark_nearest_on_line(player, &camera->position, &camera->forward, hit_at + past_hit,
		driving ? _object_mask_weapon | _object_mask_equipment | _object_mask_vehicle : _object_mask_weapon | _object_mask_equipment,
		driving ? MARK_DRIVER_ITEM_REACH : MARK_ITEM_REACH, driving ? MARK_DRIVER_ITEM_REACH_SLOPE : MARK_ITEM_REACH_SLOPE,
		driving ? MARK_DRIVER_ITEM_REACH_MOST : MARK_ITEM_REACH_MOST, view_scale);
	if (object_index != NONE)
	{
		mark_set_item(mark, object_index);
		return TRUE;
	}
	if (!hit)
		return FALSE;
	mark->kind = AE_MARK_KIND_PLACE;
	mark->object_index = NONE;
	mark->word = MARK_WORD_HERE;
	mark->spot = collision.point;
	return TRUE;
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
	entry->word = (byte)word;
	if (spot)
	{
		entry->x = spot->x;
		entry->y = spot->y;
		entry->z = spot->z;
	}
}

/* (the host) to every client machine with a player on the marker's team, and only those */
static void mark_host_send(struct distributed_mark const *entry, long team_index)
{
	long machines[HALO_PORT_MAXIMUM_NETWORK_MACHINES];
	short count = distributed_client_machines(machines, HALO_PORT_MAXIMUM_NETWORK_MACHINES);
	short index;

	if (!game_engine_has_teams())
		return;
	for (index = 0; index < count; index++)
	{
		struct data_iterator iterator;
		struct player_datum *player;
		boolean hears = FALSE;

		data_iterator_new(&iterator, player_data);
		while (!hears && (player = (struct player_datum *)data_iterator_next(&iterator)) != NULL)
		{
			hears = !player->quit_out_of_game && player->team_index == team_index &&
				distributed_machine_has_player(machines[index], (short)DATUM_INDEX_TO_ABSOLUTE_INDEX(iterator.datum_index));
		}
		if (hears)
		{
			struct mark_message message;

			csmemset(&message, 0, sizeof(message));
			message.mark = *entry;
			distributed_send_to_machine_reliably(machines[index], &message, _distributed_message_mark_down, 1,
				(word)sizeof(message));
		}
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

static void mark_press(short local_player_index)
{
	long player_index = local_player_get_player_index(local_player_index);
	struct player_datum *player;
	struct mark *own;
	struct mark next;

	if (player_index == NONE || (player = player_try_and_get(player_index)) == NULL || player->unit_index == NONE)
		return;
	own = &marks[DATUM_INDEX_TO_ABSOLUTE_INDEX(player_index) % MARK_PLAYERS];
	csmemset(&next, 0, sizeof(next));
	if (!mark_cast(local_player_index, player, &next))
	{
		platform_log("marks: view %d: nothing in reach", local_player_index);
		return;
	}
	if (own->active && ae_mark_same(own->kind, own->object_index, next.kind, next.object_index,
		distance_squared3d(&own->spot, &next.spot)))
	{
		struct distributed_mark entry;

		own->active = FALSE;
		platform_log("marks: view %d: own mark cleared", local_player_index);
		mark_entry(&entry, DATUM_INDEX_TO_ABSOLUTE_INDEX(player_index), MARK_CLEAR, 0, NULL);
		mark_share(&entry, player->team_index);
		return;
	}
	if (!mark_guard(local_player_index))
		return;
	next.active = TRUE;
	next.team_index = player->team_index;
	next.born = game_time_get();
	*own = next;
	mark_sound(next.kind);
	platform_log("marks: view %d: kind %d at %.2f %.2f %.2f team %ld", local_player_index, next.kind, next.spot.x, next.spot.y,
		next.spot.z, (long)player->team_index);
	if (mark_network())
	{
		struct distributed_mark entry;

		mark_entry(&entry, DATUM_INDEX_TO_ABSOLUTE_INDEX(player_index), next.kind, next.word, &next.spot);
		mark_share(&entry, player->team_index);
	}
}

static void mark_text(long font_index, real x, real top, wchar_t const *text, real scale,
	real_argb_color const *color, short outline_ways)
{
	static short const outline[8][2] = { { -1, 0 }, { 1, 0 }, { 0, -1 }, { 0, 1 }, { -1, -1 }, { 1, -1 }, { -1, 1 }, { 1, 1 } };
	struct font_header *font = font_definition_get(font_index);
	rectangle2d bounds;

	if (!font)
		return;
	bounds.x0 = (short)(x - 320.0f);
	bounds.x1 = (short)(x + 320.0f);
	bounds.y0 = (short)top;
	bounds.y1 = (short)(top + font->ascending_height + font->descending_height);
	if (outline_ways)
	{
		real step = hud_item_timers_ae_outline_step(scale);
		real_argb_color dark;
		short side;

		dark.alpha = color->alpha * 0.85f;
		dark.red = 0.0f;
		dark.green = 0.0f;
		dark.blue = 0.0f;
		draw_string_set_draw_mode(font_index, NONE, _text_justification_center, 0, &dark);
		for (side = 0; side < outline_ways; side++)
		{
			rasterizer_text_set_scale(scale, x + outline[side][0] * step, top + outline[side][1] * step);
			rasterizer_draw_unicode_string(&bounds, NULL, NULL, 0, text);
		}
	}
	draw_string_set_draw_mode(font_index, NONE, _text_justification_center, 0, color);
	rasterizer_text_set_scale(scale, x, top);
	rasterizer_draw_unicode_string(&bounds, NULL, NULL, 0, text);
	rasterizer_text_set_scale(1.0f, 0.0f, 0.0f);
}

static void mark_color(short kind, boolean ae_look, real_argb_color *color)
{
	color->alpha = 1.0f;
	if (kind == AE_MARK_KIND_ENEMY)
	{
		color->red = 1.0f;
		color->green = ae_look ? 0.302f : 0.298f;
		color->blue = ae_look ? 0.302f : 0.251f;
		return;
	}
	/* (a neutral white whatever the team or the HUD's colours: only an enemy's mark has a colour of its own) */
	color->red = 0.898f;
	color->green = 0.941f;
	color->blue = 1.0f;
}

static void mark_draw(long font_index, struct mark const *mark, real x, real y, real distance, boolean clamped,
	short style, boolean ae_look, long age)
{
	static short const glyphs[AE_MARK_KIND_COUNT][2] =
	{
		{ AE_WAYPOINT_GLYPH_MARK_ENEMY, AE_WAYPOINT_GLYPH_MARK_ENEMY_OUTLINE },
		{ AE_WAYPOINT_GLYPH_MARK_ITEM, AE_WAYPOINT_GLYPH_MARK_ITEM_OUTLINE },
		{ AE_WAYPOINT_GLYPH_MARK_PLACE, AE_WAYPOINT_GLYPH_MARK_PLACE_OUTLINE },
	};
	struct bitmap_data const *fill = hud_item_timers_ae_glyph(glyphs[mark->kind][0]);
	struct bitmap_data const *outline = hud_item_timers_ae_glyph(glyphs[mark->kind][1]);
	struct font_header *font = font_index != NONE ? font_definition_get(font_index) : NULL;
	real alpha = ae_mark_alpha(age);
	real text_scale = hud_item_timers_ae_waypoint_scale(distance);
	real size = MAX(MARK_GLYPH_SIZE * text_scale, MARK_GLYPH_SMALLEST) * ae_mark_appear_scale(age);
	real middle = mark->kind == AE_MARK_KIND_PLACE && !clamped ? y - size * MARK_PIN_TIP : y;
	real line_height = font ? (real)(font->ascending_height + font->descending_height) : 0.0f;
	real top = middle - size / 2.0f;
	boolean outlined = ae_look || mark->kind == AE_MARK_KIND_ENEMY;
	real_rectangle2d whole;
	real_argb_color color;
	int distance_row = 0;
	int name_row = 0;

	if (!fill)
		return;
	whole.x0 = 0.0f;
	whole.x1 = 1.0f;
	whole.y0 = 0.0f;
	whole.y1 = 1.0f;
	mark_color(mark->kind, ae_look, &color);
	color.alpha = alpha;
	if (outlined && outline)
	{
		real_argb_color dark;

		dark.alpha = 0.85f * alpha;
		dark.red = 0.0f;
		dark.green = 0.0f;
		dark.blue = 0.0f;
		ae_waypoint_sprite_draw(outline, &whole, x, middle, size / AE_WAYPOINT_GLYPH_SIZE,
			real_argb_color_to_pixel32(&dark));
	}
	ae_waypoint_sprite_draw(fill, &whole, x, middle, size / AE_WAYPOINT_GLYPH_SIZE,
		real_argb_color_to_pixel32(&color));
	if (!font)
		return;
	ae_waypoint_dead_ally_rows(style, &distance_row, &name_row);
	/* (at the ellipse's edge, room for one row: the distance, the word in TEXT, an item's name) */
	if (clamped && distance_row)
	{
		/* (an item's name says more than how far it is) */
		if (mark->kind == AE_MARK_KIND_ITEM && name_row)
			distance_row = 0;
		else
			name_row = 0;
	}
	if (name_row)
	{
		top -= MARK_ROW_GAP + line_height * text_scale;
		mark_text(font_index, x, top, mark->name[0] ? mark->name : mark_words[mark->word], text_scale, &color,
			mark->kind == AE_MARK_KIND_ENEMY ? 8 : ae_look ? 4 : 0);
	}
	if (distance_row)
	{
		/* (in metres, a world unit being ten feet: one decimal, as the waypoints') */
		long tenths = (long)(distance * 30.48f + 0.5f);
		real scale = text_scale * MARK_DISTANCE_SCALE;
		wchar_t text[12];
		real_argb_color white;

		if (tenths > 99999)
			tenths = 99999;
		usnprintf(text, NUMBEROF(text), L"%d.%dm", (int)(tenths / 10), (int)(tenths % 10));
		white.alpha = alpha;
		white.red = MARK_DISTANCE_WHITE;
		white.green = MARK_DISTANCE_WHITE;
		white.blue = MARK_DISTANCE_WHITE;
		top -= MARK_ROW_GAP + line_height * scale;
		mark_text(font_index, x, top, text, scale, &white, 4);
	}
}

/* an item taken or gone takes its mark with it: here, and from the host for everyone's */
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
}

/* a new game starts with none */
static void marks_new_game(void)
{
	long game = game_time_get();

	/* (and everything counted in the last game's ticks: the guards, a press held across) */
	if (marks_game == NONE || game < marks_game)
	{
		csmemset(marks, 0, sizeof(marks));
		csmemset(pad_spam, 0, sizeof(pad_spam));
		csmemset(host_spam, 0, sizeof(host_spam));
		csmemset(pad_press, 0, sizeof(pad_press));
	}
	marks_game = game;
}

/* ---------- public code */

void ae_marks_input(short local_player_index, unsigned long player_control_flags, unsigned long *unit_control_flags,
	int light_bit)
{
	struct ae_mark_press *press;
	int down, light = 1;
	boolean keyboard_down = FALSE;
	long now;

	if (!VALID_INDEX(local_player_index, MAXIMUM_LOCAL_PLAYERS) || !game_engine_running())
		return;
	marks_new_game();
	marks_tick();
	now = game_time_get();
	press = &pad_press[local_player_index];
	down = (int)((player_control_flags >> AE_ZOOM_FLASH_BIT) & 1);
	if (down != press->down)
		platform_log("marks: view %d: button %s at tick %ld (zoom has it %d)", local_player_index, down ? "down" : "up", now,
			(int)((player_control_flags >> AE_ZOOM_HELD_BIT) & 1));
	if (ae_mark_press_step(press, down, (int)((player_control_flags >> AE_ZOOM_HELD_BIT) & 1), now, &light))
		mark_press(local_player_index);
	if (!light)
		*unit_control_flags &= ~(1ul << light_bit);
	if (local_player_index == ae_seats_keyboard_controller())
		keyboard_down = (halo_keyboard_actions(local_player_index) & (1ULL << HALO_KEYBOARD_MARK)) != 0;
	if (keyboard_down && !key_down[local_player_index])
		mark_press(local_player_index);
	key_down[local_player_index] = keyboard_down;
}

void ae_marks_update(void)
{
	/* (a dedicated server has no local player, so ae_marks_input never runs there: the host's ticks come from here) */
	if (!game_engine_running())
		return;
	marks_new_game();
	marks_tick();
}

void ae_marks_draw(short local_player_index)
{
	long player_index;
	struct player_datum const *viewer;
	long font_index;
	short style;
	boolean ae_look;
	long now;
	short index;

	if (local_player_index == NONE)
		return;
	if (game_engine_scoreboard_shown(local_player_index) > 0.0f || game_engine_scoreboard_held(local_player_index))
		return;
	player_index = local_player_get_player_index(local_player_index);
	if (player_index == NONE)
		return;
	viewer = player_get(player_index);
	font_index = hud_item_timers_ae_font_index();
	style = hud_item_timers_ae_waypoint_style();
	ae_look = ae_hud_style_is_ae();
	now = game_time_get();
	for (index = 0; index < MARK_PLAYERS; index++)
	{
		struct mark *mark = &marks[index];
		long age;
		real_point3d view_position;
		real_point2d screen_position;
		real view_width, view_height, x, y, distance;
		boolean off_screen, clamped;

		if (!mark->active)
			continue;
		age = now - mark->born;
		if (ae_mark_alpha(age) <= 0.0f && age >= 0)
		{
			mark->active = FALSE;
			continue;
		}
		if (index != DATUM_INDEX_TO_ABSOLUTE_INDEX(player_index) && game_engine_has_teams() && mark->team_index != viewer->team_index)
			continue;
		if (index != DATUM_INDEX_TO_ABSOLUTE_INDEX(player_index) && !game_engine_has_teams())
			continue;
		matrix4x3_transform_point(&render.frustum.world_to_view, &mark->spot, &view_position);
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
		clamped = ae_mark_clamp(&x, &y, view_width * MARK_ELLIPSE_X, view_height * MARK_ELLIPSE_Y, off_screen);
		distance = distance3d(&render.camera.position, &mark->spot);
		mark_draw(font_index, mark, x + view_width * 0.5f, y + view_height * 0.5f, distance, clamped, style, ae_look, age);
	}
}

unsigned short ae_marks_entry_size(void)
{
	return (unsigned short)sizeof(struct distributed_mark);
}

/* (the host) what a client says it marked exists here: the nearest such object to the spot, or NONE */
static long mark_host_find(struct player_datum const *player, short kind, real_point3d const *spot)
{
	struct object_iterator iterator;
	struct object_datum *object;
	unsigned long types = kind == AE_MARK_KIND_ENEMY ? _object_mask_biped :
		_object_mask_weapon | _object_mask_equipment | _object_mask_vehicle;
	real best = kind == AE_MARK_KIND_ENEMY ? MARK_HOST_ENEMY_NEAR * MARK_HOST_ENEMY_NEAR : MARK_HOST_ITEM_NEAR * MARK_HOST_ITEM_NEAR;
	long found = NONE;

	object_iterator_new(&iterator, types, 0);
	while ((object = object_iterator_next(&iterator)) != NULL)
	{
		real distance = distance_squared3d(&object->object.bounding_sphere_center, spot);

		if (distance >= best || object->object.parent_object_index != NONE)
			continue;
		if (kind == AE_MARK_KIND_ENEMY && (TEST_FLAG(object->object.damage_flags, _object_dead_bit) ||
			!mark_is_enemy(player, object)))
		{
			continue;
		}
		best = distance;
		found = iterator.index;
	}
	return found;
}

void ae_marks_handle_up(long machine_index, void const *entries, int from_stream)
{
	struct distributed_mark entry;
	struct player_datum *player;
	struct mark *mark;
	real_point3d spot;
	long found = NONE;

	if (machine_index == NONE || !from_stream || !game_engine_running())
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
	spot.x = entry.x;
	spot.y = entry.y;
	spot.z = entry.z;
	/* (a spot in the world: no NaN, no infinity, inside the world's bound) */
	if (entry.kind >= AE_MARK_KIND_COUNT || entry.word >= NUMBEROF(mark_words) || !distributed_point_valid(&spot, UNIT_WORLD_BOUND))
		return;
	if (entry.kind != AE_MARK_KIND_PLACE && (found = mark_host_find(player, entry.kind, &spot)) == NONE)
	{
		platform_log("marks: host: player %d's kind %d has nothing near its spot", (int)entry.player_index, (int)entry.kind);
		return;
	}
	{
		int said = ae_mark_spam_take(&host_spam[entry.player_index], game_time_get());

		if (said == AE_MARK_SPAM_NOW_BLOCKED || said == AE_MARK_SPAM_STILL_BLOCKED)
			return;
	}
	csmemset(mark, 0, sizeof(*mark));
	mark->active = TRUE;
	mark->kind = entry.kind;
	mark->word = entry.word;
	mark->object_index = entry.kind == AE_MARK_KIND_ITEM ? found : NONE;
	mark->team_index = player->team_index;
	mark->born = game_time_get();
	mark->spot = spot;
	if (entry.kind == AE_MARK_KIND_ITEM)
		mark_item_name(mark, found);
	platform_log("marks: from player %d kind %d team %ld, heard here %d", (int)entry.player_index, (int)entry.kind,
		(long)player->team_index, (int)mark_heard_here(player->team_index));
	if (mark_heard_here(player->team_index))
		mark_sound(mark->kind);
	mark_host_send(&entry, player->team_index);
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
	if (entry.kind >= AE_MARK_KIND_COUNT || entry.word >= NUMBEROF(mark_words))
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
	csmemset(mark, 0, sizeof(*mark));
	mark->active = TRUE;
	mark->kind = entry.kind;
	mark->word = entry.word;
	mark->object_index = NONE;
	mark->team_index = player->team_index;
	mark->born = game_time_get();
	mark->spot.x = entry.x;
	mark->spot.y = entry.y;
	mark->spot.z = entry.z;
	if (entry.kind == AE_MARK_KIND_ITEM)
	{
		/* (the name is not sent: this machine has the item too) */
		long found = mark_host_find(player, AE_MARK_KIND_ITEM, &mark->spot);

		if (found != NONE)
			mark_item_name(mark, found);
	}
	platform_log("marks: from player %d kind %d team %ld, heard here %d", (int)entry.player_index, (int)entry.kind,
		(long)player->team_index, (int)mark_heard_here(player->team_index));
	if (mark_heard_here(player->team_index))
		mark_sound(mark->kind);
}
