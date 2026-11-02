/*
AE_MARK_PROMPT.C

The aim prompt (ae_mark_prompt.h). It is drawn in the view's screen pixels (x from the view's left, y from its top) as the wheel
is. The target's box is projected as ae_marks_draw places a marker: the point to the view's space, to the screen, and out when it
is past the marks' ellipse. Its height is the object's bounding sphere's. The names over heads of this view's last frame
(hud_item_timers.c notes them) and the game's text rows (ae_hud_messages.c notes them) are what it keeps clear of.
*/

#include <math.h>
#include <string.h>
#include <wchar.h>

#include "cseries/cseries.h"
#include "math/real_math.h"
#include "memory/data.h"
#include "cutscene/cinematics.h"
#include "game/game.h"
#include "game/game_engine.h"
#include "game/players.h"
#include "interface/player_ui.h"
#include "objects/objects.h"
#include "objects/object_types.h"
#include "render/render.h"
#include "render/render_camera_projection.h"
#include "saved games/player_profile.h"
#include "text/draw_string.h"
#include "text/font_group.h"

#include "ae_hud_digits.h"
#include "ae_hud_messages.h"
#include "ae_hud_text.h"
#include "ae_key_prompts.h"
#include "ae_mark_calls.h"
#include "ae_mark_prompt.h"
#include "ae_mark_prompt_rules.h"
#include "ae_mark_rules.h"
#include "ae_mark_targets.h"
#include "ae_marks.h"
#include "ae_panel_draw.h"
#include "ae_pause.h"
#include "ae_score_strip.h"
#include "ae_wheel.h"

void platform_video_drawable_size(int *width, int *height);
long hud_item_timers_ae_font_index(void);
real hud_item_timers_ae_waypoint_scale(real world_units);
short hud_item_timers_ae_name_tags(short local_player_index, rectangle2d *bounds, short maximum);
boolean game_engine_scoreboard_held(short local_player_index);
real game_engine_scoreboard_shown(short local_player_index);
/* port/linux/src/ae_config_players.c: each player's own (split screen) */
int ae_config_player_boolean(int player, char const *name);

/* the ellipse a mark stays inside, of the view's size, and a marker's glyph: ae_marks.c's */
#define PROMPT_ELLIPSE_X 0.4375f
#define PROMPT_ELLIPSE_Y 0.315f
#define PROMPT_GLYPH_SIZE 36.0f
#define PROMPT_GLYPH_SMALLEST 9.0f
#define PROMPT_ROW_GAP 1.0f
#define PROMPT_PIN_TIP 0.46875f
/* a biped's box is this wide, of its height */
#define PROMPT_BIPED_WIDTH 0.55f
#define PROMPT_TAGS_MOST 32
#define PROMPT_PLAYERS 4

static boolean block_takes[PROMPT_PLAYERS];

/* ---------- what is aimed at, as the world has it */

/* where the target is: a sphere (an object's bounding sphere; radius 0 for a point), checked just now */
struct target
{
	real_point3d centre;
	real radius;
	boolean biped;
	boolean vehicle;
	/* a marker: its glyph's size and the rows over it are the marks' */
	boolean marker;
	short marker_kind;
};

static boolean player_alive(short local_player_index)
{
	long player_index = local_player_get_player_index(local_player_index);
	struct player_datum *player = player_index == NONE ? NULL : player_try_and_get(player_index);
	struct object_datum *unit;

	if (!player || player->unit_index == NONE)
		return FALSE;
	unit = object_try_and_get_and_verify_type(player->unit_index, _object_mask_unit);
	return unit && !TEST_FLAG(unit->object.damage_flags, _object_dead_bit);
}

/* the object the aim named, if it is still there (the aim is up to 2 ticks old): its indices are checked before they are read */
static boolean target_object(long object_index, unsigned long types, boolean enemy_biped, struct target *target)
{
	struct object_datum *object;

	if (object_index == NONE || (object = object_try_and_get_and_verify_type(object_index, types)) == NULL)
		return FALSE;
	if (enemy_biped ? TEST_FLAG(object->object.damage_flags, _object_dead_bit) : object->object.parent_object_index != NONE)
		return FALSE;
	target->centre = object->object.bounding_sphere_center;
	target->radius = object->object.bounding_sphere_radius;
	target->biped = enemy_biped && TEST_FLAG(_object_mask_biped, object->object.type);
	target->vehicle = (boolean)TEST_FLAG(_object_mask_vehicle, object->object.type);
	return TRUE;
}

