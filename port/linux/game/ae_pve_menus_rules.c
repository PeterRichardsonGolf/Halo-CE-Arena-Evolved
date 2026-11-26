/*
AE_PVE_MENUS_RULES.C

See ae_pve_menus_rules.h.
*/

#include <stdio.h>
#include <string.h>

#include "ae_pve_menus_rules.h"

/* the separator dot's UTF-8 */
#define DOT "\xE2\x80\xA2"

static const char *const allowed_keys[] =
{
	"game.skulls", "game.skulls_active", "game.skulls_unlock_mode", "game.skulls_unlocked", "game.skulls_all_unlocked",
	"game.skulls_instant_unlock", "game.skulls_lose_on_death", "game.skulls_drop_count", "game.skulls_drop_chance",
	"game.skulls_random_min", "game.skulls_random_max",
	"game.campaign_score", "game.campaign_medals", "game.campaign_score_revert_cost", "network.coop_score_type",
	"network.coop_friendly_fire", "network.coop_ally_friendly_fire", "network.coop_player_collisions",
	"network.coop_team_wipe", "network.coop_enemies_mode", "network.coop_pickups", "network.coop_vehicles",
	"network.coop_enemies", "network.coop_enemies_multiplier",
};

int ae_pve_menus_key_allowed(const char *key)
{
	size_t each;

	for (each = 0; key && each < sizeof(allowed_keys) / sizeof(allowed_keys[0]); each++)
		if (!strcmp(allowed_keys[each], key))
			return 1;
	return 0;
}

/* ---------- skulls */

int ae_pve_menus_found_from_keys(int all_unlocked, int instant_unlock)
{
	return all_unlocked ? AE_PVEM_FOUND_ALL : instant_unlock ? AE_PVEM_FOUND_PICKUP : AE_PVEM_FOUND_LEVELS;
}

void ae_pve_menus_found_to_keys(int found, const char **mode, const char **all_unlocked, const char **instant_unlock)
{
	*mode = "play";
	*all_unlocked = found == AE_PVEM_FOUND_ALL ? "true" : "false";
	*instant_unlock = found == AE_PVEM_FOUND_PICKUP ? "true" : "false";
}

const char *ae_pve_menus_found_name(int found)
{
	return found == AE_PVEM_FOUND_LEVELS ? "FOUND IN LEVELS" : found == AE_PVEM_FOUND_PICKUP ? "FOUND ON PICKUP" : "ALL UNLOCKED";
}

const char *ae_pve_menus_found_help(int found)
{
	switch (found)
	{
	case AE_PVEM_FOUND_LEVELS:
		return "Enemies carry the skulls you have not found. Pick one up: it is on for that level, and yours once the level is finished.";
	case AE_PVEM_FOUND_PICKUP:
		return "Enemies carry the skulls you have not found. One you pick up is yours at once.";
	default:
		return "Every skull can be turned on now. None drop in levels.";
	}
}

const char *ae_pve_menus_lose_help(int lose)
{
	return lose ? "Dying, or going back to a checkpoint, loses the skulls you picked up and have not kept yet." :
		"A skull you picked up stays until you finish the level, however often you die.";
}

void ae_pve_menus_count_text(long count, char *text, size_t size)
{
	if (count < 1)
		snprintf(text, size, "BY DIFFICULTY");
	else
		snprintf(text, size, "%ld", count > 8 ? 8L : count);
}

static const long chance_percent[AE_PVEM_CHANCES] = { 25, 50, 100, 200, 400 };
static const char *const chance_name[AE_PVEM_CHANCES] = { "1/4X", "1/2X", "1X", "2X", "4X" };

long ae_pve_menus_chance_percent(int index)
{
	return chance_percent[index < 0 ? 0 : index >= AE_PVEM_CHANCES ? AE_PVEM_CHANCES - 1 : index];
}

const char *ae_pve_menus_chance_name(int index)
{
	return chance_name[index < 0 ? 0 : index >= AE_PVEM_CHANCES ? AE_PVEM_CHANCES - 1 : index];
}

