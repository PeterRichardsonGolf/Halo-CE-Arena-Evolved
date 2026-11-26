/*
AE_SCREEN_DAILY_SERVER.C

SERVER ADDRESS (ae_screen_daily_server.h): one text field over the settings screen, as NAME THIS SETUP is. A, Enter or X types; Enter
keeps what was typed when it is an https address (ae_daily_server_clean), else the field shows why and keeps the text; B backs out
and leaves the setting as it was.
*/

#include <stdio.h>
#include <string.h>
#include "cseries.h"
#include "../src/ae_draw.h"
#include "../src/port_config.h"
#include "ae_daily_rules.h"
#include "ae_motion.h"
#include "ae_pve_menus_ui.h"
#include "ae_screen_daily_server.h"
#include "ae_setup_flow.h"
#include "ae_sound.h"
#include "ae_strings.h"
#include "ae_style.h"
#include "ae_ui.h"
#include "ae_widgets.h"

void platform_log(char const *format, ...);

enum { SERVER_HIT = 0x7DE0 };
enum { SERVER_ROWS = SERVER_HIT + 1, SERVER_PROMPTS, SERVER_FIELD };

static struct
{
	struct ae_flow_screen flow;
	struct ae_text text;
	struct ae_field_edit edit;
	int typing, keyboard_open;
	unsigned long caret_since;
	struct ae_rect well;
	char error[96], error_for[AE_TEXT_MAXIMUM];
} server;

static struct ae_screen_class const server_class;

void ae_screen_daily_server_summary(char *text, int size)
{
	ae_daily_server_address_shown(config_string("community.server_mode"), config_string("community.custom_server"), text,
		(size_t)size);
}

static void server_save(void)
{
	char clean[AE_DAILY_SERVER_MAXIMUM + 16];

	if (!ae_daily_server_clean(server.text.text, clean, sizeof(clean)))
	{
		snprintf(server.error, sizeof(server.error), "Only an https address works, as https://example.com.");
		snprintf(server.error_for, sizeof(server.error_for), "%s", server.text.text);
		ae_sound_request(AE_SOUND_FAILURE, 0);
		return;
	}
	ae_pve_menus_ui_write("community.custom_server", clean);
	platform_log("ae daily server: address kept");
	ae_sound_request(AE_SOUND_FORWARD, 0);
	ae_ui_remove(&server);
}

static void typing_done(int keep, void *context)
{
	(void)keep;
	(void)context;
	server.typing = 0;
}

static void typing_begin(void)
{
	server.edit.done = typing_done;
	server.edit.context = NULL;
	server.edit.holder = &server;
	ae_field_edit_begin(&server.edit, &server.text);
	ae_text_select_all(&server.text);
	server.typing = 1;
	server.caret_since = ae_motion_now();
}

static void keyboard_done(int keep, void *context)
{
	(void)context;
	server.keyboard_open = 0;
	if (server.edit.active && server.edit.text == &server.text)
	{
		server.typing = 1;
		server.caret_since = ae_motion_now();
		return;
	}
	if (keep)
		server_save();
}

static void keyboard_open_now(void)
{
	struct ae_keyboard_spec spec;

	memset(&spec, 0, sizeof(spec));
	spec.text = &server.text;
	spec.field = server.well;
	spec.bounds = server.flow.frame.rect;
	spec.density = server.flow.density;
	spec.done = keyboard_done;
	server.edit.done = typing_done;
	server.edit.context = NULL;
	server.edit.holder = &server;
	spec.edit = &server.edit;
	if (ae_keyboard_open(&spec, AE_OWNER_ANY))
		server.keyboard_open = 1;
}

