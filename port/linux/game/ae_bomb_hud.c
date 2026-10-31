/*
AE_BOMB_HUD.C

SEARCH AND DESTROY's carried-bomb tile (ae_bomb_hud.h). Its pickup and drop (the approved mockup): picked up, it grows in from 135 % with a white flash and a ring
opening out (180 ms); dropped or planted, a flash, then it shrinks to 60 % and fades (320 ms). REDUCE MOTION: none.
*/

#include <math.h>
#include <string.h>
#include "cseries/cseries.h"
#include "cseries/cseries_windows.h"
#include "math/real_math.h"
#include "memory/data.h"
#include "game/players.h"
#include "interface/hud_definitions.h"
#include "interface/hud_draw.h"
#include "ae_bomb_assault.h"
#include "ae_bomb_hud.h"
#include "ae_hud_digits.h"
#include "ae_hud_style.h"
#include "ae_motion.h"
#include "ae_panel_engine.h"
#include "ae_panel_draw.h"
#include "ae_score_strip.h"
#include "ae_sensor.h"

/* the sensor's rim in HUD units (ae_sensor.c's RIM_UNITS / RIM_UNITS_SPLIT) */
#define AE_BOMB_HUD_RIM_UNITS 42.0f
#define AE_BOMB_HUD_RIM_UNITS_SPLIT 32.0f
/* the tile's side over the sensor's rim (34 px by an 84 px rim in the approved mockup); a size up with no sensor */
#define AE_BOMB_HUD_TILE_OF_RIM 0.405f
#define AE_BOMB_HUD_TILE_NO_SENSOR 1.3f
/* the gap to the rim at 1080p, px, and the tile's bearing from the sensor's centre, degrees below the right */
#define AE_BOMB_HUD_GAP 8.0f
#define AE_BOMB_HUD_BEARING 42.0f

/* the pickup's and the drop's lengths, ms */
#define AE_BOMB_HUD_PICKUP_MS 180
#define AE_BOMB_HUD_DROP_MS 320

/* each local player's tile: carried last frame, its team, and when that last changed (system ms) */
static struct
{
	boolean carrying;
	long team_index;
	unsigned long since;
} ae_bomb_hud_tiles[MAXIMUM_NUMBER_OF_LOCAL_PLAYERS];

/* the colours: the team's (the strip's team red / blue), the pictogram's white, the keyline's dark */
static void team_rgb(long team_index, real alpha, real_argb_color *color)
{
	color->alpha = alpha;
	color->red = team_index == 1 ? 89.0f / 255.0f : 1.0f;
	color->green = team_index == 1 ? 140.0f / 255.0f : 77.0f / 255.0f;
	color->blue = team_index == 1 ? 1.0f : 77.0f / 255.0f;
}

static void rgb(real r, real g, real b, real alpha, real_argb_color *color)
{
	color->alpha = alpha; color->red = r; color->green = g; color->blue = b;
}

/* a rounded rectangle (convex: the panels' fan fills it), corners of radius rad, keyline grow px out (0: none) */
static void rounded(real x0, real y0, real x1, real y1, real rad, real grow, const real_argb_color *color)
{
	real points[24][2];
	short corner, i, n = 0;

	if (rad > (x1 - x0) / 2.0f) rad = (x1 - x0) / 2.0f;
	if (rad > (y1 - y0) / 2.0f) rad = (y1 - y0) / 2.0f;
	for (corner = 0; corner < 4; corner++)
	{
		real cx = corner == 0 || corner == 3 ? x1 - rad : x0 + rad;
		real cy = corner < 2 ? y1 - rad : y0 + rad;

		for (i = 0; i < 6; i++)
		{
			real angle = (real)(corner * 90 + i * 18) * (real)M_PI / 180.0f;

			points[n][0] = cx + rad * (real)cos(angle);
			points[n][1] = cy + rad * (real)sin(angle);
			n++;
		}
	}
	ae_panel_fill_polygon_soft((const real (*)[2])points, n, grow, color);
}

/* the pictogram's 64-unit grid (tile_mk.py's), mapped into the glyph's box */
struct grid { real x, y, unit; };
static real gx(const struct grid *g, real v) { return g->x + v * g->unit; }
static real gy(const struct grid *g, real v) { return g->y + v * g->unit; }

/* the tile's look this frame: its size over the resting one, a white flash over it (0-1), its opacity (0-1), and the
pickup's ring (how far it has opened, 0-1; 1: none) */
struct tile_look { real scale, flash, alpha, ring; };

