/* HOST A SERVER: the pure rules (see the header) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ae_host_dedicated_rules.h"

/* a decimal stamp (unsigned, 64-bit); the game's files build as C89 on macOS, where <stdlib.h> declares no strtoull */
static unsigned long long read_decimal(char const *at)
{
	unsigned long long value = 0;

	while (*at >= '0' && *at <= '9')
		value = value * 10 + (unsigned long long)(*at++ - '0');
	return value;
}

static int const bots_steps[AE_HD_BOTS_STEPS] = { 0, 2, 4, 6, 8, 12, 16 };

static void copy_text(char *to, int size, char const *from)
{
	int length = 0;

	for (; from && *from && length < size - 1; from++)
	{
		unsigned char character = (unsigned char)*from;

		if (character >= 0x20 && character < 0x7F)
			to[length++] = (char)character;
	}
	if (size > 0)
		to[length] = 0;
}

void ae_host_dedicated_choices_init(struct ae_hd_choices *choices, char const *name)
{
	memset(choices, 0, sizeof(*choices));
	copy_text(choices->name, (int)sizeof(choices->name), name && name[0] ? name : "My Server");
	choices->public_listing = 0;
	choices->max_players = 16;
	choices->bots = 8;
	choices->join_when_ready = 1;
	choices->tunnel_port = AE_HD_TUNNEL_FIRST;
	choices->web_port = AE_HD_WEB_FIRST;
}

/* a TOML basic string: quotes and backslashes escaped, control characters and non-ASCII dropped */
static int put_string(char *text, int size, int used, char const *key, char const *value)
{
	int length;

	length = snprintf(text + used, (size_t)(size - used), "%s = \"", key);
	if (length < 0 || used + length >= size)
		return -1;
	used += length;
	for (; value && *value; value++)
	{
		unsigned char character = (unsigned char)*value;

		if (character < 0x20 || character >= 0x7F)
			continue;
		if (character == '"' || character == '\\')
		{
			if (used + 2 >= size)
				return -1;
			text[used++] = '\\';
		}
		if (used + 1 >= size)
			return -1;
		text[used++] = (char)character;
	}
	if (used + 2 >= size)
		return -1;
	text[used++] = '"';
	text[used++] = '\n';
	text[used] = 0;
	return used;
}

static int put_line(char *text, int size, int used, char const *line)
{
	int length = snprintf(text + used, (size_t)(size - used), "%s\n", line);

	return length < 0 || used + length >= size ? -1 : used + length;
}

int ae_host_dedicated_config_text(struct ae_hd_choices const *choices, char *text, int size)
{
	char line[96];
	int used = 0;

	if (!choices || !text || size <= 0)
		return -1;
	text[0] = 0;
	used = put_line(text, size, used, "# Written by Arena Evolved's HOST A SERVER at each start: its menus set these.");
	if (used >= 0)
		used = put_line(text, size, used, "[server]");
	if (used >= 0)
		used = put_string(text, size, used, "name", choices->name);
	/* (private by default: LAN and invites; never in the browser's lists unless PUBLIC) */
	snprintf(line, sizeof(line), "public = %s", choices->public_listing ? "true" : "false");
	if (used >= 0)
		used = put_line(text, size, used, line);
	snprintf(line, sizeof(line), "max_players = %d", choices->max_players < AE_HD_MAX_PLAYERS_FIRST ?
		AE_HD_MAX_PLAYERS_FIRST : choices->max_players > AE_HD_MAX_PLAYERS_LAST ? AE_HD_MAX_PLAYERS_LAST :
		choices->max_players);
	if (used >= 0)
		used = put_line(text, size, used, line);
	if (used >= 0)
		used = put_string(text, size, used, "bots_fill", choices->bots > 0 ? "minimum" : "off");
	snprintf(line, sizeof(line), "min_players = %d", choices->bots > 0 ? (choices->bots > 16 ? 16 : choices->bots) : 1);
	if (used >= 0)
		used = put_line(text, size, used, line);
	if (used >= 0 && choices->rotation[0])
		used = put_string(text, size, used, "rotation", choices->rotation);
	/* (its web page on this PC only; the status file the menus read) */
	if (used >= 0)
		used = put_string(text, size, used, "web_bind", "127.0.0.1");
	snprintf(line, sizeof(line), "web_port = %d", choices->web_port);
	if (used >= 0)
		used = put_line(text, size, used, line);
	if (used >= 0)
		used = put_string(text, size, used, "status_file", choices->status_file);
	if (used >= 0)
		used = put_line(text, size, used, "console_exit_on_eof = false");
	/* (the menus' own game updates it: the server is the same program) */
	if (used >= 0)
		used = put_string(text, size, used, "auto_update", "off");
	if (used >= 0)
		used = put_line(text, size, used, "[network]");
	snprintf(line, sizeof(line), "tunnel_port = %d", choices->tunnel_port);
	if (used >= 0)
		used = put_line(text, size, used, line);
	if (used >= 0 && choices->mcc_path[0])
	{
		used = put_line(text, size, used, "[game]");
		if (used >= 0)
			used = put_string(text, size, used, "mcc_path", choices->mcc_path);
	}
	return used;
}

