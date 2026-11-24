#include "ae_server_admin_rules.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int ae_key_name_valid(const char *name)
{
	int length = 0;

	if (!name)
		return 0;
	for (; name[length]; length++)
	{
		char c = name[length];

		if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '-'))
			return 0;
	}
	return length >= 1 && length <= AE_KEY_NAME_MAXIMUM;
}

static const char *const role_names[AE_KEY_ROLE_MAXIMUM + 1] = { "viewer", "moderator", "admin" };

int ae_key_role_parse(const char *text)
{
	int role;

	if (!text)
		return -1;
	for (role = 0; role <= AE_KEY_ROLE_MAXIMUM; role++)
	{
		if (!strcmp(text, role_names[role]))
			return role;
	}
	return -1;
}

const char *ae_key_role_text(int role)
{
	return role >= 0 && role <= AE_KEY_ROLE_MAXIMUM ? role_names[role] : "none";
}

int ae_key_is_hex(const char *text, int length)
{
	int index;

	for (index = 0; index < length; index++)
	{
		char c = text[index];

		if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f')))
			return 0;
	}
	return 1;
}

int ae_key_token_parse(const char *token, char *id, char *secret)
{
	if (!token || strlen(token) != AE_KEY_TOKEN_LENGTH || strncmp(token, "aek_", 4) || token[4 + AE_KEY_ID_LENGTH] != '_')
		return 0;
	if (!ae_key_is_hex(token + 4, AE_KEY_ID_LENGTH) || !ae_key_is_hex(token + 5 + AE_KEY_ID_LENGTH, AE_KEY_SECRET_LENGTH))
		return 0;
	memcpy(id, token + 4, AE_KEY_ID_LENGTH);
	id[AE_KEY_ID_LENGTH] = 0;
	memcpy(secret, token + 5 + AE_KEY_ID_LENGTH, AE_KEY_SECRET_LENGTH);
	secret[AE_KEY_SECRET_LENGTH] = 0;
	return 1;
}

int ae_key_expired(unsigned long expires, unsigned long now)
{
	return expires != 0 && now >= expires;
}

long ae_key_expiry(long days, unsigned long now, unsigned long *expires)
{
	if (days == 0)
	{
		*expires = 0;
		return 0;
	}
	if (days < 1 || days > 3650)
		return -1;
	*expires = now + (unsigned long)days * 86400UL;
	return 0;
}

int ae_key_equal(const char *first, const char *second, int length)
{
	unsigned char difference = 0;
	int index;

	for (index = 0; index < length; index++)
		difference |= (unsigned char)(first[index] ^ second[index]);
	return difference == 0;
}

int ae_key_role_may_mint(int minter_role, int role)
{
	if (role < 0 || role > AE_KEY_ROLE_MAXIMUM)
		return 0;
	return minter_role >= 3 || role <= minter_role;
}

/* one "a.b.c.d[/n]" item of an allow-list: its address and mask; the end of the item (past a comma), or NULL */
static const char *allow_item(const char *text, unsigned long *address, unsigned long *mask)
{
	unsigned long parts[4];
	unsigned long bits = 32;
	int part;
	char *end;

	for (part = 0; part < 4; part++)
	{
		unsigned long number;

		if (*text < '0' || *text > '9')
			return NULL;
		number = strtoul(text, &end, 10);
		if (end == text || number > 255 || end - text > 3)
			return NULL;
		parts[part] = number;
		text = end;
		if (part < 3)
		{
			if (*text != '.')
				return NULL;
			text++;
		}
	}
	if (*text == '/')
	{
		text++;
		if (*text < '0' || *text > '9')
			return NULL;
		bits = strtoul(text, &end, 10);
		if (end == text || bits > 32 || end - text > 2)
			return NULL;
		text = end;
	}
	if (*text == ',')
	{
		text++;
		if (!*text)
			return NULL;
	}
	else if (*text)
		return NULL;
	*address = parts[0] << 24 | parts[1] << 16 | parts[2] << 8 | parts[3];
	*mask = bits == 0 ? 0 : (0xFFFFFFFFUL << (32 - bits)) & 0xFFFFFFFFUL;
	*address &= *mask;
	return text;
}

int ae_key_allow_valid(const char *text)
{
	unsigned long address, mask;

	if (!text || strlen(text) > AE_KEY_ALLOW_MAXIMUM)
		return 0;
	while (*text)
	{
		text = allow_item(text, &address, &mask);
		if (!text)
			return 0;
	}
	return 1;
}

int ae_key_allow_permits(const char *text, unsigned long address)
{
	unsigned long item, mask;

	if (!text || !text[0])
		return 1;
	while (*text)
	{
		text = allow_item(text, &item, &mask);
		if (!text)
			return 0;
		if ((address & mask) == item)
			return 1;
	}
	return 0;
}

int ae_key_line_format(const struct ae_key_record *record, char *line, int size)
{
	int length = snprintf(line, (size_t)size, "%s\t%s\t%s\t%lu\t%lu\t%s\t%s\n", record->id, record->name,
		ae_key_role_text(record->role), record->created, record->expires, record->hash, record->allow);

	return length > 0 && length < size;
}

