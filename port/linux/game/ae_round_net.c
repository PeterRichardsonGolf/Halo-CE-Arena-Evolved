/*
AE_ROUND_NET.C

The round modes' state on the network and on screen. The host makes its view of the round (ae_round_wire.h) every
tick and sends it to every client reliably as distributed message 106 when it changes, and every two seconds anyway
(a revive's progress in sixteenths, so a hold sends at most sixteen). A client keeps the last whole one: it freezes the
countdown, counts the lives within the round and follows a teammate from it. A build that does not know the kind drops
it (only Arena Evolved machines play a round mode: ae_silent.c refuses others).

DEV MODE draws one line of it (the round, who is alive, the zone, the downed): a stand-in for the HUD that is to come.
*/

#include "cseries.h"
#include "game/game.h"
#include "game/game_engine.h"
#include "game/players.h"
#include "interface/hud_messaging.h"
#include "interface/ui_widget.h"
#include "main/main.h"
#include "memory/data.h"
#include "networking/network_game_globals.h"
#include "rasterizer/rasterizer.h"
#include "render/render.h"
#include "text/draw_string.h"
#include "text/font_group.h"
#include "text/unicode.h"

#include "network_distributed.h"
#include "ae_map_share.h"
#include "../src/delta_peer.h"
#include "halo_port_limits.h"
#include "ae_dev_mode.h"
#include "ae_round_card_rules.h"
#include "ae_round_internal.h"
#include "ae_variant_ext.h"

#include <stdio.h>
#include <string.h>

enum
{
	/* the host sends its view this often when nothing changes */
	SEND_EVERY_TICKS = 2 * TICKS_PER_SECOND,
};

struct ae_round_message
{
	struct distributed_message_header header;
	unsigned char body[AE_ROUND_WIRE_BUFFER];
};

static struct ae_round_wire client_view;
static boolean client_view_valid;
/* (a client's: it saw the round before this POST, so the card is its to show; a machine that joined during POST has none) */
static boolean client_card_armed;

/* the machines' last full body (with the END tail), so one machine is sent it once per change */
static unsigned char end_sent[AE_ROUND_WIRE_BUFFER];
static int end_sent_size;

struct ae_round_wire const *ae_round_view(void)
{
	if (network_game_distributed_client())
		return client_view_valid ? &client_view : NULL;
	return ae_round_host.running ? &ae_round_host.view : NULL;
}

static unsigned char result_byte(short result)
{
	if (result == AE_ROUND_RESULT_TIE)
		return AE_ROUND_WIRE_RESULT_TIE;
	return result >= 0 && result < AE_ROUND_WIRE_PLAYERS ? (unsigned char)result : AE_ROUND_WIRE_RESULT_NONE;
}

static unsigned char seconds_byte(int seconds)
{
	return (unsigned char)(seconds < 0 ? 0 : seconds > 255 ? 255 : seconds);
}

