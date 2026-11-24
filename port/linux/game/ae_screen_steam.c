/*
AE_SCREEN_STEAM.C

AE's STEAM ACCOUNT (ae_screen_steam.h). One screen with two pages:
the account page (signed out: ACCOUNT NAME, PASSWORD, SIGN IN and the facts card; Steam Guard: the code, or the app's
approval; signed in: ACCOUNT, LOG OUT, MCC FILES) and the WORKSHOP MAPS page (SEARCH, TYPE chips, SORT, a page of
items and the focused one's card). Left column x 96 / width 883 and the right card from x 1095 (1920 x 1080 u), the
help line over the footer's prompts, as AE's other flow screens (ae_setup_flow.h).

The state is the client's (ae_steam_client.h through ae_steam_rules.h): each frame its status is copied, the page is
the one its session state is on, and the screen asks the client for what the player does. A password or Steam Guard
code is handed to Steam and wiped here at once; neither is ever logged. DEV MODE's `menu steam <state>` runs the screen
on a made-up status (ae_steam_stage_status) and asks the client for nothing.

Focus: the fields and rows of the page, up and down; A (Enter, a click) on a field types into it (a keyboard: in the
field, Enter keeps and goes on, Esc restores; a pad: AE's keyboard under it), START anywhere on the sign-in page
signs in. B cancels a sign-in under way, goes from WORKSHOP MAPS to the account page, and leaves the screen (asking
first while a download runs).

Its log: "ae steam: open", "page <name>", "files <state>", "download <n> %" (each 10 %), "message <kind>", "sign-in
asked", "code asked", "sign-in cancelled", "log out asked", "download asked / cancelled", "workshop open / left",
"search", "closed". Never the password, the code, the token, or the account's name.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cseries.h"
#include "../src/ae_draw.h"
#include "../src/ae_cover_rules.h"
#include "../src/ae_mcc.h"
#include "../src/ae_steam_client.h"
#include "ae_motion.h"
#include "ae_screen_steam.h"
#include "ae_screen_maps_hub.h"
#include "ae_setup_flow.h"
#include "ae_sound.h"
#include "ae_steam_rules.h"
#include "ae_strings.h"
#include "ae_ui.h"
#include "ae_widgets.h"
#include "ae_map_library.h"
#include "ae_glue_setup_maps.h"
#include "mcc_maps.h"

void platform_log(char const *format, ...);
void platform_open_url(const char *url);

enum
{
	STEAM_HIT = 0x7a00,
	HIT_ROWS = STEAM_HIT + 1, HIT_PROMPTS, HIT_ITEMS, HIT_CHIPS, HIT_LIST, HIT_FIELD,
	/* (SHOW: COLLECTIONS) its rows, and the maps list of an opened collection; a field of FIELDS hits after HIT_FIELD */
	HIT_COLL = STEAM_HIT + 16, HIT_COLL_LIST
};

/* the fields, in the order of the account page */
enum { F_ACCOUNT, F_PASSWORD, F_CODE, F_SEARCH, FIELDS };
/* what can have the focus */
enum
{
	IT_NONE, IT_ACCOUNT, IT_PASSWORD, IT_SIGN_IN, IT_FILES, IT_WORKSHOP, IT_LOG_OUT, IT_CODE, IT_CODE_SIGN_IN, IT_CANCEL,
	IT_CODE_INSTEAD, IT_SEARCH, IT_SORT, IT_LIST, IT_SHOW, IT_COLL, IT_COUNT
};
enum { ITEMS_MOST = 12, LIST_ROWS = 7, TYPES = 3 };

struct field
{
	struct ae_text text;
	struct ae_field_edit edit;
	struct ae_rect well;
	unsigned long caret_since;
};

static struct
{
	struct ae_flow_screen flow;
	int stage;                       /* -1: the real client; else AE_STEAM_STAGE_* */
	int stage_percent;
	struct ae_steam_status status;
	int page, last_page;
	int workshop;                    /* the WORKSHOP MAPS page */
	char origin[40];                 /* where it was opened from (SETTINGS, MAPS & MODS ...), for the log */
	int hub;                         /* MAPS & MODS' WORKSHOP page: its tab strip over the page, the TYPE chips a SHOW row */
	int code_instead;
	short focus;
	struct field fields[FIELDS];
	int typing;                      /* the field typed into, -1 none */
	int keyboard;                    /* the field AE's keyboard is open for, -1 none */
	struct ae_rect rects[IT_COUNT];
	struct ae_list_view list;
	short list_count;
	short list_focus_sent;
	int asking;                      /* a dialog of this screen's is open */
	int armed;                       /* READY: A again downloads the files anew */
	int first_frame;
	int logged_shared, logged_decile, logged_kind;
	int logged_page;                 /* the page the log last named (-1: none): a name is logged once per change */
	char dismissed[256];             /* the notice the player moved on from */
	unsigned long page_since;
	unsigned long staged_until;      /* a staged page: the notice of a press that sent nothing shows until this time */
	int pending_logout;              /* DEV: the LOG OUT dialog to open on the first frame */
	int pending_typing;              /* DEV: typing into this field on the first frame (-1 none) */
	struct ae_rect type_chips[TYPES];
} S;

static struct ae_screen_class const steam_class;
/* (SHOW: COLLECTIONS: ae_screen_steam_collections.inc, included before the screen's drawing) */
static int coll_active(void);
static int coll_level(void);
static void coll_reset(void);
static void coll_show_step(int direction);
static void coll_sort_step(void);
static char const *coll_sort_text(void);
static void coll_accept(void);
static void coll_move(struct ae_event const *event);
static char const *coll_help(void);
static void coll_draw_list(float x, float y, float width);
static void coll_draw_open(float x, float y, float width);
static void coll_search(char const *text);
/* (the focused Workshop item's picture let go: below) */
static void picture_release(void);
static void coll_pictures_release(void);
/* (what AE's keyboard hands back to keyboard_done: the field's number) */
static int const field_ids[FIELDS] = { F_ACCOUNT, F_PASSWORD, F_CODE, F_SEARCH };

static char const *const field_labels[FIELDS] = { "ACCOUNT NAME", "PASSWORD", "CODE", "SEARCH" };
static char const *const type_names[TYPES] = { "ALL", "CAMPAIGN", "MULTIPLAYER" };
static char const *const sort_names[4] = { "TRENDING", "MOST SUBSCRIBED", "NEWEST", "TOP RATED" };

static int system_kind(void)
{
#if defined(__APPLE__)
	return AE_STEAM_SYSTEM_MACOS;
#elif defined(_WIN32)
	return AE_STEAM_SYSTEM_WINDOWS;
#else
	return AE_STEAM_SYSTEM_LINUX;
#endif
}

static char const *page_name(int page)
{
	static char const *const names[] = { "signed out", "signing in", "steam guard code", "steam guard app", "signed in" };

	return page >= 0 && page < 5 ? names[page] : "?";
}

/* ---------- the status */

/* the client's frame; a download that put files in mcc_maps: the game's lists of maps are read again, so the item's
maps are in CHANGE MAP and MAPS & MODS at once */
static void client_frame(void)
{
	ae_steam_client_frame();
	if (!ae_steam_client_take_rescan())
		return;
	mcc_maps_rescan();
	ae_mcc_workshop_rescan();
	ae_map_library_refresh();
	platform_log("ae steam: files landed in mcc_maps: %d maps there now", (int)mcc_maps_count(FALSE));
}

static void status_fetch(void)
{
	if (S.stage >= 0)
		return;
	client_frame();
	ae_steam_client_status(&S.status);
}

static int client_live(void)
{
	return S.stage < 0;
}

/* a staged page's press that would have asked Steam something: logged, and the page says so for a few seconds */
#define STAGED_NOTICE_MS 5000UL
static void staged_note(void)
{
	platform_log("ae steam: staged: nothing is sent");
	S.staged_until = ae_motion_now() + STAGED_NOTICE_MS;
}

/* ---------- the fields */

static short field_limit(int field)
{
	return field == F_CODE ? AE_STEAM_CODE_LIMIT : field == F_PASSWORD ? 100 : 64;
}

static void fields_init(void)
{
	int field;

	for (field = 0; field < FIELDS; field++)
	{
		memset(&S.fields[field], 0, sizeof(S.fields[field]));
		ae_text_init(&S.fields[field].text, "", field_limit(field), 0);
		S.fields[field].text.secret = field == F_PASSWORD;
	}
}

static void field_wipe(int field)
{
	volatile char *at = S.fields[field].text.text;
	volatile char *before = S.fields[field].edit.before.text;
	int index;

	for (index = 0; index < AE_TEXT_MAXIMUM; index++)
	{
		at[index] = 0;
		/* (the edit's snapshot of the text from before holds the same words) */
		before[index] = 0;
	}
	ae_text_init(&S.fields[field].text, "", field_limit(field), 0);
	S.fields[field].text.secret = field == F_PASSWORD;
}

static void typing_done(int keep, void *context)
{
	(void)keep;
	(void)context;
	S.typing = -1;
}

static void keyboard_done(int keep, void *context);

static void typing_begin(int field)
{
	struct field *f = &S.fields[field];

	f->edit.done = typing_done;
	f->edit.context = NULL;
	f->edit.holder = &S;
	ae_field_edit_begin(&f->edit, &f->text);
	ae_text_select_all(&f->text);
	S.typing = field;
	f->caret_since = ae_motion_now();
	ae_sound_request(AE_SOUND_FORWARD, 0);
}

static void keyboard_open_for(int field)
{
	struct ae_keyboard_spec spec;
	struct field *f = &S.fields[field];

	memset(&spec, 0, sizeof(spec));
	spec.text = &f->text;
	spec.field = f->well;
	spec.bounds = S.flow.frame.rect;
	spec.density = S.flow.density;
	spec.done = keyboard_done;
	spec.context = (void *)&field_ids[field];
	f->edit.done = typing_done;
	f->edit.context = NULL;
	f->edit.holder = &S;
	spec.edit = &f->edit;
	if (ae_keyboard_open(&spec, AE_OWNER_ANY))
		S.keyboard = field;
}

static short field_item(int field)
{
	switch (field)
	{
	case F_ACCOUNT: return IT_ACCOUNT;
	case F_PASSWORD: return IT_PASSWORD;
	case F_CODE: return IT_CODE;
	default: return IT_SEARCH;
	}
}

/* ---------- what the player asks of the client */

static void sign_in_submit(void)
{
	if (!ae_steam_can_sign_in(S.fields[F_ACCOUNT].text.text, S.fields[F_PASSWORD].text.text))
	{
		/* (the first empty field gets the focus) */
		S.focus = S.fields[F_ACCOUNT].text.length ? IT_PASSWORD : IT_ACCOUNT;
		ae_sound_request(AE_SOUND_FAILURE, 0);
		return;
	}
	platform_log("ae steam: sign-in asked");
	if (client_live())
		ae_steam_client_sign_in(S.fields[F_ACCOUNT].text.text, S.fields[F_PASSWORD].text.text);
	else
		staged_note();
	/* (to Steam only: gone from here) */
	field_wipe(F_PASSWORD);
	ae_sound_request(AE_SOUND_FORWARD, 0);
}

static void code_submit(void)
{
	if (!ae_steam_can_submit_code(S.fields[F_CODE].text.text))
	{
		S.focus = IT_CODE;
		ae_sound_request(AE_SOUND_FAILURE, 0);
		return;
	}
	platform_log("ae steam: code asked");
	if (client_live())
		ae_steam_client_guard_code(S.fields[F_CODE].text.text);
	else
		staged_note();
	field_wipe(F_CODE);
	ae_sound_request(AE_SOUND_FORWARD, 0);
}