int ae_key_line_parse(const char *line, struct ae_key_record *record)
{
	char copy[AE_KEY_LINE_SIZE];
	char *fields[7];
	char *cursor;
	int count = 0;

	if (strlen(line) >= sizeof(copy))
		return 0;
	strcpy(copy, line);
	copy[strcspn(copy, "\r\n")] = 0;
	cursor = copy;
	while (count < 7)
	{
		fields[count++] = cursor;
		cursor = strchr(cursor, '\t');
		if (!cursor)
			break;
		*cursor++ = 0;
	}
	/* (the allow field may be missing or empty) */
	if (count < 6)
		return 0;
	memset(record, 0, sizeof(*record));
	if (strlen(fields[0]) != AE_KEY_ID_LENGTH || !ae_key_is_hex(fields[0], AE_KEY_ID_LENGTH) ||
		!ae_key_name_valid(fields[1]) || ae_key_role_parse(fields[2]) < 0 || strlen(fields[5]) != 64 ||
		!ae_key_is_hex(fields[5], 64))
	{
		return 0;
	}
	if (count == 7 && !ae_key_allow_valid(fields[6]))
		return 0;
	strcpy(record->id, fields[0]);
	strcpy(record->name, fields[1]);
	record->role = ae_key_role_parse(fields[2]);
	record->created = strtoul(fields[3], NULL, 10);
	record->expires = strtoul(fields[4], NULL, 10);
	strcpy(record->hash, fields[5]);
	if (count == 7)
		strcpy(record->allow, fields[6]);
	return 1;
}

static const int warning_steps[AE_WARNING_STEPS] = { 60, 30, 10, 5 };

int ae_warning_due(long total, long left, long last_announced)
{
	int index;

	if (total <= 0 || left <= 0)
		return 0;
	/* the whole delay is announced as the countdown begins */
	if (last_announced == 0)
		return left >= total ? (int)total : 0;
	/* then each step below the last one announced, once the time left has reached it */
	for (index = 0; index < AE_WARNING_STEPS; index++)
	{
		if (warning_steps[index] < last_announced && left <= warning_steps[index])
			return warning_steps[index];
	}
	return 0;
}

int ae_daily_parse(const char *text)
{
	int hour, minute;

	if (!text || strlen(text) != 5 || text[2] != ':')
		return -1;
	if (text[0] < '0' || text[0] > '9' || text[1] < '0' || text[1] > '9' || text[3] < '0' || text[3] > '9' ||
		text[4] < '0' || text[4] > '9')
	{
		return -1;
	}
	hour = (text[0] - '0') * 10 + text[1] - '0';
	minute = (text[3] - '0') * 10 + text[4] - '0';
	if (hour > 23 || minute > 59)
		return -1;
	return hour * 60 + minute;
}

long ae_daily_seconds_until(int minute_of_day, long second_of_day)
{
	long until = (long)minute_of_day * 60 - second_of_day;

	if (until <= 0)
		until += 86400;
	return until;
}

int ae_backup_name(const char *name, int year, int month, int day, int hour, int minute, int second, char *text,
	int size)
{
	int length = snprintf(text, (size_t)size, "%s.%04d%02d%02d-%02d%02d%02d", name, year, month, day, hour, minute,
		second);

	return length > 0 && length < size;
}

int ae_backup_matches(const char *file, const char *entry)
{
	size_t length = strlen(file);
	int index;

	if (strncmp(entry, file, length) || entry[length] != '.' || strlen(entry + length + 1) != 15)
		return 0;
	entry += length + 1;
	for (index = 0; index < 15; index++)
	{
		if (index == 8 ? entry[index] != '-' : (entry[index] < '0' || entry[index] > '9'))
			return 0;
	}
	return 1;
}

long long ae_backup_stamp_time(const char *entry)
{
	const char *stamp = entry ? strrchr(entry, '.') : NULL;
	int digits[14], index, at = 0;
	long long year, month, day, era, year_of_era, day_of_year, day_of_era, days;

	stamp = stamp ? stamp + 1 : entry;
	if (!stamp || strlen(stamp) != 15)
		return -1;
	for (index = 0; index < 15; index++)
	{
		if (index == 8)
		{
			if (stamp[index] != '-')
				return -1;
			continue;
		}
		if (stamp[index] < '0' || stamp[index] > '9')
			return -1;
		digits[at++] = stamp[index] - '0';
	}
	year = digits[0] * 1000 + digits[1] * 100 + digits[2] * 10 + digits[3];
	month = digits[4] * 10 + digits[5];
	day = digits[6] * 10 + digits[7];
	if (month < 1 || month > 12 || day < 1 || day > 31 || digits[8] * 10 + digits[9] > 23 ||
		digits[10] * 10 + digits[11] > 59 || digits[12] * 10 + digits[13] > 60)
	{
		return -1;
	}
	/* (days from 1970-01-01, the proleptic Gregorian calendar's: no timegm, which Windows lacks) */
	year -= month <= 2;
	era = (year >= 0 ? year : year - 399) / 400;
	year_of_era = year - era * 400;
	day_of_year = (153 * (month + (month > 2 ? -3 : 9)) + 2) / 5 + day - 1;
	day_of_era = year_of_era * 365 + year_of_era / 4 - year_of_era / 100 + day_of_year;
	days = era * 146097 + day_of_era - 719468;
	return days * 86400 + (digits[8] * 10 + digits[9]) * 3600LL + (digits[10] * 10 + digits[11]) * 60LL +
		digits[12] * 10 + digits[13];
}

int ae_backup_prune(const char *const *entries, int count, int keep, int *drop)
{
	int index, other, dropped = 0;

	if (keep < 1)
		keep = 1;
	for (index = 0; index < count; index++)
	{
		int newer = 0;

		for (other = 0; other < count; other++)
		{
			if (strcmp(entries[other] + strlen(entries[other]) - 15, entries[index] + strlen(entries[index]) - 15) > 0)
				newer++;
		}
		drop[index] = newer >= keep;
		dropped += drop[index];
	}
	return dropped;
}
