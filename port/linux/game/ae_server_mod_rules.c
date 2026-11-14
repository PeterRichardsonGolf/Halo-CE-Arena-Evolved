#include "ae_server_mod_rules.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---------- small helpers */

static char lower(char c)
{
	return c >= 'A' && c <= 'Z' ? (char)(c + 32) : c;
}

static int is_printable(const char *text)
{
	for (; *text; text++)
	{
		if ((unsigned char)*text < 0x20 || (unsigned char)*text >= 0x7F)
			return 0;
	}
	return 1;
}

/* the line cut at a comment (a "#" at the start or after a space or tab) and trimmed; into out */
static int clean_line(const char *line, char *out, int size)
{
	int length = 0, index;

	for (index = 0; line[index] && line[index] != '\n' && line[index] != '\r'; index++)
	{
		if (line[index] == '#' && (index == 0 || line[index - 1] == ' ' || line[index - 1] == '\t'))
			break;
		if (length < size - 1)
			out[length++] = line[index];
		else
			return 0;
	}
	while (length > 0 && (out[length - 1] == ' ' || out[length - 1] == '\t'))
		length--;
	out[length] = 0;
	for (index = 0; out[index] == ' ' || out[index] == '\t'; index++)
	{
	}
	if (index)
		memmove(out, out + index, strlen(out + index) + 1);
	return out[0] != 0;
}

/* the text up to the next tab, copied into a field; the cursor after it */
static char *next_field(char **cursor)
{
	char *field = *cursor, *tab;

	if (!field)
		return NULL;
	tab = strchr(field, '\t');
	if (tab)
	{
		*tab = 0;
		*cursor = tab + 1;
	}
	else
	{
		*cursor = NULL;
	}
	return field;
}

static int number_parse(const char *text, unsigned long *value)
{
	char *end;

	if (!text || !text[0] || strlen(text) > 12 || text[0] < '0' || text[0] > '9')
		return 0;
	*value = strtoul(text, &end, 10);
	return !*end;
}

/* ---------- roles and immunity */

static const char *const role_names[AE_MOD_ROLE_OWNER + 1] = { "viewer", "moderator", "admin", "owner" };

int ae_mod_role_parse(const char *text)
{
	int role;

	if (!text)
		return -1;
	for (role = 0; role <= AE_MOD_ROLE_OWNER; role++)
	{
		if (!strcmp(text, role_names[role]))
			return role;
	}
	return -1;
}

const char *ae_mod_role_text(int role)
{
	return role >= 0 && role <= AE_MOD_ROLE_OWNER ? role_names[role] : "none";
}

void ae_mod_standing_merge(int *role, int *immunity, int other_role, int other_immunity)
{
	if (other_role < 0)
		return;
	if (*role < 0 || other_role > *role || (other_role == *role && other_immunity > *immunity))
	{
		*role = other_role;
		*immunity = other_immunity;
	}
}

int ae_mod_may_act(int actor_role, int actor_immunity, int target_role, int target_immunity)
{
	if (actor_role < 0)
		return 0;
	if (actor_role >= AE_MOD_ROLE_OWNER)
		return 1;
	if (target_role < 0)
		return 1;
	if (actor_role > target_role)
		return 1;
	return actor_role == target_role && actor_immunity > target_immunity;
}

/* ---------- admins.txt */

int ae_admin_line_format(const struct ae_admin_record *record, char *line, int size)
{
	int length;

	if (record->kind == 'n')
		length = snprintf(line, (size_t)size, "name:%s\t%s\t%d\t%lu\t%s\t%s\n", record->name,
			ae_mod_role_text(record->role), record->immunity, record->expires, record->hash, record->allow);
	else if (record->kind == 'k')
		length = snprintf(line, (size_t)size, "key:%s\t%s\t%d\t%lu\t\t\n", record->key,
			ae_mod_role_text(record->role), record->immunity, record->expires);
	else
		length = snprintf(line, (size_t)size, "cidr:%s\t%s\t%d\t%lu\t\t\n", record->cidr,
			ae_mod_role_text(record->role), record->immunity, record->expires);
	return length > 0 && length < size;
}

