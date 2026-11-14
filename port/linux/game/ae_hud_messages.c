/*
AE_HUD_MESSAGES.C

HUD STYLE AE's CE message lines in the score strip's text style
(ae_hud_messages.h).
*/

#include <math.h>
#include <string.h>

#include "cseries/cseries.h"
#include "math/real_math.h"
#include "bitmaps/bitmaps.h"
#include "bitmaps/bitmap_group.h"
#include "bitmaps/bitmap_color_conversion.h"
#include "game/players.h"
#include "interface/hud.h"
#include "interface/hud_definitions.h"
#include "interface/hud_messaging.h"
#include "interface/weapon_hud_interface_definition.h"
#include "items/weapon_definitions.h"
#include "items/weapons.h"
#include "objects/object_definitions.h"
#include "objects/objects.h"
#include "rasterizer/rasterizer.h"
#include "render/render.h"
#include "scenario/scenario.h"
#include "scenario/scenario_definitions.h"
#include "tag_files/tag_groups.h"
#include "text/draw_string.h"
#include "units/unit_definitions.h"
#include "units/units.h"

#include "interface/hud_draw.h"

#include "ae_hud_digits.h"
#include "ae_hud_message_flow.h"
#include "ae_hud_messages.h"
#include "ae_hud_text.h"
#include "ae_item_prompt.h"
#include "ae_marker_rules.h"
#include "ae_wheel_draw.h"

const char *config_string(const char *name);
void platform_log(char const *format, ...);
long game_time_get(void);

#define AE_BLEND_ALPHA_MULTIPLY_ADD 7  /* the HUD's, as ae_panel_draw.c */
#define AE_WEAPON_HUD_INTERFACE_TAG 'wphi'
#define AE_VEHICLE_TAG 'vehi'
/* hud.c's hud_state_message_type: the debug lines below set the game's own messages */
#define AE_MESSAGE_ENTER_VEHICLE 6
#define AE_MESSAGE_SWAP_WEAPON 4
#define AE_HUD_FLASH_REVERSE_COLORS_BIT 0 /* hud_draw.c's _hud_flash_reverse_colors_bit */

/* (the sizes: ae_hud_message_flow.h, the lines' own since the campaign review of 2026-10-18; they had the score
strip's sub-line 14 px, 0.36 of stock's campaign text) */
#define PICTURE_BIG 1.36f     /* the debug comparison's ("bigicons": buttons 1.7 x the capitals) */
#define PICTURE_GAP 0.12f     /* the room after a picture, of the text's size */
#define PICTURE_HALO_ALPHA 0.50f
#define TEXT_MAXIMUM 512

static const real white_rgb[3] = { 229.0f / 255.0f, 240.0f / 255.0f, 255.0f / 255.0f };
static const real dark_rgb[3] = { 6.0f / 255.0f, 10.0f / 255.0f, 18.0f / 255.0f };
/* the label blue of a feed line's caller and count (the HUD's) */
static const real blue_rgb[3] = { 117.0f / 255.0f, 186.0f / 255.0f, 255.0f / 255.0f };
/* a flashing help line (the scripts' enable_hud_help_flash) goes from the white toward the PC menus' ice-blue focus
glow, never CE's own blue-grey or an amber */
static const real flash_rgb[3] = { 161.0f / 255.0f, 206.0f / 255.0f, 255.0f / 255.0f };

static struct
{
	boolean open;
	short view;
	real scale;
	real px;   /* the text's size (its em) */
	real cap;  /* its capitals' height */
	real flash; /* a flashing help line's phase: 0 the white, 1 the ice-blue (ae_hud_messages_colour) */
	boolean opaque; /* a help or state line: drawn at full alpha, not its colour's constant 0.5 */
	real picture_wide, picture_high, picture_px; /* the last picture's texels and drawn height (the log line) */
	real drawn_alpha, flash_most; /* CE's last line colour's alpha, the most flash since the last log line */
	long saved_font;
	real_argb_color saved_color;
	struct ae_hud_message_flow flow;
	long texts, pictures, list_lines; /* drawn since the last log line */
	boolean keep;      /* the view's wheel is open: its ring and word keep the rows off (ae_wheel_text_keepout) */
	real keep_x, keep_y, keep_radius, keep_limit;
	real keep_word[4];
	boolean nowrap;    /* the feed line being drawn is not to wrap (the kill feed's: AE_RUN_NOWRAP) */
	boolean run_custom; /* its piece's colour is a run's own (AE_RUN_RGB) */
	real run_rgb[3];
	real left, view_width; /* the lines' left x and the view's width (the item prompt's line) */
} block;

