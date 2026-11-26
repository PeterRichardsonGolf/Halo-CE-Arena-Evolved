/*
AE_SCREEN_COOP_LOBBY.C

See ae_screen_coop_lobby.h. The left column is the flow's rows (LEVEL, DIFFICULTY, PLAYERS, PUBLIC and INVITE when hosting
online, START GAME / WAITING FOR THE HOST), the right the people in the game as MY PARTY lists them (ae_widgets.h), with the
profile cards of pads joining and the open card ("Press START to add a player") while this PC has a seat left. The game is the
engine's own co-op game (ae_glue_lobby.h: ae_lobby_set_coop_level); a joiner's lobby is the same screen, read only.

A game started takes every AE screen off the stack (the take-over, ae_lobby_take_pregame_ui, brings this lobby back when the
level is over, as CREATE GAME's).
*/

#include <stdio.h>
#include <string.h>

#include "cseries.h"
#include "game/game.h"
#include "game/game_engine.h"
#include "../src/ae_draw.h"
#include "../src/ae_platform.h"
#include "ae_coop_rules.h"
#include "ae_glue_campaign.h"
#include "ae_glue_lobby.h"
#include "ae_glue_profiles.h"
#include "ae_hooks.h"
#include "ae_input.h"
#include "ae_invite_share.h"
#include "ae_lobby_countdown.h"
#include "ae_screen_coop_lobby.h"
#include "../src/port_config.h"
#include "ae_pve_menus_rules.h"
#include "ae_pve_menus_ui.h"
#include "ae_screen_coop_options.h"
#include "ae_screen_scoring.h"
#include "ae_screen_skulls.h"
#include "ae_screen_lobby_profile.h"
#include "ae_setup_flow.h"
#include "ae_sound.h"
#include "ae_strings.h"
#include "ae_ui.h"
#include "ae_widgets.h"

void platform_log(char const *format, ...);
unsigned long system_milliseconds(void);
boolean main_menu_is_active(void);
void ui_widgets_close_all(void);
int config_boolean(char const *name);
long config_integer(char const *name);
/* p2p.c's (internet play: the hosted game's invite, its listing) */
int p2p_invite_link(char *link, int size);
void p2p_set_hosting_password(const char *password);
boolean ui_widget_port_cooperative_public(void);
void ui_widget_port_cooperative_public_set(boolean public_game);

enum
{
	COOP_HIT = 0x7EC0,
	HIT_ROWS = COOP_HIT + 1, HIT_PROMPTS, HIT_PARTY, HIT_CARDS,
	/* this PC's client in its own lobby: waited for this long */
	JOIN_WAIT_MS = 10000,
	ROSTER_MS = 250,
	NOTE_MS = 3500
};
enum { ROW_HEADER = AE_FLOW_ROW_HEADER, ROW_LEVEL = AE_FLOW_ROW_OWN, ROW_DIFFICULTY, ROW_PLAYERS, ROW_PUBLIC, ROW_INVITE,
	ROW_START, ROW_WAIT, ROW_RULE_SKULLS, ROW_RULE_RANDOMIZER, ROW_RULE_SCORING, ROW_RULE_COOP };
enum { STEP_IDLE, STEP_JOINING, STEP_LIVE, STEP_FAILED };

static struct
{
	struct ae_flow_screen flow;
	int joiner, step, network, public_game, starting, host_started, leaving, staged;
	short level, difficulty;
	unsigned pads;
	unsigned long since, roster_at, note_until;
	struct ae_lobby_roster roster;
	int have_roster;
	struct ae_countdown countdown;
	char how[48], crumb[64], level_name[64], players_text[24], invite[160], invite_shown[48], note[120], count_help[48];
	char names[AE_LOBBY_MAXIMUM_PLAYERS][AE_LOBBY_NAME_SIZE], subs[AE_LOBBY_MAXIMUM_PLAYERS][48];
	char difficulty_text[16], start_text[32], more_text[32];
	short logged_people;
} coop;

static struct ae_screen_class const coop_class;

static int live(void)
{
	return coop.step == STEP_LIVE && ae_lobby_owns_session();
}

/* ---------- the rows */

