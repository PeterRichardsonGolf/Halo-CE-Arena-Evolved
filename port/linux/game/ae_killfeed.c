/*
AE_KILLFEED.C

The kill feed on this machine (ae_killfeed.h): the killing blow kept as damage.c sees it, a row for each local player when the game says
a player died, and the panel that draws the rows (a lone player's view) or the line a split-screen player's message list gets.

Everything drawn is the view's pixels (x from its left, y from its top) at the HUD's scale; the time is the game's ticks.
*/

#include <math.h>
#include <string.h>
#include <wchar.h>

#include "cseries/cseries.h"
#include "bitmaps/bitmap_color_conversion.h"
#include "bitmaps/bitmap_group.h"
#include "bitmaps/bitmaps.h"
#include "cache/cache_files.h"
#include "cache/texture_cache.h"
#include "game/game.h"
#include "game/game_engine.h"
#include "game/players.h"
#include "interface/hud.h"
#include "interface/hud_definitions.h"
#include "interface/hud_draw.h"
#include "interface/hud_messaging.h"
#include "interface/weapon_hud_interface_definition.h"
#include "items/weapon_definitions.h"
#include "items/weapons.h"
#include "objects/damage.h"
#include "render/render.h"
#include "tag_files/tag_groups.h"
#include "tag_files/tag_files.h"
#include "units/units.h"

#include "../src/ae_config_players.h"

#include "ae_burst_rifle.h"
#include "ae_hud_digits.h"
#include "ae_hud_style.h"
#include "ae_hud_text.h"
#include "ae_killfeed.h"
#include "ae_killfeed_rules.h"
#include "ae_marker_rules.h"
#include "ae_medals.h"
#include "ae_medals_glue.h"
#include "ae_panel_draw.h"
#include "ae_panel_model.h"
#include "ae_rifle_profiles.h"
#include "ae_score_strip.h"
#include "ae_sensor.h"
#include "ae_waypoint_icons.h"
#include "ae_waypoint_ink.h"
#include "ae_weapon_pack.h"

void platform_log(const char *format, ...) __attribute__((format(printf, 1, 2)));

#define weapon_hud_interface_definition_get(index) \
	((struct weapon_hud_interface_definition *)tag_get('wphi', (index)))

#define PERFECT_TAG "PERF"
#define SKULL_SEQUENCE 23
#define SKULL_SHEET "ui\\hud\\bitmaps\\combined\\hud_msg_icons"

/* ---------- state */

static struct ae_kf_list lists[MAXIMUM_LOCAL_PLAYERS];

/* the blow that may have killed a player's unit, as damage.c saw it this tick */
static struct
{
	boolean set;
	long tick;
	long victim_player_index;
	long object_index;
	long definition_index;
	boolean headshot;
} blow;

/* ---------- private code */

static boolean wanted(short local_player_index)
{
	return local_player_index >= 0 && local_player_index < MAXIMUM_LOCAL_PLAYERS &&
		local_player_get_player_index(local_player_index) != NONE &&
		ae_config_player_boolean(local_player_index, "display.kill_feed") != 0;
}

static boolean anyone_wants(void)
{
	short local;

	for (local = 0; local < MAXIMUM_LOCAL_PLAYERS; local++)
		if (wanted(local))
			return TRUE;
	return FALSE;
}

static void copy_name(struct player_datum const *player, wchar_t *to)
{
	short index;

	for (index = 0; index < AE_KF_NAME - 1 && index < (short)NUMBEROF(player->name) && player->name[index]; index++)
		to[index] = player->name[index];
	to[index] = 0;
}

/* the weapon in a player's hands: its definition, NONE for none */
static long hand_definition(long player_index)
{
	struct player_datum const *player = player_index != NONE ? player_try_and_get(player_index) : NULL;
	struct unit_datum const *unit = player ? unit_try_and_get(player->unit_index) : NULL;
	short current;
	struct weapon_datum *weapon;

	if (!unit)
		return NONE;
	current = unit->unit.current_weapon_index;
	if (current < 0 || current >= MAXIMUM_WEAPONS_PER_UNIT || unit->unit.weapon_object_indices[current] == NONE)
		return NONE;
	weapon = weapon_try_and_get(unit->unit.weapon_object_indices[current]);
	return weapon ? weapon->definition_index : NONE;
}

