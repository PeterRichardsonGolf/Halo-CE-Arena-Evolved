/*
AE_SCREEN_DAILY.C

See ae_screen_daily.h. Layout (u at 1080): the title and where the day comes from; on the left DAY (left and right go to the days
before), MISSION, DIFFICULTY, SKULLS, RANDOMIZER and YOUR BEST TODAY, then the primary row PLAY TODAY'S CHALLENGE; on the right the
mission, when the next challenge comes, what is the same for everyone and what is left as the player has it, the day's skulls, and
the day's board (up to five rows). PAST DAYS lists the last thirty days with a PLAYED mark and the focused day's board.
The rows' help says what the skulls and the randomizer are on the day.
*/

#include <stdio.h>
#include <string.h>
#include <time.h>

#include "cseries.h"
#include "../src/ae_draw.h"
#include "../src/community_net.h"
#include "../src/port_config.h"
#include "ae_daily_rules.h"
#include "ae_glue_campaign.h"
#include "ae_hooks.h"
#include "ae_pve_menus_rules.h"
#include "ae_pve_menus_ui.h"
#include "ae_screen_daily.h"
#include "ae_screen_daily_server.h"
#include "ae_setup_flow.h"
#include "ae_sound.h"
#include "ae_strings.h"
#include "ae_style.h"
#include "ae_ui.h"
#include "ae_widgets.h"
#include "community.h"
#include "halo_skulls.h"

void platform_log(char const *format, ...);

enum { DAILY_HIT = 0x7BA0, PAST_HIT = 0x7BC0, BOARD_ROWS_SHOWN = 5, SKULLS_SHOWN = 8 };
enum { ROW_DAY, ROW_MISSION, ROW_DIFFICULTY, ROW_SKULLS, ROW_RANDOMIZER, ROW_BEST, ROW_PLAY, ROWS };

/* what a screen draws of a day: from the community code, or made up for DEV MODE's scenes */
struct view
{
	/* 0 nothing yet (a request waiting), 1 the day, 2 no day (not reached, refused) */
	int state;
	int online, unreachable, open;
	char day[COMMUNITY_DAY_LENGTH];
	short level, difficulty;
	int skull_count;
	short skulls[COMMUNITY_MAXIMUM_SKULLS];
	char details[COMMUNITY_MAXIMUM_DETAILS][COMMUNITY_DETAIL_LENGTH];
	int detail_count;
	long seconds_left;
	int has_mine;
	long mine_score, mine_seconds;
	char status[160];
};

struct board_row
{
	long rank, score, seconds, difficulty;
	char name[COMMUNITY_NAME_LENGTH];
};

struct board
{
	/* 0 waiting, 1 ready, 2 none (off, not reached) */
	int state, count, mine_rank;
	long players;
	struct board_row rows[COMMUNITY_BOARD_ROWS];
};

static struct
{
	struct ae_pvem_rows rows;
	/* DEV MODE's scene (NULL: the live one) */
	const char *scene;
	/* days back from today (0 to 29) and the day asked for last */
	int offset;
	char asked[COMMUNITY_DAY_LENGTH], board_asked[COMMUNITY_DAY_LENGTH];
	char today[COMMUNITY_DAY_LENGTH];
	struct view view;
	struct board board;
	char value[ROWS][48], help[ROWS][300], label[16], play[48];
	/* the window of a server not reached: asked for this open (until a choice asks the server again), the choices it shows
	as ids, its body, and the address window it opened (the day is asked again when this screen is shown once more) */
	int window_asked, reask, choice_ids[AE_DAILY_UNREACHED_CHOICES];
	char body[512];
} daily;

static struct
{
	struct ae_pvem_rows rows;
	const char *scene;
	int count;
	struct
	{
		char day[COMMUNITY_DAY_LENGTH], label[40], mission[48];
		short difficulty, skulls;
		int played;
		long mine_score;
	} days[COMMUNITY_HISTORY_DAYS];
	char value[COMMUNITY_HISTORY_DAYS][48], label[COMMUNITY_HISTORY_DAYS][48];
	char today[COMMUNITY_DAY_LENGTH];
	char board_asked[COMMUNITY_DAY_LENGTH];
	struct board board;
	int state;
	char status[160];
} past;

/* the game's text as ASCII (what the menus' fonts have; the rest a question mark) */
static void ascii_of(wchar_t const *wide, char *text, int size)
{
	int length = 0;

	for (; wide && *wide && length < size - 1; wide++)
		text[length++] = *wide >= 0x20 && *wide < 0x7F ? (char)*wide : '?';
	text[length] = 0;
}

static void today_text(char *today)
{
	time_t now = time(NULL);
	struct tm local = *localtime(&now);

	snprintf(today, COMMUNITY_DAY_LENGTH, "%04d-%02d-%02d", local.tm_year + 1900, local.tm_mon + 1, local.tm_mday);
}

static int online_now(void)
{
	return community_enabled() != 0;
}

/* ---------- the data: live, or DEV MODE's made-up day */

