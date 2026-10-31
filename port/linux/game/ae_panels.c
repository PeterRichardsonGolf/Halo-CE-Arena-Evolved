/*
AE_PANELS.C

HUD STYLE AE's ammo + grenade group and shield + health group (ae_panels.h;
spec 14.15).
*/

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "cseries/cseries.h"
#include "cseries/cseries_windows.h"
#include "math/real_math.h"
#include "game/game_engine.h"
#include "game/players.h"
#include "render/render.h"
#include "rasterizer/rasterizer.h"

#include "ae_hud_digits.h"
#include "ae_hud_messages.h"
#include "ae_hud_style.h"
#include "ae_hud_text.h"
#include "ae_panel_draw.h"
#include "ae_panel_engine.h"
#include "ae_panel_layout.h"
#include "ae_panel_model.h"
#include "ae_panels.h"
#include "ae_power_list.h"
#include "ae_score_strip.h"
#include "ae_sensor.h"

long hud_unit_port_script_flags(void);
const char *config_string(const char *name);
int config_boolean(const char *name);
unsigned long config_changes(void);
void platform_log(char const *format, ...);
long game_time_get(void);

#define AE_HUD_PANEL_HEALTH_DONT_SHOW 0     /* hud_unit.c's _hud_panel_health_dont_show_bit */
#define AE_HUD_PANEL_HEALTH_BLINK 1         /* _hud_panel_health_blink_bit (hud_blink_health) */
#define AE_HUD_PANEL_SHIELD_DONT_SHOW 2     /* _hud_panel_shield_dont_show_bit */
#define AE_HUD_PANEL_SHIELD_BLINK 3         /* _hud_panel_shield_blink_bit (hud_blink_shield) */
#define BACKING_PEAK 0.32f
#define BOX_FILL 0.08f
#define UNARMED_GLYPH 0.55f
#define UNARMED_COUNT 0.70f
#define UNARMED_COUNT_QUARTER 0.85f
#define AE_SHIELD_BLINK_DIM 0.35f   /* the shield's red on the blink's off beat */

static boolean hiding;
static long held_back;      /* CE's quads held back since the last AE draw (the log's "held") */
static long faded_labels;   /* waypoint labels faded under a panel since the last log line */
static struct { boolean valid, ammo_drawn; struct ae_panel_rect rects[2]; short count; real scale;
	unsigned long drawn_at; }
	last[MAXIMUM_LOCAL_PLAYERS];

/* (the last draw's rectangles, only while the panels are being drawn: a view whose panels are not drawn any more (its
player dead or watching, the HUD hidden or CLASSIC, another game) does not lay CE's messages and labels out round an
old one; a quarter second covers a frame's order and a slow frame) */
#define LAST_DRAW_MILLISECONDS 250
static boolean last_valid(short local_player_index)
{
	return (boolean)(last[local_player_index].valid &&
		system_milliseconds() - last[local_player_index].drawn_at < LAST_DRAW_MILLISECONDS);
}

void ae_panels_hide_ce(boolean hide)
{
	hiding = (boolean)(hide && ae_hud_style_is_ae());
}

/* (the weapon HUD's zoomed elements, hud_draw.c's hud_zoomed_layout_begin to _end: the sniper's range and elevation
numbers, angle ticks and caption show under AE's HUD too, owner 2026-10-17) */
static int zoom_elements;

void ae_panels_zoom_element(boolean begin)
{
	zoom_elements += begin ? 1 : -1;
	if (zoom_elements < 0)
		zoom_elements = 0;
}

boolean ae_panels_zoomed_number_kept(short state_type)
{
	return (boolean)(!ae_hud_style_is_ae() || ae_panel_zoomed_number_is_readout(state_type));
}

boolean ae_panels_hiding_ce(void)
{
	if (!hiding || zoom_elements)
		return FALSE;
	held_back++;
	return TRUE;
}