static void rows_make(void)
{
	struct ae_flow_screen *flow = &coop.flow;
	short count = 0, maximum = coop.have_roster && coop.roster.maximum_players > 0 ? coop.roster.maximum_players :
		AE_COOP_ONLINE_PLAYERS_MAX;
	short people = coop.have_roster ? coop.roster.people : 0;
	int counting = ae_countdown_running(&coop.countdown);

	snprintf(coop.players_text, sizeof(coop.players_text), "%d / %d", people, maximum);
	snprintf(coop.difficulty_text, sizeof(coop.difficulty_text), "%s", ae_campaign_difficulty_name(coop.difficulty));
	flow->rows[count++] = (struct ae_flow_row){ ROW_HEADER, -1, "CO-OP CAMPAIGN", NULL, NULL, NULL };
	flow->rows[count++] = (struct ae_flow_row){ ROW_LEVEL, -1, "LEVEL", coop.level_name,
		coop.joiner ? "The host's level." : "The level you chose on CAMPAIGN.", NULL, 0, 1 };
	flow->rows[count++] = (struct ae_flow_row){ ROW_DIFFICULTY, -1, "DIFFICULTY", coop.difficulty_text,
		coop.joiner ? "The host's difficulty." : "Left and right change it for everyone in the lobby.", NULL, 0,
		coop.joiner || counting };
	flow->rows[count++] = (struct ae_flow_row){ ROW_PLAYERS, -1, "PLAYERS", coop.players_text,
		"Up to 16 can play. Others join from the SERVER BROWSER, LAN GAMES or the invite.", NULL, 0, 1 };
	if (!coop.joiner && coop.network == AE_COOP_NET_ONLINE)
	{
		flow->rows[count++] = (struct ae_flow_row){ ROW_PUBLIC, -1, "PUBLIC", coop.public_game ? "ON" : "OFF",
			"ON lists the game in everyone's SERVER BROWSER; OFF is invite only.", NULL, 0, counting };
		flow->rows[count++] = (struct ae_flow_row){ ROW_INVITE, -1, "INVITE",
			ae_invite_share_copied() ? "COPIED" : coop.invite_shown[0] ? coop.invite_shown : "BEING MADE...",
			"{A} copies the invite link: send it to a friend, who pastes it in JOIN WITH INVITE.", NULL, 0, 1 };
	}
	/* (RULES: the campaign lobby's rows. The host opens them; a guest reads them. The randomizer plays on one PC, never online) */
	{
		static char skulls_text[16], scoring_text[8], coop_text[12];
		int skulls = config_boolean("game.skulls") ? ae_pve_menus_ui_skulls_on_count() : 0;

		ae_pve_menus_skulls_value(skulls, skulls_text, sizeof(skulls_text));
		snprintf(scoring_text, sizeof(scoring_text), "%s", config_boolean("game.campaign_score") ? "ON" : "OFF");
		snprintf(coop_text, sizeof(coop_text), "%s", ae_screen_coop_options_standard() ? "STANDARD" : "CHANGED");
		flow->rows[count++] = (struct ae_flow_row){ ROW_HEADER, -1, "RULES", NULL, NULL, NULL };
		flow->rows[count++] = (struct ae_flow_row){ ROW_RULE_SKULLS, -1, "SKULLS", skulls_text,
			coop.joiner ? "The host's skulls." : "Skulls change the campaign's rules. {A} opens the list.", NULL, 0, 0, coop.joiner, 1 };
		flow->rows[count++] = (struct ae_flow_row){ ROW_RULE_RANDOMIZER, -1, "RANDOMIZER", NULL,
			"The randomizer plays in games on one PC: alone or split screen. Online, everyone plays the campaign's own enemies and weapons.",
			"NOT IN ONLINE GAMES", 0, 0, 1 };
		flow->rows[count++] = (struct ae_flow_row){ ROW_RULE_SCORING, -1, "SCORING", scoring_text,
			coop.joiner ? "The host's scoring." : "Score, time and medals for each mission. {A} opens it.", NULL, 0, 0, coop.joiner, 1 };
		flow->rows[count++] = (struct ae_flow_row){ ROW_RULE_COOP, -1, "CO-OP OPTIONS", coop_text,
			coop.joiner ? "The host's co-op options." : "Friendly fire, extra enemies and pickups, what happens when the team falls.", NULL, 0, 0,
			coop.joiner, 1 };
	}
	if (coop.joiner)
		flow->rows[count++] = (struct ae_flow_row){ ROW_WAIT, -1,
			ae_countdown_running(&coop.countdown) ? coop.start_text : "WAITING FOR THE HOST TO START", NULL,
			"The host starts the level. B leaves the game.", NULL, 1, 0, 1 };
	else
		flow->rows[count++] = (struct ae_flow_row){ ROW_START, -1, counting ? coop.start_text : "START GAME", NULL,
			counting ? coop.count_help : "Starts the level for everyone in the lobby.", NULL, 1 };
	flow->count = count;
	if (flow->focus < 0 || flow->focus >= count)
		flow->focus = (short)(count - 1);
	if (flow->rows[flow->focus].kind == ROW_HEADER)
		flow->focus++;
	/* (a note in the help's place: a refusal, the server's word) */
	if (coop.note[0] && (long)(coop.note_until - system_milliseconds()) > 0)
		flow->rows[flow->focus].help = coop.note;
}