int ae_host_dedicated_ports(int (*free_port)(int port, int tcp, void *context), void *context, int *tunnel, int *web)
{
	int port;

	*tunnel = *web = 0;
	if (!free_port)
		return 0;
	for (port = AE_HD_TUNNEL_FIRST; port <= AE_HD_TUNNEL_LAST && !*tunnel; port++)
		if (free_port(port, 0, context))
			*tunnel = port;
	for (port = AE_HD_WEB_FIRST; port <= AE_HD_WEB_LAST && !*web; port++)
		if (free_port(port, 1, context))
			*web = port;
	return *tunnel && *web;
}

int ae_host_dedicated_step(int state, int status_ready, int child_alive, unsigned long waited_ms)
{
	switch (state)
	{
	case AE_HD_STARTING:
		if (!child_alive)
			return AE_HD_FAILED;
		if (status_ready)
			return AE_HD_READY;
		return waited_ms >= AE_HD_START_TIMEOUT_MS ? AE_HD_FAILED : AE_HD_STARTING;
	case AE_HD_READY:
		return child_alive ? AE_HD_READY : AE_HD_IDLE;
	case AE_HD_STOPPING:
		return child_alive ? AE_HD_STOPPING : AE_HD_IDLE;
	}
	return state;
}

/* ---------- the rows' values */

int ae_host_dedicated_bots_step(int bots, int direction)
{
	int index, at = 0;

	for (index = 0; index < AE_HD_BOTS_STEPS; index++)
		if (bots_steps[index] <= bots)
			at = index;
	at += direction < 0 ? -1 : 1;
	if (at < 0)
		at = 0;
	if (at >= AE_HD_BOTS_STEPS)
		at = AE_HD_BOTS_STEPS - 1;
	return bots_steps[at];
}

void ae_host_dedicated_bots_word(int bots, char *text, int size)
{
	if (size <= 0)
		return;
	if (bots <= 0)
		snprintf(text, (size_t)size, "OFF");
	else
		snprintf(text, (size_t)size, "UP TO %d", bots);
}

int ae_host_dedicated_players_step(int players, int direction)
{
	players += direction < 0 ? -1 : 1;
	if (players < AE_HD_MAX_PLAYERS_FIRST)
		players = AE_HD_MAX_PLAYERS_FIRST;
	if (players > AE_HD_MAX_PLAYERS_LAST)
		players = AE_HD_MAX_PLAYERS_LAST;
	return players;
}

int ae_host_dedicated_rotation_line(char const *playlist, char *line, int size)
{
	int used = 0, count = 0;

	if (size <= 0)
		return 0;
	line[0] = 0;
	while (playlist && *playlist)
	{
		char entry[160];
		int length = (int)strcspn(playlist, "\r\n"), take = 0, index;
		char const *hash;

		/* (the line's comment cut, its ends trimmed; a comma would split an entry: none taken) */
		hash = memchr(playlist, '#', (size_t)length);
		take = hash ? (int)(hash - playlist) : length;
		while (take > 0 && (playlist[take - 1] == ' ' || playlist[take - 1] == '\t'))
			take--;
		index = 0;
		while (index < take && (playlist[index] == ' ' || playlist[index] == '\t'))
			index++;
		if (take - index > 0 && take - index < (int)sizeof(entry) && !memchr(playlist + index, ',', (size_t)(take - index)))
		{
			memcpy(entry, playlist + index, (size_t)(take - index));
			entry[take - index] = 0;
			if (used + (count ? 2 : 0) + (int)strlen(entry) < size)
			{
				used += snprintf(line + used, (size_t)(size - used), "%s%s", count ? ", " : "", entry);
				count++;
			}
		}
		playlist += length;
		while (*playlist == '\r' || *playlist == '\n')
			playlist++;
	}
	return count;
}

/* ---------- running.txt */

int ae_host_dedicated_running_text(struct ae_hd_running const *running, char *text, int size)
{
	char name[AE_HD_NAME];
	int length;

	copy_text(name, (int)sizeof(name), running->name);
	length = snprintf(text, (size_t)size, "pid=%ld\ntunnel=%d\nweb=%d\nstarted=%lu\nstamp=%llu\nname=%s\n", running->pid,
		running->tunnel_port, running->web_port, running->started, running->stamp, name);
	return length > 0 && length < size ? length : -1;
}