static void target_point(real_point3d const *spot, struct target *target)
{
	target->centre = *spot;
	target->radius = 0.0f;
}

/* what the aim is on now, FALSE when it is gone */
static boolean target_get(struct ae_mark_aim const *aim, struct target *target)
{
	csmemset(target, 0, sizeof(*target));
	switch (aim->tier)
	{
	case AE_TIER_ENEMY:
		return target_object(aim->mark.object_index, _object_mask_biped | _object_mask_vehicle, TRUE, target);
	case AE_TIER_MARKER:
	{
		struct ae_mark_detail detail;
		real_point3d spot;

		if (aim->reply_to < 0 || !ae_marks_get_detail(aim->reply_to, &detail))
			return FALSE;
		spot.x = detail.x;
		spot.y = detail.y;
		spot.z = detail.z;
		target_point(&spot, target);
		target->marker = TRUE;
		target->marker_kind = (short)detail.kind;
		return TRUE;
	}
	case AE_TIER_NAMED:
	case AE_TIER_PICKUP:
		if (aim->mark.kind == AE_MARK_KIND_ITEM)
		{
			return target_object(aim->mark.object_index, _object_mask_weapon | _object_mask_equipment | _object_mask_vehicle,
				FALSE, target);
		}
		/* (a hill, a teleporter, a bomb site: a place) */
		target_point(&aim->mark.spot, target);
		return TRUE;
	case AE_TIER_BODY:
	case AE_TIER_GROUND:
		target_point(&aim->mark.spot, target);
		return TRUE;
	}
	return FALSE;
}

/* the item's, objective's or place's own word, for the rules */
static wchar_t const *aim_name(struct ae_mark_aim const *aim)
{
	if (aim->mark.kind == AE_MARK_KIND_ITEM)
		return aim->mark.name[0] ? aim->mark.name : NULL;
	if (aim->mark.kind == AE_MARK_KIND_PLACE && aim->mark.name[0])
		return aim->mark.name;
	switch (aim->mark.word)
	{
	case MARK_WORD_HILL: return L"HILL";
	case MARK_WORD_TELEPORTER: return L"TELEPORTER";
	case MARK_WORD_BOMB_SITE: return L"BOMB SITE";
	}
	return NULL;
}

/* ---------- whether and what */

boolean ae_mark_prompt_enabled(short local_player_index)
{
	struct player_profile profile;

	if (local_player_index < 0 || local_player_index >= PROMPT_PLAYERS)
		return FALSE;
	if (!ae_config_player_boolean(local_player_index, "display.mark_prompt"))
		return FALSE;
	/* (the game's hints: the profile's IN-GAME HELP) */
	player_ui_get_active_player_profile(local_player_index, &profile);
	return (boolean)!profile.controller_settings.ingame_help_disabled;
}

/* the aim and its word when a prompt is to show; the target's world place with them */
static boolean prompt_get(short local_player_index, struct ae_mark_aim const **aim, struct target *target,
	wchar_t const **word)
{
	int entry;
	boolean dead;
	real flash, alpha;

	if (local_player_index < 0 || local_player_index >= PROMPT_PLAYERS || !game_engine_running() || cinematic_in_progress() ||
		!ae_mark_prompt_enabled(local_player_index))
	{
		return FALSE;
	}
	if (game_engine_scoreboard_shown(local_player_index) > 0.0f || game_engine_scoreboard_held(local_player_index) ||
		ae_pause_player_open(local_player_index))
	{
		return FALSE;
	}
	/* (the ring's own closing too: its flash and fade are not shared) */
	if (ae_wheel_open(local_player_index) || ae_wheel_closing(local_player_index, &entry, &dead, &flash, &alpha))
		return FALSE;
	if (!player_alive(local_player_index))
		return FALSE;
	*aim = ae_marks_aim(local_player_index);
	if (!*aim || !(*aim)->found || !target_get(*aim, target))
		return FALSE;
	*word = ae_prompt_word((*aim)->tier, (*aim)->verb, aim_name(*aim));
	return *word != NULL;
}

