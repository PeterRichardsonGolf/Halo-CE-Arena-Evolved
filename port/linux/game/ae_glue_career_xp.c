/* The career XP's and the challenges' contact with the game (see the header). */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

#include "cseries.h"
#include "ae_dev_events.h"
#include "ae_glue_career.h"
#include "ae_glue_career_xp.h"
#include "ae_dev_cmd_rules.h"
#include "ae_dev_mode.h"
#include "ae_glue_social.h"
#include "ae_glue_social_game.h"

void platform_log(char const *format, ...);

static struct ae_career_last last_game[AE_SOCIAL_LOCAL];
static int art_mode;

void ae_career_art_set(int mode)
{
	art_mode = mode ? 1 : 0;
}

int ae_career_art_on(void)
{
	return art_mode == 0;
}

void ae_career_xp_last_forget(void)
{
	memset(last_game, 0, sizeof(last_game));
}

int ae_career_xp_last(short local, struct ae_career_last *last)
{
	if (local < 0 || local >= AE_SOCIAL_LOCAL || !last_game[local].valid)
		return 0;
	*last = last_game[local];
	return 1;
}

/* the career key of a local player: the profile's, or DEV MODE's made-up one (a guest has none) */
static int key_of(short local, char *key, size_t size, int dev)
{
	if (ae_social_profile_key(local, key, size))
		return 1;
	if (!dev)
		return 0;
	snprintf(key, size, "dev-player%d", (int)local + 1);
	return 1;
}

int ae_career_xp_view(short local, struct ae_career_view *view)
{
	struct ae_career *career = ae_career_store();
	struct ae_career_challenges *lines;
	char key[AE_SR_KEY];
	int each;

	memset(view, 0, sizeof(*view));
	if (!key_of(local, key, sizeof(key), ae_dev_mode_on()))
	{
		/* (a guest: the day's picks, nothing counted) */
		int picks[AE_CH_PER_PERIOD], count, slot;

		view->guest = 1;
		for (slot = 0; slot < AE_CH_SLOTS; slot++)
		{
			struct ae_ch_kind const *kind;

			count = ae_ch_pick(slot < AE_CH_PER_PERIOD ? AE_CH_DAILY : AE_CH_WEEKLY, slot < AE_CH_PER_PERIOD ? ae_career_xp_today() :
				ae_ch_week_of(ae_career_xp_today()), picks);
			kind = slot % AE_CH_PER_PERIOD < count ? ae_ch_kind_at(picks[slot % AE_CH_PER_PERIOD]) : NULL;
			if (!kind)
				continue;
			ae_ch_label(kind, view->slot[slot].label, sizeof(view->slot[slot].label));
			view->slot[slot].target = kind->target;
			view->slot[slot].xp = kind->xp;
			view->slot[slot].metric = kind->metric;
		}
		snprintf(view->name, sizeof(view->name), "%s", ae_xp_rank_name(1));
		return 1;
	}
	if (!ae_career_challenges_refresh(career, key, ae_career_xp_today(), &lines))
		return 0;
	view->xp = ae_career_xp_get(career, key);
	view->step = ae_xp_step(view->xp);
	view->rank = ae_xp_rank(view->step);
	view->tour = ae_xp_tour(view->step);
	view->percent = ae_xp_percent(view->xp);
	view->to_next = ae_xp_to_next(view->xp);
	snprintf(view->name, sizeof(view->name), "%s", ae_xp_rank_name(view->rank));
	snprintf(view->next, sizeof(view->next), "%s", view->rank < AE_XP_RANKS ? ae_xp_rank_name(view->rank + 1) : "the next tour");
	for (each = 0; each < AE_CH_SLOTS; each++)
	{
		struct ae_ch_kind const *kind = ae_ch_kind_at(ae_ch_kind_find(lines->slot[each].kind));

		if (!kind)
			continue;
		ae_ch_label(kind, view->slot[each].label, sizeof(view->slot[each].label));
		view->slot[each].progress = lines->slot[each].progress;
		view->slot[each].target = kind->target;
		view->slot[each].xp = kind->xp;
		view->slot[each].metric = kind->metric;
		view->slot[each].done = lines->slot[each].done;
		if (lines->slot[each].done)
			(each < AE_CH_PER_PERIOD ? &view->daily_done : &view->weekly_done)[0]++;
	}
	return 1;
}

long ae_career_xp_today(void)
{
	time_t now = time(NULL);
	struct tm *local = localtime(&now);

	return local ? ae_ch_day_from_date(local->tm_year + 1900, local->tm_mon + 1, local->tm_mday) : ae_ch_day_of((long)now);
}

long ae_career_xp_profile_xp(char const *profile)
{
	return ae_career_xp_get(ae_career_store(), profile);
}

