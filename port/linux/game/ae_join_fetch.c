/*
AE_JOIN_FETCH.C

See ae_join_fetch.h. The rules are ae_join_fetch_rules.c's; Steam's side is the client's (ae_steam_client.h).
*/

#include <stdio.h>
#include <string.h>

#include "cseries.h"
#include "game/game.h"
#include "networking/network_game_globals.h"
#include "networking/network_game_manager.h"
#include "ae_glue_mcc_maps.h"
#include "ae_join_fetch.h"
#include "ae_join_fetch_rules.h"
#include "ae_map_check.h"
#include "ae_map_library.h"
#include "ae_map_share.h"
#include "ae_server_lobby.h"
#include "mcc_maps.h"
#include "../src/ae_mcc.h"
#include "../src/ae_steam_client.h"
#include "ae_steam_rules.h"

void platform_log(char const *format, ...);
int config_boolean(const char *name);
long config_integer(const char *name);
void ae_chat_local_notice(char const *text);

enum
{
	SIGN_IN_WAIT_MILLISECONDS = 30000,
	BUSY_RETRY_MILLISECONDS = 2000,
	BUSY_RETRIES = 60,
	WATCH_FIRST_MILLISECONDS = 3000,
	WATCH_LAST_MILLISECONDS = 15000
};

/* (the STEAM_AE_FETCH_ reasons of ae_steam_downloads.h, which the game's files do not include) */
enum
{
	CLIENT_RUNNING = 1, CLIENT_DONE = 2, CLIENT_FAILED = -1,
	CLIENT_WHY_BUSY = 1, CLIENT_WHY_NOT_SIGNED_IN, CLIENT_WHY_REFUSED, CLIENT_WHY_NO_ROOM, CLIENT_WHY_TOO_LARGE,
	CLIENT_WHY_LIMIT_BYTES, CLIENT_WHY_NOT_MULTIPLAYER, CLIENT_WHY_ALREADY, CLIENT_WHY_CANCELLED
};

/* a Workshop map this connection needs: the identity as the server wrote it, and its item */
struct item
{
	char name[AE_PL_MAP_NAME];
	unsigned long long id;
};

/* what Steam gave each server this run (not reset by a new connection) */
static struct ae_jf_servers servers;

static struct
{
	/* the last 112 question's table, kept to answer again as maps land */
	unsigned serial;
	int table_valid;
	char names[AE_PL_MAPS][AE_PL_MAP_NAME];
	int count;
	/* this connection's: the items needed (in order), the next to start, the ones started or refused */
	struct item queue[AE_JF_ITEMS_MAXIMUM];
	int queue_count, queue_next;
	unsigned long long tried[AE_JF_ITEMS_MAXIMUM];
	int tried_count;
	int planned;
	unsigned warned;
	/* the item going */
	int running;
	struct item running_item;
	char title[96];
	unsigned long long done, size;
	unsigned long waiting_since;
	unsigned long retry_at;
	int retries;
	/* the Workshop items of the server's table looked at this connection */
	unsigned long long seen[AE_PL_MAPS];
	int seen_count;
	unsigned long watch_at, watch_gap;
	int was_client;
	/* bytes taken but not yet counted against the host's budget (the map share had not taken note of this host) */
	unsigned long long uncharged;
} F;

static void reset(void)
{
	if (F.running)
		ae_steam_client_fetch_cancel();
	memset(&F, 0, sizeof(F));
}

/* control bytes become '?' (a Steam title, which may hold UTF-8, before it is shown) */
static void clean(
	char *text)
{
	for (; *text; text++)
		if ((unsigned char)*text < 32 || (unsigned char)*text == 127)
			*text = '?';
}

/* printable ASCII only: anything else becomes '?' (a map name a server sent, before it is logged or shown) */
static void clean_name(
	char *text)
{
	for (; *text; text++)
		if ((unsigned char)*text < 32 || (unsigned char)*text >= 127)
			*text = '?';
}

static void say(
	char const *text)
{
	platform_log("join fetch: %s", text);
	ae_chat_local_notice(text);
}

