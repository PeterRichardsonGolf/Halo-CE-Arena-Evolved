/* A game's career XP and challenge progress on the career record (see the header). Pure code. */
#include <stdio.h>
#include <string.h>

#include "ae_career_xp.h"

static void slots_pick(struct ae_career_challenges *lines, int period, long period_id)
{
	int picks[AE_CH_PER_PERIOD], count = ae_ch_pick(period, period_id, picks), each;
	int first = period == AE_CH_DAILY ? 0 : AE_CH_PER_PERIOD;

	for (each = 0; each < AE_CH_PER_PERIOD; each++)
	{
		memset(&lines->slot[first + each], 0, sizeof(lines->slot[0]));
		if (each < count)
			snprintf(lines->slot[first + each].kind, sizeof(lines->slot[0].kind), "%s", ae_ch_kind_at(picks[each])->id);
	}
}

/* whether a period's slots hold the picks of its id */
static int slots_match(struct ae_career_challenges const *lines, int period, long period_id)
{
	int picks[AE_CH_PER_PERIOD], count = ae_ch_pick(period, period_id, picks), each;
	int first = period == AE_CH_DAILY ? 0 : AE_CH_PER_PERIOD;

	for (each = 0; each < AE_CH_PER_PERIOD; each++)
		if (each >= count || strcmp(lines->slot[first + each].kind, ae_ch_kind_at(picks[each])->id))
			return 0;
	return 1;
}

int ae_career_challenges_refresh(struct ae_career *career, char const *profile, long day, struct ae_career_challenges **out)
{
	struct ae_career_challenges *lines = ae_career_challenges_entry(career, profile);
	long week = ae_ch_week_of(day);

	if (!lines)
		return 0;
	if (lines->day != day || !slots_match(lines, AE_CH_DAILY, day))
	{
		slots_pick(lines, AE_CH_DAILY, day);
		lines->day = day;
	}
	if (lines->week != week || !slots_match(lines, AE_CH_WEEKLY, week))
	{
		slots_pick(lines, AE_CH_WEEKLY, week);
		lines->week = week;
	}
	if (out)
		*out = lines;
	return 1;
}

/* the counters moved by gain(kind) for each slot, a target reached paid once: the XP paid is added to result */
static void slots_count(struct ae_career_challenges *lines, long (*gain)(struct ae_ch_kind const *, void const *), void const *what,
	struct ae_xp_result *result)
{
	int each;

	for (each = 0; each < AE_CAREER_CHALLENGE_SLOTS; each++)
	{
		struct ae_career_challenge_slot *slot = &lines->slot[each];
		struct ae_ch_kind const *kind = ae_ch_kind_at(ae_ch_kind_find(slot->kind));
		long add;

		if (!kind || slot->done)
			continue;
		add = gain(kind, what);
		if (add <= 0)
			continue;
		slot->progress += add;
		if (slot->progress >= kind->target)
		{
			slot->progress = kind->target;
			slot->done = 1;
			result->challenge_xp += kind->xp;
			snprintf(result->paid[result->paid_count].id, sizeof(result->paid[0].id), "%s", kind->id);
			result->paid[result->paid_count++].xp = kind->xp;
		}
	}
}

static long game_gain(struct ae_ch_kind const *kind, void const *what)
{
	return ae_ch_game_gain(kind, (struct ae_ch_game const *)what);
}

static long campaign_gain(struct ae_ch_kind const *kind, void const *what)
{
	(void)what;
	return ae_ch_campaign_gain(kind);
}

/* the XP and the challenges' XP put on the profile's line */
static int commit(struct ae_career *career, char const *profile, unsigned long clock, struct ae_xp_result *result)
{
	struct ae_career_xp *line = ae_career_xp_entry(career, profile);

	if (!line)
		return 0;
	result->xp_before = line->xp;
	result->step_before = ae_xp_step(line->xp);
	line->xp = ae_xp_add(line->xp, result->award.total + result->challenge_xp);
	line->last = clock;
	result->xp_after = line->xp;
	result->step_after = ae_xp_step(line->xp);
	return 1;
}

int ae_career_xp_game(struct ae_career *career, char const *profile, struct ae_xp_game const *game, struct ae_ch_game const *counts,
	long day, unsigned long clock, struct ae_xp_result *result)
{
	struct ae_xp_result local;
	struct ae_career_challenges *lines;

	if (!result)
		result = &local;
	memset(result, 0, sizeof(*result));
	result->xp_before = result->xp_after = ae_career_xp_get(career, profile);
	result->step_before = result->step_after = ae_xp_step(result->xp_before);
	if (!game || !counts || game->quit || (game->seconds > 0 && game->seconds < AE_XP_MIN_SECONDS))
		return 0;
	/* (both lines or neither: a full table refuses before anything moves) */
	if (!ae_career_xp_entry(career, profile) || !ae_career_challenges_refresh(career, profile, day, &lines))
		return 0;
	ae_xp_game_award(game, &result->award);
	slots_count(lines, game_gain, counts, result);
	return commit(career, profile, clock, result);
}

int ae_career_xp_campaign(struct ae_career *career, char const *profile, int difficulty, long day, unsigned long clock,
	struct ae_xp_result *result)
{
	struct ae_xp_result local;
	struct ae_career_challenges *lines;

	if (!result)
		result = &local;
	memset(result, 0, sizeof(*result));
	result->xp_before = result->xp_after = ae_career_xp_get(career, profile);
	result->step_before = result->step_after = ae_xp_step(result->xp_before);
	if (ae_xp_campaign_award(difficulty) <= 0 || !ae_career_xp_entry(career, profile) ||
		!ae_career_challenges_refresh(career, profile, day, &lines))
		return 0;
	result->award.total = ae_xp_campaign_award(difficulty);
	slots_count(lines, campaign_gain, NULL, result);
	return commit(career, profile, clock, result);
}