boolean ae_hud_messages_open(void)
{
	return block.open;
}

/* ---------- the rows drawn (what the aim prompt and the markers' blocks keep clear of) */

/* the rows of the block being drawn, and each view's last block's: a row is the pieces on one baseline */
static struct ae_message_rows
{
	short count;
	real box[AE_HUD_MESSAGE_ROWS_MOST][4];
	real baseline[AE_HUD_MESSAGE_ROWS_MOST];
	long stamp;
} rows_building, rows_shown[MAXIMUM_LOCAL_PLAYERS];

/* a piece of the current line from x0 to x1: the row's box grows to hold it (the text's capitals and its descenders, and
the outline) */
static void row_note(real x0, real x1)
{
	short index;

	if (!block.open || x1 <= x0)
		return;
	for (index = 0; index < rows_building.count; index++)
	{
		if ((real)fabs(rows_building.baseline[index] - block.flow.baseline) < 1.0f)
			break;
	}
	if (index == rows_building.count)
	{
		if (index >= AE_HUD_MESSAGE_ROWS_MOST)
			return;
		rows_building.count++;
		rows_building.baseline[index] = block.flow.baseline;
		rows_building.box[index][0] = x0;
		rows_building.box[index][1] = block.flow.baseline - block.cap - 0.12f * block.px;
		rows_building.box[index][2] = x1;
		rows_building.box[index][3] = block.flow.baseline + 0.30f * block.px;
		return;
	}
	if (x0 < rows_building.box[index][0])
		rows_building.box[index][0] = x0;
	if (x1 > rows_building.box[index][2])
		rows_building.box[index][2] = x1;
}

/* where the row being drawn stands in this block's rows: its baseline's row, or the next one */
static short row_index_now(void)
{
	short index;

	for (index = 0; index < rows_building.count; index++)
	{
		if ((real)fabs(rows_building.baseline[index] - block.flow.baseline) < 1.0f)
			break;
	}
	return index;
}

/* whether the row being drawn is under the open wheel (ae_row_hidden_by_wheel): it keeps its place, blank, and CE's timer
for it runs on. The row's box is the last frame's, whole, since its pieces are drawn one at a time (the wheel's one frame of
opening is then the lag) */
static boolean row_hidden_now(void)
{
	struct ae_message_rows const *last = &rows_shown[block.view];
	long now = game_time_get();
	short index;
	float box[4], word[4];

	if (!block.keep || last->count <= 0 || now < last->stamp || now - last->stamp > 2)
		return FALSE;
	index = row_index_now();
	if (index >= last->count)
		return FALSE;
	box[0] = (float)last->box[index][0];
	box[1] = (float)last->box[index][1];
	box[2] = (float)last->box[index][2];
	box[3] = (float)last->box[index][3];
	word[0] = (float)block.keep_word[0];
	word[1] = (float)block.keep_word[1];
	word[2] = (float)block.keep_word[2];
	word[3] = (float)block.keep_word[3];
	return ae_row_hidden_by_wheel(box, (float)block.keep_x, (float)block.keep_y, (float)block.keep_radius, word,
		(float)block.keep_limit) != 0;
}

short ae_hud_message_rows(short local_player_index, real boxes[][4], short maximum)
{
	struct ae_message_rows const *rows;
	long now = game_time_get();
	short index, count;

	if (local_player_index < 0 || local_player_index >= MAXIMUM_LOCAL_PLAYERS)
		return 0;
	rows = &rows_shown[local_player_index];
	/* (a block of the last frames: the view draws no lines when it has none, and its last block is not left standing) */
	if (rows->count <= 0 || now < rows->stamp || now - rows->stamp > 2)
		return 0;
	count = rows->count < maximum ? rows->count : maximum;
	for (index = 0; index < count; index++)
	{
		boxes[index][0] = rows->box[index][0];
		boxes[index][1] = rows->box[index][1];
		boxes[index][2] = rows->box[index][2];
		boxes[index][3] = rows->box[index][3];
	}
	return count;
}

/* ---------- the debug lines (debug.ae_message_test) */

static boolean test_has(char const *word)
{
	char const *test = config_string("debug.ae_message_test");
	size_t length = strlen(word);
	char const *walk;

	if (!test || !test[0])
		return FALSE;
	for (walk = test; walk && *walk; walk = strchr(walk, ','), walk = walk ? walk + 1 : NULL)
		if (strcspn(walk, ",") == length && !strncmp(walk, word, length))
			return TRUE;
	return FALSE;
}

static long test_unit(short view)
{
	long player_index = local_player_get_player_index(view);

	if (player_index == NONE)
		return NONE;
	return player_get(player_index)->unit_index;
}

