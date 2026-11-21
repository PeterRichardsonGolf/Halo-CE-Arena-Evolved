/*
AE_JOIN_FETCH.C

See ae_join_fetch.h. The rules are ae_join_fetch_rules.c's; Steam's side is the client's (ae_steam_client.h).
*/

#include <stdint.h>
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
#include "ae_mcc_boot.h"
#include "ae_server_lobby.h"
#include "mcc_maps.h"
#include "../src/ae_mcc.h"
#include "../src/ae_steam_client.h"
#include "ae_steam_rules.h"

void platform_log(char const *format, ...);
unsigned long system_milliseconds(void);
void display_error_text_when_main_menu_loaded(wchar_t const *text);
void network_game_abort(void);
void map_share_report_external(int state, unsigned long done, unsigned long total);
int config_boolean(const char *name);
long config_integer(const char *name);
void ae_chat_local_notice(char const *text);

enum
{
	SIGN_IN_WAIT_MILLISECONDS = 30000,
	BUSY_RETRY_MILLISECONDS = 2000,
	BUSY_RETRIES = 60,
	WATCH_FIRST_MILLISECONDS = 3000,
	WATCH_LAST_MILLISECONDS = 15000,
	CANCEL_GIVE_UP_MILLISECONDS = 30000
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
	/* the game's map, a Workshop map this PC lacked at the join: held until Steam has given it ("" none) */
	char current[AE_PL_MAP_NAME];
	unsigned long current_since, reported_at;
	unsigned long long current_key;
	int last_why;
} F;

/* a host's side: the machines that said they fetch the game's Workshop map from Steam (their slots held, for a limited time) */
enum { HOLD_MACHINES = 128 };
static struct ae_jf_hold hold[HOLD_MACHINES];
unsigned long network_game_server_machine_address(long machine_index);
unsigned long network_game_server_machine_join_time(long machine_index);

/* the held map's item, still coming from Steam after the connection ended (the server let go of this machine before the map
was here): it is let finish, within the budget it began with, so the player's next join finds the map */
static int allowed(void);

/* whether a time set ahead (a retry, a look) is not come yet: in 32 bits, as the clock counts (0: none set) */
static int not_yet(
	unsigned long at,
	unsigned long now)
{
	return at && (int32_t)(uint32_t)(now - at) < 0;
}

static struct
{
	int active;
	char name[AE_PL_MAP_NAME];
	char title[96];
	unsigned long long key;
	unsigned long since, cancel_at;
} detached;

static void say(
	char const *text);