/* what weapon killed, and the weapon tag its picture is taken from (NONE for none): the damage's own path first; a damage the
tags do not name (a pack weapon's) by its rifle profile or by the weapon in the killer's hands */
static int classify(long definition_index, long killing_player_index, long *weapon_tag)
{
	int weapon = AE_KF_WEAPON_UNKNOWN;
	char const *name = definition_index != NONE ? tag_get_name(definition_index) : NULL;
	char path[160];

	*weapon_tag = NONE;
	/* (a pack weapon's damage can sit in another weapon's folder: the carried weapon that fires it says which it is) */
	if (definition_index != NONE)
	{
		long carried = ae_medals_killing_weapon_definition(killing_player_index, definition_index);
		short slot = carried != NONE ? ae_weapon_pack_definition_slot(carried) : 0;

		if (slot)
		{
			*weapon_tag = carried;
			return ae_kf_weapon_of_slot(slot);
		}
	}
	if (name)
		weapon = ae_kf_weapon_of_path(name);
	if (weapon != AE_KF_WEAPON_UNKNOWN)
	{
		if (!ae_kf_weapon_has_no_picture(weapon) && ae_kf_weapon_tag_path(name, path, sizeof(path)))
			*weapon_tag = tag_loaded(WEAPON_DEFINITION_TAG, path);
		return weapon;
	}
	if (definition_index != NONE)
	{
		struct ae_rifle_profile const *profile = ae_burst_rifle_damage_profile(definition_index);
		long hand = hand_definition(killing_player_index);

		if (profile)
		{
			weapon = profile == ae_rifle_profile(AE_RIFLE_BATTLE_RIFLE) ? AE_KF_WEAPON_BATTLE_RIFLE :
				profile == ae_rifle_profile(AE_RIFLE_EVO_RIFLE) ? AE_KF_WEAPON_EVO : AE_KF_WEAPON_DMR;
			if (hand != NONE && ae_burst_rifle_profile_of(hand) == profile)
				*weapon_tag = hand;
			return weapon;
		}
		if (hand != NONE)
		{
			short slot = ae_weapon_pack_definition_slot(hand);

			if (slot)
			{
				*weapon_tag = hand;
				return ae_kf_weapon_of_slot(slot);
			}
			if (tag_get_name(hand))
			{
				weapon = ae_kf_weapon_of_path(tag_get_name(hand));
				if (weapon != AE_KF_WEAPON_UNKNOWN && !ae_kf_weapon_has_no_picture(weapon))
					*weapon_tag = hand;
				return weapon;
			}
		}
	}
	return AE_KF_WEAPON_UNKNOWN;
}

/* whether this viewer (not the killer, nor the victim) hurt the victim within the assist window, on the killer's team */
static boolean helped(struct unit_datum const *victim_unit, long viewer_player_index, long killing_player_index,
	long dead_player_index)
{
	struct player_datum const *viewer, *killer;
	short index;

	if (!victim_unit || killing_player_index == NONE || killing_player_index == dead_player_index ||
		viewer_player_index == killing_player_index || viewer_player_index == dead_player_index || !game_engine_has_teams())
		return FALSE;
	viewer = player_try_and_get(viewer_player_index);
	killer = player_try_and_get(killing_player_index);
	if (!viewer || !killer || viewer->team_index != killer->team_index)
		return FALSE;
	for (index = 0; index < MAXIMUM_ATTACKERS_PER_UNIT; index++)
	{
		struct unit_attacker const *attacker = &victim_unit->unit.attackers[index];

		if (attacker->player_index == viewer_player_index &&
			(long)attacker->game_time_stamp > game_time_get() - AE_KF_ASSIST_TICKS)
		{
			return TRUE;
		}
	}
	return FALSE;
}

