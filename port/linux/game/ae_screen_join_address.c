/*
AE_SCREEN_JOIN_ADDRESS.C

AE's JOIN BY ADDRESS (ae_screen_join_address.h): a page of AE's row screens (ae_setup_flow.h) laid out as JOIN WITH
INVITE: the title, one field (ADDRESS, 64 characters) across the content's width with a line under it, then RECENT:
the newest 10 hosts joined by address (the host's name, its address, when), kept in the save root's private folder
(recent-addresses.txt) and shown with the focused one's line under them.

- Focus: the field, or RECENT's rows (down from the field, up from the first row). It opens on the first recent host
  when there is one, else on the field.
- The field: A joins what it holds (empty: typing starts, as X); X types into it (Enter or a click on a keyboard, AE's
  keyboard with a pad's A, its ' key a colon); Y pastes the clipboard's text over it; B back. Typing: Enter (A) joins,
  Esc (B) restores.
- RECENT: A puts the host's address in the field and joins it; Y forgets it; X types an address.
- Not an address (ae_address_rules.c), or a join that failed: the field's error line says why, in menu blue, until
  the text changes. Nothing answering at it in 10 s: "Can't reach <address>. ...", then A tries again.
- Joining (ae_lobby_join_host): "JOINING..." and only B, which leaves it; joined: the host is first in RECENT, player 1
  in, then the joiner's LOBBY; leaving that comes back here.

Its log: "ae join address: open", "pasted", "not an address: <why>", "joining <address>", "join failed: <why>", "joined
<address>", "forgot <address>", "join left", "closed".
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "cseries.h"
#include "../src/ae_draw.h"
#include "../src/ae_private_files.h"
#include "ae_address_rules.h"
#include "ae_glue_lobby.h"
#include "ae_motion.h"
#include "ae_screen_create_game.h"
#include "ae_screen_join_address.h"
#include "ae_screen_lobby_profile.h"
#include "ae_setup_flow.h"
#include "ae_sound.h"
#include "ae_strings.h"
#include "ae_ui.h"
#include "ae_widgets.h"

void platform_log(char const *format, ...);

enum
{
	ADDRESS_HIT = 0x7620,
	HIT_ROWS = ADDRESS_HIT + 1, HIT_PROMPTS, HIT_FIELD, HIT_LIST
};
#define RECENT_FILE "recent-addresses.txt"

static struct
{
	struct ae_flow_screen flow;
	const char *crumb;
	struct ae_text text;
	struct ae_field_edit edit;
	int typing, keyboard_open;
	unsigned long caret_since;
	struct ae_rect well;
	int in_list;                     /* the focus is on RECENT's rows, not the field */
	struct ae_address_recent recent;
	struct ae_list_view list;
	char error[200];                 /* the field's error, shown while the text is error_for */
	char error_for[AE_TEXT_MAXIMUM];
	int joining;
	int error_join;                  /* the error is a join that failed (A tries again), not a text that is no address */
	struct ae_address joined_to;     /* the address being joined (its text kept in RECENT once joined) */
} join;

static struct ae_screen_class const join_class;

static void error_set(const char *why)
{
	snprintf(join.error, sizeof(join.error), "%s", why ? why : "");
	snprintf(join.error_for, sizeof(join.error_for), "%s", join.text.text);
	ae_sound_request(AE_SOUND_FAILURE, 0);
}

/* ---------- RECENT's file */

static void recent_load(void)
{
	static char text[4096];
	long length = ae_private_file_read(RECENT_FILE, text, sizeof(text));

	ae_address_recent_init(&join.recent);
	if (length < 0)
		return;
	if (length == -2 || !ae_address_recent_read(&join.recent, text))
		platform_log("ae join address: %s had lines that are no address: those dropped", RECENT_FILE);
}