static long test_weapon_definition(short view)
{
	long unit_index = test_unit(view);
	long weapon_index;

	if (unit_index == NONE)
		return NONE;
	weapon_index = unit_inventory_get_weapon(unit_index, unit_get(unit_index)->unit.current_weapon_index);
	if (weapon_index == NONE)
		return NONE;
	return weapon_get(weapon_index)->definition_index;
}

/* the campaign's help text (hud_messaging.c's, its tag group 'hmt '): a message picked by a piece of its text, as the
scripts' hud_set_help_text picks it by name. (The tag's own layout, as hs.c keeps it: hud_messaging.c's structures are
its own) */
#define AE_HUD_MESSAGE_TEXT_TAG 'hmt '

struct ae_test_hud_message_text
{
	struct tag_data text_data;
	struct tag_block elements;
	struct tag_block messages;
};

struct ae_test_hud_message
{
	char name[32];
	word text_start_index;
	word element_start_index;
	byte element_count;
	byte pad25[3];
	long unused28[6];
};

struct ae_test_hud_message_element
{
	byte type; /* 0 text (data: its length), 1 an icon */
	byte data;
};

static short test_help_message(wchar_t const *piece)
{
	struct scenario *scenario = global_scenario_get();
	struct ae_test_hud_message_text *text;
	long piece_length = (long)wcslen(piece);
	short i;

	if (!scenario || scenario->hud_messages.index == NONE)
		return NONE;
	text = (struct ae_test_hud_message_text *)tag_get(AE_HUD_MESSAGE_TEXT_TAG, scenario->hud_messages.index);
	for (i = 0; i < (short)text->messages.count; i++)
	{
		struct ae_test_hud_message *message = TAG_BLOCK_GET_ELEMENT(&text->messages, i, struct ae_test_hud_message);
		long position = message->text_start_index;
		short e;

		for (e = 0; e < message->element_count; e++)
		{
			struct ae_test_hud_message_element *element = TAG_BLOCK_GET_ELEMENT(&text->elements,
				message->element_start_index + e, struct ae_test_hud_message_element);
			wchar_t const *run;
			long j;

			if (element->type != 0)
				continue;
			run = (wchar_t const *)tag_data_get_pointer(&text->text_data, position * (long)sizeof(wchar_t),
				element->data * (long)sizeof(wchar_t));
			for (j = 0; j + piece_length <= element->data; j++)
				if (!memcmp(run + j, piece, (size_t)piece_length * sizeof(wchar_t)))
					return i;
			position += element->data;
		}
	}
	return NONE;
}