static void sign_in_cancel(void)
{
	platform_log("ae steam: sign-in cancelled");
	if (client_live())
		ae_steam_client_cancel_sign_in();
	S.code_instead = 0;
	ae_sound_request(AE_SOUND_BACK, 0);
}

static void search_apply(void)
{
	if (coll_active())
	{
		coll_search(S.fields[F_SEARCH].text.text);
		return;
	}
	platform_log("ae steam: search");
	if (client_live())
		ae_steam_client_search(S.fields[F_SEARCH].text.text);
	ae_list_set_focus(&S.list.list, 0);
}

static void typing_finished(int field, int keep)
{
	if (!keep)
		return;
	switch (field)
	{
	case F_ACCOUNT:
		S.focus = IT_PASSWORD;
		break;
	case F_PASSWORD:
		sign_in_submit();
		break;
	case F_CODE:
		code_submit();
		break;
	case F_SEARCH:
		search_apply();
		break;
	default:
		break;
	}
}

static void keyboard_done(int keep, void *context)
{
	int field = context ? *(int const *)context : -1;

	S.keyboard = -1;
	if (field < 0 || field >= FIELDS)
		return;
	/* (a physical key closed it into the field's own typing: that goes on) */
	if (S.fields[field].edit.active && S.fields[field].edit.text == &S.fields[field].text)
	{
		S.typing = field;
		S.fields[field].caret_since = ae_motion_now();
		return;
	}
	/* (a pad: done on the account name goes on to the password; the password's done only closes, SIGN IN is the next A or START) */
	if (keep && field != F_PASSWORD)
		typing_finished(field, 1);
}

static void edit_begin(int field, int device)
{
	S.focus = field_item(field);
	if (device == AE_DEVICE_KEYBOARD_MOUSE)
		typing_begin(field);
	else
		keyboard_open_for(field);
}

static void typing_end(int keep)
{
	int field = S.typing;

	if (field < 0)
		return;
	S.typing = -1;
	ae_field_edit_end(&S.fields[field].edit, keep);
	if (keep)
		typing_finished(field, 1);
	else
		ae_sound_request(AE_SOUND_BACK, 0);
}

/* the typing ended with its text kept and nothing sent (up or down moved on) */
static void typing_stop_keep(void)
{
	int field = S.typing;

	if (field < 0)
		return;
	S.typing = -1;
	ae_field_edit_end(&S.fields[field].edit, 1);
}

/* ---------- which items the page has */

static int items_now(short *ids)
{
	int count = 0;

	if (S.workshop)
	{
		if (coll_active() && coll_level() == 2)
		{
			ids[count++] = IT_COLL;
			return count;
		}
		ids[count++] = IT_SEARCH;
		if (S.hub)
			ids[count++] = IT_SHOW;
		ids[count++] = IT_SORT;
		if (coll_active())
			ids[count++] = IT_COLL;
		else if (S.status.item_count > 0)
			ids[count++] = IT_LIST;
		return count;
	}
	switch (S.page)
	{
	case AE_STEAM_PAGE_OUT:
		ids[count++] = IT_ACCOUNT;
		ids[count++] = IT_PASSWORD;
		ids[count++] = IT_SIGN_IN;
		ids[count++] = IT_FILES;
		ids[count++] = IT_WORKSHOP;
		break;
	case AE_STEAM_PAGE_SIGNING_IN:
		ids[count++] = IT_CANCEL;
		break;
	case AE_STEAM_PAGE_GUARD_CODE:
		ids[count++] = IT_CODE;
		ids[count++] = IT_CODE_SIGN_IN;
		ids[count++] = IT_CANCEL;
		break;
	case AE_STEAM_PAGE_GUARD_APP:
		if (ae_steam_code_instead_possible(&S.status))
			ids[count++] = IT_CODE_INSTEAD;
		ids[count++] = IT_CANCEL;
		break;
	default:
		ids[count++] = IT_LOG_OUT;
		ids[count++] = IT_FILES;
		ids[count++] = IT_WORKSHOP;
		break;
	}
	return count;
}

static short default_focus(void)
{
	if (S.workshop)
		return coll_active() ? IT_COLL : S.status.item_count > 0 ? IT_LIST : IT_SEARCH;
	switch (S.page)
	{
	case AE_STEAM_PAGE_OUT:
		return S.fields[F_ACCOUNT].text.length ? IT_PASSWORD : IT_ACCOUNT;
	case AE_STEAM_PAGE_SIGNING_IN:
		return IT_CANCEL;
	case AE_STEAM_PAGE_GUARD_CODE:
		return IT_CODE;
	case AE_STEAM_PAGE_GUARD_APP:
		return ae_steam_code_instead_possible(&S.status) ? IT_CODE_INSTEAD : IT_CANCEL;
	default:
		/* (an account that owns nothing: LOG OUT, as there is nothing else to do) */
		return S.status.owns == 0 ? IT_LOG_OUT : IT_FILES;
	}
}

static int focus_valid(void)
{
	short ids[ITEMS_MOST];
	int count = items_now(ids), index;

	for (index = 0; index < count; index++)
		if (ids[index] == S.focus)
			return 1;
	return 0;
}

static void focus_step(int direction, int repeat)
{
	short ids[ITEMS_MOST];
	int count = items_now(ids), index, at = -1;

	for (index = 0; index < count; index++)
		if (ids[index] == S.focus)
			at = index;
	if (at < 0)
	{
		S.focus = default_focus();
		return;
	}
	at += direction;
	if (at < 0 || at >= count)
		return;
	S.focus = ids[at];
	S.armed = 0;
	S.dismissed[0] = 0;
	snprintf(S.dismissed, sizeof(S.dismissed), "%s", S.status.message);
	ae_sound_request(AE_SOUND_CURSOR, repeat);
}

/* ---------- the Workshop page */

static void list_sync(void)
{
	short count = (short)S.status.item_count;

	if (count != S.list_count)
	{
		S.list_count = count;
		ae_list_set_count(&S.list.list, count);
		if (S.list.list.focus < 0 && count > 0)
			ae_list_set_focus(&S.list.list, 0);
		S.list_focus_sent = -1;
	}
	if (S.list.list.focus >= 0 && client_live())
		/* (told every frame: the client moves its own focus when a page arrives) */
		ae_steam_client_item_focus(S.list.list.focus);
	else if (S.list.list.focus >= 0 && S.list.list.focus != S.list_focus_sent)
	{
		S.list_focus_sent = S.list.list.focus;
		ae_steam_stage_item_card(S.list.list.focus, &S.status);
	}
}

/* the Workshop page was asked for on the account page: the account page comes back when the hub's WORKSHOP page is left with B */
static int reopen_account;

static void workshop_open(void)
{
	if (S.page != AE_STEAM_PAGE_IN || S.status.owns == 0)
	{
		ae_sound_request(AE_SOUND_FAILURE, 0);
		return;
	}
	S.workshop = 1;
	if (client_live())
		ae_steam_client_workshop(1);
	ae_list_view_init(&S.list, 0, LIST_ROWS);
	S.list_count = 0;
	S.list_focus_sent = -1;
	S.focus = IT_LIST;
	S.armed = 0;
	platform_log("ae steam: workshop open");
	ae_sound_request(AE_SOUND_FORWARD, 0);
}

static void workshop_leave(void)
{
	S.workshop = 0;
	if (client_live())
		ae_steam_client_workshop(0);
	picture_release();
	coll_pictures_release();
	S.focus = IT_WORKSHOP;
	platform_log("ae steam: workshop left");
	ae_sound_request(AE_SOUND_BACK, 0);
}

static void type_step(int steps)
{
	int index;

	/* (MAPS & MODS' SHOW row has a fourth value, COLLECTIONS: left and right step through the four) */
	if (S.hub)
	{
		coll_show_step(steps == 1 ? 1 : -1);
		return;
	}

	if (client_live())
		for (index = 0; index < steps; index++)
			ae_steam_client_filter_step(1);
	else
		S.status.type = (S.status.type + steps) % TYPES;
	ae_list_set_focus(&S.list.list, 0);
	ae_sound_request(AE_SOUND_CURSOR, 0);
}

static void item_name_state(int index, char *name, int name_size, char *state, int state_size)
{
	if (client_live())
		ae_steam_client_item_row(index, name, name_size, state, state_size);
	else
		ae_steam_stage_item(index, name, (size_t)name_size, state, (size_t)state_size);
}

/* ---------- dialogs */

static void dialog_closed(void)
{
	S.asking = 0;
}

static void logout_picked(short choice, void *context)
{
	(void)context;
	dialog_closed();
	if (choice != 1)
		return;
	platform_log("ae steam: log out asked");
	if (client_live())
		ae_steam_client_log_out();
	else
		staged_note();
}

static void logout_ask(void)
{
	struct ae_dialog_spec spec;

	memset(&spec, 0, sizeof(spec));
	spec.kind = AE_DIALOG_CONFIRM;
	spec.title = "LOG OUT OF STEAM?";
	spec.body = "Steam ends this PC's sign-in, and AE forgets it. Maps you downloaded stay in mcc_maps.";
	spec.choices[0] = ae_string(AE_STR_CANCEL_CAPS);
	spec.choices[1] = "LOG OUT";
	spec.choice_count = 2;
	spec.safe_choice = 0;
	spec.cancel_choice = 0;
	spec.has_cancel = 1;
	spec.destructive_choices = 1u << 1;
	spec.density = S.flow.density;
	spec.bounds = S.flow.frame.rect;
	spec.picked = logout_picked;
	if (ae_dialog_open(&spec, AE_OWNER_ANY))
	{
		S.asking = 1;
		ae_sound_request(AE_SOUND_FORWARD, 0);
	}
}

/* A on a Workshop item that is here and current: asked first (its files would all be fetched again, or the item
removed from this PC). The item is the one the question was opened for, whatever the list shows when it is answered */
static unsigned long long again_item;

static void again_picked(short choice, void *context)
{
	(void)context;
	dialog_closed();
	if (choice == 2)
	{
		int in_use = 0;

		if (!client_live())
		{
			staged_note();
			return;
		}
		/* (a lobby or a game on any map of the item keeps it) */
		(void)ae_map_library_workshop_item_maps(again_item, &in_use);
		if (again_item && !in_use && ae_steam_client_item_remove(again_item))
		{
			platform_log("ae steam: item %llu removed from this PC", again_item);
			ae_sound_request(AE_SOUND_FORWARD, 0);
		}
		else
			ae_sound_request(AE_SOUND_FAILURE, 0);
		return;
	}
	if (choice != 1 || S.list.list.focus < 0)
		return;
	if (client_live() && ae_steam_client_item_id(S.list.list.focus) != again_item)
	{
		ae_sound_request(AE_SOUND_FAILURE, 0);
		return;
	}
	platform_log("ae steam: item downloaded again");
	if (client_live())
		ae_steam_client_item_choose(S.list.list.focus);
	else
		staged_note();
}

static void again_ask(void)
{
	static char body[240];
	struct ae_dialog_spec spec;

	memset(&spec, 0, sizeof(spec));
	again_item = client_live() && S.list.list.focus >= 0 ? ae_steam_client_item_id(S.list.list.focus) : 0;
	snprintf(body, sizeof(body), "%.96s is in mcc_maps and up to date. Downloading it again fetches every file of it. "
		"Removing it deletes its maps from this PC.", S.status.item_title[0] ? S.status.item_title : "This item");
	spec.kind = AE_DIALOG_CONFIRM;
	spec.title = "INSTALLED";
	spec.body = body;
	spec.choices[0] = ae_string(AE_STR_CANCEL_CAPS);
	spec.choices[1] = "DOWNLOAD AGAIN";
	spec.choices[2] = "REMOVE FROM THIS PC";
	spec.choice_count = 3;
	spec.safe_choice = 0;
	spec.cancel_choice = 0;
	spec.has_cancel = 1;
	spec.destructive_choices = 1u << 2;
	spec.density = S.flow.density;
	spec.bounds = S.flow.frame.rect;
	spec.picked = again_picked;
	if (ae_dialog_open(&spec, AE_OWNER_ANY))
	{
		S.asking = 1;
		ae_sound_request(AE_SOUND_FORWARD, 0);
	}
}