static void recent_save(void)
{
	static char text[4096];
	long length = ae_address_recent_write(&join.recent, text, sizeof(text));

	if (length < 0 || !ae_private_file_write(RECENT_FILE, text, length))
		platform_log("ae join address: %s could not be written", RECENT_FILE);
}

static void recent_count_changed(void)
{
	short focus = join.list.list.focus;

	ae_list_view_init(&join.list, join.recent.count, 10);
	if (join.recent.count)
		ae_list_set_focus(&join.list.list, focus < 0 ? 0 : focus);
	if (!join.recent.count)
		join.in_list = 0;
}

/* ---------- joining */

static void join_failed(const char *why)
{
	char text[200];

	join.joining = 0;
	platform_log("ae join address: join failed: %s", why);
	/* (nothing answered, or the name is not known: the field says so with the address, and what to check) */
	if (!strcmp(why, ae_string(AE_STR_ERR_NO_ANSWER)))
	{
		snprintf(text, sizeof(text), "Can't reach %s. Check the address, that the host's game is open, and that its "
			"firewall lets Arena Evolved through.", join.joined_to.text);
		why = text;
	}
	join.in_list = 0;
	error_set(why);
	join.error_join = 1;
}

static void join_profile_done(short player, int picked, void *context)
{
	struct ae_result result;

	(void)player;
	(void)context;
	if (!picked)
		return;
	result = ae_lobby_join_host(join.joined_to.host, join.joined_to.port);
	if (!result.ok)
	{
		join_failed(result.reason);
		return;
	}
	join.joining = 1;
	join.error[0] = 0;
	platform_log("ae join address: joining %s", join.joined_to.text);
	ae_sound_request(AE_SOUND_FORWARD, 0);
}

static void join_try(void)
{
	struct ae_address address;
	int why;

	if (!join.text.length)
	{
		ae_sound_request(AE_SOUND_FAILURE, 0);
		return;
	}
	why = ae_address_parse(join.text.text, ae_lobby_host_port(), &address);
	if (why != AE_ADDRESS_OK)
	{
		platform_log("ae join address: not an address: %s", ae_address_reason(why));
		error_set(ae_address_reason(why));
		join.error_join = 0;
		return;
	}
	join.joined_to = address;
	if (!ae_screen_lobby_profile_open(0, &join.flow, join_profile_done, NULL))
		ae_sound_request(AE_SOUND_FAILURE, 0);
}

/* the join under way: joined, player 1 in, the joiner's LOBBY; else why not (as the server browser's) */
static void join_update(void)
{
	static struct ae_lobby_roster roster;
	struct ae_result failure;
	int state = ae_lobby_join_state(&failure);

	if (state == 0)
		return;
	if (state < 0)
	{
		join_failed(failure.reason[0] ? failure.reason : "The join was left.");
		return;
	}
	/* (this PC's client in the pregame first: its players are taken from then) */
	if (!ae_lobby_roster(&roster).ok || !roster.joined)
		return;
	join.joining = 0;
	if (!ae_lobby_add_local_player(0).ok)
	{
		ae_lobby_leave();
		join_failed("The game is full, or no longer takes players.");
		return;
	}
	{
		/* (the host's name from its player's, else none: the address shows) */
		char const *name = "";
		short index;

		for (index = 0; index < roster.count; index++)
			if (roster.players[index].host && !roster.players[index].bot)
				name = roster.players[index].name;
		ae_address_recent_add(&join.recent, name, join.joined_to.text, (long)time(NULL));
		recent_save();
		recent_count_changed();
		ae_list_set_focus(&join.list.list, 0);
	}
	platform_log("ae join address: joined %s", join.joined_to.text);
	ae_screen_create_game_lobby_joined("JOINED BY ADDRESS");
}

static void join_cancel(void)
{
	ae_lobby_leave();
	join.joining = 0;
	platform_log("ae join address: join left");
	ae_sound_request(AE_SOUND_BACK, 0);
}

/* ---------- the field */

