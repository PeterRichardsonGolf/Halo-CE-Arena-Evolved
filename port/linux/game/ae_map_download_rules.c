/*
AE_MAP_DOWNLOAD_RULES.C

Arena Evolved (0.9.3): GET MAPS's pure rules (ae_map_download_rules.h).
*/

#include "ae_map_download_rules.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

long ae_map_download_frame_encode(
	struct ae_map_download_frame const *frame,
	unsigned char *bytes,
	long size)
{
	if (!frame || !bytes || size < AE_MAP_DOWNLOAD_FRAME_BYTES)
		return 0;
	memset(bytes, 0, AE_MAP_DOWNLOAD_FRAME_BYTES);
	bytes[0] = AE_MAP_DOWNLOAD_FRAME_TYPE;
	bytes[1] = (unsigned char)frame->kind;
	switch (frame->kind)
	{
	case AE_MAP_DOWNLOAD_KIND_HELLO:
		if (frame->version < 1 || frame->version > 0xFFFF)
			return 0;
		bytes[2] = (unsigned char)frame->version;
		bytes[3] = (unsigned char)(frame->version >> 8);
		return 4;
	case AE_MAP_DOWNLOAD_KIND_WELCOME:
		return 2;
	case AE_MAP_DOWNLOAD_KIND_REFUSE:
	case AE_MAP_DOWNLOAD_KIND_BYE:
		if (frame->reason < 1 || frame->reason > 0xFF)
			return 0;
		bytes[2] = (unsigned char)frame->reason;
		return 3;
	}
	return 0;
}

int ae_map_download_frame_decode(
	unsigned char const *bytes,
	long size,
	struct ae_map_download_frame *frame)
{
	if (!frame)
		return 0;
	memset(frame, 0, sizeof(*frame));
	if (!bytes || size < 2 || bytes[0] != AE_MAP_DOWNLOAD_FRAME_TYPE)
		return 0;
	frame->kind = bytes[1];
	switch (frame->kind)
	{
	case AE_MAP_DOWNLOAD_KIND_HELLO:
		if (size < 4)
			return 0;
		frame->version = bytes[2] | bytes[3] << 8;
		return frame->version >= 1;
	case AE_MAP_DOWNLOAD_KIND_WELCOME:
		return 1;
	case AE_MAP_DOWNLOAD_KIND_REFUSE:
	case AE_MAP_DOWNLOAD_KIND_BYE:
		if (size < 3 || !bytes[2])
			return 0;
		frame->reason = bytes[2];
		return 1;
	}
	frame->kind = 0;
	return 0;
}

int ae_map_download_room(
	int cap,
	int peers,
	int free_slots,
	int version)
{
	if (cap <= 0)
		return -AE_MAP_DOWNLOAD_REFUSED_OFF;
	/* (a later guest's frames are a later version: this server speaks its own, and says so) */
	if (version != AE_MAP_DOWNLOAD_VERSION)
		return -AE_MAP_DOWNLOAD_REFUSED_VERSION;
	if (peers >= cap || free_slots < AE_MAP_DOWNLOAD_FREE_SLOTS)
		return -AE_MAP_DOWNLOAD_REFUSED_FULL;
	return AE_MAP_DOWNLOAD_KIND_WELCOME;
}

int ae_map_download_hello_action(
	int peer_on,
	int refused_before)
{
	if (peer_on)
		return AE_MAP_DOWNLOAD_HELLO_AGAIN;
	return refused_before ? AE_MAP_DOWNLOAD_HELLO_IGNORE : AE_MAP_DOWNLOAD_HELLO_ANSWER;
}

int ae_map_download_heard(
	int peer_on)
{
	return peer_on != 0;
}

int ae_map_download_join_allowed(
	int peer_on)
{
	return !peer_on;
}

int ae_map_download_peers_setting(
	long value)
{
	if (value <= 0)
		return 0;
	return value > AE_MAP_DOWNLOAD_PEERS_MOST ? AE_MAP_DOWNLOAD_PEERS_MOST : (int)value;
}

int ae_map_download_peer_expired(
	unsigned long idle_ms,
	unsigned long visit_ms)
{
	return idle_ms > AE_MAP_DOWNLOAD_IDLE_MS || visit_ms > AE_MAP_DOWNLOAD_VISIT_MS;
}