/* the focused Workshop item's page on Steam, in the PC's browser */
static void item_page_open(void)
{
	unsigned long long id = client_live() && S.list.list.focus >= 0 ? ae_steam_client_item_id(S.list.list.focus) : 0;
	char url[96];

	if (!id)
	{
		if (!client_live())
			staged_note();
		else
			ae_sound_request(AE_SOUND_FAILURE, 0);
		return;
	}
	snprintf(url, sizeof(url), "https://steamcommunity.com/sharedfiles/filedetails/?id=%llu", id);
	platform_log("ae steam: Workshop page of item %llu opened", id);
	platform_open_url(url);
	ae_sound_request(AE_SOUND_FORWARD, 0);
}

static void leave_picked(short choice, void *context)
{
	(void)context;
	dialog_closed();
	if (choice != 1)
		return;
	platform_log("ae steam: downloads cancelled to leave");
	if (client_live())
		ae_steam_client_cancel_all();
	ae_ui_remove(&S);
}

static void leave_ask(void)
{
	struct ae_dialog_spec spec;
	static char words[160];

	ae_steam_leave_words(&S.status, words, sizeof(words));
	memset(&spec, 0, sizeof(spec));
	spec.kind = AE_DIALOG_CONFIRM;
	spec.title = "DOWNLOADS ARE GOING";
	spec.body = words;
	spec.choices[0] = "WAIT";
	spec.choices[1] = "CANCEL AND LEAVE";
	spec.choice_count = 2;
	spec.safe_choice = 0;
	spec.cancel_choice = 0;
	spec.has_cancel = 1;
	spec.density = S.flow.density;
	spec.bounds = S.flow.frame.rect;
	spec.picked = leave_picked;
	if (ae_dialog_open(&spec, AE_OWNER_ANY))
	{
		S.asking = 1;
		ae_sound_request(AE_SOUND_FORWARD, 0);
	}
}

/* ---------- activating */

static void files_activate(void)
{
	switch (ae_steam_shared_state(&S.status))
	{
	case AE_STEAM_SHARED_DOWNLOAD:
		if (S.status.shared_queued)
		{
			platform_log("ae steam: download cancelled");
			if (client_live())
				ae_steam_client_cancel_shared();
			else
				staged_note();
		}
		else
		{
			platform_log("ae steam: download asked");
			if (client_live())
				ae_steam_client_download_shared();
			else
				staged_note();
		}
		ae_sound_request(AE_SOUND_FORWARD, 0);
		break;
	case AE_STEAM_SHARED_DOWNLOADING:
		platform_log("ae steam: download cancelled");
		if (client_live())
			ae_steam_client_cancel_shared();
		else
			staged_note();
		ae_sound_request(AE_SOUND_BACK, 0);
		break;
	case AE_STEAM_SHARED_READY:
		/* (upstream's rule: the files are downloaded again on a second A) */
		if (!S.armed)
		{
			S.armed = 1;
			ae_sound_request(AE_SOUND_CURSOR, 0);
			break;
		}
		S.armed = 0;
		platform_log("ae steam: download asked (again)");
		if (client_live())
			ae_steam_client_download_shared();
		else
			staged_note();
		ae_sound_request(AE_SOUND_FORWARD, 0);
		break;
	default:
		ae_sound_request(AE_SOUND_FAILURE, 0);
		break;
	}
}

static void activate(short item, int device)
{
	switch (item)
	{
	case IT_ACCOUNT:
		edit_begin(F_ACCOUNT, device);
		break;
	case IT_PASSWORD:
		edit_begin(F_PASSWORD, device);
		break;
	case IT_CODE:
		edit_begin(F_CODE, device);
		break;
	case IT_SEARCH:
		edit_begin(F_SEARCH, device);
		break;
	case IT_SIGN_IN:
		sign_in_submit();
		break;
	case IT_CODE_SIGN_IN:
		code_submit();
		break;
	case IT_CANCEL:
		sign_in_cancel();
		break;
	case IT_CODE_INSTEAD:
		S.code_instead = 1;
		S.focus = IT_CODE;
		ae_sound_request(AE_SOUND_FORWARD, 0);
		break;
	case IT_LOG_OUT:
		logout_ask();
		break;
	case IT_FILES:
		if (S.page != AE_STEAM_PAGE_IN)
			ae_sound_request(AE_SOUND_FAILURE, 0);
		else
			files_activate();
		break;
	case IT_WORKSHOP:
		/* (the Workshop page is MAPS & MODS' WORKSHOP page: this account page steps aside and comes back when that is left with B) */
		if (!S.hub && S.stage < 0 && S.page == AE_STEAM_PAGE_IN && S.status.owns != 0)
		{
			reopen_account = 1;
			ae_screen_steam_close();
			if (!ae_screen_maps_hub_open(AE_HUB_TAB_WORKSHOP))
			{
				reopen_account = 0;
				ae_screen_steam_open("MAPS & MODS");
			}
			break;
		}
		workshop_open();
		break;
	case IT_SHOW:
		type_step(1);
		break;
	case IT_SORT:
		if (coll_active())
		{
			coll_sort_step();
			break;
		}
		if (client_live())
			ae_steam_client_filter_step(0);
		else
			S.status.sort = (S.status.sort + 1) % 4;
		ae_sound_request(AE_SOUND_FORWARD, 0);
		break;
	case IT_COLL:
		coll_accept();
		break;
	case IT_LIST:
		if (S.list.list.focus >= 0)
		{
			if (S.status.item_valid && S.status.item_installed == 1 && !S.status.item_downloading &&
				!S.status.item_queued)
			{
				again_ask();
				break;
			}
			if (client_live())
				ae_steam_client_item_choose(S.list.list.focus);
			else
				staged_note();
			ae_sound_request(AE_SOUND_FORWARD, 0);
		}
		break;
	default:
		break;
	}
}

/* ---------- drawing: the pieces */

static float u(float spec_units)
{
	return ae_flow_u(&S.flow, spec_units);
}

static char const *notice_text(int *warning)
{
	static char text[300];
	int kind = ae_steam_message_kind(S.status.message);

	*warning = 0;
	if (S.page != AE_STEAM_PAGE_OUT || S.workshop)
		return NULL;
	if (kind != AE_STEAM_MESSAGE_CANCELLED && kind != AE_STEAM_MESSAGE_LOGGED_OUT && kind != AE_STEAM_MESSAGE_UNREACHABLE)
		return NULL;
	if (!strcmp(S.dismissed, S.status.message))
		return NULL;
	ae_steam_message_text(kind, S.status.message, text, sizeof(text));
	*warning = kind == AE_STEAM_MESSAGE_UNREACHABLE;
	return text;
}

static char const *help_text(void)
{
	char const *notice;
	int warning;

	if (S.workshop && coll_active())
		return coll_help();
	if (S.stage >= 0 && ae_motion_now() < S.staged_until)
		return "TEST PAGE: nothing is sent to Steam. This press did nothing.";
	notice = notice_text(&warning);
	if (notice)
		return notice;
	switch (S.focus)
	{
	case IT_ACCOUNT:
		return "Your Steam account name: the one you sign in to Steam with, not your email or profile name.";
	case IT_PASSWORD:
		if (ae_steam_message_kind(S.status.message) == AE_STEAM_MESSAGE_WRONG_PASSWORD && !S.fields[F_PASSWORD].text.length)
			return "The password was cleared. Steam signs in with the account name, not with your email address.";
		return "Typed as stars. It goes to Steam only, and is never saved on this PC.";
	case IT_SIGN_IN:
	case IT_CODE_SIGN_IN:
		return "Signs in with Steam. The password goes to Steam only.";
	case IT_CODE:
		return "Type the code Steam sent. It is not sent until you sign in.";
	case IT_CODE_INSTEAD:
		return "Type the code the Steam app shows, if approving there does not work.";
	case IT_CANCEL:
		return "Stops signing in. Nothing is kept.";
	case IT_LOG_OUT:
		if (S.status.owns == 0)
			return "Logs the account out of this PC and ends its sign-in with Steam.";
		return "Logs the account out of this PC and ends its sign-in with Steam.";
	case IT_FILES:
		if (S.page != AE_STEAM_PAGE_IN)
			return "Sign in first. These are the files MCC's maps need, from Halo: CE Anniversary on Steam.";
		switch (ae_steam_shared_state(&S.status))
		{
		case AE_STEAM_SHARED_DOWNLOADING:
			return "A cancels it. While it runs, B asks first: wait for it, or cancel it, to leave this screen.";
		case AE_STEAM_SHARED_READY:
			return S.armed ? "A again downloads them anew." :
				"MCC's shared files are in mcc_maps/_shared. A twice downloads them again.";
		case AE_STEAM_SHARED_NOT_OWNED:
			return "Steam gives these files only to an account that owns Halo: The Master Chief Collection.";
		default:
			return "Downloads the three shared files from Steam into mcc_maps/_shared. A again cancels it.";
		}
	case IT_WORKSHOP:
		return S.page == AE_STEAM_PAGE_IN ? "Search the Halo 1 Workshop and install maps into mcc_maps." :
			"Sign in first. The Workshop's maps need a Steam account that owns MCC.";
	case IT_SEARCH:
		return "A name or word of the Workshop item. An empty search clears it.";
	case IT_SORT:
		return "A changes the order. A search lists by relevance.";
	case IT_LIST:
		return "Installs into mcc_maps/<item>; AE lists it with MCC's maps. The publisher is the Steam name that published it.";
	default:
		return "";
	}
}

/* a row of this screen: drawn at y, its rect kept; returns its height */
static float row_draw(float x, float y, float width, short item, char const *label, char const *value, unsigned int flags,
	char const *reason)
{
	struct ae_row row;
	float height;

	memset(&row, 0, sizeof(row));
	row.label = label;
	row.value = value;
	row.reason = reason;
	row.flags = flags | (S.focus == item ? AE_ROW_FOCUSED : 0);
	height = ae_widget_row(&S.flow.density, x, y, width, &row, HIT_ITEMS, item);
	S.rects[item].x = x;
	S.rects[item].y = y;
	S.rects[item].width = width;
	S.rects[item].height = height;
	return height;
}

/* the SIGN IN button: the selection bar or the row's fill, the START mark (Enter's cap while the password is typed) and
the label centred */
static float button_draw(float x, float y, float width, short item, char const *label, int enter_mark)
{
	struct ae_density const *d = &S.flow.density;
	float height = row_draw(x, y, width, item, "", NULL, 0, NULL);
	float text = ae_size_text(d), center = y + height * 0.5f, mark = ae_size_glyph(d), gap = u(12.0f);
	float mark_width, label_width, left;
	int on_bar = S.focus == item;

	label_width = ae_draw_text_width(AE_FONT_ROW, text, label);
	if (enter_mark)
		mark_width = ae_key_cap_width(d, mark, "ENTER");
	else
		mark_width = ae_widget_button_mark(d, AE_BUTTON_START, 0.0f, center, mark, on_bar, 0);
	left = x + (width - (mark_width + gap + label_width)) * 0.5f;
	if (enter_mark)
		ae_widget_key_cap(d, left, center - mark * 0.5f, mark, "ENTER");
	else
		ae_widget_button_mark(d, AE_BUTTON_START, left, center, mark, on_bar, 1);
	ae_draw_text(AE_FONT_ROW, text, left + mark_width + gap, ae_flow_cap_top(AE_FONT_ROW, text, center), AE_ALIGN_LEFT,
		on_bar ? AE_COLOR_SELECTION_TEXT : AE_COLOR_TITLE, label);
	return height;
}

