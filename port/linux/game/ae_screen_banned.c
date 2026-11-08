/*
AE_SCREEN_BANNED.C

AE's BANNED PLAYERS (ae_screen_banned.h): a column of the bans (the newest first), each a row with the name it was made
under and how long ago; A on one asks "Unban <name>?" with CANCEL focused, and UNBAN takes its line out of bans.txt.
Nothing banned: one quiet line, "No banned players."

Its log: "ae banned: open (<n> bans)", "unban asked <name>", "unbanned <name>", "closed".
*/

#include <stdio.h>
#include <string.h>

#include "cseries.h"
#include "../src/ae_draw.h"
#include "ae_moderation.h"
#include "ae_screen_banned.h"
#include "ae_setup_flow.h"
#include "ae_sound.h"
#include "ae_strings.h"
#include "ae_ui.h"
#include "ae_widgets.h"

void platform_log(char const *format, ...);

enum
{
	BANNED_HIT = 0x7A40,
	HIT_ROWS = BANNED_HIT + 1, HIT_PROMPTS
};
enum { ROW_BAN = AE_FLOW_ROW_OWN, ROW_QUIET, ROW_MORE, LISTED = AE_FLOW_ROWS - 1 };

static struct
{
	struct ae_flow_screen flow;
	struct ae_moderation_ban bans[AE_MODERATION_BANS_MAXIMUM];
	long count;
	/* the bans shown, newest first: their place in bans */
	short shown[AE_FLOW_ROWS];
	short shown_count;
	char when[AE_FLOW_ROWS][32];
	char help[AE_FLOW_ROWS][96];
	char more[48];
	short asking;     /* the ban the open dialog is about, -1 none */
} screen;

static void bans_read(void)
{
	screen.count = ae_moderation_bans(screen.bans, AE_MODERATION_BANS_MAXIMUM);
}

/* "3 days ago", from a ban line's date; the date itself when the clock does not say */
static void when_text(struct ae_moderation_ban const *ban, char *text, int size)
{
	long age = ae_moderation_ban_age_seconds(ban);

	if (ban->until)
		snprintf(text, (size_t)size, "FOR A WHILE");
	else if (age < 0)
		snprintf(text, (size_t)size, "%s", ban->when);
	else if (age < 90)
		snprintf(text, (size_t)size, "JUST NOW");
	else if (age < 3600)
		snprintf(text, (size_t)size, "%ld MIN AGO", age / 60);
	else if (age < 86400)
		snprintf(text, (size_t)size, "%ld H AGO", age / 3600);
	else
		snprintf(text, (size_t)size, "%ld %s AGO", age / 86400, age / 86400 == 1 ? "DAY" : "DAYS");
}

static void rows_make(void)
{
	struct ae_flow_screen *flow = &screen.flow;
	short count = 0, index;

	screen.shown_count = 0;
	/* (the newest are last in the file) */
	for (index = (short)(screen.count - 1); index >= 0 && screen.shown_count < LISTED; index--)
		screen.shown[screen.shown_count++] = index;
	if (!screen.count)
	{
		flow->rows[count++] = (struct ae_flow_row){ ROW_QUIET, -1, "No banned players.", NULL,
			"Players you ban from the pause menu's PLAYERS page are listed here.", NULL, 0, 0, 1 };
	}
	for (index = 0; index < screen.shown_count; index++)
	{
		struct ae_moderation_ban const *ban = &screen.bans[screen.shown[index]];

		when_text(ban, screen.when[index], sizeof(screen.when[index]));
		snprintf(screen.help[index], sizeof(screen.help[index]), "{A} lets %.40s join the games this PC hosts again.", ban->name);
		flow->rows[count++] = (struct ae_flow_row){ ROW_BAN, -1, ban->name, screen.when[index], screen.help[index], NULL,
			0, 1 };
	}
	if (screen.count > screen.shown_count)
	{
		snprintf(screen.more, sizeof(screen.more), "%ld older in bans.txt", screen.count - screen.shown_count);
		flow->rows[count++] = (struct ae_flow_row){ ROW_MORE, -1, screen.more, NULL, "Take those lines out of bans.txt by hand.",
			NULL, 0, 0, 1 };
	}
	flow->count = count;
	if (flow->focus >= count)
		flow->focus = (short)(count > 0 ? count - 1 : 0);
}

static void unban_picked(short choice, void *context)
{
	short index = screen.asking;

	(void)context;
	screen.asking = -1;
	if (choice != 1 || index < 0 || index >= screen.count)
		return;
	if (ae_moderation_unban(screen.bans[index].number))
	{
		platform_log("ae banned: unbanned %s", screen.bans[index].name);
		ae_sound_request(AE_SOUND_FORWARD, 0);
		bans_read();
	}
	else
		ae_flow_error(&screen.flow, "Could not unban", "bans.txt could not be written. Take the line out by hand.");
}