/* the paid game logged: its XP by part, the rank when it rose, each challenge that paid */
static void result_log(char const *profile, char const *what, struct ae_xp_result const *result)
{
	char why[160];
	int each;

	if (result->award.finish || result->award.win || result->award.kills || result->award.medals || result->award.objectives)
		snprintf(why, sizeof(why), "%s finish %ld win %ld kills %ld medals %ld objectives %ld", what, result->award.finish,
			result->award.win, result->award.kills, result->award.medals, result->award.objectives);
	else
		snprintf(why, sizeof(why), "%s %ld", what, result->award.total);
	{
		char title[96];

		ae_xp_title(result->step_after, title, sizeof(title));
		platform_log("career xp: +%ld (%s), total %ld, %s", result->xp_after - result->xp_before, why, result->xp_after, title);
		if (result->step_after > result->step_before)
			platform_log("career xp: rank up to %s", title);
	}
	for (each = 0; each < result->paid_count; each++)
		platform_log("career challenge: %s done, +%ld xp", result->paid[each].id, result->paid[each].xp);
	ae_dev_event("career_xp", "\"profile\":\"%s\",\"gained\":%ld,\"total\":%ld,\"step\":%d,\"challenges\":%d", profile,
		result->xp_after - result->xp_before, result->xp_after, result->step_after, result->paid_count);
}

/* the result saved, or the career put back when the file cannot be written */
static int result_keep(struct ae_career *career, struct ae_career const *before, char const *profile, char const *what,
	struct ae_xp_result const *result)
{
	if (!ae_career_store_save())
	{
		*career = *before;
		platform_log("career xp: %s: the career file could not be written: not counted", profile);
		return 0;
	}
	result_log(profile, what, result);
	return 1;
}

int ae_career_xp_game_ended(struct ae_sr_game const *game)
{
	static struct ae_career before;
	struct ae_career *career = ae_career_store();
	long day = ae_career_xp_today();
	int local, paid = 0, each;

	if (!game)
		return 0;
	for (local = 0; local < 4; local++)
	{
		char key[AE_SR_KEY];

		if (!ae_social_profile_key((short)local, key, sizeof(key)))
			continue;
		for (each = 0; each < game->count; each++)
		{
			struct ae_sr_game_player const *player = &game->player[each];
			char player_key[AE_SR_KEY];
			struct ae_xp_game facts;
			struct ae_ch_game counts;
			struct ae_xp_result result;
			long objectives;

			ae_sr_key(player->name, player_key, sizeof(player_key));
			if (!player->me || strcmp(player_key, key))
				continue;
			objectives = player->x.x.flag_caps + player->x.x.flag_returns + player->x.x.bomb_plants + player->x.x.bomb_defuses;
			memset(&facts, 0, sizeof(facts));
			facts.won = player->won;
			facts.tied = player->has_x ? player->x.tied : 0;
			facts.quit = player->quit;
			facts.kills = player->kills;
			facts.medals = player->medals_earned;
			facts.objectives = objectives;
			facts.seconds = player->has_x ? player->x.x.seconds : 0;
			memset(&counts, 0, sizeof(counts));
			counts.kind = game->mode;
			counts.won = player->won;
			counts.quit = player->quit;
			counts.kills = player->kills;
			counts.assists = player->assists;
			counts.medals = player->medals_earned;
			counts.objectives = objectives;
			counts.seconds = facts.seconds;
			before = *career;
			if (ae_career_xp_game(career, key, &facts, &counts, day, (unsigned long)time(NULL), &result) &&
				result_keep(career, &before, key, "game", &result))
			{
				struct ae_career_last *last = &last_game[local];

				paid++;
				memset(last, 0, sizeof(*last));
				last->valid = 1;
				last->gained = result.xp_after - result.xp_before;
				last->challenge_xp = result.challenge_xp;
				last->step_before = result.step_before;
				last->step_after = result.step_after;
				last->paid_count = result.paid_count;
				last->award = result.award;
				if (result.paid_count > 0)
				{
					int kind = ae_ch_kind_find(result.paid[0].id);

					if (kind >= 0)
						ae_ch_label(ae_ch_kind_at(kind), last->challenge, sizeof(last->challenge));
				}
			}
			break;
		}
	}
	return paid;
}

int ae_career_xp_campaign_won(int difficulty)
{
	static struct ae_career before;
	struct ae_career *career = ae_career_store();
	long day = ae_career_xp_today();
	int local, paid = 0;

	for (local = 0; local < 4; local++)
	{
		char key[AE_SR_KEY];
		struct ae_xp_result result;

		if (!ae_social_profile_key((short)local, key, sizeof(key)))
			continue;
		before = *career;
		if (ae_career_xp_campaign(career, key, difficulty, day, (unsigned long)time(NULL), &result) &&
			result_keep(career, &before, key, "campaign level", &result))
			paid++;
	}
	return paid;
}

/* ---------- DEV MODE */

