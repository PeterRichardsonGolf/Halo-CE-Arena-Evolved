/*
AE_JOIN_HELPER.C

The join helper (ae_join_helper.h).
*/

#include <stdio.h>
#include <string.h>

#include "cseries.h"
#include "ae_glue_lobby.h"
#include "ae_hooks.h"
#include "../src/ae_mcc.h"
#include "ae_join_helper.h"
#include "ae_silent_rules.h"
#include "ae_screen_server_lobby.h"
#include "ae_screen_attention.h"
#include "ae_sound.h"

void platform_log(char const *format, ...);
unsigned long system_milliseconds(void);
int config_set(char const *name, char const *value);
void updater_start(void);
void platform_open_url(const char *url);

/* a rejection code counts for this long after it came (the join screens see the failure a few frames later) */
#define REJECTION_MS 6000UL

static struct
{
	int code;                      /* -1 none */
	unsigned long at;
	int taken;
} note = { -1, 0, 0 };

/* the one request being shown, kept for its second look after OPEN IN STEAM / GET MCC */
static struct
{
	struct ae_join_helper_request request;
	char detail[160], map[64], title[96];
	int opened;
} shown;

static void show_words(void);

static void again_pick(void)
{
	void (*again)(void *) = shown.request.again;
	void *context = shown.request.context;

	if (again)
		again(context);
}

/* (0.7.9) the words a cause has on this PC: GET MCC only where Steam's MCC can be played (not a Mac, not Android) */
static void words_here(struct ae_join_helper_words *words)
{
	ae_join_helper_words(shown.request.cause, shown.detail, shown.map, words);
#if defined(__APPLE__) || defined(HALO_ANDROID)
	if (words->action_kind == AE_JH_ACTION_GET_MCC)
	{
		words->action_kind = AE_JH_ACTION_OK;
		snprintf(words->action, sizeof(words->action), "OK");
		words->second[0] = 0;
	}
#endif
	/* (no join to make again from here (a map found missing after the join): no JOIN AGAIN) */
	if (words->second_kind == AE_JH_ACTION_AGAIN && !shown.request.again)
	{
		words->second_kind = AE_JH_ACTION_OK;
		snprintf(words->second, sizeof(words->second), "NOT NOW");
		snprintf(words->note, sizeof(words->note), "When Steam's download is done, join the game again.");
	}
}

/* (0.7.9) a Workshop map's JOIN AGAIN: Steam's new download looked for first (the catalog is read once a start) */
static void again_after_steam(void)
{
	if (shown.request.cause == AE_JH_WORKSHOP)
		ae_mcc_workshop_rescan();
	again_pick();
}

static void act(int kind)
{
	char item[24], link[200];

	switch (kind)
	{
	case AE_JH_ACTION_UPDATE:
		ae_join_helper_update();
		break;
	case AE_JH_ACTION_AGAIN:
		again_after_steam();
		break;
	/* (0.7.9) MCC FILES ARE OFF: turned on, and the same join made again */
	case AE_JH_ACTION_MCC_ON:
		ae_mcc_answer_use();
		platform_log("join helper: MCC files turned on, joining again");
		again_pick();
		break;
	/* (0.7.9) the item's page in the Steam app; its browser link copied too, for a PC whose Steam does not open */
	case AE_JH_ACTION_OPEN_STEAM:
		ae_join_helper_workshop_item(shown.map, item, (int)sizeof(item));
		ae_join_helper_steam_link(item, link, (int)sizeof(link));
		if (!link[0])
			break;
		platform_open_url(link);
		ae_join_helper_workshop_link(item, link, (int)sizeof(link));
		ae_host_clipboard_set(link);
		platform_log("join helper: Workshop item %s opened in Steam (link copied: %s)", item, link);
		shown.opened = 1;
		show_words();
		break;
	case AE_JH_ACTION_GET_MCC:
		platform_open_url(AE_JH_MCC_STORE_LINK);
		platform_log("join helper: MCC's store page opened in Steam");
		break;
	default:
		break;
	}
}

static void picked(short button, void *context)
{
	struct ae_join_helper_words words;

	(void)context;
	words_here(&words);
	if (button == 0)
		act(words.action_kind);
	else if (button == 1 && words.second[0])
		act(words.second_kind);
}