static void paste(void)
{
	char text[512];

	text[0] = 0;
	if (!ae_host_clipboard_get(text, sizeof(text)) || !text[0])
	{
		error_set("The clipboard is empty: copy the host's address, then paste it.");
		join.error_join = 0;
		return;
	}
	ae_text_init(&join.text, text, AE_ADDRESS_LIMIT, 0);
	join.error[0] = 0;
	join.in_list = 0;
	platform_log("ae join address: pasted");
	ae_sound_request(AE_SOUND_CURSOR, 0);
}

static void typing_done(int keep, void *context)
{
	(void)keep;
	(void)context;
	join.typing = 0;
}

static void typing_begin(void)
{
	join.in_list = 0;
	join.edit.done = typing_done;
	join.edit.context = NULL;
	join.edit.holder = &join;
	ae_field_edit_begin(&join.edit, &join.text);
	ae_text_select_all(&join.text);
	join.typing = 1;
	join.caret_since = ae_motion_now();
	ae_sound_request(AE_SOUND_FORWARD, 0);
}

static void typing_end(int keep)
{
	if (!join.typing)
		return;
	join.typing = 0;
	ae_field_edit_end(&join.edit, keep);
	if (keep && join.text.length)
		join_try();
	else
		ae_sound_request(AE_SOUND_BACK, 0);
}

static void keyboard_done(int keep, void *context)
{
	(void)context;
	join.keyboard_open = 0;
	/* (a physical key closed it into the field's own typing: that goes on) */
	if (join.edit.active && join.edit.text == &join.text)
	{
		join.typing = 1;
		join.caret_since = ae_motion_now();
		return;
	}
	if (keep && join.text.length)
		join_try();
}

static void keyboard_open(void)
{
	struct ae_keyboard_spec spec;

	join.in_list = 0;
	memset(&spec, 0, sizeof(spec));
	spec.text = &join.text;
	spec.field = join.well;
	spec.bounds = join.flow.frame.rect;
	spec.density = join.flow.density;
	spec.done = keyboard_done;
	spec.address = 1;
	join.edit.done = typing_done;
	join.edit.context = NULL;
	join.edit.holder = &join;
	spec.edit = &join.edit;
	if (ae_keyboard_open(&spec, AE_OWNER_ANY))
		join.keyboard_open = 1;
}

/* X (A on an empty field): typing on a keyboard, AE's keyboard with a pad */
static void edit_begin(int device)
{
	if (device == AE_DEVICE_KEYBOARD_MOUSE)
		typing_begin();
	else
		keyboard_open();
}

/* ---------- RECENT */

static struct ae_address_recent_entry const *focused_entry(void)
{
	short focus = join.list.list.focus;

	return join.in_list && focus >= 0 && focus < join.recent.count ? &join.recent.entries[focus] : NULL;
}

/* the entry's address in the field, joined */
static void recent_join(void)
{
	struct ae_address_recent_entry const *entry = focused_entry();

	if (!entry)
		return;
	ae_text_init(&join.text, entry->text, AE_ADDRESS_LIMIT, 0);
	join.error[0] = 0;
	join_try();
}

static void recent_forget(void)
{
	struct ae_address_recent_entry const *entry = focused_entry();

	if (!entry)
	{
		ae_sound_request(AE_SOUND_FAILURE, 0);
		return;
	}
	platform_log("ae join address: forgot %s", entry->text);
	ae_address_recent_forget(&join.recent, join.list.list.focus);
	recent_save();
	recent_count_changed();
	ae_sound_request(AE_SOUND_BACK, 0);
}

/* a time as the player's clock reads it (the menus' thread only: the C library's one buffer, copied at once) */
static void local_time(time_t when, struct tm *out)
{
	struct tm const *local = localtime(&when);

	if (local)
		*out = *local;
	else
		memset(out, 0, sizeof(*out));
}