static short focused_kind(void)
{
	struct ae_flow_screen const *flow = &coop.flow;

	return flow->focus >= 0 && flow->focus < flow->count ? flow->rows[flow->focus].kind : -1;
}

static void note(char const *text)
{
	snprintf(coop.note, sizeof(coop.note), "%s", text);
	coop.note_until = system_milliseconds() + NOTE_MS;
}

/* ---------- the session */

static void level_texts(void)
{
	ae_campaign_level_name(coop.level, coop.level_name, sizeof(coop.level_name));
	snprintf(coop.crumb, sizeof(coop.crumb), "%s", coop.joiner ? (coop.how[0] ? coop.how : "JOINED") :
		coop.network == AE_COOP_NET_ONLINE ? (coop.public_game ? "ONLINE \xE2\x80\xA2 PUBLIC" : "ONLINE \xE2\x80\xA2 INVITE ONLY") :
		"YOUR NETWORK");
}

static void clear_stack(void)
{
	while (ae_ui_depth() && ae_ui_top())
	{
		if (!ae_ui_top()->data || !ae_ui_remove(ae_ui_top()->data))
			ae_ui_pop();
	}
}

/* B: the session closed (a host's) or left (a joiner's), the screen off the stack */
static void leave_now(void)
{
	platform_log("ae coop lobby: leave");
	coop.leaving = 1;
	if (ae_lobby_owns_session())
		ae_lobby_leave();
	coop.step = STEP_IDLE;
	coop.starting = 0;
	ae_ui_remove(&coop);
}

static void failed_open(int error)
{
	char const *title, *body;

	ae_coop_host_error_text(error, &title, &body);
	platform_log("ae coop lobby: open failed: %s", body);
	if (ae_lobby_owns_session())
		ae_lobby_leave();
	coop.step = STEP_FAILED;
	ae_flow_error(&coop.flow, title, body);
	ae_ui_remove(&coop);
}

/* this PC's client is in its own lobby: player 1, the level, then this PC's other players: the lobby is live */
static void host_continue(void)
{
	static struct ae_lobby_roster roster;
	struct ae_result result;
	short controller;

	if (!ae_lobby_roster(&roster).ok || !roster.joined)
	{
		if (system_milliseconds() - coop.since >= JOIN_WAIT_MS)
			failed_open(AE_COOP_HOST_ERR_TIMEOUT);
		return;
	}
	if (!ae_lobby_add_local_player(0).ok)
	{
		failed_open(AE_COOP_HOST_ERR_HOST);
		return;
	}
	result = ae_lobby_set_coop_level(coop.level, coop.difficulty);
	if (!result.ok)
	{
		failed_open(strstr(result.reason, "co-op") ? AE_COOP_HOST_ERR_PEER : AE_COOP_HOST_ERR_LEVEL);
		return;
	}
	if (coop.network == AE_COOP_NET_ONLINE)
		ui_widget_port_cooperative_public_set(coop.public_game);
	for (controller = 1; controller < AE_COOP_CONTROLLERS; controller++)
	{
		if (coop.pads & (1u << controller))
		{
			result = ae_lobby_join_controller(controller);
			platform_log("ae coop lobby: player on controller %d joins: %s", controller + 1, result.ok ? "ok" : result.reason);
		}
	}
	coop.step = STEP_LIVE;
	platform_log("ae coop lobby: hosted, player 1 in");
}

/* ---------- the people */

static void roster_read(unsigned long now)
{
	if (now - coop.roster_at < ROSTER_MS)
		return;
	coop.roster_at = now;
	coop.have_roster = ae_lobby_owns_session() && ae_lobby_roster(&coop.roster).ok;
	if (coop.have_roster && coop.roster.people != coop.logged_people)
	{
		coop.logged_people = coop.roster.people;
		platform_log("ae coop lobby: players %d", coop.logged_people);
	}
	/* (the game's level and difficulty: a joiner's follow the host's; the host's own are as it set them) */
	if (coop.joiner && coop.have_roster)
	{
		short level, difficulty;

		if (ae_lobby_coop_game(&level, &difficulty) && (level != coop.level || difficulty != coop.difficulty))
		{
			coop.level = level;
			coop.difficulty = difficulty;
			level_texts();
		}
	}
	/* (the invite: made once the game is hosted for the internet; shown by its first digits only) */
	if (!coop.joiner && coop.network == AE_COOP_NET_ONLINE)
	{
		if (!config_boolean("network.online"))
			snprintf(coop.invite_shown, sizeof(coop.invite_shown), "INTERNET PLAY IS OFF");
		else if (!live() || !p2p_invite_link(coop.invite, sizeof(coop.invite)) || !coop.invite[0])
		{
			coop.invite[0] = 0;
			snprintf(coop.invite_shown, sizeof(coop.invite_shown), "BEING MADE...");
		}
		else
		{
			char const *digits = strstr(coop.invite, "join/");

			snprintf(coop.invite_shown, sizeof(coop.invite_shown), "halo://join/%.8s...", digits ? digits + 5 : coop.invite);
		}
	}
}

