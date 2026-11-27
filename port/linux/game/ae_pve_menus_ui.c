/*
AE_PVE_MENUS_UI.C

See ae_pve_menus_ui.h.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cseries.h"
#include "halo_skulls.h"
#include "../src/ae_draw.h"
#include "../src/ae_ce_art.h"
#include "ae_skull_pack.h"
#include "ae_skull_pack_rules.h"
#include "../src/port_config.h"
#include "ae_glue_campaign.h"
#include "ae_input.h"
#include "ae_medals.h"
#include "ae_pve_menus_rules.h"
#include "ae_randomizer_menus_rules.h"
#include "ae_pve_menus_ui.h"
#include "ae_sound.h"
#include "ae_strings.h"
#include "ae_style.h"
#include "ae_ui.h"
#include "ae_widgets.h"

void platform_log(char const *format, ...);
/* ae_medal_pack.h (its types need the game's headers in another order) */
boolean ae_medal_pack_has_picture(int medal);
int ae_medal_pack_menu_image(int medal);

int ae_pve_menus_ui_write(const char *key, const char *value)
{
	if (!ae_pve_menus_key_allowed(key) && !ae_randomizer_menus_key(key))
	{
		platform_log("ae pve menus: %s is not a setting these screens write", key ? key : "(none)");
		return 0;
	}
	return config_write(key, value);
}

const char *ae_pve_menus_ui_read(void *context, const char *key)
{
	static char text[8][96];
	static int next;
	char *buffer = text[next++ & 7];

	(void)context;
	buffer[0] = 0;
	if ((!ae_pve_menus_key_allowed(key) && !ae_randomizer_menus_key(key)) || !config_text(key, buffer, sizeof(text[0])))
		return "";
	return buffer;
}

void ae_pve_menus_ui_profile(char *name, int size)
{
	struct ae_campaign campaign;
	int each;

	snprintf(name, (size_t)size, "NO PROFILE");
	if (!ae_campaign_read(&campaign) || !campaign.have_profile)
		return;
	snprintf(name, (size_t)size, "%s", campaign.profile);
	for (each = 0; name[each]; each++)
		if (name[each] >= 'a' && name[each] <= 'z')
			name[each] = (char)(name[each] - 32);
}

int ae_pve_menus_ui_skulls_on_count(void)
{
	struct skull_set on = skull_set_intersection(skulls_set_from_text(config_string("game.skulls_active")), skulls_unlocked_set());
	int skull, count = 0;

	for (skull = 0; skull < NUMBER_OF_SKULLS; skull++)
		if (skull_set_has(&on, skull))
			count++;
	return count;
}

static int ce_images[128];

struct skull_image
{
	int id;
	unsigned short width, height;
};

static struct skull_image ce_skull_images[NUMBER_OF_SKULLS];

/* a campaign medal's picture from the player's own MCC install (its medal pack), made when asked: 0 when there is none */
static int mcc_medal_picture(const char *key)
{
	int medal;

	for (medal = 0; medal < AE_MEDAL_COUNT; medal++)
	{
		const char *medal_key = ae_medal_key(medal);

		if (medal_key && !strcmp(medal_key, key) && ae_medal_pack_has_picture(medal))
			return ae_medal_pack_menu_image(medal);
	}
	return 0;
}

int ae_pve_menus_ui_ce_picture(const char *name)
{
	unsigned int each;

	if (!name || !ae_draw_available())
		return 0;
	/* (a medal: MCC's own picture for a player who has MCC, else Content Evolved's below) */
	if (!strncmp(name, "medal_", 6))
	{
		int image = mcc_medal_picture(name);

		if (image)
			return image;
	}
	for (each = 0; each < ae_ce_art_count && each < sizeof(ce_images) / sizeof(ce_images[0]); each++)
	{
		if (strcmp(ae_ce_art[each].name, name))
			continue;
		if (ce_images[each] && ae_draw_image_live(ce_images[each]))
			return ce_images[each];
		ce_images[each] = ae_draw_image_load((const unsigned char *)ae_ce_art[each].rgba, (int)ae_ce_art[each].width,
			(int)ae_ce_art[each].height);
		return ce_images[each];
	}
	return 0;
}

/* a Content Evolved skull picture, copied and trimmed when its drawing image is made */
static int ce_skull_picture(int skull, int *width, int *height)
{
	struct skull_image *picture;
	unsigned int each;

	*width = *height = 0;
	if (skull < 0 || skull >= NUMBER_OF_SKULLS || !ae_draw_available())
		return 0;
	picture = &ce_skull_images[skull];
	if (picture->id && ae_draw_image_live(picture->id))
	{
		*width = picture->width;
		*height = picture->height;
		return picture->id;
	}
	for (each = 0; each < ae_ce_art_count; each++)
	{
		struct ae_ce_art const *art = &ae_ce_art[each];
		unsigned char *rgba;
		int trimmed_width, trimmed_height;

		if (strcmp(art->name, skull_descriptions[skull].key))
			continue;
		rgba = malloc((size_t)art->width * art->height * 4);
		if (!rgba)
			return 0;
		memcpy(rgba, art->rgba, (size_t)art->width * art->height * 4);
		if (!ae_skull_rgba_trim(rgba, (int)art->width, (int)art->height, &trimmed_width, &trimmed_height))
		{
			free(rgba);
			return 0;
		}
		picture->id = ae_draw_image_load(rgba, trimmed_width, trimmed_height);
		free(rgba);
		if (!picture->id)
			return 0;
		picture->width = (unsigned short)trimmed_width;
		picture->height = (unsigned short)trimmed_height;
		*width = trimmed_width;
		*height = trimmed_height;
		return picture->id;
	}
	return 0;
}

