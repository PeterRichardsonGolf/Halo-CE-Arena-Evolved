/*
AE_GLUE_CAMPAIGN.H

AE's CAMPAIGN glue (ae_glue_campaign.c; the 0.3.7 menus): player 1's campaign as the PC menus read it
(menu_functions.c's campaign_profile and campaign_levels_read): the levels' names, which are reached (played, the one
after the last finished, the first) and the difficulties each is finished on, the saved game; and a level started, or
the saved game continued, as campaign_start does. Menus' thread only. Include after cseries.h.
*/

#ifndef __AE_GLUE_CAMPAIGN_H
#define __AE_GLUE_CAMPAIGN_H

#include "ae_campaign_workshop_rules.h"

enum { AE_CAMPAIGN_LEVELS = 10, AE_CAMPAIGN_DIFFICULTIES = 4 };

struct ae_campaign_level
{
	char name[64];
	int reached;
	int finished[AE_CAMPAIGN_DIFFICULTIES];   /* (the game marks NORMAL, HEROIC, LEGENDARY: [1]-[3]) */
};

struct ae_campaign
{
	int have_profile;
	char profile[48];
	struct ae_campaign_level levels[AE_CAMPAIGN_LEVELS];
	short saved_level, saved_difficulty;      /* -1: no saved game */
	short difficulty;                         /* the last one played */
};

/* a level's name as the PC menus' map list says it, on one line in capitals ("" for a number out of range) */
void ae_campaign_level_name(short level, char *text, int size);
/* the level (0-9) a scenario's name or path is (its file name exactly: a10, a30...), -1 when it is not one of the campaign's ten (a
listed game's map) */
short ae_campaign_level_of_map(const char *map);
/* player 1's campaign now (their profile made player 1's, as campaign_profile does); 0 when there is no profile */
int ae_campaign_read(struct ae_campaign *campaign);
/* a level (0-9) at a difficulty (0-3) for player 1 on controller 0: the game loads it; 0 when not reached */
int ae_campaign_start(struct ae_campaign const *campaign, short level, short difficulty);
/* (0.9.4 split-screen co-op) a level for player 1 and the other local players on pads (the mask of controllers 1-3, bit c
for controller c; 0: one player, as ae_campaign_start; up to four players): a player's profile is the one the pad's card chose
(player_ui slot of that controller), players 2-4 in the order of their controllers. Returns 0 when the game loads, else an
AE_COOP_ERR_* (ae_coop_rules.h) */
int ae_campaign_start_players(struct ae_campaign const *campaign, short level, short difficulty, unsigned pads);
/* the saved game goes on; 0 when there is none */
int ae_campaign_continue(struct ae_campaign const *campaign);
/* the custom campaigns installed: each MCC package of mcc_maps with a campaign manifest, in the map catalog's order
(mcc_maps.c), its missions in the manifest's (ae_campaign_workshop_rules.h); at most maximum, how many. None with the
setting game.mcc_campaigns off. Read again each call. Each package or mission left out is one log line; the first's
sentence is copied to fault (size bytes; "" none) and *fault_count says how many (either NULL: not wanted) */
int ae_campaign_workshop_list(struct ae_ws_campaign *campaigns, int maximum, char *fault, int fault_size, int *fault_count);
/* the difficulties player 1 has finished a custom campaign's mission on, as levels' finished ([1]-[3]: NORMAL, HEROIC,
LEGENDARY; MCC's own progress file, mcc_progress.c) */
void ae_campaign_workshop_finished(const char *level, int finished[AE_CAMPAIGN_DIFFICULTIES]);
/* a custom campaign's mission (its level name, mcc_maps\<package>\<map>) at a difficulty, as ae_campaign_start_players
starts a level (pads: the same mask; 0 one player): the same start of the game (a saved game of that mission at that
difficulty in the campaign's own file goes on, as a stock level's does). The pause's SAVE AND QUIT writes the campaign's
own file (ae_saved_game_file.h). Returns 0 when the game loads, else an AE_COOP_ERR_* */
int ae_campaign_start_mission(struct ae_campaign const *campaign, const char *level, short difficulty, unsigned pads);
/* a custom campaign's saved game, in its own file (any of its missions' level names names the campaign): 1 one to go
on, its level and difficulty (0-3); 0 none (or one of another build's layout, as the stock CONTINUE offers none); -1 one
that cannot go on, why (why_size bytes; one log line): unreadable, of another campaign, or its mission's map missing or
damaged */
int ae_campaign_workshop_saved(const char *mission, char level[256], short *difficulty, char *why, int why_size);
/* that saved game goes on (upstream's campaign_continue: the package checked whole, the level started, its saved game
taken); 0 when there is none to go on */
int ae_campaign_workshop_continue(const char *mission);
/* EASY, NORMAL, HEROIC, LEGENDARY */
const char *ae_campaign_difficulty_name(short difficulty);
/* a network client's co-op game just lost its host: remember that until AE takes the engine's deferred disconnect
error; the take returns its dialog words once, then clears them */
int ae_campaign_host_left_mark(void);
int ae_campaign_host_left_take(const char **title, const char **body);

#endif