/* a name's colour as this viewer's own names above heads have it (the game's rule: hud_player_name_color), white where CLASSIC
with ENEMY NAME COLOR CLASSIC would make it the HUD's blue (the feed alone) */
void ae_killfeed_name_color(short local_player_index, long viewer_player_index, long person_player_index, short person_team,
	boolean betrayal, real_argb_color *color)
{
	struct player_datum const *viewer = player_try_and_get(viewer_player_index);
	boolean same_player = (boolean)(person_player_index == viewer_player_index);
	boolean ally = (boolean)(ae_kf_is_ally(game_engine_has_teams(), same_player, viewer && person_team == viewer->team_index) ||
		betrayal);
	short saved = render.local_player_index;

	if (ae_kf_name_white(ae_hud_style_is_ae(), game_engine_enemy_name_red(), ally))
	{
		struct ae_kf_rgb white = ae_kf_white();

		color->alpha = 1.0f;
		color->red = (real)white.r / 255.0f;
		color->green = (real)white.g / 255.0f;
		color->blue = (real)white.b / 255.0f;
		return;
	}
	/* (the rule reads the view being drawn: this player's) */
	render.local_player_index = local_player_index;
	hud_player_name_color(ally, color);
	render.local_player_index = saved;
}

static struct ae_kf_rgb to_rgb(real_argb_color const *color)
{
	struct ae_kf_rgb rgb;

	rgb.r = (unsigned char)(color->red * 255.0f + 0.5f);
	rgb.g = (unsigned char)(color->green * 255.0f + 0.5f);
	rgb.b = (unsigned char)(color->blue * 255.0f + 0.5f);
	return rgb;
}

static char const *weapon_name_for_log(int weapon)
{
	char const *word = ae_kf_weapon_tag(weapon);

	return word ? word : "?";
}

/* ---------- recording */

void ae_killfeed_new_map(void)
{
	short local;

	for (local = 0; local < MAXIMUM_LOCAL_PLAYERS; local++)
		ae_kf_list_reset(&lists[local]);
	blow.set = FALSE;
}

void ae_killfeed_damage(long victim_player_index, long object_index, long definition_index, boolean headshot,
	unsigned long being_damaged_flags)
{
	if (victim_player_index == NONE || !TEST_FLAG(being_damaged_flags, _object_being_damaged_body_depleted_bit))
		return;
	blow.set = TRUE;
	blow.tick = game_time_get();
	blow.victim_player_index = victim_player_index;
	blow.object_index = object_index;
	blow.definition_index = definition_index;
	blow.headshot = headshot;
}

boolean ae_killfeed_replaces_message(short local_player_index, long message)
{
	/* (the medals say the game's multikill and spree lines, in both styles; MEDALS off keeps them) */
	if (ae_kf_message_is_medal_line(message) && ae_config_player_boolean(local_player_index, "display.medals"))
		return TRUE;
	return (boolean)(ae_kf_message_is_kill_line(message) && wanted(local_player_index));
}

boolean ae_killfeed_hides_pickup_text(short local_player_index)
{
	/* (only where the feed draws: a game engine's game; campaign and co-op keep their pickup lines) */
	return (boolean)(game_engine_running() && wanted(local_player_index) && !ae_kf_pickup_text_shown(TRUE));
}

