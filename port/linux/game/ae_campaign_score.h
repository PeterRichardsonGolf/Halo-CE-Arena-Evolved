/*
AE_CAMPAIGN_SCORE.H

The campaign's score, the engine's side (game.campaign_score, off by default: a campaign plays as it always did, with no log line,
no saved byte and no cost). The rules are ae_campaign_score_rules.h's and ae_campaign_medals_rules.h's; the values are
campaign_score_values.json's. It scores only the machine that runs the AI (a single player's, the host's of a co-op game): a
client of a network game is told nothing and scores nothing. Derived from AGPL-3.0-or-later code (README credits).

Nothing here draws: a HUD panel reads the numbers below.
*/

#ifndef __AE_CAMPAIGN_SCORE_H
#define __AE_CAMPAIGN_SCORE_H

#include <wchar.h>

#include "ae_campaign_score_rules.h"

/* game.campaign_score is on, and a campaign level is being played, on a machine that scores it */
int ae_campaign_score_active(void);

/* (interface.c's interface_initialize_for_new_map) a new level begins: the log, the time and the sprees are empty */
void ae_campaign_score_new_map(void);

/* (damage.c's object_damage_aftermath) a unit was hurt (`lethal`: its body was depleted) by a blow of `killing_player_index`'s
(-1: none), to the body part, by melee or not, going `direction` (a real_vector3d); `no_statistics`: a kill not counted. An
enemy a player kills scores by the blow's style; an ally's death, a player's death and a betrayal are penalties; the hurt is
remembered for one shot, one kill */
void ae_campaign_score_unit_damaged(long unit_index, long killing_player_index, short body_part, int melee, void const *direction,
	int lethal, int no_statistics, long damage_definition_index, long killer_object_index);

/* (game.c's game_tick) the level's time counted on: a checkpoint reverted to takes nothing from it */
void ae_campaign_score_tick(void);

/* (ae_glue_pause.c's and the classic pause menu's Revert to Checkpoint) the revert's cost, for each of this machine's players */
void ae_campaign_score_checkpoint_reverted(void);

/* (main.c's main_won_map) the level is won: Perfection, the level's totals to the log */
void ae_campaign_score_level_won(void);

/* (game_state_xbox.c, game_state.c) the saved game was written (its checksum), or taken: the score file beside it, written
or read. A saved game made with the setting off has no score file: the score starts at 0, with one log line */
void ae_campaign_score_saved_game_written(unsigned long checksum);
void ae_campaign_score_saved_game_taken(void);
/* (game_state_xbox.c) a saved game's header was read: the checksum it was written with, which its score file is bound to */
void ae_campaign_score_saved_game_header(unsigned long checksum);

/* the multiplier of the skulls on now: 1.0 until skulls exist (the one place they plug in) */
float ae_campaign_score_skulls_multiplier(void);

/* ---------- the numbers a HUD panel and an end-of-level card read (by a player's datum index) */

long ae_campaign_score_player_points(long player_index);
long ae_campaign_score_player_kills(long player_index);
long ae_campaign_score_player_deaths(long player_index);
long ae_campaign_score_team_points(void);
/* whether each player has a score of their own (network.coop_score_type "competitive"), else the team's is everyone's */
int ae_campaign_score_is_competitive(void);
/* how many of a campaign medal (ae_campaign_medals_rules.h) a player (datum index; -1: anyone's) won so far */
long ae_campaign_score_medal_count(long player_index, int medal);
/* the level's time: in ticks, and as H:MM:SS */
long ae_campaign_score_level_ticks(void);
void ae_campaign_score_time_text(wchar_t *text, long count);
/* the par time in seconds (-1: none), the time over it (< 0: none), and the time's multiplier now */
long ae_campaign_score_par_seconds(void);
float ae_campaign_score_par_fraction(void);
float ae_campaign_score_time_multiplier(void);
/* the difficulty's and the skulls' multipliers, all three together, and a multiplier as a panel writes it (x3, x0.5, x1.32) */
float ae_campaign_score_difficulty_multiplier(void);
float ae_campaign_score_total_multiplier(void);
void ae_campaign_score_multiplier_text(float multiplier, wchar_t *text, long count);
/* a player's score at the level's end: the points with the time's multiplier on them */
long ae_campaign_score_player_final(long player_index);
/* the medals a player won, the newest last: up to `count` campaign medals into `medals`, the number given */
long ae_campaign_score_recent_medals(long player_index, short *medals, long count);
/* a player's place among the players by points (1 first; 0: none yet), the difficulty's name (EASY .. LEGENDARY) */
int ae_campaign_score_player_place(long player_index);
char const *ae_campaign_score_difficulty_name(void);
/* the par time in ticks (-1: none); the level's tick at which the time multiplier next drops and its value then: 1, or 0 when it
cannot drop (no par time, or the last step) */
long ae_campaign_score_par_ticks(void);
int ae_campaign_score_next_drop(long *drop_tick, float *next_multiplier);
/* the award events, oldest first: the log's entries. kind is ae_campaign_score_rules.h's AE_CAMPAIGN_SCORE_ENTRY_*, style its bits
(AE_CAMPAIGN_SCORE_STYLE_*), variant the enemy's tag index (a campaign medal's number for a medal entry) and `name` the tag's own
name (its last path part): no designed short name exists (NOT PROVIDED: see the facts page) */
struct ae_campaign_score_event
{
	long tick;
	short player_slot;
	unsigned char kind;
	unsigned char style;
	long points;
	long variant;
	char name[40];
};
long ae_campaign_score_event_count(void);
int ae_campaign_score_event_get(long index, struct ae_campaign_score_event *event);
/* what the level's log adds up to for a player (datum index; -1: the team's): kills by style, penalties by kind, medals, points */
struct ae_campaign_score_summary;
int ae_campaign_score_summary_get(long player_index, struct ae_campaign_score_summary *summary);
/* the level's title for a card: its tag's last name (a10), the custom campaign's title where it has one */
void ae_campaign_score_level_name(char *text, long size);

/* ---------- DEV MODE (ae_dev_cmd.c's campaign_score): `status`, `on`, `off`, `reset` and `add <enemy> <style>`: one answer line
into text; 1 when it ran, 0 for a refusal */
int ae_campaign_score_dev(char const *verb, char const *enemy, char const *style, char *text, long size);

#endif
