/*
AE_PLAYER_SETTINGS.C

A player's own settings file, its text (ae_player_settings.h).
*/

#include <stdio.h>
#include <string.h>

#include "ae_player_settings.h"

void ae_player_settings_clear(struct ae_player_settings *settings)
{
	memset(settings, 0, sizeof(*settings));
}

static const char *skip_spaces(const char *at, const char *end)
{
	while (at < end && (*at == ' ' || *at == '\t'))
		at++;
	return at;
}

static const char *trim_end(const char *start, const char *end)
{
	while (end > start && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\r'))
		end--;
	return end;
}

/* a value's text: a quoted string unquoted (\" and \\), else up to a comment; 0 when it can't be read */
static int value_text(const char *at, const char *end, char *out, size_t size)
{
	size_t length = 0;

	at = skip_spaces(at, end);
	if (at < end && *at == '"')
	{
		for (at++; at < end && *at != '"'; at++)
		{
			char character = *at;

			if (character == '\\' && at + 1 < end)
				character = *++at;
			if (length + 1 >= size)
				return 0;
			out[length++] = character;
		}
		if (at >= end)
			return 0;
	}
	else
	{
		const char *stop = at;

		while (stop < end && *stop != '#')
			stop++;
		stop = trim_end(at, stop);
		if (stop == at || (size_t)(stop - at) >= size)
			return 0;
		length = (size_t)(stop - at);
		memcpy(out, at, length);
	}
	out[length] = 0;
	return 1;
}

int ae_player_settings_parse(const char *text, struct ae_player_settings *settings)
{
	char section[AE_PLAYER_SETTINGS_NAME] = "";
	int read = 0;

	ae_player_settings_clear(settings);
	while (text && *text)
	{
		const char *end = text + strcspn(text, "\n");
		const char *next = *end ? end + 1 : end;
		const char *at = skip_spaces(text, end);

		if (at < end && *at == '[')
		{
			const char *close = memchr(at, ']', (size_t)(end - at));

			if (close && (size_t)(close - at - 1) < sizeof(section))
				snprintf(section, sizeof(section), "%.*s", (int)(close - at - 1), at + 1);
		}
		else if (at < end && *at != '#')
		{
			const char *equals = memchr(at, '=', (size_t)(end - at));
			char name[AE_PLAYER_SETTINGS_NAME * 2], value[AE_PLAYER_SETTINGS_VALUE];

			if (equals)
			{
				const char *key_end = trim_end(at, equals);

				snprintf(name, sizeof(name), "%s%s%.*s", section, section[0] ? "." : "", (int)(key_end - at), at);
				if (key_end > at && value_text(equals + 1, end, value, sizeof(value)))
				{
					if (!strcmp(name, "ae.version"))
					{
						int version = 0;

						if (sscanf(value, "%d", &version) == 1 && version > 0)
							settings->version = version;
					}
					else if (strlen(name) < AE_PLAYER_SETTINGS_NAME && ae_player_settings_put(settings, name, value))
						read++;
				}
			}
		}
		text = next;
	}
	return read;
}

const char *ae_player_settings_get(struct ae_player_settings const *settings, const char *name)
{
	int index;

	for (index = 0; index < settings->count; index++)
	{
		if (!strcmp(settings->settings[index].name, name))
			return settings->settings[index].value;
	}
	return NULL;
}

int ae_player_settings_put(struct ae_player_settings *settings, const char *name, const char *value)
{
	int index;

	if (!name || !value || !strchr(name, '.') || strlen(name) >= AE_PLAYER_SETTINGS_NAME ||
		strlen(value) >= AE_PLAYER_SETTINGS_VALUE)
		return 0;
	for (index = 0; index < settings->count; index++)
	{
		if (!strcmp(settings->settings[index].name, name))
			break;
	}
	if (index == settings->count)
	{
		if (settings->count >= AE_PLAYER_SETTINGS_MAXIMUM)
			return 0;
		settings->count++;
		snprintf(settings->settings[index].name, sizeof(settings->settings[index].name), "%s", name);
	}
	snprintf(settings->settings[index].value, sizeof(settings->settings[index].value), "%s", value);
	return 1;
}

/* appends; 0 when out of room */
static int append(char *text, size_t size, size_t *length, const char *part)
{
	size_t add = strlen(part);

	if (*length + add + 1 > size)
		return 0;
	memcpy(text + *length, part, add + 1);
	*length += add;
	return 1;
}

int ae_player_settings_write(struct ae_player_settings const *settings, int (*is_string)(const char *name),
	char *text, size_t size)
{
	size_t length = 0;
	char line[AE_PLAYER_SETTINGS_NAME + AE_PLAYER_SETTINGS_VALUE * 2 + 16];
	int done[AE_PLAYER_SETTINGS_MAXIMUM] = { 0 };
	int index;

	if (!size)
		return 0;
	text[0] = 0;
	snprintf(line, sizeof(line), "# Arena Evolved: this player's own settings (SETTINGS: CONTROLS, VIEW, HUD).\n"
		"# The game writes it; a setting not here follows config.toml.\n\n[ae]\nversion = %d\n",
		settings->version > 0 ? settings->version : AE_PLAYER_SETTINGS_VERSION);
	if (!append(text, size, &length, line))
		return 0;
	/* (each section once, with every setting of it, in the order its first was put) */
	for (index = 0; index < settings->count; index++)
	{
		const char *dot = strchr(settings->settings[index].name, '.');
		int section_length, other;

		if (done[index] || !dot)
			continue;
		section_length = (int)(dot - settings->settings[index].name);
		snprintf(line, sizeof(line), "\n[%.*s]\n", section_length, settings->settings[index].name);
		if (!append(text, size, &length, line))
			return 0;
		for (other = index; other < settings->count; other++)
		{
			struct ae_player_setting const *setting = &settings->settings[other];
			char quoted[AE_PLAYER_SETTINGS_VALUE * 2 + 3];
			const char *character;
			size_t at = 0;

			if (done[other] || strncmp(setting->name, settings->settings[index].name, (size_t)section_length + 1))
				continue;
			done[other] = 1;
			if (is_string && is_string(setting->name))
			{
				quoted[at++] = '"';
				for (character = setting->value; *character; character++)
				{
					if (*character == '"' || *character == '\\')
						quoted[at++] = '\\';
					quoted[at++] = *character;
				}
				quoted[at++] = '"';
				quoted[at] = 0;
			}
			else
				snprintf(quoted, sizeof(quoted), "%s", setting->value);
			snprintf(line, sizeof(line), "%s = %s\n", setting->name + section_length + 1, quoted);
			if (!append(text, size, &length, line))
				return 0;
		}
	}
	return 1;
}

int ae_player_settings_profile_has_file(long profile)
{
	return profile != AE_PLAYER_SETTINGS_NO_PROFILE && profile != AE_PLAYER_SETTINGS_UNREAD;
}

int ae_player_settings_needs_seed(int found, int version)
{
	return !found || version < AE_PLAYER_SETTINGS_VERSION;
}

int ae_player_settings_seed(struct ae_player_settings *settings, const char *const *names, int count,
	int (*seed)(const char *name, char *text, size_t size))
{
	int index, put = 0;

	for (index = 0; index < count; index++)
	{
		char value[AE_PLAYER_SETTINGS_VALUE];

		if (!names[index] || ae_player_settings_get(settings, names[index]))
			continue;
		if (seed && seed(names[index], value, sizeof(value)) && ae_player_settings_put(settings, names[index], value))
			put++;
	}
	settings->version = AE_PLAYER_SETTINGS_VERSION;
	return put;
}