static void staged_board(struct board *board, int empty)
{
	static char const *const names[3] = { "Kat", "Chief", "Rook" };
	static const long scores[3] = { 61200, 57900, 40310 }, seconds[3] = { 1120, 1265, 1452 };
	int each;

	memset(board, 0, sizeof(*board));
	board->state = 1;
	board->count = empty ? 0 : 3;
	board->players = board->count;
	board->mine_rank = empty ? 0 : 2;
	for (each = 0; each < board->count; each++)
	{
		board->rows[each].rank = each + 1;
		snprintf(board->rows[each].name, sizeof(board->rows[each].name), "%s", names[each]);
		board->rows[each].score = scores[each];
		board->rows[each].seconds = seconds[each];
		board->rows[each].difficulty = each == 2 ? 2 : 3;
	}
}

static void staged_view(struct view *view, const char *scene)
{
	memset(view, 0, sizeof(*view));
	if (!ae_daily_day_shift(daily.today, -daily.offset, view->day, sizeof(view->day)))
		snprintf(view->day, sizeof(view->day), "%s", daily.today);
	view->state = 1;
	view->level = 0;
	view->difficulty = 3;
	view->skull_count = 3;
	view->skulls[0] = 0;
	view->skulls[1] = 11;
	view->skulls[2] = 14;
	snprintf(view->details[0], COMMUNITY_DETAIL_LENGTH, "ENEMIES: ALL KINDS, ALLIES TOO");
	snprintf(view->details[1], COMMUNITY_DETAIL_LENGTH, "ENEMY WEAPONS: NOT RANDOMIZED");
	snprintf(view->details[2], COMMUNITY_DETAIL_LENGTH, "PICKUPS: SHOTGUN ONLY");
	view->detail_count = 3;
	view->open = 1;
	view->seconds_left = 9 * 3600 + 12 * 60;
	view->has_mine = 1;
	view->mine_score = 57900;
	view->mine_seconds = 1265;
	if (!strcmp(scene, "online"))
		view->online = 1;
	else if (!strcmp(scene, "unreached") || !strcmp(scene, "unreached-custom"))
	{
		view->online = 1;
		view->unreachable = 1;
		view->state = 2;
		snprintf(view->status, sizeof(view->status), "The community server can't be reached right now. Check your connection, or try again later.");
	}
	else if (!strcmp(scene, "loading"))
	{
		view->online = 1;
		view->state = 0;
	}
	else if (!strcmp(scene, "empty"))
		view->has_mine = 0;
	else if (!strcmp(scene, "closed"))
	{
		view->open = 0;
		view->seconds_left = 0;
	}
}

static void view_read(void)
{
	struct view *view = &daily.view;
	struct community_challenge const *challenge;
	short state;

	if (daily.scene)
	{
		staged_view(view, daily.scene);
		return;
	}
	memset(view, 0, sizeof(*view));
	view->online = online_now();
	state = community_daily_state();
	view->unreachable = community_daily_unreachable();
	challenge = community_daily();
	if (challenge && state == _community_ready)
	{
		int each;

		view->state = 1;
		snprintf(view->day, sizeof(view->day), "%s", challenge->day);
		view->level = challenge->level_index;
		view->difficulty = challenge->difficulty;
		view->skull_count = challenge->skull_count;
		for (each = 0; each < challenge->skull_count && each < COMMUNITY_MAXIMUM_SKULLS; each++)
		{
			short skull;

			view->skulls[each] = NONE;
			for (skull = 0; skull < NUMBER_OF_SKULLS; skull++)
				if (!strcmp(skull_descriptions[skull].key, challenge->skulls[each]))
					view->skulls[each] = skull;
		}
		view->detail_count = challenge->detail_count;
		memcpy(view->details, challenge->details, sizeof(view->details));
		view->open = challenge->open;
		view->seconds_left = community_daily_seconds_left();
		view->has_mine = challenge->has_mine;
		view->mine_score = challenge->mine_score;
		view->mine_seconds = challenge->mine_seconds;
		if (!daily.offset && view->online)
			snprintf(daily.today, sizeof(daily.today), "%s", challenge->day);
	}
	else if (state == _community_unavailable || state == _community_refused || state == _community_off)
	{
		view->state = 2;
		ascii_of(community_status_text(), view->status, (int)sizeof(view->status));
	}
}

static void board_read(struct board *board, const char *day, char *asked, int staged_scene)
{
	if (staged_scene)
	{
		if (!board->state)
			staged_board(board, 0);
		return;
	}
	if (strcmp(asked, day))
	{
		snprintf(asked, COMMUNITY_DAY_LENGTH, "%s", day);
		community_board_open_day(day);
	}
	{
		short state = community_board_state(), each, count;

		memset(board, 0, sizeof(*board));
		board->state = state == _community_ready ? 1 : state == _community_loading ? 0 : 2;
		if (state != _community_ready)
			return;
		count = community_board_count();
		board->players = community_board_players();
		for (each = 0; each < count && each < COMMUNITY_BOARD_ROWS; each++)
		{
			struct community_row const *row = community_board_row(each);

			if (!row)
				continue;
			board->rows[board->count].rank = row->rank;
			board->rows[board->count].score = row->score;
			board->rows[board->count].seconds = row->seconds;
			board->rows[board->count].difficulty = row->difficulty;
			ascii_of(row->name, board->rows[board->count].name, (int)sizeof(board->rows[0].name));
			board->count++;
		}
		if (community_board_mine())
			board->mine_rank = (int)community_board_mine()->rank;
	}
}

/* ---------- the right-hand side */