int ae_admin_line_parse(const char *line, struct ae_admin_record *record)
{
	char copy[AE_ADMIN_LINE_SIZE];
	char *fields[6], *cursor;
	unsigned long number;
	int index, role;

	if (!line || line[0] == '#' || strlen(line) >= sizeof(copy))
		return 0;
	strcpy(copy, line);
	copy[strcspn(copy, "\r\n")] = 0;
	if (!copy[0])
		return 0;
	cursor = copy;
	for (index = 0; index < 6; index++)
	{
		fields[index] = next_field(&cursor);
		if (!fields[index])
			fields[index] = (char *)"";
	}
	memset(record, 0, sizeof(*record));
	role = ae_mod_role_parse(fields[1]);
	if (role < 0 || role > 2)
		return 0;
	record->role = role;
	if (!number_parse(fields[2], &number) || number > AE_IMMUNITY_MAXIMUM)
		return 0;
	record->immunity = (int)number;
	if (!number_parse(fields[3], &number))
		return 0;
	record->expires = number;
	if (!strncmp(fields[0], "name:", 5))
	{
		record->kind = 'n';
		if (!ae_key_name_valid(fields[0] + 5))
			return 0;
		strcpy(record->name, fields[0] + 5);
		if (strlen(fields[4]) != 64 || !ae_key_is_hex(fields[4], 64))
			return 0;
		strcpy(record->hash, fields[4]);
		if (!ae_key_allow_valid(fields[5]))
			return 0;
		strcpy(record->allow, fields[5]);
	}
	else if (!strncmp(fields[0], "key:", 4))
	{
		int at;

		record->kind = 'k';
		if (strlen(fields[0] + 4) != 64)
			return 0;
		for (at = 0; at < 64; at++)
		{
			char character = fields[0][4 + at];

			if (character >= 'A' && character <= 'F')
				character = (char)(character - 'A' + 'a');
			if (!((character >= '0' && character <= '9') || (character >= 'a' && character <= 'f')))
				return 0;
			record->key[at] = character;
		}
		record->key[64] = 0;
	}
	else if (!strncmp(fields[0], "cidr:", 5))
	{
		record->kind = 'c';
		if (!fields[0][5] || !ae_key_allow_valid(fields[0] + 5))
			return 0;
		strcpy(record->cidr, fields[0] + 5);
	}
	else
	{
		return 0;
	}
	return 1;
}

int ae_admin_expired(const struct ae_admin_record *record, unsigned long now)
{
	return ae_key_expired(record->expires, now);
}

/* ---------- command_access.txt */

int ae_access_line_parse(const char *line, char *command, int size, int *role)
{
	char text[160];
	char *space;
	int length, index, parsed;

	if (!line || !clean_line(line, text, sizeof(text)))
		return 0;
	space = strpbrk(text, " \t");
	if (!space)
		return 0;
	length = (int)(space - text);
	if (length < 1 || length > AE_COMMAND_NAME_MAXIMUM || length >= size)
		return 0;
	for (index = 0; index < length; index++)
	{
		char c = lower(text[index]);

		if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_'))
			return 0;
		command[index] = c;
	}
	command[length] = 0;
	while (*space == ' ' || *space == '\t')
		space++;
	parsed = ae_mod_role_parse(space);
	if (parsed < 0)
		return 0;
	*role = parsed;
	return 1;
}

/* ---------- whitelist.txt */

static int hardware_id_clean(const char *text, char *out, int size)
{
	int length = (int)strlen(text), index;

	if (length < 1 || length > 32 || length >= size)
		return 0;
	for (index = 0; index < length; index++)
	{
		char c = lower(text[index]);

		if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f')))
			return 0;
		out[index] = c;
	}
	out[length] = 0;
	return 1;
}

static int digits_valid(const char *text, int maximum)
{
	int length = (int)strlen(text), index;

	if (length < 1 || length > maximum)
		return 0;
	for (index = 0; index < length; index++)
	{
		if (text[index] < '0' || text[index] > '9')
			return 0;
	}
	return 1;
}