/* debug.ae_panel_state: states forced over the game's for the tests and the contact sheets, a comma list of: low,
empty, overheated, heat:<0..1>, grenades:<frags>/<plasmas>, plasma (armed), shield_low, shield_down, overshield1,
overshield2, camo, buffs (both rows, the overshield's in its last 5 s), health_low, trail (a hit's trail, held),
script_blink (the scripts' health / shield / sensor blink bits on) */
/* (read twice a view a frame: copied again only when a setting changed) */
static char const *panel_state(void)
{
	static unsigned long read_at = (unsigned long)-1;
	static char state[256];

	if (read_at != config_changes())
	{
		char const *value = config_string("debug.ae_panel_state");

		read_at = config_changes();
		snprintf(state, sizeof(state), "%s", value ? value : "");
	}
	return state;
}

static void forced_state(struct ae_panel_snapshot *s)
{
	char const *state = panel_state();
	char const *walk;

	if (!state || !state[0])
		return;
	for (walk = state; walk && *walk; walk = strchr(walk, ','), walk = walk ? walk + 1 : NULL)
	{
		size_t length = strcspn(walk, ",");
		int frags = 0, plasmas = 0;
		double heat = 0.0;

#define IS(word) (length == strlen(word) && !strncmp(walk, word, length))
		if (IS("low") && s->rounds_per_magazine > 0)
		{
			s->rounds_loaded = (short)(s->rounds_per_magazine / 6);
			s->low_ammo_flash_on = (boolean)((game_time_get() / 15) % 2 == 0);
		}
		else if (IS("empty"))
			s->rounds_loaded = 0;
		else if (IS("overheated"))
		{
			s->heat = 1.0f;
			s->overheated = TRUE;
		}
		else if (!strncmp(walk, "heat:", 5) && sscanf(walk + 5, "%lf", &heat) == 1)
			s->heat = (real)heat;
		else if (!strncmp(walk, "grenades:", 9) && sscanf(walk + 9, "%d/%d", &frags, &plasmas) == 2)
		{
			s->grenades[0] = (short)frags;
			s->grenades[1] = (short)plasmas;
		}
		else if (IS("plasma"))
			s->armed_grenade = 1;
		else if (IS("trail"))
		{
			/* (a hit from full to 0.6, its trail held) */
			s->shield = 0.6f;
			s->shield_trail = 1.0f;
			s->shield_trail_alpha = 1.0f;
		}
		else if (IS("shield_low"))
		{
			s->shield = 0.2f;
			s->shield_low = TRUE;
		}
		else if (IS("shield_down"))
		{
			s->shield = 0.0f;
			s->shield_down = TRUE;
		}
		else if (IS("overshield1"))
		{
			s->shield = 1.6f;
			s->overshield_seconds = 44.2f;
		}
		else if (IS("overshield2"))
		{
			s->shield = 2.6f;
			s->overshield_seconds = 81.0f;
		}
		else if (IS("camo"))
			s->camo_ticks = 37 * 30;
		else if (IS("buffs"))
		{
			s->shield = 1.05f;
			s->overshield_seconds = 4.2f;
			s->camo_ticks = 23 * 30;
		}
		else if (IS("health_low"))
		{
			s->health = 0.2f;
			s->health_low = TRUE;
		}
#undef IS
	}
}

/* debug.ae_panel_state's "script_blink": the scripts' blink bits forced on (health, shield, sensor) */
boolean ae_panels_forced_script_blink(void)
{
	char const *state = panel_state();
	char const *walk;

	for (walk = state; walk && *walk; walk = strchr(walk, ','), walk = walk ? walk + 1 : NULL)
		if (strcspn(walk, ",") == strlen("script_blink") && !strncmp(walk, "script_blink", strlen("script_blink")))
			return TRUE;
	return FALSE;
}