void ae_killfeed_player_killed(long killing_player_index, long dead_player_index, boolean friendly_fire)
{
	struct player_datum const *dead, *killer;
	struct unit_datum const *victim_unit = NULL;
	struct ae_kf_row row;
	boolean have_blow;
	short local;

	if (dead_player_index == NONE)
		return;
	dead = player_try_and_get(dead_player_index);
	if (!dead)
		return;
	killer = killing_player_index != NONE ? player_try_and_get(killing_player_index) : NULL;
	/* (the MODERN medal stack's "Killed" row, for each of the player's own kills: whether or not the feed is on) */
	if (killer && killing_player_index != dead_player_index)
	{
		wchar_t victim_name[AE_KF_NAME];

		copy_name(dead, victim_name);
		for (local = 0; local < MAXIMUM_LOCAL_PLAYERS; local++)
			if (local_player_get_player_index(local) == killing_player_index)
				ae_medal_display_killed(local, dead_player_index, (short)dead->team_index, victim_name, (boolean)(friendly_fire != FALSE));
	}
	if (!anyone_wants())
		return;
	memset(&row, 0, sizeof(row));
	row.weapon_tag = NONE;
	row.killer = NONE;
	row.victim = dead_player_index;
	row.victim_team = (short)dead->team_index;
	copy_name(dead, row.victim_name);
	if (killer && killing_player_index != dead_player_index)
	{
		row.kind = AE_KF_KIND_KILL;
		row.killer = killing_player_index;
		row.killer_team = (short)killer->team_index;
		copy_name(killer, row.killer_name);
		row.betrayal = (boolean)(friendly_fire != FALSE);
	}
	else if (killer)
		row.kind = AE_KF_KIND_SUICIDE;
	else
		row.kind = AE_KF_KIND_ENVIRONMENT;
	have_blow = (boolean)(blow.set && blow.tick == game_time_get() && blow.victim_player_index == dead_player_index);
	if (have_blow)
	{
		row.weapon = classify(blow.definition_index, killing_player_index, &row.weapon_tag);
		row.headshot = blow.headshot;
		victim_unit = unit_try_and_get(blow.object_index);
	}
	/* (what the medals' rules found: the host's alone) */
	row.perfect = (boolean)(row.kind == AE_KF_KIND_KILL && row.headshot && ae_medals_kill_was_perfect(dead_player_index));
	if (!victim_unit)
		victim_unit = unit_try_and_get(dead->dead_unit_index);
	blow.set = FALSE;

	for (local = 0; local < MAXIMUM_LOCAL_PLAYERS; local++)
	{
		long viewer = local_player_get_player_index(local);
		struct ae_kf_row mine = row;

		if (!wanted(local))
			continue;
		mine.mine = (boolean)(viewer == dead_player_index || (row.kind != AE_KF_KIND_ENVIRONMENT && viewer == killing_player_index));
		mine.assist = (boolean)(!mine.mine && !row.betrayal && helped(victim_unit, viewer, killing_player_index, dead_player_index));
		ae_kf_list_add(&lists[local], &mine, game_time_get());
		platform_log("killfeed: view %d: kind %d weapon %s tag %ld headshot %d perfect %d betrayal %d mine %d assist %d (%s)", (int)local,
			row.kind, weapon_name_for_log(row.weapon), row.weapon_tag, row.headshot, row.perfect, row.betrayal, mine.mine,
			mine.assist, have_blow ? "blow seen" : "no blow");
		/* (split screen: the line in this player's message list, the weapon as a word) */
		if (local_player_count() > 1)
		{
			struct ae_kf_split line;
			real_argb_color killer_color, victim_color;
			wchar_t text[96];

			ae_killfeed_name_color(local, viewer, row.killer, row.killer_team, row.betrayal, &killer_color);
			ae_killfeed_name_color(local, viewer, row.victim, row.victim_team, row.betrayal, &victim_color);
			memset(&line, 0, sizeof(line));
			line.killer = row.killer_name;
			line.victim = row.victim_name;
			line.kind = row.kind;
			line.weapon = row.weapon;
			line.headshot = row.headshot;
			line.perfect = row.perfect;
			line.mine = mine.mine;
			line.killer_rgb = to_rgb(&killer_color);
			line.victim_rgb = to_rgb(&victim_color);
			if (ae_kf_split_line(text, (int)NUMBEROF(text), &line))
				hud_print_feed_message(local, text, 0);
		}
	}
}

/* ---------- pictures */

struct picture
{
	struct bitmap_data const *bitmap;
	real_rectangle2d clip;
	int width;
	int height;
};

