/*
CINEMATICS.C

symbols in this file:
000813F0 0040:
	_cinematic_initialize (0000)
00081430 0010:
	_cinematic_dispose (0000)
00081440 0030:
	_cinematic_initialize_for_new_map (0000)
00081470 0020:
	_cinematic_dispose_from_old_map (0000)
00081490 0040:
	_cinematic_start (0000)
000814D0 0010:
	_cinematic_can_be_skipped (0000)
000814E0 0010:
	_cinematic_skip_start (0000)
000814F0 0010:
	_cinematic_skip_stop (0000)
00081500 0030:
	_cinematic_show_letterbox (0000)
00081530 0150:
	_draw_quad (0000)
00081680 0020:
	_cinematic_force_title (0000)
000816A0 0020:
	_cinematic_suppress_bsp_object_creation (0000)
000816C0 0050:
	_cinematic_stop (0000)
00081710 0010:
	_cinematic_in_progress (0000)
00081720 0090:
	_cinematic_set_title_delayed (0000)
000817B0 0500:
	_cinematic_render (0000)
00081CB0 0020:
	_cinematic_set_title (0000)
002589FC 0012:
	??_C@_0BC@NGABDHNK@cinematic_globals?$AA@ (0000)
00258A10 0025:
	??_C@_0CF@PIGNLDBH@c?3?2halo?2SOURCE?2cutscene?2cinemati@ (0000)
00258A38 0012:
	??_C@_0BC@PJBLDLNO@cinematic?5globals?$AA@ (0000)
00258A4C 0032:
	??_C@_0DC@GMMMKPOA@no?5free?5chapter?5title?5slots?5to?5d@ (0000)
00258A80 0004:
	__real@3e000000 (0000)
00435CA0 0004:
	_cinematic_globals (0000)
*/

/* ---------- headers */

#include "cseries.h"
#include "cseries/errors.h"
#include "cinematics.h"
#include "ai/ai.h"
#include "bitmaps/bitmaps.h"
#include "bitmaps/bitmap_group.h"
#include "editor/editor_stubs.h"
#include "game/game.h"
#include "game/game_engine.h"
#include "game/game_globals.h"
#include "game/players.h"
#include "hs/hs.h"
#include "interface/hud.h"
#include "interface/hud_definitions.h"
#include "interface/ui_widget.h"
#include "items/projectiles.h"
#include "rasterizer/rasterizer.h"
#include "rasterizer/rasterizer_cinematics.h"
#include "render/render.h"
#include "saved games/game_state.h"
#include "scenario/scenario.h"
#include "scenario/scenario_definitions.h"
#include "text/draw_string.h"
#include "text/text_group.h"
#include "network_coop.h" /* port: port/linux/game/network_coop.c */

/* ---------- constants */

enum
{
	MAXIMUM_QUEUED_CINEMATIC_TITLES = 4,
};

/* ---------- macros */

/* ---------- structures */

struct scenario_cutscene_title
{
	long flags;
	char name[TAG_STRING_LENGTH+1];
	long pad24;
	rectangle2d bounds;
	short text_index;
	word style;
	word justification;
	word pad36;
	unsigned long text_flags;
	pixel32 foreground_color;
	pixel32 shadow_color;
	real fade_in_time;
	real up_time;
	real fade_out_time;
	byte unused50[0x10];
};

typedef char verify_scenario_cutscene_title_size[
	sizeof(struct scenario_cutscene_title) == 0x60 ? 1 : -1];
typedef char verify_scenario_cutscene_title_name_offset[
	offsetof(struct scenario_cutscene_title, name) == 0x04 ? 1 : -1];
typedef char verify_scenario_cutscene_title_bounds_offset[
	offsetof(struct scenario_cutscene_title, bounds) == 0x28 ? 1 : -1];
typedef char verify_scenario_cutscene_title_fade_offset[
	offsetof(struct scenario_cutscene_title, fade_in_time) == 0x44 ? 1 : -1];
typedef char verify_hud_global_single_player_font_offset[
	offsetof(struct hud_globals_definition, messaging.single_player_font.index) == 0x54 ? 1 : -1];