/* a reason told once in a connection (the bit is the reason's) */
static void say_why(
	int why)
{
	char text[200];

	if (why <= 0 || why > 30 || (F.warned & (1u << why)))
		return;
	F.warned |= 1u << why;
	ae_jf_why_text(why, text, sizeof(text));
	if (text[0])
		say(text);
}

static int tried(
	unsigned long long id)
{
	int at;

	for (at = 0; at < F.tried_count; at++)
		if (F.tried[at] == id)
			return 1;
	return 0;
}

static void tried_add(
	unsigned long long id)
{
	if (!tried(id) && F.tried_count < AE_JF_ITEMS_MAXIMUM)
		F.tried[F.tried_count++] = id;
}

static int allowed(void)
{
	return config_boolean("network.map_download_steam") != 0 && ae_map_share_all_policy_allows();
}

/* the maps held now of the 112 table, told to the server again */
static void tell_again(void)
{
	if (F.table_valid)
		ae_map_check_send_update(F.serial, F.names, F.count);
}

static void need_add(
	char const *identity);

void ae_join_fetch_offer(
	unsigned serial,
	char const names[][AE_PL_MAP_NAME],
	int count,
	unsigned char const *held_bits,
	int has_mcc)
{
	int at;
	struct network_game *game = network_game_get_game();

	(void)held_bits;
	(void)has_mcc;
	if (count < 1 || count > AE_PL_MAPS || game_connection() != _game_connection_network_client)
		return;
	F.was_client = 1;
	F.serial = serial;
	F.count = count;
	F.table_valid = 1;
	memcpy(F.names, names, sizeof(F.names[0]) * (size_t)count);
	for (at = 0; at < count; at++)
		F.names[at][AE_PL_MAP_NAME - 1] = 0;
	/* (what the server's table lists and this PC lacks: the game's map first when it is in the table, then the table's
	order) */
	if (game)
		for (at = 0; at < count; at++)
			if (!strcmp(game->map.name, F.names[at]))
				need_add(F.names[at]);
	for (at = 0; at < count; at++)
		need_add(F.names[at]);
}

/* a map of the lobby's needs added to the queue: a Workshop map this PC lacks, not asked for before */
static void need_add(
	char const *identity)
{
	unsigned long long id;
	char name[AE_PL_MAP_NAME], shown[64];
	int at;

	if (!identity || !identity[0])
		return;
	snprintf(name, sizeof(name), "%s", identity);
	if (!ae_jf_item_of(name, &id))
		return;
	for (at = 0; at < F.seen_count; at++)
		if (F.seen[at] == id)
			return;
	if (F.seen_count >= AE_PL_MAPS)
		return;
	F.seen[F.seen_count++] = id;
	if (ae_mcc_map_present(name) || tried(id))
		return;
	for (at = 0; at < F.queue_count; at++)
		if (F.queue[at].id == id)
			return;
	if (F.queue_count >= AE_JF_ITEMS_MAXIMUM || F.planned >= AE_JF_ITEMS_MAXIMUM)
	{
		say_why(AE_JF_LIMIT_ITEMS);
		return;
	}
	snprintf(F.queue[F.queue_count].name, AE_PL_MAP_NAME, "%s", name);
	F.queue[F.queue_count].id = id;
	F.queue_count++;
	F.planned++;
	snprintf(shown, sizeof(shown), "%.48s", name);
	clean_name(shown);
	platform_log("join fetch: %s needed (item %llu)", shown, id);
}

int ae_join_fetch_progress(
	char *title,
	int size,
	unsigned long long *done,
	unsigned long long *total)
{
	struct network_game *game = network_game_get_game();

	/* (only the map being loaded owns the lobby's download card; the others are told in the notice line) */
	if (!F.running || !game || strcmp(game->map.name, F.running_item.name))
		return 0;
	if (title && size > 0)
		snprintf(title, (size_t)size, "%s", F.title);
	*done = F.done;
	*total = F.size;
	return 1;
}

static void landed(void)
{
	mcc_maps_rescan();
	ae_mcc_workshop_rescan();
	ae_map_library_refresh();
}

static void drop_pending(void)
{
	F.queue_next = F.queue_count;
}