/* a sequence of a bitmap group's first sprite, checked as the waypoints' are: a map's own sheet may be anything */
static boolean sheet_picture(long group_index, short sequence, struct picture *out)
{
	struct bitmap_group const *group;
	struct bitmap_group_sequence const *entry;
	struct bitmap_data const *bitmap = NULL;
	real_rectangle2d const *clip = NULL;
	int width, height;

	if (group_index == NONE || sequence < 0)
		return FALSE;
	group = bitmap_group_get(group_index);
	if (sequence >= group->sequences.count)
		return FALSE;
	entry = TAG_BLOCK_GET_ELEMENT(&group->sequences, sequence, struct bitmap_group_sequence);
	if (entry->sprites.count < 1)
		return FALSE;
	hud_retrieve_bitmap_and_bounding_rect(group_index, sequence, 0, &bitmap, &clip);
	if (!bitmap || !clip)
		return FALSE;
	if (!ae_waypoint_sprite_size(bitmap->width, bitmap->height, clip->x0, clip->x1, clip->y0, clip->y1, &width, &height))
		return FALSE;
	if (!_texture_cache_bitmap_get_hardware_format((struct bitmap_data *)bitmap, FALSE, TRUE))
		return FALSE;
	out->bitmap = bitmap;
	out->clip = *clip;
	out->width = width;
	out->height = height;
	return TRUE;
}

/* a weapon's picture: the one its pickup prompt has (its HUD interface's messaging icon; a pack weapon's from its own sheet) */
static boolean weapon_picture(long weapon_tag, struct picture *out)
{
	long interface_index;
	struct icon_hud_element_definition const *icon;
	long group_index;

	if (weapon_tag == NONE || !hud_globals || tag_get_group_tag(weapon_tag) != WEAPON_DEFINITION_TAG)
		return FALSE;
	interface_index = weapon_definition_get(weapon_tag)->weapon.interface_definition.hud_interface.index;
	if (interface_index == NONE || tag_get_group_tag(interface_index) != 'wphi')
		return FALSE;
	icon = &weapon_hud_interface_definition_get(interface_index)->messaging_icon;
	if (icon->sequence_index < 0)
		return FALSE;
	group_index = hud_globals->messaging.messaging_icons.index;
	icon = ae_weapon_pack_message_icon(icon, &group_index);
	return icon ? sheet_picture(group_index, icon->sequence_index, out) : FALSE;
}

/* the skull of the Xbox's message icons, only on a map whose HUD has that sheet */
static boolean skull_picture(struct picture *out)
{
	long group_index;
	char const *name;

	if (!hud_globals)
		return FALSE;
	group_index = hud_globals->messaging.messaging_icons.index;
	name = group_index != NONE ? tag_get_name(group_index) : NULL;
	if (!name || _stricmp(name, SKULL_SHEET))
		return FALSE;
	return sheet_picture(group_index, SKULL_SEQUENCE, out);
}

/* ---------- drawing */

static pixel32 premultiplied(struct ae_kf_rgb rgb, real alpha)
{
	real_argb_color color;

	color.alpha = alpha > 0.0f ? (alpha > 1.0f ? 1.0f : alpha) : 0.0f;
	color.red = (real)rgb.r / 255.0f * color.alpha;
	color.green = (real)rgb.g / 255.0f * color.alpha;
	color.blue = (real)rgb.b / 255.0f * color.alpha;
	return real_argb_color_to_pixel32(&color);
}

static void argb(struct ae_kf_rgb rgb, real alpha, real_argb_color *color)
{
	color->alpha = alpha;
	color->red = (real)rgb.r / 255.0f;
	color->green = (real)rgb.g / 255.0f;
	color->blue = (real)rgb.b / 255.0f;
}

/* a picture in ink with a dark outline of a pixel or two round it */
static void draw_picture(struct picture const *picture, real x, real y, real width, real height, struct ae_kf_rgb ink,
	real alpha, real scale)
{
	static real const offsets[4][2] = { { -1.0f, 0.0f }, { 1.0f, 0.0f }, { 0.0f, -1.0f }, { 0.0f, 1.0f } };
	struct ae_kf_rgb dark = { 6, 10, 18 };
	real step = scale < 1.0f ? 1.0f : (real)floor(scale + 0.5f);
	int index;

	x = (real)floor(x + 0.5f);
	y = (real)floor(y + 0.5f);
	for (index = 0; index < 4; index++)
		ae_waypoint_sprite_draw_pixels(picture->bitmap, &picture->clip, x + offsets[index][0] * step, y + offsets[index][1] * step,
			x + width + offsets[index][0] * step, y + height + offsets[index][1] * step, premultiplied(dark, alpha * 0.85f));
	ae_waypoint_sprite_draw_pixels(picture->bitmap, &picture->clip, x, y, x + width, y + height, premultiplied(ink, alpha));
}