/* the time as a sentence's words: "today at 20:41", "yesterday", "3 days ago", "October 12" */
static void when_words(long when, char *words, size_t size)
{
	struct tm then, now;
	time_t then_time = (time_t)when, now_time = time(NULL);
	char label[40];
	size_t index;

	local_time(then_time, &then);
	local_time(now_time, &now);
	ae_address_when_label(&then, &now, label, sizeof(label));
	for (index = 0; label[index]; index++)
		if (index && label[index] >= 'A' && label[index] <= 'Z')
			label[index] = (char)(label[index] - 'A' + 'a');
	if (label[0] == 'T' || label[0] == 'Y')
		label[0] = (char)(label[0] - 'A' + 'a');
	/* ("today 20:41" reads "today at 20:41") */
	if (!strncmp(label, "today ", 6))
		snprintf(words, size, "today at %s", label + 6);
	else
		snprintf(words, size, "%s", label);
}

struct item_context { struct ae_density const *density; float width; };

static void list_item(void *context, short item, float x, float y, float width, float height, unsigned int flags)
{
	struct item_context const *list = context;
	struct ae_address_recent_entry const *entry = &join.recent.entries[item];
	struct ae_row row;
	struct tm then;
	time_t when = (time_t)entry->when, now_time = time(NULL);
	struct tm now;
	char name[64], upper[64], when_label[40];
	float pad = ae_flow_u(&join.flow, list->density->metrics->pad), text = ae_size_text(list->density) * 0.9f;
	float center = y + height * 0.5f, right = x + width - pad, when_width = width * 0.10f;
	int on_bar = (flags & AE_ROW_ON_BAR) != 0, index;
	unsigned int muted = on_bar ? AE_COLOR_SELECTION_MUTED : AE_COLOR_MUTED;

	local_time(when, &then);
	local_time(now_time, &now);
	ae_address_when_label(&then, &now, when_label, sizeof(when_label));
	/* (a host with no name: its address shows as the name, in the address column once) */
	snprintf(upper, sizeof(upper), "%s", entry->name[0] ? entry->name : entry->text);
	for (index = 0; upper[index]; index++)
		if (upper[index] >= 'a' && upper[index] <= 'z')
			upper[index] = (char)(upper[index] - 'a' + 'A');
	memset(&row, 0, sizeof(row));
	ae_fit_text(AE_FONT_ROW, ae_size_text(list->density), upper, width * 0.5f, name, sizeof(name));
	row.label = name;
	row.flags = flags;
	ae_widget_row(list->density, x, y, width, &row, HIT_LIST, item);
	ae_draw_text_tracked(AE_FONT_BODY, text * 0.85f, 0.08f, right, ae_flow_cap_top(AE_FONT_BODY, text * 0.85f, center),
		AE_ALIGN_RIGHT, muted, when_label);
	ae_draw_text(AE_FONT_BODY, text, right - when_width - ae_flow_u(&join.flow, 12.0f),
		ae_flow_cap_top(AE_FONT_BODY, text, center), AE_ALIGN_RIGHT, on_bar ? AE_COLOR_SELECTION_TEXT : AE_COLOR_ACCENT,
		entry->text);
}

/* ---------- the screen */

