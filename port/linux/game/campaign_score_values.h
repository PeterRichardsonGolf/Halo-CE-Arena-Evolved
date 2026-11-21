/*
CAMPAIGN_SCORE_VALUES.H

The campaign's scoring values (ae_campaign_score.c), as
campaign_score_values.json has them: tools/campaign_score_values.py writes
campaign_score_values.c from it (configure.py runs it, and ninja reruns
configure.py when the file changes).
*/

#ifndef __CAMPAIGN_SCORE_VALUES_H
#define __CAMPAIGN_SCORE_VALUES_H

enum
{
	/* (the style's and the skulls' "combine") */
	_campaign_score_combine_multiply = 0,
	_campaign_score_combine_highest,
};

/* points for a tag whose name has any of the words (lower case; the list
ends with NULL) */
struct campaign_score_match_value
{
	char const *const *words;
	int points;
};

/* an actor type's points (actor_types.h's enum): its variants' by their
names' words, the first that matches, else its default */
struct campaign_score_actor_value
{
	short actor_type;
	int points;
	short variant_count;
	struct campaign_score_match_value const *variants;
};

/* the level's time over its par time, up to (and including) up_to_par (less
than 0: any) */
struct campaign_score_time_bracket
{
	float up_to_par;
	float multiplier;
};

/* a level's par time, by its name (a10, the last of levels\a10\a10) */
struct campaign_score_par_time
{
	char const *level;
	int seconds;
};

/* a skull's multiplier, by its key (skulls.c's) */
struct campaign_score_skull_value
{
	char const *skull;
	float multiplier;
};

/* a medal's points, by its key (ae_campaign_medals_rules.c's) */
struct campaign_score_medal_value
{
	char const *medal;
	int points;
};

enum
{
	/* the weapons the medals tell apart by the killing blow's damage
	effect's name: sniper, shotgun, flame, grenade stick, splatter */
	CAMPAIGN_SCORE_MEDAL_WEAPONS = 5,
};

struct campaign_score_values
{
	/* easy, normal, heroic, legendary */
	float difficulty[4];

	short time_bracket_count;
	struct campaign_score_time_bracket const *time_brackets;
	short par_time_count;
	struct campaign_score_par_time const *par_times;
	/* the par times' multipliers: by the difficulty (easy, normal, heroic,
	legendary), and in a daily challenge's run */
	float par_difficulty[4];
	float par_daily_challenge;

	short style_combine;
	float style_regular;
	float style_one_shot_one_kill;
	float style_headshot;
	float style_melee;
	float style_backsmack;

	short enemy_count;
	struct campaign_score_actor_value const *enemies;
	short vehicle_count;
	struct campaign_score_match_value const *vehicles;

	int penalty_death;
	int penalty_betrayal;
	int penalty_checkpoint_revert;
	short ally_count;
	struct campaign_score_actor_value const *allies;
	short ally_vehicle_count;
	struct campaign_score_match_value const *ally_vehicles;

	short skull_combine;
	float skull_default;
	short skull_count;
	struct campaign_score_skull_value const *skulls;

	/* the medals: the multi-kill's window, Perfection's kills, Close Call's
	health (a fraction), how many the history shows, how long one is shown,
	whether their points are times the difficulty and the skulls, the
	weapons' words (each list ends with NULL), and each medal's points */
	float medal_multi_kill_seconds;
	int medal_perfection_kills;
	float medal_close_call_health;
	short medal_history;
	float medal_popup_seconds;
	short medal_scaled;
	char const *const *medal_weapon_words[CAMPAIGN_SCORE_MEDAL_WEAPONS];
	short medal_count;
	struct campaign_score_medal_value const *medals;
};

extern struct campaign_score_values const campaign_score_values;

#endif