typedef char verify_hud_global_default_title_bounds_offset[
	offsetof(struct hud_globals_definition, defaults.default_title_bounds) == 0x2DC ? 1 : -1];

/* ---------- prototypes */

/* ---------- globals */

struct cinematic_global_data *cinematic_globals = NULL;

/* port: where NHE's clock (its titles, shown with MATCH CLOCK off) was last
drawn: a rectangle round each line of each of its titles as the text's
layout put it (in split screen a title repeats itself after blank lines,
for the lower view), the screen's coordinates; and the game time then. The
HUD's power column stacks itself above it where they would meet
(hud_item_timers.c, cinematic_nhe_clock_bounds). Kept a second after the
last title is drawn, so that the column does not jump between them */
#define CINEMATIC_NHE_CLOCK_MAXIMUM_LINES 16
static struct
{
	short count;
	rectangle2d lines[CINEMATIC_NHE_CLOCK_MAXIMUM_LINES];
	long game_time;
	/* (this frame's, while the titles are drawn) */
	short drawn_count;
	rectangle2d drawn_lines[CINEMATIC_NHE_CLOCK_MAXIMUM_LINES];
} cinematic_nhe_clock = { 0 };

/* ---------- public code */

void cinematic_initialize(
	void)
{
	cinematic_globals = game_state_malloc(
		"cinematic globals",
		NULL,
		sizeof(*cinematic_globals));
	match_assert(
		"c:\\halo\\SOURCE\\cutscene\\cinematics.c",
		24,
		cinematic_globals);

	return;
}

void cinematic_dispose(
	void)
{
	return;
}

void cinematic_initialize_for_new_map(
	void)
{
	csmemset(cinematic_globals, 0, sizeof(*cinematic_globals));
	csmemset(
		cinematic_globals->queued_titles,
		NONE,
		sizeof(cinematic_globals->queued_titles));
	/* port: (no NHE clock seen on this map yet) */
	csmemset(&cinematic_nhe_clock, 0, sizeof(cinematic_nhe_clock));

	return;
}

void cinematic_skip_start(
	void)
{
	cinematic_globals->can_be_skipped = TRUE;

	return;
}

void cinematic_skip_stop(
	void)
{
	cinematic_globals->can_be_skipped = FALSE;

	return;
}

void cinematic_dispose_from_old_map(
	void)
{
	cinematic_globals->show_letterbox = FALSE;
	cinematic_globals->in_progress = FALSE;

	return;
}

void cinematic_start(
	void)
{
	player_input_enable(FALSE);
	ai_globals_dialogue_triggers_enabled(FALSE);
	cinematic_globals->show_letterbox = TRUE;
	cinematic_globals->letterbox_last_game_time = game_time_get();
	cinematic_globals->in_progress = TRUE;
	projectiles_delete_all();

	return;
}

boolean cinematic_can_be_skipped(
	void)
{
	return cinematic_globals->can_be_skipped;
}

void cinematic_show_letterbox(
	boolean show)
{
	cinematic_globals->show_letterbox = show;
	if (show)
	{
		cinematic_globals->letterbox_last_game_time = game_time_get();
	}

	return;
}

void cinematic_force_title(
	unsigned short title_index)
{
	cinematic_globals->queued_titles[0].title_index = title_index;
	cinematic_globals->queued_titles[0].time = 0;

	return;
}

void cinematic_suppress_bsp_object_creation(
	boolean suppress)
{
	cinematic_globals->suppress_bsp_object_creation = suppress;

	return;
}

boolean cinematic_in_progress(
	void)
{
	return cinematic_globals->in_progress;
}

void cinematic_stop(
	void)
{
	cinematic_globals->show_letterbox = FALSE;
	player_input_enable(TRUE);
	ai_globals_dialogue_triggers_enabled(TRUE);
	cinematic_globals->in_progress = FALSE;
	rasterizer_screen_effects_initialize_for_new_map();
	if (global_rasterizer_model_ambient_reflection_tint)
	{
		csmemset(
			global_rasterizer_model_ambient_reflection_tint,
			0,
			sizeof(*global_rasterizer_model_ambient_reflection_tint));
	}
	rasterizer_set_near_clip_distance(0.0f);
	display_errors_deferred_until_cinematic_stop();

	return;
}