int ae_host_dedicated_running_read(char const *text, struct ae_hd_running *running)
{
	int have = 0;

	memset(running, 0, sizeof(*running));
	while (text && *text)
	{
		int length = (int)strcspn(text, "\n");
		char line[128];

		if (length < (int)sizeof(line))
		{
			memcpy(line, text, (size_t)length);
			line[length] = 0;
			if (!strncmp(line, "pid=", 4))
			{
				running->pid = strtol(line + 4, NULL, 10);
				have |= 1;
			}
			else if (!strncmp(line, "tunnel=", 7))
			{
				running->tunnel_port = atoi(line + 7);
				have |= 2;
			}
			else if (!strncmp(line, "web=", 4))
			{
				running->web_port = atoi(line + 4);
				have |= 4;
			}
			else if (!strncmp(line, "started=", 8))
				running->started = strtoul(line + 8, NULL, 10);
			else if (!strncmp(line, "stamp=", 6))
				running->stamp = read_decimal(line + 6);
			else if (!strncmp(line, "name=", 5))
				copy_text(running->name, (int)sizeof(running->name), line + 5);
		}
		text += length;
		if (*text == '\n')
			text++;
	}
	return have == 7 && running->pid > 1 && running->tunnel_port > 0 && running->tunnel_port < 65536 &&
		running->web_port > 0 && running->web_port < 65536;
}

int ae_host_dedicated_running_identity(struct ae_hd_running const *running, unsigned long long current_stamp)
{
	return running && running->pid > 1 && running->stamp && current_stamp == running->stamp;
}

int ae_host_dedicated_running_reused(struct ae_hd_running const *running, int process_alive, unsigned long long current_stamp)
{
	return process_alive && ae_host_dedicated_running_identity(running, current_stamp);
}

/* ---------- the status file */

/* the first "key": value of the JSON (a string into text, else the number's text), 1 if found */
static int json_value(char const *json, char const *key, char *text, int size)
{
	char pattern[48];
	char const *at;
	int length = 0;

	snprintf(pattern, sizeof(pattern), "\"%s\":", key);
	at = strstr(json, pattern);
	if (!at || size <= 0)
		return 0;
	at += strlen(pattern);
	while (*at == ' ')
		at++;
	if (*at == '"')
	{
		for (at++; *at && *at != '"' && length < size - 1; at++)
		{
			if (*at == '\\' && at[1])
				at++;
			text[length++] = *at;
		}
	}
	else
	{
		for (; *at && *at != ',' && *at != '}' && *at != ']' && length < size - 1; at++)
			text[length++] = *at;
	}
	text[length] = 0;
	return 1;
}

int ae_host_dedicated_status_read(char const *json, struct ae_hd_status *status)
{
	char number[32];

	memset(status, 0, sizeof(*status));
	if (!json || !json_value(json, "state", status->state, (int)sizeof(status->state)))
		return 0;
	json_value(json, "name", status->name, (int)sizeof(status->name));
	json_value(json, "map", status->map, (int)sizeof(status->map));
	json_value(json, "gametype", status->gametype, (int)sizeof(status->gametype));
	json_value(json, "invite", status->invite, (int)sizeof(status->invite));
	if (json_value(json, "players", number, (int)sizeof(number)))
		status->players = atoi(number);
	if (json_value(json, "max_players", number, (int)sizeof(number)))
		status->max_players = atoi(number);
	if (json_value(json, "uptime", number, (int)sizeof(number)))
		status->uptime = strtol(number, NULL, 10);
	return 1;
}

int ae_host_dedicated_status_ready(struct ae_hd_status const *status)
{
	return status && status->state[0] && strcmp(status->state, "starting") != 0 &&
		strcmp(status->state, "stopping") != 0;
}

/* ---------- quitting */

int ae_host_dedicated_quit_choices(int people_on, int *choices)
{
	int count = 0;

	choices[count++] = AE_HD_QUIT_KEEP;
	if (people_on)
		choices[count++] = AE_HD_QUIT_STOP_EMPTY;
	choices[count++] = AE_HD_QUIT_STOP;
	return count;
}

char const *ae_host_dedicated_quit_label(int choice)
{
	switch (choice)
	{
	case AE_HD_QUIT_KEEP: return "KEEP RUNNING";
	case AE_HD_QUIT_STOP_EMPTY: return "STOP WHEN EMPTY";
	case AE_HD_QUIT_STOP: return "STOP NOW";
	}
	return "";
}

/* ---------- the data folder */

int ae_host_dedicated_link_wanted(char const *name, int is_folder)
{
	static char const *const kept_out[] = { "screenshots", "test-runs", "videos", "captures", "logs", "server", "saves" };
	int index;

	if (!name || !name[0] || name[0] == '.' || !is_folder)
		return 0;
	for (index = 0; index < (int)(sizeof(kept_out) / sizeof(kept_out[0])); index++)
		if (!strcmp(name, kept_out[index]))
			return 0;
	return 1;
}
