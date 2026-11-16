/*
AE_SCREEN_TAB_MENU.C

See ae_screen_tab_menu.h. The list is AE's flow rows (ae_setup_flow.h) in a panel of its own over a scrim: 40 u rows,
QUIT under a rule. AE's popover motion fades and scales it in (none with REDUCE MOTION).
*/

#include <stdio.h>
#include <string.h>

#include "cseries.h"
#include "../src/ae_draw.h"
#include "ae_screen_keys.h"
#include "ae_screen_main_menu.h"
#include "ae_screen_profiles.h"
#include "ae_screen_settings.h"
#include "ae_screen_tab_menu.h"
#include "ae_settings_rows.h"
#include "ae_setup_flow.h"
#include "ae_sound.h"
#include "ae_strings.h"
#include "ae_ui.h"
#include "ae_widgets.h"

void platform_log(char const *format, ...);

enum
{
	TAB_HIT = 0x76E0,
	HIT_ROWS = TAB_HIT + 1, HIT_PROMPTS
};
enum { ROW_PLAYER = AE_FLOW_ROW_OWN, ROW_CONTROLLER, ROW_KEYBOARD, ROW_SETTINGS, ROW_QUIT };

#define PANEL_WIDTH_U 520.0f
#define PANEL_PAD_U 24.0f
#define SCRIM 0x0A0F1499u

static struct
{
	struct ae_flow_screen flow;
	short player;
} tab;

static struct ae_screen_class const tab_class;

static void rows(void)
{
	struct ae_flow_screen *flow = &tab.flow;
	short count = 0;

	flow->rows[count++] = (struct ae_flow_row){ ROW_PLAYER, -1, "PLAYER", NULL, "Your profile: name, colours and controls.",
		NULL };
	flow->rows[count++] = (struct ae_flow_row){ ROW_CONTROLLER, -1, "CONTROLLER", NULL,
		"Your controller's buttons, sticks and feel.", NULL };
	flow->rows[count++] = (struct ae_flow_row){ ROW_KEYBOARD, -1, "KEYBOARD", NULL, "The keys and the mouse.", NULL };
	flow->rows[count++] = (struct ae_flow_row){ ROW_SETTINGS, -1, "SETTINGS", NULL,
		"Video, audio, controls, network and menu settings.", NULL };
	flow->rows[count++] = (struct ae_flow_row){ AE_FLOW_ROW_HEADER, -1, "", NULL, NULL, NULL };
	flow->rows[count++] = (struct ae_flow_row){ ROW_QUIT, -1, "QUIT", NULL, "Asks first, then the game closes.", NULL };
	flow->count = count;
}

static void close_tab(void)
{
	ae_ui_remove(&tab);
}

static void accept(short row)
{
	struct ae_flow_row const *r;
	int opened = 0;

	if (row < 0 || row >= tab.flow.count || tab.flow.rows[row].kind == AE_FLOW_ROW_HEADER)
		return;
	r = &tab.flow.rows[row];
	platform_log("ae tab menu: %s", r->label);
	/* (the menu closes first: the screen it opens comes back to the main menu) */
	close_tab();
	switch (r->kind)
	{
	case ROW_PLAYER:
		opened = ae_screen_profiles_open();
		break;
	case ROW_CONTROLLER:
		opened = ae_screen_settings_open_group(AE_SETTINGS_CONTROLS, tab.player);
		break;
	case ROW_KEYBOARD:
		opened = ae_screen_keys_open(NULL);
		break;
	case ROW_SETTINGS:
		opened = ae_screen_settings_open(tab.player);
		break;
	case ROW_QUIT:
		opened = ae_screen_main_menu_quit_ask();
		break;
	default:
		break;
	}
	ae_sound_request(opened ? AE_SOUND_FORWARD : AE_SOUND_FAILURE, 0);
}