/* port: whether a title is one of Halo 1: NHE's maps' clock (their scripts
set it each second from ui\hud\hudtimer: s_* its seconds, t_* its tens,
m_* its minutes; not g_*, its countdown) */
static boolean cinematic_title_is_nhe_clock(
	struct scenario_cutscene_title const *title)
{
	return hs_scenario_is_nhe() &&
		(title->name[0] == 's' || title->name[0] == 't' || title->name[0] == 'm') &&
		title->name[1] == '_';
}

/* port: whether a title is NHE's clock, which with MATCH CLOCK on is not
shown: the HUD has the engine's own (hud_item_timers.c). Its Cortana sounds
are the scripts' own and stay */
static boolean cinematic_title_is_hidden_nhe_clock(
	short title_index)
{
	struct scenario_cutscene_title *title;

	if (!game_engine_running() || !hs_scenario_is_nhe() ||
		game_engine_match_clock_setting() == _match_clock_off ||
		title_index < 0 || title_index >= global_scenario_get()->cutscene_chapter_titles.count)
	{
		return FALSE;
	}
	title = TAG_BLOCK_GET_ELEMENT(
		&global_scenario_get()->cutscene_chapter_titles,
		title_index,
		struct scenario_cutscene_title);

	return cinematic_title_is_nhe_clock(title);
}

/* the lines of an NHE clock title's text just drawn in bounds (with the
draw mode it was drawn in), noted as cinematic_nhe_clock's */
static void cinematic_nhe_clock_note(
	rectangle2d const *bounds,
	wchar_t const *text)
{
	wchar_t prefix[128];
	short index;
	short line_start = 0;

	for (index = 0; index < (short)NUMBEROF(prefix) - 1; index++)
	{
		wchar_t character = text[index];

		if (character == L'\n' || !character)
		{
			short at;
			short end = index;
			boolean printed = FALSE;

			/* (a "\r\n" line's text ends before its "\r", which the layout
			takes as a break of its own) */
			while (end > line_start && text[end - 1] == L'\r')
				end--;
			for (at = line_start; at < end; at++)
				printed |= text[at] != L' ' && text[at] != L'\t';
			if (printed && cinematic_nhe_clock.drawn_count < CINEMATIC_NHE_CLOCK_MAXIMUM_LINES)
			{
				wchar_t line[128];
				rectangle2d text_bounds;
				rectangle2d cursor_bounds;
				rectangle2d *noted = &cinematic_nhe_clock.drawn_lines[cinematic_nhe_clock.drawn_count];

				/* (its height: where the text up to its end leaves the
				cursor's line; its sides: the line alone, justified in the
				same bounds) */
				csmemcpy(prefix, text, end * sizeof(wchar_t));
				prefix[end] = 0;
				draw_unicode_string_compute_bounds(bounds, prefix, &text_bounds, &cursor_bounds);
				noted->y0 = (short)(cursor_bounds.y0 + render.camera.viewport_bounds.y0);
				noted->y1 = (short)(cursor_bounds.y1 + render.camera.viewport_bounds.y0);
				csmemcpy(line, text + line_start, (end - line_start) * sizeof(wchar_t));
				line[end - line_start] = 0;
				draw_unicode_string_compute_bounds(bounds, line, &text_bounds, &cursor_bounds);
				if (text_bounds.x1 > text_bounds.x0)
				{
					noted->x0 = (short)(text_bounds.x0 + render.camera.viewport_bounds.x0);
					noted->x1 = (short)(text_bounds.x1 + render.camera.viewport_bounds.x0);
					cinematic_nhe_clock.drawn_count++;
				}
			}
			line_start = (short)(index + 1);
		}
		if (!character)
			break;
	}
}

