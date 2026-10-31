/*
AE_DEAD_ALLY_MARKERS.C

DEAD ALLY MARKERS (HUD > NAMES, display.dead_ally_markers, each player's
own, ON): in a team game, the cross where a teammate fell, over their
body, seen through walls, until they respawn or the body goes (owner
2026-10-24; the name tag rule: "When players die, the name tag is
replaced with a red cross").

It follows the HUD's own looks, so it reads as the names and the item
waypoints do (owner: "markers need to respect halo ae hud and markers along
with classic"):
- its colour is the teammate's name colour (hud.c's hud_player_name_color:
  HUD STYLE AE's rule, white while ENEMY NAME COLOR is RED, so a fallen ally
  never reads as an enemy; else the HUD's);
- its rows are WAYPOINTS' (display.waypoint_style): the distance (ICONS,
  BOTH), the name (TEXT, BOTH), then the cross (always), sized as the item
  waypoints' labels (WAYPOINT SIZE, the view, the distance);
- HUD STYLE AE gives the name and the cross the names' dark outline;
  CLASSIC draws them plain (the distance has its outline in both, as the
  waypoints' does).

Only drawing, on each machine: every machine knows each player's team and
dead body (players.c's player_died keeps dead_unit_index until the respawn).
The idea is aLTis94's hud_features.lua for Chimera (dead ally markers; no
licence: no code of it is here). The rules are ae_waypoint_icons.c's
(ae_waypoint_dead_ally_marked, ae_waypoint_dead_ally_rows; unit tested).
*/

#include "cseries.h"
#include "math/real_math.h"
#include "memory/data.h"
#include "game/game_engine.h"
#include "game/players.h"
#include "objects/objects.h"
#include "objects/object_types.h"
#include "units/units.h"
#include "interface/hud.h"
#include "render/render.h"
#include "render/render_camera_projection.h"
#include "rasterizer/rasterizer.h"
#include "bitmaps/bitmaps.h"
#include "bitmaps/bitmap_color_conversion.h"
#include "text/draw_string.h"
#include "text/font_group.h"
#include "text/unicode.h"

#include "ae_dead_ally_markers.h"
#include "ae_hud_style.h"
#include "ae_test_hooks.h"
#include "ae_waypoint_icons.h"
#include "ae_waypoint_ink.h"

void platform_log(const char *format, ...) __attribute__((format(printf, 1, 2)));

/* port/linux/src/ae_config_players.c: each player's own (split screen) */
int ae_config_player_boolean(int player, const char *name);
unsigned long ae_config_player_changes(void);
/* source/interface/hud_item_timers.c's, for the item waypoints' looks (its AE hooks) */
struct bitmap_data const *hud_item_timers_ae_glyph(short glyph);
long hud_item_timers_ae_font_index(void);
short hud_item_timers_ae_waypoint_style(void);
real hud_item_timers_ae_waypoint_scale(real world_units);
real hud_item_timers_ae_outline_step(real row_scale);

/* over the body's head, as a teammate's triangle is over a living one's (hud.c's names) */
#define DEAD_ALLY_HEIGHT 0.3f
/* the cross's height, in the text scale's units (about 1.6 of the name's capitals) */
#define DEAD_ALLY_CROSS_SIZE 24.0f
/* the gap between the rows, in the HUD's units */
#define DEAD_ALLY_ROW_GAP 1.0f
/* the distance row is 4/5 of the name's, near white (the item waypoints') */
#define DEAD_ALLY_DISTANCE_SCALE 0.8f
#define DEAD_ALLY_DISTANCE_WHITE 0.96f