static void draw_tile(real cx, real cy, real side, long team_index, const struct tile_look *look)
{
	real x0, y0;
	real_argb_color tile, white, dark;
	struct grid g;
	short i;

	/* (the ring opens out from 60 % to 130 % of the resting side, fading) */
	if (look->ring < 1.0f)
	{
		real half = side * (0.6f + 0.7f * ae_ease_out(look->ring)) / 2.0f;
		real width = side * 0.09f;

		ae_panel_draw_line(cx - half, cy - half, cx + half, cy - half, width, _ae_tone_white, 1.0f - look->ring, FALSE);
		ae_panel_draw_line(cx + half, cy - half, cx + half, cy + half, width, _ae_tone_white, 1.0f - look->ring, FALSE);
		ae_panel_draw_line(cx + half, cy + half, cx - half, cy + half, width, _ae_tone_white, 1.0f - look->ring, FALSE);
		ae_panel_draw_line(cx - half, cy + half, cx - half, cy - half, width, _ae_tone_white, 1.0f - look->ring, FALSE);
	}
	side *= look->scale;
	x0 = cx - side / 2.0f;
	y0 = cy - side / 2.0f;
	team_rgb(team_index, 0.94f * look->alpha, &tile);
	rgb(245.0f / 255.0f, 250.0f / 255.0f, 1.0f, look->alpha, &white);
	rgb(6.0f / 255.0f, 10.0f / 255.0f, 18.0f / 255.0f, 0.90f * look->alpha, &dark);
	/* the tile: the HUD's keyline, then the team's colour */
	rounded(x0, y0, x0 + side, y0 + side, side * 0.12f, 1.0f, &dark);
	rounded(x0, y0, x0 + side, y0 + side, side * 0.12f, 0.0f, &tile);
	g.unit = side * 0.78f / 64.0f;
	g.x = cx - 32.0f * g.unit;
	g.y = cy - 32.0f * g.unit;
	/* three capsule cases */
	for (i = 0; i < 3; i++)
	{
		real left = i == 0 ? 9.0f : i == 1 ? 25.5f : 42.0f;

		rounded(gx(&g, left), gy(&g, 10.0f), gx(&g, left + 13.0f), gy(&g, 58.0f), 6.5f * g.unit, 0.0f, &white);
	}
	/* two straps bridging them, a hairline of the tile's colour each side */
	for (i = 0; i < 2; i++)
	{
		real y = i ? 49.0f : 19.0f;

		rounded(gx(&g, 6.0f), gy(&g, y - 1.2f), gx(&g, 58.0f), gy(&g, y + 3.2f), 0.0f, 0.0f, &tile);
		rounded(gx(&g, 7.0f), gy(&g, y), gx(&g, 57.0f), gy(&g, y + 2.0f), 1.0f * g.unit, 0.0f, &white);
	}
	/* the timer plate on the middle case, a ring of the tile's colour round it, its three bars across and the beacon
	cut out */
	rounded(gx(&g, 19.0f), gy(&g, 26.0f), gx(&g, 45.0f), gy(&g, 42.0f), 3.5f * g.unit, 0.0f, &tile);
	rounded(gx(&g, 21.0f), gy(&g, 28.0f), gx(&g, 43.0f), gy(&g, 40.0f), 2.5f * g.unit, 0.0f, &white);
	for (i = 0; i < 3; i++)
	{
		real y = 30.6f + (real)i * 2.9f;

		rounded(gx(&g, 23.5f), gy(&g, y), gx(&g, 36.0f), gy(&g, y + 1.6f), 0.8f * g.unit, 0.0f, &tile);
	}
	ae_panel_fill_disc(gx(&g, 39.25f), gy(&g, 33.75f), 1.75f * g.unit, &tile);
	/* the wire: an arc over the left case into the plate */
	for (i = 0; i < 8; i++)
	{
		real a0 = (195.0f + (real)i * 18.75f) * (real)M_PI / 180.0f;
		real a1 = (195.0f + (real)(i + 1) * 18.75f) * (real)M_PI / 180.0f;

		ae_panel_draw_line(gx(&g, 23.0f + 9.0f * (real)cos(a0)), gy(&g, 11.0f + 8.0f * (real)sin(a0)),
			gx(&g, 23.0f + 9.0f * (real)cos(a1)), gy(&g, 11.0f + 8.0f * (real)sin(a1)), 2.0f * g.unit, _ae_tone_white,
			look->alpha, FALSE);
	}
	/* the flash: white over the whole tile */
	if (look->flash > 0.0f)
	{
		real_argb_color flash;

		rgb(1.0f, 1.0f, 1.0f, look->flash * look->alpha, &flash);
		rounded(x0, y0, x0 + side, y0 + side, side * 0.12f, 0.0f, &flash);
	}
}

/* a linear step through keyframes (ms, value), clamped at both ends */
static real keyframes(const real (*frames)[2], short count, real ms)
{
	short i;

	if (ms <= frames[0][0])
		return frames[0][1];
	for (i = 1; i < count; i++)
	{
		if (ms <= frames[i][0])
			return frames[i - 1][1] + (frames[i][1] - frames[i - 1][1]) * (ms - frames[i - 1][0]) /
				(frames[i][0] - frames[i - 1][0]);
	}
	return frames[count - 1][1];
}

/* the viewer's team when it carries the bomb, NONE when not */
static long carrying_team(short local_player_index)
{
	long player_index;
	struct player_datum *player;

	if (!ae_hud_style_is_ae())
		return NONE;
	player_index = local_player_get_player_index(local_player_index);
	player = player_index != NONE ? player_try_and_get(player_index) : NULL;
	if (!player || !ae_bomb_snd_unit_carries(player->unit_index))
		return NONE;
	return player->team_index;
}

