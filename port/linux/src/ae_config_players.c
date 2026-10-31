/*
AE_CONFIG_PLAYERS.C

(Arena Evolved) Per-player settings in the game's memory (ae_config_players.h).
*/

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "port_config.h"
#include "ae_config_players.h"

#define PLAYER_TEXT 64

static const char *const names[] =
{
	/* CONTROLS (the profile's own ten are in the profile) */
	"input.left_stick_deadzone", "input.right_stick_deadzone", "input.zoom_mode", "input.zoom_reset",
	"input.zoom_hold_time", "input.mouse_zoom_mode", "input.wheel_zoom", "input.mouse_zoom_reset", "input.mouse_zoom_hold_time",
	"input.back_left", "input.back_right", "input.adaptive_triggers", "input.trigger_strength", "input.lightbar",
	"input.mouse_style", "input.mouse_sensitivity", "input.mouse_vertical_sensitivity", "input.mouse_mcc_sensitivity",
	"input.mouse_zoom_scale", "input.mouse_vehicle_scale", "input.invert_mouse", "input.mouse_aim_assist",
	"input.keyboard_prompts",
	/* VIEW */
	"display.fov", "display.viewmodel_fov", "display.viewmodel_visible", "display.first_person_legs",
	"display.viewmodel_shield", "display.zoom_blur", "display.passenger_third_person", "display.backpack_weapons", "display.shield_color",
	"display.shield_glow", "display.shield_glow_intensity", "display.overshield_effect", "display.pistol_color", "display.pistol_trim",
	/* HUD */
	"display.hud_style", "display.hud_area", "display.compact_hud", "display.match_clock",
	"display.match_clock_labels", "display.campaign_timer", "display.powerup_timers", "display.short_messages",
	"display.latency_meter", "display.hud_button_style", "display.ally_reticle", "display.player_names", "display.player_name_scale",
	"display.enemy_name_color", "display.dead_ally_markers", "display.scoreboard_team_layout", "display.scoreboard_background",
	"display.scoreboard_fade", "display.show_quit_players", "display.scoreboard_background_color",
	"display.waypoint_style", "display.waypoint_size", "display.power_list", "display.shotgun_power",
	"display.spawn_heat"
	/* (CALLOUTS, CALLOUT DETAIL, CALLOUT VOICE: one voice on this PC's speakers for every player, so this PC's) */
};

#define SETTINGS ((int)(sizeof(names) / sizeof(names[0])))
/* each one's type, in the names' order: b boolean, i integer, r real, s string */
static const char types[] =
	"iissrsbsrssbsssrrrrrbbb"
	"rrbbbbbbsbsbss"
	"ssbsbbbbbsbsrsbsbsbssrsbs";
typedef char ae_config_players_types_match[sizeof(types) - 1 == sizeof(names) / sizeof(names[0]) ? 1 : -1];

struct player_value
{
	int boolean;
	long integer;
	double real;
	char text[PLAYER_TEXT];
	int set;
};

static struct player_value values[AE_CONFIG_PLAYERS][SETTINGS];
/* (the environment's: read once a setting) */
static signed char environment_given[SETTINGS];   /* 0 not read, 1 given, -1 not */
/* any player has a value of their own (else every lookup is skipped at once) */
static int any_set;
static unsigned long change_count;
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
/* the player the game works for on this thread, plus one (0: no one, player 1's); -1: config.toml's own (reading for
a seed) */
static __thread int now;
static void (*writer)(int player, const char *name, const char *value);

static int slot_of(const char *name)
{
	int index;

	if (!name)
		return -1;
	for (index = 0; index < SETTINGS; index++)
	{
		if (!strcmp(names[index], name))
			return index;
	}
	return -1;
}

static int text_is_false(const char *text)
{
	return !strcmp(text, "false") || !strcmp(text, "0") || !strcmp(text, "no") || !strcmp(text, "off") || !*text;
}

static int is_number(const char *text)
{
	char *end;

	strtod(text, &end);
	return end != text && !*end;
}

int ae_config_player_value(const char *name, const char *environment, int type, int *boolean, long *integer,
	double *real, char **string)
{
	struct player_value const *value;
	int slot;

	if (!any_set && now >= 0)
		return 0;
	slot = slot_of(name);
	if (slot < 0)
		return 0;
	if (!environment_given[slot])
		environment_given[slot] = environment && getenv(environment) ? 1 : -1;
	if (environment_given[slot] > 0 || now < 0)
		return 0;
	value = &values[now > 0 ? now - 1 : 0][slot];
	if (!value->set)
		return 0;
	switch (type)
	{
	case 0:
		*boolean = value->boolean;
		break;
	case 1:
		*integer = value->integer;
		break;
	case 2:
		*real = value->real;
		break;
	default:
		/* (config_string's callers may hold it: the player's own buffer, never freed) */
		*string = (char *)value->text;
		break;
	}
	return 1;
}