/* a pad's profile card is answered (ae_screen_lobby_profile.h): picked joins it with its profile */
static void seat_done(short player, int picked, void *context)
{
	struct ae_result joined;

	(void)context;
	if (player < 1 || player >= AE_COOP_CONTROLLERS || !picked)
		return;
	joined = ae_lobby_join_controller(player);
	platform_log("ae coop lobby: player on controller %d joins: %s", player + 1, joined.ok ? "ok" : joined.reason);
	if (!joined.ok)
		ae_flow_error(&coop.flow, "The player did not join", joined.reason);
	else
		ae_sound_request(AE_SOUND_FORWARD, 0);
}

/* the seated players of this PC (those in the game), as count */
static short local_players(void)
{
	return (short)ae_lobby_local_players();
}

/* a controller's press for the seats; 1: taken */
static int seat_press(struct ae_event const *event)
{
	short controller = ae_input_event_controller();

	if (controller < 1 || controller >= AE_COOP_CONTROLLERS)
		return 0;
	if (ae_screen_lobby_profile_event(controller, event))
		return 1;
	if (event->action == AE_ACTION_START)
	{
		if (live() && !ae_lobby_controller_plays(controller) && local_players() + ae_screen_lobby_profile_choosing_count() <
			AE_COOP_PLAYERS_MAX && !ae_countdown_running(&coop.countdown))
		{
			if (!ae_screen_lobby_profile_open(controller, &coop.flow, seat_done, NULL))
				ae_sound_request(AE_SOUND_FAILURE, 0);
		}
		return 1;
	}
	if (event->action == AE_ACTION_BACK && ae_lobby_controller_plays(controller))
	{
		struct ae_result left = ae_lobby_leave_controller(controller);

		platform_log("ae coop lobby: player on controller %d leaves: %s", controller + 1, left.ok ? "ok" : left.reason);
		ae_sound_request(left.ok ? AE_SOUND_BACK : AE_SOUND_FAILURE, 0);
		return 1;
	}
	/* (another pad's buttons do nothing here: the menu is player 1's) */
	return 1;
}

/* the right column: the people in the game, this PC's pads leaving with B, the cards of those joining */
static void people_panel(float x, float y, float width)
{
	struct ae_flow_screen *flow = &coop.flow;
	struct ae_density const *d = &flow->density;
	float inset = ae_flow_u(flow, AE_FLOW_INSET_U), indent = ae_flow_u(flow, AE_PARTY_INDENT_U);
	float gap = ae_flow_u(flow, AE_FLOW_CARD_GAP_U), bottom = ae_flow_content_bottom(flow);
	float row_height = ae_party_row_height(d, AE_PARTY_PERSON);
	short index, drawn = 0, people = 0, shown = 0;
	short maximum = coop.have_roster && coop.roster.maximum_players > 0 ? coop.roster.maximum_players : AE_COOP_ONLINE_PLAYERS_MAX;
	char count[16];
	struct ae_party_row row;

	x += inset;
	width -= 2.0f * inset;
	if (coop.have_roster)
		people = coop.roster.people;
	snprintf(count, sizeof(count), "%d/%d", people, maximum);
	y += ae_widget_party_head(d, x, y, width, coop.joiner ? "In a co-op lobby" : "Hosting co-op", count, HIT_PARTY);
	for (index = 0; coop.have_roster && index < coop.roster.count; index++)
	{
		struct ae_lobby_player const *player = &coop.roster.players[index];
		float height;

		if (player->bot)
			continue;
		shown++;
		/* (rows that do not fit are counted under the last: the lobby holds 16 and the list can be long at 130 %) */
		if (y + row_height + gap > bottom - row_height)
			continue;
		memset(&row, 0, sizeof(row));
		row.kind = AE_PARTY_PERSON;
		snprintf(coop.names[index], sizeof(coop.names[index]), "%s", player->name);
		if (player->host)
			snprintf(coop.subs[index], sizeof(coop.subs[index]), "Host%s", player->local ? " \xE2\x80\xA2 this PC" : "");
		else if (player->local)
			snprintf(coop.subs[index], sizeof(coop.subs[index]), "This PC \xE2\x80\xA2 Controller %d", player->controller + 1);
		else
			snprintf(coop.subs[index], sizeof(coop.subs[index]), "In the lobby");
		row.name = coop.names[index];
		row.sub_line = coop.subs[index];
		row.color = player->color;
		row.slot = (short)(shown);
		row.badge = player->host ? "HOST" : NULL;
		/* (this PC's own pads, not player 1: B takes the player out) */
		if (player->local && player->controller >= 1)
		{
			float hint = ae_screen_lobby_profile_hint(d, AE_BUTTON_B, "Leave", 0.0f, 0.0f, 0);

			row.right_room = hint;
			height = ae_widget_party_row(d, x, y, width, &row, HIT_PARTY, index);
			ae_screen_lobby_profile_hint(d, AE_BUTTON_B, "Leave", x + width - ae_flow_u(flow, 16.0f) - hint,
				y + row_height * 0.5f, 1);
		}
		else
			height = ae_widget_party_row(d, x, y, width, &row, HIT_PARTY, index);
		y += height;
		drawn++;
	}
	if (shown > drawn)
	{
		snprintf(coop.more_text, sizeof(coop.more_text), "+ %d MORE", shown - drawn);
		y += ae_widget_party_section(d, x, y, width, coop.more_text);
	}
	/* (the cards of the pads choosing their profile, then the open card while this PC has a seat) */
	for (index = 1; index < AE_COOP_CONTROLLERS; index++)
	{
		float height = ae_screen_lobby_profile_card_draw(d, index, x + indent, y, width - indent, HIT_CARDS);

		if (height > 0.0f)
			y += height + gap;
	}
	if (live() && local_players() + ae_screen_lobby_profile_choosing_count() < AE_COOP_PLAYERS_MAX &&
		ae_screen_lobby_profile_choosing_count() == 0 && y + row_height < bottom)
		ae_widget_open_card(d, x + indent, y, width - indent, 0, HIT_CARDS);
}