static void view_make(struct ae_round_wire *view)
{
	struct ae_round_host const *host = &ae_round_host;
	int tenths = ae_round_revive_tenths(ae_variant_ext_game());
	int bleed_seconds = ae_round_bleedout_seconds(ae_variant_ext_game());
	long index;

	memset(view, 0, sizeof(*view));
	view->phase = (unsigned char)host->phase;
	view->round = (unsigned char)(host->round > 255 ? 255 : host->round);
	view->flags = (unsigned char)((host->teams ? AE_ROUND_WIRE_TEAMS : 0) | (host->switched ? AE_ROUND_WIRE_SWITCHED : 0) |
		(host->zone && host->zone_ready && !host->zone_disabled ? AE_ROUND_WIRE_ZONE : 0) |
		(host->revive ? AE_ROUND_WIRE_REVIVE : 0));
	view->wins_red = (unsigned char)(host->team_wins[_team_red] > 255 ? 255 : host->team_wins[_team_red]);
	view->wins_blue = (unsigned char)(host->team_wins[_team_blue] > 255 ? 255 : host->team_wins[_team_blue]);
	view->last_result = result_byte(host->last_result);
	view->zone_phase = (unsigned char)(host->zone_phase >= 0 && host->zone_phase < AE_ZONE_PHASE_COUNT ?
		host->zone_phase : 0);
	view->max_rounds = (unsigned char)(host->max_rounds > 255 ? 255 : host->max_rounds);
	view->to_win = (unsigned char)(host->to_win > 255 ? 255 : host->to_win);
	view->round_start = host->round_start;
	view->live_start = host->live_start != NONE ? host->live_start : 0;
	view->post_end = host->post_end != NONE ? host->post_end : 0;
	view->zone_start = host->zone_start;
	if (ae_round_kind_respawns(host->kind))
	{
		int index;
		long red = 0, blue = 0;

		view->flags |= AE_ROUND_WIRE_EXTRA;
		view->pool_red = (unsigned char)(host->pool[0] > 255 ? 255 : host->pool[0] < 0 ? 0 : host->pool[0]);
		view->pool_blue = (unsigned char)(host->pool[1] > 255 ? 255 : host->pool[1] < 0 ? 0 : host->pool[1]);
		view->zone_wait_2_seconds = (unsigned char)(host->zone_plan.wait_2_seconds > 255 ? 255 :
			host->zone_plan.wait_2_seconds);
		(void)index;
		(void)red;
		(void)blue;
		view->kills_red = (unsigned char)ae_round_net_side_kills(0);
		view->kills_blue = (unsigned char)ae_round_net_side_kills(1);
	}
	/* (0.7.9) the round card's tail, in POST only: why it ended and who made the most kills) */
	if (host->phase == AE_ROUND_PHASE_POST && host->card)
	{
		view->flags |= AE_ROUND_WIRE_END;
		view->reason = (unsigned char)(host->card_reason >= 0 && host->card_reason < AE_ROUND_REASON_COUNT ?
			host->card_reason : 0);
		view->mvp_player = host->card_mvp >= 0 && host->card_mvp < AE_ROUND_WIRE_PLAYERS ? (unsigned char)host->card_mvp :
			AE_ROUND_WIRE_NO_PLAYER;
		view->mvp_kills = view->mvp_player == AE_ROUND_WIRE_NO_PLAYER ? 0 : (unsigned char)host->card_mvp_kills;
	}
	if (host->zone_ready)
	{
		view->zone_centre[0] = ae_round_wire_coordinate(host->zone_centre.x);
		view->zone_centre[1] = ae_round_wire_coordinate(host->zone_centre.y);
		view->zone_centre[2] = ae_round_wire_coordinate(host->zone_centre.z);
		view->zone_radius = ae_round_wire_radius(host->zone_plan.start_radius);
		view->zone_wait_seconds = seconds_byte(host->zone_plan.wait_seconds);
		view->zone_closing_seconds = seconds_byte(host->zone_plan.closing_seconds);
		view->zone_closing_2_seconds = seconds_byte(host->zone_plan.closing_2_seconds);
	}
	for (index = 0; index < AE_ROUND_SLOTS; index++)
	{
		struct ae_round_slot const *slot = &host->slots[index];
		struct ae_round_wire_player *entry;
		long needed;

		if (slot->player_index == NONE || view->count >= AE_ROUND_WIRE_PLAYERS)
			continue;
		entry = &view->players[view->count++];
		entry->player = (unsigned char)index;
		entry->state = (unsigned char)(slot->state >= 0 && slot->state < AE_ROUND_PLAYER_STATE_COUNT ? slot->state : 0);
		entry->revives = (unsigned char)(slot->revives > 255 ? 255 : slot->revives);
		needed = ae_round_revive_ticks(tenths, slot->revives);
		/* (in sixteenths: a hold sends at most sixteen changes) */
		entry->progress = (unsigned char)((slot->progress >= needed ? 255 : slot->progress * 256 / needed) & 0xF0);
		entry->reviver = slot->reviver != NONE ? (unsigned char)DATUM_INDEX_TO_ABSOLUTE_INDEX(slot->reviver) :
			AE_ROUND_WIRE_NO_PLAYER;
		if (entry->reviver >= AE_ROUND_WIRE_PLAYERS)
			entry->reviver = AE_ROUND_WIRE_NO_PLAYER;
		entry->wins = (unsigned char)(slot->wins > 255 ? 255 : slot->wins);
		if (slot->state == AE_ROUND_PLAYER_DOWN && bleed_seconds > 0)
		{
			long left = (long)bleed_seconds * TICKS_PER_SECOND - (game_time_get() - slot->down_since);

			/* (a second a step, up: 1 until the last tick) */
			entry->bleed = (unsigned char)(left <= 0 ? 1 : (left + TICKS_PER_SECOND - 1) / TICKS_PER_SECOND);
		}
		if (slot->state == AE_ROUND_PLAYER_DOWN || slot->state == AE_ROUND_PLAYER_OUT ||
			slot->state == AE_ROUND_PLAYER_REVIVED)
		{
			entry->spot[0] = ae_round_wire_coordinate(slot->spot.x);
			entry->spot[1] = ae_round_wire_coordinate(slot->spot.y);
			entry->spot[2] = ae_round_wire_coordinate(slot->spot.z);
		}
	}
}