static void log_layout(short view, const struct ae_panel_model *m, const struct ae_panel_layout *l)
{
	static long logged[MAXIMUM_LOCAL_PLAYERS] = { -1, -1, -1, -1 };
	long tick = game_time_get();
	char const *state;

	if (!ae_panel_engine_log_on() || tick < 0 || tick / 60 == logged[view])
		return;
	state = panel_state();
	logged[view] = tick / 60;
	platform_log("ae panels: tick %ld view %d state=%s number_box=%.1f,%.1f,%.1f,%.1f grenade_box=%.1f,%.1f,%.1f,%.1f "
		"baseline=%.1f cap=%.1f rounds=%d round0=%.1f,%.1f,%.1f lit=%d red=%d bar=%d,%.1f,%.1f,%.1f,%.1f,%d "
		"shield_bar=%.1f,%.1f,%.1f,%.1f icon=%.1f,%.1f,%.1f cell0=%.1f,%.1f,%.1f,%.1f cross=%.0f,%.0f,%.0f "
		"buffs=%d buff0=%.1f,%.1f,%.1f shield_red=%d held=%ld faded=%ld", tick / 60 * 60, view,
		state && state[0] ? state : "none", l->number_box.x0, l->number_box.y0, l->number_box.x1, l->number_box.y1,
		l->grenade_box.x0, l->grenade_box.y0, l->grenade_box.x1, l->grenade_box.y1, l->digit_baseline_y, l->digit_cap,
		l->round_count, l->round_count ? l->round[0].x : 0.0f, l->round_count ? l->round[0].y : 0.0f,
		l->round_count ? l->round[0].height : 0.0f, m->lit, m->rounds_red, l->bar_shown, l->bar.x0, l->bar.y0,
		l->bar.x1, l->bar.y1, m->bar_red, l->shield_bar.x0, l->shield_bar.y0, l->shield_bar.x1, l->shield_bar.y1,
		l->shield_icon_x, l->shield_icon_y, l->shield_icon_height, l->health_cell[0].x0, l->health_cell[0].y0,
		l->health_cell[7].x1, l->health_cell[0].y1, l->cross_x, l->cross_y, l->cross_size, l->buff_count,
		l->buff_count ? l->buff[0].time_right_x : 0.0f, l->buff_count ? l->buff[0].baseline_y : 0.0f,
		l->buff_count ? l->buff[0].px : 0.0f, m->shield_red, held_back, faded_labels);
	faded_labels = 0;
}

void ae_panels_rectangles(short local_player_index, struct ae_panel_rect out[2], short *count)
{
	*count = 0;
	if (local_player_index < 0 || local_player_index >= MAXIMUM_LOCAL_PLAYERS || !last_valid(local_player_index))
		return;
	memcpy(out, last[local_player_index].rects, sizeof(last[local_player_index].rects));
	*count = last[local_player_index].count;
}

static void tone(short which, real alpha, real_argb_color *color)
{
	real rgb[3];

	ae_panel_tone_rgb(which, rgb);
	color->alpha = alpha;
	color->red = rgb[0];
	color->green = rgb[1];
	color->blue = rgb[2];
}

static void backing(short view, const struct ae_panel_rect *r)
{
	ae_hud_backing_draw(view, (r->x0 + r->x1) / 2.0f, (r->y0 + r->y1) / 2.0f, (r->x1 - r->x0) / 2.0f,
		(r->y1 - r->y0) / 2.0f, BACKING_PEAK);
}

/* CE's counter digits in the panels' fixed cells with the strip's glow (or a zero count's outline) */
static void digits(short view, real x, real baseline, const wchar_t *text, const struct ae_panel_layout *l,
	real alpha, boolean outline)
{
	struct ae_hud_digits_options options;
	real_argb_color white;

	memset(&options, 0, sizeof(options));
	options.style = outline ? _ae_digits_outline : _ae_digits_glow;   /* the strip's digits (owner 10-15) */
	options.glow_alpha = 0.85f;
	options.cell_px = l->digit_cell;
	options.fill_alpha = alpha;
	tone(_ae_tone_white, 1.0f, &white);
	ae_hud_digits_draw_ex(view, x, baseline, text, l->digit_cap, &white, &options);
}

/* a label in the panels' face: Overpass 900 with the strip's outline, right-aligned when right_x */
static void label(short view, real x, real baseline, const wchar_t *text, real px, short which, boolean right)
{
	real_argb_color color;

	tone(which, 1.0f, &color);
	if (right)
		x -= ae_hud_text_measure(text, px);
	ae_hud_text_draw_ex(view, x, baseline, text, px, &color, _ae_text_outline);
}