/* debug.txt's line, once for each body marked (the last few kept) */
static void dead_ally_log_once(
	short local_player_index,
	long player_index,
	long body_index,
	boolean on_screen,
	real distance)
{
	static struct { long body; boolean on_screen; boolean used; } logged[32];
	static short next;
	short index;

	/* (once off screen and once on, for each body) */
	for (index = 0; index < (short)NUMBEROF(logged); index++)
	{
		if (logged[index].used && logged[index].body == body_index && logged[index].on_screen == on_screen)
			return;
	}
	logged[next].body = body_index;
	logged[next].on_screen = on_screen;
	logged[next].used = TRUE;
	next = (short)((next + 1) % NUMBEROF(logged));
	platform_log("dead ally markers: view %d marks player %ld's body %08lx at %.1f m%s", (int)local_player_index,
		DATUM_INDEX_TO_ABSOLUTE_INDEX(player_index), (unsigned long)body_index, distance * 3.048f,
		on_screen ? "" : " (off screen)");
	/* (debug.capture_on_event's "dead_ally": the first sight of each) */
	if (on_screen)
		ae_test_capture("dead_ally", 150);
}

static boolean dead_ally_markers_on(
	short local_player_index)
{
	static boolean enabled[MAXIMUM_LOCAL_PLAYERS];
	static unsigned long changes;
	static boolean read = FALSE;
	short index;

	if (!read || changes != ae_config_player_changes())
	{
		read = TRUE;
		changes = ae_config_player_changes();
		for (index = 0; index < MAXIMUM_LOCAL_PLAYERS; index++)
			enabled[index] = ae_config_player_boolean(index, "display.dead_ally_markers") != 0;
	}

	return local_player_index >= 0 && local_player_index < MAXIMUM_LOCAL_PLAYERS && enabled[local_player_index];
}

