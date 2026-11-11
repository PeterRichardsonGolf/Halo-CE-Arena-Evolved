/*
AE_DEV_CMD_STATE.C

DEV MODE's command state that outlives a command (ae_dev_cmd_state.h).
*/

#include <stdio.h>
#include <string.h>

#include "ae_dev_cmd_state.h"

void ae_dev_game_new(struct ae_dev_game_state *state)
{
	state->generation++;
	state->reset_pending = 0;
	state->options_saved = 0;
}

void ae_dev_game_reset_begin(struct ae_dev_game_state *state)
{
	state->reset_pending = 1;
	state->reset_generation = state->generation;
}

int ae_dev_game_reset_current(struct ae_dev_game_state *state)
{
	if (state->reset_pending && state->reset_generation != state->generation)
		state->reset_pending = 0;
	return state->reset_pending;
}

void ae_dev_game_reset_end(struct ae_dev_game_state *state)
{
	state->reset_pending = 0;
}

int ae_dev_game_options_keep(struct ae_dev_game_state *state)
{
	if (state->options_saved && state->options_generation == state->generation)
		return 0;
	state->options_saved = 1;
	state->options_generation = state->generation;
	return 1;
}

int ae_dev_game_options_take(struct ae_dev_game_state *state)
{
	int take = state->options_saved && state->options_generation == state->generation;

	state->options_saved = 0;
	return take;
}

void ae_dev_game_options_drop(struct ae_dev_game_state *state)
{
	state->options_saved = 0;
}

int ae_dev_settings_wants(struct ae_dev_settings const *settings, char const *key)
{
	short index;

	if (!key || !key[0] || strlen(key) >= AE_DEV_SETTING_KEY || !strcmp(key, "system.dev_mode") ||
		settings->count >= AE_DEV_SETTINGS)
	{
		return 0;
	}
	for (index = 0; index < settings->count; index++)
	{
		if (!strcmp(settings->item[index].key, key))
			return 0;
	}
	return 1;
}

int ae_dev_settings_keep(struct ae_dev_settings *settings, char const *key, char const *value)
{
	if (!value || !ae_dev_settings_wants(settings, key))
		return 0;
	snprintf(settings->item[settings->count].key, AE_DEV_SETTING_KEY, "%s", key);
	snprintf(settings->item[settings->count].value, AE_DEV_SETTING_VALUE, "%s", value);
	settings->count++;
	return 1;
}

int ae_dev_settings_back(struct ae_dev_settings *settings, int (*set)(char const *key, char const *value, void *context),
	void *context)
{
	int index, count = settings->count;

	for (index = 0; index < count; index++)
	{
		if (set)
			set(settings->item[index].key, settings->item[index].value, context);
	}
	settings->count = 0;
	return count;
}

int ae_dev_dummy_settings_keep(struct ae_dev_dummy_settings *settings, char const *count, char const *teams)
{
	if (settings->kept)
		return 0;
	settings->kept = 1;
	snprintf(settings->count, sizeof settings->count, "%s", count);
	snprintf(settings->teams, sizeof settings->teams, "%s", teams);
	settings->set_count = -1;
	return 1;
}

void ae_dev_dummy_settings_set(struct ae_dev_dummy_settings *settings, long count)
{
	settings->set_count = count;
}

int ae_dev_dummy_settings_take(struct ae_dev_dummy_settings *settings, long current_count, char *count_out, int count_size,
	char *teams_out, int teams_size)
{
	if (!settings->kept)
		return 0;
	settings->kept = 0;
	snprintf(count_out, (size_t)count_size, "%s", current_count == settings->set_count ? settings->count : "");
	snprintf(teams_out, (size_t)teams_size, "%s", settings->teams);
	return 1;
}