static float section_label(float x, float y, char const *label)
{
	struct ae_density const *d = &S.flow.density;
	float size = ae_size_minor(d) * 0.9f;

	ae_draw_text_tracked(AE_FONT_BODY, size, 0.09f, x + u(10.0f), y, AE_ALIGN_LEFT, AE_COLOR_MUTED, label);
	return u(38.0f);
}

/* a field: its error line under it when the error is set; returns the height incl. the error's room */
static float field_draw(int field, float x, float y, float width, char const *error)
{
	struct ae_density const *d = &S.flow.density;
	struct field *f = &S.fields[field];
	struct ae_field draw;
	float height;
	int editing = S.typing == field || S.keyboard == field;

	memset(&draw, 0, sizeof(draw));
	draw.label = field_labels[field];
	draw.placeholder = field == F_ACCOUNT ? "Steam account name" : field == F_PASSWORD ? "Steam password" :
		field == F_CODE ? "5 letters and numbers" : "Search the Workshop";
	draw.text = &f->text;
	draw.flags = (S.focus == field_item(field) ? AE_FIELD_FOCUSED : 0) | (editing ? AE_FIELD_EDITING : 0) |
		(error ? AE_FIELD_ERROR : 0) | (field == F_PASSWORD ? AE_FIELD_SECRET : 0);
	draw.error = error;
	draw.caret_since = f->caret_since;
	ae_field_well(d, x, y, width, &draw, &f->well);
	height = ae_widget_field(d, x, y, width, &draw, (short)(HIT_FIELD + field));
	S.rects[field_item(field)] = f->well;
	return height;
}

/* ---------- the card */

struct section { char const *label, *body; };
struct card
{
	char const *title;
	unsigned int stripe, head_color;
	struct section sections[4];
	int section_count;
	int files;                       /* the shared files' three rows, and the bar while they download */
};

static float card_paint(struct card const *card, float x, float y, float width, int draw)
{
	struct ae_density const *d = &S.flow.density;
	float inset = u(23.0f), inner = width - 2.0f * inset, body = ae_size_text(d) * 0.88f, line = u(31.0f);
	float head = u(28.0f), label_size = ae_size_minor(d) * 0.9f, top = y, cy;
	char lines[10][160];
	int index, count, each;

	cy = y + u(24.0f);
	if (draw)
		ae_draw_text(AE_FONT_ROW, head, x + inset, cy, AE_ALIGN_LEFT, card->head_color, card->title);
	cy += ae_draw_cap_height(AE_FONT_ROW, head) + u(22.0f);
	for (index = 0; index < card->section_count; index++)
	{
		struct section const *section = &card->sections[index];

		if (section->label)
		{
			if (draw)
				ae_draw_text_tracked(AE_FONT_BODY, label_size, 0.09f, x + inset, cy, AE_ALIGN_LEFT, AE_COLOR_MUTED,
					section->label);
			cy += ae_draw_cap_height(AE_FONT_BODY, label_size) + u(14.0f);
		}
		count = ae_wrap_text(AE_FONT_BODY, body, section->body, inner, lines, 10);
		for (each = 0; each < count; each++)
			if (draw)
				ae_draw_text(AE_FONT_BODY, body, x + inset, cy + line * (float)each, AE_ALIGN_LEFT, AE_COLOR_TEXT,
					lines[each]);
		cy += line * (float)count + u(20.0f);
	}
	if (card->files)
	{
		int file, state;
		int going = ae_steam_shared_state(&S.status) == AE_STEAM_SHARED_DOWNLOADING;

		for (file = 0; file < AE_STEAM_FILES; file++)
		{
			unsigned int color;
			float pitch = u(44.0f), size = ae_size_text(d) * 0.92f, mid = cy + pitch * 0.5f;

			state = ae_steam_file_state(&S.status, file);
			color = state == AE_STEAM_FILE_MISSING ? AE_COLOR_WARNING : state == AE_STEAM_FILE_DOWNLOADING ? AE_COLOR_ACCENT :
				state == AE_STEAM_FILE_NEXT || state == AE_STEAM_FILE_CHECKING ? AE_COLOR_MUTED : AE_COLOR_TEXT;
			if (draw)
			{
				ae_draw_rect(x + inset, cy, inner, u(1.0f), 0.0f, AE_COLOR_RULE);
				ae_draw_text(AE_FONT_ROW, size, x + inset, ae_flow_cap_top(AE_FONT_ROW, size, mid), AE_ALIGN_LEFT,
					AE_COLOR_TITLE, ae_steam_file_label(file));
				ae_draw_text_tracked(AE_FONT_BODY, label_size, 0.09f, x + inset + inner,
					ae_flow_cap_top(AE_FONT_BODY, label_size, mid), AE_ALIGN_RIGHT, color, ae_steam_file_state_text(state));
			}
			cy += pitch;
		}
		if (draw)
			ae_draw_rect(x + inset, cy, inner, u(1.0f), 0.0f, AE_COLOR_RULE);
		cy += u(18.0f);
		if (going)
		{
			char words[160];
			float fraction = S.status.size ? (float)S.status.done / (float)S.status.size : 0.0f;

			if (fraction > 1.0f)
				fraction = 1.0f;
			if (draw)
			{
				ae_draw_rect(x + inset, cy, inner, u(6.0f), 0.0f, AE_COLOR_RULE);
				ae_draw_rect(x + inset, cy, inner * fraction, u(6.0f), 0.0f, AE_COLOR_ACCENT);
			}
			cy += u(20.0f);
			ae_steam_progress_line(&S.status, words, sizeof(words));
			if (draw)
				ae_draw_text(AE_FONT_BODY, body, x + inset, cy, AE_ALIGN_LEFT, AE_COLOR_TEXT, words);
			cy += line;
		}
	}
	return cy - top + u(3.0f);
}

static void card_draw(struct card const *card, float x, float y, float width)
{
	float height = card_paint(card, x, y, width, 0);

	ae_draw_rect(x, y, width, height, 0.0f, AE_COLOR_PANEL);
	ae_draw_outline(x, y, width, height, 0.0f, u(1.0f), AE_COLOR_RULE);
	ae_draw_rect(x, y, width, u(4.0f), 0.0f, card->stripe);
	card_paint(card, x, y, width, 1);
}

static void card_for_page(struct card *card)
{
	static char signed_as[160], workshop[200];
	char masked[16];

	memset(card, 0, sizeof(*card));
	card->stripe = AE_COLOR_ACCENT;
	card->head_color = AE_COLOR_TITLE;
	switch (S.page)
	{
	case AE_STEAM_PAGE_GUARD_CODE:
		card->title = "STEAM GUARD";
		snprintf(signed_as, sizeof(signed_as), "Steam asks for a second step before it signs you in.");
		card->sections[0].body = signed_as;
		card->sections[1].label = "THE CODE";
		card->sections[1].body = "Steam emailed it to the address on this account, or the Steam app shows it. It works once, for "
			"a few minutes; a new sign-in sends a new one.";
		card->sections[2].label = "NO EMAIL?";
		card->sections[2].body = "Look in the spam folder, or cancel and sign in again for a new code.";
		card->section_count = 3;
		return;
	case AE_STEAM_PAGE_GUARD_APP:
		card->title = "STEAM GUARD";
		card->sections[0].body = "Steam asks for a second step before it signs you in.";
		card->sections[1].label = "IN THE STEAM APP";
		card->sections[1].body = "Open the Steam app and approve 'Arena Evolved' in Steam Guard. If Steam asks about a sign-in you did not "
			"start, deny it. This screen goes on by itself once you approve.";
		card->sections[2].label = "NO PHONE AT HAND?";
		card->sections[2].body = "ENTER A CODE INSTEAD takes the code from the app's Steam Guard page.";
		card->section_count = 3;
		return;
	case AE_STEAM_PAGE_IN:
		if (S.focus == IT_LOG_OUT)
		{
			card->title = "STEAM ACCOUNT";
			ae_steam_account_masked(S.status.account, masked, sizeof(masked));
			snprintf(signed_as, sizeof(signed_as), "Signed in as %s. %s", masked[0] ? masked : "this account",
				ae_steam_store_text(system_kind(), ae_steam_client_store()));
			card->sections[0].body = signed_as;
			card->section_count = 1;
			return;
		}
		if (S.status.owns == 0 && S.focus != IT_WORKSHOP)
		{
			card->title = "THIS ACCOUNT DOES NOT OWN MCC";
			card->stripe = AE_COLOR_WARNING;
			card->head_color = AE_COLOR_WARNING;
			ae_steam_account_masked(S.status.account, masked, sizeof(masked));
			snprintf(signed_as, sizeof(signed_as), "Steam gives these files only to an account that owns Halo: The Master "
				"Chief Collection with Halo: Combat Evolved Anniversary. %s does not.",
				masked[0] ? masked : "This account");
			card->sections[0].body = signed_as;
			card->sections[1].label = "WHAT TO DO";
			card->sections[1].body = "Log out, then sign in with the account that owns them. Maps you already have keep working.";
			card->section_count = 2;
			return;
		}
		if (S.focus == IT_WORKSHOP)
		{
			card->title = "WORKSHOP MAPS";
			snprintf(workshop, sizeof(workshop), "Halo 1 maps from the Steam Workshop, installed into mcc_maps. %d installed.",
				S.status.packages);
			card->sections[0].body = workshop;
			card->section_count = 1;
			return;
		}
		card->title = "MCC FILE STATUS";
		card->sections[0].body = ae_steam_shared_state(&S.status) == AE_STEAM_SHARED_READY ?
			"All three are here. Maps that need them play now; Workshop maps install with WORKSHOP MAPS." :
			"Resources many MCC and Workshop maps need, from Halo: CE Anniversary on Steam. They go to mcc_maps/_shared.";
		card->section_count = 1;
		card->files = 1;
		return;
	default:
		break;
	}
	card->title = "SIGN IN WITH STEAM";
	card->sections[0].label = "WHAT IT DOES";
	card->sections[0].body = "Downloads MCC's shared map files and Halo 1 Workshop maps into mcc_maps, for a PC without MCC "
		"installed.";
	card->sections[1].label = "WHAT YOU NEED";
	card->sections[1].body = "A Steam account that owns Halo: The Master Chief Collection and Halo: Combat Evolved Anniversary.";
	card->sections[2].label = "YOUR PASSWORD";
	card->sections[2].body = "Goes to Steam only, encrypted with Steam's own key. Arena Evolved never saves it.";
	card->sections[3].label = "STAYING SIGNED IN";
	card->sections[3].body = ae_steam_store_text(system_kind(), ae_steam_client_store());
	card->section_count = 4;
}

/* ---------- the pages */