static void board_draw(struct ae_flow_screen const *flow, float x, float width, float y, const struct board *board, int online)
{
	static const float at[5] = { 0.0f, 0.07f, 0.52f, 0.72f, 1.0f };
	static const char *const heads[5] = { "#", "PROFILE", "DIFFICULTY", "TIME", "SCORE" };
	float u = ae_flow_u(flow, 1.0f), top = flow->frame.rect.y, row_h = 44.0f * u, py = top + y * u;
	char text[64];
	int each;

	if (board->state != 1 || board->count == 0)
	{
		ae_pve_menus_ui_paragraph(flow, 16.0f, 24.0f, x, py + 14.0f * u, width, 3, AE_COLOR_MUTED,
			board->state == 0 ? "Asking the community server..." :
			board->state == 2 ? "No board to show." : online ? "Nobody has played this day yet." : "Nobody on this PC has played this day yet.");
		return;
	}
	for (each = 0; each < 5; each++)
		ae_pve_menus_ui_text(flow, AE_FONT_BODY, 14.0f, x + 12.0f * u + (width - 24.0f * u) * at[each], py + 10.0f * u,
			each == 4 ? AE_ALIGN_RIGHT : AE_ALIGN_LEFT, AE_COLOR_MUTED, heads[each]);
	py += 26.0f * u;
	for (each = 0; each < board->count && each < BOARD_ROWS_SHOWN; each++)
	{
		struct board_row const *row = &board->rows[each];
		int mine = board->mine_rank && row->rank == board->mine_rank;
		unsigned int ink = mine ? AE_COLOR_SELECTION_TEXT : AE_COLOR_TEXT;
		float mid = py + row_h * 0.5f;

		ae_draw_rect(x, py, width, row_h, 0.0f, mine ? AE_COLOR_SELECTION : AE_COLOR_ROW);
		snprintf(text, sizeof(text), "%ld", row->rank);
		ae_pve_menus_ui_text(flow, AE_FONT_ROW, 20.0f, x + 12.0f * u + (width - 24.0f * u) * at[0], mid, AE_ALIGN_LEFT, ink, text);
		ae_pve_menus_ui_text(flow, AE_FONT_ROW, 20.0f, x + 12.0f * u + (width - 24.0f * u) * at[1], mid, AE_ALIGN_LEFT, ink, row->name);
		ae_pve_menus_ui_text(flow, AE_FONT_BODY, 17.0f, x + 12.0f * u + (width - 24.0f * u) * at[2], mid, AE_ALIGN_LEFT, ink,
			ae_daily_difficulty_name((int)row->difficulty)[0] ? ae_daily_difficulty_name((int)row->difficulty) : "-");
		ae_pve_menus_time_text(row->seconds, text, sizeof(text));
		ae_pve_menus_ui_text(flow, AE_FONT_BODY, 17.0f, x + 12.0f * u + (width - 24.0f * u) * at[3], mid, AE_ALIGN_LEFT, ink, text);
		ae_pve_menus_number_text(row->score, text, sizeof(text));
		ae_pve_menus_ui_text(flow, AE_FONT_ROW, 20.0f, x + width - 12.0f * u, mid, AE_ALIGN_RIGHT, ink, text);
		py += row_h + 4.0f * u;
	}
	if (board->players > BOARD_ROWS_SHOWN)
	{
		snprintf(text, sizeof(text), "%ld PLAYED THIS DAY", board->players);
		ae_pve_menus_ui_text(flow, AE_FONT_BODY, 14.0f, x, py + 12.0f * u, AE_ALIGN_LEFT, AE_COLOR_MUTED, text);
	}
}

static void mission_text(short level, char *text, int size)
{
	if (level >= 0 && level < AE_CAMPAIGN_LEVELS)
		ae_campaign_level_name(level, text, size);
	else
		snprintf(text, (size_t)size, "A MISSION THIS GAME DOES NOT HAVE");
}

/* a plate over the backdrop, then a line: label muted at the left, value at the right of the label's column */
static void fact(struct ae_flow_screen const *flow, float x, float y, const char *label, const char *value, const char *note)
{
	float u = ae_flow_u(flow, 1.0f), top = flow->frame.rect.y;

	ae_pve_menus_ui_text(flow, AE_FONT_BODY, 14.0f, x, top + y * u, AE_ALIGN_LEFT, AE_COLOR_MUTED, label);
	ae_pve_menus_ui_text(flow, AE_FONT_ROW, 19.0f, x + 232.0f * u, top + y * u, AE_ALIGN_LEFT, AE_COLOR_TITLE, value);
	if (note && note[0])
		ae_pve_menus_ui_text(flow, AE_FONT_BODY, 16.0f, x + 232.0f * u + 8.0f * u +
			(float)strlen(value) * 11.0f * u, top + y * u, AE_ALIGN_LEFT, AE_COLOR_MUTED, note);
}

