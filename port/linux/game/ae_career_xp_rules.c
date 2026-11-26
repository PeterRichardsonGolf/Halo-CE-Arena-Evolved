/* The career XP ladder (see the header). Pure code. */
#include <stdio.h>

#include "ae_career_xp_rules.h"

static long cost(int step)
{
	return 400 + 60L * (step % AE_XP_RANKS);
}

long ae_xp_step_start(int step)
{
	long start = 0;
	int each;

	if (step < 0 || step >= AE_XP_STEPS)
		return 0;
	for (each = 0; each < step; each++)
		start += cost(each);
	return start;
}

int ae_xp_step(long xp)
{
	int step = 0;
	long next = cost(0);

	while (step < AE_XP_STEPS - 1 && xp >= next)
	{
		step++;
		next += cost(step);
	}
	return step;
}

int ae_xp_tour(int step)
{
	return step < 0 ? 1 : step >= AE_XP_STEPS ? AE_XP_TOURS : step / AE_XP_RANKS + 1;
}

int ae_xp_rank(int step)
{
	return step < 0 ? 1 : step >= AE_XP_STEPS ? AE_XP_RANKS : step % AE_XP_RANKS + 1;
}

char const *ae_xp_rank_name(int rank)
{
	static char const *const names[AE_XP_RANKS] =
	{
		"Rookie", "Recruit", "Private", "Private Second Class", "Private First Class", "Lance Corporal", "Corporal", "Sergeant",
		"Sergeant Second Class", "Sergeant First Class", "Staff Sergeant", "Gunnery Sergeant", "First Sergeant", "Sergeant Major",
		"Command Sergeant Major", "Warrant Officer", "Warrant Officer Third Class", "Warrant Officer Second Class",
		"Warrant Officer First Class", "Chief Warrant Officer", "Second Lieutenant", "First Lieutenant", "Captain", "Major",
		"Lieutenant Colonel", "Colonel", "Brigadier General", "Major General", "Lieutenant General", "General"
	};

	return rank >= 1 && rank <= AE_XP_RANKS ? names[rank - 1] : "";
}

long ae_xp_to_next(long xp)
{
	int step = ae_xp_step(xp);

	return step >= AE_XP_STEPS - 1 ? 0 : ae_xp_step_start(step + 1) - (xp < 0 ? 0 : xp);
}

int ae_xp_percent(long xp)
{
	int step = ae_xp_step(xp);
	long from, span;

	if (step >= AE_XP_STEPS - 1)
		return 100;
	from = ae_xp_step_start(step);
	span = ae_xp_step_start(step + 1) - from;
	return (int)(((xp < from ? 0 : xp - from) * 100) / span);
}

void ae_xp_title(int step, char *out, int size)
{
	if (!out || size < 1)
		return;
	snprintf(out, (size_t)size, "%s, tour %d", ae_xp_rank_name(ae_xp_rank(step)), ae_xp_tour(step));
}

static long part(long count, long each)
{
	long value = count < 0 ? 0 : count * each;

	return value > AE_XP_PART_CAP ? AE_XP_PART_CAP : value;
}

long ae_xp_game_award(struct ae_xp_game const *game, struct ae_xp_award *award)
{
	struct ae_xp_award local = { 0, 0, 0, 0, 0, 0 };

	if (!award)
		award = &local;
	*award = local;
	if (!game || game->quit || (game->seconds > 0 && game->seconds < AE_XP_MIN_SECONDS))
		return 0;
	award->finish = AE_XP_FINISH;
	award->win = game->won ? AE_XP_WIN : game->tied ? AE_XP_TIE : 0;
	award->kills = part(game->kills, AE_XP_PER_KILL);
	award->medals = part(game->medals, AE_XP_PER_MEDAL);
	award->objectives = part(game->objectives, AE_XP_PER_OBJECTIVE);
	award->total = award->finish + award->win + award->kills + award->medals + award->objectives;
	return award->total;
}

long ae_xp_campaign_award(int difficulty)
{
	static long const by_difficulty[AE_XP_DIFFICULTIES] = { 150, 250, 400, 600 };

	return difficulty >= 0 && difficulty < AE_XP_DIFFICULTIES ? by_difficulty[difficulty] : 0;
}

long ae_xp_add(long xp, long delta)
{
	long total = xp + delta;

	return total < 0 ? 0 : total > AE_XP_MAX ? AE_XP_MAX : total;
}
