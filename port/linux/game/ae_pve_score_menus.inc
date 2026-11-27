/*
AE_PVE_SCORE_MENUS.INC

Included at the end of campaign_score.c (the score's statics are in view): what AE's menus read of the score. See
ae_pve_score_menus.h.
*/

float ae_pve_score_skulls_multiplier(struct skull_set set)
{
	struct campaign_score_values const *values = &campaign_score_values;
	boolean highest = values->skull_combine == _campaign_score_combine_highest;
	boolean any = FALSE;
	real multiplier = 1.0f;
	int skull;

	for (skull = 0; skull < NUMBER_OF_SKULLS; skull++)
	{
		real value = values->skull_default;
		short index;

		if (!skull_set_has(&set, skull))
			continue;
		for (index = 0; index < values->skull_count; index++)
		{
			if (!strcmp(values->skulls[index].skull, skull_descriptions[skull].key))
			{
				value = values->skulls[index].multiplier;
				break;
			}
		}
		if (highest)
			multiplier = any ? MAX(multiplier, value) : value;
		else
			multiplier *= value;
		any = TRUE;
	}
	return (float)multiplier;
}

long ae_pve_score_par_seconds(char const *level_name, short difficulty)
{
	short index;

	if (!level_name || difficulty < 0 || difficulty >= NUMBER_OF_GAME_DIFFICULTY_LEVELS)
		return NONE;
	for (index = 0; index < campaign_score_values.par_time_count; index++)
	{
		if (!strcmp(campaign_score_values.par_times[index].level, level_name))
			return (long)((real)campaign_score_values.par_times[index].seconds * campaign_score_values.par_difficulty[difficulty] + 0.5f);
	}
	return NONE;
}

/* whether the summary is up over the level just won: shown by the game's card or AE's, until each player's has been dismissed */
static boolean ae_pve_card_up(void)
{
	return campaign_score_postgame && campaign_score_summary_shown < campaign_score_summary_count &&
		campaign_score_summary[campaign_score_summary_shown].valid && campaign_score_level() && game_in_progress() &&
		!cinematic_in_progress();
}

static void ae_pve_card_narrow(wchar_t const *wide, char *text, int size)
{
	int length = 0;

	for (; wide && *wide && length < size - 1; wide++)
		text[length++] = *wide >= 0x20 && *wide < 0x7F ? (char)*wide : '?';
	text[length] = 0;
}

int ae_pve_card_shown(struct ae_pve_card *card, int *index, int *count)
{
	struct campaign_score_report const *report;
	short each;

	if (!ae_pve_card_up())
		return 0;
	report = &campaign_score_summary[campaign_score_summary_shown];
	memset(card, 0, sizeof(*card));
	card->competitive = report->competitive;
	ae_pve_card_narrow(report->player, card->player, (int)sizeof(card->player));
	ae_pve_card_narrow(report->level, card->level, (int)sizeof(card->level));
	card->player_number = report->player_number;
	card->player_total = report->player_total;
	card->difficulty = report->difficulty;
	card->record_score = report->record_score;
	card->record_time = report->record_time;
	card->difficulty_multiplier = report->difficulty_multiplier;
	card->skulls_multiplier = report->skulls_multiplier;
	card->time_multiplier = report->time_multiplier;
	card->seconds = report->seconds;
	card->par_seconds = report->par_seconds;
	card->kills = report->kills;
	card->headshots = report->headshots;
	card->melee = report->melee;
	card->medals = report->medals;
	card->deaths = report->deaths;
	card->betrayals = report->betrayals;
	card->allies = report->allies;
	card->points = report->points;
	card->score = report->score;
	for (each = 0; each < NUMBER_OF_CAMPAIGN_MEDALS && each < AE_PVE_CARD_MEDALS; each++)
		card->medal_counts[each] = report->medal_counts[each];
	card->player_count = report->player_count < AE_PVE_CARD_PLAYERS ? report->player_count : AE_PVE_CARD_PLAYERS;
	for (each = 0; each < card->player_count; each++)
	{
		snprintf(card->players[each].name, sizeof(card->players[each].name), "%s", report->players[each].name);
		card->players[each].score = report->players[each].score;
		card->players[each].kills = report->players[each].kills;
	}
	*index = campaign_score_summary_shown;
	*count = campaign_score_summary_count;
	return 1;
}

void ae_pve_card_advance(void)
{
	if (campaign_score_summary_shown < campaign_score_summary_count)
		campaign_score_summary_shown++;
}

char const *ae_pve_card_medal_key(int medal)
{
	return medal >= 0 && medal < NUMBER_OF_CAMPAIGN_MEDALS ? campaign_medal_descriptions[medal].key : "";
}

char const *ae_pve_card_medal_label(int medal)
{
	return medal >= 0 && medal < NUMBER_OF_CAMPAIGN_MEDALS ? campaign_medal_descriptions[medal].label : "";
}

int ae_pve_card_medal_count(void)
{
	return NUMBER_OF_CAMPAIGN_MEDALS;
}

int ae_pve_card_level_index(void)
{
	static char const *const stock[] = { "a10", "a30", "a50", "b30", "b40", "c10", "c20", "c40", "d20", "d40" };
	char lower[128];
	char const *last;
	short index;

	campaign_score_lower_name(global_scenario_index, lower, sizeof(lower));
	last = lower;
	for (index = 0; lower[index]; index++)
		if (lower[index] == '\\' || lower[index] == '/')
			last = lower + index + 1;
	for (index = 0; index < (short)(sizeof(stock) / sizeof(stock[0])); index++)
		if (!strcmp(stock[index], last))
			return index;
	return -1;
}