static int same_name(
	char const *a,
	char const *b)
{
	while (*a && *b && tolower((unsigned char)*a) == tolower((unsigned char)*b))
	{
		a++;
		b++;
	}
	return !*a && !*b;
}

int ae_map_download_is_wanted(
	char const (*wanted)[AE_MAP_DOWNLOAD_NAME],
	int count,
	char const *name)
{
	int index;

	if (!wanted || !name || !name[0])
		return 0;
	for (index = 0; index < count; index++)
	{
		size_t length = strlen(wanted[index]);
		int digit;

		if (same_name(wanted[index], name))
			return 1;
		/* (a Custom Edition map, <name>@ce in a playlist, as the host's list names it: <name>_<8 hexadecimal digits of
		its hash>) */
		if (length > 3 && wanted[index][length - 3] == '@' && tolower((unsigned char)wanted[index][length - 2]) == 'c' &&
			tolower((unsigned char)wanted[index][length - 1]) == 'e')
		{
			length -= 3;
		}
		if (!length || strlen(name) != length + 9 || name[length] != '_')
			continue;
		for (digit = 0; digit < 8 && isxdigit((unsigned char)name[length + 1 + digit]); digit++)
			;
		if (digit < 8)
			continue;
		for (digit = 0; digit < (int)length &&
			tolower((unsigned char)wanted[index][digit]) == tolower((unsigned char)name[digit]); digit++)
			;
		if (digit == (int)length)
			return 1;
	}
	return 0;
}

int ae_map_download_wanted(
	char const *const *names,
	int const *fallbacks,
	int count,
	ae_map_download_name_fn is_stock,
	ae_map_download_name_fn present,
	void *context,
	char (*out)[AE_MAP_DOWNLOAD_NAME],
	int maximum)
{
	int index, found = 0;

	if (!names || !out || maximum <= 0)
		return 0;
	for (index = 0; index < count && found < maximum; index++)
	{
		char const *name = names[index];

		if (!name || !name[0] || strlen(name) >= AE_MAP_DOWNLOAD_NAME || (fallbacks && fallbacks[index]))
			continue;
		if ((is_stock && is_stock(context, name)) || (present && present(context, name)))
			continue;
		if (ae_map_download_is_wanted((char const (*)[AE_MAP_DOWNLOAD_NAME])out, found, name))
			continue;
		snprintf(out[found], AE_MAP_DOWNLOAD_NAME, "%s", name);
		found++;
	}
	return found;
}

int ae_map_download_outcome_of_refusal(
	int reason)
{
	switch (reason)
	{
	case AE_MAP_DOWNLOAD_REFUSED_OFF: return AE_MAP_DOWNLOAD_OUTCOME_OFF;
	case AE_MAP_DOWNLOAD_REFUSED_FULL: return AE_MAP_DOWNLOAD_OUTCOME_FULL;
	case AE_MAP_DOWNLOAD_REFUSED_FILLED: return AE_MAP_DOWNLOAD_OUTCOME_FILLED;
	case AE_MAP_DOWNLOAD_REFUSED_VERSION: return AE_MAP_DOWNLOAD_OUTCOME_OLD;
	}
	return AE_MAP_DOWNLOAD_OUTCOME_OFF;
}

int ae_map_download_outcome_failed(
	int outcome)
{
	return outcome != AE_MAP_DOWNLOAD_OUTCOME_NONE && outcome != AE_MAP_DOWNLOAD_OUTCOME_DONE &&
		outcome != AE_MAP_DOWNLOAD_OUTCOME_NOTHING_NEEDED && outcome != AE_MAP_DOWNLOAD_OUTCOME_CANCELLED;
}