static void daily_side(struct ae_pvem_rows *r, float x, float width)
{
	struct ae_flow_screen const *flow = &r->flow;
	struct view const *view = &daily.view;
	float u = ae_flow_u(flow, 1.0f), top = flow->frame.rect.y;
	char text[160], label[16], head[64];

	ae_pve_menus_ui_plate(flow, x, width, 168.0f, 700.0f);
	if (view->state != 1)
	{
		ae_pve_menus_ui_text(flow, AE_FONT_ROW, 30.0f, x, top + 218.0f * u, AE_ALIGN_LEFT, AE_COLOR_TITLE,
			view->state == 0 ? "ASKING THE SERVER" : "NO CHALLENGE TO SHOW");
		ae_pve_menus_ui_paragraph(flow, 18.0f, 26.0f, x, top + 262.0f * u, width, 4, AE_COLOR_TEXT,
			view->state == 0 ? "The day is asked of the community server. Nothing is played until it answers." : view->status);
		if (view->state == 2 && view->online)
			ae_pve_menus_ui_paragraph(flow, 16.0f, 24.0f, x, top + 380.0f * u, width, 4, AE_COLOR_MUTED,
				"SETTINGS > CAMPAIGN > DAILY CHALLENGE turns ONLINE DAY AND BOARDS off to play this PC's day.");
		return;
	}
	mission_text(view->level, text, sizeof(text));
	ae_pve_menus_ui_text(flow, AE_FONT_ROW, 34.0f, x, top + 218.0f * u, AE_ALIGN_LEFT, AE_COLOR_TITLE, text);
	ae_daily_day_label(view->day, daily.today, label, sizeof(label));
	ae_daily_countdown_text(view->seconds_left, daily.offset == 0, text, sizeof(text));
	snprintf(head, sizeof(head), "%s \xE2\x80\xA2 %s", label[0] ? label : view->day, view->open ? text : "THIS DAY IS CLOSED");
	ae_pve_menus_ui_text(flow, AE_FONT_BODY, 16.0f, x, top + 252.0f * u, AE_ALIGN_LEFT, AE_COLOR_MUTED, head);
	fact(flow, x, 290.0f, "FOR EVERYONE", "the same enemies", "one seed a day");
	fact(flow, x, 326.0f, "YOUR SETTINGS", "left as they are", "this run only");
	/* (the day's skulls) */
	if (view->skull_count > 0)
	{
		float size = 40.0f * u, px = x;
		int each, shown = view->skull_count < SKULLS_SHOWN ? view->skull_count : SKULLS_SHOWN;
		int use_mcc = ae_pve_menus_ui_skulls_use_mcc(view->skulls, shown);

		for (each = 0; each < shown; each++)
		{
			ae_pve_menus_ui_skull_picture(view->skulls[each], use_mcc, px, top + 352.0f * u, size, size, 0xFFFFFFFFu);
			px += size + 8.0f * u;
		}
	}
	ae_daily_board_head(daily.offset == 0, view->online, head, sizeof(head));
	ae_pve_menus_ui_text(flow, AE_FONT_BODY, 16.0f, x, top + 418.0f * u, AE_ALIGN_LEFT, AE_COLOR_MUTED, head);
	board_draw(flow, x, width, 430.0f, &daily.board, view->online);
	if (!view->online)
		ae_pve_menus_ui_paragraph(flow, 16.0f, 22.0f, x, top + 770.0f * u, width, 3, AE_COLOR_MUTED, ae_daily_offline_note());
}

/* ---------- DAILY CHALLENGE's rows */

static void daily_open_day(void)
{
	char day[COMMUNITY_DAY_LENGTH];

	if (daily.scene)
		return;
	if (daily.offset == 0)
		day[0] = 0;
	else if (!ae_daily_day_shift(daily.today, -daily.offset, day, sizeof(day)))
		return;
	snprintf(daily.asked, sizeof(daily.asked), "%s", day[0] ? day : "today");
	daily.board_asked[0] = 0;
	community_daily_open(day[0] ? day : NULL);
}

/* ---------- the window of a server not reached */

static int unreached_custom(void)
{
	return daily.scene ? !strcmp(daily.scene, "unreached-custom") : community_net_custom() != 0;
}

static void unreached_picked(short choice, void *context)
{
	const char *key, *value;
	int id = choice >= 0 && choice < AE_DAILY_UNREACHED_CHOICES ? daily.choice_ids[choice] : AE_DAILY_UNREACHED_BACK;

	(void)context;
	if (daily.scene)
	{
		/* (a scene shows the window only: nothing is written, opened or asked) */
		platform_log("ae daily: staged scene: choice %d, nothing is changed", id);
		ae_sound_request(id == AE_DAILY_UNREACHED_BACK ? AE_SOUND_BACK : AE_SOUND_FORWARD, 0);
		return;
	}
	if (ae_daily_unreached_write(id, &key, &value))
	{
		if (!ae_pve_menus_ui_write(key, value))
			platform_log("ae daily: could not write %s", key);
	}
	if (id == AE_DAILY_UNREACHED_BACK)
		return;
	if (id == AE_DAILY_UNREACHED_ADDRESS)
	{
		daily.reask = 1;
		if (!ae_screen_daily_server_open())
			daily.reask = 0;
		return;
	}
	/* (the default server, offline or TRY AGAIN: the day is asked for again, which may bring the window once more) */
	daily.window_asked = 0;
	daily_open_day();
}

