/*
AE_ITEM_PROMPT.C

The item prompt (ae_item_prompt.h; the rules are ae_item_prompt_rules.h's). The game sets its state line from the player's action result
(hud.c's hud_show_action_response); on HUD STYLE AE that function asks here first, and a prompt taken here is drawn by this file instead of
by the state line: under the reticle in one view, as the first line of the message list in two or more.
*/

#include <math.h>
#include <string.h>
#include <wchar.h>

#include "cseries/cseries.h"
#include "math/real_math.h"
#include "cutscene/cinematics.h"
#include "game/game.h"
#include "game/game_engine.h"
#include "game/players.h"
#include "interface/hud.h"
#include "interface/hud_definitions.h"
#include "interface/hud_messaging.h"
#include "items/weapons.h"
#include "objects/object_definitions.h"
#include "objects/objects.h"
#include "render/render.h"
#include "scenario/scenario.h"
#include "scenario/scenario_definitions.h"
#include "tag_files/tag_groups.h"
#include "text/text_group.h"
#include "text/unicode.h"
#include "units/unit_definitions.h"
#include "units/units.h"
#include "devices/device_controls.h"

#include "ae_bomb_assault.h"
#include "ae_hud_digits.h"
#include "ae_hud_messages.h"
#include "ae_hud_style.h"
#include "ae_hud_text.h"
#include "ae_item_prompt.h"
#include "ae_item_prompt_rules.h"
#include "ae_key_prompts.h"
#include "ae_hold_act.h"
#include "ae_marks.h"
#include "ae_panel_draw.h"
#include "ae_pause.h"
#include "ae_score_strip.h"
#include "ae_wheel.h"

void platform_video_drawable_size(int *width, int *height);
real hud_item_timers_ae_waypoint_scale(real world_units);
boolean game_engine_scoreboard_held(short local_player_index);
real game_engine_scoreboard_shown(short local_player_index);
const char *config_string(const char *name);
void platform_log(char const *format, ...);

#define PLAYERS 4
/* hud.c's hud_state_message_type: the game's messages whose button the tag names */
#define MESSAGE_PICKUP 0
#define MESSAGE_TOUCH_DEVICE 2
#define MESSAGE_SWAP_WEAPON 4
#define MESSAGE_REMIND_TO_SWITCH 5
#define MESSAGE_ENTER_VEHICLE 6
#define MESSAGE_FLIP_VEHICLE 8
/* hud_messaging.c's hud_icon_type: the action button, and the switch weapons button */
#define ICON_ACTION 18
#define ICON_ROTATE_WEAPONS 24
#define HUD_MESSAGE_TEXT_TAG 'hmt '
#define HUD_MESSAGE_TYPE_ICON 1
#define SEAT_SIZE 24

static real const white_rgb[3] = { 229.0f / 255.0f, 240.0f / 255.0f, 255.0f / 255.0f };
static real const grey_rgb[3] = { 188.0f / 255.0f, 198.0f / 255.0f, 212.0f / 255.0f };
static real const ice_rgb[3] = { 184.0f / 255.0f, 214.0f / 255.0f, 255.0f / 255.0f };

/* what each view's prompt is, from the last tick the game said it was true */
static struct record
{
	boolean have;
	int kind;
	long object_index;
	short icon_type;
	wchar_t name[AE_IP_NAME_SIZE];
	wchar_t detail[AE_IP_AFTER_SIZE];
	long began, last_true;
	boolean instant;
	/* the block drawn in the last frames (the aim prompt keeps clear of it) */
	boolean drawn;
	long drawn_at;
	float box[4];
} records[PLAYERS];

/* ---------- what the game's own tag says the button is */

/* the tag's message layout, as hud_messaging.c keeps it (ae_hud_messages.c reads the same tag the same way) */
struct tag_message_text
{
	struct tag_data text_data;
	struct tag_block elements;
	struct tag_block messages;
};

struct tag_message
{
	char name[32];
	word text_start_index;
	word element_start_index;
	byte element_count;
	byte pad25[3];
	long unused28[6];
};

struct tag_message_element
{
	byte type;
	byte data;
};