/* a row of text centred on x, its top at top, at scale; in a dark outline (four ways, as the waypoints' distance)
when outlined */
static void dead_ally_text(
	long font_index,
	real x,
	real top,
	wchar_t const *text,
	real scale,
	real_argb_color const *color,
	boolean outlined)
{
	static short const outline[4][2] = { { -1, 0 }, { 1, 0 }, { 0, -1 }, { 0, 1 } };
	struct font_header *font = font_definition_get(font_index);
	rectangle2d bounds;

	if (!font)
		return;
	bounds.x0 = (short)(x - 320.0f);
	bounds.x1 = (short)(x + 320.0f);
	bounds.y0 = (short)top;
	bounds.y1 = (short)(top + font->ascending_height + font->descending_height);
	if (outlined)
	{
		real step = hud_item_timers_ae_outline_step(scale);
		real_argb_color dark;
		short side;

		dark.alpha = color->alpha * 0.85f;
		dark.red = 0.0f;
		dark.green = 0.0f;
		dark.blue = 0.0f;
		draw_string_set_draw_mode(font_index, NONE, _text_justification_center, 0, &dark);
		for (side = 0; side < 4; side++)
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

/* one marker: the cross with its middle at x, middle; the rows over it */
static void dead_ally_marker_draw(
	long font_index,
	struct player_datum const *player,
	real x,
	real middle,
	real distance,
	short style,
	boolean ae_look)
{
	real_rectangle2d whole;
	struct bitmap_data const *cross = hud_item_timers_ae_glyph(AE_WAYPOINT_GLYPH_DEAD_ALLY);
	struct bitmap_data const *outline = hud_item_timers_ae_glyph(AE_WAYPOINT_GLYPH_DEAD_ALLY_OUTLINE);
	struct font_header *font = font_index != NONE ? font_definition_get(font_index) : NULL;
	real text_scale = hud_item_timers_ae_waypoint_scale(distance);
	real cross_size = DEAD_ALLY_CROSS_SIZE * text_scale;
	real line_height = font ? (real)(font->ascending_height + font->descending_height) : 0.0f;
	real top = middle - cross_size / 2.0f;
	real_argb_color color;
	int distance_row = 0;
	int name_row = 0;

	if (!cross)
		return;
	whole.x0 = 0.0f;
	whole.x1 = 1.0f;
	whole.y0 = 0.0f;
	whole.y1 = 1.0f;
	hud_player_name_color(TRUE, &color);
	color.alpha = 1.0f;
	if (ae_look && outline)
	{
		real_argb_color dark;

		dark.alpha = 0.85f;
		dark.red = 0.0f;
		dark.green = 0.0f;
		dark.blue = 0.0f;
		ae_waypoint_sprite_draw(outline, &whole, x, middle, cross_size / AE_WAYPOINT_GLYPH_SIZE,
			real_argb_color_to_pixel32(&dark));
	}
	ae_waypoint_sprite_draw(cross, &whole, x, middle, cross_size / AE_WAYPOINT_GLYPH_SIZE,
		real_argb_color_to_pixel32(&color));
	if (!font)
		return;
	ae_waypoint_dead_ally_rows(style, &distance_row, &name_row);
	if (name_row)
	{
		wchar_t name[NUMBEROF(player->name) + 1];
		short index;

		for (index = 0; index < (short)NUMBEROF(player->name); index++)
			name[index] = player->name[index];
		name[NUMBEROF(player->name)] = 0;
		top -= DEAD_ALLY_ROW_GAP + line_height * text_scale;
		dead_ally_text(font_index, x, top, name, text_scale, &color, ae_look);
	}
	if (distance_row)
	{
		/* (in metres, a world unit being ten feet: one decimal, as the waypoints') */
		long tenths = (long)(distance * 30.48f + 0.5f);
		real scale = text_scale * DEAD_ALLY_DISTANCE_SCALE;
		wchar_t text[12];
		real_argb_color white;

		if (tenths > 99999)
			tenths = 99999;
		usnprintf(text, NUMBEROF(text), L"%d.%dm", (int)(tenths / 10), (int)(tenths % 10));
		white.alpha = 1.0f;
		white.red = DEAD_ALLY_DISTANCE_WHITE;
		white.green = DEAD_ALLY_DISTANCE_WHITE;
		white.blue = DEAD_ALLY_DISTANCE_WHITE;
		top -= DEAD_ALLY_ROW_GAP + line_height * scale;
		dead_ally_text(font_index, x, top, text, scale, &white, TRUE);
	}
}

void ae_dead_ally_markers_draw(
	short local_player_index)
{
	long player_index;
	long font_index;
	struct player_datum const *viewer;
	struct player_datum *player;
	struct data_iterator iterator;
	short style;
	boolean ae_look;

	if (local_player_index == NONE || !game_engine_running() || !dead_ally_markers_on(local_player_index))
		return;
	/* (none while this view's scoreboard shows, as the item waypoints) */
	if (game_engine_scoreboard_shown(local_player_index) > 0.0f || game_engine_scoreboard_held(local_player_index))
		return;
	player_index = local_player_get_player_index(local_player_index);
	if (player_index == NONE)
		return;
	viewer = player_get(player_index);
	if (viewer->unit_index == NONE)
		return;
	font_index = hud_item_timers_ae_font_index();
	style = hud_item_timers_ae_waypoint_style();
	ae_look = ae_hud_style_is_ae();
	data_iterator_new(&iterator, player_data);
	while ((player = data_iterator_next(&iterator)) != NULL)
	{
		long body_index = player->dead_unit_index;
		boolean body = body_index != NONE && object_try_and_get_and_verify_type(body_index, _object_mask_unit) != NULL;
		real_point3d position;
		real_point3d view_position;
		real_point2d screen_position;

		if (!ae_waypoint_dead_ally_marked(game_engine_has_teams(), iterator.datum_index == player_index,
			player->team_index == viewer->team_index, player->unit_index != NONE, body))
		{
			continue;
		}
		unit_get_head_position(body_index, &position);
		position.z += DEAD_ALLY_HEIGHT;
		matrix4x3_transform_point(&render.frustum.world_to_view, &position, &view_position);
		if (!render_camera_view_to_screen(&render.camera, &render.frustum, &view_position, &screen_position))
		{
			dead_ally_log_once(local_player_index, iterator.datum_index, body_index, FALSE,
				distance3d(&render.camera.position, &position));
			continue;
		}
		dead_ally_log_once(local_player_index, iterator.datum_index, body_index, TRUE,
			distance3d(&render.camera.position, &position));
		dead_ally_marker_draw(font_index, player,
			screen_position.x - (real)render.camera.viewport_bounds.x0,
			screen_position.y - (real)render.camera.viewport_bounds.y0,
			distance3d(&render.camera.position, &position), style, ae_look);
	}
}
