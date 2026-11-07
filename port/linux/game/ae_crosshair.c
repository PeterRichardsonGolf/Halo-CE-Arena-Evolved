/*
AE_CROSSHAIR.C

CROSSHAIR v3 in the game (ae_crosshair_rules.h's rules): HUD > RETICLE's CROSSHAIR is a code-drawn crosshair the player
made (up to 8 saved, one MAIN, a choice a weapon, vehicle guns too), drawn by the AE draw layer at the view's centre in
place of the held weapon's aim items. Zoomed, the weapon's own zoom reticle shows instead (with its zoom pictures and flashes). It moves with ae_reticle_motion.c's
signal (the trigger's real spread, a shot's kick, the ground speed). The host's gametype rule CUSTOM CROSSHAIRS (universal
variant ball_melee bit 2, SET = off) turns every player's back into their weapon's own reticle; the stock reticle keeps
the player's RETICLE SIZE, SCALING and COLOR (STOCK rows) either way. Red over an enemy and green over an ally keep
their meaning in every colour.

Only what this machine draws changes: nothing is sent and nothing is kept but a cache of the settings.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cseries.h"
#include "cseries/errors.h"
#include "game/game.h"
#include "game/game_engine.h"
#include "game/players.h"
#include "items/weapon_definitions.h"
#include "items/weapons.h"
#include "render/render.h"
#include "tag_files/tag_groups.h"

#include "../src/ae_draw.h"
#include "../src/ae_layout.h"
#include "ae_ally_reticle.h"
#include "ae_ally_reticle_rules.h"
#include "ae_crosshair.h"
#include "ae_hud_digits.h"
#include "ae_reticle_motion.h"
#include "ae_score_strip.h"
#include "ae_weapon_ids.h"
#include "ae_weapon_registry.h"

const char *config_string(const char *name); /* port/linux/src/port_config.c */
real render_fov_reticle_scale(short local_player_index);

/* port/linux/src/ae_config_players.c: each player's own (split screen) */
int ae_config_player_text(int player, const char *name, char *text, size_t size);
unsigned long ae_config_player_changes(void);

#define PLAYERS 4

enum { WEAPONS_TEXT = 1024 };

static struct
{
	struct ae_crosshair slot[AE_XH_SLOTS];
	unsigned char present[AE_XH_SLOTS];
	int main_choice;
	struct ae_xh_weapons weapons;
	int size, scaling, tint, fov_base, motion;
	int scope_same; /* IN SCOPE: the drawn crosshair stays while zoomed (SAME CROSSHAIR), or the weapon's scope reticle shows */
} cache[PLAYERS];

static void cache_read(void)
{
	static unsigned long changes = (unsigned long)-1;

	if (changes != ae_config_player_changes())
	{
		int player, slot;

		changes = ae_config_player_changes();
		for (player = 0; player < PLAYERS; player++)
		{
			char text[WEAPONS_TEXT], key[40];

			memset(&cache[player], 0, sizeof(cache[player]));
			for (slot = 0; slot < AE_XH_SLOTS; slot++)
			{
				text[0] = 0;
				snprintf(key, sizeof(key), "display.crosshair_%d", slot + 1);
				ae_config_player_text(player, key, text, sizeof(text));
				cache[player].present[slot] = (unsigned char)ae_xh_slot_parse(text, &cache[player].slot[slot]);
			}
			text[0] = 0;
			ae_config_player_text(player, "display.crosshair", text, sizeof(text));
			cache[player].main_choice = ae_xh_main_of(text);
			text[0] = 0;
			ae_config_player_text(player, "display.crosshair_weapons", text, sizeof(text));
			ae_xh_weapons_parse(text, &cache[player].weapons);
			text[0] = 0;
			ae_config_player_text(player, "display.reticle_size", text, sizeof(text));
			cache[player].size = ae_xh_reticle_size_clamp(text[0] ? atol(text) : AE_XH_RETICLE_SIZE_DEFAULT);
			text[0] = 0;
			ae_config_player_text(player, "display.reticle_scaling", text, sizeof(text));
			cache[player].scaling = ae_xh_reticle_scaling_of(text);
			text[0] = 0;
			ae_config_player_text(player, "display.reticle_color", text, sizeof(text));
			cache[player].tint = ae_xh_tint_of(text);
			text[0] = 0;
			ae_config_player_text(player, "display.reticle_fov_base", text, sizeof(text));
			cache[player].fov_base = ae_xh_fov_base_of(text);
			text[0] = 0;
			ae_config_player_text(player, "display.reticle_motion", text, sizeof(text));
			cache[player].motion = ae_xh_stock_motion_of(text);
			text[0] = 0;
			ae_config_player_text(player, "display.crosshair_in_scope", text, sizeof(text));
			cache[player].scope_same = strcmp(text, "scope") != 0;
		}
	}
}

static int valid_player(short local_player_index)
{
	return local_player_index >= 0 && local_player_index < PLAYERS;
}