/* the game's own messages set for the pictures and the tests, as hud.c sets them: the state line of a vehicle in
reach ("vehicle"), of a weapon to swap for (the player's own: "swap"), or a long custom line with a button picture
("long"); a list line now and then: an ammunition pickup ("pickup"). The campaign's help text with its line breaks
(a10's, "|n" in the tag's text): the look inversion question ("help_invert"), the shields' "Press A to continue"
("help_shield"), move and look ("help_move"); a custom line with the same breaks in any game ("breaks") */
static void test_messages(short view)
{
	static long last_tick[MAXIMUM_LOCAL_PLAYERS] = { -1000, -1000, -1000, -1000 };
	long tick = game_time_get();
	wchar_t const *help = test_has("help_invert") ? L"inverted|n|nPress " :
		test_has("help_shield") ? L"Charged|n|nPress " : test_has("help_move") ? L" to move|n|nUse " : NULL;

	if (help)
	{
		short message = test_help_message(help);

		if (message != NONE)
		{
			scripted_show_hud_help_text(TRUE);
			scripted_hud_set_state_message(message);
		}
	}
	else if (test_has("breaks"))
	{
		hud_enable_custom_state_message(view, TRUE);
		hud_set_state_text(view, L"Vertical looking is now inverted|n|nPress %a-button to try it the other way|n|n"
			L"Press %b-button to leave it like it is");
	}
	else if (test_has("long"))
	{
		/* (a custom line, as the game engine's "Hold BACK for score": its %icons go through
		draw_string_and_hack_in_icons) */
		hud_enable_custom_state_message(view, TRUE);
		hud_set_state_text(view, L"Hold %x-button to see where a long line breaks: a custom prompt with a button "
			L"picture, long enough to reach past the ammo group and on toward the far side of the view");
	}
	else if (test_has("vehicle"))
	{
		static char const *const names[] = { "vehicles\\scorpion\\scorpion_mp", "vehicles\\warthog\\mp_warthog",
			"vehicles\\ghost\\ghost_mp", "vehicles\\scorpion\\scorpion", "vehicles\\warthog\\warthog" };
		long vehicle = NONE;
		short i;

		for (i = 0; i < (short)NUMBEROF(names) && vehicle == NONE; i++)
			vehicle = tag_loaded(AE_VEHICLE_TAG, names[i]);
		if (vehicle != NONE && unit_definition_get(vehicle)->unit.seats.count > 0)
		{
			struct unit_seat *seat = TAG_BLOCK_GET_ELEMENT(&unit_definition_get(vehicle)->unit.seats, 0,
				struct unit_seat);

			hud_set_state_message(view, AE_MESSAGE_ENTER_VEHICLE);
			hud_set_state_message_text(view, 0, seat->icon_text_index, FALSE);
			hud_set_state_message_text(view, 1, object_definition_get(vehicle)->object.icon_text_index, FALSE);
		}
	}
	else if (test_has("swap"))
	{
		long weapon = test_weapon_definition(view);
		long hud_index = weapon == NONE ? NONE :
			weapon_definition_get(weapon)->weapon.interface_definition.hud_interface.index;

		if (hud_index != NONE)
		{
			struct weapon_hud_interface_definition *hud =
				(struct weapon_hud_interface_definition *)tag_get(AE_WEAPON_HUD_INTERFACE_TAG, hud_index);

			if (hud->messaging_icon.sequence_index != NONE)
			{
				hud_set_state_message(view, AE_MESSAGE_SWAP_WEAPON);
				hud_set_state_message_icon(view, 0, &hud->messaging_icon);
			}
		}
	}
	if (tick >= 0 && (tick < last_tick[view] || tick - last_tick[view] >= 4 * 30))
	{
		last_tick[view] = tick;
		if (test_has("pickup"))
		{
			long weapon = test_weapon_definition(view);

			/* (hud_add_item_message itself, as hud_picked_up_ammunition calls it: that one is hidden with KILL FEED on, and the preview
			must show its lines; only this debug preview skips the hide) */
			if (weapon != NONE)
				hud_add_item_message(view, weapon, 60, 1);
		}
	}
}

/* ---------- the block */

boolean ae_hud_messages_begin(short local_player_index, real left, real first_cap_top, real view_width,
	real group_width, real scale)
{
	static long logged_tick[MAXIMUM_LOCAL_PLAYERS] = { -1, -1, -1, -1 };
	real right, pitch, test_px;
	long tick = game_time_get();

	block.open = FALSE;
	if (local_player_index < 0 || local_player_index >= MAXIMUM_LOCAL_PLAYERS || scale <= 0.0f)
		return FALSE;
	/* (no font, no AE text: CE's lines as before) */
	test_px = ae_hud_message_px(scale);
	if (ae_hud_text_measure(L"H", test_px) <= 0.0f)
		return FALSE;
	test_messages(local_player_index);
	/* ("ce": the lines as before the restyle, CE's own at 0.60, for the before-and-after pictures) */
	if (test_has("ce"))
		return FALSE;

	block.view = local_player_index;
	block.left = left;
	block.view_width = view_width;
	block.scale = scale;
	block.px = test_px;
	block.cap = AE_HUD_TEXT_CAP * block.px;
	block.flash = 0.0f;
	block.opaque = FALSE;
	pitch = ae_hud_message_pitch(block.px);
	/* the wrap: no wider than twice the ammo group (owner 10-15: a block under the counter); with no group (the
	campaign's start, no weapon yet) 26 em, not the whole view (campaign review F3) */
	right = ae_hud_message_wrap_right(left, view_width, group_width, block.px, scale);
	ae_hud_message_flow_start(&block.flow, left, right, first_cap_top + block.cap, pitch);
	block.open = TRUE;
	rows_building.count = 0;
	block.keep = ae_wheel_text_keepout(local_player_index, &block.keep_x, &block.keep_y, &block.keep_radius, block.keep_word,
		&block.keep_limit);

	if (config_string("debug.ae_message_test")[0] && tick >= 0 && tick / 60 != logged_tick[local_player_index])
	{
		logged_tick[local_player_index] = tick / 60;
		platform_log("ae messages: tick %ld view %d left=%.1f cap_top=%.1f right=%.1f px=%.1f pitch=%.1f "
			"texts=%ld pictures=%ld list=%ld picture=%.0fx%.0f@%.1f alpha=%.2f flash=%.2f", tick / 60 * 60,
			local_player_index, left, first_cap_top, right, block.px, pitch, block.texts, block.pictures,
			block.list_lines, block.picture_wide, block.picture_high, block.picture_px, block.drawn_alpha,
			block.flash_most);
		block.texts = block.pictures = block.list_lines = 0;
		block.flash_most = 0.0f;
	}
	return TRUE;
}