void ae_config_player_written(const char *name, const char *value)
{
	if (writer && slot_of(name) >= 0)
		writer(ae_config_player_now(), name, value);
}

int ae_config_player_setting(const char *name)
{
	return slot_of(name) >= 0;
}

int ae_config_player_is_string(const char *name)
{
	int slot = slot_of(name);

	return slot >= 0 && types[slot] == 's';
}

int ae_config_player_setting_count(void)
{
	return SETTINGS;
}

const char *ae_config_player_setting_name(int index)
{
	return index >= 0 && index < SETTINGS ? names[index] : NULL;
}

int ae_config_player_set(int player, const char *name, const char *text)
{
	int slot = slot_of(name);
	struct player_value *value;

	if (player < 0 || player >= AE_CONFIG_PLAYERS || slot < 0 || (text && strlen(text) >= PLAYER_TEXT))
		return 0;
	pthread_mutex_lock(&lock);
	value = &values[player][slot];
	if (!text)
		value->set = 0;
	else
	{
		snprintf(value->text, sizeof(value->text), "%s", text);
		value->boolean = !text_is_false(text);
		value->integer = strtol(text, NULL, 10);
		value->real = strtod(text, NULL);
		value->set = 1;
		any_set = 1;
	}
	change_count++;
	pthread_mutex_unlock(&lock);
	return 1;
}

void ae_config_player_clear(int player)
{
	int slot;

	if (player < 0 || player >= AE_CONFIG_PLAYERS)
		return;
	pthread_mutex_lock(&lock);
	for (slot = 0; slot < SETTINGS; slot++)
		values[player][slot].set = 0;
	change_count++;
	pthread_mutex_unlock(&lock);
}

int ae_config_player_has(int player, const char *name)
{
	int slot = slot_of(name);

	return player >= 0 && player < AE_CONFIG_PLAYERS && slot >= 0 && values[player][slot].set;
}

int ae_config_player_text(int player, const char *name, char *text, size_t size)
{
	int previous = ae_config_player_enter(player);
	int found = config_text(name, text, size);

	ae_config_player_leave(previous);
	return found;
}

int ae_config_player_boolean(int player, const char *name)
{
	int previous = ae_config_player_enter(player);
	int value = config_boolean(name);

	ae_config_player_leave(previous);
	return value;
}

double ae_config_player_real(int player, const char *name)
{
	int previous = ae_config_player_enter(player);
	double value = config_real(name);

	ae_config_player_leave(previous);
	return value;
}

int ae_config_player_seed_text(const char *name, char *text, size_t size)
{
	char standard[PLAYER_TEXT];
	int previous = now, found;

	if (slot_of(name) < 0 || !config_default(name, standard, sizeof(standard)))
		return 0;
	now = -1;
	found = config_text(name, text, size);
	now = previous;
	if (!found)
		return 0;
	/* (the same number written another way is the default too: 1.0 and 1) */
	if (!strcmp(text, standard) || (is_number(text) && is_number(standard) &&
		strtod(text, NULL) == strtod(standard, NULL)))
		return 0;
	/* (the environment's is the test's, not the player's) */
	{
		int slot = slot_of(name);

		if (environment_given[slot] > 0)
			return 0;
	}
	return 1;
}

int ae_config_player_enter(int player)
{
	int previous = now > 0 ? now - 1 : -1;

	now = player >= 0 && player < AE_CONFIG_PLAYERS ? player + 1 : 0;
	return previous;
}

void ae_config_player_leave(int previous)
{
	now = previous >= 0 && previous < AE_CONFIG_PLAYERS ? previous + 1 : 0;
}

int ae_config_player_now(void)
{
	return now > 0 ? now - 1 : 0;
}

unsigned long ae_config_player_value_changes(void)
{
	return change_count;
}

unsigned long ae_config_player_changes(void)
{
	/* (config_changes counts the players' values too) */
	return config_changes() * AE_CONFIG_PLAYERS + (unsigned long)ae_config_player_now();
}

void ae_config_player_set_writer(void (*set)(int player, const char *name, const char *value))
{
	writer = set;
}