static void join_draw(struct ae_screen *instance)
{
	struct ae_flow_screen *flow = &join.flow;
	struct ae_density const *d = &flow->density;
	struct ae_field field;
	struct item_context context;
	float x, width, y, minor, bottom, list_top, list_bottom;
	char expanded[200];
	int error;

	(void)instance;
	ae_flow_full_density(flow);
	x = ae_flow_left_x(flow);
	width = ae_flow_right_x(flow) - x;
	y = ae_frame_y(&flow->frame, AE_FLOW_CONTENT_Y);
	minor = ae_size_minor(d);
	bottom = ae_flow_content_bottom(flow);
	ae_flow_title(flow, "JOIN BY ADDRESS", join.joining ? "JOINING..." : join.crumb);
	/* (an error lasts until the text changes) */
	error = join.error[0] && !strcmp(join.error_for, join.text.text);
	if (!error)
		join.error[0] = 0;
	memset(&field, 0, sizeof(field));
	field.label = "ADDRESS";
	field.placeholder = "An IP address or a computer's name, like 192.168.1.20 or arena-host";
	field.text = &join.text;
	field.flags = ((join.in_list && !join.typing) ? 0 : AE_FIELD_FOCUSED) | (join.typing ? AE_FIELD_EDITING : 0) |
		(error ? AE_FIELD_ERROR : 0) | (join.joining ? AE_FIELD_DISABLED : 0);
	/* (the error stands where the help does, in menu blue: the field wears its error ring, its own line one blank
	space that is taken back below) */
	field.error = error ? " " : NULL;
	field.caret_since = join.caret_since;
	ae_field_well(d, x, y, width, &field, &join.well);
	y += ae_widget_field(d, x, y, width, &field, HIT_FIELD) + ae_flow_u(flow, 18.0f);
	if (error)
		y -= minor * 1.4f + 4.0f * d->unit;
	/* (the line under the field: what to do, or that the join goes on) */
	if (join.joining)
	{
		char words[160];

		if (ae_lobby_join_looking_up())
			snprintf(words, sizeof(words), "Looking up %s... {B} leaves it.", join.joined_to.host);
		else
			snprintf(words, sizeof(words), "Reaching the host's game. {B} leaves it.");
		ae_draw_text(AE_FONT_BODY, minor, x, y + ae_flow_u(flow, 12.0f), AE_ALIGN_LEFT, AE_COLOR_ACCENT,
			ae_help_expand(words, expanded, sizeof(expanded)));
	}
	else if (error)
	{
		char lines[2][160];
		int count = ae_wrap_text(AE_FONT_BODY, minor, join.error, width, lines, 2), index;

		for (index = 0; index < count; index++)
			ae_draw_text(AE_FONT_BODY, minor, x, y + ae_flow_u(flow, 12.0f) + (float)index * minor * 1.4f, AE_ALIGN_LEFT,
				AE_COLOR_WARNING, lines[index]);
		if (count > 1)
			y += minor * 1.4f;
	}
	else
		ae_draw_text(AE_FONT_BODY, minor, x, y + ae_flow_u(flow, 12.0f), AE_ALIGN_LEFT, AE_COLOR_MUTED,
			"Your network's or a private network's address. Add :port when the host uses another port.");
	y += ae_flow_u(flow, 72.0f);
	/* RECENT */
	list_top = y;
	if (join.recent.count)
	{
		ae_draw_text_tracked(AE_FONT_BODY, minor * 0.85f, 0.12f, x, y, AE_ALIGN_LEFT, AE_COLOR_MUTED, "RECENT");
		list_top = y + ae_flow_u(flow, 22.0f);
		list_bottom = bottom - ae_flow_u(flow, 90.0f);
		context.density = d;
		context.width = width;
		ae_widget_list(d, &join.list, x, list_top, width, list_bottom - list_top, list_item, &context, HIT_LIST);
		{
			struct ae_address_recent_entry const *entry = &join.recent.entries[join.list.list.focus < 0 ? 0 :
				join.list.list.focus];
			char line[200], when[48];

			if (join.in_list && !join.joining)
			{
				when_words(entry->when, when, sizeof(when));
				{
					char name[AE_ADDRESS_NAME_LIMIT * 2 + 1];
					int index;

					snprintf(name, sizeof(name), "%s", entry->name[0] ? entry->name : "This host");
					for (index = 0; name[index]; index++)
						if (name[index] >= 'a' && name[index] <= 'z')
							name[index] = (char)(name[index] - 'a' + 'A');
					snprintf(line, sizeof(line), "%s at %s, joined %s. {A} joins it again.", name, entry->text, when);
				}
				ae_draw_rect(x, bottom - ae_flow_u(flow, 78.0f), width, 1.0f, 0.0f, AE_COLOR_RULE);
				ae_draw_text(AE_FONT_BODY, ae_size_text(d) * 0.9f, x, bottom - ae_flow_u(flow, 60.0f), AE_ALIGN_LEFT,
					AE_COLOR_TEXT, ae_help_expand(line, expanded, sizeof(expanded)));
			}
		}
	}
	else
		ae_draw_text(AE_FONT_BODY, ae_size_text(d) * 0.9f, x, y + ae_flow_u(flow, 24.0f), AE_ALIGN_LEFT, AE_COLOR_MUTED,
			"Addresses you join show here, with the host's name.");
	flow->prompt_count = 0;
	if (join.joining)
		flow->prompts[flow->prompt_count++] = ae_flow_prompt(AE_BUTTON_B, "Leave the join", AE_ACTION_BACK);
	else
	{
		if (join.in_list)
		{
			flow->prompts[flow->prompt_count++] = ae_flow_prompt(AE_BUTTON_A, "Join", AE_ACTION_ACCEPT);
			flow->prompts[flow->prompt_count++] = ae_flow_prompt(AE_BUTTON_X, "Type an address", AE_ACTION_X);
			flow->prompts[flow->prompt_count++] = ae_flow_prompt(AE_BUTTON_Y, "Forget", AE_ACTION_Y);
		}
		else if (join.typing)
			flow->prompts[flow->prompt_count++] = ae_flow_prompt(AE_BUTTON_A, "Join", AE_ACTION_ACCEPT);
		else
		{
			/* (a join that failed: A tries it again, X edits; a text that is no address: A edits it) */
			if (error && join.error_join)
				flow->prompts[flow->prompt_count++] = ae_flow_prompt(AE_BUTTON_A, "Try again", AE_ACTION_ACCEPT);
			else if (error)
				flow->prompts[flow->prompt_count++] = ae_flow_prompt(AE_BUTTON_A, "Edit", AE_ACTION_ACCEPT);
			else
				flow->prompts[flow->prompt_count++] = ae_flow_prompt(AE_BUTTON_A, join.text.length ? "Join" : "Type it",
					AE_ACTION_ACCEPT);
			if (join.text.length && !(error && !join.error_join))
				flow->prompts[flow->prompt_count++] = ae_flow_prompt(AE_BUTTON_X, error ? "Edit" : "Type", AE_ACTION_X);
			if (!error || !join.error_join)
				flow->prompts[flow->prompt_count++] = ae_flow_prompt(AE_BUTTON_Y, "Paste", AE_ACTION_Y);
		}
		flow->prompts[flow->prompt_count++] = ae_flow_prompt(AE_BUTTON_B, ae_string(AE_STR_BACK), AE_ACTION_BACK);
	}
	ae_flow_draw_prompts(flow);
}

