/*
AE_GLUE_PVE_MENUS.H

What the campaign's saved games, leaderboard and scoring screens read of the game (ae_glue_pve_menus.c): the profiles on this
PC, each profile's saved game (read with the profile as player 1's, which then is again), a level's records in each profile's
campaign_records.txt, and a saved game deleted. Include after cseries.h.
*/

#ifndef __AE_GLUE_PVE_MENUS_H
#define __AE_GLUE_PVE_MENUS_H

enum { AE_PVE_PROFILES = 64 };

struct ae_pve_saved
{
	long profile_index;
	char name[48];
	short level, difficulty;
	/* the profile player 1 plays with now */
	int active;
	/* when the file was kept: days since 1970 (local), and the words' parts; day -1 when unknown */
	long day;
	int hour, minute, month, year, month_day;
};

/* every profile's saved game of the stock campaign (a profile with none is left out): how many, up to maximum */
int ae_pve_saved_games(struct ae_pve_saved *saved, int maximum);
/* player 1 plays with the profile from now on (the saved game then is the campaign's CONTINUE); 1 when done */
int ae_pve_saved_pick(long profile_index);
/* the profile's saved game file deleted (its finished missions and records stay); 1 when it was there and is gone */
int ae_pve_saved_delete(long profile_index);

/* a profile's records of a stock level (0 to 9) */
struct ae_pve_run
{
	long profile_index;
	char name[48];
	long score, seconds, difficulty, skulls, skulls_percent, medals;
};
/* the profiles with a record of the level, up to maximum */
int ae_pve_level_runs(short level, struct ae_pve_run *runs, int maximum);
/* the level's map name as the records name it (levels\a10\a10) */
const char *ae_pve_level_map(short level);
/* a level's last name (a10), for the par table */
const char *ae_pve_level_short(short level);
/* player 1's profile index, NONE for none */
long ae_pve_player_profile(void);


/* ---------- the randomizer's item sets (Tag Injector profiles) */

enum { AE_PVE_ITEM_SETS = 32, AE_PVE_ITEM_TYPE = 32 };
struct ae_pve_item_set
{
	char folder[64];
	char name[48];
	long items_on;
};
/* the item sets this PC has (the folders found again first), up to maximum */
int ae_pve_item_sets(struct ae_pve_item_set *sets, int maximum);
/* the set the randomizer plays (game.randomizer_tag_profile) as an index of the list, -1 for none */
int ae_pve_item_set_played(void);
/* the played set's custom types of its actors (weapons 0) or weapons (1): how many, each in capitals; 0 with no set played */
int ae_pve_item_set_types(int weapons, char (*types)[AE_PVE_ITEM_TYPE], int maximum);
/* the playing set's weapons for PLAYER LOADOUT's picker: each weapon on (not a flag or an oddball), its name in capitals and its value
("tag:<hash>", randomizer_loadout_tag_value's); how many (at most maximum); 0 with no set played */
int ae_pve_item_set_weapons(char (*labels)[48], char (*values)[24], int maximum);
/* what the set holds on: the weapon, the actor and the equipment tags */
void ae_pve_item_set_counts(long profile_list_index, long *weapons, long *actors);

#endif
