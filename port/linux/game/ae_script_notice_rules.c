#include <stdio.h>
#include <string.h>
#include "ae_script_notice_rules.h"

int ae_script_notice_lines(long refused, char first[AE_SCRIPT_NOTICE_LINE_SIZE], char second[AE_SCRIPT_NOTICE_LINE_SIZE])
{
	first[0] = second[0] = 0;
	if (refused <= 0)
		return 0;
	snprintf(first, AE_SCRIPT_NOTICE_LINE_SIZE, "%ld of this map's scripts %s switched off;", refused > 99999 ? 99999L : refused,
		refused == 1 ? "was" : "were");
	snprintf(second, AE_SCRIPT_NOTICE_LINE_SIZE, "the mission may not play correctly");
	return 2;
}

int ae_script_notice_due(long refused, int shown, int campaign_level_running, int hud_hidden, long game_ticks)
{
	return refused > 0 && !shown && campaign_level_running && !hud_hidden && game_ticks >= AE_SCRIPT_NOTICE_TICK;
}

static int lower(int c)
{
	return c >= 'A' && c <= 'Z' ? c + ('a' - 'A') : c;
}

int ae_script_notice_forced(const char *setting, const char *script_name)
{
	if (!setting || !setting[0] || !script_name)
		return 0;
	for (; *setting && *script_name; setting++, script_name++)
		if (lower((unsigned char)*setting) != lower((unsigned char)*script_name))
			return 0;
	return !*setting && !*script_name;
}