int ae_cidr_normalise(const char *text, char *kept, int size)
{
	unsigned int bytes[4], bits = 32;
	char end;
	int matched;

	if (!text || strlen(text) > 18 || strspn(text, "0123456789./") != strlen(text))
		return 0;
	if (strchr(text, '/'))
	{
		matched = sscanf(text, "%3u.%3u.%3u.%3u/%2u%c", &bytes[0], &bytes[1], &bytes[2], &bytes[3], &bits, &end);
		if (matched != 5 || bits > 32 || bits == 0)
			return 0;
	}
	else
	{
		matched = sscanf(text, "%3u.%3u.%3u.%3u%c", &bytes[0], &bytes[1], &bytes[2], &bytes[3], &end);
		if (matched != 4)
			return 0;
	}
	if (bytes[0] > 255 || bytes[1] > 255 || bytes[2] > 255 || bytes[3] > 255)
		return 0;
	if (bits == 32)
		return snprintf(kept, (size_t)size, "%u.%u.%u.%u", bytes[0], bytes[1], bytes[2], bytes[3]) < size;
	return snprintf(kept, (size_t)size, "%u.%u.%u.%u/%u", bytes[0], bytes[1], bytes[2], bytes[3], bits) < size;
}

int ae_whitelist_line_parse(const char *line, struct ae_whitelist_entry *entry)
{
	char text[160];

	if (!line || !clean_line(line, text, sizeof(text)))
		return 0;
	memset(entry, 0, sizeof(*entry));
	if (!strncmp(text, "ip:", 3))
	{
		entry->kind = AE_WHITELIST_IP;
		return ae_cidr_normalise(text + 3, entry->value, sizeof(entry->value));
	}
	if (!strncmp(text, "hwid:", 5))
	{
		entry->kind = AE_WHITELIST_HWID;
		return hardware_id_clean(text + 5, entry->value, sizeof(entry->value));
	}
	if (!strncmp(text, "discord:", 8))
	{
		entry->kind = AE_WHITELIST_DISCORD;
		if (!digits_valid(text + 8, 20))
			return 0;
		strcpy(entry->value, text + 8);
		return 1;
	}
	if (!strncmp(text, "name:", 5))
	{
		entry->kind = AE_WHITELIST_NAME;
		if (!text[5] || strlen(text + 5) > AE_NAME_MAXIMUM || !is_printable(text + 5))
			return 0;
		strcpy(entry->value, text + 5);
		return 1;
	}
	return 0;
}

int ae_whitelist_line_format(const struct ae_whitelist_entry *entry, char *line, int size)
{
	static const char *const kinds[] = { "", "ip", "hwid", "discord", "name" };

	if (entry->kind < AE_WHITELIST_IP || entry->kind > AE_WHITELIST_NAME)
		return 0;
	return snprintf(line, (size_t)size, "%s:%s\n", kinds[entry->kind], entry->value) < size;
}

static int name_equal(const char *first, const char *second)
{
	for (; *first && *second; first++, second++)
	{
		if (lower(*first) != lower(*second))
			return 0;
	}
	return *first == *second;
}

static int network_holds(const char *network, unsigned long address)
{
	return ae_key_allow_permits(network, address & 0xFFFFFFFFUL);
}

int ae_whitelist_door_ok(const struct ae_whitelist_entry *entries, int count, unsigned long address,
	const char *hardware_id, const char *discord_id)
{
	int index;

	for (index = 0; index < count; index++)
	{
		const struct ae_whitelist_entry *entry = &entries[index];

		if (entry->kind == AE_WHITELIST_IP && address && network_holds(entry->value, address))
			return 1;
		if (entry->kind == AE_WHITELIST_HWID && hardware_id && hardware_id[0] && name_equal(entry->value, hardware_id))
			return 1;
		if (entry->kind == AE_WHITELIST_DISCORD && discord_id && discord_id[0] && !strcmp(entry->value, discord_id))
			return 1;
	}
	return 0;
}

int ae_whitelist_has_names(const struct ae_whitelist_entry *entries, int count)
{
	int index;

	for (index = 0; index < count; index++)
	{
		if (entries[index].kind == AE_WHITELIST_NAME)
			return 1;
	}
	return 0;
}

int ae_whitelist_name_ok(const struct ae_whitelist_entry *entries, int count, const char *name)
{
	int index;

	for (index = 0; index < count; index++)
	{
		if (entries[index].kind == AE_WHITELIST_NAME && name && name_equal(entries[index].value, name))
			return 1;
	}
	return 0;
}

/* ---------- bans by network and by name */