/* the button icon (a hud_icon_type) the game's message for this state names, else fallback: the action button for "Hold X to swap" */
static short message_icon(short message_index, short fallback)
{
	struct tag_message_text *text;
	struct tag_message *message;
	short element_index;

	if (!hud_globals || hud_globals->messaging.hud_messages.index == NONE)
		return fallback;
	text = (struct tag_message_text *)tag_get(HUD_MESSAGE_TEXT_TAG, hud_globals->messaging.hud_messages.index);
	if (message_index < 0 || message_index >= text->messages.count)
		return fallback;
	message = TAG_BLOCK_GET_ELEMENT(&text->messages, message_index, struct tag_message);
	for (element_index = 0; element_index < message->element_count; element_index++)
	{
		long index = message->element_start_index + element_index;
		struct tag_message_element *element;

		if (index >= text->elements.count)
			break;
		element = TAG_BLOCK_GET_ELEMENT(&text->elements, index, struct tag_message_element);
		if (element->type == HUD_MESSAGE_TYPE_ICON)
			return element->data;
	}
	return fallback;
}

/* a string of the HUD's icon text list (vehicles, seats), "" for none */
static wchar_t const *icon_string(short string_index)
{
	wchar_t const *text;

	if (!hud_globals || hud_globals->messaging.alternate_icon_text.index == NONE || string_index == NONE)
		return L"";
	text = ae_bomb_snd_item_text(hud_globals->messaging.alternate_icon_text.index, string_index);
	return text ? text : L"";
}

/* ---------- taking a prompt */

static boolean view_ok(short local_player_index)
{
	return (boolean)(local_player_index >= 0 && local_player_index < PLAYERS);
}

/* whether the view's player has a button to name at all, and the prompt could be drawn: the chip is measured at 20 px */
static boolean chip_available(short view, short icon_type)
{
	real width;
	wchar_t word[AE_IP_NAME_SIZE];

	return (boolean)(ae_key_prompts_icon_chip(view, icon_type, 0.0f, 0.0f, 20.0f, 1.0f, FALSE, &width, word, AE_IP_NAME_SIZE) !=
		AE_ICON_CHIP_NONE);
}

static void record_set(short view, int kind, long object_index, short icon_type, wchar_t const *name, wchar_t const *detail)
{
	struct record *record = &records[view];
	long now = game_time_get();
	boolean visible = (boolean)(record->have && ae_ip_alpha(now, record->began, record->last_true, 1) > 0.0f);
	boolean same = (boolean)(visible && record->kind == kind && record->object_index == object_index);

	if (!visible)
	{
		/* (a prompt from nothing fades in) */
		record->began = now;
		record->instant = FALSE;
	}
	else if (same)
	{
		/* (back in range during its hold or fade: full at once) */
		if (record->last_true < now - 1)
			record->instant = TRUE;
	}
	else
	{
		/* (another item while one is up: the new text at once) */
		record->began = now;
		record->instant = TRUE;
	}
	record->have = TRUE;
	record->kind = kind;
	record->object_index = object_index;
	record->icon_type = icon_type;
	record->last_true = now;
	ae_ip_upper(name ? name : L"", record->name, AE_IP_NAME_SIZE);
	wcsncpy(record->detail, detail ? detail : L"", AE_IP_AFTER_SIZE - 1);
	record->detail[AE_IP_AFTER_SIZE - 1] = 0;
}

/* the state line, left as the prompt needs it: empty and out of the way in one view, reserved (empty text) in the split views */
static void state_line(short view)
{
	hud_enable_custom_state_message(view, (boolean)(local_player_count() > 1));
}

static boolean usable(short view)
{
	return (boolean)(view_ok(view) && ae_hud_style_is_ae() && hud_globals && hud_scripted_globals &&
		!hud_scripted_globals->show_hud_help_text);
}

/* an item's name: the bomb's own, else its tag's as a mark's row has it */
static void item_name(long object_index, wchar_t *name, int size)
{
	name[0] = 0;
	if (ae_bomb_snd_item_name(object_index, NONE) != NONE)
	{
		wcsncpy(name, L"BOMB", (size_t)size - 1);
		name[size - 1] = 0;
		return;
	}
	ae_marks_item_label(object_index, name, size);
}