/* ---------- starting */

static void start_press(void)
{
	struct ae_result result;
	long countdown_ms = config_integer("debug.ae_lobby_countdown_ms");

	if (!live() || ae_countdown_running(&coop.countdown))
	{
		ae_sound_request(AE_SOUND_FAILURE, 0);
		return;
	}
	result = countdown_ms > 0 ? ae_lobby_start_countdown(countdown_ms) : ae_lobby_start();
	if (!result.ok)
	{
		ae_flow_error(&coop.flow, "The game did not start", result.reason);
		return;
	}
	platform_log("ae coop lobby: start");
	if (countdown_ms > 0)
	{
		ae_countdown_begin(&coop.countdown, countdown_ms, system_milliseconds());
		coop.flow.focus = (short)(coop.flow.count - 1);
	}
	ae_sound_request(AE_SOUND_FORWARD, 0);
	/* (the screen stays up through the countdown; it leaves once the game runs: coop_update) */
	coop.host_started = 1;
}

static void cancel_press(void)
{
	struct ae_result result;

	if (coop.joiner)
	{
		note("The host started the game. Only the host can cancel it.");
		ae_sound_request(AE_SOUND_FAILURE, 0);
		return;
	}
	result = ae_lobby_cancel_start();
	if (!result.ok)
	{
		note(result.reason);
		ae_sound_request(AE_SOUND_FAILURE, 0);
		return;
	}
	ae_countdown_cancel(&coop.countdown, system_milliseconds());
	coop.host_started = 0;
	platform_log("ae coop lobby: start cancelled");
	ae_sound_request(AE_SOUND_BACK, 0);
}

/* the countdown followed from the seconds this machine's client has, each frame: each number logged and beeped */
static void countdown_follow(unsigned long now)
{
	short seconds = ae_lobby_owns_session() ? ae_lobby_seconds_to_start() : AE_COUNTDOWN_SECONDS_NONE;

	switch (ae_countdown_update(&coop.countdown, seconds, now))
	{
	case AE_COUNTDOWN_NUMBER:
	case AE_COUNTDOWN_ZERO:
		platform_log("ae lobby: countdown %d", (int)coop.countdown.seconds);
		ae_glue_sound_countdown(ae_countdown_beep(coop.countdown.seconds, 0), coop.countdown.seconds);
		break;
	case AE_COUNTDOWN_HOLD:
		platform_log("ae lobby: countdown held at 0 (machines loading the level)");
		break;
	case AE_COUNTDOWN_CANCELLED:
		platform_log("ae lobby: the server cancelled the countdown (NONE)");
		coop.host_started = 0;
		note(coop.joiner ? "The host cancelled the start." : "Start cancelled.");
		ae_sound_request(AE_SOUND_BACK, 0);
		break;
	default:
		break;
	}
	if (ae_countdown_running(&coop.countdown))
	{
		snprintf(coop.start_text, sizeof(coop.start_text), "STARTING IN %d", (int)coop.countdown.seconds);
		snprintf(coop.count_help, sizeof(coop.count_help), "{B} cancels the start.");
	}
}

