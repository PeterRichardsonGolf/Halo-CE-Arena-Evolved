/*
CAMPAIGN_MEDALS.H

The campaign's medals (campaign_medals.c): Halo 3's and Halo: Reach's that
the campaign's weapons, enemies and vehicles can earn, each worth points
(campaign_score_values.json's "medals").
*/

#ifndef __CAMPAIGN_MEDALS_H
#define __CAMPAIGN_MEDALS_H

/* the medals, in campaign_medal_descriptions' order: the frames of the
menus' bitmap medals/icons (port/assets/medals, tools/ce_menus.py) */
enum
{
	/* (kills within the multi-kill window of each other) */
	_campaign_medal_double_kill = 0,
	_campaign_medal_triple_kill,
	_campaign_medal_overkill,
	_campaign_medal_killtacular,
	_campaign_medal_killtrocity,
	_campaign_medal_killimanjaro,
	_campaign_medal_killtastrophe,
	_campaign_medal_killpocalypse,
	_campaign_medal_killionaire,
	/* (kills without dying: 5, 10, ... 40) */
	_campaign_medal_killing_spree,
	_campaign_medal_killing_frenzy,
	_campaign_medal_running_riot,
	_campaign_medal_rampage,
	_campaign_medal_untouchable,
	_campaign_medal_invincible,
	_campaign_medal_inconceivable,
	_campaign_medal_unfriggenbelievable,
	/* (kills of a weapon's without dying: 5 and 10) */
	_campaign_medal_shotgun_spree,
	_campaign_medal_open_season,
	_campaign_medal_sniper_spree,
	_campaign_medal_sharpshooter,
	_campaign_medal_splatter_spree,
	_campaign_medal_vehicular_manslaughter,
	_campaign_medal_stick_spree,
	/* (a kill's) */
	_campaign_medal_beat_down,
	_campaign_medal_assassin,
	_campaign_medal_headshot,
	_campaign_medal_sniper_kill,
	_campaign_medal_grenade_stick,
	_campaign_medal_splatter,
	_campaign_medal_incineration,
	_campaign_medal_death_from_the_grave,
	_campaign_medal_close_call,
	_campaign_medal_first_strike,
	_campaign_medal_revenge,
	_campaign_medal_avenger,
	_campaign_medal_wheelman,
	/* (the level's) */
	_campaign_medal_perfection,
	NUMBER_OF_CAMPAIGN_MEDALS
};

struct campaign_medal_description
{
	char const *key;
	char const *label;
};

extern struct campaign_medal_description const campaign_medal_descriptions[NUMBER_OF_CAMPAIGN_MEDALS];

/* medals are given, as the score is kept (game.campaign_score: the host's,
in co-op), or in a multiplayer game with game.multiplayer_medals (each
machine its own players'); game.campaign_medals, whether this machine shows
those just won at the center left in the campaign (the history above the
score is always shown; a multiplayer game's medals always show both) */
boolean campaign_medals_enabled(void);
boolean campaign_medals_popups_shown(void);

/* (campaign_score.c) a new level: the sprees and the popups forgotten */
void campaign_medals_new_map(void);

/* (campaign_score.c, on the machine that runs the AI) an enemy killed: by a
player (its datum index, NONE for none) or any unit (`killer_object_index`,
an AI gunner's for Wheelman), by the damage effect, and how */
void campaign_medals_enemy_killed(
	long killer_player_index,
	long killer_object_index,
	long victim_unit_index,
	long damage_definition_index,
	boolean headshot,
	boolean melee,
	boolean backsmack);

/* (campaign_score.c) a multiplayer game's player killed by another (datum
indices), with the unit that did it (NONE for none), by the damage effect,
and how; on every machine, for its own players' medals */
void campaign_medals_player_killed_player(
	long killer_player_index,
	long killer_object_index,
	long victim_player_index,
	long damage_definition_index,
	boolean headshot,
	boolean melee,
	boolean backsmack);

/* (campaign_score.c) a player died, killed by the object (NONE: none), and
in a multiplayer game by the player (NONE: none): the sprees end, and the
killer is remembered for Revenge and Avenger */
void campaign_medals_player_died(
	long player_index,
	long killer_player_index,
	long killer_object_index);

/* (campaign_score.c) the level is won: Perfection */
void campaign_medals_level_won(void);

/* (campaign_score.c) a medal logged for a player (its absolute index), on
every machine: shown to the player, if one of this machine's */
void campaign_medals_awarded(
	short player_index,
	short medal);

/* (campaign_score.c's campaign_score_draw_view) the medals just won, at the
center left of the view's player's view */
void campaign_medals_draw_popups(
	long player_index);

/* the medals' icons, the menus' bitmap (NONE without the PC menus) */
long campaign_medals_bitmap(void);

#endif