void ae_hud_messages_end(void)
{
	if (block.open)
	{
		rows_shown[block.view] = rows_building;
		rows_shown[block.view].stamp = game_time_get();
	}
	block.open = FALSE;
}

void ae_hud_messages_colour(boolean objective, boolean help, boolean flashing,
	struct hud_color_definition const *help_color, union real_argb_color const *drawn)
{
	real_argb_color base, flash;
	real axis[3], length = 0.0f, along = 0.0f, t;
	short i;

	block.flash = 0.0f;
	block.opaque = FALSE;
	if (drawn)
		block.drawn_alpha = drawn->alpha;
	if (!block.open || objective)
		return;
	/* (a help or state line's colour has a constant alpha, 0.5 in the stock maps, not a fade: AE's white at it was a
	grey (117,123,134) on a10's cryo bay. An objective's alpha is its fade and stays, as the list lines' do) */
	block.opaque = TRUE;
	if (!help || !flashing || !help_color || !drawn)
		return;
	/* (get_flash_color mixes the colour toward its flash colour by the phase: read the phase back from the colour CE
	drew, so AE's white mixes toward the ice-blue in the same beat; reversed colours start at the flash colour) */
	pixel32_to_real_argb_color(help_color->color, &base);
	pixel32_to_real_argb_color(help_color->flash_color, &flash);
	axis[0] = flash.red - base.red;
	axis[1] = flash.green - base.green;
	axis[2] = flash.blue - base.blue;
	for (i = 0; i < 3; i++)
		length += axis[i] * axis[i];
	if (length <= 0.0001f)
		return;
	along = (drawn->red - base.red) * axis[0] + (drawn->green - base.green) * axis[1] +
		(drawn->blue - base.blue) * axis[2];
	t = along / length;
	t = t < 0.0f ? 0.0f : t > 1.0f ? 1.0f : t;
	if (TEST_FLAG(help_color->flash_flags, AE_HUD_FLASH_REVERSE_COLORS_BIT))
		t = 1.0f - t;
	block.flash = t;
	if (t > block.flash_most)
		block.flash_most = t;
}

/* the text's colour: AE's white, or toward the ice-blue while a help line flashes */
static void text_rgb(real rgb[3])
{
	short i;

	for (i = 0; i < 3; i++)
		rgb[i] = white_rgb[i] + (flash_rgb[i] - white_rgb[i]) * block.flash;
}

/* CE's draw mode kept round AE's text (its font and colour; AE's text sets its own, with no indents) */
static void save_mode(void)
{
	block.saved_font = draw_string_get_font();
	draw_string_get_color(&block.saved_color);
	draw_string_set_indents(0, 0);
}

static void restore_mode(void)
{
	draw_string_set_font(block.saved_font);
	draw_string_set_color(&block.saved_color);
}

static real measure_run(const wchar_t *text, long count, void *context)
{
	wchar_t run[TEXT_MAXIMUM];

	(void)context;
	if (count <= 0)
		return 0.0f;
	if (count > TEXT_MAXIMUM - 1)
		count = TEXT_MAXIMUM - 1;
	memcpy(run, text, (size_t)count * sizeof(wchar_t));
	run[count] = 0;
	return ae_hud_text_measure(run, block.px);
}