static void reset(void)
{
	if (F.running)
	{
		if (F.current[0] && !strcmp(F.running_item.name, F.current) && !detached.active && allowed())
		{
			detached.active = 1;
			snprintf(detached.name, sizeof(detached.name), "%s", F.running_item.name);
			snprintf(detached.title, sizeof(detached.title), "%s", F.title);
			detached.key = F.current_key;
			detached.since = ae_jf_hold_clock(system_milliseconds());
			detached.cancel_at = 0;
			say("The map is still coming from Steam. Join the game again once it is here.");
		}
		else
			ae_steam_client_fetch_cancel();
	}
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

/* whether the reasons told now are the held map's (finish_item sets it while it reports that item) */
static int record_why;

/* the reason the held map is not coming, for the words of the join that is left */
static void held_why(
	int why)
{
	if (F.current[0] && why > 0 && why <= 30)
		F.last_why = why;
}

/* a reason told once in a connection (the bit is the reason's) */
static void say_why(
	int why)
{
	char text[200];

	if (record_why && why > 0 && why <= 30)
		F.last_why = why;
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
	if (F.queue_count >= AE_JF_ITEMS_MAXIMUM || F.planned >= AE_JF_ITEMS_MAXIMUM ||
		!ae_jf_server_item_add(&servers, ae_map_share_all_host_key(), id))
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
	record_why = F.current[0] && !strcmp(F.running_item.name, F.current);
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
	record_why = 0;
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
		if (detached.active)
		{
			/* (the Steam fetch is one at a time: an item left running by the last connection is this one's to take over, or
			is waited for, with no retry counted) */
			if (strcmp(detached.name, item->name) || detached.key != ae_map_share_all_host_key())
				return 0;
			F.running = 1;
			F.running_item = *item;
			F.done = F.size = 0;
			snprintf(F.title, sizeof(F.title), "%s", detached.title);
			detached.active = 0;
			F.queue_next++;
			platform_log("join fetch: item %llu, left running by the last connection, is taken over (%d of %d)", item->id,
				F.queue_next, F.queue_count);
			return 1;
		}
		left = ae_jf_budget_left(config_integer("network.map_download_limit"), ae_map_share_all_taken(),
			ae_jf_server_bytes(&servers, ae_map_share_all_host_key()));
		if (!left)
		{
			held_why(AE_JF_LIMIT_BYTES);
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

	if (not_yet(F.watch_at, now))
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

static void tick_body(
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
		held_why(AE_JF_REFUSED);
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
	if (not_yet(F.retry_at, now))
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
			F.waiting_since = ae_jf_hold_clock(now);
		if ((uint32_t)(now - F.waiting_since) < SIGN_IN_WAIT_MILLISECONDS)
			return;
		gate = AE_JF_NOT_SIGNED_IN;
	}
	else
		F.waiting_since = 0;
	if (gate != AE_JF_OK)
	{
		/* (told once; the items stay waited for: a sign-in made meanwhile starts them; the game's map is not waited for) */
		say_why(gate);
		held_why(gate);
		if (F.current[0])
			drop_pending();
		return;
	}
	/* (looked for only when a fetch can happen: nothing else can arrive that this watches for) */
	watch(now);
	if (F.queue_next < F.queue_count)
		start_next(now);
}

/* ---------- the game's map */

static int current_pending(void)
{
	int at;

	if (F.running && !strcmp(F.running_item.name, F.current))
		return 1;
	for (at = F.queue_next; at < F.queue_count; at++)
		if (!strcmp(F.queue[at].name, F.current))
			return 1;
	return 0;
}

/* what this machine tells the server while it holds: 0 downloading, 1 done, 2 failed (map_share.c's progress message) */
static void current_report(
	int state,
	unsigned long now)
{
	unsigned long done = F.running && F.done < 0xFFFFFFFFull ? (unsigned long)F.done : 0;
	unsigned long total = F.running && F.size < 0xFFFFFFFFull ? (unsigned long)F.size : 0;

	map_share_report_external(state, done, total);
	F.reported_at = now | 1;
}

static void current_fail(
	unsigned long now)
{
	char text[256], reason[200];
	wchar_t wide[256];
	int at;

	/* (a hold that is given up on does not leave its item coming: the fetch is cancelled) */
	if (F.running && !strcmp(F.running_item.name, F.current))
		ae_steam_client_fetch_cancel();
	/* (the join helper's Workshop box, OPEN IN STEAM, is chosen by this note) */
	ae_mcc_stock_missing_message(F.current, text, (long)sizeof(text));
	ae_jf_why_text(F.last_why ? F.last_why : AE_JF_REFUSED, reason, sizeof(reason));
	snprintf(text, sizeof(text), "This game's Workshop map could not be got from Steam.\r\n\r\n%s", reason);
	for (at = 0; text[at] && at < 255; at++)
		wide[at] = (wchar_t)(unsigned char)text[at];
	wide[at] = 0;
	platform_log("join fetch: the game's map %.48s was not got: the join is left (reason %d)", F.current, F.last_why);
	current_report(2, now);
	F.current[0] = 0;
	display_error_text_when_main_menu_loaded(wide);
	network_game_abort();
}

static void current_check(
	unsigned long now)
{
	if (!F.current[0])
		return;
	if (game_connection() != _game_connection_network_client)
	{
		F.current[0] = 0;
		return;
	}
	{
		struct network_game *game = network_game_get_game();
		unsigned long long item;

		/* (the server changed its map: the held one is not waited for, its item goes on as a table item; the new map's own check
		decided the join) */
		if (game && game->map.name[0] && strcmp(game->map.name, F.current))
		{
			platform_log("join fetch: the game's map is now another: %.48s is no longer held for", F.current);
			F.current[0] = 0;
			/* (told to the server, which holds this machine for the old map: the new one is not waited for here, unless it is a
			Workshop map this PC lacks, which is held for by its own check) */
			if (!ae_jf_item_of(game->map.name, &item) || ae_mcc_map_present(game->map.name))
				map_share_report_external(1, 0, 0);
			return;
		}
	}
	if (ae_mcc_map_present(F.current))
	{
		platform_log("join fetch: the game's map %.48s is here: the join goes on", F.current);
		current_report(1, now);
		F.current[0] = 0;
		return;
	}
	if (!current_pending() || ae_jf_hold_expired(F.current_since, now))
	{
		if (current_pending())
			F.last_why = AE_JF_TIMEOUT;
		current_fail(now);
		return;
	}
	if (!F.reported_at || now - F.reported_at >= AE_JF_REPORT_INTERVAL_MS)
		current_report(0, now);
}

/* (each frame) a fetch that outlived its connection: let go once it ends, the player told */
static void detached_tick(void)
{
	struct ae_steam_fetch status;
	char text[200], title[96];

	if (!detached.active || F.running)
		return;
	if (!allowed() || ae_jf_hold_expired(detached.since, system_milliseconds()))
	{
		ae_steam_client_fetch_cancel();
		ae_steam_client_fetch_status(&status);
		if (!detached.cancel_at)
			detached.cancel_at = ae_jf_hold_clock(system_milliseconds());
		if (status.state == CLIENT_RUNNING)
		{
			/* (a download that does not stop when asked is let go of: later items ask Steam as usual and wait if it is busy) */
			if ((uint32_t)(system_milliseconds() - detached.cancel_at) >= CANCEL_GIVE_UP_MILLISECONDS)
			{
				platform_log("join fetch: the map left running after the connection ended did not stop when cancelled: let go of");
				detached.active = 0;
			}
			return;
		}
	}
	else
	{
		if (ae_steam_client_opened())
			ae_steam_client_frame();
		ae_steam_client_fetch_status(&status);
		if (status.state == CLIENT_RUNNING)
			return;
	}
	if (status.state == CLIENT_DONE)
	{
		ae_jf_server_add(&servers, detached.key, status.why == CLIENT_WHY_ALREADY ? 0 : status.taken);
		landed();
		if (ae_mcc_map_present(detached.name))
		{
			snprintf(title, sizeof(title), "%s", status.title[0] ? status.title : detached.title);
			clean(title);
			ae_jf_done_text(title, 1, 1, text, sizeof(text));
			strncat(text, " Join the game again.", sizeof(text) - strlen(text) - 1);
			say(text);
		}
	}
	platform_log("join fetch: the map left running after the connection ended is %s", status.state == CLIENT_DONE ? "in" : "not got");
	detached.active = 0;
	ae_steam_client_fetch_clear();
}

void ae_join_fetch_tick(
	unsigned long now)
{
	/* (first: a result that is in is taken before another connection's fetch can begin over it) */
	detached_tick();
	tick_body(now);
	current_check(now);
}

int ae_join_fetch_current_map(
	char const *level)
{
	unsigned long long id;
	struct ae_steam_status session;
	int opened = ae_steam_client_opened(), at, found = -1;
	unsigned long now = system_milliseconds();

	if (!level)
		return 0;
	if (F.current[0])
	{
		if (!strcmp(F.current, level))
			return 1;
		/* (the server went on to another map: that one is held for, or not, by its own check) */
		platform_log("join fetch: the game's map is now another: %.48s is no longer held for", F.current);
		F.current[0] = 0;
	}
	if (!ae_jf_item_of(level, &id) || game_connection() != _game_connection_network_client || !allowed())
		return 0;
	memset(&session, 0, sizeof(session));
	if (opened)
		ae_steam_client_status(&session);
	if (ae_jf_gate(1, ae_steam_client_built(), ae_mcc_menu_on(), opened ? session.session : AE_JF_SESSION_OUT,
		opened && (session.checking || session.store_waiting)) != AE_JF_OK)
		return 0;
	if (!ae_jf_budget_left(config_integer("network.map_download_limit"), ae_map_share_all_taken(),
		ae_jf_server_bytes(&servers, ae_map_share_all_host_key())))
		return 0;
	/* (the join's 15 minutes of holding are used up: the map is refused as it was before holds) */
	if (F.current_since && ae_jf_hold_expired(F.current_since, now))
		return 0;
	F.was_client = 1;
	need_add(level);
	for (at = F.queue_next; at < F.queue_count; at++)
		if (!strcmp(F.queue[at].name, level))
			found = at;
	if (found < 0 && !(F.running && !strcmp(F.running_item.name, level)))
		return 0;
	if (found > F.queue_next)
	{
		struct item first = F.queue[found];

		memmove(&F.queue[F.queue_next + 1], &F.queue[F.queue_next], sizeof(F.queue[0]) * (size_t)(found - F.queue_next));
		F.queue[F.queue_next] = first;
	}
	snprintf(F.current, sizeof(F.current), "%s", level);
	F.current_key = ae_map_share_all_host_key();
	/* (the join's 15 minutes run from its first hold: a later map's hold has what is left) */
	if (!F.current_since)
		F.current_since = ae_jf_hold_clock(now);
	F.reported_at = 0;
	F.last_why = 0;
	platform_log("join fetch: the game's map is a Workshop map this PC lacks (item %llu): held until Steam gives it", id);
	return 1;
}

int ae_join_fetch_current_holds(
	char const *level)
{
	return F.current[0] && (!level || !strcmp(level, F.current));
}

/* ---------- a host: a joiner that fetches the game's Workshop map from Steam */

/* the key of the game's map for a machine's record (0 when there is no game) */
static unsigned long hold_map(void)
{
	struct network_game *game = network_game_get_game();

	return game ? ae_jf_hold_map_key(game->map.name) : 0;
}

int ae_join_fetch_host_progress(
	long machine_index,
	int state)
{
	struct network_game *game = network_game_get_game();
	unsigned long address = machine_index >= 0 && machine_index < HOLD_MACHINES ?
		network_game_server_machine_address(machine_index) : 0;

	if (!address || !game || !ae_jf_hold_allowed(game->map.name))
		return 0;
	ae_jf_hold_progress(&hold[machine_index], address, network_game_server_machine_join_time(machine_index), hold_map(),
		state, system_milliseconds());
	return 1;
}

int ae_join_fetch_host_holds(
	long machine_index,
	int needs_map)
{
	if (machine_index < 0 || machine_index >= HOLD_MACHINES)
		return 0;
	return ae_jf_hold_active(&hold[machine_index], network_game_server_machine_address(machine_index),
		network_game_server_machine_join_time(machine_index), hold_map(), needs_map, system_milliseconds());
}

int ae_join_fetch_host_lapsed(
	long machine_index)
{
	if (machine_index < 0 || machine_index >= HOLD_MACHINES)
		return 0;
	return ae_jf_hold_lapsed(&hold[machine_index], network_game_server_machine_address(machine_index),
		network_game_server_machine_join_time(machine_index), hold_map(), system_milliseconds());
}
