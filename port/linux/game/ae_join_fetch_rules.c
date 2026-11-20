/*
AE_JOIN_FETCH_RULES.C

See ae_join_fetch_rules.h.
*/

#include <stdio.h>
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

unsigned long long ae_jf_server_bytes(
	struct ae_jf_servers const *servers,
	unsigned long long key)
{
	int at;

	for (at = 0; at < AE_JF_SERVERS; at++)
		if (servers->bytes[at] && servers->key[at] == key)
			return servers->bytes[at];
	return 0;
}

void ae_jf_server_add(
	struct ae_jf_servers *servers,
	unsigned long long key,
	unsigned long long bytes)
{
	int at;

	if (!bytes)
		return;
	for (at = 0; at < AE_JF_SERVERS; at++)
		if (servers->bytes[at] && servers->key[at] == key)
		{
			servers->bytes[at] += bytes;
			return;
		}
	at = servers->next++ % AE_JF_SERVERS;
	servers->key[at] = key;
	servers->bytes[at] = bytes;
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
	case AE_JF_LIMIT_ITEMS: words = "This server has more Workshop maps than this game fetches at once: the rest are left."; break;
	case AE_JF_NOT_MULTIPLAYER: words = "A Workshop map of this server is not a Halo CE multiplayer map."; break;
	case AE_JF_NOT_THE_MAP: words = "A Workshop item of this server does not hold the map it was named for."; break;
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