static void draw_account_left(float x, float y, float width)
{
	struct ae_density const *d = &S.flow.density;
	float gap = u(d->metrics->gap), height;
	char value[64], reason[64];
	char const *error = NULL;
	int kind = ae_steam_message_kind(S.status.message);
	unsigned int flags;
	int files_state = ae_steam_shared_state(&S.status);

	switch (S.page)
	{
	case AE_STEAM_PAGE_OUT:
	case AE_STEAM_PAGE_SIGNING_IN:
	{
		int signing = S.page == AE_STEAM_PAGE_SIGNING_IN;
		static char wrong[160];

		y += field_draw(F_ACCOUNT, x, y, width, NULL) + u(16.0f);
		if (kind == AE_STEAM_MESSAGE_WRONG_PASSWORD && !S.fields[F_PASSWORD].text.length && !signing)
		{
			ae_steam_message_text(kind, S.status.message, wrong, sizeof(wrong));
			error = wrong;
		}
		y += field_draw(F_PASSWORD, x, y, width, error) + u(16.0f);
		if (signing)
		{
			height = row_draw(x, y, width, IT_CANCEL, ae_steam_checking_text(&S.status) ? ae_steam_checking_text(&S.status) :
				"SIGNING IN...", NULL, 0, NULL);
			ae_draw_text(AE_FONT_BODY, ae_size_minor(d), x + width - u(20.0f),
				ae_flow_cap_top(AE_FONT_BODY, ae_size_minor(d), y + height * 0.5f), AE_ALIGN_RIGHT,
				S.focus == IT_CANCEL ? AE_COLOR_SELECTION_MUTED : AE_COLOR_MUTED, "B CANCELS");
			y += height + u(24.0f);
		}
		else
			y += button_draw(x, y, width, IT_SIGN_IN, "SIGN IN", S.typing == F_PASSWORD) + u(24.0f);
		y += section_label(x, y, "MCC FILES");
		/* (the rows exist, and tell why they do nothing) */
		y += row_draw(x, y, width, IT_FILES, "SHARED MAP FILES", NULL, AE_ROW_DISABLED, "Sign in first") + gap;
		row_draw(x, y, width, IT_WORKSHOP, "WORKSHOP MAPS", NULL, AE_ROW_DISABLED, "Sign in first");
		break;
	}
	case AE_STEAM_PAGE_GUARD_CODE:
	case AE_STEAM_PAGE_GUARD_APP:
	{
		float head = u(32.0f), sub = ae_size_text(d) * 0.88f, clock_size = ae_size_minor(d) * 0.9f;
		unsigned long seconds = (ae_motion_now() - S.page_since) / 1000ul;
		char clock[16];

		snprintf(clock, sizeof(clock), "%lu:%02lu", seconds / 60ul, seconds % 60ul);
		ae_draw_text(AE_FONT_ROW, head, x, ae_flow_cap_top(AE_FONT_ROW, head, y + u(12.0f) + u(16.0f)), AE_ALIGN_LEFT,
			AE_COLOR_TITLE, S.page == AE_STEAM_PAGE_GUARD_CODE ? "STEAM GUARD CODE" : "APPROVE IN THE STEAM APP");
		ae_draw_text_tracked(AE_FONT_BODY, clock_size, 0.09f, x + width, y + u(16.0f), AE_ALIGN_RIGHT, AE_COLOR_MUTED, clock);
		ae_draw_text(AE_FONT_BODY, sub, x, y + u(56.0f), AE_ALIGN_LEFT, AE_COLOR_TEXT,
			S.page == AE_STEAM_PAGE_GUARD_CODE ? "Type the code Steam sent: emailed, or from the Steam app." :
			"Approve 'Arena Evolved' in the Steam app on your phone.");
		if (S.page == AE_STEAM_PAGE_GUARD_CODE)
		{
			static char wrong[160];

			if (kind == AE_STEAM_MESSAGE_WRONG_CODE && !S.fields[F_CODE].text.length)
			{
				ae_steam_message_text(kind, S.status.message, wrong, sizeof(wrong));
				error = wrong;
			}
			y += u(96.0f);
			y += field_draw(F_CODE, x, y, width, error) + u(16.0f);
			y += button_draw(x, y, width, IT_CODE_SIGN_IN, "SIGN IN", S.typing == F_CODE) + gap;
			row_draw(x, y, width, IT_CANCEL, "CANCEL SIGN-IN", NULL, 0, NULL);
		}
		else
		{
			float track = y + u(85.0f), run = width * 0.30f, offset;
			unsigned long period = 1400ul, phase = ae_motion_now() % period;

			ae_draw_rect(x, track, width, u(6.0f), 0.0f, AE_COLOR_RULE);
			if (ae_motion_reduced())
			{
				/* (no run: the track pulses .6 to 1 in 1.2 s steps) */
				float alpha = ((ae_motion_now() / 1200ul) & 1ul) ? 1.0f : 0.6f;

				ae_draw_set_alpha(alpha);
				ae_draw_rect(x, track, width, u(6.0f), 0.0f, AE_COLOR_RULE);
				ae_draw_set_alpha(1.0f);
			}
			else
			{
				offset = (width + run) * (float)phase / (float)period - run;
				ae_draw_clip_push(x, track, width, u(6.0f));
				ae_draw_rect(x + offset, track, run, u(6.0f), 0.0f, AE_COLOR_ACCENT);
				ae_draw_clip_pop();
			}
			y += u(130.0f);
			if (ae_steam_code_instead_possible(&S.status))
				y += row_draw(x, y, width, IT_CODE_INSTEAD, "ENTER A CODE INSTEAD", NULL, 0, NULL) + gap;
			row_draw(x, y, width, IT_CANCEL, "CANCEL SIGN-IN", NULL, 0, NULL);
		}
		break;
	}
	default:
	{
		struct ae_row row;
		char name[96];

		y += section_label(x, y, "ACCOUNT");
		/* (a status: who is signed in, nothing to do on it) */
		memset(&row, 0, sizeof(row));
		ae_steam_account_masked(S.status.account, name, sizeof(name));
		if (!name[0])
			snprintf(name, sizeof(name), "SIGNED IN");
		row.label = "STEAM ACCOUNT";
		row.value = name;
		row.flags = AE_ROW_STATUS;
		y += ae_widget_row(d, x, y, width, &row, HIT_ROWS, 0) + gap;
		y += row_draw(x, y, width, IT_LOG_OUT, "LOG OUT", NULL, 0, NULL) + u(24.0f);
		y += section_label(x, y, "MCC FILES");
		ae_steam_shared_value(&S.status, value, sizeof(value));
		flags = files_state == AE_STEAM_SHARED_DOWNLOAD && !S.status.shared_queued ? AE_ROW_LINK : AE_ROW_STATUS;
		reason[0] = 0;
		if (files_state == AE_STEAM_SHARED_NOT_OWNED)
		{
			snprintf(reason, sizeof(reason), "NOT OWNED");
			flags = AE_ROW_DISABLED;
		}
		if (S.armed)
			snprintf(value, sizeof(value), "DOWNLOAD AGAIN?");
		y += row_draw(x, y, width, IT_FILES, "SHARED MAP FILES", reason[0] ? NULL : value, flags,
			reason[0] ? reason : NULL) + gap;
		snprintf(value, sizeof(value), "%d INSTALLED", S.status.packages);
		if (S.status.owns == 0)
			row_draw(x, y, width, IT_WORKSHOP, "WORKSHOP MAPS", NULL, AE_ROW_DISABLED, "Needs MCC on this account");
		else
			row_draw(x, y, width, IT_WORKSHOP, "WORKSHOP MAPS", value, AE_ROW_SUBSCREEN, NULL);
		break;
	}
	}
}

struct list_context { float width; };

static void list_item(void *context, short item, float x, float y, float width, float height, unsigned int flags)
{
	struct ae_row row;
	char name[96], state[48];

	(void)context;
	(void)height;
	item_name_state(item, name, (int)sizeof(name), state, (int)sizeof(state));
	memset(&row, 0, sizeof(row));
	row.label = name;
	row.value = state[0] ? state : NULL;
	row.flags = flags | (state[0] ? AE_ROW_STATUS : 0);
	ae_widget_row(&S.flow.density, x, y, width, &row, HIT_LIST, item);
}

static void draw_workshop_left(float x, float y, float width)
{
	struct ae_density const *d = &S.flow.density;
	struct ae_chip chips[TYPES];
	struct ae_rect rects[TYPES];
	float glyph = ae_size_glyph(d), chips_x, chips_height, minor = ae_size_minor(d) * 0.9f;
	char label[48], count[48], value[48];
	long pages;
	int index;

	if (S.hub && coll_active() && coll_level() == 2)
	{
		coll_draw_open(x, y, width);
		return;
	}
	if (S.hub)
	{
		/* (LB / RB are the hub's tabs: the type is a SHOW row, left and right) */
		y += field_draw(F_SEARCH, x, y, width, NULL) + u(18.0f);
		y += row_draw(x, y, width, IT_SHOW, "SHOW", coll_active() ? "COLLECTIONS" : type_names[S.status.type % TYPES], 0, NULL) + u(6.0f);
		snprintf(value, sizeof(value), "%s", coll_active() ? coll_sort_text() : S.status.search[0] ? "RELEVANCE" : sort_names[S.status.sort & 3]);
		y += row_draw(x, y, width, IT_SORT, "SORT", value, AE_ROW_LINK, NULL) + u(16.0f);
	}
	else
	{
	y += field_draw(F_SEARCH, x, y, width, NULL) + u(26.0f);
	/* the TYPE chips with the LB / RB marks */
	for (index = 0; index < TYPES; index++)
	{
		chips[index].label = type_names[index];
		chips[index].count = -1;
		chips[index].flags = index == S.status.type ? AE_CHIP_ON : 0;
	}
	chips_x = x + ae_draw_button_width(AE_FONT_LAST_DEVICE, AE_BUTTON_LEFT_SHOULDER, glyph) + u(10.0f);
	chips_height = ae_chips_layout(d, width - (chips_x - x) - u(60.0f), chips, TYPES, rects);
	ae_widget_chip_key(AE_FONT_LAST_DEVICE, AE_BUTTON_LEFT_SHOULDER, glyph, x, y + rects[0].height * 0.5f);
	ae_widget_chips(d, chips_x, y, width, chips, TYPES, HIT_CHIPS);
	ae_widget_chip_key(AE_FONT_LAST_DEVICE, AE_BUTTON_RIGHT_SHOULDER, glyph,
		chips_x + rects[TYPES - 1].x + rects[TYPES - 1].width + u(10.0f), y + rects[TYPES - 1].y + rects[TYPES - 1].height * 0.5f);
	for (index = 0; index < TYPES; index++)
		S.type_chips[index] = rects[index];
	y += chips_height + u(16.0f);
	snprintf(value, sizeof(value), "%s", S.status.search[0] ? "RELEVANCE" : sort_names[S.status.sort & 3]);
	y += row_draw(x, y, width, IT_SORT, "SORT", value, AE_ROW_LINK, NULL) + u(24.0f);
	}
	if (coll_active())
	{
		coll_draw_list(x, y, width);
		return;
	}
	/* PAGE n OF m, the items' count on the right */
	pages = (long)((S.status.total + 19ul) / 20ul);
	if (pages < 1)
		pages = 1;
	snprintf(label, sizeof(label), "PAGE %d OF %ld", S.status.page > 0 ? S.status.page : 1, pages);
	if (S.status.loading)
		snprintf(count, sizeof(count), "LOADING...");
	else
	{
		char number[32];

		ae_steam_count_text(S.status.total, number, sizeof(number));
		snprintf(count, sizeof(count), "%s ITEMS", number);
	}
	ae_draw_text_tracked(AE_FONT_BODY, minor, 0.09f, x + u(10.0f), y, AE_ALIGN_LEFT, AE_COLOR_MUTED, label);
	ae_draw_text_tracked(AE_FONT_BODY, minor, 0.09f, x + width - u(10.0f), y, AE_ALIGN_RIGHT, AE_COLOR_MUTED, count);
	y += u(38.0f);
	list_sync();
	if (S.status.item_count > 0)
	{
		struct list_context context;
		float bottom = ae_flow_content_bottom(&S.flow) - u(96.0f);

		context.width = width;
		ae_widget_list(d, &S.list, x, y, width, bottom - y, list_item, &context, HIT_LIST);
		S.rects[IT_LIST].x = x;
		S.rects[IT_LIST].y = y;
		S.rects[IT_LIST].width = width;
		S.rects[IT_LIST].height = bottom - y;
	}
	else
		ae_draw_text(AE_FONT_BODY, ae_size_text(d) * 0.9f, x + u(10.0f), y + u(8.0f), AE_ALIGN_LEFT, AE_COLOR_MUTED,
			S.status.loading ? "Asking Steam..." : "Nothing found.");
}