/* a rounded rectangle's outline as points (convex, for a fan fill) */
#define PLATE_CORNER_STEPS 3
#define PLATE_POINTS (4 * (PLATE_CORNER_STEPS + 1))

static void plate_points(real left, real top, real width, real height, real radius, real points[PLATE_POINTS][2])
{
	static real const centres[4][2] = { { 1.0f, 0.0f }, { 1.0f, 1.0f }, { 0.0f, 1.0f }, { 0.0f, 0.0f } };
	int corner, step, count = 0;

	if (radius * 2.0f > height)
		radius = height / 2.0f;
	for (corner = 0; corner < 4; corner++)
	{
		real cx = left + (centres[corner][0] > 0.5f ? width - radius : radius);
		real cy = top + (centres[corner][1] > 0.5f ? height - radius : radius);

		for (step = 0; step <= PLATE_CORNER_STEPS; step++)
		{
			real angle = ((real)corner * 90.0f - 90.0f + 90.0f * (real)step / (real)PLATE_CORNER_STEPS) * 3.14159265f / 180.0f;

			points[count][0] = cx + radius * (real)cos(angle);
			points[count][1] = cy + radius * (real)sin(angle);
			count++;
		}
	}
}

/* the plate and tick of the player's own row */
static void draw_backing(struct ae_kf_row const *row, real left, real top, real width, real scale, real alpha)
{
	real points[PLATE_POINTS][2];
	real quad[4][2];
	real tick = AE_KF_TICK_WIDTH * scale < 2.0f ? 2.0f : AE_KF_TICK_WIDTH * scale;
	real height = AE_KF_ROW_HEIGHT * scale;
	real_argb_color color;
	boolean assist = (boolean)!row->mine;

	argb(ae_kf_navy(), (assist ? AE_KF_BACKING_ALPHA_ASSIST : AE_KF_BACKING_ALPHA) * alpha, &color);
	plate_points(left, top, width, height, AE_KF_PLATE_RADIUS * scale, points);
	ae_panel_fill_polygon_soft((real const (*)[2])points, PLATE_POINTS, 0.0f, &color);
	argb(assist ? ae_kf_tick_assist() : ae_kf_tick_mine(), (assist ? AE_KF_TICK_ALPHA_ASSIST : AE_KF_TICK_ALPHA) * alpha, &color);
	{
		real x0 = AE_KF_ALIGN == AE_KF_ALIGN_RIGHT ? left + width - tick : left;

		quad[0][0] = x0; quad[0][1] = top;
		quad[1][0] = x0 + tick; quad[1][1] = top;
		quad[2][0] = x0 + tick; quad[2][1] = top + height;
		quad[3][0] = x0; quad[3][1] = top + height;
		ae_hud_fill_quad_color(quad, &color);
	}
}

static boolean is_ae(void)
{
	return ae_hud_style_is_ae();
}

/* the icons' ink: ice on AE, the HUD's blue on Classic */
static struct ae_kf_rgb ink_rgb(void)
{
	real_argb_color color;

	if (is_ae())
		return ae_kf_ice();
	hud_get_text_color(&color);
	return to_rgb(&color);
}

static void draw_row(short local_player_index, struct ae_kf_row const *row, real left, real top, real alpha, real scale, real px,
	struct ae_kf_layout const *layout, struct picture const *picture, boolean have_picture, real icon_width, real icon_height)
{
	long viewer = local_player_get_player_index(local_player_index);
	real_argb_color color;
	real baseline = top + AE_KF_ROW_HEIGHT * scale / 2.0f + AE_HUD_TEXT_CAP * px / 2.0f;
	real middle = top + AE_KF_ROW_HEIGHT * scale / 2.0f;
	real tag_px = px * AE_KF_TAG_SHARE < AE_KF_TEXT_FLOOR_PX ? AE_KF_TEXT_FLOOR_PX : px * AE_KF_TAG_SHARE;
	struct ae_kf_rgb ink = ink_rgb();
	wchar_t tag[16];
	char const *word;
	int index;