static void draw_seats(short view, const struct ae_panel_model *m, const struct ae_panel_layout *l, real s)
{
	short i;

	for (i = 0; i < l->seat_count; i++)
	{
		if (l->seat[i].shell)
			ae_panel_draw_art(_ae_art_rocket, l->seat[i].shell_x, l->seat[i].shell_y, l->seat[i].shell_height,
				(boolean)(m->seat[i].fill > 0.0f), _ae_tone_blue);
		else
			ae_panel_draw_bar(&l->seat[i].bar, s, m->seat[i].fill, m->seat[i].red ? _ae_tone_red : _ae_tone_blue,
				m->seat[i].red ? _ae_tone_red : _ae_tone_blue, 0.0f, 0.0f);
		label(view, l->seat[i].label_x, l->seat[i].label_baseline_y, m->seat[i].label, l->seat[i].label_px,
			_ae_tone_grey, FALSE);
	}
}

static void draw_ammo(short view, const struct ae_panel_model *m, const struct ae_panel_layout *l, real s)
{
	short i;

	if (!l->ammo_group_shown)
		return;
	backing(view, &l->backing_ammo);
	if (m->weapon_kind == _ae_panel_seat)
	{
		draw_seats(view, m, l, s);
		return;
	}
	ae_panel_draw_box(&l->number_box, s, BOX_FILL, _ae_tone_blue);
	ae_panel_draw_box(&l->grenade_box, s, BOX_FILL, _ae_tone_blue);
	if (l->number_shown)
		digits(view, l->number_cell_x[0], l->digit_baseline_y, m->number, l, 1.0f, FALSE);
	if (l->x_mark_shown)
		ae_panel_draw_x(l->x_mark_x, l->digit_baseline_y, l->x_mark_size, s, _ae_tone_blue);
	if (l->glyph_shown)
		ae_panel_draw_glyph(m->glyph, l->glyph_x, l->glyph_y, l->glyph_height, _ae_tone_blue, 1.0f);

	/* the grenades: the armed pair full white; the other's glyph at 0.55 and count at 0.70 (0.85 in quarter views),
	keylines always full (G2); a zero count as an outline */
	if (m->grenade[0].shown)
	{
		real count_alpha = s < 0.8f ? UNARMED_COUNT_QUARTER : UNARMED_COUNT;

		for (i = 0; i < 2; i++)
		{
			boolean armed = m->grenade[i].armed;

			if (i == 0)
				ae_panel_draw_glyph(_ae_glyph_frag, l->frag_x, l->frag_y, l->frag_height, _ae_tone_white,
					armed ? 1.0f : UNARMED_GLYPH);
			else
				ae_panel_draw_plasma(l->plasma_cx, l->plasma_cy, l->plasma_diameter, _ae_tone_white,
					armed ? 1.0f : UNARMED_GLYPH);
			digits(view, l->count_ink_x[i], l->digit_baseline_y, m->grenade[i].count, l,
				armed ? 1.0f : count_alpha, m->grenade[i].zero);
		}
	}

	/* (the loaded rounds in the shield bar's light blue, full strength, light keyline: ae_panel_draw_art; owner 10-15) */
	for (i = 0; i < l->round_count; i++)
		ae_panel_draw_art(m->art, l->round[i].x, l->round[i].y, l->round[i].height, l->round[i].lit,
			m->rounds_red ? _ae_tone_red : _ae_tone_blue);
	if (l->bar_shown)
		ae_panel_draw_bar(&l->bar, s, m->bar_fill, m->bar_red ? _ae_tone_red : _ae_tone_blue,
			m->bar_red ? _ae_tone_red : _ae_tone_blue, 0.0f, 0.0f);
}

