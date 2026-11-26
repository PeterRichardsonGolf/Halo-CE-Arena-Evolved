/*
AE_SPLIT_GRAPHICS.C

SPLIT GRAPHICS for the renderer (ae_split_graphics.h): how many local players are in the game, display.split_graphics
read again when Settings changes it, and the preset's values in place of the stored ones.
*/

#include <string.h>

#include "cseries/cseries.h"
#include "game/players.h"

#include "ae_split_graphics.h"
#include "ae_split_graphics_rules.h"

const char *config_string(const char *name);
int config_boolean(const char *name);
unsigned long config_changes(void);
void platform_log(const char *format, ...);
/* (source/game/players.c: local_player_count, 0 before the players' globals are made) */
short players_port_local_player_count(void);

#define PRESET_IN_PLAY_BIT 0x80000000ul

static struct
{
	boolean read;
	unsigned long read_at;
	int value;
	/* (the preset last in play, for the log: AE_GRAPHICS_PRESET_NONE for none; logged once a change) */
	boolean logged;
	int logged_preset;
} split_graphics;

/* the preset in play now: display.split_graphics' while two or more local players are in the game */
static int preset_in_play(void)
{
	/* (0 before the players' globals are made: the renderer asks from its first frame) */
	short players = players_port_local_player_count();
	int preset;

	if (!split_graphics.read || split_graphics.read_at != config_changes())
	{
		split_graphics.read = TRUE;
		split_graphics.read_at = config_changes();
		split_graphics.value = ae_split_graphics_value(config_string("display.split_graphics"));
	}
	preset = players >= 2 ? ae_split_graphics_preset(split_graphics.value) : AE_GRAPHICS_PRESET_NONE;
	if (!split_graphics.logged || split_graphics.logged_preset != preset)
	{
		/* (the log line is the telemetry: what a test or a report reads to know which graphics played) */
		if (split_graphics.logged || preset != AE_GRAPHICS_PRESET_NONE)
		{
			platform_log("split graphics: %s (%d local players, display.split_graphics %s)",
				preset == AE_GRAPHICS_PRESET_QUALITY ? "QUALITY" :
				preset == AE_GRAPHICS_PRESET_PERFORMANCE ? "PERFORMANCE" : "one player's settings",
				(int)players, config_string("display.split_graphics"));
		}
		split_graphics.logged = TRUE;
		split_graphics.logged_preset = preset;
	}
	return preset;
}

unsigned long ae_split_graphics_changes(void)
{
	unsigned long changes = config_changes();

	return preset_in_play() != AE_GRAPHICS_PRESET_NONE ? changes ^ PRESET_IN_PLAY_BIT : changes;
}

const char *ae_split_graphics_string(const char *name)
{
	const char *value = ae_split_graphics_preset_value(preset_in_play(), name);

	return value ? value : config_string(name);
}

int ae_split_graphics_boolean(const char *name)
{
	const char *value = ae_split_graphics_preset_value(preset_in_play(), name);

	if (value)
		return !strcmp(value, "true");
	return config_boolean(name);
}

int ae_split_graphics_sharp_reflections(void)
{
	static unsigned long read_at = (unsigned long)-1;
	static int sharp;

	if (read_at != ae_split_graphics_changes())
	{
		read_at = ae_split_graphics_changes();
		sharp = ae_split_graphics_boolean("display.sharp_reflections") != 0;
	}
	return sharp;
}