int ae_pve_menus_ui_skulls_use_mcc(short const *skulls, int count)
{
	int each;

	if (!skulls || count <= 0)
		return 0;
	for (each = 0; each < count; each++)
		if (skulls[each] < 0 || skulls[each] >= NUMBER_OF_SKULLS || !ae_skull_pack_has_picture(skulls[each]))
			return 0;
	return 1;
}

void ae_pve_menus_ui_skull_picture(int skull, int use_mcc, float x, float y, float width, float height, unsigned int tint)
{
	int image_width, image_height;
	int image;
	float scale, draw_width, draw_height;

	if (width <= 0.0f || height <= 0.0f)
		return;
	image = use_mcc ? ae_skull_pack_menu_image(skull, &image_width, &image_height) :
		ce_skull_picture(skull, &image_width, &image_height);
	if (!image || image_width <= 0 || image_height <= 0)
		return;
	scale = width / (float)image_width;
	if ((float)image_height * scale > height)
		scale = height / (float)image_height;
	draw_width = (float)image_width * scale;
	draw_height = (float)image_height * scale;
	ae_draw_image(image, x + (width - draw_width) * 0.5f, y + (height - draw_height) * 0.5f, draw_width, draw_height, tint);
}

void ae_pve_menus_ui_text(struct ae_flow_screen const *flow, int font, float size_u, float x, float center_y, int align,
	unsigned int color, const char *text)
{
	float size = ae_flow_u(flow, size_u);

	ae_draw_text(font, size, x, ae_flow_cap_top(font, size, center_y), align, color, text);
}

int ae_pve_menus_ui_paragraph(struct ae_flow_screen const *flow, float size_u, float pitch_u, float x, float center_y, float width,
	int lines, unsigned int color, const char *text)
{
	char wrapped[6][160];
	int count, each;

	if (lines > 6)
		lines = 6;
	count = ae_wrap_text(AE_FONT_BODY, ae_flow_u(flow, size_u), text, width, wrapped, lines);
	for (each = 0; each < count; each++)
		ae_pve_menus_ui_text(flow, AE_FONT_BODY, size_u, x, center_y + pitch_u * ae_flow_u(flow, 1.0f) * (float)each, AE_ALIGN_LEFT,
			color, wrapped[each]);
	return count;
}

void ae_pve_menus_ui_side(struct ae_flow_screen const *flow, float *x, float *width)
{
	*x = flow->frame.rect.x + flow->frame.rect.width * AE_FLOW_PANEL_X;
	*width = flow->frame.rect.x + flow->frame.rect.width - flow->frame.margin - *x;
}

void ae_pve_menus_ui_plate(struct ae_flow_screen const *flow, float x, float width, float top_u, float height_u)
{
	float u = ae_flow_u(flow, 1.0f);

	ae_draw_rect(x - 24.0f * u, flow->frame.rect.y + top_u * u, width + 48.0f * u, height_u * u, 0.0f, AE_COLOR_PANEL);
}

void ae_pve_menus_ui_back_prompt(struct ae_flow_screen *flow)
{
	flow->prompts[flow->prompt_count++] = ae_flow_prompt(AE_BUTTON_B, ae_string(AE_STR_BACK), AE_ACTION_BACK);
}

/* ---------- a screen of rows */

static struct ae_screen_class const rows_class;