/* ---------- the focused Workshop item's picture: the client's square (decoded off this thread, copied into its frame
snapshot), made an image here once per item; one image at a time, let go when the item has none or the screen goes. A
staged page shows a made-up picture and asks nothing. */

/* (a staged page's picture: no item's) */
#define STAGED_PICTURE_KEY ((unsigned long long)1)

static struct
{
	unsigned long long key;  /* the item the image is of (0 none) */
	int image;               /* 0: none, or none could be made for key */
} picture;
static unsigned int picture_pixels[AE_STEAM_PICTURE_SIDE * AE_STEAM_PICTURE_SIDE];
static unsigned char picture_rgba[AE_STEAM_PICTURE_SIDE * AE_STEAM_PICTURE_SIDE * 4];

static void picture_release(void)
{
	ae_draw_image_release(picture.image);
	picture.image = 0;
	picture.key = 0;
}

/* (a staged page's: a wide picture letterboxed on black, as the client fits one, in the theme's row and accent) */
static void picture_staged(unsigned int *pixels)
{
	int x, y, side = AE_STEAM_PICTURE_SIDE, top = (AE_STEAM_PICTURE_SIDE - AE_STEAM_PICTURE_SIDE * 9 / 16) / 2;

	for (y = 0; y < side; y++)
		for (x = 0; x < side; x++)
		{
			unsigned int mix = (unsigned int)(x + y) * 255u / (unsigned int)(2 * side - 2), red, green, blue;
			int edge;

			red = (32u * (255u - mix) + 52u * mix) / 255u;
			green = (40u * (255u - mix) + 110u * mix) / 255u;
			blue = (48u * (255u - mix) + 130u * mix) / 255u;
			edge = x < 4 || x >= side - 4 || y < top + 4 || y >= side - top - 4 || (x >= side / 2 - 2 && x < side / 2 + 2) ||
				(y >= side / 2 - 2 && y < side / 2 + 2);

			if (edge)
				red = 232u, green = 238u, blue = 242u;
			pixels[y * side + x] = y < top || y >= side - top ? 0xFF000000u : 0xFF000000u | red << 16 | green << 8 | blue;
		}
}

/* whether every pixel of a row of the converted picture is opaque black (a letterbox band) */
static int picture_row_black(int row)
{
	int column;

	for (column = 0; column < AE_STEAM_PICTURE_SIDE; column++)
	{
		unsigned char const *pixel = picture_rgba + 4 * (row * AE_STEAM_PICTURE_SIDE + column);

		if (pixel[0] || pixel[1] || pixel[2] || pixel[3] != 255)
			return 0;
	}
	return 1;
}

/* the focused item's picture as an image, or 0 (the plain box) */
static int picture_image(void)
{
	unsigned long long key = 0;
	int side = AE_STEAM_PICTURE_SIDE, index;

	if (client_live())
		side = ae_steam_client_picture(&key, NULL);
	else if (S.status.item_valid)
		key = STAGED_PICTURE_KEY;
	if (!key || side != AE_STEAM_PICTURE_SIDE)
	{
		if (picture.key)
			picture_release();
		return 0;
	}
	if (key == picture.key && (!picture.image || ae_draw_image_live(picture.image)))
		return picture.image;
	picture_release();
	if (client_live())
	{
		if (ae_steam_client_picture(&key, picture_pixels) != AE_STEAM_PICTURE_SIDE || !key)
			return 0;
	}
	else
		picture_staged(picture_pixels);
	for (index = 0; index < AE_STEAM_PICTURE_SIDE * AE_STEAM_PICTURE_SIDE; index++)
	{
		unsigned int pixel = picture_pixels[index];

		picture_rgba[4 * index] = (unsigned char)(pixel >> 16);
		picture_rgba[4 * index + 1] = (unsigned char)(pixel >> 8);
		picture_rgba[4 * index + 2] = (unsigned char)pixel;
		picture_rgba[4 * index + 3] = (unsigned char)(pixel >> 24);
	}
	picture.key = key;
	/* (a wide picture comes letterboxed on black in the square: its black bands cut off, so the picture alone is
	the image and fills the 16:9 frame; a square one has no bands and shows whole) */
	{
		int above = 0, below = 0;

		while (above < side / 2 && picture_row_black(above))
			above++;
		while (below < side / 2 && picture_row_black(side - 1 - below))
			below++;
		if (above >= 4 && below >= 4)
			picture.image = ae_draw_image_load(picture_rgba + 4 * side * above, side, side - above - below);
		else
			picture.image = ae_draw_image_load(picture_rgba, side, side);
	}
	return picture.image;
}

/* a label / value line of the card: the label tracked and muted on the left, the value on the right (cut with "..." when
it does not fit, never clipped), a rule under it */
static float card_fact(float x, float y, float width, char const *label, char const *value)
{
	struct ae_density const *d = &S.flow.density;
	float inset = u(23.0f), label_size = ae_size_minor(d) * 0.84f, value_size = ae_size_text(d) * 0.83f;
	float label_width = ae_draw_text_tracked_width(AE_FONT_BODY, label_size, 0.09f, label);
	char fitted[160];

	ae_draw_text_tracked(AE_FONT_BODY, label_size, 0.09f, x + inset, y + u(4.0f), AE_ALIGN_LEFT, AE_COLOR_MUTED, label);
	ae_fit_text(AE_FONT_ROW, value_size, value, width - 2.0f * inset - label_width - u(20.0f), fitted, sizeof(fitted));
	ae_draw_text(AE_FONT_ROW, value_size, x + width - inset, y, AE_ALIGN_RIGHT, AE_COLOR_TITLE, fitted);
	ae_draw_rect(x + inset, y + u(31.0f), width - 2.0f * inset, u(1.0f), 0.0f, AE_COLOR_RULE);
	return u(40.0f);
}

/* the focused item's card: its picture across the card's inside (in the 16:9 frame, at most 260 u tall), its name, then label / value lines under it, so a narrower card only shortens the gap between label and value */
static void draw_workshop_card(float x, float y, float width, float height)
{
	struct ae_density const *d = &S.flow.density;
	float inset = u(23.0f), picture_w = width - 2.0f * inset, picture_h = u(260.0f), line, py, picture_x;
	float body = ae_size_text(d) * 0.88f;
	char lines[6][160], number[32], size[32], title[100];
	int count, each, image;
	char const *type;

	ae_draw_rect(x, y, width, height, 0.0f, AE_COLOR_PANEL);
	ae_draw_outline(x, y, width, height, 0.0f, u(1.0f), AE_COLOR_RULE);
	ae_draw_rect(x, y, width, u(4.0f), 0.0f, AE_COLOR_ACCENT);
	if (!S.status.item_valid)
	{
		ae_draw_text(AE_FONT_BODY, body, x + inset, y + u(30.0f), AE_ALIGN_LEFT, AE_COLOR_MUTED,
			S.status.loading ? "Asking Steam..." : "Pick a map to see it.");
		return;
	}
	/* the item's picture, whole, in its middle; the plain box until it is there (never a broken picture) */
	py = y + u(24.0f);
	/* (every map's picture in the same 16:9 frame; one too tall for the card is narrowed to it) */
	if (ae_cover_frame_height(picture_w) > picture_h)
		picture_w = picture_h * 16.0f / 9.0f;
	picture_h = ae_cover_frame_height(picture_w);
	/* (centred in the card's inside) */
	picture_x = x + inset + (width - 2.0f * inset - picture_w) * 0.5f;
	ae_draw_rect(picture_x, py, picture_w, picture_h, 0.0f, AE_COLOR_ROW);
	image = picture_image();
	if (image)
		ae_draw_image_fill(image, picture_x, py, picture_w, picture_h, 0xFFFFFFFFu);
	py += picture_h + u(20.0f);
	snprintf(title, sizeof(title), "%s", S.status.item_title);
	for (each = 0; title[each]; each++)
		if (title[each] >= 'a' && title[each] <= 'z')
			title[each] = (char)(title[each] - 'a' + 'A');
	{
		char fitted[100];

		ae_fit_text(AE_FONT_TITLE, u(40.0f), title, width - 2.0f * inset, fitted, sizeof(fitted));
		ae_draw_text(AE_FONT_TITLE, u(40.0f), x + inset, py, AE_ALIGN_LEFT, AE_COLOR_TITLE, fitted);
	}
	py += u(62.0f);
	ae_steam_size_text(S.status.item_size, size, sizeof(size));
	ae_steam_count_text(S.status.item_subscribers, number, sizeof(number));
	type = ae_steam_item_type(S.status.item_tags);
	py += card_fact(x, py, width, "STATE", ae_steam_item_state_text(&S.status));
	py += card_fact(x, py, width, "SIZE", size);
	py += card_fact(x, py, width, "SUBSCRIBERS", number);
	if (type[0])
		py += card_fact(x, py, width, "TYPE", type);
	py += card_fact(x, py, width, "PUBLISHER", S.status.item_author[0] ? S.status.item_author : "Asking Steam...");
	py += u(8.0f);
	line = u(31.0f);
	count = ae_wrap_text(AE_FONT_BODY, body, S.status.item_description, width - 2.0f * inset, lines, 2);
	for (each = 0; each < count && each < 2 && py + line * (float)each < y + height - u(20.0f); each++)
		ae_draw_text(AE_FONT_BODY, body, x + inset, py + line * (float)each, AE_ALIGN_LEFT, AE_COLOR_TEXT, lines[each]);
}

/* ---------- SHOW: COLLECTIONS */

#include "ae_screen_steam_collections.inc" /* AE hook */

/* ---------- the screen */