static void leave_picked(short choice, void *context)
{
	(void)context;
	if (choice == 1)
		leave_now();
}

static void leave_or_ask(void)
{
	struct ae_dialog_spec spec;
	int others = coop.have_roster ? coop.roster.people - local_players() : 0;

	/* (the host alone, with this PC's players: nothing to ask) */
	if (!coop.joiner && others <= 0)
	{
		ae_sound_request(AE_SOUND_BACK, 0);
		leave_now();
		return;
	}
	memset(&spec, 0, sizeof(spec));
	spec.kind = AE_DIALOG_CONFIRM;
	spec.title = "Leave the lobby?";
	spec.body = coop.joiner ? "You leave the host's game." : "The game closes for everyone in it.";
	spec.choices[0] = "STAY";
	spec.choices[1] = "LEAVE";
	spec.choice_count = 2;
	spec.has_cancel = 1;
	spec.density = coop.flow.density;
	spec.bounds = coop.flow.frame.rect;
	spec.picked = leave_picked;
	ae_dialog_open(&spec, AE_OWNER_ANY);
}

/* ---------- changes the host makes */

static void difficulty_step(short direction)
{
	short next = ae_value_step(AE_CAMPAIGN_DIFFICULTIES, coop.difficulty, direction);
	struct ae_result result;

	if (coop.joiner || !live() || ae_countdown_running(&coop.countdown))
	{
		ae_sound_request(AE_SOUND_FAILURE, 0);
		return;
	}
	if (next == coop.difficulty)
	{
		ae_sound_request(AE_SOUND_FAILURE, 0);
		return;
	}
	result = ae_lobby_set_coop_difficulty(next);
	if (!result.ok)
	{
		note(result.reason);
		ae_sound_request(AE_SOUND_FAILURE, 0);
		return;
	}
	coop.difficulty = next;
	ae_sound_request(AE_SOUND_CURSOR, 0);
	platform_log("ae coop lobby: difficulty %s", ae_campaign_difficulty_name(next));
}

static void public_toggle(void)
{
	if (coop.joiner || coop.network != AE_COOP_NET_ONLINE || ae_countdown_running(&coop.countdown))
	{
		ae_sound_request(AE_SOUND_FAILURE, 0);
		return;
	}
	coop.public_game = !coop.public_game;
	ui_widget_port_cooperative_public_set(coop.public_game);
	level_texts();
	platform_log("ae coop lobby: public %s", coop.public_game ? "on" : "off");
	ae_sound_request(AE_SOUND_CURSOR, 0);
}

static void invite_copy(void)
{
	if (coop.joiner || !ae_invite_share_copy())
	{
		ae_sound_request(AE_SOUND_FAILURE, 0);
		return;
	}
	platform_log("ae coop lobby: invite copied");
	ae_sound_request(AE_SOUND_FORWARD, 0);
}

static void accept(short row)
{
	if (row < 0 || row >= coop.flow.count)
		return;
	switch (coop.flow.rows[row].kind)
	{
	case ROW_START:
		if (ae_countdown_running(&coop.countdown))
			cancel_press();
		else
			start_press();
		break;
	case ROW_DIFFICULTY:
		difficulty_step(1);
		break;
	case ROW_PUBLIC:
		public_toggle();
		break;
	case ROW_INVITE:
		invite_copy();
		break;
	case ROW_RULE_SKULLS:
	case ROW_RULE_SCORING:
	case ROW_RULE_COOP:
		/* (a guest reads the host's rules) */
		if (coop.joiner)
			break;
		ae_sound_request(AE_SOUND_FORWARD, 0);
		if (coop.flow.rows[row].kind == ROW_RULE_SKULLS)
			ae_screen_skulls_open();
		else if (coop.flow.rows[row].kind == ROW_RULE_SCORING)
			ae_screen_scoring_open(0, 1);
		else
			ae_screen_coop_options_open("CO-OP CAMPAIGN");
		break;
	case ROW_RULE_RANDOMIZER:
		ae_sound_request(AE_SOUND_FAILURE, 0);
		break;
	default:
		ae_sound_request(AE_SOUND_FAILURE, 0);
		break;
	}
}

/* ---------- the screen */