boolean ae_mark_prompt_row(short local_player_index, wchar_t *word, int size)
{
	struct ae_mark_aim const *aim;
	struct target target;
	wchar_t const *text;
	int index;

	if (!word || size < 1)
		return FALSE;
	word[0] = 0;
	if (!prompt_get(local_player_index, &aim, &target, &text))
		return FALSE;
	for (index = 0; index < size - 1 && text[index]; index++)
		word[index] = text[index];
	word[index] = 0;
	return TRUE;
}

void ae_mark_prompt_block_takes(short local_player_index, boolean takes)
{
	if (local_player_index >= 0 && local_player_index < PROMPT_PLAYERS)
		block_takes[local_player_index] = takes;
}

/* ---------- where it is on the screen */

struct view
{
	real width, height;		/* in screen pixels */
	real ppu;				/* pixels to a HUD unit */
	real scale;				/* s */
	boolean split;
};

static boolean view_get(struct view *view)
{
	view->ppu = ae_hud_pixels_per_unit();
	if (view->ppu <= 0.0f)
		return FALSE;
	view->width = (real)(render.camera.viewport_bounds.x1 - render.camera.viewport_bounds.x0) * view->ppu;
	view->height = (real)(render.camera.viewport_bounds.y1 - render.camera.viewport_bounds.y0) * view->ppu;
	view->split = (boolean)(local_player_count() > 1);
	view->scale = ae_score_strip_view_scale();
	return view->width > 0.0f && view->height > 0.0f && view->scale > 0.0f;
}

/* the point on the screen, in the HUD's units from the view's top left, as ae_marks_draw finds a marker's: FALSE off the
screen or past the marks' ellipse (no prompt out there). view_position is the point in the view's space */
static boolean project(real_point3d const *point, real *x, real *y, real_point3d *view_position)
{
	real_point2d screen;
	real width = (real)(render.camera.viewport_bounds.x1 - render.camera.viewport_bounds.x0);
	real height = (real)(render.camera.viewport_bounds.y1 - render.camera.viewport_bounds.y0);
	real across, down;

	matrix4x3_transform_point(&render.frustum.world_to_view, point, view_position);
	if (!render_camera_view_to_screen(&render.camera, &render.frustum, view_position, &screen))
		return FALSE;
	across = screen.x - (real)render.camera.viewport_bounds.x0 - width * 0.5f;
	down = screen.y - (real)render.camera.viewport_bounds.y0 - height * 0.5f;
	if (ae_mark_clamp(&across, &down, width * PROMPT_ELLIPSE_X, height * PROMPT_ELLIPSE_Y, FALSE))
		return FALSE;
	*x = across + width * 0.5f;
	*y = down + height * 0.5f;
	return TRUE;
}

/* how far a sphere's edge reaches on the screen, in HUD units: a point of the view's space moved along the screen's up and
across, as far as the radius */
static real project_reach(real_point3d const *view_position, real radius, real x, real y)
{
	real_point3d edge;
	real_point2d screen;
	real range = 0.0f;

	edge = *view_position;
	edge.y += radius;
	if (render_camera_view_to_screen(&render.camera, &render.frustum, &edge, &screen))
		range = (real)fabs(screen.y - (real)render.camera.viewport_bounds.y0 - y);
	edge = *view_position;
	edge.x += radius;
	if (render_camera_view_to_screen(&render.camera, &render.frustum, &edge, &screen))
	{
		real across = (real)fabs(screen.x - (real)render.camera.viewport_bounds.x0 - x);

		if (across > range)
			range = across;
	}
	return range;
}