static void unreached_open(struct ae_pvem_rows *rows)
{
	struct ae_dialog_spec spec;
	const char *words[AE_DAILY_UNREACHED_CHOICES];
	char server[AE_DAILY_SERVER_MAXIMUM + 32], status[160];
	int custom = unreached_custom(), count, each;

	if (daily.scene)
		snprintf(server, sizeof(server), "%s", custom ? "https://example.com" : "the default community server");
	else
		community_net_server_text(server, (int)sizeof(server));
	snprintf(status, sizeof(status), "%s", daily.view.status);
	ae_daily_unreached_body(server, status, custom, daily.body, sizeof(daily.body));
	count = ae_daily_unreached_choices(custom, daily.choice_ids, words);
	memset(&spec, 0, sizeof(spec));
	spec.kind = AE_DIALOG_CONFIRM;
	spec.title = "SERVER NOT REACHED";
	spec.body = daily.body;
	for (each = 0; each < count; each++)
		spec.choices[each] = words[each];
	spec.choice_count = (short)count;
	/* (the first focus is BACK, which sends nothing; B picks it too) */
	spec.safe_choice = (short)(count - 1);
	spec.cancel_choice = (short)(count - 1);
	spec.has_cancel = 1;
	spec.density = rows->flow.density;
	spec.bounds = rows->flow.frame.rect;
	spec.picked = unreached_picked;
	spec.context = &daily;
	if (!ae_dialog_open(&spec, AE_OWNER_ANY))
		daily.window_asked = 0;
}

static void daily_build(struct ae_pvem_rows *r)
{
	struct ae_flow_screen *flow = &r->flow;
	struct view const *view;
	char text[96], day_label[16];
	int each;
	static const char *const labels[ROWS] = { "DAY", "MISSION", "DIFFICULTY", "SKULLS", "RANDOMIZER", "YOUR BEST TODAY", "" };

	if (daily.reask && ae_ui_top() && ae_ui_top()->data == &daily.rows)
	{
		/* (back from the address window: the server is asked again, at the address kept) */
		daily.reask = 0;
		daily.window_asked = 0;
		daily_open_day();
	}
	view_read();
	view = &daily.view;
	if (ae_daily_unreached_wanted(view->online, view->unreachable, daily.window_asked))
	{
		daily.window_asked = 1;
		r->dialog_pending = 1;
	}
	snprintf(r->crumb, sizeof(r->crumb), "%s", ae_daily_crumb(view->online, view->unreachable));
	ae_daily_day_label(daily.offset == 0 ? daily.today : view->day[0] ? view->day : "", daily.today, day_label, sizeof(day_label));
	if (daily.offset > 0 && !day_label[0])
	{
		char shifted[COMMUNITY_DAY_LENGTH];

		if (ae_daily_day_shift(daily.today, -daily.offset, shifted, sizeof(shifted)))
			ae_daily_day_label(shifted, daily.today, day_label, sizeof(day_label));
	}
	if (view->state == 1)
		board_read(&daily.board, view->day, daily.board_asked, daily.scene != NULL);
	else
		memset(&daily.board, 0, sizeof(daily.board));
	snprintf(daily.label, sizeof(daily.label), "%s", day_label[0] ? day_label : "TODAY");
	snprintf(daily.value[ROW_DAY], sizeof(daily.value[0]), "%s", daily.label);
	mission_text(view->level, daily.value[ROW_MISSION], (int)sizeof(daily.value[0]));
	snprintf(daily.value[ROW_DIFFICULTY], sizeof(daily.value[0]), "%s", ae_daily_difficulty_name(view->difficulty));
	ae_pve_menus_skulls_value(view->skull_count, daily.value[ROW_SKULLS], sizeof(daily.value[0]));
	ae_daily_randomizer_summary((char const (*)[64])view->details, view->detail_count, daily.value[ROW_RANDOMIZER], sizeof(daily.value[0]));
	if (view->has_mine)
		ae_pve_menus_number_text(view->mine_score, daily.value[ROW_BEST], sizeof(daily.value[0]));
	else
		snprintf(daily.value[ROW_BEST], sizeof(daily.value[0]), "NOT PLAYED");
	if (view->state != 1)
		for (each = ROW_MISSION; each <= ROW_BEST; each++)
			snprintf(daily.value[each], sizeof(daily.value[0]), "%s", view->state == 0 ? "ASKING..." : "-");
	snprintf(daily.help[ROW_DAY], sizeof(daily.help[0]), "%s", ae_daily_day_help());
	snprintf(daily.help[ROW_MISSION], sizeof(daily.help[0]), "The mission the day sets for everyone. A run counts for its day's board.");
	snprintf(daily.help[ROW_DIFFICULTY], sizeof(daily.help[0]), "The difficulty the day sets. Your own difficulty is not used.");
	text[0] = 0;
	for (each = 0; each < view->skull_count; each++)
		if (view->skulls[each] >= 0 && view->skulls[each] < NUMBER_OF_SKULLS)
			snprintf(text + strlen(text), sizeof(text) - strlen(text), "%s%s", each ? ", " : "", skull_descriptions[view->skulls[each]].label);
	snprintf(daily.help[ROW_SKULLS], sizeof(daily.help[0]), "%s%s%s", view->skull_count ? "Skulls on in this run: " : "No skull is on in this run.",
		text, view->skull_count ? ". Yours stay as they are." : "");
	text[0] = 0;
	for (each = 0; each < view->detail_count; each++)
		snprintf(text + strlen(text), sizeof(text) - strlen(text), "%s%s", each ? ". " : "", view->details[each]);
	snprintf(daily.help[ROW_RANDOMIZER], sizeof(daily.help[0]), "%s", text[0] ? text : "The day's randomizer.");
	snprintf(daily.help[ROW_BEST], sizeof(daily.help[0]), "%s", view->has_mine ?
		"Your best score on this day's board." : "You have not played this day yet.");
	ae_daily_play_text(daily.label, daily.play, sizeof(daily.play));
	if (ae_daily_retry_shown(view->online, view->state))
		snprintf(daily.play, sizeof(daily.play), "RETRY");
	snprintf(daily.help[ROW_PLAY], sizeof(daily.help[0]), "%s", view->state == 1 ?
		(view->open ? "Plays the day's mission with its skulls and randomizer. Your settings are back after the run." :
			"This day is closed: it takes no more scores. A run is still played, and not ranked.") : view->state == 0 ?
		"Waiting for the day." : ae_daily_retry_shown(view->online, view->state) ?
		"Asks the community server for the day again." : "There is no challenge to play.");
	for (each = 0; each < ROWS; each++)
	{
		struct ae_flow_row *row = &flow->rows[each];

		if (each == ROW_PLAY)
			*row = (struct ae_flow_row){ AE_FLOW_ROW_OWN, -1, daily.play, NULL, daily.help[each], view->state == 1 || ae_daily_retry_shown(view->online, view->state) ? NULL : "NO DAY", 1 };
		else
		{
			*row = (struct ae_flow_row){ AE_FLOW_ROW_OWN, -1, labels[each], daily.value[each], daily.help[each], NULL };
			row->status = each != ROW_DAY;
		}
	}
	flow->count = ROWS;
	r->y_hidden = r->x_hidden = 0;
}