/* port: where NHE's clock shows in an area (a view: the screen's
coordinates), the union of its lines there; FALSE where it does not */
boolean cinematic_nhe_clock_bounds(
	rectangle2d const *area,
	rectangle2d *bounds)
{
	short index;
	boolean found = FALSE;

	if (!game_engine_running() || !hs_scenario_is_nhe() || !cinematic_nhe_clock.count ||
		game_time_get() < cinematic_nhe_clock.game_time ||
		game_time_get() - cinematic_nhe_clock.game_time > TICKS_PER_SECOND)
	{
		return FALSE;
	}
	for (index = 0; index < cinematic_nhe_clock.count; index++)
	{
		rectangle2d const *line = &cinematic_nhe_clock.lines[index];

		if (line->x0 >= area->x1 || line->x1 <= area->x0 || line->y0 >= area->y1 || line->y1 <= area->y0)
			continue;
		if (!found)
		{
			*bounds = *line;
			found = TRUE;
		}
		else
		{
			bounds->x0 = MIN(bounds->x0, line->x0);
			bounds->y0 = MIN(bounds->y0, line->y0);
			bounds->x1 = MAX(bounds->x1, line->x1);
			bounds->y1 = MAX(bounds->y1, line->y1);
		}
	}

	return found;
}

/* port: whether one of Halo 1: NHE's maps' countdown titles (g_*: their
scripts' 3, 2, 1 at a game's start) is queued or showing, so that MATCH
CLOCK's corner keeps out of it (hud_item_timers.c) */
boolean cinematic_nhe_countdown_title_showing(
	void)
{
	short title_slot_index;

	if (!game_engine_running() || !hs_scenario_is_nhe() || !cinematic_globals)
		return FALSE;
	for (title_slot_index = 0; title_slot_index < MAXIMUM_QUEUED_CINEMATIC_TITLES; title_slot_index++)
	{
		short title_index = cinematic_globals->queued_titles[title_slot_index].title_index;
		struct scenario_cutscene_title *title;

		if (title_index < 0 || title_index >= global_scenario_get()->cutscene_chapter_titles.count)
			continue;
		title = TAG_BLOCK_GET_ELEMENT(
			&global_scenario_get()->cutscene_chapter_titles,
			title_index,
			struct scenario_cutscene_title);
		if (title->name[0] == 'g' && title->name[1] == '_')
			return TRUE;
	}

	return FALSE;
}

void cinematic_set_title_delayed(
	short title_index,
	real delay)
{
	short title_slot_index;

	/* port: (not queued: no slot taken) */
	if (cinematic_title_is_hidden_nhe_clock(title_index))
		return;

	network_coop_note_title(title_index, delay);
	for (title_slot_index = 0;
		title_slot_index < MAXIMUM_QUEUED_CINEMATIC_TITLES &&
		cinematic_globals->queued_titles[title_slot_index].title_index != NONE;
		title_slot_index++);

	if (title_slot_index < MAXIMUM_QUEUED_CINEMATIC_TITLES)
	{
		cinematic_globals->queued_titles[title_slot_index].title_index = title_index;
		cinematic_globals->queued_titles[title_slot_index].time =
			(short)-fast_ftol(delay * TICKS_PER_SECOND);
	}
	else
	{
		struct scenario_cutscene_title *title = TAG_BLOCK_GET_ELEMENT(
			&global_scenario_get()->cutscene_chapter_titles,
			title_index,
			struct scenario_cutscene_title);
		error(
			_error_silent,
			"no free chapter title slots to display title '%s'",
			title->name);
	}

	return;
}