/* a piece of text on the pen, broken between words where it reaches the wrap, in the text's white or (a feed line's caller and
count) the label blue */
static void flow_piece(wchar_t const *source, boolean blue)
{
	wchar_t text[TEXT_MAXIMUM];
	long length;
	real_argb_color color;
	real rgb[3];
	wchar_t *at;

	/* (the text with CE's line ends as '\n': the campaign's help text breaks its lines with "|n". AE's text measured
	such a piece as 0 px (a10's "Energy Shields Charged|n|nPress ") and drew only its first line, so the button and the
	next piece started back at the line's left, over the first words) */
	length = ae_hud_message_flow_clean(source, text, TEXT_MAXIMUM);
	if (block.run_custom)
	{
		rgb[0] = block.run_rgb[0];
		rgb[1] = block.run_rgb[1];
		rgb[2] = block.run_rgb[2];
	}
	else if (blue)
	{
		rgb[0] = blue_rgb[0];
		rgb[1] = blue_rgb[1];
		rgb[2] = blue_rgb[2];
	}
	else
		text_rgb(rgb);
	color.alpha = block.opaque ? 1.0f : block.saved_color.alpha;
	color.red = rgb[0];
	color.green = rgb[1];
	color.blue = rgb[2];

	at = text;
	/* (a line that does not wrap: the piece whole on the pen's line, past the wrap limit if it is wider) */
	if (block.nowrap && length > 0)
	{
		ae_hud_message_flow_line(&block.flow);
		row_note(block.flow.x, block.flow.x + ae_hud_text_measure(at, block.px));
		if (!row_hidden_now())
			ae_hud_text_draw_ex(block.view, block.flow.x, block.flow.baseline, at, block.px, &color, _ae_text_outline);
		block.flow.x += ae_hud_text_measure(at, block.px);
		block.flow.line_empty = FALSE;
		return;
	}
	while (length > 0)
	{
		long skip, count;

		if (*at == L'\n')
		{
			ae_hud_message_flow_new_line(&block.flow);
			at++;
			length--;
			continue;
		}
		ae_hud_message_flow_line(&block.flow);
		count = ae_hud_message_flow_fit(&block.flow, at, length, measure_run, NULL, &skip);
		at += skip;
		length -= skip;
		if (count == 0)
		{
			if (length > 0 && *at != L'\n')
				ae_hud_message_flow_new_line(&block.flow);
			continue;
		}
		{
			wchar_t saved = at[count];

			at[count] = 0;
			row_note(block.flow.x, block.flow.x + ae_hud_text_measure(at, block.px));
			if (!row_hidden_now())
				ae_hud_text_draw_ex(block.view, block.flow.x, block.flow.baseline, at, block.px, &color, _ae_text_outline);
			block.flow.x += ae_hud_text_measure(at, block.px);
			at[count] = saved;
		}
		block.flow.line_empty = FALSE;
		at += count;
		length -= count;
		if (length > 0 && *at != L'\n')
			ae_hud_message_flow_new_line(&block.flow);
	}
}

/* a text for the pen. runs: a feed line's colour runs (ae_marker_rules.h) cut it into pieces of the white and the label blue; no
other text may use them, and they are left out of it */
static boolean run_mark(wchar_t c)
{
	return c == AE_RUN_WHITE || c == AE_RUN_BLUE || c == AE_RUN_RGB || c == AE_RUN_NOWRAP || AE_RUN_IS_BYTE(c);
}

static void flow_text(wchar_t const *source, boolean runs)
{
	wchar_t piece[TEXT_MAXIMUM];
	boolean blue = FALSE;

	block.run_custom = FALSE;
	while (source && *source)
	{
		long length = 0;

		while (*source && length < TEXT_MAXIMUM - 1 && (!runs || !run_mark(*source)))
		{
			if (runs || !run_mark(*source))
				piece[length++] = *source;
			source++;
		}
		piece[length] = 0;
		if (length > 0)
			flow_piece(piece, blue);
		if (runs && *source == AE_RUN_BLUE)
		{
			blue = TRUE;
			block.run_custom = FALSE;
			source++;
		}
		else if (runs && *source == AE_RUN_WHITE)
		{
			blue = FALSE;
			block.run_custom = FALSE;
			source++;
		}
		else if (runs && *source == AE_RUN_RGB)
		{
			int channel;

			source++;
			for (channel = 0; channel < 3 && AE_RUN_IS_BYTE(*source); channel++, source++)
				block.run_rgb[channel] = (real)AE_RUN_BYTE_VALUE(*source) / 255.0f;
			block.run_custom = (boolean)(channel == 3);
		}
		else if (runs && *source == AE_RUN_NOWRAP)
		{
			block.nowrap = TRUE;
			source++;
		}
		else if (runs && AE_RUN_IS_BYTE(*source))
			source++;
	}
	block.run_custom = FALSE;
}

boolean ae_hud_message_text(wchar_t const *text)
{
	if (!block.open)
		return FALSE;
	if (!text || !text[0])
		return TRUE;
	save_mode();
	flow_text(text, FALSE);
	restore_mode();
	block.texts++;
	return TRUE;
}

boolean ae_hud_message_list_line(wchar_t const *text, boolean feed)
{
	if (!block.open)
	{
		/* (a style with no block: the stock draw shows the text, and a feed line's runs would be missing glyphs) */
		if (feed)
			ae_feed_strip_runs((wchar_t *)text);
		return FALSE;
	}
	/* (the list's lines after the help line keep their own fade and AE's white) */
	block.opaque = FALSE;
	block.flash = 0.0f;
	ae_hud_message_flow_new_line(&block.flow);
	if (text && text[0])
	{
		save_mode();
		block.nowrap = FALSE;
		flow_text(text, feed);
		block.nowrap = FALSE;
		restore_mode();
	}
	block.list_lines++;
	return TRUE;
}