static int level_of_name(char const *map)
{
	return ae_campaign_level_of_map(map);
}

static void play_run(int random)
{
	static struct ae_campaign none;
	char const *map = NULL;
	short difficulty = 0, level;

	if (daily.scene)
	{
		platform_log("ae daily: staged scene: nothing is played");
		ae_sound_request(AE_SOUND_FAILURE, 0);
		return;
	}
	if (!(random ? community_random_begin(&map, &difficulty) : community_daily_begin(&map, &difficulty)))
	{
		ae_sound_request(AE_SOUND_FAILURE, 0);
		platform_log("ae daily: the %s was not begun", random ? "random run" : "challenge");
		return;
	}
	level = (short)level_of_name(map);
	if (level < 0 || !ae_campaign_start(&none, level, difficulty))
	{
		community_daily_cancel();
		ae_sound_request(AE_SOUND_FAILURE, 0);
		platform_log("ae daily: the level did not start");
		return;
	}
	platform_log("ae daily: %s started (level %d)", random ? "random run" : "challenge", (int)level + 1);
	ae_sound_request(AE_SOUND_FORWARD, 0);
	ae_ui_game_starting();
	while (ae_ui_depth() && ae_ui_top())
	{
		if (!ae_ui_top()->data || !ae_ui_remove(ae_ui_top()->data))
			ae_ui_pop();
	}
}

static void daily_step(struct ae_pvem_rows *r, int row, int direction)
{
	(void)r;
	if (row == ROW_PLAY)
	{
		if (daily.view.state == 1)
			play_run(0);
		else if (ae_daily_retry_shown(daily.view.online, daily.view.state))
		{
			/* (RETRY: the online day asked for again; a staged scene asks nothing) */
			if (daily.scene)
			{
				platform_log("ae daily: staged scene: nothing is asked");
				ae_sound_request(AE_SOUND_FAILURE, 0);
			}
			else
			{
				ae_sound_request(AE_SOUND_FORWARD, 0);
				daily.window_asked = 0;
				daily_open_day();
			}
		}
		return;
	}
	if (row != ROW_DAY)
		return;
	/* (left: the day before, right: the day after) */
	{
		int offset = daily.offset - direction;

		if (offset < 0 || offset >= AE_DAILY_HISTORY_DAYS)
		{
			ae_sound_request(AE_SOUND_FAILURE, 0);
			return;
		}
		daily.offset = offset;
		ae_sound_request(AE_SOUND_CURSOR, 0);
		daily_open_day();
	}
}

static const char *daily_a_word(struct ae_pvem_rows *r, int row)
{
	(void)r;
	return row == ROW_PLAY ? (ae_daily_retry_shown(daily.view.online, daily.view.state) ? "Retry" : "Play") : row == ROW_DAY ? "Next day" : "Look";
}

static int past_open_now(void);

static void daily_x(struct ae_pvem_rows *r)
{
	(void)r;
	if (past_open_now())
		ae_sound_request(AE_SOUND_FORWARD, 0);
}

static void daily_y(struct ae_pvem_rows *r)
{
	(void)r;
	/* (a random run is rolled on this PC: it plays with the server out of reach as well) */
	play_run(1);
}

