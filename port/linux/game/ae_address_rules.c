/* ae_address_rules.c: see ae_address_rules.h */
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ae_address_rules.h"

static char const *const reasons[] =
{
	"",
	"Type the address of the host's game.",
	"That address is too long: 64 characters at most.",
	"An address has letters, digits, dots, dashes and one colon before the port.",
	"The address needs a host before the colon.",
	"The port is a number from 1 to 65535."
};

const char *ae_address_reason(int reason)
{
	return reason >= 0 && reason < (int)(sizeof(reasons) / sizeof(reasons[0])) ? reasons[reason] : "";
}

int ae_address_parse(const char *text, unsigned short default_port, struct ae_address *address)
{
	char trimmed[AE_ADDRESS_LIMIT + 8];
	char *colon, *end;
	size_t length, index;
	unsigned long port = default_port;
	int typed = 0;

	if (!text)
		return AE_ADDRESS_EMPTY;
	while (*text == ' ' || *text == '\t')
		text++;
	length = strlen(text);
	while (length && (text[length - 1] == ' ' || text[length - 1] == '\t' || text[length - 1] == '\r' ||
		text[length - 1] == '\n'))
		length--;
	if (!length)
		return AE_ADDRESS_EMPTY;
	if (length > AE_ADDRESS_LIMIT)
		return AE_ADDRESS_TOO_LONG;
	memcpy(trimmed, text, length);
	trimmed[length] = 0;
	colon = strchr(trimmed, ':');
	if (colon)
	{
		if (strchr(colon + 1, ':'))
			return AE_ADDRESS_BAD_CHARACTER;
		*colon = 0;
	}
	if (!trimmed[0])
		return AE_ADDRESS_NO_HOST;
	for (index = 0; trimmed[index]; index++)
	{
		unsigned char c = (unsigned char)trimmed[index];

		if (!(isalnum(c) || c == '.' || c == '-' || c == '_'))
			return AE_ADDRESS_BAD_CHARACTER;
	}
	if (colon)
	{
		if (!colon[1])
			return AE_ADDRESS_BAD_PORT;
		for (index = 1; colon[index]; index++)
			if (!isdigit((unsigned char)colon[index]))
				return AE_ADDRESS_BAD_PORT;
		port = strtoul(colon + 1, &end, 10);
		if (strlen(colon + 1) > 5 || port < 1 || port > 65535)
			return AE_ADDRESS_BAD_PORT;
		typed = 1;
	}
	if (address)
	{
		memset(address, 0, sizeof(*address));
		/* (the host is at most 64 characters, as the whole text is; "host:port" is the typed text with its port
		written plainly, no longer) */
		memcpy(address->host, trimmed, strlen(trimmed) <= AE_ADDRESS_HOST_LIMIT ? strlen(trimmed) : AE_ADDRESS_HOST_LIMIT);
		address->port = (unsigned short)port;
		address->port_typed = typed;
		memcpy(address->text, address->host, strlen(address->host));
		if (typed)
		{
			char digits[24];

			snprintf(digits, sizeof(digits), ":%lu", port);
			if (strlen(address->text) + strlen(digits) <= AE_ADDRESS_LIMIT)
				memcpy(address->text + strlen(address->text), digits, strlen(digits));
		}
	}
	return AE_ADDRESS_OK;
}

/* ---------- RECENT */

void ae_address_recent_init(struct ae_address_recent *recent)
{
	memset(recent, 0, sizeof(*recent));
}

static int same_text(const char *a, const char *b)
{
	for (; *a && *b; a++, b++)
		if (tolower((unsigned char)*a) != tolower((unsigned char)*b))
			return 0;
	return *a == *b;
}

/* the name as kept: no tab or line end, at most the limit's bytes cut at a character */
static void name_copy(char *out, const char *name)
{
	size_t length = 0;

	while (name && *name && length < AE_ADDRESS_NAME_LIMIT)
	{
		unsigned char c = (unsigned char)*name;
		size_t size = c < 0x80 ? 1 : c >= 0xF0 ? 4 : c >= 0xE0 ? 3 : c >= 0xC0 ? 2 : 1;

		if (length + size > AE_ADDRESS_NAME_LIMIT)
			break;
		if (c == '\t' || c == '\r' || c == '\n')
			c = ' ';
		if (size == 1)
			out[length++] = (char)c;
		else
		{
			memcpy(out + length, name, size);
			length += size;
		}
		name += size;
	}
	out[length] = 0;
}