void ae_map_download_outcome_text(
	int outcome,
	int done,
	char const *server,
	char *out,
	int size)
{
	char const *name = server && server[0] ? server : "The server";

	if (!out || size <= 0)
		return;
	switch (outcome)
	{
	case AE_MAP_DOWNLOAD_OUTCOME_DONE:
		snprintf(out, (size_t)size, done == 1 ? "Got 1 map. Every playlist map is here." :
			"Got %d maps. Every playlist map is here.", done);
		break;
	case AE_MAP_DOWNLOAD_OUTCOME_SOME:
		snprintf(out, (size_t)size, done == 1 ? "Got 1 map. No server has the rest right now." :
			"Got %d maps. No server has the rest right now.", done);
		break;
	case AE_MAP_DOWNLOAD_OUTCOME_NOTHING_NEEDED:
		snprintf(out, (size_t)size, "Every playlist map is already here.");
		break;
	case AE_MAP_DOWNLOAD_OUTCOME_NO_SERVERS:
		snprintf(out, (size_t)size, "No AE servers are listed right now. Try again later.");
		break;
	case AE_MAP_DOWNLOAD_OUTCOME_NONE_HAVE:
		snprintf(out, (size_t)size, "No server has the missing maps right now. Try again later.");
		break;
	case AE_MAP_DOWNLOAD_OUTCOME_OFFLINE:
		snprintf(out, (size_t)size, "%s did not answer. Try again later.", name);
		break;
	case AE_MAP_DOWNLOAD_OUTCOME_OLD:
		snprintf(out, (size_t)size, "%s needs version 0.9.3 to share maps.", name);
		break;
	case AE_MAP_DOWNLOAD_OUTCOME_FULL:
		snprintf(out, (size_t)size, "%s is busy. Try again in a few minutes.", name);
		break;
	case AE_MAP_DOWNLOAD_OUTCOME_OFF:
		snprintf(out, (size_t)size, "%s does not share maps.", name);
		break;
	case AE_MAP_DOWNLOAD_OUTCOME_FILLED:
		snprintf(out, (size_t)size, "%s filled up with players. Try again later.", name);
		break;
	case AE_MAP_DOWNLOAD_OUTCOME_DOWNLOADS_OFF:
		snprintf(out, (size_t)size, "Map downloads are off. Turn on Download maps in Settings > Network.");
		break;
	case AE_MAP_DOWNLOAD_OUTCOME_ONLINE_OFF:
		snprintf(out, (size_t)size, "Internet play is off. Turn it on in Settings > Network.");
		break;
	case AE_MAP_DOWNLOAD_OUTCOME_BUSY:
		snprintf(out, (size_t)size, "Leave the game you are in first.");
		break;
	case AE_MAP_DOWNLOAD_OUTCOME_CANCELLED:
		snprintf(out, (size_t)size, done == 1 ? "Stopped. Got 1 map." : "Stopped. Got %d maps.", done);
		break;
	default:
		out[0] = 0;
		break;
	}
}

void ae_map_download_progress_text(
	char const *server,
	int index,
	int count,
	unsigned long long bytes,
	unsigned long long total,
	char *out,
	int size)
{
	char const *name = server && server[0] ? server : "AE SERVERS";
	char upper[48];
	int at;

	if (!out || size <= 0)
		return;
	for (at = 0; name[at] && at < (int)sizeof(upper) - 1; at++)
		upper[at] = (char)toupper((unsigned char)name[at]);
	upper[at] = 0;
	if (count <= 0)
		snprintf(out, (size_t)size, "LOOKING FOR MAPS ON %s", upper);
	else if (total > 0)
		snprintf(out, (size_t)size, "GETTING MAPS FROM %s  %d OF %d  %llu / %llu MB", upper, index, count,
			bytes / (1024ULL * 1024ULL), (total + 1024ULL * 1024ULL - 1) / (1024ULL * 1024ULL));
	else
		snprintf(out, (size_t)size, "GETTING MAPS FROM %s  %d OF %d", upper, index, count);
}

int ae_map_download_needs_custom_edition(
	char const *name)
{
	size_t length = name ? strlen(name) : 0;

	return length > 3 && name[length - 3] == '@' && tolower((unsigned char)name[length - 2]) == 'c' &&
		tolower((unsigned char)name[length - 1]) == 'e';
}

void ae_map_download_custom_edition_note(
	int count,
	char const *first,
	char *out,
	int size)
{
	if (!out || size <= 0)
		return;
	out[0] = 0;
	if (count <= 0)
		return;
	if (count == 1 && first && first[0])
		snprintf(out, (size_t)size, "%.*s needs MCC or Halo PC.", (int)strcspn(first, "@"), first);
	else
		snprintf(out, (size_t)size, "%d maps need MCC or Halo PC.", count);
}

int ae_map_download_refusal_kept(
	unsigned long refused_address,
	unsigned long slot_address,
	unsigned long age_ms)
{
	return refused_address != 0 && slot_address == refused_address && age_ms < AE_MAP_DOWNLOAD_REFUSAL_MS;
}
