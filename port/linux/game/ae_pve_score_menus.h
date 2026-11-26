/*
AE_PVE_SCORE_MENUS.H

What AE's menus read of the campaign score (campaign_score.c holds the numbers; ae_pve_score_menus.inc, included at its end,
answers): the score's multipliers for a set of skulls and a level's par time as the menus show them before a game, and the
level summary cards for AE's end-of-mission card (ae_screen_end_card.c). Include after cseries.h and halo_skulls.h.
*/

#ifndef __AE_PVE_SCORE_MENUS_H
#define __AE_PVE_SCORE_MENUS_H

/* the score's multiplier of these skulls (each one's, multiplied together, or the highest as the values say) */
float ae_pve_score_skulls_multiplier(struct skull_set set);
/* a level's par time in seconds at a difficulty (0 easy to 3 legendary), by its map's last name (a10); -1 for none */
long ae_pve_score_par_seconds(char const *level_name, short difficulty);

/* a level summary card (one for each local player of this machine, shown in turn) as the menus draw it */
enum { AE_PVE_CARD_PLAYERS = 8, AE_PVE_CARD_MEDALS = 40 };
struct ae_pve_card
{
	int competitive;
	char player[48], level[96];
	short player_number, player_total, difficulty;
	int record_score, record_time;
	float difficulty_multiplier, skulls_multiplier, time_multiplier;
	long seconds, par_seconds, kills, headshots, melee, medals, deaths, betrayals, allies, points, score;
	short medal_counts[AE_PVE_CARD_MEDALS];
	short player_count;
	struct { char name[24]; long score, kills; } players[AE_PVE_CARD_PLAYERS];
};
/* the summary shown now: its place and how many cards there are (0 when none is up: no level won, a cinematic playing, no level
being played); fills card */
int ae_pve_card_shown(struct ae_pve_card *card, int *index, int *count);
/* the shown card dismissed: the next of this machine's players' card comes, or none is left and the level goes on */
void ae_pve_card_advance(void);
/* the stock level being played (0 to 9, the campaign's order), -1 for another */
int ae_pve_card_level_index(void);
/* the campaign medals' keys by place, and how many there are */
char const *ae_pve_card_medal_key(int medal);
char const *ae_pve_card_medal_label(int medal);
int ae_pve_card_medal_count(void);


/* (campaign_score_draw's hook) 1 when AE's end-of-mission card draws the summary and takes its presses: AE's menus are on */
int ae_end_card_wanted(void);

#endif