/* the view as every machine reads it: without the END flag (a build before the card drops a body with a flag it does not
know) */
static void plain_view(struct ae_round_wire const *view, struct ae_round_wire *plain)
{
	*plain = *view;
	plain->flags &= (unsigned char)~AE_ROUND_WIRE_END;
	plain->reason = 0;
	plain->mvp_player = 0;
	plain->mvp_kills = 0;
}

static void send_body(long machine_index, unsigned char const *body, int size)
{
	struct ae_round_message message;

	memset(&message, 0, sizeof(message));
	memcpy(message.body, body, (size_t)size);
	if (machine_index == NONE)
	{
		distributed_send(&message, _distributed_message_ae_round, 1, (word)(sizeof(message.header) + size),
			_distributed_to_clients_reliably);
		memcpy(ae_round_host.sent, message.body, (size_t)size);
		ae_round_host.sent_size = size;
	}
	else
		distributed_send_to_machine_reliably(machine_index, &message, _distributed_message_ae_round, 1,
			(word)(sizeof(message.header) + size));
}

static void send_view(long machine_index)
{
	struct ae_round_wire plain;
	unsigned char body[AE_ROUND_WIRE_BUFFER];
	int size;

	plain_view(&ae_round_host.view, &plain);
	size = ae_round_wire_encode(&plain, body, sizeof(body));
	if (size)
		send_body(machine_index, body, size);
}

short network_distributed_server_machines(long *machine_indices, short maximum);

/* whether a machine is an Arena Evolved build that reads the END tail */
static boolean machine_has_card(long machine_index)
{
	char build[48];

	if (!ae_peer_is_ae(machine_index))
		return FALSE;
	return delta_peer_game_machine_build((int)machine_index, build, (int)sizeof(build)) > 0 &&
		ae_round_build_has_card(build);
}

/* POST's full body (the END tail) to each machine that reads it, once per change; the others have had the plain one */
static void send_end(void)
{
	unsigned char body[AE_ROUND_WIRE_BUFFER];
	long machines[HALO_PORT_MAXIMUM_NETWORK_MACHINES];
	short count, index;
	int size;

	if (!(ae_round_host.view.flags & AE_ROUND_WIRE_END))
	{
		end_sent_size = 0;
		return;
	}
	size = ae_round_wire_encode(&ae_round_host.view, body, sizeof(body));
	if (!size || (size == end_sent_size && !memcmp(body, end_sent, (size_t)size)))
		return;
	count = network_distributed_server_machines(machines, HALO_PORT_MAXIMUM_NETWORK_MACHINES);
	for (index = 0; index < count; index++)
		if (machine_has_card(machines[index]))
			send_body(machines[index], body, size);
	memcpy(end_sent, body, (size_t)size);
	end_sent_size = size;
}

void ae_round_net_host_tick(long now)
{
	unsigned char bytes[AE_ROUND_WIRE_BUFFER];
	struct ae_round_wire plain;
	int size;

	view_make(&ae_round_host.view);
	if (game_connection() != _game_connection_network_server || main_menu_is_active())
		return;
	plain_view(&ae_round_host.view, &plain);
	size = ae_round_wire_encode(&plain, bytes, sizeof(bytes));
	if (!size)
		return;
	if (size == ae_round_host.sent_size && !memcmp(bytes, ae_round_host.sent, (size_t)size) &&
		ae_round_host.sent_at != NONE && now - ae_round_host.sent_at < SEND_EVERY_TICKS && now >= ae_round_host.sent_at)
	{
		send_end();
		return;
	}
	ae_round_host.sent_at = now;
	send_view(NONE);
	send_end();
}