real ae_crosshair_stock_size(short local_player_index)
{
	if (!valid_player(local_player_index))
		return 1.0f;
	cache_read();
	return (real)cache[local_player_index].size / 100.0f;
}

real ae_crosshair_fov_scale(short local_player_index)
{
	real base = 1.0f;

	if (valid_player(local_player_index))
	{
		cache_read();
		if (cache[local_player_index].scaling == AE_XH_SCALING_FIXED)
			return 1.0f;
		/* (FOV BASE: the reticle is its stock size at that field of view; at the game's own 70 it is base's factor) */
		base = (real)ae_xh_fov_base_factor(cache[local_player_index].fov_base);
	}
	return render_fov_reticle_scale(local_player_index) * base;
}

int ae_crosshair_stock_motion(short local_player_index)
{
	if (!valid_player(local_player_index))
		return AE_XH_STOCK_MOTION_RIFLES;
	cache_read();
	return cache[local_player_index].motion;
}

unsigned long ae_crosshair_stock_color(short local_player_index, short frame_index, unsigned long color)
{
	if (valid_player(local_player_index) && frame_index == 0)
	{
		int tint;

		cache_read();
		tint = cache[local_player_index].tint;
		if (tint != AE_XH_TINT_ORIGINAL)
			color = (color & 0xFF000000ul) | ae_xh_tint_rgb(tint, 0x96D2FFul);
	}
	return ae_ally_reticle_color(local_player_index, frame_index, color);
}

/* the host's rule: a game with CUSTOM CROSSHAIRS off draws every weapon's own reticle (menus, the campaign and
offline: allowed) */
static boolean allowed_now(void)
{
	const char *forced = config_string("debug.crosshair_rule");

	/* (the automated tests' host rule: "off" is the gametype's CUSTOM CROSSHAIRS off, "on" on) */
	if (forced && !strcmp(forced, "off"))
		return FALSE;
	if (forced && !strcmp(forced, "on"))
		return TRUE;
	return !game_engine_running() || ae_xh_allowed_of(game_engine_get_variant()->universal_variant.ball_melee);
}

static void weapon_key(long weapon_index, char *key, size_t size)
{
	struct weapon_datum *weapon = weapon_get(weapon_index);
	struct ae_weapon_registry_row const *row = ae_weapon_id_row_of_definition(weapon->definition_index);

	key[0] = 0;
	if (row && row->key && row->key[0])
		ae_xh_weapon_key_of_tag(row->key, key, size);
	else
		ae_xh_weapon_key_of_tag(tag_get_name(weapon->definition_index), key, size);
}

void ae_crosshair_paint(const struct ae_crosshair *xh, const struct ae_xh_motion *motion, unsigned long rgb, float x,
	float y, float build_scale, float unit_factor, float alpha)
{
	static struct ae_xh_piece pieces[AE_XH_PIECES];
	int count = ae_xh_build(xh, motion, rgb, build_scale, pieces, AE_XH_PIECES), index;

	for (index = 0; index < count; index++)
	{
		const struct ae_xh_piece *p = &pieces[index];
		unsigned int rgba = p->rgba;

		if (alpha < 1.0f)
			rgba = (rgba & 0xFFFFFF00u) | (unsigned int)((float)(rgba & 0xFF) * (alpha < 0.0f ? 0.0f : alpha));
		switch (p->kind)
		{
		case AE_XH_PIECE_RECT:
			ae_draw_rect(x + p->x0 * unit_factor, y + p->y0 * unit_factor, (p->x1 - p->x0) * unit_factor,
				(p->y1 - p->y0) * unit_factor, 0.0f, rgba);
			break;
		case AE_XH_PIECE_SEGMENT:
			ae_draw_line(x + p->x0 * unit_factor, y + p->y0 * unit_factor, x + p->x1 * unit_factor,
				y + p->y1 * unit_factor, p->size * unit_factor, rgba);
			break;
		default:
			ae_draw_rect(x + (p->x0 - p->size) * unit_factor, y + (p->y0 - p->size) * unit_factor,
				2.0f * p->size * unit_factor, 2.0f * p->size * unit_factor, p->size * unit_factor, rgba);
			break;
		}
	}
}

