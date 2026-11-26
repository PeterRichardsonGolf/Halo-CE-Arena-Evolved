/*
AE_JOIN_FETCH_RULES.C

See ae_join_fetch_rules.h.
*/

#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "ae_join_fetch_rules.h"
#include "ae_map_kind.h"

int ae_jf_item_of(
	char const *identity,
	unsigned long long *id)
{
	char name[AE_PL_MAP_NAME], digits[24];
	unsigned long long value = 0;
	char const *at;

	if (!identity || !id || !ae_map_kind_workshop_parts(identity, name, sizeof(name), digits, sizeof(digits)))
		return 0;
	/* (a file name and nothing else: no folder (mcc_maps\x@ws1), no drive, no control byte) */
	if (!name[0] || name[0] == '.')
		return 0;
	for (at = name; *at; at++)
		if (*at == '\\' || *at == '/' || *at == ':' || (unsigned char)*at < 32)
			return 0;
	for (at = digits; *at; at++)
	{
		unsigned digit;

		if (*at < '0' || *at > '9')
			return 0;
		digit = (unsigned)(*at - '0');
		if (value > (0xFFFFFFFFFFFFFFFFull - digit) / 10)
			return 0;
		value = value * 10 + digit;
	}
	if (!value)
		return 0;
	*id = value;
	return 1;
}

int ae_jf_gate(
	int setting_on,
	int built,
	int has_mcc,
	int session,
	int checking)
{
	if (!setting_on)
		return AE_JF_OFF;
	if (!built)
		return AE_JF_NOT_BUILT;
	if (!has_mcc)
		return AE_JF_NO_MCC;
	if (session == AE_JF_SESSION_IN)
		return AE_JF_OK;
	/* (a saved sign-in being read, or a sign-in under way: not settled) */
	if (checking || (session > AE_JF_SESSION_OUT && session < AE_JF_SESSION_IN))
		return AE_JF_WAIT;
	return AE_JF_NOT_SIGNED_IN;
}

unsigned long long ae_jf_budget_left(
	long limit_megabytes,
	unsigned long long connection_taken,
	unsigned long long server_taken)
{
	unsigned long long limit = 0, left;

	if (limit_megabytes > 0)
		limit = (unsigned long long)limit_megabytes << 20;
	left = connection_taken >= limit ? 0 : limit - connection_taken;
	if (server_taken >= AE_JF_SERVER_BYTES_MAXIMUM)
		return 0;
	return left < AE_JF_SERVER_BYTES_MAXIMUM - server_taken ? left : AE_JF_SERVER_BYTES_MAXIMUM - server_taken;
}

static int server_slot(
	struct ae_jf_servers const *servers,
	unsigned long long key)
{
	int at;

	for (at = 0; at < AE_JF_SERVERS; at++)
		if ((servers->bytes[at] || servers->items[at]) && servers->key[at] == key)
			return at;
	return -1;
}

static int server_slot_make(
	struct ae_jf_servers *servers,
	unsigned long long key)
{
	int at = server_slot(servers, key);

	if (at >= 0)
		return at;
	at = servers->next++ % AE_JF_SERVERS;
	servers->key[at] = key;
	servers->bytes[at] = 0;
	servers->items[at] = 0;
	memset(servers->item_ids[at], 0, sizeof(servers->item_ids[at]));
	return at;
}

unsigned long long ae_jf_server_bytes(
	struct ae_jf_servers const *servers,
	unsigned long long key)
{
	int at = server_slot(servers, key);

	return at < 0 ? 0 : servers->bytes[at];
}

void ae_jf_server_add(
	struct ae_jf_servers *servers,
	unsigned long long key,
	unsigned long long bytes)
{
	if (!bytes)
		return;
	servers->bytes[server_slot_make(servers, key)] += bytes;
}

int ae_jf_server_items(
	struct ae_jf_servers const *servers,
	unsigned long long key)
{
	int at = server_slot(servers, key);

	return at < 0 ? 0 : servers->items[at];
}

int ae_jf_server_item_add(
	struct ae_jf_servers *servers,
	unsigned long long key,
	unsigned long long item)
{
	int at = server_slot_make(servers, key), known;

	for (known = 0; known < servers->items[at]; known++)
		if (servers->item_ids[at][known] == item)
			return 1;
	if (servers->items[at] >= AE_JF_ITEMS_MAXIMUM)
		return 0;
	servers->item_ids[at][servers->items[at]++] = item;
	return 1;
}

/* the milliseconds from then to now, in the 32 bits the clock counts in (it wraps after 49 days, whatever the width of a long) */
static unsigned long elapsed32(
	unsigned long then,
	unsigned long now)
{
	return (unsigned long)(uint32_t)(now - then);
}

unsigned long ae_jf_hold_clock(
	unsigned long now)
{
	return (uint32_t)now ? now : 1;
}

int ae_jf_hold_expired(
	unsigned long since,
	unsigned long now)
{
	return !since || elapsed32(since, now) >= AE_JF_HOLD_MAXIMUM_MS;
}

static int hold_quiet(
	struct ae_jf_hold const *hold,
	unsigned long now)
{
	return elapsed32(hold->heard, now) > AE_JF_HOLD_FRESH_MS;
}

/* the record is this machine's in this join (its address and join time); the map it is for is the record's state's */
static int hold_mine(
	struct ae_jf_hold const *hold,
	unsigned long address,
	unsigned long joined)
{
	return hold->address == address && hold->joined == joined && (hold->state != AE_JF_HOLD_NONE || hold->since);
}