int ae_pattern_valid(const char *pattern)
{
	int length, index, literal = 0;

	if (!pattern)
		return 0;
	length = (int)strlen(pattern);
	if (length < 1 || length > AE_PATTERN_MAXIMUM || !is_printable(pattern))
		return 0;
	for (index = 0; index < length; index++)
	{
		if (pattern[index] == '\t')
			return 0;
		if (pattern[index] != '*' && pattern[index] != '?')
			literal++;
	}
	return literal >= 1;
}

int ae_glob_match(const char *pattern, const char *text)
{
	const char *star = NULL, *mark = NULL;

	while (*text)
	{
		if (*pattern == '*')
		{
			star = pattern++;
			mark = text;
		}
		else if (*pattern == '?' || (*pattern && lower(*pattern) == lower(*text)))
		{
			pattern++;
			text++;
		}
		else if (star)
		{
			pattern = star + 1;
			text = ++mark;
		}
		else
		{
			return 0;
		}
	}
	while (*pattern == '*')
		pattern++;
	return *pattern == 0;
}

/* a field of a bans.txt line (its tab, name and "="), into text; 1 if there is one */
static int ban_field(const char *line, const char *name, char *text, int size)
{
	const char *at = line;
	int name_length = (int)strlen(name), length;

	while (at && *at)
	{
		if ((at == line || at[-1] == '\t') && !strncmp(at, name, (size_t)name_length) && at[name_length] == '=')
		{
			at += name_length + 1;
			length = (int)strcspn(at, "\t\r\n");
			if (length >= size)
				length = size - 1;
			memcpy(text, at, (size_t)length);
			text[length] = 0;
			return 1;
		}
		at = strchr(at, '\t');
		if (at)
			at++;
	}
	text[0] = 0;
	return 0;
}

static void text_keep(char *out, int size, const char *text)
{
	int length = 0;

	for (; text && *text && length < size - 1; text++)
	{
		unsigned char c = (unsigned char)*text;

		out[length++] = c >= 0x20 && c < 0x7F && c != '\t' ? (char)c : '?';
	}
	out[length] = 0;
}

int ae_ban_line_format(const struct ae_ban_record *record, char *line, int size)
{
	char players[AE_BAN_FIELD_SIZE], reason[AE_BAN_FIELD_SIZE];
	int length;

	text_keep(players, sizeof(players), record->players);
	text_keep(reason, sizeof(reason), record->reason);
	length = snprintf(line, (size_t)size, "%s\tip=%s\thwid=%s\tdiscord_username=none\tdiscord_id=%s\tplayers=%s\treason=%s",
		record->when, record->ip[0] ? record->ip : "none", record->hwid[0] ? record->hwid : "none",
		record->discord_id[0] ? record->discord_id : "none", players, reason);
	if (length > 0 && length < size && record->pattern[0])
		length += snprintf(line + length, (size_t)(size - length), "\tnamepat=%s", record->pattern);
	if (length > 0 && length < size && record->until)
		length += snprintf(line + length, (size_t)(size - length), "\tuntil=%lu", record->until);
	if (length > 0 && length < size - 1)
	{
		line[length++] = '\n';
		line[length] = 0;
		return 1;
	}
	return 0;
}

int ae_ban_line_parse(const char *line, struct ae_ban_record *record)
{
	char text[16], when[24];
	unsigned long until = 0;
	int has_ip, has_hwid, has_pattern;

	memset(record, 0, sizeof(*record));
	if (!line || line[0] == '#')
		return 0;
	has_ip = ban_field(line, "ip", record->ip, sizeof(record->ip));
	has_hwid = ban_field(line, "hwid", record->hwid, sizeof(record->hwid));
	has_pattern = ban_field(line, "namepat", record->pattern, sizeof(record->pattern));
	if (!has_ip && !has_hwid && !has_pattern)
		return 0;
	if (!strcmp(record->ip, "none"))
		record->ip[0] = 0;
	if (!strcmp(record->hwid, "none"))
		record->hwid[0] = 0;
	ban_field(line, "discord_id", record->discord_id, sizeof(record->discord_id));
	/* (a self-reported Discord id is not a ban's: only the number is kept) */
	record->discord_id[strcspn(record->discord_id, " ")] = 0;
	if (!strcmp(record->discord_id, "none"))
		record->discord_id[0] = 0;
	ban_field(line, "players", record->players, sizeof(record->players));
	ban_field(line, "reason", record->reason, sizeof(record->reason));
	if (ban_field(line, "until", text, sizeof(text)) && number_parse(text, &until))
		record->until = until;
	{
		char first[24];
		int cut = (int)strcspn(line, "\t\r\n");

		if (cut >= (int)sizeof(first))
			cut = (int)sizeof(first) - 1;
		memcpy(first, line, (size_t)cut);
		first[cut] = 0;
		text_keep(when, sizeof(when), first);
		memcpy(record->when, when, sizeof(record->when) - 1);
	}
	return 1;
}