boolean ae_item_prompt_take(short local_player_index, long player_index)
{
	struct player_datum *player;
	wchar_t name[AE_IP_NAME_SIZE];
	wchar_t detail[AE_IP_AFTER_SIZE];
	short icon_type;
	int kind;

	if (!usable(local_player_index) || player_index == NONE)
		return FALSE;
	player = player_get(player_index);
	name[0] = 0;
	detail[0] = 0;
	switch (player->action_result)
	{
	case _player_action_result_add_weapon_to_inventory:
	case _player_action_result_swap_for_weapon:
		if (weapon_try_and_get(player->action_object_index) == NULL)
			return FALSE;
		kind = player->action_result == _player_action_result_swap_for_weapon ? AE_IP_SWAP : AE_IP_PICKUP;
		icon_type = message_icon(kind == AE_IP_SWAP ? MESSAGE_SWAP_WEAPON : MESSAGE_PICKUP, ICON_ACTION);
		item_name(player->action_object_index, name, AE_IP_NAME_SIZE);
		break;
	case _player_action_result_enter_vehicle:
	case _player_action_result_evict_from_vehicle:
	{
		struct unit_datum *vehicle = unit_get(player->action_object_index);
		struct unit_definition *definition = unit_definition_get(vehicle->definition_index);

		if (player->action_seat_index < 0 || player->action_seat_index >= definition->unit.seats.count)
			return FALSE;
		kind = AE_IP_ENTER;
		icon_type = message_icon(MESSAGE_ENTER_VEHICLE, ICON_ACTION);
		wcsncpy(detail, icon_string(TAG_BLOCK_GET_ELEMENT(&definition->unit.seats, player->action_seat_index,
			struct unit_seat)->icon_text_index), AE_IP_AFTER_SIZE - 1);
		wcsncpy(name, icon_string(object_definition_get(vehicle->definition_index)->object.icon_text_index), AE_IP_NAME_SIZE - 1);
		break;
	}
	case _player_action_result_flip_vehicle:
		kind = AE_IP_FLIP;
		icon_type = message_icon(MESSAGE_FLIP_VEHICLE, ICON_ACTION);
		wcsncpy(name, icon_string(object_definition_get(object_get(player->action_object_index)->definition_index)->
			object.icon_text_index), AE_IP_NAME_SIZE - 1);
		break;
	case _player_action_result_touch_device:
	{
		struct control_datum *control = control_get(player->action_object_index);

		if (control->control.custom_name_index != NONE && global_scenario_get()->custom_object_names.index != NONE)
		{
			kind = AE_IP_CUSTOM;
			icon_type = message_icon(3, ICON_ACTION);
			wcsncpy(detail, unicode_string_list_get_string(global_scenario_get()->custom_object_names.index,
				control->control.custom_name_index), AE_IP_AFTER_SIZE - 1);
		}
		else
		{
			kind = AE_IP_DEVICE;
			icon_type = message_icon(MESSAGE_TOUCH_DEVICE, ICON_ACTION);
		}
		break;
	}
	default:
		return FALSE;
	}
	name[AE_IP_NAME_SIZE - 1] = 0;
	detail[AE_IP_AFTER_SIZE - 1] = 0;
	/* (no name, or no picture of the button: the game's own line says it) */
	if (ae_ip_kind_named(kind) && !name[0])
		return FALSE;
	if (!chip_available(local_player_index, icon_type))
		return FALSE;
	record_set(local_player_index, kind, player->action_object_index, icon_type, name, detail);
	state_line(local_player_index);
	return TRUE;
}

boolean ae_item_prompt_switch(short local_player_index, long weapon_object_index)
{
	wchar_t name[AE_IP_NAME_SIZE];
	short icon_type;

	if (!usable(local_player_index) || weapon_try_and_get(weapon_object_index) == NULL)
		return FALSE;
	item_name(weapon_object_index, name, AE_IP_NAME_SIZE);
	if (!name[0])
		return FALSE;
	icon_type = message_icon(MESSAGE_REMIND_TO_SWITCH, ICON_ROTATE_WEAPONS);
	if (!chip_available(local_player_index, icon_type))
		return FALSE;
	record_set(local_player_index, AE_IP_SWITCH, weapon_object_index, icon_type, name, L"");
	state_line(local_player_index);
	return TRUE;
}

/* ---------- drawing */

struct chip_context
{
	short view;
	short icon_type;
};

/* the chip's width at a size (a wide key's word when the rule made it one) */
static float chip_width_get(int chip_px, int action_px, void *context)
{
	struct chip_context const *chip = (struct chip_context const *)context;
	wchar_t word[AE_IP_NAME_SIZE];
	real width = 0.0f;

	switch (ae_key_prompts_icon_chip(chip->view, chip->icon_type, 0.0f, 0.0f, (real)chip_px, 1.0f, FALSE, &width, word,
		AE_IP_NAME_SIZE))
	{
	case AE_ICON_CHIP_WORD:
		return (float)ae_hud_text_measure(word, (real)action_px);
	case AE_ICON_CHIP_PICTURE:
		return (float)width;
	}
	return (float)chip_px;
}

