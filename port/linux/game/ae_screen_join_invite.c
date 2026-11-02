/*
AE_SCREEN_JOIN_INVITE.C

AE's JOIN WITH INVITE (ae_screen_join_invite.h), a page of AE's row screens (ae_setup_flow.h) laid out as PROFILES'
NEW PROFILE: the title, one field (INVITE LINK OR CODE) across the content's width (a link is 76 characters), a line
under it. The field opens holding the clipboard's invite when it has one ("From your clipboard."), else empty.

- A picks player 1's profile, then joins what the field holds (empty: typing starts, as X); X types into it (Enter or a click on a keyboard, AE's
  keyboard with a pad's A); Y pastes the clipboard's text over it; B back. Typing: Enter (A) joins, Esc (B) restores.
- Not an invite (ae_invite_rules.c: a halo://join/ link or its 64-digit code), internet play off, or a join that
  failed: the field's error line says why, until the text changes.
- Joining (ae_lobby_join_address, as the server browser's Y did): "JOINING..." and only B, which leaves it; joined:
  player 1 in, then the joiner's LOBBY; leaving that comes back here.

HALO_AE_TEST_INVITE (tests only): the clipboard's text as this screen reads it (a headless game has no clipboard).

Its log: "ae join invite: open", "pasted", "not an invite", "joining", "join failed: <why>", "joined", "join left".
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cseries.h"
#include "../src/ae_draw.h"
#include "ae_glue_lobby.h"
#include "ae_invite_rules.h"
#include "ae_motion.h"
#include "ae_screen_create_game.h"
#include "ae_screen_join_invite.h"
#include "ae_screen_lobby_profile.h"
#include "ae_setup_flow.h"
#include "ae_sound.h"
#include "ae_strings.h"
#include "ae_ui.h"
#include "ae_widgets.h"

void platform_log(char const *format, ...);

enum
{
	JOIN_HIT = 0x7600,
	HIT_ROWS = JOIN_HIT + 1, HIT_PROMPTS, HIT_FIELD
};
/* (a link: "halo://join/" and the code's 64 digits; room for a pasted line's spaces around it) */
enum { TEXT_LIMIT = 96 };

#define NOT_AN_INVITE "That is not an invite: a halo://join/ link or its 64-character code."

static struct
{
	struct ae_flow_screen flow;
	const char *crumb;
	struct ae_text text;
	struct ae_field_edit edit;
	int typing, keyboard_open;
	unsigned long caret_since;
	struct ae_rect well;
	int from_clipboard;              /* the field holds the clipboard's invite (clip) as it was put there */
	char clip[AE_TEXT_MAXIMUM];
	char error[160];                 /* the field's error, shown while the text is error_for */
	char error_for[AE_TEXT_MAXIMUM];
	int joining;
} join;

static struct ae_screen_class const join_class;

static void error_set(const char *why)
{
	snprintf(join.error, sizeof(join.error), "%s", why ? why : "");
	snprintf(join.error_for, sizeof(join.error_for), "%s", join.text.text);
	ae_sound_request(AE_SOUND_FAILURE, 0);
}

/* the clipboard's text (HALO_AE_TEST_INVITE's in tests); 0 when empty */
static int clipboard_text(char *text, int size)
{
	char const *test = getenv("HALO_AE_TEST_INVITE");

	if (test && *test)
	{
		snprintf(text, (size_t)size, "%s", test);
		return 1;
	}
	return ae_host_clipboard_get(text, size) && text[0];
}

/* the clipboard's invite as a link (a message around it dropped); 0 when it holds none */
static int clipboard_invite(char *link, int size)
{
	char text[512], code[AE_INVITE_CODE_DIGITS + 1];

	text[0] = 0;
	if (!clipboard_text(text, sizeof(text)) || !ae_invite_code(text, code, sizeof(code)))
		return 0;
	snprintf(link, size, "halo://join/%s", code);
	return 1;
}

/* ---------- joining */

static void join_failed(const char *why)
{
	join.joining = 0;
	platform_log("ae join invite: join failed: %s", why);
	error_set(why);
}

static void join_profile_done(short player, int picked, void *context)
{
	struct ae_result result;

	(void)player;
	(void)context;
	if (!picked)
		return;
	result = ae_lobby_join_address(join.text.text);
	if (!result.ok)
	{
		join_failed(result.reason);
		return;
	}
	join.joining = 1;
	join.error[0] = 0;
	platform_log("ae join invite: joining");
	ae_sound_request(AE_SOUND_FORWARD, 0);
}

static void join_try(void)
{
	char code[AE_INVITE_CODE_DIGITS + 1];

	if (!join.text.length)
	{
		ae_sound_request(AE_SOUND_FAILURE, 0);
		return;
	}
	if (!ae_invite_code(join.text.text, code, sizeof(code)))
	{
		platform_log("ae join invite: not an invite");
		error_set(NOT_AN_INVITE);
		return;
	}
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
	platform_log("ae join invite: joined");
	ae_screen_create_game_lobby_joined("JOINED BY INVITE");
}

static void join_cancel(void)
{
	ae_lobby_leave();
	join.joining = 0;
	platform_log("ae join invite: join left");
	ae_sound_request(AE_SOUND_BACK, 0);
}

/* ---------- the field */