void ae_round_send(long machine_index)
{
	if (game_connection() != _game_connection_network_server || !ae_round_host.running || !ae_round_on())
		return;
	send_view(machine_index);
}

void ae_round_net_reset(void)
{
	memset(&client_view, 0, sizeof(client_view));
	client_view_valid = FALSE;
	client_card_armed = FALSE;
	end_sent_size = 0;
}

int ae_round_handles_message(unsigned short const *message, unsigned short size)
{
	struct distributed_message_header header;

	if (size < sizeof(header))
		return 0;
	memcpy(&header, message, sizeof(header));
	return header.type == _distributed_message_ae_round;
}

/* (the host's alone, over its stream, whole: a body the reader takes, every field in range) */
void ae_round_handle_message(long machine_index, unsigned short const *data, unsigned short size)
{
	struct ae_round_message message;
	struct ae_round_wire read;

	if (machine_index != NONE || game_connection() != _game_connection_network_client ||
		network_connection_last_read_was_unreliable() || size < sizeof(message.header) + AE_ROUND_WIRE_HEAD ||
		size > sizeof(message))
	{
		return;
	}
	memcpy(&message, data, size);
	if (GET_MESSAGE_TYPE(message.header.header) != 2 || message.header.type != _distributed_message_ae_round ||
		message.header.count != 1)
	{
		return;
	}
	if (!ae_round_wire_decode(message.body, (int)(size - sizeof(message.header)), &read))
		return;
	/* (the card is this machine's when it saw the round go on before this POST) */
	if (read.phase != AE_ROUND_PHASE_POST)
		client_card_armed = TRUE;
	else if (!client_view_valid)
		client_card_armed = FALSE;
	/* (the host's two-second repeat comes without the END tail, the machine has had it once: keep it for the POST) */
	if (client_view_valid && read.phase == AE_ROUND_PHASE_POST && !(read.flags & AE_ROUND_WIRE_END) &&
		client_view.phase == AE_ROUND_PHASE_POST && (client_view.flags & AE_ROUND_WIRE_END) && read.round == client_view.round)
	{
		read.flags |= AE_ROUND_WIRE_END;
		read.reason = client_view.reason;
		read.mvp_player = client_view.mvp_player;
		read.mvp_kills = client_view.mvp_kills;
	}
	client_view = read;
	client_view_valid = TRUE;
}

/* the view the card is drawn from: the host's own, a client's when it saw the round before this POST (else NULL) */
struct ae_round_wire const *ae_round_card_view(void)
{
	struct ae_round_wire const *view = ae_round_view();

	if (!view || !(view->flags & AE_ROUND_WIRE_END) || view->phase != AE_ROUND_PHASE_POST)
		return NULL;
	if (network_game_distributed_client() && !client_card_armed)
		return NULL;
	return view;
}

/* ---------- DEV MODE's line */

static struct player_datum *player_of_slot(long slot)
{
	struct data_iterator iterator;
	struct player_datum *player;

	data_iterator_new(&iterator, player_data);
	while ((player = (struct player_datum *)data_iterator_next(&iterator)) != NULL)
	{
		if (DATUM_INDEX_TO_ABSOLUTE_INDEX(iterator.datum_index) == slot)
			return player;
	}
	return NULL;
}