int ae_pve_menus_chance_index(long percent)
{
	int each, best = 2;
	long gap = 1000000;

	for (each = 0; each < AE_PVEM_CHANCES; each++)
	{
		long d = percent > chance_percent[each] ? percent - chance_percent[each] : chance_percent[each] - percent;

		if (d < gap)
			gap = d, best = each;
	}
	return best;
}

long ae_pve_menus_random_limit(long value)
{
	return value < 0 ? 0 : value > 40 ? 40 : value;
}

void ae_pve_menus_random_clamp(long *fewest, long *most, int moved)
{
	*fewest = ae_pve_menus_random_limit(*fewest);
	*most = ae_pve_menus_random_limit(*most);
	if (*most < *fewest)
	{
		if (moved == 0)
			*most = *fewest;
		else
			*fewest = *most;
	}
}

long ae_pve_menus_random_pick(long fewest, long most, long had, unsigned long r)
{
	long wanted;

	ae_pve_menus_random_clamp(&fewest, &most, 1);
	if (had < 0)
		had = 0;
	if (fewest > had)
		fewest = had;
	if (most > had)
		most = had;
	wanted = fewest + (long)(r % (unsigned long)(most - fewest + 1));
	return wanted;
}

void ae_pve_menus_skulls_value(int on_count, char *text, size_t size)
{
	if (on_count <= 0)
		snprintf(text, size, "OFF");
	else
		snprintf(text, size, "%d ON", on_count);
}

void ae_pve_menus_rules_fact(int skulls_on, int scoring_on, char *text, size_t size)
{
	if (skulls_on > 0 && scoring_on)
		snprintf(text, size, "%d SKULL%s " DOT " SCORING ON", skulls_on, skulls_on == 1 ? "" : "S");
	else if (skulls_on > 0)
		snprintf(text, size, "%d SKULL%s", skulls_on, skulls_on == 1 ? "" : "S");
	else if (scoring_on)
		snprintf(text, size, "SCORING ON");
	else
		snprintf(text, size, "NONE");
}

void ae_pve_menus_skull_help(const char *name, const char *description, int on, int found, char *text, size_t size)
{
	if (!found)
		snprintf(text, size, "%s: %s Not found yet: enemies drop the skulls you have not found, 1 a level on EASY up to 4 on LEGENDARY.",
			name, description);
	else
		snprintf(text, size, "%s: %s %s", name, description, on ? "{A} turns it off." : "{A} turns it on.");
}

/* ---------- co-op options */

static const struct ae_pve_menus_option coop_options[AE_PVEM_COOP_OPTIONS] =
{
	{ "network.coop_friendly_fire", "on" },
	{ "network.coop_ally_friendly_fire", "true" },
	{ "network.coop_player_collisions", "true" },
	{ "network.coop_team_wipe", "auto" },
	{ "network.coop_enemies_mode", "per_player" },
	{ "network.coop_pickups", "none" },
	{ "network.coop_vehicles", "none" },
	{ "network.coop_enemies", "50" },
	{ "network.coop_enemies_multiplier", "2" },
};

struct ae_pve_menus_option const *ae_pve_menus_coop_option(int index)
{
	return index >= 0 && index < AE_PVEM_COOP_OPTIONS ? &coop_options[index] : NULL;
}

int ae_pve_menus_coop_is_standard(const char *(*get)(void *context, const char *key), void *context)
{
	int each;

	for (each = 0; each < AE_PVEM_COOP_OPTIONS; each++)
	{
		const char *value = get(context, coop_options[each].key);

		if (!value || strcmp(value, coop_options[each].standard))
			return 0;
	}
	return 1;
}