	if (row->mine || row->assist)
		draw_backing(row, left, top, layout->width, scale, alpha);
	if (row->kind == AE_KF_KIND_KILL)
	{
		ae_killfeed_name_color(local_player_index, viewer, row->killer, row->killer_team, row->betrayal, &color);
		color.alpha *= alpha;
		ae_hud_text_draw(local_player_index, (real)floor(left + layout->killer_x + 0.5f), (real)floor(baseline + 0.5f),
			row->killer_name, px, &color, TRUE);
	}
	/* (the picture: the weapon's, the skull of a suicide; where there is none its plain text tag) */
	if (have_picture)
	{
		draw_picture(picture, left + layout->icon_x, middle - icon_height / 2.0f, icon_width, icon_height, ink, alpha, scale);
	}
	else if (row->weapon == AE_KF_WEAPON_FRAG && row->kind != AE_KF_KIND_SUICIDE)
	{
		real height = AE_KF_ICON_HEIGHT * scale * 0.85f;

		ae_panel_draw_glyph(_ae_glyph_frag, left + layout->icon_x, middle - height / 2.0f, height, _ae_tone_blue, alpha);
	}
	else
	{
		word = row->kind == AE_KF_KIND_SUICIDE ? "SELF" : ae_kf_weapon_tag(row->weapon);
		for (index = 0; word[index] && index < 15; index++)
			tag[index] = (wchar_t)(unsigned char)word[index];
		tag[index] = 0;
		argb(ink, alpha, &color);
		ae_hud_text_draw(local_player_index, (real)floor(left + layout->icon_x + 0.5f),
			(real)floor(middle + AE_HUD_TEXT_CAP * tag_px / 2.0f + 0.5f), tag, tag_px, &color, TRUE);
	}
	if (row->kind == AE_KF_KIND_KILL && row->perfect)
	{
		/* (no picture exists for it yet: a plain text tag) */
		wchar_t perfect[] = L"" PERFECT_TAG;

		argb(ink, alpha, &color);
		ae_hud_text_draw(local_player_index, (real)floor(left + layout->mark_x + 0.5f),
			(real)floor(middle + AE_HUD_TEXT_CAP * tag_px / 2.0f + 0.5f), perfect, tag_px, &color, TRUE);
	}
	else if (row->kind == AE_KF_KIND_KILL && row->headshot)
	{
		real size = AE_KF_MARK_SIZE * scale;
		real width = size * 0.11f < 1.5f ? 1.5f : size * 0.11f;

		ae_panel_draw_set_fade(alpha);
		ae_panel_draw_ring(left + layout->mark_x + size / 2.0f, middle, size / 2.0f - width, width, _ae_tone_blue, 1.0f, TRUE);
		ae_panel_draw_disc(left + layout->mark_x + size / 2.0f, middle, size * 0.16f, _ae_tone_blue, 1.0f, TRUE);
		ae_panel_draw_set_fade(1.0f);
	}
	ae_killfeed_name_color(local_player_index, viewer, row->victim, row->victim_team, row->betrayal, &color);
	color.alpha *= alpha;
	ae_hud_text_draw(local_player_index, (real)floor(left + layout->victim_x + 0.5f), (real)floor(baseline + 0.5f), row->victim_name, px,
		&color, TRUE);
}