void cinematic_render(
	void)
{
	if ((cinematic_globals->show_letterbox ||
		cinematic_globals->letterbox_amount > 0.0f) &&
		!ui_widgets_active())
	{
		long game_time;
		long elapsed_ticks;
		real letterbox_amount;

		game_time = game_time_get();
		elapsed_ticks =
			game_time - cinematic_globals->letterbox_last_game_time;
		cinematic_globals->letterbox_last_game_time = game_time;
		if (cinematic_globals->show_letterbox)
		{
			cinematic_globals->letterbox_amount +=
				(real)elapsed_ticks * (1.0f / (real)TICKS_PER_SECOND);
			letterbox_amount =
				MIN(cinematic_globals->letterbox_amount, 1.0f);
		}
		else
		{
			cinematic_globals->letterbox_amount =
				cinematic_globals->letterbox_amount -
				(real)elapsed_ticks * (1.0f / (real)TICKS_PER_SECOND);
			letterbox_amount =
				MAX(cinematic_globals->letterbox_amount, 0.0f);
		}

		cinematic_globals->letterbox_amount = letterbox_amount;

		if (cinematic_globals->letterbox_amount > 0.0f)
		{
			rectangle2d bar;
			real bar_height;
			real viewport_height;

			bar_height = cinematic_globals->letterbox_amount * 0.125f;
			viewport_height = (real)(
				render.camera.viewport_bounds.y1 -
				render.camera.viewport_bounds.y0);

			bar.x0 = (short)fast_ftol((real)render.camera.viewport_bounds.x0);
			bar.x1 = (short)fast_ftol((real)render.camera.viewport_bounds.x1);
			bar.y0 = (short)fast_ftol((real)render.camera.viewport_bounds.y0);
			bar_height *= viewport_height;
			bar.y1 = (short)fast_ftol(
				(real)render.camera.viewport_bounds.y0 + bar_height);
			draw_quad(&bar, 0xFF000000);

			bar.x0 = (short)fast_ftol((real)render.camera.viewport_bounds.x0);
			bar.x1 = (short)fast_ftol((real)render.camera.viewport_bounds.x1);
			bar.y0 = (short)fast_ftol(
				(real)render.camera.viewport_bounds.y1 - bar_height);
			bar.y1 = (short)fast_ftol((real)render.camera.viewport_bounds.y1);
			draw_quad(&bar, 0xFF000000);
		}
	}

	{
		short title_slot_index;

		/* port: (NHE's clock's lines drawn this frame) */
		cinematic_nhe_clock.drawn_count = 0;
		for (title_slot_index = 0;
			title_slot_index < MAXIMUM_QUEUED_CINEMATIC_TITLES;
			title_slot_index++)
		{
			struct cinematic_title *active_title;
			struct scenario_cutscene_title *title;
			struct string_list *string_list;
			rectangle2d const *title_bounds;
			long font_index;
			long help_text_tag_index;
			real fade_amount;

			active_title =
				&cinematic_globals->queued_titles[title_slot_index];

			if (active_title->title_index == NONE)
				continue;

			/* port: none once a multiplayer game is over: titles fade by
			game time, which stops then, so a map's titles (Halo 1:
			NHE's clock) stayed over the postgame carnage report */
			if (game_engine_running() && game_engine_showing_postgame())
			{
				active_title->title_index = NONE;
				active_title->time = NONE;
				continue;
			}
			/* port: (one queued before MATCH CLOCK was turned on) */
			if (cinematic_title_is_hidden_nhe_clock(active_title->title_index))
			{
				active_title->title_index = NONE;
				active_title->time = NONE;
				continue;
			}

			font_index = hud_globals->messaging.single_player_font.index;
			if (font_index == NONE)
				continue;

			title = TAG_BLOCK_GET_ELEMENT(
				&global_scenario_get()->cutscene_chapter_titles,
				active_title->title_index,
				struct scenario_cutscene_title);

			help_text_tag_index =
				global_scenario_get()->ingame_help_text.index;
			if (help_text_tag_index == NONE)
				continue;

			string_list =
				unicode_string_list_definition_get(help_text_tag_index);
			if (title->text_index < 0 ||
				title->text_index >= string_list->strings.count)
			{
				continue;
			}

			title_bounds = &title->bounds;
			fade_amount = 1.0f;
			if (title_bounds->x1 == title_bounds->x0 ||
				title_bounds->y1 == title_bounds->y0)
			{
				title_bounds = &hud_globals->defaults.default_title_bounds;
			}

			if (!game_in_editor())
			{
				real title_time = (real)active_title->time;

				if (title_time < title->fade_in_time)
				{
					fade_amount =
						title_time / title->fade_in_time;
				}
				else if (title_time > title->up_time)
				{
					fade_amount =
						1.0f -
						(title_time - title->up_time) /
						title->fade_out_time;
				}

				fade_amount = PIN(fade_amount, 0.0f, 1.0f);
			}

			{
				real_argb_color text_color;
				long shadow_alpha;

				pixel32_to_real_argb_color(
					title->foreground_color,
					&text_color);
				text_color.alpha *= fade_amount;

				if (fabs(text_color.red - 1.0f) < _real_epsilon &&
					fabs(text_color.green - 1.0f) < _real_epsilon &&
					fabs(text_color.blue - 1.0f) < _real_epsilon)
				{
					text_color.red = MIN(text_color.red, 0.8f);
					text_color.green = MIN(text_color.green, 0.8f);
					text_color.blue = MIN(text_color.blue, 0.8f);
				}

				draw_string_set_draw_mode(
					font_index,
					title->style - 1,
					title->justification,
					title->text_flags,
					&text_color);

				shadow_alpha = PIN(
					fast_ftol(
						(real)(long)(byte)(title->shadow_color >> 24) *
						fade_amount),
					0,
					255);

				rasterizer_text_set_shadow_color(
					((pixel32)shadow_alpha << 24) |
					(title->shadow_color & 0x00FFFFFF));

				{
					/* the bounds are for 640 columns: on a wider screen move
					them with the side they are on (a title in the left
					third stays, one in the right third moves the whole
					extra width, one between moves half of it: centred).
					By side rather than by each title's own centre, so
					titles placed side by side (Halo 1: NHE's clock, its
					minutes, tens and seconds each a title) move together.
					On Halo 1: NHE's maps the side is where the text hangs
					from: a left-justified title's left edge (their
					countdown, "3...", from the screen's middle to its right
					edge, stays at the middle), a right-justified one's right
					edge, a centred one's centre; elsewhere (the campaign's
					chapter titles) the bounds' centre */
					static rectangle2d wide_bounds;
					short anchor = (short)((title_bounds->x0 + title_bounds->x1) / 2);
					short extra = (short)(halo_screen_width() - 640);
					short shift;

					if (hs_scenario_is_nhe())
					{
						anchor =
							title->justification == _text_justification_left ? title_bounds->x0 :
							title->justification == _text_justification_right ? title_bounds->x1 :
							anchor;
					}
					shift = anchor < 640 / 3 ? 0 : anchor > 640 * 2 / 3 ? extra : (short)(extra / 2);

					wide_bounds = *title_bounds;
					wide_bounds.x0 += shift;
					wide_bounds.x1 += shift;
					title_bounds = &wide_bounds;
				}
				{
					wchar_t const *text = unicode_string_list_get_string(
						help_text_tag_index,
						title->text_index);
					/* port: a title of Halo 1: NHE's maps that repeats itself
					after a run of blank lines, for the lower view of split
					screen (their clock), only once with one view: the Xbox's
					font pushed the copy to the screen's edge, the port's
					high-res text keeps it on screen. Only on their maps, and
					only where there is text before the run (one that starts
					with blank lines, to push its text down, is drawn whole) */
					/* (one per title slot: the text is drawn later in the
					frame, so the titles cannot share one) */
					static wchar_t first_copies[MAXIMUM_QUEUED_CINEMATIC_TITLES][64];
					wchar_t *first_copy = first_copies[title_slot_index];

					if (text && local_player_count() <= 1 && hs_scenario_is_nhe())
					{
						short index;
						short blank_lines = 0;

						for (index = 0; text[index] && index < (short)NUMBEROF(first_copies[0]) - 1; index++)
						{
							if (text[index] == L'\n')
							{
								if (++blank_lines >= 5)
								{
									/* (cut where the run began, if after
									text) */
									while (index > 0 && (first_copy[index - 1] == L'\n' || first_copy[index - 1] == L'\r'))
										index--;
									if (index > 0)
									{
										first_copy[index] = 0;
										text = first_copy;
									}
									break;
								}
							}
							else if (text[index] != L'\r')
							{
								blank_lines = 0;
							}
							first_copy[index] = text[index];
						}
					}
					rasterizer_draw_unicode_string(
						title_bounds,
						NULL,
						NULL,
						0,
						text);
					/* port: where NHE's clock is (cinematic_nhe_clock) */
					if (text && fade_amount > 0.0f && cinematic_title_is_nhe_clock(title))
						cinematic_nhe_clock_note(title_bounds, text);
				}

				rasterizer_text_set_shadow_color(0);
			}

			active_title->time += game_time_get_paused()
				? 0
				: game_time_get_elapsed();

			if (!game_in_editor() &&
				(real)active_title->time >=
					title->up_time + title->fade_out_time)
			{
				active_title->title_index = NONE;
				active_title->time = NONE;
			}
		}
		/* port: (kept where none was drawn this frame: a second, from the
		last drawn, cinematic_nhe_clock_bounds) */
		if (cinematic_nhe_clock.drawn_count)
		{
			cinematic_nhe_clock.count = cinematic_nhe_clock.drawn_count;
			csmemcpy(cinematic_nhe_clock.lines, cinematic_nhe_clock.drawn_lines,
				cinematic_nhe_clock.count * sizeof(rectangle2d));
			cinematic_nhe_clock.game_time = game_time_get();
		}
	}

	return;
}