static void tab_draw(struct ae_screen *instance)
{
	struct ae_flow_screen *flow = &tab.flow;
	float width, height, x, y, pad, row_height = 0.0f;
	short index;

	(void)instance;
	ae_flow_full_density(flow);
	rows();
	ae_draw_rect(flow->frame.rect.x, flow->frame.rect.y, flow->frame.rect.width, flow->frame.rect.height, 0.0f, SCRIM);
	pad = ae_flow_u(flow, PANEL_PAD_U);
	width = ae_flow_u(flow, PANEL_WIDTH_U);
	/* (its height: the rows as drawn last frame, else a first guess) */
	for (index = 0; index < flow->count; index++)
		row_height += flow->rects[index].width > 0.0f ? flow->rects[index].height + ae_flow_u(flow, 8.0f) :
			ae_flow_u(flow, 24.0f);
	height = row_height + 2.0f * pad + ae_flow_u(flow, 48.0f);
	x = flow->frame.rect.x + (flow->frame.rect.width - width) * 0.5f;
	y = flow->frame.rect.y + (flow->frame.rect.height - height) * 0.5f;
	ae_draw_rect(x, y, width, height, 0.0f, AE_COLOR_PANEL);
	ae_draw_outline(x, y, width, height, 0.0f, ae_flow_u(flow, AE_FLOW_RULE_U), AE_COLOR_RULE);
	ae_draw_text_tracked(AE_FONT_BODY, ae_size(&flow->density, 16.0f, 12.0f), 0.12f, x + pad, y + pad, AE_ALIGN_LEFT,
		AE_COLOR_MUTED, "MENU");
	y += pad + ae_flow_u(flow, 40.0f);
	for (index = 0; index < flow->count; index++)
	{
		struct ae_flow_row const *row = &flow->rows[index];
		struct ae_row widget;

		if (row->kind == AE_FLOW_ROW_HEADER)
		{
			/* (QUIT under a rule) */
			ae_draw_rect(x + pad, y + ae_flow_u(flow, 8.0f), width - 2.0f * pad, ae_flow_u(flow, AE_FLOW_RULE_U), 0.0f,
				AE_COLOR_RULE);
			flow->rects[index].width = 0.0f;
			y += ae_flow_u(flow, 24.0f);
			continue;
		}
		memset(&widget, 0, sizeof(widget));
		widget.label = row->label;
		if (index == flow->focus)
			widget.flags |= AE_ROW_FOCUSED;
		if (row->kind == ROW_QUIT)
			widget.flags |= AE_ROW_DESTRUCTIVE;
		flow->rects[index].x = x + pad;
		flow->rects[index].y = y;
		flow->rects[index].width = width - 2.0f * pad;
		flow->rects[index].height = ae_widget_row(&flow->density, x + pad, y, width - 2.0f * pad, &widget, HIT_ROWS, index);
		y += flow->rects[index].height + ae_flow_u(flow, 8.0f);
	}
	flow->prompt_count = 0;
	flow->prompts[flow->prompt_count++] = ae_flow_prompt(AE_BUTTON_A, ae_string(AE_STR_SELECT), AE_ACTION_ACCEPT);
	flow->prompts[flow->prompt_count++] = ae_flow_prompt(AE_BUTTON_B, ae_string(AE_STR_BACK), AE_ACTION_BACK);
	ae_flow_draw_prompts(flow);
}

static int tab_handle(struct ae_screen *instance, struct ae_event const *event)
{
	struct ae_flow_screen *flow = &tab.flow;

	(void)instance;
	switch (event->action)
	{
	case AE_ACTION_BACK:
	case AE_ACTION_START:
		platform_log("ae tab menu: closed");
		ae_sound_request(AE_SOUND_BACK, 0);
		close_tab();
		return 1;
	case AE_ACTION_TAB_NEXT:
	case AE_ACTION_TAB_PREVIOUS:
		/* (the keyboard's Tab closes it as it opened it) */
		if (event->device == AE_DEVICE_KEYBOARD_MOUSE)
		{
			platform_log("ae tab menu: closed");
			ae_sound_request(AE_SOUND_BACK, 0);
			close_tab();
		}
		return 1;
	case AE_ACTION_UP:
	case AE_ACTION_DOWN:
		ae_flow_focus_step(flow, event->action == AE_ACTION_UP ? -1 : 1, event->repeat);
		return 1;
	case AE_ACTION_ACCEPT:
		accept(flow->focus);
		return 1;
	default:
		return 1;
	}
}

static void tab_pointer(struct ae_screen *instance, struct ae_pointer const *pointer)
{
	struct ae_flow_screen *flow = &tab.flow;
	struct ae_event event;
	struct ae_hit hit;

	if (pointer->right_clicks)
	{
		event.action = AE_ACTION_BACK;
		tab_handle(instance, &event);
		return;
	}
	if (ae_prompts_pointer(flow->prompts, flow->prompt_count, pointer, HIT_PROMPTS, &event))
	{
		tab_handle(instance, &event);
		return;
	}
	if (ae_hit_at(pointer->x, pointer->y, 0, &hit) && hit.id == HIT_ROWS)
	{
		if (pointer->moved)
			flow->focus = hit.index;
		if (pointer->left_clicks)
			accept(hit.index);
	}
	else if (pointer->left_clicks)
	{
		platform_log("ae tab menu: closed");
		close_tab();
	}
}

static struct ae_screen_class const tab_class =
{
	.name = "tab menu", .handle = tab_handle, .draw = tab_draw, .pointer = tab_pointer, .popover = 1,
};

int ae_screen_tab_menu_open(struct ae_flow_screen const *under, short local_player)
{
	(void)under;
	if (ae_ui_holds(&tab))
		return 0;
	memset(&tab, 0, sizeof(tab));
	tab.player = local_player;
	tab.flow.hit_rows = HIT_ROWS;
	tab.flow.hit_prompts = HIT_PROMPTS;
	if (!ae_ui_push(&tab_class, AE_OWNER_ANY, &tab))
		return 0;
	rows();
	platform_log("ae tab menu: open");
	return 1;
}

int ae_screen_tab_menu_is(void const *data)
{
	return data == &tab;
}

void ae_screen_tab_menu_close(void)
{
	(void)ae_ui_remove(&tab);
}