static int daily_open_scene(const char *scene, int offset)
{
	if (ae_ui_holds(&daily.rows))
		ae_pve_menus_rows_close(&daily.rows);
	memset(&daily, 0, sizeof(daily));
	daily.scene = scene;
	daily.offset = offset;
	today_text(daily.today);
	daily.rows.name = "daily challenge";
	daily.rows.title = "DAILY CHALLENGE";
	daily.rows.build = daily_build;
	daily.rows.dialog_open = unreached_open;
	daily.rows.step = daily_step;
	daily.rows.a_word = daily_a_word;
	daily.rows.x_action = daily_x;
	daily.rows.x_word = "Past days";
	daily.rows.y_action = daily_y;
	daily.rows.y_word = "Random run";
	daily.rows.side = daily_side;
	daily.rows.hit_base = DAILY_HIT;
	daily_open_day();
	view_read();
	snprintf(daily.rows.crumb, sizeof(daily.rows.crumb), "%s", ae_daily_crumb(daily.view.online, daily.view.unreachable));
	if (!ae_pve_menus_rows_open(&daily.rows))
		return 0;
	/* (focus on PLAY once there is a day, else on DAY) */
	daily.rows.flow.focus = ROW_DAY;
	return 1;
}

int ae_screen_daily_open(void)
{
	return daily_open_scene(NULL, 0);
}

int ae_screen_daily_row_shown(void)
{
	return config_boolean("game.daily_challenge") != 0;
}

void ae_screen_daily_after_win(void)
{
	boolean random = FALSE;

	if (!community_daily_won_take(&random))
		return;
	platform_log("ae daily: a %s was won: the screen comes back", random ? "random run" : "challenge");
	if (!ae_screen_daily_open())
		return;
	if (!random && community_daily_won_day()[0])
	{
		char today[COMMUNITY_DAY_LENGTH];
		int ago;

		today_text(today);
		ago = ae_daily_days_ago(community_daily_won_day(), today);
		if (ago > 0 && ago < AE_DAILY_HISTORY_DAYS)
		{
			daily.offset = ago;
			daily_open_day();
		}
	}
}

/* ---------- PAST DAYS */

static void past_build(struct ae_pvem_rows *r)
{
	struct ae_flow_screen *flow = &r->flow;
	int each;

	for (each = 0; each < past.count; each++)
	{
		struct ae_flow_row *row = &flow->rows[each];
		char name[24];

		ae_daily_day_label(past.days[each].day, past.today, name, sizeof(name));
		snprintf(past.label[each], sizeof(past.label[0]), "%s%s", name, past.days[each].played ? "  \xC2\xB7  PLAYED" : "");
		snprintf(past.value[each], sizeof(past.value[0]), "%s", past.days[each].mission);
		*row = (struct ae_flow_row){ AE_FLOW_ROW_OWN, -1, past.label[each], past.value[each], "", NULL };
		row->status = 1;
		/* (the focused day's line under the rows) */
		if (past.days[each].day[0])
		{
			static char help[COMMUNITY_HISTORY_DAYS][200];
			char text[24];

			ae_daily_day_label(past.days[each].day, past.today, text, sizeof(text));
			snprintf(help[each], sizeof(help[0]), "A opens %s's challenge: %s on %s. Its runs go to its own board.", text,
				past.days[each].mission, ae_daily_difficulty_name(past.days[each].difficulty));
			row->help = help[each];
		}
	}
	flow->count = (short)past.count;
}

static void past_side(struct ae_pvem_rows *r, float x, float width)
{
	struct ae_flow_screen const *flow = &r->flow;
	float u = ae_flow_u(flow, 1.0f), top = flow->frame.rect.y;
	short focus = flow->focus;
	char text[120], name[24];

	ae_pve_menus_ui_plate(flow, x, width, 168.0f, 700.0f);
	if (past.state == 0)
	{
		ae_pve_menus_ui_paragraph(flow, 18.0f, 26.0f, x, top + 230.0f * u, width, 3, AE_COLOR_TEXT, "Asking the community server for the days...");
		return;
	}
	if (past.state == 2 || focus < 0 || focus >= past.count)
	{
		ae_pve_menus_ui_paragraph(flow, 18.0f, 26.0f, x, top + 230.0f * u, width, 4, AE_COLOR_TEXT,
			past.status[0] ? past.status : "No past days to show.");
		return;
	}
	ae_pve_menus_ui_text(flow, AE_FONT_ROW, 34.0f, x, top + 218.0f * u, AE_ALIGN_LEFT, AE_COLOR_TITLE, past.days[focus].mission);
	ae_daily_day_label(past.days[focus].day, past.today, name, sizeof(name));
	snprintf(text, sizeof(text), "%s \xE2\x80\xA2 %s \xE2\x80\xA2 %d SKULL%s", name, ae_daily_difficulty_name(past.days[focus].difficulty),
		past.days[focus].skulls, past.days[focus].skulls == 1 ? "" : "S");
	ae_pve_menus_ui_text(flow, AE_FONT_BODY, 16.0f, x, top + 252.0f * u, AE_ALIGN_LEFT, AE_COLOR_MUTED, text);
	if (past.days[focus].played)
		ae_pve_menus_number_text(past.days[focus].mine_score, text, sizeof(text));
	else
		snprintf(text, sizeof(text), "NOT PLAYED");
	ae_pve_menus_ui_text(flow, AE_FONT_BODY, 14.0f, x, top + 290.0f * u, AE_ALIGN_LEFT, AE_COLOR_MUTED, "YOUR BEST");
	ae_pve_menus_ui_text(flow, AE_FONT_ROW, 19.0f, x + 232.0f * u, top + 290.0f * u, AE_ALIGN_LEFT, AE_COLOR_TITLE, text);
	ae_daily_board_head(0, online_now(), text, sizeof(text));
	ae_pve_menus_ui_text(flow, AE_FONT_BODY, 16.0f, x, top + 336.0f * u, AE_ALIGN_LEFT, AE_COLOR_MUTED, text);
	board_read(&past.board, past.days[focus].day, past.board_asked, past.scene != NULL);
	board_draw(flow, x, width, 348.0f, &past.board, online_now());
}