static void line_make(struct ae_round_wire const *view, char *text, int size)
{
	long now = game_time_get();
	int alive[2] = { 0, 0 }, players = 0, standing = 0, index;
	int used;
	boolean teams = (view->flags & AE_ROUND_WIRE_TEAMS) != 0;

	for (index = 0; index < view->count; index++)
	{
		struct ae_round_wire_player const *entry = &view->players[index];
		struct player_datum *player = player_of_slot(entry->player);
		boolean up = entry->state == AE_ROUND_PLAYER_ALIVE || entry->state == AE_ROUND_PLAYER_REVIVED;

		if (!player || entry->state == AE_ROUND_PLAYER_WAITING)
			continue;
		players++;
		standing += up;
		if (up && (player->team_index == _team_red || player->team_index == _team_blue))
			alive[player->team_index]++;
	}
	used = snprintf(text, size, "ROUND %d/%d %s", view->round, view->max_rounds, ae_round_phase_name(view->phase));
	if (view->phase == AE_ROUND_PHASE_PRE)
		used += snprintf(text + used, size > used ? size - used : 0, " %ld",
			(ae_round_countdown_left(view->round_start, now) + TICKS_PER_SECOND - 1) / TICKS_PER_SECOND);
	if (teams)
		used += snprintf(text + used, size > used ? size - used : 0, " | RED %d won, %d alive | BLUE %d won, %d alive%s",
			view->wins_red, alive[0], view->wins_blue, alive[1], view->flags & AE_ROUND_WIRE_SWITCHED ? " | SWITCHED" : "");
	else
		used += snprintf(text + used, size > used ? size - used : 0, " | %d of %d alive", standing, players);
	if ((view->flags & AE_ROUND_WIRE_ZONE) && view->phase == AE_ROUND_PHASE_LIVE && now >= view->zone_start)
	{
		struct ae_zone_plan plan;
		long elapsed = now - view->zone_start;

		plan.start_radius = (real)view->zone_radius / 16.0f;
		plan.mid_radius = plan.start_radius * AE_ZONE_MID_FRACTION;
		plan.wait_seconds = view->zone_wait_seconds;
		plan.wait_2_seconds = (view->flags & AE_ROUND_WIRE_EXTRA) ? view->zone_wait_2_seconds : AE_ZONE_WAIT_2_SECONDS;
		plan.closing_seconds = view->zone_closing_seconds;
		plan.closing_2_seconds = view->zone_closing_2_seconds;
		used += snprintf(text + used, size > used ? size - used : 0, " | ZONE %s %lds r%.0f",
			ae_zone_phase_at(&plan, elapsed) == AE_ZONE_PHASE_CLOSING ||
			ae_zone_phase_at(&plan, elapsed) == AE_ZONE_PHASE_CLOSING_2 ? "CLOSING" :
			ae_zone_phase_at(&plan, elapsed) == AE_ZONE_PHASE_CLOSED ? "CLOSED" : "WAIT",
			ae_zone_phase_ticks_left(&plan, elapsed) / TICKS_PER_SECOND, ae_zone_radius_at(&plan, elapsed));
	}
	for (index = 0; index < view->count; index++)
	{
		struct ae_round_wire_player const *entry = &view->players[index];
		struct player_datum *player;
		char name[12];
		int letter;

		if (entry->state != AE_ROUND_PLAYER_DOWN || !(player = player_of_slot(entry->player)))
			continue;
		for (letter = 0; letter < 11 && player->name[letter]; letter++)
			name[letter] = player->name[letter] >= 32 && player->name[letter] < 127 ? (char)player->name[letter] : '?';
		name[letter] = 0;
		used += snprintf(text + used, size > used ? size - used : 0, " | DOWN %s %d%%", name, entry->progress * 100 / 255);
	}
	if (view->phase == AE_ROUND_PHASE_POST || view->phase == AE_ROUND_PHASE_OVER)
		used += snprintf(text + used, size > used ? size - used : 0, " | %s",
			view->last_result == AE_ROUND_WIRE_RESULT_TIE ? "TIED" : view->last_result == _team_red && teams ? "RED WON" :
			view->last_result == _team_blue && teams ? "BLUE WON" : "WON");
	(void)used;
}