/* the target's box in the view's pixels: an object's by its bounding sphere (a biped's narrower), a marker's by the glyph
and the rows it stands under, a point a point. FALSE where it is not on the screen */
static boolean target_box(struct target const *target, struct view const *view, struct ae_prompt_box *box)
{
	real_point3d view_position;
	real x, y, half_height, half_width;

	if (!project(&target->centre, &x, &y, &view_position))
		return FALSE;
	if (target->marker)
	{
		/* (ae_marks.c's glyph, and the two rows it keeps above itself until its block draws the prompt) */
		struct font_header *font;
		real text_scale, size, middle, line_height = 0.0f;
		long font_index = hud_item_timers_ae_font_index();

		text_scale = hud_item_timers_ae_waypoint_scale(distance3d(&render.camera.position, &target->centre));
		size = PROMPT_GLYPH_SIZE * text_scale;
		if (size < PROMPT_GLYPH_SMALLEST)
			size = PROMPT_GLYPH_SMALLEST;
		middle = target->marker_kind == AE_MARK_KIND_PLACE ? y - size * PROMPT_PIN_TIP : y;
		font = font_index != NONE ? font_definition_get(font_index) : NULL;
		if (font)
			line_height = (real)(font->ascending_height + font->descending_height);
		box->x0 = (x - size / 2.0f) * view->ppu;
		box->x1 = (x + size / 2.0f) * view->ppu;
		box->y1 = (middle + size / 2.0f) * view->ppu;
		box->y0 = (middle - size / 2.0f - 2.0f * (PROMPT_ROW_GAP + line_height * text_scale)) * view->ppu;
		return TRUE;
	}
	if (target->radius > 0.0f)
	{
		half_height = project_reach(&view_position, target->radius, x, y);
		half_width = target->biped ? half_height * PROMPT_BIPED_WIDTH : half_height;
	}
	else
	{
		half_height = 0.0f;
		half_width = 0.0f;
	}
	box->x0 = (x - half_width) * view->ppu;
	box->x1 = (x + half_width) * view->ppu;
	box->y0 = (y - half_height) * view->ppu;
	box->y1 = (y + half_height) * view->ppu;
	return TRUE;
}

/* the names over heads this view drew in its last frame that stand over the target, where the prompt would go: how many, and
the one when there is one. A vehicle with a name over it carries riders, a name and a triangle each: a pile */
static int tags_over(short local_player_index, struct view const *view, struct ae_prompt_in const *in, struct target const *target,
	struct ae_prompt_box *one)
{
	rectangle2d tags[PROMPT_TAGS_MOST];
	struct ae_prompt_box region;
	real gap = AE_PROMPT_GAP * view->scale;
	short count = hud_item_timers_ae_name_tags(local_player_index, tags, PROMPT_TAGS_MOST);
	short index;
	int over = 0;

	region.x0 = in->target.x0 - gap;
	region.x1 = in->target.x1 + gap;
	region.y0 = in->target.y0 - (in->height + 4.0f * gap);
	region.y1 = in->target.y1;
	for (index = 0; index < count; index++)
	{
		struct ae_prompt_box tag;

		tag.x0 = (real)tags[index].x0 * view->ppu;
		tag.x1 = (real)tags[index].x1 * view->ppu;
		tag.y0 = (real)tags[index].y0 * view->ppu;
		tag.y1 = (real)tags[index].y1 * view->ppu;
		if (tag.x0 < region.x1 && region.x0 < tag.x1 && tag.y0 < region.y1 && region.y0 < tag.y1)
		{
			*one = tag;
			over++;
		}
	}
	return over == 1 && target->vehicle ? 2 : over;
}

/* ---------- the draw */

/* the capitals' middle of a text at y, on whole pixels, as the wheel's */
static void text_left(short view, wchar_t const *text, real x, real middle, real px, real const rgb[3], enum ae_hud_text_edge edge)
{
	real_argb_color color;

	color.alpha = 1.0f;
	color.red = rgb[0];
	color.green = rgb[1];
	color.blue = rgb[2];
	ae_hud_text_draw_ex(view, (real)floor(x + 0.5f), (real)floor(middle + AE_HUD_TEXT_CAP * px / 2.0f + 0.5f), text, px, &color,
		edge);
}

