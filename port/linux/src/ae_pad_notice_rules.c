/*
AE_PAD_NOTICE_RULES.C

A controller's battery and connection in words (ae_pad_notice_rules.h). No SDL here.
*/

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "ae_pad_notice_rules.h"
#include "ae_steam_pad_rules.h"

int ae_pad_battery_rules_pad(struct ae_pad_battery_state *state, unsigned id)
{
	if (state->id == id)
		return 0;
	memset(state, 0, sizeof(*state));
	state->id = id;
	return 1;
}

int ae_pad_battery_rules_step(struct ae_pad_battery_state *state, int power, int percent)
{
	int notice = AE_PAD_NOTICE_NONE;

	if (power == AE_PAD_POWER_BATTERY && percent >= 0)
	{
		if (percent >= AE_PAD_BATTERY_REARM_PERCENT)
			state->disarmed = 0;
		else if (percent <= AE_PAD_BATTERY_LOW_PERCENT && !state->disarmed)
		{
			state->disarmed = 1;
			state->low_given = 1;
			notice = AE_PAD_NOTICE_LOW;
		}
	}
	else if (power == AE_PAD_POWER_CHARGING)
	{
		/* (a pad put on its charger: armed again; the charging notice only after a low one, once a plug-in) */
		state->disarmed = 0;
		if (state->low_given && !state->was_charging)
			notice = AE_PAD_NOTICE_CHARGING;
	}
	state->was_charging = power == AE_PAD_POWER_CHARGING;
	state->started = 1;
	return notice;
}

int ae_pad_notice_text(int notice, int player, int percent, char text[AE_PAD_NOTICE_TEXT])
{
	char prefix[24] = "", level[24] = "";

	text[0] = 0;
	if (notice != AE_PAD_NOTICE_LOW && notice != AE_PAD_NOTICE_CHARGING)
		return 0;
	if (player >= 2)
		snprintf(prefix, sizeof(prefix), "PLAYER %d: ", player);
	if (percent >= 0)
		snprintf(level, sizeof(level), " %d %%", percent > 100 ? 100 : percent);
	snprintf(text, AE_PAD_NOTICE_TEXT, "%sCONTROLLER %s%s", prefix, notice == AE_PAD_NOTICE_LOW ? "BATTERY LOW" : "CHARGING",
		level);
	return 1;
}

double ae_pad_notice_alpha(unsigned long elapsed_ms)
{
	double seconds = (double)elapsed_ms / 1000.0;

	if (seconds <= AE_PAD_NOTICE_SECONDS)
		return 1.0;
	if (seconds >= AE_PAD_NOTICE_SECONDS + AE_PAD_NOTICE_FADE_SECONDS)
		return 0.0;
	return 1.0 - (seconds - AE_PAD_NOTICE_SECONDS) / AE_PAD_NOTICE_FADE_SECONDS;
}

double ae_pad_notice_pulse(unsigned long elapsed_ms, int reduce_motion)
{
	double t = (double)elapsed_ms / 1000.0;

	if (reduce_motion || t >= 1.0)
		return 0.0;
	return 0.5 * (1.0 - cos(4.0 * 3.14159265358979323846 * t));
}

static void add_part(struct ae_pad_details *details, const char *text)
{
	if (details->count < AE_PAD_DETAIL_PARTS)
		snprintf(details->part[details->count++], AE_PAD_DETAIL_TEXT, "%s", text);
}

/* the battery's part, if the pad has one to tell */
static void add_battery(struct ae_pad_details *details, int power, int percent)
{
	char text[AE_PAD_DETAIL_TEXT];

	if (percent > 100)
		percent = 100;
	if (power == AE_PAD_POWER_BATTERY && percent >= 0)
	{
		details->low = percent <= AE_PAD_BATTERY_LOW_PERCENT ? details->count : -1;
		snprintf(text, sizeof(text), "%s %d %%", percent <= AE_PAD_BATTERY_LOW_PERCENT ? "BATTERY LOW" : "BATTERY", percent);
		add_part(details, text);
	}
	else if (power == AE_PAD_POWER_CHARGING)
	{
		if (percent >= 0)
			snprintf(text, sizeof(text), "CHARGING %d %%", percent);
		else
			snprintf(text, sizeof(text), "CHARGING");
		add_part(details, text);
	}
	else if (power == AE_PAD_POWER_CHARGED)
		add_part(details, "CHARGED");
}

void ae_pad_details_build(struct ae_pad_details *details, int world, int keyboard, const char *name, const char *mode,
	int power, int percent)
{
	memset(details, 0, sizeof(*details));
	details->low = -1;
	switch (world)
	{
	case AE_STEAM_WORLD_DIRECT:
		add_part(details, "STEAM CONTROLLER");
		if (mode && mode[0])
			add_part(details, mode);
		add_battery(details, power, percent);
		return;
	case AE_STEAM_WORLD_STEAM_INPUT:
		add_part(details, "STEAM CONTROLLER");
		add_part(details, "THROUGH STEAM INPUT");
		return;
	case AE_STEAM_WORLD_STEAM_INPUT_OTHER:
		add_part(details, "THROUGH STEAM INPUT");
		return;
	case AE_STEAM_WORLD_NO_ACCESS:
		add_part(details, "STEAM CONTROLLER");
		add_part(details, "AS MOUSE AND KEYS");
		return;
	default:
		break;
	}
	if (keyboard || !name || !name[0])
	{
		add_part(details, "KEYBOARD AND MOUSE");
		return;
	}
	add_part(details, name);
	if (mode && mode[0])
		add_part(details, mode);
	add_battery(details, power, percent);
}

int ae_pad_backpack_note_shown(const int set[4])
{
	return set[0] && set[1] && set[2] && set[3];
}