/* the tile to draw this frame (its team, NONE: none) and its look: carried, picked up a moment ago, or dropped a
moment ago (the mockup's keyframes) */
static long carrier_team(short local_player_index, struct tile_look *look)
{
	static const real pickup_scale[][2] = { { 0, 1.35f }, { 60, 1.22f }, { 120, 1.08f }, { 180, 1.0f } };
	static const real pickup_flash[][2] = { { 0, 0.9f }, { 60, 0.6f }, { 120, 0.3f }, { 180, 0.0f } };
	static const real drop_scale[][2] = { { 0, 1.0f }, { 60, 1.06f }, { 120, 1.0f }, { 220, 0.8f }, { 320, 0.6f } };
	static const real drop_flash[][2] = { { 0, 0.0f }, { 60, 0.7f }, { 120, 0.4f }, { 220, 0.0f } };
	static const real drop_alpha[][2] = { { 0, 1.0f }, { 120, 1.0f }, { 220, 0.55f }, { 320, 0.0f } };
	long team = carrying_team(local_player_index);
	unsigned long now = system_milliseconds();
	boolean reduced = ae_panel_engine_reduced_motion();
	real ms;

	look->scale = 1.0f;
	look->flash = 0.0f;
	look->alpha = 1.0f;
	look->ring = 1.0f;
	if (local_player_index < 0 || local_player_index >= MAXIMUM_NUMBER_OF_LOCAL_PLAYERS)
		return team;
	if ((team != NONE) != ae_bomb_hud_tiles[local_player_index].carrying)
	{
		ae_bomb_hud_tiles[local_player_index].carrying = team != NONE;
		ae_bomb_hud_tiles[local_player_index].since = now;
		if (team != NONE)
			ae_bomb_hud_tiles[local_player_index].team_index = team;
	}
	ms = (real)(now - ae_bomb_hud_tiles[local_player_index].since);
	if (team != NONE)
	{
		if (!reduced && ms < AE_BOMB_HUD_PICKUP_MS)
		{
			look->scale = keyframes(pickup_scale, NUMBEROF(pickup_scale), ms);
			look->flash = keyframes(pickup_flash, NUMBEROF(pickup_flash), ms);
			look->ring = ms / AE_BOMB_HUD_PICKUP_MS;
		}
		return team;
	}
	if (reduced || ms >= AE_BOMB_HUD_DROP_MS || ae_bomb_hud_tiles[local_player_index].team_index == NONE ||
		!ae_hud_style_is_ae())
	{
		return NONE;
	}
	look->scale = keyframes(drop_scale, NUMBEROF(drop_scale), ms);
	look->flash = keyframes(drop_flash, NUMBEROF(drop_flash), ms);
	look->alpha = keyframes(drop_alpha, NUMBEROF(drop_alpha), ms);
	return ae_bomb_hud_tiles[local_player_index].team_index;
}

void ae_bomb_hud_tile(short local_player_index, const union point2d *sensor_corner, boolean split)
{
	struct tile_look look;
	long team = carrier_team(local_player_index, &look);
	real ppu, r, cx, cy, side, distance, angle;

	if (team == NONE || !sensor_corner)
		return;
	ppu = ae_hud_pixels_per_unit();
	r = (split ? AE_BOMB_HUD_RIM_UNITS_SPLIT : AE_BOMB_HUD_RIM_UNITS) * ppu;
	cx = (real)sensor_corner->x * ppu;
	cy = (real)sensor_corner->y * ppu;
	side = AE_BOMB_HUD_TILE_OF_RIM * r;
	distance = r + AE_BOMB_HUD_GAP * ae_score_strip_view_scale() + 0.62f * side;
	angle = AE_BOMB_HUD_BEARING * (real)M_PI / 180.0f;
	draw_tile(cx + distance * (real)cos(angle), cy + distance * (real)sin(angle), side, team, &look);
}

void ae_bomb_hud_tile_no_sensor(short local_player_index, struct hud_placement_definition const *blip_placement)
{
	struct hud_absolute_placement_definition placement;
	struct tile_look look;
	long team = carrier_team(local_player_index, &look);
	boolean split = hud_split_screen_layout();
	point2d corner;
	real ppu, r, side;

	if (team == NONE || !blip_placement)
		return;
	memset(&placement, 0, sizeof(placement));
	placement.corner = _hud_anchor_bottom_left;
	hud_calculate_point(local_player_index, &placement, blip_placement, NULL, split, 0.0f, &corner);
	ppu = ae_hud_pixels_per_unit();
	r = (split ? AE_BOMB_HUD_RIM_UNITS_SPLIT : AE_BOMB_HUD_RIM_UNITS) * ppu;
	side = AE_BOMB_HUD_TILE_OF_RIM * AE_BOMB_HUD_TILE_NO_SENSOR * r;
	/* (in the sensor's corner, a little in from its centre toward the screen's corner, as the mockup) */
	draw_tile((real)corner.x * ppu - 0.36f * r, (real)corner.y * ppu + 0.36f * r, side, team, &look);
}