int ae_ban_over(const struct ae_ban_record *record, unsigned long now)
{
	return record->until != 0 && record->until <= now;
}

int ae_ban_record_clean(struct ae_ban_record *record)
{
	char kept[40];

	if (record->ip[0] && strcmp(record->ip, "none"))
	{
		if (!ae_cidr_normalise(record->ip, kept, sizeof(kept)))
			return 0;
		strcpy(record->ip, kept);
	}
	else
	{
		record->ip[0] = 0;
	}
	if (record->hwid[0] && strcmp(record->hwid, "none"))
	{
		if (!hardware_id_clean(record->hwid, kept, sizeof(kept)))
			return 0;
		strcpy(record->hwid, kept);
	}
	else
	{
		record->hwid[0] = 0;
	}
	if (record->discord_id[0] && strcmp(record->discord_id, "none"))
	{
		if (!digits_valid(record->discord_id, 20))
			return 0;
	}
	else
	{
		record->discord_id[0] = 0;
	}
	if (record->pattern[0] && !ae_pattern_valid(record->pattern))
		return 0;
	text_keep(record->players, sizeof(record->players), record->players);
	text_keep(record->reason, sizeof(record->reason), record->reason);
	if (!record->ip[0] && !record->hwid[0] && !record->discord_id[0] && !record->pattern[0])
		return 0;
	if (!record->when[0])
		strcpy(record->when, "imported");
	text_keep(record->when, sizeof(record->when), record->when);
	return 1;
}

int ae_ban_same(const struct ae_ban_record *first, const struct ae_ban_record *second)
{
	return !strcmp(first->ip, second->ip) && !strcmp(first->hwid, second->hwid) &&
		!strcmp(first->discord_id, second->discord_id) && name_equal(first->pattern, second->pattern);
}

int ae_ban_network_holds(const struct ae_ban_record *record, unsigned long address)
{
	if (!strchr(record->ip, '/'))
		return 0;
	return address != 0 && ae_key_allow_permits(record->ip, address & 0xFFFFFFFFUL);
}

/* ---------- a chat flood */

int ae_flood_note(struct ae_flood *flood, unsigned long now_ms, int limit)
{
	int index, within = 0;

	if (limit <= 0)
		return 0;
	if (limit >= AE_FLOOD_STAMPS)
		limit = AE_FLOOD_STAMPS - 1;
	flood->stamps[flood->head] = now_ms;
	flood->head = (flood->head + 1) % AE_FLOOD_STAMPS;
	if (flood->count < AE_FLOOD_STAMPS)
		flood->count++;
	for (index = 0; index < flood->count; index++)
	{
		if (now_ms - flood->stamps[index] < (unsigned long)AE_FLOOD_WINDOW_MS)
			within++;
	}
	if (within > limit)
	{
		memset(flood, 0, sizeof(*flood));
		return 1;
	}
	return 0;
}

/* ---------- a ping watch */

int ae_ping_watch_note(struct ae_ping_watch *watch, long ping_ms, long limit_ms, long seconds, unsigned long now_ms)
{
	if (limit_ms <= 0 || ping_ms < 0 || ping_ms <= limit_ms)
	{
		watch->over = 0;
		return 0;
	}
	if (!watch->over)
	{
		watch->over = 1;
		watch->over_since = now_ms;
		return 0;
	}
	if (seconds < 1)
		seconds = 1;
	return now_ms - watch->over_since >= (unsigned long)seconds * 1000UL;
}

/* ---------- event hooks */