boolean ae_round_strip_state(long viewer_player_index, long viewer_team, struct ae_round_strip *out)
{
	struct ae_round_wire const *view = ae_round_view();
	boolean teams;
	int index, best_other = 0;

	if (!out || !ae_round_on() || !view || view->phase == AE_ROUND_PHASE_OFF)
		return FALSE;
	memset(out, 0, sizeof(*out));
	teams = (view->flags & AE_ROUND_WIRE_TEAMS) != 0 && (viewer_team == _team_red || viewer_team == _team_blue);
	out->teams = teams;
	out->round = view->round;
	out->max_rounds = view->max_rounds;
	out->to_win = view->to_win;
	if (teams)
	{
		out->wins_me = viewer_team == _team_red ? view->wins_red : view->wins_blue;
		out->wins_other = viewer_team == _team_red ? view->wins_blue : view->wins_red;
	}
	for (index = 0; index < view->count; index++)
	{
		struct ae_round_wire_player const *entry = &view->players[index];
		struct player_datum *player = player_of_slot(entry->player);
		boolean up = entry->state == AE_ROUND_PLAYER_ALIVE || entry->state == AE_ROUND_PLAYER_REVIVED ||
			entry->state == AE_ROUND_PLAYER_NONE;
		boolean mine;

		if (!player || entry->state == AE_ROUND_PLAYER_WAITING)
			continue;
		mine = teams ? player->team_index == viewer_team :
			DATUM_INDEX_TO_ABSOLUTE_INDEX(viewer_player_index) == entry->player;
		if (mine)
		{
			out->players_me++;
			out->alive_me += up;
			if (!teams)
				out->wins_me = entry->wins;
		}
		else
		{
			out->players_other++;
			out->alive_other += up;
			if (!teams && entry->wins > best_other)
				best_other = entry->wins;
		}
	}
	if (!teams)
		out->wins_other = best_other;
	return TRUE;
}

boolean ae_round_player_out(long player_index)
{
	struct ae_round_wire const *view = ae_round_view();
	int index;

	if (!ae_round_on() || !view || (view->phase != AE_ROUND_PHASE_LIVE && view->phase != AE_ROUND_PHASE_POST) ||
		player_index == NONE)
	{
		return FALSE;
	}
	for (index = 0; index < view->count; index++)
	{
		struct ae_round_wire_player const *entry = &view->players[index];

		if (entry->player == DATUM_INDEX_TO_ABSOLUTE_INDEX(player_index))
			return entry->state == AE_ROUND_PLAYER_DOWN || entry->state == AE_ROUND_PLAYER_OUT ||
				entry->state == AE_ROUND_PLAYER_WAITING;
	}
	return FALSE;
}

long ae_round_player_scoreboard_state(long player_index)
{
	struct ae_round_wire const *view = ae_round_view();
	int index;

	if (!ae_round_on() || !view || (view->phase != AE_ROUND_PHASE_LIVE && view->phase != AE_ROUND_PHASE_POST) ||
		player_index == NONE)
	{
		return 0;
	}
	for (index = 0; index < view->count; index++)
	{
		struct ae_round_wire_player const *entry = &view->players[index];

		if (entry->player == DATUM_INDEX_TO_ABSOLUTE_INDEX(player_index))
			return entry->state == AE_ROUND_PLAYER_DOWN ? 1 : entry->state == AE_ROUND_PLAYER_OUT ? 2 : 0;
	}
	return 0;
}

void ae_round_debug_draw(short local_player_index)
{
	struct ae_round_wire const *view;
	long font_index = hud_get_font_index();
	struct font_header *font;
	real_argb_color color = *global_real_argb_white;
	rectangle2d bounds;
	char text[200];
	wchar_t wide[200];
	short height;
	int index;

	(void)local_player_index;
	if (!ae_dev_mode_on() || !ae_round_on() || font_index == NONE || !(view = ae_round_view()))
		return;
	line_make(view, text, sizeof(text));
	for (index = 0; text[index] && index < (int)NUMBEROF(wide) - 1; index++)
		wide[index] = (wchar_t)(unsigned char)text[index];
	wide[index] = 0;
	/* one line, centered, above the follow camera's line near the bottom of the view, in the HUD's font */
	font = font_definition_get(font_index);
	height = (short)(font->ascending_height + font->descending_height);
	bounds = render.camera.window_bounds;
	bounds.y0 = (short)(bounds.y1 - 2 * height - 32);
	bounds.y1 = (short)(bounds.y1 - height - 32);
	color.alpha = 0.85f;
	color.green = 0.95f;
	color.blue = 0.6f;
	draw_string_set_draw_mode(font_index, NONE, 2, 0, &color);
	rasterizer_draw_unicode_string(&bounds, NULL, NULL, 0, wide);
}