static void draw_shield(short view, const struct ae_panel_model *m, const struct ae_panel_layout *l, real s,
	boolean shield_shown, boolean health_shown, real shield_blink, real health_blink)
{
	short i;

	if (!shield_shown && !health_shown && !l->buff_count)
		return;
	backing(view, &l->backing_shield);
	if (shield_shown)
	{
		/* low or down: red, and on the blink's off beat the same red dimmed (never the normal blue: owner 10-15); the
		scripts' blink (hud_blink_shield) dims it on the same beat */
		real fade = (m->shield_hidden_blink ? AE_SHIELD_BLINK_DIM : 1.0f) * shield_blink;

		ae_panel_draw_bar_faded(&l->shield_bar, s, m->shield_fill, m->shield_red ? _ae_tone_red : _ae_tone_blue,
			m->shield_red ? _ae_tone_red : _ae_tone_blue, m->overshield1, m->overshield2, fade, m->shield_trail,
			m->shield_trail_alpha);
		ae_panel_draw_glyph(_ae_glyph_shield, l->shield_icon_x, l->shield_icon_y, l->shield_icon_height,
			m->shield_red ? _ae_tone_red : _ae_tone_blue, fade);
	}
	if (health_shown)
	{
		/* (the scripts' blink, hud_blink_health: the cells and the cross dimmed on the beat) */
		ae_panel_draw_set_fade(health_blink);
		for (i = 0; i < AE_PANEL_HEALTH_CELLS; i++)
			ae_panel_draw_cell(&l->health_cell[i], (boolean)(i < m->health_cells_lit),
				m->health_red ? _ae_tone_red : _ae_tone_blue);
		ae_panel_draw_cross(l->cross_x, l->cross_y, l->cross_size, m->health_red ? _ae_tone_red : _ae_tone_blue);
		ae_panel_draw_set_fade(1.0f);
	}
	/* own-buff rows: the time white, right-aligned on the right column (red in its last 5 s); the name white 8 px
	left of the "0:00" column */
	for (i = 0; i < l->buff_count; i++)
	{
		label(view, l->buff[i].time_right_x, l->buff[i].baseline_y, m->buff[i].time, l->buff[i].px,
			m->buff[i].time_red ? _ae_tone_red : _ae_tone_white, TRUE);
		label(view, l->buff[i].name_right_x, l->buff[i].baseline_y, m->buff[i].name, l->buff[i].px, _ae_tone_white,
			TRUE);
	}
}

boolean ae_panels_draw(short local_player_index)
{
	struct ae_panel_snapshot snapshot;
	struct ae_panel_model model;
	struct ae_panel_view view;
	struct ae_panel_metrics metrics;
	static struct ae_panel_layout layout;
	long flags = hud_unit_port_script_flags();
	long player_index;
	boolean shield_shown, health_shown;
	real shield_blink, health_blink;

	if (local_player_index < 0 || local_player_index >= MAXIMUM_LOCAL_PLAYERS)
		return FALSE;
	last[local_player_index].valid = FALSE;
	if (!ae_hud_style_is_ae() || !ae_panel_engine_snapshot(local_player_index, &snapshot))
		return FALSE;
	player_index = local_player_get_player_index(local_player_index);
	forced_state(&snapshot);
	ae_panel_model_build(&snapshot, &model);
	ae_panel_engine_view(local_player_index, &view);
	ae_panel_metrics_ce(&metrics);
	metrics.digits_ink = ae_hud_digits_ink_bounds;
	metrics.text_width = ae_hud_text_measure;
	ae_panel_layout_build(&model, &metrics, &view, &layout);

	/* the scripts' and the gametype's show flags, as hud_render_unit_interface obeys them (hud_unit.c:963, 1089) */
	shield_shown = (boolean)(game_engine_has_shield(player_index) && !TEST_FLAG(flags, AE_HUD_PANEL_SHIELD_DONT_SHOW));
	health_shown = (boolean)!TEST_FLAG(flags, AE_HUD_PANEL_HEALTH_DONT_SHOW);
	/* and their blink bits (hud_blink_health / hud_blink_shield: a10's health and shield lessons; hud_unit.c flashes
	CE's meters from them), or debug.ae_panel_state's "script_blink" for the pictures */
	shield_blink = ae_panel_script_blink_fade((boolean)(TEST_FLAG(flags, AE_HUD_PANEL_SHIELD_BLINK) ||
		ae_panels_forced_script_blink()), game_time_get(), snapshot.reduced_motion);
	health_blink = ae_panel_script_blink_fade((boolean)(TEST_FLAG(flags, AE_HUD_PANEL_HEALTH_BLINK) ||
		ae_panels_forced_script_blink()), game_time_get(), snapshot.reduced_motion);

	draw_ammo(local_player_index, &model, &layout, view.scale);
	draw_shield(local_player_index, &model, &layout, view.scale, shield_shown, health_shown, shield_blink,
		health_blink);

	/* (the groups drawn, the ammo group first: the messages are laid out under it; the shield group only when
	something is drawn there, so that labels do not fade over an empty corner) */
	last[local_player_index].count = 0;
	last[local_player_index].ammo_drawn = layout.ammo_group_shown;
	if (layout.ammo_group_shown)
		last[local_player_index].rects[last[local_player_index].count++] = layout.ammo_group;
	if (shield_shown || health_shown || layout.buff_count)
		last[local_player_index].rects[last[local_player_index].count++] = layout.shield_group;
	last[local_player_index].scale = view.scale;
	last[local_player_index].valid = TRUE;
	last[local_player_index].drawn_at = system_milliseconds();
	log_layout(local_player_index, &model, &layout);
	held_back = 0;
	return TRUE;
}