int ae_hook_line_parse(const char *line, struct ae_hook_rule *rule)
{
	char text[AE_HOOK_COMMANDS_SIZE + 160];
	char *arrow, *cursor, *open;
	int length;

	if (!line || !clean_line(line, text, sizeof(text)))
		return 0;
	arrow = strstr(text, "->");
	if (!arrow)
		return 0;
	memset(rule, 0, sizeof(*rule));
	*arrow = 0;
	cursor = arrow + 2;
	while (*cursor == ' ' || *cursor == '\t')
		cursor++;
	if (!*cursor || strlen(cursor) >= sizeof(rule->commands))
		return 0;
	strcpy(rule->commands, cursor);
	length = (int)strlen(text);
	while (length > 0 && (text[length - 1] == ' ' || text[length - 1] == '\t'))
		text[--length] = 0;
	open = strchr(text, '[');
	if (open)
	{
		char *equals, *close = strchr(open, ']');

		if (!close || close[1] || !(equals = strchr(open, '=')) || equals > close)
			return 0;
		*open = 0;
		*equals = 0;
		*close = 0;
		if (strlen(open + 1) < 1 || strlen(open + 1) >= sizeof(rule->field) || strlen(equals + 1) >= sizeof(rule->value) ||
			!is_printable(equals + 1))
		{
			return 0;
		}
		strcpy(rule->field, open + 1);
		strcpy(rule->value, equals + 1);
		for (length = 0; rule->field[length]; length++)
		{
			char c = rule->field[length];

			if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_'))
				return 0;
		}
	}
	length = (int)strlen(text);
	if (length < 1 || length > AE_KEY_NAME_MAXIMUM)
		return 0;
	for (cursor = text; *cursor; cursor++)
	{
		if (!((*cursor >= 'a' && *cursor <= 'z') || (*cursor >= '0' && *cursor <= '9') || *cursor == '_'))
			return 0;
	}
	strcpy(rule->event, text);
	return 1;
}

int ae_hook_split(const struct ae_hook_rule *rule, char commands[][AE_HOOK_COMMAND_SIZE])
{
	const char *cursor = rule->commands;
	int count = 0;

	while (*cursor && count < AE_HOOK_COMMAND_MAXIMUM)
	{
		int length = (int)strcspn(cursor, ";");
		const char *start = cursor;
		int end = length;

		while (length > 0 && (*start == ' ' || *start == '\t'))
		{
			start++;
			length--;
		}
		while (length > 0 && (start[length - 1] == ' ' || start[length - 1] == '\t'))
			length--;
		if (length > 0 && length < AE_HOOK_COMMAND_SIZE)
		{
			memcpy(commands[count], start, (size_t)length);
			commands[count][length] = 0;
			count++;
		}
		cursor += end;
		if (*cursor == ';')
			cursor++;
	}
	return count;
}

int ae_hook_expand(const char *command, ae_hook_lookup lookup, void *context, char *out, int size)
{
	int length = 0;

	while (*command)
	{
		if (*command == '{')
		{
			const char *close = strchr(command, '}');
			char field[32], value[AE_HOOK_VALUE_SIZE];
			int field_length, index;

			if (close && close - command - 1 >= 1 && close - command - 1 < (int)sizeof(field))
			{
				field_length = (int)(close - command - 1);
				memcpy(field, command + 1, (size_t)field_length);
				field[field_length] = 0;
				value[0] = 0;
				if (!lookup || !lookup(context, field, value, sizeof(value)))
					value[0] = 0;
				for (index = 0; value[index]; index++)
				{
					unsigned char c = (unsigned char)value[index];

					if (length >= size - 1)
						return 0;
					out[length++] = c < 0x20 || c >= 0x7F || c == '"' || c == ';' || c == '{' || c == '}' || c == '\\' ?
						'_' : (char)c;
				}
				command = close + 1;
				continue;
			}
		}
		if (length >= size - 1)
			return 0;
		out[length++] = *command++;
	}
	out[length] = 0;
	return 1;
}

int ae_hook_fires(const struct ae_hook_rule *rule, const char *event, ae_hook_lookup lookup, void *context)
{
	char value[AE_HOOK_VALUE_SIZE];

	if (!event || strcmp(rule->event, event))
		return 0;
	if (!rule->field[0])
		return 1;
	value[0] = 0;
	if (!lookup || !lookup(context, rule->field, value, sizeof(value)))
		return 0;
	return name_equal(rule->value, value);
}