static void finish_item(
	struct ae_steam_fetch const *status,
	unsigned long now)
{
	char text[200], title[96];

	snprintf(title, sizeof(title), "%s", status->title[0] ? status->title : F.title);
	clean(title);
	F.running = 0;
	tried_add(F.running_item.id);
	if (status->state == CLIENT_DONE)
	{
		unsigned long long bytes = status->why == CLIENT_WHY_ALREADY ? 0 : status->taken ? status->taken : F.size;

		/* (the one budget: the host's maps and Steam's count together, and each server's Steam bytes are kept) */
		if (!ae_map_share_all_charge(bytes))
			F.uncharged += bytes;
		ae_jf_server_add(&servers, ae_map_share_all_host_key(), bytes);
		F.retries = 0;
		landed();
		if (ae_mcc_map_present(F.running_item.name))
		{
			ae_jf_done_text(title, F.queue_next, F.queue_count, text, sizeof(text));
			say(text);
			tell_again();
		}
		else
			say_why(AE_JF_NOT_THE_MAP);
	}
	else
	{
		switch (status->why)
		{
		case CLIENT_WHY_NO_ROOM: say_why(AE_JF_NO_ROOM); drop_pending(); break;
		case CLIENT_WHY_LIMIT_BYTES: say_why(AE_JF_LIMIT_BYTES); break;
		case CLIENT_WHY_NOT_SIGNED_IN: say_why(AE_JF_NOT_SIGNED_IN); drop_pending(); break;
		case CLIENT_WHY_TOO_LARGE: say_why(AE_JF_TOO_LARGE); break;
		case CLIENT_WHY_NOT_MULTIPLAYER: say_why(AE_JF_NOT_MULTIPLAYER); break;
		case CLIENT_WHY_BUSY:
			/* (the downloader's queue was full: this item asked again in a moment) */
			F.queue_next--;
			F.tried_count--;
			F.retry_at = now + BUSY_RETRY_MILLISECONDS;
			if (++F.retries > BUSY_RETRIES)
			{
				say_why(AE_JF_REFUSED);
				drop_pending();
			}
			break;
		case CLIENT_WHY_CANCELLED: break;
		default: say_why(AE_JF_REFUSED); break;
		}
		platform_log("join fetch: item %llu not fetched (reason %d)", F.running_item.id, status->why);
	}
	F.size = F.done = 0;
	ae_steam_client_fetch_clear();
}

static int start_next(
	unsigned long now)
{
	unsigned long long left;
	char const *why_not = NULL;

	while (F.queue_next < F.queue_count)
	{
		struct item const *item = &F.queue[F.queue_next];

		if (ae_mcc_map_present(item->name))
		{
			/* (landed meanwhile, or held after all) */
			tried_add(item->id);
			F.queue_next++;
			continue;
		}
		left = ae_jf_budget_left(config_integer("network.map_download_limit"), ae_map_share_all_taken(),
			ae_jf_server_bytes(&servers, ae_map_share_all_host_key()));
		if (!left)
		{
			say_why(AE_JF_LIMIT_BYTES);
			drop_pending();
			return 0;
		}
		if (!ae_steam_client_fetch_begin(item->id, AE_JF_ITEM_BYTES_MAXIMUM, left))
		{
			if (++F.retries > BUSY_RETRIES)
			{
				say_why(AE_JF_REFUSED);
				drop_pending();
			}
			F.retry_at = now + BUSY_RETRY_MILLISECONDS;
			return 0;
		}
		F.running = 1;
		F.running_item = *item;
		F.done = F.size = 0;
		snprintf(F.title, sizeof(F.title), "%.48s", item->name);
		clean_name(F.title);
		F.queue_next++;
		platform_log("join fetch: item %llu asked of Steam (%d of %d)", item->id, F.queue_next, F.queue_count);
		return 1;
	}
	(void)why_not;
	return 0;
}