static void coop_draw(struct ae_screen *instance)
{
	struct ae_flow_screen *flow = &coop.flow;
	float x, width;

	(void)instance;
	ae_flow_full_density(flow);
	rows_make();
	ae_flow_title(flow, coop.joiner ? "CO-OP LOBBY" : "HOST CO-OP", coop.crumb);
	ae_flow_draw_rows(flow, flow->frame.rect.width * AE_FLOW_ROWS_WIDTH);
	x = flow->frame.rect.x + flow->frame.rect.width * AE_FLOW_PANEL_X;
	width = ae_flow_right_x(flow) - x;
	people_panel(x, ae_frame_y(&flow->frame, AE_FLOW_CONTENT_Y), width);
	flow->prompt_count = 0;
	if (ae_countdown_running(&coop.countdown))
		flow->prompts[flow->prompt_count++] = ae_flow_prompt(AE_BUTTON_B, coop.joiner ? ae_string(AE_STR_BACK) : "Cancel",
			AE_ACTION_BACK);
	else
	{
		short kind = focused_kind();

		if (!coop.joiner && kind != ROW_RULE_RANDOMIZER)
			flow->prompts[flow->prompt_count++] = ae_flow_prompt(AE_BUTTON_A, kind == ROW_START ? "Start game" :
				kind == ROW_INVITE ? "Copy invite" : (kind == ROW_RULE_SKULLS || kind == ROW_RULE_SCORING || kind == ROW_RULE_COOP) ? "Open" :
				"Change", AE_ACTION_ACCEPT);
		if (!coop.joiner && coop.network == AE_COOP_NET_ONLINE && kind != ROW_INVITE)
			flow->prompts[flow->prompt_count++] = ae_flow_prompt(AE_BUTTON_X, "Copy invite", AE_ACTION_X);
		flow->prompts[flow->prompt_count++] = ae_flow_prompt(AE_BUTTON_B, "Leave", AE_ACTION_BACK);
	}
	ae_flow_draw_prompts(flow);
}

static int coop_handle(struct ae_screen *instance, struct ae_event const *event)
{
	(void)instance;
	if (seat_press(event))
		return 1;
	if (ae_countdown_running(&coop.countdown))
	{
		/* (the host's input is locked through the count but for the cancel; a joiner's B is refused) */
		if (event->action == AE_ACTION_BACK)
			cancel_press();
		return 1;
	}
	switch (event->action)
	{
	case AE_ACTION_BACK:
		leave_or_ask();
		return 1;
	case AE_ACTION_UP:
	case AE_ACTION_DOWN:
		ae_flow_focus_step(&coop.flow, event->action == AE_ACTION_UP ? -1 : 1, event->repeat);
		return 1;
	case AE_ACTION_LEFT:
	case AE_ACTION_RIGHT:
		if (focused_kind() == ROW_DIFFICULTY)
			difficulty_step(event->action == AE_ACTION_RIGHT ? 1 : -1);
		else if (focused_kind() == ROW_PUBLIC)
			public_toggle();
		return 1;
	case AE_ACTION_X:
		invite_copy();
		return 1;
	case AE_ACTION_ACCEPT:
		accept(coop.flow.focus);
		return 1;
	default:
		return 1;
	}
}

static void coop_pointer(struct ae_screen *instance, struct ae_pointer const *pointer)
{
	struct ae_event event;
	struct ae_hit hit;
	short row;

	if (ae_prompts_pointer(coop.flow.prompts, coop.flow.prompt_count, pointer, HIT_PROMPTS, &event))
	{
		coop_handle(instance, &event);
		return;
	}
	/* (a card's profile row: hover focuses, a click picks) */
	if (ae_screen_lobby_profile_choosing_count() && ae_hit_at(pointer->x, pointer->y, 0, &hit) && hit.id == HIT_CARDS &&
		hit.part == AE_PART_ITEM)
	{
		ae_screen_lobby_profile_pointer(hit.index, pointer->moved, pointer->left_clicks);
		return;
	}
	if (ae_countdown_running(&coop.countdown))
		return;
	row = ae_flow_rows_pointer(&coop.flow, pointer);
	if (row == -2 || row == -3)
	{
		if (focused_kind() == ROW_DIFFICULTY)
			difficulty_step(row == -3 ? 1 : -1);
		else if (focused_kind() == ROW_PUBLIC)
			public_toggle();
	}
	else if (row >= 0)
		accept(row);
}

/* once a frame for the top screen */
static void coop_update(struct ae_screen *instance)
{
	unsigned long now = system_milliseconds();

	(void)instance;
	if (coop.step == STEP_JOINING)
		host_continue();
	roster_read(now);
	countdown_follow(now);
	/* (a joiner whose game is gone at the main menu: the host left, or the join failed: back to where it joined from) */
	if (!coop.leaving && coop.step != STEP_JOINING && !ae_lobby_owns_session() && main_menu_is_active() && !coop.staged)
	{
		platform_log("ae coop lobby: the game is gone");
		leave_now();
		return;
	}
	/* (a joiner, or the host after START: the game runs; no AE screen over the game, the take-over brings the lobby back) */
	if ((coop.joiner || coop.host_started) && game_engine_running() && !main_menu_is_active() && !game_engine_showing_postgame())
	{
		platform_log("ae coop lobby: the game runs");
		coop.host_started = 0;
		ae_countdown_reset(&coop.countdown);
		coop.starting = 1;
		clear_stack();
	}
}