static const char *const friendly_words[] = { "off", "on", "shields_only", "explosives_only" };
static const char *const friendly_labels[] = { "OFF", "ON", "SHIELD ONLY", "EXPLOSIVES ONLY" };
static const char *const wipe_words[] = { "auto", "revert", "respawn" };
static const char *const wipe_labels[] = { "AUTO", "REVERT CHECKPOINT", "RESPAWN ONLY" };
static const char *const mode_words[] = { "per_player", "multiplier", "none" };
static const char *const mode_labels[] = { "PER PLAYER", "FIXED", "NONE" };
static const char *const growth_words[] = { "25", "50", "100", "150", "200" };
static const char *const growth_labels[] = { "25%", "50%", "100%", "150%", "200%" };
static const char *const fixed_words[] = { "2", "4", "8", "16", "32" };
static const char *const fixed_labels[] = { "2X", "4X", "8X", "16X", "32X" };

static const struct { const char *const *words, *const *labels; int count; } coop_kinds[AE_PVEM_COOP_KINDS] =
{
	{ friendly_words, friendly_labels, 4 }, { wipe_words, wipe_labels, 3 }, { mode_words, mode_labels, 3 },
	{ growth_words, growth_labels, 5 }, { fixed_words, fixed_labels, 5 },
};

int ae_pve_menus_coop_value_count(int kind)
{
	return kind >= 0 && kind < AE_PVEM_COOP_KINDS ? coop_kinds[kind].count : 0;
}

const char *ae_pve_menus_coop_value_word(int kind, int index)
{
	return ae_pve_menus_coop_value_count(kind) > index && index >= 0 ? coop_kinds[kind].words[index] : "";
}

const char *ae_pve_menus_coop_value_label(int kind, int index)
{
	return ae_pve_menus_coop_value_count(kind) > index && index >= 0 ? coop_kinds[kind].labels[index] : "";
}

int ae_pve_menus_coop_value_index(int kind, const char *word)
{
	int each;

	for (each = 0; word && each < ae_pve_menus_coop_value_count(kind); each++)
		if (!strcmp(coop_kinds[kind].words[each], word))
			return each;
	return -1;
}

long ae_pve_menus_coop_factor(const char *mode, long growth_percent, long fixed_times, int players)
{
	if (players < 1)
		players = 1;
	if (mode && !strcmp(mode, "per_player"))
		return 100 + (long)(players - 1) * growth_percent;
	if (mode && !strcmp(mode, "multiplier"))
		return fixed_times * 100;
	return 100;
}

void ae_pve_menus_coop_factor_text(long hundredths, char *text, size_t size)
{
	if (hundredths % 100 == 0)
		snprintf(text, size, "%ld\xC3\x97", hundredths / 100);
	else
		snprintf(text, size, "%ld.%ld\xC3\x97", hundredths / 100, (hundredths % 100) / 10);
}

const char *ae_pve_menus_coop_mode_help(const char *mode)
{
	if (mode && !strcmp(mode, "per_player"))
		return "Enemy squads grow with the players: by PER PLAYER GROWTH for each player past the first.";
	if (mode && !strcmp(mode, "multiplier"))
		return "Every enemy squad is FIXED SIZE times as large, for any number of players.";
	return "No extra enemies: the mission has the squads the campaign gives it.";
}

const char *ae_pve_menus_coop_wipe_help(const char *word)
{
	if (word && !strcmp(word, "revert"))
		return "When the whole team falls, the game goes back to the last checkpoint.";
	if (word && !strcmp(word, "respawn"))
		return "When the whole team falls, everyone respawns where they are. The game is not set back.";
	return "Private games go back to the checkpoint; public games only respawn. A changed team or a missing save always respawns.";
}

/* ---------- scoring */

const char *ae_pve_menus_scoring_reason(int row, int scoring_on)
{
	return row == AE_PVEM_SCORING_ROW_ON || scoring_on ? NULL : "NEEDS SCORING";
}

/* ---------- leaderboard */