static void show_words(void)
{
	struct ae_join_helper_words words;
	struct ae_attention_spec spec;
	char item[24], line1[96], line2[96];

	words_here(&words);
	memset(&spec, 0, sizeof(spec));
	spec.title = words.title;
	spec.body = words.reason;
	spec.buttons[0] = words.action;
	spec.button_count = 1;
	spec.danger = -1;
	spec.back = -1;
	spec.note = words.note[0] ? words.note : NULL;
	spec.back_label = "Back";
	spec.density = shown.request.density;
	spec.picked = picked;
	if (words.second[0])
	{
		spec.buttons[1] = words.second;
		spec.button_count = 2;
		/* (opened in Steam: the next step is the second button) */
		if (shown.opened)
			spec.focus = 1;
	}
	/* a Workshop map names its item: its title, its number */
	ae_join_helper_workshop_item(shown.map, item, (int)sizeof(item));
	if (shown.request.cause == AE_JH_WORKSHOP && item[0])
	{
		snprintf(line1, sizeof(line1), "Steam Workshop item %s", item);
		snprintf(line2, sizeof(line2), "%s", shown.opened ? "Opened in Steam. Link copied too, for a browser." :
			"Subscribe to it in Steam");
		spec.item_title = shown.title[0] ? shown.title : "Workshop map";
		spec.item_line1 = line1;
		spec.item_line2 = line2;
		spec.item_tile = shown.opened ? "IN STEAM" : "NOT ON THIS PC";
	}
	if (!ae_attention_open(&spec))
		platform_log("join helper: could not open");
}

int ae_join_helper_show(struct ae_join_helper_request const *request)
{
	if (!request || (request->cause == AE_JH_GENERIC && !(request->detail && request->detail[0])))
		return 0;
	memset(&shown, 0, sizeof(shown));
	shown.request = *request;
	snprintf(shown.detail, sizeof(shown.detail), "%s", request->detail ? request->detail : "");
	snprintf(shown.map, sizeof(shown.map), "%s", request->map ? request->map : "");
	snprintf(shown.title, sizeof(shown.title), "%s", request->item_title ? request->item_title : "");
	shown.request.detail = shown.detail;
	shown.request.map = shown.map;
	shown.request.item_title = shown.title;
	platform_log("join helper: cause %d%s%s", request->cause, shown.detail[0] ? ": " : "", shown.detail);
	show_words();
	return ae_attention_is_open();
}

int ae_join_helper_failed(int reason, char const *detail, struct ae_density const *density,
	void (*again)(void *context), void *context)
{
	struct ae_join_helper_request request;
	int cause = ae_join_helper_cause_of_reason(reason);
	char minutes[16];

	if (note.code >= 0 && system_milliseconds() - note.at < REJECTION_MS &&
		ae_join_helper_cause_of_rejection(note.code) != AE_JH_GENERIC)
		cause = ae_join_helper_cause_of_rejection(note.code);
	/* (0.7.9) held out: the wait is in the code's high byte, the words' detail */
	if (cause == AE_JH_HELD)
	{
		snprintf(minutes, sizeof(minutes), "%d", ae_held_rejection_minutes((unsigned int)note.code));
		detail = minutes;
	}
	/* (0.7.9) MCC's files refused, but Steam's MCC is here and only turned off: the helper offers to turn it on */
	if (cause == AE_JH_NEEDS_MCC && ae_mcc_state() == AE_MCC_FOUND && ae_mcc_use() != AE_MCC_USE_YES)
		cause = AE_JH_MCC_OFF;
	note.code = -1;
	note.taken = 0;
	memset(&request, 0, sizeof(request));
	request.cause = cause;
	request.detail = detail;
	request.density = *density;
	request.again = again;
	request.context = context;
	return ae_join_helper_show(&request);
}

void ae_join_helper_rejected(unsigned short code)
{
	note.code = code;
	note.at = system_milliseconds();
	note.taken = 0;
	/* (an Arena Evolved join that waits for the host's answer: its screen shows this; another join keeps the
	game's own error box) */
	if (ae_menus_active() && ae_lobby_join_state(NULL) == 0 && ae_join_helper_cause_of_rejection(code) != AE_JH_GENERIC)
		note.taken = 1;
	/* (0.7.7: removed from a dedicated server's lobby: its page tells it) */
	if (ae_menus_active() && ae_screen_server_lobby_rejected(code))
		note.taken = 1;
}

int ae_join_helper_taken(void)
{
	return note.taken;
}

int ae_join_helper_refusal_recent(void)
{
	return note.code >= 0 && system_milliseconds() - note.at < REJECTION_MS &&
		ae_join_helper_cause_of_rejection(note.code) != AE_JH_GENERIC;
}

void ae_join_helper_update(void)
{
	/* the game asks as it does at its start: the check runs, and an update found is offered by the game's own box
	(an install; on a Mac its download page). The AUTO UPDATE switch is for the start only: this asks anyway */
	config_set("update.auto", "true");
	updater_start();
	platform_log("join helper: update asked for");
}