void ae_jf_hold_progress(
	struct ae_jf_hold *hold,
	unsigned long address,
	unsigned long joined,
	unsigned long map,
	int progress,
	unsigned long now)
{
	if (!hold_mine(hold, address, joined))
	{
		memset(hold, 0, sizeof(*hold));
		hold->address = address;
		hold->joined = joined;
		hold->map = map;
	}
	/* (a new map: its fetching and done are its own; the time since the join's first hold, and a failure or a lapse, are the
	join's, so no rotation of maps gives a joiner more than AE_JF_HOLD_MAXIMUM_MS in all) */
	if (hold->map != map)
	{
		hold->map = map;
		if (hold->state == AE_JF_HOLD_FETCHING || hold->state == AE_JF_HOLD_DONE)
			hold->state = AE_JF_HOLD_NONE;
	}
	if (hold->state == AE_JF_HOLD_FAILED || hold->state == AE_JF_HOLD_LAPSED || hold->state == AE_JF_HOLD_DONE)
		return;
	if (progress == 2)
	{
		hold->state = AE_JF_HOLD_FAILED;
		return;
	}
	/* (the join's 15 minutes run from its first message, a done as well) */
	if (!hold->since)
		hold->since = ae_jf_hold_clock(now);
	if ((hold->state == AE_JF_HOLD_FETCHING && hold_quiet(hold, now)) || ae_jf_hold_expired(hold->since, now))
	{
		hold->state = AE_JF_HOLD_LAPSED;
		return;
	}
	if (progress == 1)
	{
		hold->state = AE_JF_HOLD_DONE;
		hold->done_at = now;
		return;
	}
	hold->state = AE_JF_HOLD_FETCHING;
	hold->heard = now;
}

int ae_jf_hold_active(
	struct ae_jf_hold const *hold,
	unsigned long address,
	unsigned long joined,
	unsigned long map,
	int needs_map,
	unsigned long now)
{
	if (!hold_mine(hold, address, joined) || hold->map != map)
		return 0;
	if (hold->state == AE_JF_HOLD_FETCHING)
		return !hold_quiet(hold, now) && !ae_jf_hold_expired(hold->since, now);
	if (hold->state == AE_JF_HOLD_DONE)
		return !needs_map && elapsed32(hold->done_at, now) <= AE_JF_HOLD_DONE_MS;
	return 0;
}

int ae_jf_hold_lapsed(
	struct ae_jf_hold *hold,
	unsigned long address,
	unsigned long joined,
	unsigned long map,
	unsigned long now)
{
	if (!hold_mine(hold, address, joined))
		return 0;
	if (hold->map == map && hold->state == AE_JF_HOLD_FETCHING &&
		(hold_quiet(hold, now) || ae_jf_hold_expired(hold->since, now)))
		hold->state = AE_JF_HOLD_LAPSED;
	return hold->state == AE_JF_HOLD_LAPSED;
}

unsigned long ae_jf_hold_map_key(
	char const *name)
{
	uint32_t key = 2166136261u;

	for (; name && *name; name++)
		key = (key ^ (unsigned char)*name) * 16777619u;
	return key ? key : 1;
}

int ae_jf_hold_allowed(
	char const *identity)
{
	unsigned long long id;

	return ae_jf_item_of(identity, &id);
}

void ae_jf_why_text(
	int why,
	char *text,
	int size)
{
	char const *words = "";

	switch (why)
	{
	case AE_JF_NOT_BUILT: words = "This version cannot get Workshop maps from Steam."; break;
	case AE_JF_NO_MCC: words = "This server's Workshop maps need MCC's files on this PC."; break;
	case AE_JF_NOT_SIGNED_IN:
		words = "Sign in on SETTINGS > SYSTEM > MCC > STEAM ACCOUNT to get this server's Workshop maps.";
		break;
	case AE_JF_REFUSED: words = "Steam would not give a Workshop map for this server."; break;
	case AE_JF_NO_ROOM: words = "There is no room on the disk for a Workshop map of this server."; break;
	case AE_JF_TOO_LARGE: words = "A Workshop map of this server is larger than this game takes."; break;
	case AE_JF_LIMIT_BYTES: words = "This server's Workshop maps are over DOWNLOAD LIMIT: the rest are left."; break;
	case AE_JF_LIMIT_ITEMS: words = "This server has more Workshop maps than this game fetches from one server: the rest are left."; break;
	case AE_JF_NOT_MULTIPLAYER: words = "A Workshop map of this server is not a Halo CE multiplayer map."; break;
	case AE_JF_NOT_THE_MAP: words = "A Workshop item of this server does not hold the map it was named for."; break;
	case AE_JF_TIMEOUT: words = "Steam did not give this game's map in 15 minutes."; break;
	default: break;
	}
	if (size > 0)
		snprintf(text, (size_t)size, "%s", words);
}

void ae_jf_start_text(
	char const *title,
	int number,
	int of,
	char *text,
	int size)
{
	if (of > 1)
		snprintf(text, (size_t)size, "Getting \"%s\" from Steam (%d of %d).", title, number, of);
	else
		snprintf(text, (size_t)size, "Getting \"%s\" from Steam.", title);
}

void ae_jf_done_text(
	char const *title,
	int number,
	int of,
	char *text,
	int size)
{
	if (of > 1)
		snprintf(text, (size_t)size, "Got \"%s\" from Steam (%d of %d).", title, number, of);
	else
		snprintf(text, (size_t)size, "Got \"%s\" from Steam.", title);
}

void ae_jf_progress_text(
	unsigned long long done,
	unsigned long long total,
	char *text,
	int size)
{
	if (total)
		snprintf(text, (size_t)size, "%llu of %llu MB, from Steam.", (done + 0x80000) >> 20, (total + 0x80000) >> 20);
	else
		snprintf(text, (size_t)size, "Asking Steam for it...");
}