static float measure(wchar_t const *text, float px)
{
	return (float)ae_hud_text_measure(text, (real)px);
}

static void text_at(short view, real x, real baseline, wchar_t const *text, real px, real const rgb[3], real alpha)
{
	real_argb_color color;

	if (!text || !text[0])
		return;
	color.alpha = alpha;
	color.red = rgb[0];
	color.green = rgb[1];
	color.blue = rgb[2];
	ae_hud_text_draw_ex(view, x, baseline, text, px, &color, _ae_text_outline);
}

/* a prompt's words in the record */
static boolean words_get(struct record const *record, struct ae_ip_text *text)
{
	memset(text, 0, sizeof(*text));
	if (!ae_ip_words(record->kind, record->detail, text->before, text->after, AE_IP_AFTER_SIZE))
		return FALSE;
	/* (HOLD TO ACT: getting in or flipping is a hold too, as the classic prompt says) */
	if ((record->kind == AE_IP_ENTER || record->kind == AE_IP_FLIP) && ae_hold_act_active())
		wcscpy(text->before, L"Hold");
	if (ae_ip_kind_named(record->kind))
	{
		wcsncpy(text->name, record->name, AE_IP_NAME_SIZE - 1);
		text->name[AE_IP_NAME_SIZE - 1] = 0;
	}
	return TRUE;
}

static boolean player_alive(short local_player_index)
{
	long player_index = local_player_get_player_index(local_player_index);
	struct player_datum *player = player_index == NONE ? NULL : player_try_and_get(player_index);
	struct object_datum *unit;

	if (!player || player->unit_index == NONE)
		return FALSE;
	unit = object_try_and_get_and_verify_type(player->unit_index, _object_mask_unit);
	return (boolean)(unit && !TEST_FLAG(unit->object.damage_flags, _object_dead_bit));
}

/* whether the thing the prompt was about is no longer to take: an item in a hand, a vehicle the player is in */
static boolean taken(short local_player_index, struct record const *record)
{
	long player_index = local_player_get_player_index(local_player_index);
	struct player_datum *player = player_index == NONE ? NULL : player_try_and_get(player_index);
	struct object_datum *object;

	if (record->object_index == NONE || !player)
		return FALSE;
	object = object_try_and_get_and_verify_type(record->object_index, _object_mask_all);
	if (!object)
		return (boolean)(record->kind != AE_IP_DEVICE && record->kind != AE_IP_CUSTOM);
	if (record->kind == AE_IP_PICKUP || record->kind == AE_IP_SWAP)
		return (boolean)(object->object.parent_object_index != NONE);
	if (record->kind == AE_IP_ENTER && player->unit_index != NONE)
	{
		struct object_datum *unit = object_try_and_get_and_verify_type(player->unit_index, _object_mask_unit);

		return (boolean)(unit && unit->object.parent_object_index == record->object_index);
	}
	return FALSE;
}

/* the view's pixel size, middle and scale; FALSE where the view has no size */
static boolean view_get(real *width, real *height, real *view_scale)
{
	real ppu = ae_hud_pixels_per_unit();

	if (!(ppu > 0.0f))
		return FALSE;
	*width = (real)(render.camera.viewport_bounds.x1 - render.camera.viewport_bounds.x0) * ppu;
	*height = (real)(render.camera.viewport_bounds.y1 - render.camera.viewport_bounds.y0) * ppu;
	*view_scale = ae_score_strip_view_scale();
	return (boolean)(*width > 0.0f && *height > 0.0f && *view_scale > 0.0f);
}