boolean ae_crosshair_replaced(short local_player_index, long weapon_index, short frame_index, unsigned long color,
	boolean *drawn)
{
	static int logged_state[PLAYERS] = { -2, -2, -2, -2 }, logged_off;
	short player = valid_player(local_player_index) ? local_player_index : 0;
	const struct ae_crosshair *xh = NULL;
	char key[40], source[48];
	int choice;
	real ppu = ae_hud_pixels_per_unit(), s, centre_x, centre_y, alpha;
	struct ae_layout layout;
	struct ae_reticle_motion motion;
	struct ae_xh_motion xm;
	unsigned long base, after, rgb;
	float x, y, offset_px;
	int state;

	cache_read();
	if (ppu <= 0.0f || weapon_index == NONE)
		return FALSE;
	if (!allowed_now())
	{
		if (!logged_off)
		{
			logged_off = 1;
			error(_error_silent, "crosshair: custom crosshairs are off in this game (the host's rule): the weapon's own reticle");
		}
		return FALSE;
	}
	logged_off = 0;
	/* (zoomed with IN SCOPE at WEAPON'S SCOPE: the weapon's own zoom reticle, its scope's marks and range lines among them;
	the drawn crosshair comes back with the zoom's end. SAME CROSSHAIR, the default, draws on through the zoom, at the
	zoom's size: ae_crosshair_fov_scale follows the zoomed field of view) */
	if (player_control_get_zoom_level(local_player_index) != NONE && !cache[player].scope_same)
	{
		if (logged_state[player] != -3)
		{
			logged_state[player] = -3;
			error(_error_silent, "crosshair: zoomed, the weapon's own zoom reticle (local player %d)", (int)local_player_index);
		}
		return FALSE;
	}
	weapon_key(weapon_index, key, sizeof(key));
	choice = ae_xh_weapons_get(&cache[player].weapons, key);
	snprintf(source, sizeof(source), choice == AE_XH_CHOICE_MAIN ? "main" : "weapon %s", key);
	if (choice == AE_XH_CHOICE_MAIN)
		choice = cache[player].main_choice;
	if (choice >= 0 && choice < AE_XH_SLOTS && cache[player].present[choice])
		xh = &cache[player].slot[choice];
	if (!xh)
	{
		if (logged_state[player] != -1)
		{
			logged_state[player] = -1;
			error(_error_silent, "crosshair: the weapon's own reticle (%s, local player %d)", key, (int)local_player_index);
		}
		return FALSE;
	}
	if (*drawn)
		return TRUE;
	/* (the overlays that cover the view hide the stock reticle, which they draw over: the draw layer's picture is on top of
	them, so it is held back too: the pre-game and round countdown's screen, the game over's) */
	if (game_engine_pregame_countdown_ticks_left() > 0 || game_engine_game_over())
	{
		*drawn = TRUE;
		return TRUE;
	}
	s = ae_score_strip_view_scale();
	if (s <= 0.0f)
		return FALSE;
	ae_draw_current_layout(&layout);
	if (layout.scale <= 0.0f)
		return FALSE;
	/* (as hud_draw_bitmap_internal places a piece: the view's centre in HUD units, here whole pixels) */
	centre_x = ((render.camera.window_bounds.x1 + render.camera.window_bounds.x0) / 2 -
		render.camera.viewport_bounds.x0) * ppu;
	centre_y = ((render.camera.window_bounds.y1 + render.camera.window_bounds.y0) / 2 -
		render.camera.viewport_bounds.y0) * ppu;
	centre_x = (real)(long)(centre_x + 0.5f);
	centre_y = (real)(long)(centre_y + 0.5f);
	alpha = (real)((color >> 24) & 0xFF) / 255.0f;
	if (alpha <= 0.0f)
		alpha = 1.0f;
	/* (the colour: red over an enemy (the aim assist's frame, or the tests' forced one), green over an ally) */
	base = ((unsigned long)xh->color[0] << 16) | ((unsigned long)xh->color[1] << 8) | xh->color[2];
	after = ae_ally_reticle_color(local_player_index, frame_index, 0xFF000000ul | base);
	{
		const char *forced = config_string("debug.reticle_force_state");

		if (forced && !strcmp(forced, "enemy"))
			frame_index = 1;
		else if (forced && !strcmp(forced, "ally") && frame_index == 0)
			after = 0xFF000000ul | AE_ALLY_RETICLE_RGB;
	}
	state = frame_index != 0 ? 1 : (after & 0xFFFFFFul) != base ? 2 : 0;
	rgb = ae_xh_state_rgb(base, state == 1, state == 2);
	ae_reticle_motion_of(local_player_index, weapon_index, &motion);
	offset_px = (real)xh->inner.offset * 0.5f;
	xm.spread = motion.spread;
	xm.kick = motion.kick * 0.35f * (offset_px < 8.0f ? 8.0f : offset_px);
	xm.move = motion.move;
	ae_layout_from_pixels(&layout, (float)centre_x, (float)centre_y, &x, &y);
	ae_crosshair_paint(xh, &xm, rgb, x, y, (float)s, 1.0f / layout.scale, (float)alpha);
	*drawn = TRUE;
	if (logged_state[player] != state)
	{
		logged_state[player] = state;
		error(_error_silent, "crosshair: drawn %s, %s (%s, local player %d, rgb %06lx)", xh->shape ? ae_xh_shape_name(xh->shape) : "NO SHAPE",
			state == 1 ? "enemy" : state == 2 ? "ally" : "normal", source, (int)local_player_index, rgb);
	}
	(void)after;
	return TRUE;
}