int ae_career_xp_dev(struct ae_dev_cmd const *cmd, short player, char *text, int size)
{
	struct ae_career *career = ae_career_store();
	char key[AE_SR_KEY];
	char const *sub = cmd->word[1];
	struct ae_career_xp *line;
	int numbers = cmd->words;

	if (!key_of(player, key, sizeof(key), 1))
		return snprintf(text, (size_t)size, "social career: no profile"), 0;
	if (!strcmp(sub, "art"))
	{
		ae_career_art_set(numbers > 2 && !strcmp(cmd->word[2], "off"));
		snprintf(text, (size_t)size, "social career art: %s", ae_career_art_on() ? "auto (MCC's pictures when found)" : "off");
		return 1;
	}
	if (!strcmp(sub, "xp") || !strcmp(sub, "rank"))
	{
		long value = numbers > 2 ? atol(cmd->word[2]) : 0;

		if (!strcmp(sub, "rank"))
		{
			long tour = numbers > 3 ? atol(cmd->word[3]) : 1;

			if (value < 1 || value > AE_XP_RANKS || tour < 1 || tour > AE_XP_TOURS)
				return snprintf(text, (size_t)size, "social career rank: <1-30> <1-11>"), 0;
			value = ae_xp_step_start((int)((tour - 1) * AE_XP_RANKS + value - 1));
		}
		line = ae_career_xp_entry(career, key);
		if (!line)
			return snprintf(text, (size_t)size, "social career: the career file is full"), 0;
		line->xp = ae_xp_add(0, value);
		line->last = (unsigned long)time(NULL);
		snprintf(text, (size_t)size, ae_career_store_save() ? "social career %s: %ld XP, step %d" : "social career %s: %ld XP, step %d (not written)",
			key, line->xp, ae_xp_step(line->xp));
		return 1;
	}
	if (!strcmp(sub, "games"))
	{
		struct ae_career_social *entry;
		long games = numbers > 3 ? atol(cmd->word[3]) : 0, wins = numbers > 4 ? atol(cmd->word[4]) : 0, ties = numbers > 5 ? atol(cmd->word[5]) : 0;

		if (numbers < 4 || games < 0 || wins < 0 || ties < 0 || wins + ties > games)
			return snprintf(text, (size_t)size, "social career games <playlist> <games> [<wins> [<ties>]]"), 0;
		entry = ae_career_social_entry(career, key, cmd->word[2]);
		if (!entry)
			return snprintf(text, (size_t)size, "social career games: the career file is full"), 0;
		entry->games = games;
		entry->wins = wins;
		entry->ties = ties;
		ae_career_store_save();
		snprintf(text, (size_t)size, "social career games %s: %ld games, %ld wins, %ld ties", cmd->word[2], games, wins, ties);
		return 1;
	}
	if (!strcmp(sub, "challenge"))
	{
		struct ae_career_challenges *lines;
		long slot = numbers > 3 ? atol(cmd->word[3]) : 0;
		struct ae_ch_kind const *kind;

		if (!ae_career_challenges_refresh(career, key, ae_career_xp_today(), &lines) || slot < 1 || slot > AE_CH_SLOTS)
			return snprintf(text, (size_t)size, "social career challenge done <1-6>"), 0;
		kind = ae_ch_kind_at(ae_ch_kind_find(lines->slot[slot - 1].kind));
		if (!kind)
			return snprintf(text, (size_t)size, "social career challenge: no such slot"), 0;
		lines->slot[slot - 1].progress = kind->target;
		lines->slot[slot - 1].done = 1;
		ae_career_store_save();
		snprintf(text, (size_t)size, "social career challenge %ld (%s): done", slot, kind->id);
		return 1;
	}
	if (!strcmp(sub, "last"))
	{
		struct ae_career_last *last = &last_game[player];
		long gained = numbers > 2 ? atol(cmd->word[2]) : 136;
		int each, rankup = 0, challenge = 0;
		long xp = ae_career_xp_get(career, key);

		for (each = 3; each < numbers; each++)
		{
			rankup |= !strcmp(cmd->word[each], "rankup");
			challenge |= !strcmp(cmd->word[each], "challenge");
		}
		memset(last, 0, sizeof(*last));
		last->valid = 1;
		last->gained = gained + (challenge ? 300 : 0);
		last->challenge_xp = challenge ? 300 : 0;
		last->step_after = ae_xp_step(xp);
		last->step_before = rankup && last->step_after > 0 ? last->step_after - 1 : last->step_after;
		last->award.finish = 50;
		last->award.win = 50;
		last->award.kills = gained > 100 ? 24 : 0;
		last->award.medals = gained > 100 ? 12 : 0;
		last->award.total = gained;
		last->paid_count = challenge;
		if (challenge)
			snprintf(last->challenge, sizeof(last->challenge), "WIN 2 GAMES");
		snprintf(text, (size_t)size, "social career last: +%ld XP%s%s", last->gained, rankup ? ", rank up" : "", challenge ? ", challenge" : "");
		return 1;
	}
	return snprintf(text, (size_t)size, "social career xp <n> | rank <1-30> <1-11> | games <playlist> <n> [<wins> [<ties>]] | challenge done <1-6> | last <xp> [rankup] [challenge] | art auto|off"), 0;
}