void ae_killfeed_draw(short local_player_index)
{
	struct ae_kf_list *list;
	real ppu = ae_hud_pixels_per_unit();
	real scale, px, anchor_x, anchor_y;
	short area_left = 0, area_right = 0;
	real sensor_left = 0.0f;
	long now = game_time_get();
	int index;
	boolean hidden;

	if (local_player_index < 0 || local_player_index >= MAXIMUM_LOCAL_PLAYERS)
		return;
	list = &lists[local_player_index];
	/* (the clock goes on while it is not drawn, so a row does not wait for the scoreboard) */
	ae_kf_list_step(list, now);
	if (!wanted(local_player_index) || local_player_count() > 1 || ae_kf_list_count(list) < 1 || !(ppu > 0.0f))
		return;
	hidden = (boolean)(game_engine_scoreboard_shown(local_player_index) > 0.0f || game_engine_scoreboard_held(local_player_index) ||
		(hud_scripted_globals && !hud_scripted_globals->show_hud));
	if (hidden)
		return;
	scale = ae_score_strip_view_scale();
	if (!(scale > 0.0f))
		return;
	px = ae_kf_text_px(scale);
	hud_area_insets(&area_left, &area_right);
	/* (the anchor: the tick a little left of the sensor disc's left edge (either HUD style's), the bottom a stage line that the player's
	medal style gives) */
	anchor_x = ae_kf_anchor_x(ae_sensor_disc_left(local_player_index, &sensor_left), sensor_left, (real)area_left * ppu, scale);
	anchor_y = ae_kf_anchor_y(ae_medal_style_is_mcc(local_player_index), scale);
	for (index = 0; index < AE_KF_ROWS; index++)
	{
		struct ae_kf_row const *row = &list->rows[index];
		struct ae_kf_parts parts;
		struct ae_kf_layout layout;
		struct picture picture;
		boolean have_picture = FALSE;
		real slot, alpha, slide, icon_width, icon_height, tag_px, left, top;
		wchar_t word[16];
		int letter;
		char const *tag;

		if (!row->used || !ae_kf_row_pose(row, now, &slot, &alpha, &slide))
			continue;
		memset(&parts, 0, sizeof(parts));
		parts.scale = scale;
		tag_px = px * AE_KF_TAG_SHARE < AE_KF_TEXT_FLOOR_PX ? AE_KF_TEXT_FLOOR_PX : px * AE_KF_TAG_SHARE;
		parts.has_killer = (boolean)(row->kind == AE_KF_KIND_KILL);
		parts.killer_width = parts.has_killer ? ae_hud_text_measure(row->killer_name, px) : 0.0f;
		parts.victim_width = ae_hud_text_measure(row->victim_name, px);
		/* (the picture, and its size fitted into the icon box) */
		if (row->kind == AE_KF_KIND_SUICIDE)
			have_picture = skull_picture(&picture);
		else if (row->weapon_tag != NONE && !ae_kf_weapon_has_no_picture(row->weapon))
			have_picture = weapon_picture(row->weapon_tag, &picture);
		if (have_picture && ae_kf_icon_fit((real)picture.width, (real)picture.height, scale, &icon_width, &icon_height))
		{
			parts.icon_width = icon_width;
		}
		else
		{
			have_picture = FALSE;
			icon_width = icon_height = 0.0f;
			if (row->weapon == AE_KF_WEAPON_FRAG && row->kind != AE_KF_KIND_SUICIDE)
				parts.icon_width = ae_panel_glyph_width(_ae_glyph_frag, AE_KF_ICON_HEIGHT * scale * 0.85f);
			else
			{
				tag = row->kind == AE_KF_KIND_SUICIDE ? "SELF" : ae_kf_weapon_tag(row->weapon);
				for (letter = 0; tag[letter] && letter < 15; letter++)
					word[letter] = (wchar_t)(unsigned char)tag[letter];
				word[letter] = 0;
				parts.icon_width = ae_hud_text_measure(word, tag_px);
			}
		}
		parts.has_mark = (boolean)(row->kind == AE_KF_KIND_KILL && (row->headshot || row->perfect));
		if (row->kind == AE_KF_KIND_KILL && row->perfect)
		{
			wchar_t perfect[] = L"" PERFECT_TAG;

			parts.mark_width = ae_hud_text_measure(perfect, tag_px);
		}
		else
			parts.mark_width = AE_KF_MARK_SIZE * scale;
		ae_kf_layout(&parts, &layout);
		left = ae_kf_row_left(anchor_x, layout.width);
		top = ae_kf_slot_top(anchor_y, slot, scale) + (AE_KF_GROWS_UP ? slide : -slide) * scale;
		draw_row(local_player_index, row, left, top, alpha, scale, px, &layout, &picture, have_picture, icon_width, icon_height);
	}
}
