/*
CAMPAIGN_SCORE.H

The campaign's score (game.campaign_score: campaign_score.c): each enemy a
player kills is worth points by its actor variant.
*/

#ifndef __CAMPAIGN_SCORE_H
#define __CAMPAIGN_SCORE_H

/* game.campaign_score, and a campaign level being played */
boolean campaign_score_enabled(void);

/* (interface.c's hud_initialize_for_new_map) a map loads: a new level's
score begins; a level just won has its summary shown over this one's start */
void campaign_score_new_map(void);

/* (damage.c's object_damage_aftermath) a unit was hurt (`lethal`: its body
was depleted) by a blow of `killing_player_index`'s (NONE for no player's),
to the body part (units.c's), by melee or not, going `direction` (a
real_vector3d): an enemy's kill scores, by its style; an ally's, a player's
death and another player's are penalties; the hurt is remembered for one
shot, one kill. `no_statistics`: a kill not counted. The blow's damage
effect and the object it is of (its unit: an AI's too) tell the kill's
medals (campaign_medals.c) */
void campaign_score_unit_damaged(
	long unit_index,
	long killing_player_index,
	short body_part,
	boolean melee,
	void const *direction,
	boolean lethal,
	boolean no_statistics,
	long damage_definition_index,
	long killer_object_index);

/* the multiplayer score (game.multiplayer_scoring, a game engine's game):
its medals (game.multiplayer_medals) and its HUD panel
(game.multiplayer_score_hud), on each machine for its own players only */
boolean campaign_score_multiplayer_medals(void);
boolean campaign_score_multiplayer_hud(void);

/* (damage.c's object_damage_aftermath) a unit's killing blow, before the
kill is announced (game_engine_player_killed): to the body part, by melee or
not, going `direction` (a real_vector3d), of the damage effect */
void campaign_score_killing_blow(
	long unit_index,
	short body_part,
	boolean melee,
	void const *direction,
	long damage_definition_index);

/* (game_engine.c's game_engine_player_killed, on every machine: a client's
with the host's killer) a player killed in a multiplayer game: the medals
of the kill, with its killing blow's style, for this machine's players */
void campaign_score_player_killed(
	long killing_player_index,
	long killing_object_index,
	long dead_player_index,
	boolean friendly_fire);

/* (campaign_medals.c) a medal won by the player (its datum index), worth
its points: logged, and sent to a co-op game's clients */
void campaign_score_award_medal(
	long player_index,
	short medal);

/* (firefight_objectives.c) points a player (its datum index) earned with a
Firefight objective, logged as they are */
void campaign_score_award_points(
	long player_index,
	long points);

/* (ui_widget_event_handler_functions.c's pause_game_restart_at_checkpoint)
Revert to Checkpoint was chosen: its penalty, for this machine's players */
void campaign_score_checkpoint_reverted(void);

/* (main.c's main_won_map) the level is won: its summary is written to the
log and kept for the next level's start */
void campaign_score_level_won(void);

/* (main.c's main_won_map_private) a level was won: TRUE while the level's
summary is shown over it, until A or B is pressed; the level is left only
when FALSE */
boolean campaign_score_postgame_wait(void);

/* (interface.c's interface_draw_fullscreen_overlays) the last level's summary */
void campaign_score_draw(void);
/* Firefight's report at the end of a match, in the postgame's 640 by 480: its
players by absolute datum index, best first */
void campaign_score_firefight_report_draw(long const *players, short player_count);

/* (hud.c's hud_draw_screen) the score and the level's time, a panel at the
bottom right of the view being drawn, before the skulls' icons; in a
multiplayer game the player's kills and deaths, and the game type's score */
void campaign_score_draw_view(long player_index);

/* (skulls.c's skulls_hud_draw) how far, in the HUD's 480 lines, the panel
the view drawn last took above the corner's margin: 0 for none */
short campaign_score_hud_height(void);

