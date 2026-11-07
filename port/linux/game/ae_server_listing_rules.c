/*
AE_SERVER_LISTING_RULES.C

See ae_server_listing_rules.h.
*/

#include "ae_server_listing_rules.h"

#include <stdio.h>
#include <string.h>

static int hex_digit(char character)
{
	if (character >= '0' && character <= '9')
		return character - '0';
	if (character >= 'a' && character <= 'f')
		return character - 'a' + 10;
	if (character >= 'A' && character <= 'F')
		return character - 'A' + 10;
	return -1;
}

int ae_listing_key_usable(
	char const *text,
	unsigned char *key_bytes)
{
	unsigned char bytes[AE_LISTING_KEY_BYTES];
	int index, different = 0;

	if (!text || strncmp(text, "ed25519:", 8) || strlen(text) != 8 + 2 * AE_LISTING_KEY_BYTES)
		return 0;
	for (index = 0; index < AE_LISTING_KEY_BYTES; index++)
	{
		int high = hex_digit(text[8 + 2 * index]), low = hex_digit(text[8 + 2 * index + 1]);

		if (high < 0 || low < 0)
			return 0;
		bytes[index] = (unsigned char)(high << 4 | low);
		different |= bytes[index] != bytes[0];
	}
	/* (a placeholder: every byte alike, zeros or any other fill) */
	if (!different)
		return 0;
	if (key_bytes)
		memcpy(key_bytes, bytes, sizeof(bytes));
	return 1;
}

int ae_listing_is_ae_master(
	unsigned char const *key,
	char const *pinned_text,
	char const *extra_text)
{
	unsigned char known[AE_LISTING_KEY_BYTES];
	char const *cursor = extra_text;

	if (ae_listing_key_usable(pinned_text, known) && !memcmp(known, key, sizeof(known)))
		return 1;
	while (cursor && *cursor)
	{
		char entry[8 + 2 * AE_LISTING_KEY_BYTES + 1];
		size_t length = strcspn(cursor, ", \t\r\n");

		if (length == sizeof(entry) - 1)
		{
			memcpy(entry, cursor, length);
			entry[length] = 0;
			if (ae_listing_key_usable(entry, known) && !memcmp(known, key, sizeof(known)))
				return 1;
		}
		cursor += length;
		while (*cursor == ',' || *cursor == ' ' || *cursor == '\t' || *cursor == '\r' || *cursor == '\n')
			cursor++;
	}
	return 0;
}

int ae_listing_word_valid(
	char const *word)
{
	size_t index, length = word ? strlen(word) : 0;

	if (length < 1 || length > AE_LISTING_WORD_SIZE - 1)
		return 0;
	for (index = 0; index < length; index++)
	{
		if (!((word[index] >= 'a' && word[index] <= 'z') || word[index] == '+'))
			return 0;
	}
	return 1;
}

/* region text the master takes: lowercase letters, digits, -, up to 16 (anything else: empty) */
void ae_listing_region_clean(
	char const *region,
	char *clean)
{
	size_t index, length = region ? strlen(region) : 0;

	clean[0] = 0;
	if (length > AE_LISTING_REGION_SIZE - 1)
		return;
	for (index = 0; index < length; index++)
	{
		char character = region[index];

		if (character >= 'A' && character <= 'Z')
			character = (char)(character - 'A' + 'a');
		if (!((character >= 'a' && character <= 'z') || (character >= '0' && character <= '9') || character == '-'))
		{
			clean[0] = 0;
			return;
		}
		clean[index] = character;
	}
	clean[length] = 0;
}

int ae_listing_extras_canonical(
	char *out,
	int size,
	char const *join_now,
	char const *region)
{
	char clean[AE_LISTING_REGION_SIZE + 1];
	int length;

	if (!ae_listing_word_valid(join_now))
		return 0;
	ae_listing_region_clean(region, clean);
	/* (keys sorted: entries, join_now, join_range, region; entries empty until the playlist's audiences exist) */
	length = snprintf(out, (size_t)size, "{\"entries\":[],\"join_now\":\"%s\",\"join_range\":[\"%s\"],\"region\":\"%s\"}",
		join_now, join_now, clean);
	return length > 0 && length < size ? length : 0;
}

int ae_listing_signed_text(
	char *out,
	int size,
	char const *server_id,
	char const *canonical)
{
	int length = snprintf(out, (size_t)size, "AE-Extras-v1\n%s\n%s", server_id, canonical);

	return length > 0 && length < size ? length : 0;
}

char const *ae_browser_badge_word(
	char const *join_now)
{
	if (!join_now || !join_now[0])
		return "AE";
	if (!strcmp(join_now, "all"))
		return "ANYONE";
	if (!strcmp(join_now, "ae+mcc"))
		return "AE+MCC";
	if (!strcmp(join_now, "ae+workshop"))
		return "AE+WORKSHOP";
	return "AE";
}

int ae_browser_merge_word(
	char const *current,
	char const *incoming)
{
	int has_current = current && current[0], has_incoming = incoming && incoming[0];

	return has_incoming && !has_current;
}

