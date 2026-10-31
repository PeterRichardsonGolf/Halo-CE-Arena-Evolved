/*
AE_OVERLAY_PING.C

The ping on the performance line (ae_overlay_ping.h).
*/

#include "cseries.h"
#include "game/game.h"
#include "game/game_engine.h"
#include "interface/ui_widget.h"
#include "../src/port_config.h"
#include "network_distributed.h"

#include "ae_overlay_ping.h"

#include <stdio.h>
#include <string.h>

void ae_overlay_ping_format(char *text, long size, long ping, int host, int problem)
{
	if (!text || size <= 0)
		return;
	if (problem && !host)
		snprintf(text, (size_t)size, "PING --");
	else if (ping == NONE)
		text[0] = 0;
	else
		snprintf(text, (size_t)size, "%s %ld MS", host ? "MAX PING" : "PING", ping > 999 ? 999L : ping);
}

int ae_overlay_ping_wanted(void)
{
	char const *level = config_string("display.performance");

	return config_boolean("display.performance_ping") && level && level[0] && strcmp(level, "off") != 0;
}

void ae_overlay_ping_append(char *line, long size)
{
	short connection = game_connection();
	int host = connection == _game_connection_network_server;
	char text[32];
	size_t length;

	/* (only on a line already shown: the setting is one of display.performance's) */
	if (!line || size <= 0 || !line[0] || !config_boolean("display.performance_ping"))
		return;
	if ((!host && connection != _game_connection_network_client) || !game_in_progress() || !game_engine_in_play() ||
		main_menu_is_active())
	{
		return;
	}
	ae_overlay_ping_format(text, (long)sizeof(text), host ? distributed_slowest_client_ping() : distributed_own_ping(),
		host, !host && distributed_connection_problem());
	length = strlen(line);
	/* (before a second line, if one is there) */
	if (text[0] && length + 2 + strlen(text) < (size_t)size)
	{
		char *newline = strchr(line, '\n');

		if (newline)
		{
			char rest[128];

			snprintf(rest, sizeof(rest), "%s", newline);
			snprintf(newline, (size_t)size - (size_t)(newline - line), "  %s%s", text, rest);
		}
		else
			snprintf(line + length, (size_t)size - length, "  %s", text);
	}
}