#define LABEL_FADE 0.35f
#define LABEL_FADE_GROW 8.0f

static boolean overlaps(real ax0, real ay0, real ax1, real ay1, const real b[4], real grow)
{
	return (boolean)(ax1 > b[0] - grow && ax0 < b[2] + grow && ay1 > b[1] - grow && ay0 < b[3] + grow);
}

real ae_panels_label_alpha(short local_player_index, const union rectangle2d *bounds)
{
	real ppu = ae_hud_pixels_per_unit();
	real x0, y0, x1, y1, r[4];
	short i;

	if (!ae_hud_style_is_ae() || local_player_index < 0 || local_player_index >= MAXIMUM_LOCAL_PLAYERS || !bounds)
		return 1.0f;
	x0 = (real)bounds->x0 * ppu; y0 = (real)bounds->y0 * ppu;
	x1 = (real)bounds->x1 * ppu; y1 = (real)bounds->y1 * ppu;
	if (last_valid(local_player_index))
		for (i = 0; i < last[local_player_index].count; i++)
		{
			r[0] = last[local_player_index].rects[i].x0; r[1] = last[local_player_index].rects[i].y0;
			r[2] = last[local_player_index].rects[i].x1; r[3] = last[local_player_index].rects[i].y1;
			if (overlaps(x0, y0, x1, y1, r, LABEL_FADE_GROW))
			{
				faded_labels++;
				return LABEL_FADE;
			}
		}
	if (ae_sensor_rectangle(local_player_index, &r[0], &r[1], &r[2], &r[3]) && overlaps(x0, y0, x1, y1, r, LABEL_FADE_GROW))
	{
		faded_labels++;
		return LABEL_FADE;
	}
	if (ae_power_list_extent(local_player_index, r) && overlaps(x0, y0, x1, y1, r, LABEL_FADE_GROW))
	{
		faded_labels++;
		return LABEL_FADE;
	}
	if (ae_score_strip_extent(local_player_index, r) && overlaps(x0, y0, x1, y1, r, LABEL_FADE_GROW))
	{
		faded_labels++;
		return LABEL_FADE;
	}
	return 1.0f;
}

#define MESSAGE_GAP 4.0f    /* px x the view's scale from the ammo group's foot to the (scaled) line's top: the line
                               is pulled up under the group (owner 10-15: "move up closer to the counter") */

#define MESSAGE_SCALE 0.60f   /* CE's message lines under AE when AE's text cannot draw them: scaled to the panels (owner
                                 10-15: 0.72 still too big) */
#define MESSAGE_CAP_GAP 6.0f  /* px x scale from the ammo group's foot to the first restyled line's capitals */
#define MESSAGE_DROP 16.0f    /* px x scale more for every line under the group (owner 2026-10-18, the key prompts: "same
                                 drop always"): the tallest picture on a prompt line, a weapon's, keeps 8 px clear under
                                 the bar's outline, where the X chip touched it */
static boolean message_scaled;
static boolean message_under_group[MAXIMUM_LOCAL_PLAYERS]; /* message_top moved the lines under the ammo group */