void ae_pve_menus_runs_sort(struct ae_pve_menus_run *runs, int count, int by_time)
{
	int a, b;

	for (a = 1; a < count; a++)
	{
		struct ae_pve_menus_run run = runs[a];

		for (b = a - 1; b >= 0; b--)
		{
			int before;

			if (by_time)
			{
				int none_b = runs[b].seconds <= 0, none_run = run.seconds <= 0;

				before = none_b != none_run ? none_b : (!none_run && run.seconds < runs[b].seconds) ||
					(run.seconds == runs[b].seconds && run.score > runs[b].score);
			}
			else
				before = run.score > runs[b].score || (run.score == runs[b].score && run.seconds > 0 &&
					(runs[b].seconds <= 0 || run.seconds < runs[b].seconds));
			if (!before)
				break;
			runs[b + 1] = runs[b];
		}
		runs[b + 1] = run;
	}
}

void ae_pve_menus_time_text(long seconds, char *text, size_t size)
{
	if (seconds <= 0)
		snprintf(text, size, "-");
	else if (seconds >= 3600)
		snprintf(text, size, "%ld:%02ld:%02ld", seconds / 3600, (seconds / 60) % 60, seconds % 60);
	else
		snprintf(text, size, "%ld:%02ld", seconds / 60, seconds % 60);
}

void ae_pve_menus_number_text(long value, char *text, size_t size)
{
	char digits[32];
	int length, each, at = 0;

	if (value < 0)
		value = 0;
	snprintf(digits, sizeof(digits), "%ld", value);
	length = (int)strlen(digits);
	for (each = 0; each < length && (size_t)at + 2 < size; each++)
	{
		if (each && (length - each) % 3 == 0)
			text[at++] = ',';
		text[at++] = digits[each];
	}
	text[at] = 0;
}

/* ---------- saved games */

long ae_pve_menus_day(int year, int month, int month_day)
{
	/* (days from civil: March first starts the year, so the leap day is the year's last) */
	long y = month < 2 ? year - 1 : year, era = (y >= 0 ? y : y - 399) / 400, year_of_era = y - era * 400;
	long day_of_year = (153L * (month < 2 ? month + 10 : month - 2) + 2) / 5 + month_day - 1;
	long day_of_era = year_of_era * 365 + year_of_era / 4 - year_of_era / 100 + day_of_year;

	return era * 146097 + day_of_era - 719468;
}

static const char *const month_names[12] = { "JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC" };

void ae_pve_menus_when_text(long day, long today, int hour, int minute, int month, int year, int month_day, char *text, size_t size)
{
	if (day == today)
		snprintf(text, size, "TODAY %02d:%02d", hour, minute);
	else if (day == today - 1)
		snprintf(text, size, "YESTERDAY");
	else
		snprintf(text, size, "%d %s %d", month_day, month_names[month < 0 ? 0 : month > 11 ? 11 : month], year);
}

/* ---------- end card */

int ae_pve_menus_press_step(struct ae_pve_menus_press *press, int down)
{
	int pressed = 0;

	if (!press->primed)
		press->primed = 1;
	else if (down && !press->held)
		pressed = 1;
	press->held = down != 0;
	return pressed;
}

int ae_pve_menus_card_a(int index, int count, int last_mission)
{
	if (index + 1 < count)
		return AE_PVEM_CARD_NEXT_PLAYER;
	return last_mission ? AE_PVEM_CARD_FINISH : AE_PVEM_CARD_NEXT_MISSION;
}

const char *ae_pve_menus_card_a_word(int action)
{
	return action == AE_PVEM_CARD_NEXT_PLAYER ? "Next player" : action == AE_PVEM_CARD_FINISH ? "Finish" : "Next mission";
}

void ae_pve_menus_card_sub(const char *mission, const char *difficulty, int skulls, char *text, size_t size)
{
	if (skulls > 0)
		snprintf(text, size, "%s " DOT " %s " DOT " %d SKULL%s", mission, difficulty, skulls, skulls == 1 ? "" : "S");
	else
		snprintf(text, size, "%s " DOT " %s " DOT " NO SKULLS", mission, difficulty);
}