static void join_screen_update(struct ae_screen *instance)
{
	(void)instance;
	if (join.typing && join.edit.active && !ae_field_edit_keys(&join.edit))
		ae_sound_request(AE_SOUND_FAILURE, 0);
	if (join.joining)
		join_update();
}

static int join_handle(struct ae_screen *instance, struct ae_event const *event)
{
	(void)instance;
	/* (joining: only B, which leaves the join) */
	if (join.joining)
	{
		if (event->action == AE_ACTION_BACK)
			join_cancel();
		return 1;
	}
	/* (typing: Enter or START keeps and joins, Esc or B restores; the rest is the typing's) */
	if (join.typing)
	{
		if (event->action == AE_ACTION_START || event->action == AE_ACTION_ACCEPT)
			typing_end(1);
		else if (event->action == AE_ACTION_BACK)
			typing_end(0);
		return 1;
	}
	switch (event->action)
	{
	case AE_ACTION_BACK:
		return 0;
	case AE_ACTION_ACCEPT:
		if (join.in_list)
			recent_join();
		else if (join.text.length && !(join.error[0] && !join.error_join && !strcmp(join.error_for, join.text.text)))
			join_try();
		else
			edit_begin(event->device);
		return 1;
	case AE_ACTION_START:
		if (!join.in_list)
			join_try();
		return 1;
	case AE_ACTION_X:
		edit_begin(event->device);
		return 1;
	case AE_ACTION_Y:
		if (join.in_list)
			recent_forget();
		else
			paste();
		return 1;
	case AE_ACTION_UP:
	case AE_ACTION_DOWN:
		if (!join.recent.count)
			return 1;
		if (!join.in_list)
		{
			if (event->action == AE_ACTION_DOWN)
			{
				join.in_list = 1;
				ae_list_set_focus(&join.list.list, 0);
				ae_sound_request(AE_SOUND_CURSOR, event->repeat);
			}
		}
		else if (event->action == AE_ACTION_UP && join.list.list.focus <= 0)
		{
			join.in_list = 0;
			ae_sound_request(AE_SOUND_CURSOR, event->repeat);
		}
		else
			ae_list_view_event(&join.list, event);
		return 1;
	default:
		if (join.in_list)
			ae_list_view_event(&join.list, event);
		return 1;
	}
}