void ae_mark_prompt_draw(short local_player_index)
{
	boolean ground;
	struct ae_mark_aim const *aim;
	struct target target;
	struct view view;
	struct ae_prompt_metrics metrics;
	struct ae_prompt_in in;
	wchar_t const *word;
	real rgb[3];
	real drawable_height_scale = 1.0f;
	real chip_width = 0.0f, word_width, middle;
	real boxes[AE_HUD_MESSAGE_ROWS_MOST][4];
	float x, y;
	int drawable_width = 0, drawable_height = 0;
	short count, index;
	boolean taken;

	if (local_player_index < 0 || local_player_index >= PROMPT_PLAYERS)
		return;
	/* (a marker's block that drew the row says so for this frame only) */
	taken = block_takes[local_player_index];
	block_takes[local_player_index] = FALSE;
	if (taken || !prompt_get(local_player_index, &aim, &target, &word) || !view_get(&view))
		return;
	platform_video_drawable_size(&drawable_width, &drawable_height);
	if (drawable_height > 0)
		drawable_height_scale = (real)drawable_height / 1080.0f;
	ae_prompt_metrics_get(view.split, drawable_height_scale, &metrics);
	/* (no picture of the button, no prompt: a word alone would not say what to press) */
	if (!ae_key_prompts_chip_width(local_player_index, AE_CHIP_MARK, metrics.chip_height, &chip_width) || chip_width <= 0.0f)
		return;
	word_width = ae_hud_text_measure(word, metrics.word_px) + 2.0f;

	memset(&in, 0, sizeof(in));
	in.view_width = view.width;
	in.view_height = view.height;
	in.scale = view.scale;
	in.width = (real)ceil(2.0f * metrics.halo + chip_width + metrics.chip_gap + word_width);
	in.height = metrics.chip_height + 2.0f * metrics.halo;
	/* (the ground is where the reticle is: its point, up to 3 ticks old in a fast turn, may project anywhere, even behind the view) */
	ground = aim->tier == AE_TIER_GROUND;
	if (!target_box(&target, &view, &in.target))
	{
		if (!ground)
			return;
		in.target.x0 = in.target.x1 = view.width / 2.0f;
		in.target.y0 = in.target.y1 = view.height / 2.0f;
	}
	in.tags = tags_over(local_player_index, &view, &in, &target, &in.tag);
	if (VALID_INDEX(local_player_index, MAXIMUM_LOCAL_PLAYERS))
	{
		static int sides[MAXIMUM_LOCAL_PLAYERS];

		sides[local_player_index] = ae_prompt_side_keep((in.target.x0 + in.target.x1) / 2.0f, view.width / 2.0f, view.scale,
			sides[local_player_index]);
		in.side = sides[local_player_index];
	}
	count = ae_hud_message_rows(local_player_index, boxes, AE_HUD_MESSAGE_ROWS_MOST);
	for (index = 0; index < count; index++)
	{
		in.rows[index].x0 = boxes[index][0];
		in.rows[index].y0 = boxes[index][1];
		in.rows[index].x1 = boxes[index][2];
		in.rows[index].y1 = boxes[index][3];
	}
	in.row_count = count;
	{
		static int pins[MAXIMUM_LOCAL_PLAYERS];
		int pinned = 0;

		if (VALID_INDEX(local_player_index, MAXIMUM_LOCAL_PLAYERS))
		{
			pins[local_player_index] = ae_prompt_pin_keep((in.target.x0 + in.target.x1) / 2.0f, (in.target.y0 + in.target.y1) / 2.0f,
				view.width / 2.0f, view.height / 2.0f, view.scale, in.height, pins[local_player_index]);
			pinned = pins[local_player_index];
		}
		if (ground)
			pinned = 1;
		if ((pinned ? ae_prompt_place_pinned(&in, &x, &y) : ae_prompt_place(&in, &x, &y)) == AE_PROMPT_NONE)
			return;
	}
	middle = (real)floor(y + in.height / 2.0f + 0.5f);
	{
		real drawn = 0.0f;

		if (!ae_key_prompts_draw_chip(local_player_index, AE_CHIP_MARK, x + metrics.halo, middle, metrics.chip_height, 1.0f,
			&drawn))
		{
			return;
		}
		ae_panel_tone_rgb(ae_prompt_word_is_enemy(aim->tier, aim->verb) ? _ae_tone_red : _ae_tone_white, rgb);
		text_left(local_player_index, word, x + metrics.halo + drawn + metrics.chip_gap, middle, metrics.word_px, rgb,
			_ae_text_outline);
	}
}
