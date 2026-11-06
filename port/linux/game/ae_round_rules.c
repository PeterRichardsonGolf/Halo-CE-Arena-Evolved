/* The round modes' rules (see the header) */
#include "ae_round_rules.h"

int ae_round_players_full(int players)
{
	return players >= AE_ROUND_PLAYER_LIMIT;
}

long ae_round_bot_cap(long wanted, long humans)
{
	long room = AE_ROUND_PLAYER_LIMIT - (humans < 1 ? 1 : humans);

	if (room < 0)
		room = 0;
	return wanted < 0 ? 0 : wanted > room ? room : wanted;
}

int ae_round_judge(int sides, int const *members, int const *alive, int *winner)
{
	int side, judged = 0, standing = 0, last = -1;

	if (winner)
		*winner = -1;
	if (sides > AE_ROUND_SIDES)
		sides = AE_ROUND_SIDES;
	for (side = 0; side < sides; side++)
	{
		if (members[side] <= 0)
			continue;
		judged++;
		if (alive[side] > 0)
		{
			standing++;
			last = side;
		}
	}
	if (!judged)
		return AE_ROUND_GOES_ON;
	if (!standing)
		return AE_ROUND_TIED;
	if (standing == 1 && judged >= 2)
	{
		if (winner)
			*winner = last;
		return AE_ROUND_WON;
	}
	return AE_ROUND_GOES_ON;
}

int ae_round_match_over(int max_rounds, int to_win, int resolve_ties, int played, int best, int runner_up)
{
	if (to_win > 0 && best >= to_win)
		return 1;
	if (played < max_rounds)
		return 0;
	if (best > runner_up)
		return 1;
	return !resolve_ties;
}

int ae_round_halftime(int max_rounds, int to_win)
{
	if (to_win > 0)
		return to_win;
	return max_rounds / 2 + 1;
}

int ae_round_sides_switched(int round, int max_rounds, int to_win, int switch_on)
{
	return switch_on && max_rounds > 1 && round >= ae_round_halftime(max_rounds, to_win);
}

long ae_round_revive_ticks(int tenths, int revives_before)
{
	long ticks = (long)tenths * AE_ROUND_TICKS_PER_SECOND / 10;
	int step;

	if (ticks < 1)
		ticks = 1;
	if (revives_before > AE_ROUND_REVIVE_GROWTH_LIMIT)
		revives_before = AE_ROUND_REVIVE_GROWTH_LIMIT;
	for (step = 0; step < revives_before; step++)
		ticks = ticks * 3 / 2;
	return ticks;
}

long ae_round_revive_step(long progress, int holding)
{
	return holding ? progress + 1 : 0;
}

long ae_round_countdown_left(long round_start, long now)
{
	if (round_start < 0 || now < round_start || now - round_start >= AE_ROUND_PRE_TICKS)
		return 0;
	return AE_ROUND_PRE_TICKS - (now - round_start);
}

int ae_round_max_rounds(struct ae_variant_ext const *ext)
{
	int rounds = ae_ext_rounds_count(ext);

	return rounds > 0 ? rounds : AE_ROUND_DEFAULT_ROUNDS;
}

int ae_round_to_win(struct ae_variant_ext const *ext)
{
	if (ae_ext_rounds_count(ext) <= 0)
		return AE_ROUND_DEFAULT_TO_WIN;
	return ae_ext_rounds_to_win(ext);
}

int ae_round_revive_on(struct ae_variant_ext const *ext, int teams)
{
	return teams && !(ext->round_flags & AE_EXT_ROUND_NO_REVIVE);
}

int ae_round_zone_on(struct ae_variant_ext const *ext)
{
	return !(ext->round_flags & AE_EXT_ROUND_NO_ZONE);
}

int ae_round_switch_on(struct ae_variant_ext const *ext, int teams)
{
	return teams && !(ext->round_flags & AE_EXT_ROUND_NO_SIDE_SWITCH);
}

int ae_round_revive_tenths(struct ae_variant_ext const *ext)
{
	return ext->revive_10ths ? ext->revive_10ths : AE_ROUND_DEFAULT_REVIVE_10THS;
}

int ae_round_bleedout_seconds(struct ae_variant_ext const *ext)
{
	return ext->bleedout_s;
}

int ae_round_zone_wait_seconds(struct ae_variant_ext const *ext)
{
	return ext->zone_wait_s ? ext->zone_wait_s : 60;
}

int ae_round_zone_shrink_seconds(struct ae_variant_ext const *ext)
{
	return ext->zone_shrink_s ? ext->zone_shrink_s : 30;
}

char const *ae_round_phase_name(int phase)
{
	static char const *const names[AE_ROUND_PHASE_COUNT] = { "off", "pre", "live", "post", "over" };

	return phase >= 0 && phase < AE_ROUND_PHASE_COUNT ? names[phase] : "?";
}

char const *ae_round_player_state_name(int state)
{
	static char const *const names[AE_ROUND_PLAYER_STATE_COUNT] = { "none", "alive", "down", "out", "waiting",
		"revived" };

	return state >= 0 && state < AE_ROUND_PLAYER_STATE_COUNT ? names[state] : "?";
}

int ae_round_bot_revive_safe(long ticks_since_enemy_seen, int downed_is_human)
{
	return ticks_since_enemy_seen > (downed_is_human ? AE_ROUND_BOT_HUMAN_SAFE_TICKS : AE_ROUND_BOT_SAFE_TICKS);
}

int ae_round_bot_revive_decide(real orb_distance, long ticks_since_hurt, real enemy_distance, int nearer_free_bots)
{
	if (orb_distance < 0.0f)
		return AE_ROUND_REVIVE_NO_ORB;
	if (orb_distance > AE_ROUND_BOT_REVIVE_RANGE)
		return AE_ROUND_REVIVE_FAR;
	if (ticks_since_hurt >= 0 && ticks_since_hurt < AE_ROUND_REVIVE_FIRE_TICKS)
		return AE_ROUND_REVIVE_UNDER_FIRE;
	if (enemy_distance >= 0.0f && enemy_distance < AE_ROUND_REVIVE_ENEMY_RANGE)
		return AE_ROUND_REVIVE_ENEMY_NEAR;
	if (nearer_free_bots > 0)
		return AE_ROUND_REVIVE_NOT_NEAREST;
	return AE_ROUND_REVIVE_GO;
}

int ae_round_revive_mate_is_nearer(real mate_distance, real my_distance, int mate_has_lower_index, int is_reviver)
{
	if (is_reviver)
		return mate_distance <= my_distance - AE_ROUND_REVIVE_HOLD_MARGIN;
	return mate_distance < my_distance || (mate_distance == my_distance && mate_has_lower_index);
}

char const *ae_round_revive_reason_name(int reason)
{
	static char const *const names[AE_ROUND_REVIVE_REASON_COUNT] = { "go", "no_orb", "far", "under_fire", "enemy_near",
		"not_nearest" };

	return reason >= 0 && reason < AE_ROUND_REVIVE_REASON_COUNT ? names[reason] : "?";
}