static void prompts_build(void)
{
	struct ae_flow_screen *flow = &S.flow;
	int signing_page = !S.workshop && (S.page == AE_STEAM_PAGE_OUT || S.page == AE_STEAM_PAGE_GUARD_CODE);

	flow->prompt_count = 0;
	if (S.typing >= 0)
	{
		flow->prompts[flow->prompt_count++] = ae_flow_prompt(AE_BUTTON_A, S.typing == F_ACCOUNT ? "Next" :
			S.typing == F_SEARCH ? "Search" : "Sign in", AE_ACTION_ACCEPT);
		flow->prompts[flow->prompt_count++] = ae_flow_prompt(AE_BUTTON_B, "Stop typing", AE_ACTION_BACK);
		return;
	}
	if (S.workshop && coll_active())
	{
		coll_prompts();
		return;
	}
	if (S.workshop)
	{
		flow->prompts[flow->prompt_count++] = ae_flow_prompt(AE_BUTTON_A, S.status.item_downloading ? "Cancel" :
			S.status.item_queued ? "Leave queue" : S.status.item_installed == 2 ? "Update" :
			S.status.item_installed == 1 ? "Options" : "Install", AE_ACTION_ACCEPT);
		flow->prompts[flow->prompt_count++] = ae_flow_prompt(AE_BUTTON_X, "Search", AE_ACTION_X);
		if (S.status.item_valid)
			flow->prompts[flow->prompt_count++] = ae_flow_prompt(AE_BUTTON_Y, "Steam page", AE_ACTION_Y);
		flow->prompts[flow->prompt_count++] = ae_flow_prompt(AE_BUTTON_LEFT_TRIGGER, "Page", AE_ACTION_PAGE_UP);
		flow->prompts[flow->prompt_count - 1].pair = 1;
		if (S.hub)
		{
			flow->prompts[flow->prompt_count++] = ae_flow_prompt(AE_BUTTON_LEFT_SHOULDER, "Pages", AE_ACTION_TAB_PREVIOUS);
			flow->prompts[flow->prompt_count - 1].pair = 1;
		}
		flow->prompts[flow->prompt_count++] = ae_flow_prompt(AE_BUTTON_B, ae_string(AE_STR_BACK), AE_ACTION_BACK);
		return;
	}
	switch (S.focus)
	{
	case IT_ACCOUNT:
	case IT_PASSWORD:
	case IT_CODE:
		flow->prompts[flow->prompt_count++] = ae_flow_prompt(AE_BUTTON_A, "Type", AE_ACTION_ACCEPT);
		break;
	case IT_SIGN_IN:
	case IT_CODE_SIGN_IN:
		flow->prompts[flow->prompt_count++] = ae_flow_prompt(AE_BUTTON_A, "Sign in", AE_ACTION_ACCEPT);
		break;
	case IT_CANCEL:
	case IT_CODE_INSTEAD:
		flow->prompts[flow->prompt_count++] = ae_flow_prompt(AE_BUTTON_A, "Select", AE_ACTION_ACCEPT);
		break;
	case IT_LOG_OUT:
		flow->prompts[flow->prompt_count++] = ae_flow_prompt(AE_BUTTON_A, "Log out", AE_ACTION_ACCEPT);
		break;
	case IT_FILES:
		if (S.page == AE_STEAM_PAGE_IN)
		{
			switch (ae_steam_shared_state(&S.status))
			{
			case AE_STEAM_SHARED_DOWNLOAD:
				flow->prompts[flow->prompt_count++] = ae_flow_prompt(AE_BUTTON_A, S.status.shared_queued ? "Cancel" :
					"Download", AE_ACTION_ACCEPT);
				break;
			case AE_STEAM_SHARED_DOWNLOADING:
				flow->prompts[flow->prompt_count++] = ae_flow_prompt(AE_BUTTON_A, "Cancel download", AE_ACTION_ACCEPT);
				break;
			default:
				break;
			}
		}
		break;
	case IT_WORKSHOP:
		if (S.page == AE_STEAM_PAGE_IN && S.status.owns != 0)
			flow->prompts[flow->prompt_count++] = ae_flow_prompt(AE_BUTTON_A, "Open", AE_ACTION_ACCEPT);
		break;
	default:
		break;
	}
	if (signing_page)
		flow->prompts[flow->prompt_count++] = ae_flow_prompt(AE_BUTTON_START, "Sign in", AE_ACTION_START);
	flow->prompts[flow->prompt_count++] = ae_flow_prompt(AE_BUTTON_B, S.page == AE_STEAM_PAGE_OUT ||
		S.page == AE_STEAM_PAGE_IN ? ae_string(AE_STR_BACK) : "Cancel sign-in", AE_ACTION_BACK);
}

static void steam_draw(struct ae_screen *instance)
{
	struct ae_flow_screen *flow = &S.flow;
	struct ae_density const *d;
	struct card card;
	float x, width, top, card_x, card_width, bottom, line, body;
	char expanded[320], lines[2][160];
	char const *help, *notice;
	int warning = 0, count, index;

	(void)instance;
	ae_flow_full_density(flow);
	d = &flow->density;
	x = ae_flow_left_x(flow);
	width = flow->frame.rect.width * AE_FLOW_ROWS_WIDTH;
	card_x = ae_frame_x(&flow->frame, AE_FLOW_PANEL_X);
	card_width = ae_flow_right_x(flow) - card_x;
	top = ae_frame_y(&flow->frame, S.hub ? 252.0f / 1080.0f : AE_FLOW_CONTENT_Y);
	ae_flow_title(flow, S.hub ? "MAPS & MODS" : S.workshop ? "WORKSHOP MAPS" : "STEAM ACCOUNT", S.hub ? "MAIN MENU" : S.stage >= 0 ?
		"TEST PAGE: NOTHING IS SENT TO STEAM" : S.workshop ? "MCC \xE2\x80\xA2 STEAM \xE2\x80\xA2 THIS PC" :
		"SYSTEM \xE2\x80\xA2 MCC \xE2\x80\xA2 THIS PC");
	if (S.hub)
		ae_screen_maps_hub_strip(flow, AE_HUB_TAB_WORKSHOP);
	if (!focus_valid())
		S.focus = default_focus();
	memset(S.rects, 0, sizeof(S.rects));
	if (S.workshop)
		draw_workshop_left(x, top, width);
	else
		draw_account_left(x, top, width);
	if (S.workshop && coll_active())
		coll_draw_card(card_x, top, card_width, ae_flow_content_bottom(flow) - u(96.0f) - top);
	else if (S.workshop)
		draw_workshop_card(card_x, top, card_width, ae_flow_content_bottom(flow) - u(96.0f) - top);
	else
	{
		card_for_page(&card);
		card_draw(&card, card_x, top, card_width);
	}
	/* the help line over the footer */
	bottom = ae_flow_content_bottom(flow);
	body = ae_flow_u(flow, d->metrics->body);
	line = ae_flow_u(flow, d->metrics->body_line);
	help = help_text();
	notice = notice_text(&warning);
	ae_draw_rect(x, bottom - line * 2.0f - ae_flow_u(flow, 14.0f), ae_flow_right_x(flow) - x, ae_flow_u(flow, AE_FLOW_RULE_U),
		0.0f, AE_COLOR_RULE);
	count = ae_wrap_text(AE_FONT_BODY, body, ae_help_expand(help, expanded, sizeof(expanded)), ae_flow_right_x(flow) - x,
		lines, 2);
	for (index = 0; index < count; index++)
		ae_draw_text(AE_FONT_BODY, body, x, bottom - line * 2.0f + line * (float)index, AE_ALIGN_LEFT,
			(notice && warning) || (S.stage >= 0 && ae_motion_now() < S.staged_until) ? AE_COLOR_WARNING :
				AE_COLOR_TEXT, lines[index]);
	prompts_build();
	ae_flow_draw_prompts(flow);
}

/* ---------- the frame's update and the log */

static void log_changes(void)
{
	int shared = ae_steam_shared_state(&S.status), kind = ae_steam_message_kind(S.status.message);

	if (S.page != S.last_page)
	{
		if (S.page != S.logged_page)
			platform_log("ae steam: page %s", page_name(S.page));
		S.logged_page = S.page;
		S.last_page = S.page;
		S.page_since = ae_motion_now();
		if (S.status.session != AE_STEAM_SESSION_GUARD)
			S.code_instead = 0;
		S.focus = default_focus();
		S.armed = 0;
		/* (a Workshop page does not outlive the sign-in, nor does its picture) */
		if (S.page != AE_STEAM_PAGE_IN)
		{
			S.workshop = 0;
			picture_release();
			coll_pictures_release();
		}
		if (S.page == AE_STEAM_PAGE_GUARD_CODE)
			field_wipe(F_CODE);
	}
	if (S.page == AE_STEAM_PAGE_IN && shared != S.logged_shared)
	{
		static char const *const names[] = { "checking", "download", "downloading", "ready", "not owned" };

		platform_log("ae steam: files %s", names[shared]);
		S.logged_shared = shared;
		S.logged_decile = -1;
	}
	if (shared == AE_STEAM_SHARED_DOWNLOADING && ae_steam_percent(&S.status) / 10 != S.logged_decile)
	{
		S.logged_decile = ae_steam_percent(&S.status) / 10;
		platform_log("ae steam: download %d %%", S.logged_decile * 10);
	}
	if (kind != S.logged_kind)
	{
		static char const *const names[] = { "none", "wrong password", "wrong code", "cancelled", "logged out", "unreachable" };

		S.logged_kind = kind;
		if (kind != AE_STEAM_MESSAGE_NONE)
			platform_log("ae steam: message %s", names[kind]);
	}
}

static void steam_update(struct ae_screen *instance)
{
	(void)instance;
	if (S.typing >= 0 && S.fields[S.typing].edit.active && !ae_field_edit_keys(&S.fields[S.typing].edit))
		ae_sound_request(AE_SOUND_FAILURE, 0);
	status_fetch();
	S.page = ae_steam_page(&S.status, S.code_instead);
	log_changes();
	/* (a download of the Workshop page's list from the client) */
	if (S.workshop && S.page != AE_STEAM_PAGE_IN)
	{
		S.workshop = 0;
		picture_release();
		coll_pictures_release();
		/* (the hub's WORKSHOP page does not outlive the sign-in: the hub shows its signed-out card) */
		if (S.hub)
			(void)ae_ui_remove(&S);
	}
	/* (the first frame's asks wait for the page's first layout: a dialog needs its text sizes) */
	if (S.first_frame && S.flow.density.metrics)
	{
		S.first_frame = 0;
		if (S.pending_typing >= 0)
		{
			S.focus = field_item(S.pending_typing);
			typing_begin(S.pending_typing);
		}
		if (S.pending_logout)
		{
			S.focus = IT_LOG_OUT;
			logout_ask();
		}
	}
	coll_update();
}

/* ---------- input */

static int back_pressed(void)
{
	if (S.hub && coll_active() && coll_level() == 2)
	{
		coll_back();
		return 1;
	}
	if (S.hub)
	{
		/* (B leaves MAPS & MODS: this page, then the hub under it; STEAM ACCOUNT again when it was the account page that asked) */
		int again = reopen_account;

		ae_ui_pop();
		ae_ui_pop();
		if (again)
		{
			reopen_account = 0;
			ae_screen_steam_open("MAPS & MODS");
		}
		return 1;
	}
	if (S.workshop)
	{
		workshop_leave();
		return 1;
	}
	if (S.page == AE_STEAM_PAGE_SIGNING_IN || S.page == AE_STEAM_PAGE_GUARD_CODE || S.page == AE_STEAM_PAGE_GUARD_APP)
	{
		sign_in_cancel();
		return 1;
	}
	if (ae_steam_leave_asks(&S.status))
	{
		leave_ask();
		return 1;
	}
	return 0;
}