void cinematic_set_title(
	unsigned short title_index)
{
	cinematic_set_title_delayed(title_index, 0.0f);

	return;
}

/* ---------- private code */

void draw_quad(
	rectangle2d *rectangle,
	pixel32 color)
{
	struct bitmap_data *map;
	real_point2d positions[4];
	struct rasterizer_dynamic_screen_geometry_parameters parameters;
	struct dynamic_screen_vertex vertices[4];
	real_point2d *position;
	struct dynamic_screen_vertex *vertex;
	short vertex_index;
	struct game_globals *game_globals;
	struct game_globals_rasterizer_data *rasterizer_data;

	global_scenario_get();
	game_globals = scenario_get_game_globals();
	rasterizer_data = game_globals->rasterizer_data.count
		? TAG_BLOCK_GET_ELEMENT(
			&game_globals->rasterizer_data,
			0,
			struct game_globals_rasterizer_data)
		: NULL;
	map = TAG_BLOCK_GET_ELEMENT(
		&bitmap_group_get(rasterizer_data->default_textures[0].index)->bitmaps,
		1,
		struct bitmap_data);

	rasterizer_globals.current_lock_operation = _rasterizer_lock_cinematics;

	positions[0].x = (real)rectangle->x0;
	positions[0].y = (real)rectangle->y0;
	positions[1].x = (real)rectangle->x1;
	positions[1].y = (real)rectangle->y0;
	positions[2].x = (real)rectangle->x1;
	positions[2].y = (real)rectangle->y1;
	positions[3].x = (real)rectangle->x0;
	positions[3].y = (real)rectangle->y1;

	position = positions;
	vertex = vertices;
	for (vertex_index = 0; vertex_index < NUMBEROF(vertices); vertex_index++)
	{
		vertex->position.x = position->x;
		vertex->color = color;
		vertex->texture_coordinates.x = 0.0f;
		vertex->texture_coordinates.y = 0.0f;
		vertex->position.y = position->y;
		position++;
		vertex++;
	}

	csmemset(&parameters, 0, sizeof(parameters));
	parameters.framebuffer_blend_function = 0;
	parameters.map_texture_scale[0].j = 1.0f;
	parameters.map_texture_scale[0].i = 1.0f;
	parameters.map_scale[0].j = 1.0f;
	parameters.map_scale[0].i = 1.0f;
	parameters.meter_parameters = NULL;
	parameters.point_sampled = FALSE;
	parameters.map[0] = map;

	rasterizer_psuedo_dynamic_screen_quad_draw(&parameters, vertices);

	rasterizer_globals.current_lock_operation = _rasterizer_lock_none;

	return;
}