static void paste(void)
{
	char text[512];

	text[0] = 0;
	if (!clipboard_text(text, sizeof(text)))
	{
		error_set("The clipboard is empty: copy the invite your friend sent, then paste it.");
		return;
	}
	/* (an invite inside a message: the link alone; anything else as it is, and said not to be one) */
	if (!clipboard_invite(text, sizeof(text)))
	{
		ae_text_init(&join.text, text, TEXT_LIMIT, 0);
		platform_log("ae join invite: pasted, not an invite");
		error_set(NOT_AN_INVITE);
		join.from_clipboard = 0;
		return;
	}
	ae_text_init(&join.text, text, TEXT_LIMIT, 0);
	snprintf(join.clip, sizeof(join.clip), "%s", join.text.text);
	join.from_clipboard = 1;
	join.error[0] = 0;
	platform_log("ae join invite: pasted");
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

	memset(&spec, 0, sizeof(spec));
	spec.text = &join.text;
	spec.field = join.well;
	spec.bounds = join.flow.frame.rect;
	spec.density = join.flow.density;
	spec.done = keyboard_done;
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

/* ---------- the screen */

static void join_draw(struct ae_screen *instance)
{
	struct ae_flow_screen *flow = &join.flow;
	struct ae_density const *d = &flow->density;
	struct ae_field field;
	float x = ae_flow_left_x(flow), width = ae_flow_right_x(flow) - x, y = ae_frame_y(&flow->frame, AE_FLOW_CONTENT_Y);
	char expanded[160];
	int error;

	(void)instance;
	ae_flow_full_density(flow);
	ae_flow_title(flow, "JOIN WITH INVITE", join.joining ? "JOINING..." : join.crumb);
	/* (an error lasts until the text changes) */
	error = join.error[0] && !strcmp(join.error_for, join.text.text);
	if (!error)
		join.error[0] = 0;
	if (join.from_clipboard && strcmp(join.text.text, join.clip))
		join.from_clipboard = 0;
	memset(&field, 0, sizeof(field));
	field.label = "INVITE LINK OR CODE";
	field.placeholder = "halo://join/... or the invite's 64-character code";
	field.text = &join.text;
	field.flags = AE_FIELD_FOCUSED | (join.typing ? AE_FIELD_EDITING : 0) | (error ? AE_FIELD_ERROR : 0) |
		(join.joining ? AE_FIELD_DISABLED : 0);
	field.error = error ? join.error : NULL;
	field.caret_since = join.caret_since;
	ae_field_well(d, x, y, width, &field, &join.well);
	y += ae_widget_field(d, x, y, width, &field, HIT_FIELD) + ae_flow_u(flow, 18.0f);
	if (join.joining)
		ae_draw_text(AE_FONT_BODY, ae_size_minor(d), x, y + ae_flow_u(flow, 12.0f), AE_ALIGN_LEFT, AE_COLOR_ACCENT,
			ae_help_expand("Reaching the host's game. {B} leaves it.", expanded, sizeof(expanded)));
	else
		ae_draw_text(AE_FONT_BODY, ae_size_minor(d), x, y + ae_flow_u(flow, 12.0f), AE_ALIGN_LEFT, AE_COLOR_MUTED,
			ae_help_expand(join.from_clipboard ? "From your clipboard. {A} joins its game." :
			"Paste the link a friend sent you ({Y}, or Ctrl+V while typing), or type its code.", expanded, sizeof(expanded)));
	flow->prompt_count = 0;
	if (!join.joining)
	{
		if (join.typing || join.text.length)
			flow->prompts[flow->prompt_count++] = ae_flow_prompt(AE_BUTTON_A, "Join", AE_ACTION_ACCEPT);
		else
			flow->prompts[flow->prompt_count++] = ae_flow_prompt(AE_BUTTON_A, "Type it", AE_ACTION_ACCEPT);
		if (!join.typing)
		{
			if (join.text.length)
				flow->prompts[flow->prompt_count++] = ae_flow_prompt(AE_BUTTON_X, "Type", AE_ACTION_X);
			flow->prompts[flow->prompt_count++] = ae_flow_prompt(AE_BUTTON_Y, "Paste", AE_ACTION_Y);
		}
	}
	flow->prompts[flow->prompt_count++] = ae_flow_prompt(AE_BUTTON_B, ae_string(AE_STR_BACK), AE_ACTION_BACK);
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
		if (join.text.length)
			join_try();
		else
			edit_begin(event->device);
		return 1;
	case AE_ACTION_START:
		join_try();
		return 1;
	case AE_ACTION_X:
		edit_begin(event->device);
		return 1;
	case AE_ACTION_Y:
		paste();
		return 1;
	default:
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
	/* (a click on the field starts typing into it) */
	if (pointer->left_clicks && !join.typing && !join.joining && ae_hit_at(pointer->x, pointer->y, 0, &hit) &&
		hit.id == HIT_FIELD)
		typing_begin();
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
	platform_log("ae join invite: closed");
}

static struct ae_screen_class const join_class =
{
	.name = "join invite", .leave = join_leave, .handle = join_handle, .draw = join_draw, .pointer = join_pointer,
	.update = join_screen_update,
};

int ae_screen_join_invite_open(const char *crumb)
{
	char link[AE_TEXT_MAXIMUM];

	memset(&join, 0, sizeof(join));
	join.crumb = crumb ? crumb : "MULTIPLAYER";
	join.flow.hit_rows = HIT_ROWS;
	join.flow.hit_prompts = HIT_PROMPTS;
	join.from_clipboard = clipboard_invite(link, sizeof(link));
	ae_text_init(&join.text, join.from_clipboard ? link : "", TEXT_LIMIT, 0);
	snprintf(join.clip, sizeof(join.clip), "%s", join.text.text);
	if (!ae_ui_push(&join_class, AE_OWNER_ANY, &join))
		return 0;
	platform_log("ae join invite: open (%s)", join.from_clipboard ? "the clipboard's invite" : "empty");
	return 1;
}