/* (game_engine.c's scoreboard) a player's points and kills (by datum
index), the team's points, whether co-op scores each player on their own,
and the level's time (in ticks, and H:MM:SS) */
long campaign_score_player_points(long player_index);
long campaign_score_player_kills(long player_index);
long campaign_score_player_deaths(long player_index);
long campaign_score_team_points(void);
boolean campaign_score_is_competitive(void);
/* (social.c) how many of a medal (campaign_medals.h's) a player (absolute
index; NONE: anyone's) won in this level or game: its log's, as reverted
checkpoints left it; a multiplayer game's, this machine's players' alone
with game.multiplayer_medals */
long campaign_score_medal_count(short player_index, short medal);
long campaign_score_level_ticks(void);
/* (game.c's game_tick) the level's time counted on, each tick: a checkpoint
reverted to takes nothing from it */
void campaign_score_tick(void);
/* the level's time's multiplier now (by its par time: x1 for a level with
none), and its time over its par time (less than 0: none) */
real campaign_score_time_multiplier(void);
real campaign_score_par_fraction(void);
/* the score's multipliers now: the difficulty's, the skulls' (those on),
and the difficulty's, the skulls' and the time's together; a multiplier as
the HUD has it (x3, x0.5, x1.32) */
real campaign_score_difficulty_multiplier_for(short difficulty);
real campaign_score_difficulty_multiplier(void);
real campaign_score_skulls_multiplier(void);
real campaign_score_total_multiplier(void);
void campaign_score_multiplier_text(real multiplier, wchar_t *text, long count);
void campaign_score_time_text(wchar_t *text, long count);

/* a player's name (by datum index): their own, else, for one of this
machine's campaign, their profile's (a single player's has none) */
void campaign_score_player_display_name(long player_index, wchar_t *name, long count);
/* a level's records in a profile's, each its own best, whichever run's:
the most points, the shortest time (in seconds), the most skulls on, the
highest skulls' multiplier (in hundredths) and the most medals; and the
best score's difficulty (0 easy to 3 legendary); NONE for those a record
older than the leaderboard has not */
struct campaign_score_record
{
	long score;
	long seconds;
	long difficulty;
	long skulls;
	long skulls_percent;
	long medals;
};

/* (menu_functions.c's New Game and leaderboard) a level's records (by its
name, levels\a10\a10) in the profile's (its index; its
campaign_records.txt); FALSE for none */
boolean campaign_score_level_record(char const *level_name, long profile_index, struct campaign_score_record *record);
/* the level's title (The Pillar of Autumn), else its map's name */
void campaign_score_level_title(wchar_t *text, long count);

/* (network_coop.c) network co-op: the host scores the kills and penalties,
and sends each to the clients (its kind and style too), and when the level
is won */
void campaign_score_network_kill(short player_index, long variant_index, long points, short kind, byte style,
	boolean competitive);
void campaign_score_network_won(boolean competitive);

/* ---------- the panels (the HUD's, the level's summary card; social_menu.c's
player card is drawn as the summary's is)

a panel is laid out in units of the screen's 480 lines from its top left,
and drawn scaled (its scale: the view's pixels to a unit) */

/* draw_string.c's justifications (its enum is its own) */
enum
{
	_campaign_score_text_left = 0,
	_campaign_score_text_right = 1,
	_campaign_score_text_center = 2,
};

/* the summary's colours (ARGB, the Master Chief Collection's blue): a
card's fill, its rules and its edge; and the subline's text, smaller than
the row's */
#define CAMPAIGN_SCORE_CARD_FILL 0xF0081830
#define CAMPAIGN_SCORE_CARD_RULE 0x804F9BE0
#define CAMPAIGN_SCORE_PANEL_EDGE 0xE04F9BE0
#define CAMPAIGN_SCORE_SUBLINE_SIZE 0.72f

union real_argb_color;

struct campaign_score_panel
{
	real scale;
	/* (the panel's top left, in the view's pixels) */
	real left;
	real top;
	/* (how wide it is, in its own units: CAMPAIGN_SCORE_PANEL_WIDTH, or a
	multiplayer row's wider to fit its text) */
	real width;
	long font_index;
	short line_height;
};

/* a rectangle of the panel, in its own units from its top left; a
rectangle's edge, a unit wide */
void campaign_score_panel_quad(struct campaign_score_panel const *panel, real x0, real y0, real x1, real y1,
	unsigned long color);
void campaign_score_panel_frame(struct campaign_score_panel const *panel, real x0, real y0, real x1, real y1,
	unsigned long color);

/* a line of text (text, or unicode_text) from x0 to x1 of the panel, its top
at y, `size` times the panel's font (scaled about its own top left:
rasterizer_text_set_scale), justified (_campaign_score_text_*) */
void campaign_score_panel_text(struct campaign_score_panel const *panel, char const *text,
	wchar_t const *unicode_text, real x0, real x1, real y, real size, short justification,
	union real_argb_color const *color);

#endif