int ae_listing_address_split(
	char const *text,
	char *host,
	int host_size,
	unsigned short *port)
{
	char const *colon;
	long value = 0;
	int length, index, dots = 0, digits_only = 1;

	if (!text || !host || !port || host_size < 2)
		return 0;
	length = (int)strlen(text);
	if (length < 3 || length > AE_LISTING_ADDRESS_SIZE)
		return 0;
	colon = strchr(text, ':');
	if (!colon || strchr(colon + 1, ':') || colon == text || !colon[1] || strlen(colon + 1) > 5)
		return 0;
	for (index = 0; colon[1 + index]; index++)
	{
		if (colon[1 + index] < '0' || colon[1 + index] > '9')
			return 0;
		value = value * 10 + (colon[1 + index] - '0');
	}
	if (value < 1 || value > 65535 || (int)(colon - text) >= host_size)
		return 0;
	for (index = 0; text + index < colon; index++)
	{
		char c = text[index];

		if (c == '.')
			dots++;
		else if (c < '0' || c > '9')
			digits_only = 0;
		if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '.' || c == '-'))
			return 0;
	}
	if (text[0] == '.' || colon[-1] == '.' || strstr(text, ".."))
		return 0;
	if (digits_only)
	{
		/* (an IPv4 literal: four numbers of 0 to 255) */
		int parts = 0, number = 0, seen = 0;

		if (dots != 3)
			return 0;
		for (index = 0; text + index <= colon; index++)
		{
			char c = text + index < colon ? text[index] : '.';

			if (c == '.')
			{
				if (!seen || number > 255)
					return 0;
				parts++;
				number = 0;
				seen = 0;
			}
			else
			{
				number = number * 10 + (c - '0');
				seen = 1;
				if (number > 999)
					return 0;
			}
		}
		if (parts != 4)
			return 0;
	}
	memcpy(host, text, (size_t)(colon - text));
	host[colon - text] = 0;
	*port = (unsigned short)value;
	return 1;
}

int ae_browser_joins_direct(
	char const *address)
{
	char host[AE_LISTING_ADDRESS_SIZE + 1];
	unsigned short port;

	return address && address[0] && ae_listing_address_split(address, host, sizeof(host), &port);
}

int ae_listing_extras_canonical_bots(
	char *out,
	int size,
	char const *join_now,
	char const *region,
	int bots)
{
	char block[AE_LISTING_EXTRAS_SIZE];
	int length;

	if (bots < 0)
		return ae_listing_extras_canonical(out, size, join_now, region);
	if (bots > 255)
		bots = 255;
	length = ae_listing_extras_canonical(block, sizeof(block), join_now, region);
	if (!length)
		return 0;
	length = snprintf(out, (size_t)size, "{\"bots\":%d,%s", bots, block + 1);
	return length > 0 && length < size ? length : 0;
}

int ae_browser_seat_flags(
	int players,
	int maximum,
	int bots)
{
	int flags = 0;

	(void)bots;
	if (maximum > 0 && players >= maximum)
		flags |= 1;
	if (players <= 0)
		flags |= 2;
	return flags;
}

void ae_browser_players_text(
	char *out,
	int size,
	int players,
	int maximum,
	int bots)
{
	if (bots > 0)
		snprintf(out, (size_t)size, "%d / %d \xC2\xB7 %d bot%s", players, maximum, bots, bots == 1 ? "" : "s");
	else
		snprintf(out, (size_t)size, "%d / %d", players, maximum);
}

/* the words of a range mask (bit 0 all, 1 ae, 2 ae+mcc) as a JSON array's items, in that order */
static int range_items(
	char *out,
	int size,
	int mask,
	char const *join_now)
{
	static char const *const words[3] = { "all", "ae", "ae+mcc" };
	int index, length = 0;

	out[0] = 0;
	if (!(mask & 7))
	{
		length = snprintf(out, (size_t)size, "\"%s\"", join_now);
		return length > 0 && length < size;
	}
	for (index = 0; index < 3; index++)
	{
		int wrote;

		if (!(mask & (1 << index)))
			continue;
		wrote = snprintf(out + length, (size_t)(size - length), "%s\"%s\"", length ? "," : "", words[index]);
		if (wrote < 0 || wrote >= size - length)
			return 0;
		length += wrote;
	}
	return 1;
}

int ae_listing_extras_canonical_range(
	char *out,
	int size,
	char const *join_now,
	int range_mask,
	char const *region,
	int bots)
{
	char clean[AE_LISTING_REGION_SIZE + 1], items[64];
	int length;

	if (!ae_listing_word_valid(join_now) || !range_items(items, sizeof(items), range_mask, join_now))
		return 0;
	ae_listing_region_clean(region, clean);
	if (bots >= 0)
		length = snprintf(out, (size_t)size, "{\"bots\":%d,\"entries\":[],\"join_now\":\"%s\",\"join_range\":[%s],\"region\":\"%s\"}",
			bots > 255 ? 255 : bots, join_now, items, clean);
	else
		length = snprintf(out, (size_t)size, "{\"entries\":[],\"join_now\":\"%s\",\"join_range\":[%s],\"region\":\"%s\"}",
			join_now, items, clean);
	return length > 0 && length < size ? length : 0;
}