int ae_hook_allow(unsigned long *window_start, int *used, unsigned long now_ms, int limit, unsigned long window_ms)
{
	if (now_ms - *window_start >= window_ms)
	{
		*window_start = now_ms;
		*used = 0;
	}
	if (*used >= limit)
		return 0;
	(*used)++;
	return 1;
}

/* ---------- the alert webhook */

int ae_webhook_url_valid(const char *url)
{
	const char *host;
	size_t length, host_length;

	if (!url)
		return 0;
	length = strlen(url);
	if (length < 9 || length > 256)
		return 0;
	for (host = url; *host; host++)
	{
		if ((unsigned char)*host <= 0x20 || (unsigned char)*host >= 0x7F || *host == '"' || *host == '\\')
			return 0;
	}
	if (!strncmp(url, "https://", 8))
		host = url + 8;
	else if (!strncmp(url, "http://", 7))
		host = url + 7;
	else
		return 0;
	host_length = strcspn(host, ":/?#");
	if (host_length < 1)
		return 0;
	if (url[4] == ':')
	{
		/* plain http: this machine alone */
		if (!((host_length == 9 && !strncmp(host, "127.0.0.1", 9)) || (host_length == 9 && !strncmp(host, "localhost", 9))))
			return 0;
	}
	return 1;
}

int ae_json_escape(const char *text, char *out, int size)
{
	int length = 0;

	for (; *text; text++)
	{
		unsigned char c = (unsigned char)*text;

		if (c == '"' || c == '\\')
		{
			if (length + 2 >= size)
				return 0;
			out[length++] = '\\';
			out[length++] = (char)c;
		}
		else
		{
			if (length + 1 >= size)
				return 0;
			out[length++] = c < 0x20 ? ' ' : (char)c;
		}
	}
	out[length] = 0;
	return 1;
}

int ae_alert_body(const char *server, const char *kind, const char *text, unsigned long now, char *out, int size)
{
	char name[96], event[64], message[2 * AE_ALERT_TEXT_SIZE];
	int length;

	if (!ae_json_escape(server, name, sizeof(name)) || !ae_json_escape(kind, event, sizeof(event)) ||
		!ae_json_escape(text, message, sizeof(message)))
	{
		return 0;
	}
	length = snprintf(out, (size_t)size, "{\"source\":\"arena-evolved-server\",\"server\":\"%s\",\"event\":\"%s\","
		"\"text\":\"%s\",\"content\":\"%s: %s\",\"time\":%lu}", name, event, message, name, message, now);
	return length > 0 && length < size;
}

int ae_restart_loop(const unsigned long *starts, int number, unsigned long now, long window_s, int count)
{
	int index, within = 0;

	if (count < 1)
		return 0;
	for (index = 0; index < number; index++)
	{
		if (starts[index] <= now && now - starts[index] < (unsigned long)window_s)
			within++;
	}
	return within >= count;
}

int ae_alert_gap_ok(unsigned long last, unsigned long now, long min_gap)
{
	return last == 0 || now < last || now - last >= (unsigned long)min_gap;
}

int ae_disk_low(unsigned long long free_bytes, long threshold_mb)
{
	return threshold_mb > 0 && free_bytes < (unsigned long long)threshold_mb * 1024ULL * 1024ULL;
}

int ae_empty_alert_due(unsigned long empty_since, unsigned long now, long hours, int alerted)
{
	return hours > 0 && !alerted && empty_since != 0 && now >= empty_since &&
		now - empty_since >= (unsigned long)hours * 3600UL;
}

/* ---------- the first-run claim */

int ae_claim_token_valid(const char *token)
{
	return token && strlen(token) == AE_CLAIM_TOKEN_LENGTH && ae_key_is_hex(token, AE_CLAIM_TOKEN_LENGTH);
}

int ae_claim_password_valid(const char *password)
{
	size_t length;

	if (!password)
		return 0;
	length = strlen(password);
	return length >= AE_CLAIM_PASSWORD_MINIMUM && length <= AE_CLAIM_PASSWORD_MAXIMUM && is_printable(password);
}

int ae_claim_link(const char *bind, long port, char *out, int size)
{
	const char *host = bind && bind[0] && strcmp(bind, "0.0.0.0") && strcmp(bind, "::") ? bind : "localhost";

	return snprintf(out, (size_t)size, "http://%s:%ld/claim", host, port) < size;
}