static boolean group_drawn(short local_player_index)
{
	return (boolean)(last_valid(local_player_index) && last[local_player_index].ammo_drawn);
}

void ae_panels_message_scale_begin(short local_player_index, short x, short y)
{
	real tx, ty, ppu = ae_hud_pixels_per_unit();
	short width, height;
	rectangle2d visible;

	message_scaled = FALSE;
	if (!ae_hud_style_is_ae() || local_player_index < 0 || local_player_index >= MAXIMUM_LOCAL_PLAYERS)
		return;
	width = (short)(render.camera.viewport_bounds.x1 - render.camera.viewport_bounds.x0);
	height = (short)(render.camera.viewport_bounds.y1 - render.camera.viewport_bounds.y0);

	/* the restyle (owner 10-15, "full restyle"): CE's lines handed to AE's text in the strip's style
	(ae_hud_messages.c), from the ammo group's left edge, the first line's capitals 6 px x scale under its foot */
	if (ppu > 0.0f)
	{
		boolean group = group_drawn(local_player_index);
		real scale = group ? last[local_player_index].scale : ae_score_strip_view_scale();
		real left = group ? last[local_player_index].rects[0].x0 : (real)x * ppu;
		real cap_top = group && message_under_group[local_player_index] ?
			last[local_player_index].rects[0].y1 + (MESSAGE_CAP_GAP + MESSAGE_DROP) * scale : (real)y * ppu;
		real group_width = group ? last[local_player_index].rects[0].x1 - last[local_player_index].rects[0].x0 : 0.0f;

		if (ae_hud_messages_begin(local_player_index, left, cap_top, (real)width * ppu, group_width, scale))
			return;
	}

	/* (the fallback: CE's own drawing smaller, through the screen transform (rasterizer.h), about the lines'
	top-left, which stays put; the clip is the part of the drawing's space that lands in the view) */
	tx = (real)x * (1.0f - MESSAGE_SCALE);
	ty = (real)y * (1.0f - MESSAGE_SCALE);
	visible.x0 = (short)floorf(-tx / MESSAGE_SCALE);
	visible.y0 = (short)floorf(-ty / MESSAGE_SCALE);
	visible.x1 = (short)ceilf(((real)width - tx) / MESSAGE_SCALE);
	visible.y1 = (short)ceilf(((real)height - ty) / MESSAGE_SCALE);
	rasterizer_screen_transform_set(MESSAGE_SCALE, tx, ty, &visible);
	message_scaled = TRUE;
}

void ae_panels_message_scale_end(void)
{
	ae_hud_messages_end();
	if (message_scaled)
		rasterizer_screen_transform_reset();
	message_scaled = FALSE;
}

short ae_panels_message_left(short local_player_index, short x)
{
	real ppu = ae_hud_pixels_per_unit();

	if (!ae_hud_style_is_ae() || local_player_index < 0 || local_player_index >= MAXIMUM_LOCAL_PLAYERS ||
		!group_drawn(local_player_index) || ppu <= 0.0f)
	{
		return x;
	}
	return (short)(last[local_player_index].rects[0].x0 / ppu + 0.5f);
}

short ae_panels_message_top(short local_player_index, short line_top)
{
	real ppu = ae_hud_pixels_per_unit();
	real bottom;

	if (local_player_index >= 0 && local_player_index < MAXIMUM_LOCAL_PLAYERS)
		message_under_group[local_player_index] = FALSE;
	if (!ae_hud_style_is_ae() || local_player_index < 0 || local_player_index >= MAXIMUM_LOCAL_PLAYERS ||
		!group_drawn(local_player_index) || ppu <= 0.0f)
	{
		return line_top;
	}
	/* (the ammo group is the first rectangle when it is drawn) */
	bottom = (last[local_player_index].rects[0].y1 + (MESSAGE_GAP + MESSAGE_DROP) * last[local_player_index].scale) / ppu;
	if ((real)line_top * ppu < last[local_player_index].rects[0].y0)
		return line_top;
	message_under_group[local_player_index] = TRUE;
	return (short)(bottom + 0.5f);
}