static int steam_handle(struct ae_screen *instance, struct ae_event const *event)
{
	(void)instance;
	/* typing: Enter or START keeps (and goes on), Esc or B restores; up and down keep and move on */
	if (S.typing >= 0)
	{
		if (event->action == AE_ACTION_START || event->action == AE_ACTION_ACCEPT)
			typing_end(1);
		else if (event->action == AE_ACTION_BACK)
			typing_end(0);
		else if (event->action == AE_ACTION_UP || event->action == AE_ACTION_DOWN)
		{
			typing_stop_keep();
			focus_step(event->action == AE_ACTION_UP ? -1 : 1, 0);
		}
		return 1;
	}
	switch (event->action)
	{
	case AE_ACTION_BACK:
		return back_pressed();
	case AE_ACTION_UP:
	case AE_ACTION_DOWN:
		if (S.workshop && S.focus == IT_COLL)
		{
			coll_move(event);
			return 1;
		}
		if (S.workshop && S.focus == IT_LIST)
		{
			if (event->action == AE_ACTION_UP && S.list.list.focus <= 0)
				S.focus = IT_SORT;
			else
				ae_list_view_event(&S.list, event);
			return 1;
		}
		focus_step(event->action == AE_ACTION_UP ? -1 : 1, event->repeat);
		return 1;
	case AE_ACTION_LEFT:
	case AE_ACTION_RIGHT:
		if (S.hub && S.focus == IT_SHOW)
			type_step(event->action == AE_ACTION_RIGHT ? 1 : TYPES - 1);
		else if (S.workshop && S.focus == IT_LIST)
			ae_list_view_event(&S.list, event);
		return 1;
	case AE_ACTION_ACCEPT:
		activate(S.focus, event->device);
		return 1;
	case AE_ACTION_START:
		if (!S.workshop && S.page == AE_STEAM_PAGE_OUT)
			sign_in_submit();
		else if (!S.workshop && S.page == AE_STEAM_PAGE_GUARD_CODE)
			code_submit();
		return 1;
	case AE_ACTION_X:
		if (S.workshop && !(coll_active() && coll_level() == 2))
			edit_begin(F_SEARCH, event->device);
		return 1;
	case AE_ACTION_Y:
		if (S.workshop && coll_active())
			coll_page_open();
		else if (S.workshop && S.status.item_valid)
			item_page_open();
		return 1;
	case AE_ACTION_TAB_PREVIOUS:
	case AE_ACTION_TAB_NEXT:
		if (S.hub)
		{
			int direction = event->action == AE_ACTION_TAB_NEXT ? 1 : -1;

			ae_ui_pop();
			ae_screen_maps_hub_step(direction);
		}
		else if (S.workshop)
			type_step(event->action == AE_ACTION_TAB_NEXT ? 1 : TYPES - 1);
		return 1;
	case AE_ACTION_PAGE_UP:
	case AE_ACTION_PAGE_DOWN:
		if (S.workshop && coll_active())
			coll_page(event->action == AE_ACTION_PAGE_DOWN ? 1 : -1);
		else if (S.workshop)
		{
			if (client_live())
				ae_steam_client_page(event->action == AE_ACTION_PAGE_DOWN ? 1 : -1);
			ae_list_set_focus(&S.list.list, 0);
			ae_sound_request(AE_SOUND_CURSOR, 0);
		}
		return 1;
	default:
		if (S.workshop && S.focus == IT_LIST)
			ae_list_view_event(&S.list, event);
		return 1;
	}
}

static int point_in(struct ae_rect const *rect, float x, float y)
{
	return rect->width > 0.0f && x >= rect->x && x < rect->x + rect->width && y >= rect->y && y < rect->y + rect->height;
}

static void steam_pointer(struct ae_screen *instance, struct ae_pointer const *pointer)
{
	struct ae_event event;
	struct ae_hit hit;
	short ids[ITEMS_MOST];
	int count, index;

	if (ae_prompts_pointer(S.flow.prompts, S.flow.prompt_count, pointer, HIT_PROMPTS, &event))
	{
		event.device = AE_DEVICE_KEYBOARD_MOUSE;
		steam_handle(instance, &event);
		return;
	}
	if (S.asking)
		return;
	if (S.hub && ae_screen_maps_hub_strip_pointer(pointer))
		return;
	if (S.typing >= 0)
	{
		/* (typing: a click on another field or row keeps what was typed, sends nothing, and goes on there; a click
		anywhere else, and the pointer moving, change nothing) */
		int other = 0;

		if (pointer->left_clicks)
		{
			count = items_now(ids);
			for (index = 0; index < count; index++)
				if (ids[index] != IT_LIST && ids[index] != field_item(S.typing) &&
					point_in(&S.rects[ids[index]], pointer->x, pointer->y))
					other = 1;
		}
		if (!other)
			return;
		typing_stop_keep();
	}
	if (S.workshop && coll_active())
	{
		if (coll_pointer(pointer))
			return;
	}
	else if (S.workshop)
	{
		short item = ae_list_view_pointer(&S.list, pointer, HIT_LIST);

		if (item >= 0 && item < S.status.item_count)
		{
			if (S.focus == IT_LIST && item == S.list.list.focus)
				activate(IT_LIST, AE_DEVICE_KEYBOARD_MOUSE);
			S.focus = IT_LIST;
		}
		if (pointer->left_clicks && ae_hit_at(pointer->x, pointer->y, 0, &hit) && hit.id == HIT_CHIPS && hit.index >= 0 &&
			hit.index < TYPES)
			type_step((hit.index - S.status.type + TYPES) % TYPES);
	}
	count = items_now(ids);
	for (index = 0; index < count; index++)
	{
		short item = ids[index];

		if (item == IT_LIST || !point_in(&S.rects[item], pointer->x, pointer->y))
			continue;
		if (pointer->moved && S.focus != item)
		{
			S.focus = item;
			S.armed = 0;
		}
		if (pointer->left_clicks)
		{
			S.focus = item;
			activate(item, AE_DEVICE_KEYBOARD_MOUSE);
		}
		return;
	}
}

static void steam_leave(struct ae_screen *instance)
{
	int field;

	(void)instance;
	if (S.typing >= 0)
	{
		int typing = S.typing;

		S.typing = -1;
		ae_field_edit_end(&S.fields[typing].edit, 0);
	}
	/* (a sign-in under way when the screen goes: stopped; nothing typed stays) */
	if (client_live())
	{
		if (S.page == AE_STEAM_PAGE_SIGNING_IN || S.page == AE_STEAM_PAGE_GUARD_CODE || S.page == AE_STEAM_PAGE_GUARD_APP)
			ae_steam_client_cancel_sign_in();
		if (S.workshop)
			ae_steam_client_workshop(0);
	}
	for (field = 0; field < FIELDS; field++)
		field_wipe(field);
	picture_release();
	coll_pictures_release();
	platform_log("ae steam: closed");
}

static struct ae_screen_class const steam_class =
{
	.name = "steam account", .leave = steam_leave, .handle = steam_handle, .draw = steam_draw, .pointer = steam_pointer,
	.update = steam_update,
};

/* ---------- the doors */

int ae_screen_steam_available(void)
{
	return ae_steam_client_built();
}

static int open_screen(int stage, int percent)
{
	memset(&S, 0, sizeof(S));
	coll_reset();
	S.stage = stage;
	S.stage_percent = percent;
	S.typing = -1;
	S.keyboard = -1;
	S.pending_typing = -1;
	S.logged_shared = -1;
	S.logged_decile = -1;
	S.last_page = -1;
	S.logged_page = -1;
	S.first_frame = 1;
	S.flow.hit_rows = HIT_ROWS;
	S.flow.hit_prompts = HIT_PROMPTS;
	S.list_focus_sent = -1;
	fields_init();
	ae_list_view_init(&S.list, 0, LIST_ROWS);
	if (stage >= 0)
		ae_steam_stage_status(stage, percent, &S.status);
	else
	{
		ae_steam_client_open();
		client_frame();
		ae_steam_client_status(&S.status);
	}
	S.page = ae_steam_page(&S.status, 0);
	S.focus = default_focus();
	if (stage == AE_STEAM_STAGE_TYPING)
	{
		ae_text_init(&S.fields[F_ACCOUNT].text, "arena_player", 64, 0);
		ae_text_init(&S.fields[F_PASSWORD].text, "hunter2pass", 100, 0);
		S.fields[F_PASSWORD].text.secret = 1;
		S.focus = IT_PASSWORD;
		S.pending_typing = F_PASSWORD;
	}
	else if (stage == AE_STEAM_STAGE_WRONG_PASSWORD)
	{
		ae_text_init(&S.fields[F_ACCOUNT].text, "arena_player", 64, 0);
		S.focus = IT_PASSWORD;
	}
	else if (stage == AE_STEAM_STAGE_WORKSHOP)
	{
		S.workshop = 1;
		S.list_count = -1;
	}
	else if (stage == AE_STEAM_STAGE_LOGOUT)
		S.pending_logout = 1;
	if (!ae_ui_push(&steam_class, AE_OWNER_ANY, &S))
		return 0;
	if (S.workshop)
		S.focus = IT_LIST;
	platform_log("ae steam: open%s", stage >= 0 ? " (staged)" : "");
	return 1;
}

int ae_screen_steam_open(const char *crumb)
{
	int opened;

	if (!ae_steam_client_built())
		return 0;
	opened = open_screen(-1, 0);
	if (opened)
	{
		snprintf(S.origin, sizeof(S.origin), "%s", crumb ? crumb : "");
		platform_log("ae steam: opened from %s", S.origin[0] ? S.origin : "?");
	}
	return opened;
}

void ae_screen_steam_return_forget(void)
{
	reopen_account = 0;
}

int ae_screen_steam_workshop_ready(void)
{
	struct ae_steam_status status;

	if (!ae_steam_client_built())
		return 0;
	ae_steam_client_open();
	client_frame();
	ae_steam_client_status(&status);
	return ae_steam_page(&status, 0) == AE_STEAM_PAGE_IN && status.owns != 0;
}

int ae_screen_steam_open_workshop_hub(int staged)
{
	int opened;

	if (staged)
		ae_screen_steam_close();
	else if (!ae_screen_steam_workshop_ready())
		return 0;
	opened = open_screen(staged ? AE_STEAM_STAGE_WORKSHOP : -1, 0);
	if (!opened)
		return 0;
	S.hub = 1;
	S.workshop = 1;
	S.list_count = -1;
	S.focus = IT_LIST;
	if (!staged)
		ae_steam_client_workshop(1);
	return 1;
}

/* DEV MODE's `menu maps workshop <collections scene>`: the WORKSHOP page on made-up collections (ae_collections_data.h) */
int ae_screen_steam_open_collections_staged(int scene)
{
	struct ae_coll const *coll;
	int opened = ae_screen_steam_open_workshop_hub(1);

	if (!opened || scene <= AE_COLL_SCENE_NONE || scene >= AE_COLL_SCENES)
		return opened;
	C.on = 1;
	C.scene = scene == AE_COLL_SCENE_CONFIRM_GET ? AE_COLL_SCENE_OPEN : scene == AE_COLL_SCENE_CONFIRM_REMOVE ?
		AE_COLL_SCENE_INSTALLED : scene;
	coll_enter();
	S.focus = scene == AE_COLL_SCENE_SHOW ? IT_SHOW : IT_COLL;
	C.want_focus = S.focus;
	if (scene >= AE_COLL_SCENE_OPEN)
	{
		coll_open_row(0);
		coll = coll_open();
		if (scene == AE_COLL_SCENE_MAP && coll)
			C.focus = coll_action_count(coll) + 3;
		if (scene == AE_COLL_SCENE_CONFIRM_GET)
			C.pending_dialog = 1;
		if (scene == AE_COLL_SCENE_CONFIRM_REMOVE)
			C.pending_dialog = 2;
	}
	return opened;
}

int ae_screen_steam_open_staged(int stage, int percent)
{
	if (stage < 0 || stage >= AE_STEAM_STAGES)
		return 0;
	ae_screen_steam_close();
	return open_screen(stage, percent);
}

void ae_screen_steam_close(void)
{
	(void)ae_ui_remove(&S);
}

void ae_screen_steam_summary(char *text, int size)
{
	struct ae_steam_status status;
	char line[64];

	if (!ae_steam_client_built())
	{
		snprintf(text, (size_t)size, "NOT AVAILABLE");
		return;
	}
	/* (the client starts here the first time the row is drawn, so a kept sign-in shows as soon as it resumes) */
	ae_steam_client_open();
	client_frame();
	ae_steam_client_status(&status);
	ae_steam_account_summary(&status, ae_mcc_state() == AE_MCC_FOUND, line, sizeof(line));
	snprintf(text, (size_t)size, "%s", line);
}