static void join_pointer(struct ae_screen *instance, struct ae_pointer const *pointer)
{
	struct ae_event event;
	struct ae_hit hit;

	if (ae_prompts_pointer(join.flow.prompts, join.flow.prompt_count, pointer, HIT_PROMPTS, &event))
	{
		if (event.action == AE_ACTION_BACK && !join.typing && !join.joining)
		{
			ae_sound_request(AE_SOUND_BACK, 0);
			ae_ui_pop();
		}
		else
		{
			event.device = AE_DEVICE_KEYBOARD_MOUSE;
			join_handle(instance, &event);
		}
		return;
	}
	if (join.joining || join.typing)
		return;
	/* (a click on the field starts typing into it; on a recent host, focuses it, and joins it when focused) */
	if (pointer->left_clicks && ae_hit_at(pointer->x, pointer->y, 0, &hit) && hit.id == HIT_FIELD)
	{
		typing_begin();
		return;
	}
	if (join.recent.count)
	{
		short focus = join.list.list.focus;
		short item = ae_list_view_pointer(&join.list, pointer, HIT_LIST);

		if (item >= 0 && item < join.recent.count)
		{
			if (join.in_list && item == focus)
				recent_join();
			join.in_list = 1;
		}
	}
}

static void join_leave(struct ae_screen *instance)
{
	(void)instance;
	if (join.typing)
	{
		join.typing = 0;
		ae_field_edit_end(&join.edit, 0);
	}
	/* (a join still under way when the screen goes: left) */
	if (join.joining)
		join_cancel();
	platform_log("ae join address: closed");
}

static struct ae_screen_class const join_class =
{
	.name = "join address", .leave = join_leave, .handle = join_handle, .draw = join_draw, .pointer = join_pointer,
	.update = join_screen_update,
};

int ae_screen_join_address_open(const char *crumb)
{
	memset(&join, 0, sizeof(join));
	join.crumb = crumb ? crumb : "JOIN LAN GAMES";
	join.flow.hit_rows = HIT_ROWS;
	join.flow.hit_prompts = HIT_PROMPTS;
	ae_text_init(&join.text, "", AE_ADDRESS_LIMIT, 0);
	recent_load();
	ae_list_view_init(&join.list, join.recent.count, 10);
	join.in_list = join.recent.count > 0;
	if (!ae_ui_push(&join_class, AE_OWNER_ANY, &join))
		return 0;
	platform_log("ae join address: open (%d recent)", join.recent.count);
	return 1;
}