void ae_item_prompt_draw(short local_player_index)
{
	struct record *record;
	struct ae_ip_text text;
	struct ae_ip_layout layout;
	struct ae_ip_place place;
	struct chip_context context;
	wchar_t word[AE_IP_NAME_SIZE];
	real view_width, view_height, view_scale, k, alpha, top, chip_width = 0.0f;
	real box_top, box_bottom, box_left, box_right;
	long now = game_time_get();
	int drawable_width = 0, drawable_height = 0, chip_kind;
	int entry;
	boolean dead;
	real flash, closing_alpha;

	if (!view_ok(local_player_index))
		return;
	record = &records[local_player_index];
	record->drawn = FALSE;
	if (!record->have || local_player_count() > 1 || !ae_hud_style_is_ae() || !hud_scripted_globals ||
		!hud_scripted_globals->show_hud)
	{
		return;
	}
	if (cinematic_in_progress() || !player_alive(local_player_index))
	{
		/* (dead, or a cinematic: gone, not held) */
		if (!player_alive(local_player_index))
			record->have = FALSE;
		return;
	}
	if (record->last_true < now && taken(local_player_index, record))
	{
		/* (taken: gone that tick, no hold and no fade) */
		record->have = FALSE;
		return;
	}
	/* (as the aim prompt: not under the scores, the pause, or the ring) */
	if (game_engine_running() && (game_engine_scoreboard_shown(local_player_index) > 0.0f ||
		game_engine_scoreboard_held(local_player_index) || ae_pause_player_open(local_player_index) ||
		ae_wheel_open(local_player_index) || ae_wheel_closing(local_player_index, &entry, &dead, &flash, &closing_alpha)))
	{
		return;
	}
	alpha = ae_ip_alpha(now, record->began, record->last_true, record->instant);
	if (record->last_true < now && now - record->last_true > AE_IP_HOLD_TICKS + AE_IP_FADE_OUT_TICKS)
	{
		record->have = FALSE;
		return;
	}
	if (!(alpha > 0.0f) || !view_get(&view_width, &view_height, &view_scale) || !words_get(record, &text))
		return;
	platform_video_drawable_size(&drawable_width, &drawable_height);
	k = ae_ip_scale(hud_item_timers_ae_waypoint_scale(0.0f), (float)drawable_height);
	if (!(k > 0.0f))
		return;
	context.view = local_player_index;
	context.icon_type = record->icon_type;
	ae_ip_layout_get(k, ae_ip_width_limit(view_scale), &text, measure, chip_width_get, &context, &layout);
	top = ae_ip_top(view_height / 2.0f, view_scale);
	ae_ip_place_get(view_width / 2.0f, top, &layout, &place);
	if (config_string("debug.ae_message_test")[0])
	{
		/* (the debug lines' log: the sizes and where the block went, when they change) */
		static float last[PLAYERS][8];
		float now_values[8];
		int index;

		now_values[0] = (float)record->kind;
		now_values[1] = k;
		now_values[2] = (float)layout.drawn.action;
		now_values[3] = (float)layout.drawn.name;
		now_values[4] = (float)layout.drawn.chip;
		now_values[5] = place.name_baseline;
		now_values[6] = place.chip_middle;
		now_values[7] = layout.row_width;
		for (index = 0; index < 8 && last[local_player_index][index] == now_values[index]; index++)
			;
		if (index < 8)
		{
			memcpy(last[local_player_index], now_values, sizeof(now_values));
			platform_log("item prompt: view %d kind %d k=%.2f view scale %.2f name %d/%d action %d/%d chip %d gap %d row gap %d top %.0f "
				"name baseline %.0f chip middle %.1f row %.0f of %.0f", (int)local_player_index, record->kind, k, view_scale,
				layout.drawn.name, layout.nominal.name, layout.drawn.action, layout.nominal.action, layout.drawn.chip,
				layout.drawn.gap, layout.drawn.row_gap, top, place.name_baseline, place.chip_middle, layout.row_width,
				ae_ip_width_limit(view_scale));
		}
	}

	if (layout.name[0])
		text_at(local_player_index, place.name_x, place.name_baseline, layout.name, (real)layout.drawn.name, white_rgb, alpha);
	text_at(local_player_index, place.before_x, place.action_baseline, text.before, (real)layout.drawn.action, white_rgb, alpha);
	chip_kind = ae_key_prompts_icon_chip(local_player_index, record->icon_type, place.chip_x, place.chip_middle,
		(real)layout.drawn.chip, alpha, TRUE, &chip_width, word, AE_IP_NAME_SIZE);
	if (chip_kind == AE_ICON_CHIP_WORD)
		text_at(local_player_index, place.chip_x, place.action_baseline, word, (real)layout.drawn.action, ice_rgb, alpha);
	text_at(local_player_index, place.after_x, place.action_baseline, layout.after, (real)layout.drawn.action, white_rgb, alpha);

	/* the block's box with its outlines, for the aim prompt to keep clear of */
	box_left = place.row_x;
	box_right = place.row_x + layout.row_width;
	if (layout.name[0])
	{
		if (place.name_x < box_left)
			box_left = place.name_x;
		if (place.name_x + layout.name_width > box_right)
			box_right = place.name_x + layout.name_width;
		box_top = place.name_baseline - 0.70f * (real)layout.drawn.name;
	}
	else
		box_top = place.chip_top;
	box_bottom = place.chip_top + (real)layout.drawn.chip;
	record->drawn = TRUE;
	record->drawn_at = now;
	record->box[0] = box_left - 3.0f;
	record->box[1] = box_top - 3.0f;
	record->box[2] = box_right + 3.0f;
	record->box[3] = box_bottom + 3.0f;
}