int ae_address_recent_add(struct ae_address_recent *recent, const char *name, const char *text, long when)
{
	struct ae_address_recent_entry entry;
	int index, found = -1;

	if (!text || !text[0] || strlen(text) > AE_ADDRESS_LIMIT)
		return 0;
	memset(&entry, 0, sizeof(entry));
	name_copy(entry.name, name);
	memcpy(entry.text, text, strlen(text));
	entry.when = when;
	for (index = 0; index < recent->count; index++)
		if (same_text(recent->entries[index].text, text))
			found = index;
	if (found >= 0)
	{
		/* (the host's name kept when this time gave none) */
		if (!entry.name[0])
			memcpy(entry.name, recent->entries[found].name, strlen(recent->entries[found].name));
		ae_address_recent_forget(recent, found);
	}
	if (recent->count >= AE_ADDRESS_RECENT_LIMIT)
		recent->count = AE_ADDRESS_RECENT_LIMIT - 1;
	memmove(recent->entries + 1, recent->entries, (size_t)recent->count * sizeof(recent->entries[0]));
	recent->entries[0] = entry;
	recent->count++;
	return 1;
}

int ae_address_recent_forget(struct ae_address_recent *recent, int index)
{
	if (index < 0 || index >= recent->count)
		return 0;
	memmove(recent->entries + index, recent->entries + index + 1,
		(size_t)(recent->count - index - 1) * sizeof(recent->entries[0]));
	recent->count--;
	return 1;
}

long ae_address_recent_write(struct ae_address_recent const *recent, char *buffer, long size)
{
	long length = 0;
	int index;

	if (size < 1)
		return -1;
	buffer[0] = 0;
	for (index = 0; index < recent->count; index++)
	{
		struct ae_address_recent_entry const *entry = &recent->entries[index];
		int wrote = snprintf(buffer + length, (size_t)(size - length), "%ld\t%s\t%s\n", entry->when, entry->text,
			entry->name);

		if (wrote < 0 || length + wrote >= size)
			return -1;
		length += wrote;
	}
	return length;
}

int ae_address_recent_read(struct ae_address_recent *recent, const char *text)
{
	int whole = 1;

	ae_address_recent_init(recent);
	while (text && *text)
	{
		char line[AE_ADDRESS_LIMIT + AE_ADDRESS_NAME_LIMIT + 64];
		char *tab1, *tab2, *end;
		size_t length = strcspn(text, "\n");
		long when;
		struct ae_address address;

		if (length >= sizeof(line))
		{
			whole = 0;
			text += length + (text[length] ? 1 : 0);
			continue;
		}
		memcpy(line, text, length);
		line[length] = 0;
		text += length + (text[length] ? 1 : 0);
		if (length && line[length - 1] == '\r')
			line[length - 1] = 0;
		if (!line[0])
			continue;
		tab1 = strchr(line, '\t');
		tab2 = tab1 ? strchr(tab1 + 1, '\t') : NULL;
		if (!tab1 || !tab2)
		{
			whole = 0;
			continue;
		}
		*tab1 = 0;
		*tab2 = 0;
		when = strtol(line, &end, 10);
		if (*end || ae_address_parse(tab1 + 1, 0, &address) != AE_ADDRESS_OK || recent->count >= AE_ADDRESS_RECENT_LIMIT)
		{
			whole = 0;
			continue;
		}
		{
			struct ae_address_recent_entry *entry = &recent->entries[recent->count++];

			memset(entry, 0, sizeof(*entry));
			memcpy(entry->text, address.text, strlen(address.text));
			name_copy(entry->name, tab2 + 1);
			entry->when = when;
		}
	}
	return whole;
}

/* ---------- when */

/* days since 1970-01-01 of a civil date */
static long days_of(int year, int month, int day)
{
	long y = year - (month <= 2);
	long era = (y >= 0 ? y : y - 399) / 400;
	long yoe = y - era * 400;
	long doy = (153 * (month + (month > 2 ? -3 : 9)) + 2) / 5 + day - 1;
	long doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;

	return era * 146097 + doe - 719468;
}

void ae_address_when_label(struct tm const *then, struct tm const *now, char *label, size_t size)
{
	static char const *const months[12] = { "JANUARY", "FEBRUARY", "MARCH", "APRIL", "MAY", "JUNE", "JULY", "AUGUST",
		"SEPTEMBER", "OCTOBER", "NOVEMBER", "DECEMBER" };
	long days = days_of(now->tm_year + 1900, now->tm_mon + 1, now->tm_mday) -
		days_of(then->tm_year + 1900, then->tm_mon + 1, then->tm_mday);

	if (days <= 0)
		snprintf(label, size, "TODAY %02d:%02d", then->tm_hour, then->tm_min);
	else if (days == 1)
		snprintf(label, size, "YESTERDAY");
	else if (days < 7)
		snprintf(label, size, "%ld DAYS AGO", days);
	else
		snprintf(label, size, "%s %d", months[then->tm_mon % 12], then->tm_mday);
}