static void past_step(struct ae_pvem_rows *r, int row, int direction)
{
	(void)direction;
	if (row < 0 || row >= past.count)
		return;
	/* (the day opened on the screen under this one) */
	if (!past.scene && ae_daily_days_ago(past.days[row].day, past.today) >= 0)
	{
		int ago = ae_daily_days_ago(past.days[row].day, past.today);

		ae_pve_menus_rows_close(r);
		if (ae_ui_holds(&daily.rows) && ago < AE_DAILY_HISTORY_DAYS)
		{
			daily.offset = ago;
			daily_open_day();
			ae_sound_request(AE_SOUND_FORWARD, 0);
		}
		return;
	}
	ae_sound_request(AE_SOUND_CURSOR, 0);
}

static void past_read(void)
{
	short each, count;

	past.count = 0;
	if (past.scene)
	{
		static char const *const missions[5] = { "HALO", "TWO BETRAYALS", "KEYES", "THE MAW", "343 GUILTY SPARK" };
		char day[COMMUNITY_DAY_LENGTH];

		for (each = 0; each < COMMUNITY_HISTORY_DAYS; each++)
		{
			if (!ae_daily_day_shift(past.today, -each, day, sizeof(day)))
				break;
			snprintf(past.days[each].day, sizeof(past.days[0].day), "%s", day);
			snprintf(past.days[each].mission, sizeof(past.days[0].mission), "%s", missions[each % 5]);
			past.days[each].difficulty = (short)(each % 3 + 1);
			past.days[each].skulls = (short)(each % 4);
			past.days[each].played = each == 0 || each == 2;
			past.days[each].mine_score = 57900 - 1000 * each;
			past.count++;
		}
		past.state = 1;
		return;
	}
	{
		short state = community_history_state();

		past.state = state == _community_ready ? 1 : state == _community_loading ? 0 : 2;
		if (past.state == 2)
			ascii_of(community_status_text(), past.status, (int)sizeof(past.status));
		if (past.state != 1)
			return;
		count = community_history_count();
		for (each = 0; each < count && each < COMMUNITY_HISTORY_DAYS; each++)
		{
			struct community_history_day const *day = community_history_day(each);

			if (!day)
				continue;
			snprintf(past.days[past.count].day, sizeof(past.days[0].day), "%s", day->day);
			mission_text(day->level_index, past.days[past.count].mission, (int)sizeof(past.days[0].mission));
			past.days[past.count].difficulty = day->difficulty;
			past.days[past.count].skulls = day->skull_count;
			past.days[past.count].played = day->has_mine;
			past.days[past.count].mine_score = day->mine_score;
			past.count++;
		}
	}
}

static void past_refresh(struct ae_pvem_rows *r)
{
	past_read();
	past_build(r);
}

static int past_open_scene(const char *scene)
{
	if (ae_ui_holds(&past.rows))
		ae_pve_menus_rows_close(&past.rows);
	memset(&past, 0, sizeof(past));
	past.scene = scene;
	today_text(past.today);
	if (daily.today[0])
		snprintf(past.today, sizeof(past.today), "%s", daily.today);
	if (!scene)
		community_history_open();
	past.rows.name = "daily past days";
	past.rows.title = "PAST DAYS";
	snprintf(past.rows.crumb, sizeof(past.rows.crumb), "DAILY CHALLENGE \xC2\xB7 THIS PC");
	if (online_now())
		snprintf(past.rows.crumb, sizeof(past.rows.crumb), "DAILY CHALLENGE \xC2\xB7 ONLINE");
	past.rows.build = past_refresh;
	past.rows.step = past_step;
	past.rows.a_word = NULL;
	past.rows.side = past_side;
	past.rows.hit_base = PAST_HIT;
	past_read();
	if (!ae_pve_menus_rows_open(&past.rows))
		return 0;
	return 1;
}

static int past_open_now(void)
{
	return past_open_scene(daily.scene ? "past" : NULL);
}

/* ---------- DEV MODE */

int ae_screen_daily_open_staged(const char *scene)
{
	if (!scene || !scene[0])
		scene = "today";
	if (!strcmp(scene, "past"))
	{
		today_text(daily.today);
		return past_open_scene("past");
	}
	if (!strcmp(scene, "today") || !strcmp(scene, "online") || !strcmp(scene, "unreached") || !strcmp(scene, "unreached-custom") || !strcmp(scene, "loading") ||
		!strcmp(scene, "empty") || !strcmp(scene, "closed"))
	{
		static char kept[24];

		snprintf(kept, sizeof(kept), "%s", scene);
		return daily_open_scene(kept, !strcmp(scene, "closed") ? 1 : 0);
	}
	return 0;
}