/* an item that is here now, whoever put it here (the player's STEAM ACCOUNT, say), is no longer needed and is told to the
server; looked for only while something is waited for, less often the longer nothing comes */
static void watch(
	unsigned long now)
{
	int at, arrived = 0;

	if (F.watch_at && (long)(now - F.watch_at) < 0)
		return;
	/* (in a game being played it looks at the longest gap only: nothing here may cost a frame) */
	F.watch_gap = game_in_progress() ? WATCH_LAST_MILLISECONDS : !F.watch_gap ? WATCH_FIRST_MILLISECONDS :
		F.watch_gap * 2 > WATCH_LAST_MILLISECONDS ? WATCH_LAST_MILLISECONDS : F.watch_gap * 2;
	F.watch_at = now + F.watch_gap;
	ae_mcc_workshop_rescan();
	for (at = F.queue_next; at < F.queue_count; at++)
		if (ae_mcc_map_present(F.queue[at].name))
		{
			tried_add(F.queue[at].id);
			memmove(&F.queue[at], &F.queue[at + 1], sizeof(F.queue[0]) * (size_t)(F.queue_count - at - 1));
			F.queue_count--;
			at--;
			arrived++;
		}
	if (arrived)
	{
		platform_log("join fetch: %d needed Workshop maps are here now", arrived);
		F.watch_gap = 0;
		tell_again();
	}
}

void ae_join_fetch_tick(
	unsigned long now)
{
	struct ae_steam_status session;
	struct ae_steam_fetch status;
	int gate, opened = ae_steam_client_opened();

	if (game_connection() != _game_connection_network_client)
	{
		if (F.was_client)
		{
			platform_log("join fetch: no longer a client: the queue is dropped");
			reset();
		}
		return;
	}
	F.was_client = 1;
	if (F.uncharged && ae_map_share_all_charge(F.uncharged))
		F.uncharged = 0;
	if (!allowed())
	{
		/* (turned off while it went: the item finishes after the cancel, and nothing else is asked) */
		if (F.running)
		{
			ae_steam_client_fetch_cancel();
			ae_steam_client_fetch_status(&status);
			if (status.state != CLIENT_RUNNING)
				finish_item(&status, now);
		}
		drop_pending();
		if (!(F.warned & (1u << 31)))
		{
			F.warned |= 1u << 31;
			platform_log("join fetch: off (network.map_download_steam %d, host policy %d)",
				config_boolean("network.map_download_steam") != 0, ae_map_share_all_policy_allows() != 0);
		}
		return;
	}
	if (!F.running && F.queue_next >= F.queue_count)
		return;
	if (opened)
		ae_steam_client_frame();
	if (F.running)
	{
		ae_steam_client_fetch_status(&status);
		if (status.state == CLIENT_RUNNING)
		{
			char title[96], text[200];

			F.done = status.done;
			F.size = status.size;
			if (status.title[0] && strcmp(status.title, F.title))
			{
				snprintf(F.title, sizeof(F.title), "%s", status.title);
				clean(F.title);
				snprintf(title, sizeof(title), "%s", F.title);
				ae_jf_start_text(title, F.queue_next, F.queue_count, text, sizeof(text));
				say(text);
			}
		}
		else if (status.state == CLIENT_DONE || status.state == CLIENT_FAILED)
			finish_item(&status, now);
		return;
	}
	if (F.retry_at && (long)(now - F.retry_at) < 0)
		return;
	if (F.queue_next >= F.queue_count)
		return;
	ae_steam_client_status(&session);
	/* (the Steam client is not started by a join: a saved sign-in may ask the system's password store for its key, over
	the game's window; it is started by STEAM ACCOUNT) */
	gate = ae_jf_gate(1, ae_steam_client_built(), ae_mcc_menu_on(), opened ? session.session : AE_JF_SESSION_OUT,
		opened && (session.checking || session.store_waiting));
	if (gate == AE_JF_WAIT)
	{
		if (!F.waiting_since)
			F.waiting_since = now | 1;
		if (now - F.waiting_since < SIGN_IN_WAIT_MILLISECONDS)
			return;
		gate = AE_JF_NOT_SIGNED_IN;
	}
	else
		F.waiting_since = 0;
	if (gate != AE_JF_OK)
	{
		/* (told once; the items stay waited for: a sign-in made meanwhile starts them) */
		say_why(gate);
		return;
	}
	/* (looked for only when a fetch can happen: nothing else can arrive that this watches for) */
	watch(now);
	if (F.queue_next < F.queue_count)
		start_next(now);
}