boolean ae_item_prompt_box(short local_player_index, float box[4])
{
	struct record const *record;
	long now = game_time_get();

	if (!view_ok(local_player_index))
		return FALSE;
	record = &records[local_player_index];
	if (!record->drawn || now < record->drawn_at || now - record->drawn_at > 2)
		return FALSE;
	memcpy(box, record->box, sizeof(record->box));
	return TRUE;
}

/* ---------- the split line */

void ae_item_prompt_split_draw(short local_player_index)
{
	struct record const *record;
	struct ae_ip_text text;
	struct chip_context context;
	wchar_t name[AE_IP_NAME_SIZE], after[AE_IP_AFTER_SIZE], word[AE_IP_NAME_SIZE];
	real left, view_width, view_scale, px, baseline, cap, x;
	real chip_px, gap, name_gap, chip_width = 0.0f, before_width, middle;
	int chip_kind;
	boolean visible;

	if (!view_ok(local_player_index) || local_player_count() < 2)
		return;
	record = &records[local_player_index];
	if (!record->have || record->last_true != game_time_get() || !ae_hud_style_is_ae() || !words_get(record, &text))
		return;
	if (!ae_hud_message_prompt_begin(&left, &view_width, &view_scale, &px, &baseline, &cap))
		return;
	context.view = local_player_index;
	context.icon_type = record->icon_type;
	/* (the list's own size; the chip about 1.35 x the capitals, a third of an em from the words, the name an em from the action) */
	chip_px = (real)ae_ip_split_chip((float)px);
	gap = (real)ae_ip_split_gap((float)px);
	name_gap = (real)ae_ip_split_name_gap((float)px);
	chip_width = (real)chip_width_get((int)chip_px, (int)(px + 0.5f), &context);
	before_width = ae_hud_text_measure(text.before, px);
	/* (the name first, cut only when the line without the action's words is already past the view's edge less 16 s; the words after the
	chip get what is left, cut with an ellipsis: a long custom device's own words) */
	name[0] = 0;
	after[0] = 0;
	ae_ip_split_fit((float)(view_width - 16.0f * view_scale - left), (float)name_gap, (float)(before_width + gap + chip_width),
		text.after[0] ? (float)gap : 0.0f, text.name, text.after, measure, (float)px, name, after);
	if (name[0])
	{
		ae_hud_message_prompt_text(name, white_rgb);
		ae_hud_message_prompt_space(name_gap);
	}
	ae_hud_message_prompt_text(text.before, grey_rgb);
	ae_hud_message_prompt_space(gap);
	middle = baseline - cap / 2.0f;
	if (ae_key_prompts_icon_chip(local_player_index, record->icon_type, 0.0f, middle, chip_px, 1.0f, FALSE, &chip_width, word,
		AE_IP_NAME_SIZE) == AE_ICON_CHIP_WORD)
	{
		/* (a wide key's name, a plain word in the action's colour of the one view's rule) */
		ae_hud_message_prompt_text(word, ice_rgb);
	}
	else
	{
		visible = ae_hud_message_prompt_reserve(chip_width, &x);
		chip_kind = visible ? ae_key_prompts_icon_chip(local_player_index, record->icon_type, x, middle, chip_px, 1.0f, TRUE,
			&chip_width, word, AE_IP_NAME_SIZE) : AE_ICON_CHIP_NONE;
		(void)chip_kind;
	}
	if (after[0])
	{
		ae_hud_message_prompt_space(gap);
		ae_hud_message_prompt_text(after, grey_rgb);
	}
}