void ae_hud_messages_keep_first_line(void)
{
	if (block.open)
	{
		ae_hud_message_flow_line(&block.flow);
		/* (the item prompt's line, in the split views, is the first line of the list) */
		ae_item_prompt_split_draw(block.view);
	}
}

/* ---------- the item prompt's line (ae_item_prompt.c): pieces on the first line's pen, never wrapping */

boolean ae_hud_message_prompt_begin(real *left, real *view_width, real *scale, real *px, real *baseline, real *cap)
{
	if (!block.open)
		return FALSE;
	ae_hud_message_flow_line(&block.flow);
	if (block.flow.lines != 1 || !block.flow.line_empty)
		return FALSE;
	*left = block.left;
	*view_width = block.view_width;
	*scale = block.scale;
	*px = block.px;
	*baseline = block.flow.baseline;
	*cap = block.cap;
	return TRUE;
}

void ae_hud_message_prompt_text(wchar_t const *text, real const rgb[3])
{
	if (!block.open || !text || !text[0])
		return;
	block.opaque = TRUE;
	block.flash = 0.0f;
	block.nowrap = TRUE;
	block.run_custom = TRUE;
	block.run_rgb[0] = rgb[0];
	block.run_rgb[1] = rgb[1];
	block.run_rgb[2] = rgb[2];
	flow_piece(text, FALSE);
	block.run_custom = FALSE;
	block.nowrap = FALSE;
	block.opaque = FALSE;
	block.texts++;
}

real ae_hud_message_prompt_x(void)
{
	return block.flow.x;
}

void ae_hud_message_prompt_space(real dx)
{
	block.flow.x += dx;
}

boolean ae_hud_message_prompt_reserve(real width, real *x)
{
	*x = block.flow.x;
	row_note(block.flow.x, block.flow.x + width);
	block.flow.x += width;
	block.flow.line_empty = FALSE;
	block.pictures++;
	return (boolean)!row_hidden_now();
}

/* ---------- pictures */

boolean ae_hud_message_metrics(short *view, real *px, real *cap, real *alpha)
{
	real_argb_color text_color;

	if (!block.open)
		return FALSE;
	draw_string_get_color(&text_color);
	*view = block.view;
	*px = block.px;
	*cap = block.cap;
	*alpha = block.opaque ? 1.0f : text_color.alpha;
	return TRUE;
}

void ae_hud_message_place(real width, real height, real before, real after, real *x, real *top)
{
	ae_hud_message_flow_line(&block.flow);
	if (block.flow.line_empty)
		ae_hud_message_flow_picture(&block.flow, width, after, x);
	else if (ae_hud_message_flow_picture(&block.flow, before + width, after, x))
		block.flow.x -= before; /* (sent to the next line: it starts it, no room before) */
	else
		*x += before;
	*top = block.flow.baseline - block.cap / 2.0f - height / 2.0f;
	row_note(*x, *x + width);
	block.pictures++;
}

static void picture_quad(struct bitmap_data const *bitmap, real const texture[4], real x0, real y0, real x1, real y1,
	pixel32 color)
{
	struct dynamic_screen_vertex vertices[4];
	struct rasterizer_dynamic_screen_geometry_parameters parameters;
	real units = 1.0f / ae_hud_pixels_per_unit();
	short i;

	/* (corners clockwise from the top-left, as hud_draw.c's sprites) */
	for (i = 0; i < 4; i++)
	{
		boolean right = (boolean)(((i + 1) & 2) != 0), bottom = (boolean)(i > 1);

		vertices[i].position.x = (right ? x1 : x0) * units;
		vertices[i].position.y = (bottom ? y1 : y0) * units;
		vertices[i].texture_coordinates.x = right ? texture[2] : texture[0];
		vertices[i].texture_coordinates.y = bottom ? texture[3] : texture[1];
		vertices[i].color = color;
	}
	csmemset(&parameters, 0, sizeof(parameters));
	parameters.map_texture_scale[0].i = 1.0f;
	parameters.map_texture_scale[0].j = 1.0f;
	parameters.map_scale[0].i = 1.0f;
	parameters.map_scale[0].j = 1.0f;
	parameters.framebuffer_blend_function = AE_BLEND_ALPHA_MULTIPLY_ADD;
	parameters.map[0] = (struct bitmap_data *)bitmap;
	rasterizer_psuedo_dynamic_screen_quad_draw(&parameters, vertices);
}