static void rows_draw(struct ae_screen *instance)
{
	struct ae_pvem_rows *rows = instance->data;
	struct ae_flow_screen *flow = &rows->flow;
	float x, width;
	short focus;

	ae_flow_full_density(flow);
	rows->build(rows);
	ae_flow_title(flow, rows->title, rows->crumb);
	/* (a dialog asked for is opened once the frame has its place for this draw: the dialog's bounds are the frame's) */
	if (rows->dialog_pending)
	{
		rows->dialog_pending = 0;
		if (rows->dialog_open)
			rows->dialog_open(rows);
	}
	if (rows->backing)
	{
		float u = ae_flow_u(flow, 1.0f), left = ae_flow_left_x(flow) - 24.0f * u, top = ae_frame_y(&flow->frame, AE_FLOW_CONTENT_Y) - 12.0f * u;

		ae_draw_rect(left, top, flow->frame.rect.width * AE_FLOW_ROWS_WIDTH + 48.0f * u, ae_flow_content_bottom(flow) - top, 0.0f, AE_COLOR_PANEL);
	}
	ae_flow_draw_rows(flow, flow->frame.rect.width * AE_FLOW_ROWS_WIDTH);
	ae_pve_menus_ui_side(flow, &x, &width);
	if (rows->side)
		rows->side(rows, x, width);
	flow->prompt_count = 0;
	focus = flow->focus;
	if (focus >= 0 && focus < flow->count && flow->rows[focus].kind != AE_FLOW_ROW_HEADER && !flow->rows[focus].quiet)
		flow->prompts[flow->prompt_count++] = ae_flow_prompt(AE_BUTTON_A, rows->a_word ? rows->a_word(rows, focus) : "Change",
			AE_ACTION_ACCEPT);
	if (rows->x_action && !rows->x_hidden)
		flow->prompts[flow->prompt_count++] = ae_flow_prompt(AE_BUTTON_X, rows->x_word, AE_ACTION_X);
	if (rows->y_action && !rows->y_hidden)
		flow->prompts[flow->prompt_count++] = ae_flow_prompt(AE_BUTTON_Y, rows->y_word, AE_ACTION_Y);
	flow->prompts[flow->prompt_count++] = ae_flow_prompt(AE_BUTTON_B, ae_string(AE_STR_BACK), AE_ACTION_BACK);
	ae_flow_draw_prompts(flow);
}

/* a row asked to change: a dimmed row refuses with the nudge */
static void rows_step(struct ae_pvem_rows *rows, int row, int direction)
{
	struct ae_flow_row const *r;

	if (row < 0 || row >= rows->flow.count)
		return;
	r = &rows->flow.rows[row];
	if (r->kind == AE_FLOW_ROW_HEADER)
		return;
	if (r->reason && !r->danger)
	{
		ae_flow_nudge(&rows->flow, (short)row);
		return;
	}
	rows->step(rows, row, direction);
}

static int rows_handle(struct ae_screen *instance, struct ae_event const *event)
{
	struct ae_pvem_rows *rows = instance->data;

	if (rows->handle_first && rows->handle_first(rows, event))
		return 1;
	switch (event->action)
	{
	case AE_ACTION_BACK:
		return 0;
	case AE_ACTION_UP:
	case AE_ACTION_DOWN:
		ae_flow_focus_step(&rows->flow, event->action == AE_ACTION_UP ? -1 : 1, event->repeat);
		return 1;
	case AE_ACTION_LEFT:
	case AE_ACTION_RIGHT:
		rows_step(rows, rows->flow.focus, event->action == AE_ACTION_RIGHT ? 1 : -1);
		return 1;
	case AE_ACTION_ACCEPT:
		rows_step(rows, rows->flow.focus, 1);
		return 1;
	case AE_ACTION_X:
		if (rows->x_action && !rows->x_hidden)
			rows->x_action(rows);
		return 1;
	case AE_ACTION_Y:
		if (rows->y_action && !rows->y_hidden)
			rows->y_action(rows);
		return 1;
	default:
		return 1;
	}
}

static void rows_pointer(struct ae_screen *instance, struct ae_pointer const *pointer)
{
	struct ae_pvem_rows *rows = instance->data;
	struct ae_event event;
	short row;

	if (ae_prompts_pointer(rows->flow.prompts, rows->flow.prompt_count, pointer, rows->flow.hit_prompts, &event))
	{
		if (event.action == AE_ACTION_BACK)
		{
			ae_sound_request(AE_SOUND_BACK, 0);
			ae_ui_pop();
		}
		else
			rows_handle(instance, &event);
		return;
	}
	row = ae_flow_rows_pointer(&rows->flow, pointer);
	if (row >= 0)
		rows_step(rows, row, 1);
	else if (row == -2 || row == -3)
		rows_step(rows, rows->flow.focus, row == -3 ? 1 : -1);
}

static void rows_leave(struct ae_screen *instance)
{
	struct ae_pvem_rows *rows = instance->data;

	platform_log("ae %s: closed", rows->name);
}

static struct ae_screen_class const rows_class =
{
	.name = "pve rows", .leave = rows_leave, .handle = rows_handle, .draw = rows_draw, .pointer = rows_pointer,
};

void ae_pve_menus_rows_focus_first(struct ae_pvem_rows *rows)
{
	rows->build(rows);
	for (rows->flow.focus = 0; rows->flow.focus < rows->flow.count - 1 &&
		rows->flow.rows[rows->flow.focus].kind == AE_FLOW_ROW_HEADER; rows->flow.focus++)
		;
}

int ae_pve_menus_rows_open(struct ae_pvem_rows *rows)
{
	rows->flow.hit_rows = rows->hit_base;
	rows->flow.hit_prompts = (short)(rows->hit_base + 1);
	if (!ae_ui_push(&rows_class, AE_OWNER_ANY, rows))
		return 0;
	ae_pve_menus_rows_focus_first(rows);
	platform_log("ae %s: open (%s)", rows->name, rows->crumb);
	return 1;
}

void ae_pve_menus_rows_close(struct ae_pvem_rows *rows)
{
	(void)ae_ui_remove(rows);
}