static void ask_unban(short row)
{
	struct ae_dialog_spec spec;
	static char title[96], body[160];
	short index;

	if (row < 0 || row >= screen.flow.count || screen.flow.rows[row].kind != ROW_BAN)
	{
		ae_sound_request(AE_SOUND_FAILURE, 0);
		return;
	}
	index = screen.shown[row];
	screen.asking = index;
	snprintf(title, sizeof(title), "Unban %s?", screen.bans[index].name);
	snprintf(body, sizeof(body), "%s can join the games this PC hosts again.", screen.bans[index].name);
	memset(&spec, 0, sizeof(spec));
	spec.kind = AE_DIALOG_CONFIRM;
	spec.title = title;
	spec.body = body;
	spec.choices[0] = ae_string(AE_STR_CANCEL_CAPS);
	spec.choices[1] = "UNBAN";
	spec.choice_count = 2;
	spec.safe_choice = 0;
	spec.cancel_choice = 0;
	spec.has_cancel = 1;
	spec.density = screen.flow.density;
	spec.bounds = screen.flow.frame.rect;
	spec.picked = unban_picked;
	if (!ae_dialog_open(&spec, AE_OWNER_ANY))
		screen.asking = -1;
	else
	{
		ae_sound_request(AE_SOUND_FORWARD, 0);
		platform_log("ae banned: unban asked %s", screen.bans[index].name);
	}
}

static void banned_draw(struct ae_screen *instance)
{
	struct ae_flow_screen *flow = &screen.flow;

	(void)instance;
	ae_flow_full_density(flow);
	rows_make();
	ae_flow_title(flow, "BANNED PLAYERS", "ONLINE \xE2\x80\xA2 HOSTING");
	ae_flow_draw_rows(flow, flow->frame.rect.width * AE_FLOW_ROWS_WIDTH);
	flow->prompt_count = 0;
	if (screen.count)
		flow->prompts[flow->prompt_count++] = ae_flow_prompt(AE_BUTTON_A, "Unban", AE_ACTION_ACCEPT);
	flow->prompts[flow->prompt_count++] = ae_flow_prompt(AE_BUTTON_B, ae_string(AE_STR_BACK), AE_ACTION_BACK);
	ae_flow_draw_prompts(flow);
}

static int banned_handle(struct ae_screen *instance, struct ae_event const *event)
{
	(void)instance;
	switch (event->action)
	{
	case AE_ACTION_BACK:
		return 0;
	case AE_ACTION_UP:
	case AE_ACTION_DOWN:
		ae_flow_focus_step(&screen.flow, event->action == AE_ACTION_UP ? -1 : 1, event->repeat);
		return 1;
	case AE_ACTION_ACCEPT:
		ask_unban(screen.flow.focus);
		return 1;
	default:
		return 1;
	}
}

static void banned_pointer(struct ae_screen *instance, struct ae_pointer const *pointer)
{
	struct ae_event event;
	short row;

	if (ae_prompts_pointer(screen.flow.prompts, screen.flow.prompt_count, pointer, HIT_PROMPTS, &event))
	{
		if (event.action == AE_ACTION_BACK)
		{
			ae_sound_request(AE_SOUND_BACK, 0);
			ae_ui_pop();
		}
		else
			banned_handle(instance, &event);
		return;
	}
	row = ae_flow_rows_pointer(&screen.flow, pointer);
	if (row >= 0)
		ask_unban(row);
}

static void banned_leave(struct ae_screen *instance)
{
	(void)instance;
	platform_log("ae banned: closed");
}

static struct ae_screen_class const banned_class =
{
	.name = "banned players", .leave = banned_leave, .handle = banned_handle, .draw = banned_draw,
	.pointer = banned_pointer,
};

int ae_screen_banned_open(void)
{
	memset(&screen, 0, sizeof(screen));
	screen.asking = -1;
	screen.flow.hit_rows = HIT_ROWS;
	screen.flow.hit_prompts = HIT_PROMPTS;
	bans_read();
	if (!ae_ui_push(&banned_class, AE_OWNER_ANY, &screen))
		return 0;
	platform_log("ae banned: open (%ld bans)", screen.count);
	return 1;
}

void ae_screen_banned_summary(char *text, int size)
{
	static struct ae_moderation_ban bans[AE_MODERATION_BANS_MAXIMUM];
	long count = ae_moderation_bans(bans, AE_MODERATION_BANS_MAXIMUM);

	if (!count)
		snprintf(text, (size_t)size, "NONE");
	else
		snprintf(text, (size_t)size, "%ld %s", count, count == 1 ? "PLAYER" : "PLAYERS");
}