static void coop_leave(struct ae_screen *instance)
{
	(void)instance;
	platform_log("ae coop lobby: %s", coop.leaving ? "left" : "off the stack");
	coop.leaving = 0;
	ae_screen_lobby_profile_cancel_all();
}

static struct ae_screen_class const coop_class =
{
	.name = "coop lobby", .leave = coop_leave, .handle = coop_handle, .draw = coop_draw, .pointer = coop_pointer,
	.update = coop_update, .start_from_anyone = 1,
};

/* ---------- the entries */

static int open_screen(void)
{
	coop.flow.hit_rows = HIT_ROWS;
	coop.flow.hit_prompts = HIT_PROMPTS;
	ae_lobby_set_pregame_screen(&coop_class, &coop);
	if (!ae_ui_push(&coop_class, AE_OWNER_ANY, &coop))
	{
		platform_log("ae coop lobby: could not open");
		return 0;
	}
	level_texts();
	coop.logged_people = -1;
	rows_make();
	/* (the host on START GAME, the last row; a joiner on WAITING, the last too) */
	coop.flow.focus = (short)(coop.flow.count - 1);
	return 1;
}

static int host_with(short level, short difficulty, unsigned pads, int network, int staged)
{
	struct ae_result result;
	int busy = game_engine_running() && !main_menu_is_active();

	if (busy)
		return AE_COOP_HOST_ERR_IN_GAME;
	ae_screen_lobby_profile_cancel_all();
	memset(&coop, 0, sizeof(coop));
	coop.level = level;
	coop.difficulty = difficulty;
	coop.pads = pads;
	coop.network = network;
	coop.public_game = network == AE_COOP_NET_ONLINE && ui_widget_port_cooperative_public();
	coop.staged = staged;
	/* (the game's own hosting, as CREATE GAME's: ae_lobby_host closes the classic menus, makes the server and this PC's
	client; ONLINE follows OpenCE's saved co-op PUBLIC choice once the level is set) */
	result = ae_lobby_host(network == AE_COOP_NET_ONLINE ? AE_LOBBY_ONLINE : AE_LOBBY_LAN);
	if (!result.ok)
	{
		platform_log("ae coop lobby: open failed: %s", result.reason);
		return AE_COOP_HOST_ERR_HOST;
	}
	p2p_set_hosting_password(NULL);
	coop.step = STEP_JOINING;
	coop.since = system_milliseconds();
	platform_log("ae coop lobby: host level %d (%s)", level + 1, network == AE_COOP_NET_ONLINE ?
		(coop.public_game ? "online, public" : "online, invite only") : "LAN");
	if (!open_screen())
	{
		ae_lobby_leave();
		return AE_COOP_HOST_ERR_HOST;
	}
	return AE_COOP_HOST_ERR_NONE;
}

int ae_screen_coop_lobby_host(short level, short difficulty, unsigned pads, struct ae_flow_screen const *flow)
{
	int error;

	(void)flow;
	if (level < 0 || level >= AE_CAMPAIGN_LEVELS)
		return AE_COOP_HOST_ERR_LEVEL;
	error = host_with(level, difficulty < 0 ? 1 : difficulty > 3 ? 3 : difficulty, pads,
		ae_coop_host_network(config_boolean("network.online")), 0);
	return error;
}

int ae_screen_coop_lobby_open_staged(int players)
{
	unsigned pads = 0;
	short controller;

	/* (guests on controllers 2 up, as pads' cards would seat them) */
	for (controller = 1; controller < players && controller < AE_COOP_CONTROLLERS; controller++)
	{
		if (!ae_profile_guest(controller).ok)
			return 0;
		pads |= 1u << controller;
	}
	if (ae_ui_holds(&coop))
		ae_ui_remove(&coop);
	return host_with(0, 1, pads, AE_COOP_NET_LAN, 1) == AE_COOP_HOST_ERR_NONE;
}

int ae_screen_coop_lobby_joined(const char *how)
{
	short level = 0, difficulty = 1;

	ae_screen_lobby_profile_cancel_all();
	memset(&coop, 0, sizeof(coop));
	coop.joiner = 1;
	coop.step = STEP_LIVE;
	ae_lobby_coop_game(&level, &difficulty);
	coop.level = level;
	coop.difficulty = difficulty;
	snprintf(coop.how, sizeof(coop.how), "%s", how ? how : "");
	if (!open_screen())
		return 0;
	platform_log("ae coop lobby: joined (level %d, %s)", level + 1, ae_campaign_difficulty_name(difficulty));
	return 1;
}