static void server_draw(struct ae_screen *instance)
{
	struct ae_flow_screen *flow = &server.flow;
	struct ae_density const *d = &flow->density;
	struct ae_field field;
	float x = ae_flow_left_x(flow), width = ae_flow_right_x(flow) - x, y = ae_frame_y(&flow->frame, AE_FLOW_CONTENT_Y);
	char expanded[160];
	int error;

	(void)instance;
	ae_flow_full_density(flow);
	ae_flow_title(flow, "SERVER ADDRESS", "DAILY CHALLENGE");
	error = server.error[0] && !strcmp(server.error_for, server.text.text);
	if (!error)
		server.error[0] = 0;
	memset(&field, 0, sizeof(field));
	field.label = "ADDRESS";
	field.placeholder = "https://example.com";
	field.text = &server.text;
	field.flags = AE_FIELD_FOCUSED | (server.typing ? AE_FIELD_EDITING : 0) | (error ? AE_FIELD_ERROR : 0);
	field.error = error ? server.error : NULL;
	field.caret_since = server.caret_since;
	ae_field_well(d, x, y, width, &field, &server.well);
	y += ae_widget_field(d, x, y, width, &field, SERVER_FIELD) + ae_flow_u(flow, 18.0f);
	ae_draw_text(AE_FONT_BODY, ae_size_minor(d), x, y + ae_flow_u(flow, 12.0f), AE_ALIGN_LEFT, AE_COLOR_MUTED,
		ae_help_expand(ae_daily_server_address_help(), expanded, sizeof(expanded)));
	flow->prompt_count = 0;
	flow->prompts[flow->prompt_count++] = ae_flow_prompt(AE_BUTTON_A, server.text.length ? "Save" : "Type it", AE_ACTION_ACCEPT);
	flow->prompts[flow->prompt_count++] = ae_flow_prompt(AE_BUTTON_B, ae_string(AE_STR_BACK), AE_ACTION_BACK);
	ae_flow_draw_prompts(flow);
}

static void server_update(struct ae_screen *instance)
{
	(void)instance;
	if (server.typing && server.edit.active && !ae_field_edit_keys(&server.edit))
		ae_sound_request(AE_SOUND_FAILURE, 0);
}

static int server_handle(struct ae_screen *instance, struct ae_event const *event)
{
	(void)instance;
	if (server.typing)
	{
		if (event->action == AE_ACTION_START || event->action == AE_ACTION_ACCEPT)
		{
			server.typing = 0;
			ae_field_edit_end(&server.edit, 1);
			server_save();
		}
		else if (event->action == AE_ACTION_BACK)
		{
			server.typing = 0;
			ae_field_edit_end(&server.edit, 0);
		}
		return 1;
	}
	switch (event->action)
	{
	case AE_ACTION_BACK:
		return 0;
	case AE_ACTION_ACCEPT:
	case AE_ACTION_START:
		if (server.text.length)
			server_save();
		else if (event->device == AE_DEVICE_KEYBOARD_MOUSE)
			typing_begin();
		else
			keyboard_open_now();
		return 1;
	case AE_ACTION_X:
		if (event->device == AE_DEVICE_KEYBOARD_MOUSE)
			typing_begin();
		else
			keyboard_open_now();
		return 1;
	default:
		return 1;
	}
}

static void server_pointer(struct ae_screen *instance, struct ae_pointer const *pointer)
{
	struct ae_event event;
	struct ae_hit hit;

	if (ae_prompts_pointer(server.flow.prompts, server.flow.prompt_count, pointer, SERVER_PROMPTS, &event))
	{
		if (event.action == AE_ACTION_BACK && !server.typing)
		{
			ae_sound_request(AE_SOUND_BACK, 0);
			ae_ui_pop();
		}
		else
		{
			event.device = AE_DEVICE_KEYBOARD_MOUSE;
			server_handle(instance, &event);
		}
		return;
	}
	if (pointer->left_clicks && !server.typing && ae_hit_at(pointer->x, pointer->y, 0, &hit) && hit.id == SERVER_FIELD)
		typing_begin();
}

static void server_leave(struct ae_screen *instance)
{
	(void)instance;
	if (server.typing)
	{
		server.typing = 0;
		ae_field_edit_end(&server.edit, 0);
	}
}

static struct ae_screen_class const server_class =
{
	.name = "daily server", .leave = server_leave, .handle = server_handle, .draw = server_draw, .pointer = server_pointer,
	.update = server_update,
};

int ae_screen_daily_server_open(void)
{
	memset(&server, 0, sizeof(server));
	server.flow.hit_rows = SERVER_ROWS;
	server.flow.hit_prompts = SERVER_PROMPTS;
	ae_text_init(&server.text, config_string("community.custom_server"), AE_DAILY_SERVER_MAXIMUM, 0);
	if (!ae_ui_push(&server_class, AE_OWNER_ANY, &server))
		return 0;
	platform_log("ae daily server: open");
	return 1;
}