/* a colour premultiplied by its alpha (the HUD's blend adds the colour over 1 - alpha of what is under it) */
static pixel32 premultiplied(real const rgb[3], real alpha)
{
	real_argb_color color;

	color.alpha = alpha < 0.0f ? 0.0f : alpha > 1.0f ? 1.0f : alpha;
	color.red = rgb[0] * color.alpha;
	color.green = rgb[1] * color.alpha;
	color.blue = rgb[2] * color.alpha;
	return real_argb_color_to_pixel32(&color);
}

/* the A button's height in texels when bitmap is a page of the HUD's message icons sheet, else 0: CE's pictures keep
their proportions to it (ae_hud_message_picture_height). (A page, or a stand-in of one: hud_hires_tags.c's and the
PlayStation pictures' placeholders are new bitmaps of the page's size, so a page is known by its size too) */
static real sheet_button_texels(struct bitmap_data const *bitmap)
{
	long group_index = hud_globals ? hud_globals->messaging.messaging_icons.index : NONE;
	struct bitmap_group *group;
	struct bitmap_data const *button = NULL;
	real_rectangle2d const *clip = NULL;
	boolean member = FALSE;
	long i;

	if (group_index == NONE || !bitmap)
		return 0.0f;
	group = bitmap_group_get(group_index);
	for (i = 0; i < group->bitmaps.count && !member; i++)
	{
		struct bitmap_data const *page = TAG_BLOCK_GET_ELEMENT(&group->bitmaps, i, struct bitmap_data);

		member = (boolean)(page == bitmap || (page->width == bitmap->width && page->height == bitmap->height));
	}
	if (!member)
		return 0.0f;
	hud_retrieve_bitmap_and_bounding_rect(group_index, 0, 0, &button, &clip);
	if (!button || !clip || button->height <= 0)
		return 0.0f;
	return (clip->y1 - clip->y0) * (real)button->height;
}

boolean ae_hud_message_icon(struct bitmap_data const *bitmap, union real_rectangle2d const *clip,
	pixel32 color, boolean own_color)
{
	static const real around[8][2] = { { 1.0f, 0.0f }, { -1.0f, 0.0f }, { 0.0f, 1.0f }, { 0.0f, -1.0f },
		{ 0.7071f, 0.7071f }, { -0.7071f, 0.7071f }, { 0.7071f, -0.7071f }, { -0.7071f, -0.7071f } };
	real texture[4], texels_wide, texels_high, height, width, x, top, halo, alpha, rgb[3];
	real_argb_color text_color;
	short pass;

	if (!block.open)
		return FALSE;
	if (!bitmap || bitmap->width <= 0 || bitmap->height <= 0)
		return TRUE;
	/* (CE's own sizes: hud_draw.c's hud_calculate_bitmap_bounds of a sprite that is not an interface bitmap) */
	texture[0] = clip ? clip->x0 : 0.0f;
	texture[1] = clip ? clip->y0 : 0.0f;
	texture[2] = clip ? clip->x1 : 1.0f;
	texture[3] = clip ? clip->y1 : 1.0f;
	texels_wide = (texture[2] - texture[0]) * (real)bitmap->width;
	texels_high = (texture[3] - texture[1]) * (real)bitmap->height;
	if (texels_wide <= 0.0f || texels_high <= 0.0f)
		return TRUE;
	block.picture_wide = texels_wide;
	block.picture_high = texels_high;
	height = ae_hud_message_picture_height(block.cap, texels_wide, texels_high, sheet_button_texels(bitmap));
	if (test_has("bigicons"))
		height *= PICTURE_BIG; /* (taller pictures, to compare) */
	block.picture_px = height;
	width = height * texels_wide / texels_high;
	ae_hud_message_flow_picture(&block.flow, width, PICTURE_GAP * block.px, &x);
	top = block.flow.baseline - block.cap / 2.0f - height / 2.0f;
	row_note(x, x + width);
	/* (under the open wheel: the picture's place stays blank) */
	if (row_hidden_now())
	{
		block.pictures++;
		return TRUE;
	}

	/* (the line's alpha: CE's draw colour's, an objective's fade) */
	draw_string_get_color(&text_color);
	alpha = block.opaque ? 1.0f : text_color.alpha;
	halo = 2.0f * (real)sqrt(block.px / 36.0f);
	for (pass = 0; pass < 8; pass++)
		picture_quad(bitmap, texture, x + around[pass][0] * halo, top + around[pass][1] * halo,
			x + width + around[pass][0] * halo, top + height + around[pass][1] * halo,
			premultiplied(dark_rgb, PICTURE_HALO_ALPHA * alpha));
	text_rgb(rgb);
	picture_quad(bitmap, texture, x, top, x + width, top + height,
		own_color ? color : premultiplied(rgb, alpha));
	block.pictures++;
	return TRUE;
}
