/*
AE_CAMPAIGN_ROWS_RULES.H

The CAMPAIGN screen's SKIP TRAINING row, its CONTINUE row with an AI squad, the dialog of a saved game the squad cannot
join, and the party count: which row is live or dimmed and why, the words, and what a dialog choice does. Pure (no engine
includes; port/linux/tests/ae_campaign_rows_rules_test.c). ae_screen_campaign.c draws and acts on the answers.

Difficulties are the game's order: 0 EASY, 1 NORMAL, 2 HEROIC, 3 LEGENDARY.
*/

#ifndef AE_CAMPAIGN_ROWS_RULES_H
#define AE_CAMPAIGN_ROWS_RULES_H

/* why SKIP TRAINING is dimmed (the first cause that applies); NONE: live */
enum
{
	AE_CAMPAIGN_ROWS_SKIP_LIVE = 0,
	AE_CAMPAIGN_ROWS_SKIP_SPLIT_SCREEN,
	AE_CAMPAIGN_ROWS_SKIP_DIFFICULTY
};

/* SKIP TRAINING acts only with one human player on EASY or NORMAL (the script is told EASY as NORMAL): the level's script takes its short start when game_is_cooperative
(more than one player spawned, a second human) or the difficulty is HEROIC or LEGENDARY. An AI squad joins after the script has chosen,
so it does not count */
int ae_campaign_rows_skip_cause(int players, int difficulty);
/* the dimmed row's word, and the row's help for each cause or, when live, for each value (on: 1) */
const char *ae_campaign_rows_skip_reason(void);
void ae_campaign_rows_skip_help(int cause, int difficulty, int on, char *text, int size);

/* PLAYER DOWN: what happens when a player goes down, the setting's value (game.companions_player_down: "respawn", anything else
checkpoint), the value's word and the row's help (the squad on: the choice; off: the row is dimmed, NEEDS AI SQUAD) */
int ae_campaign_rows_down_is_respawn(char const *setting);
char const *ae_campaign_rows_down_value(int respawn);
char const *ae_campaign_rows_down_next(int respawn);
char const *ae_campaign_rows_down_help(int respawn, int squad_on);
char const *ae_campaign_rows_down_reason(void);
/* DOOR STACK (game.companions_door_stack): "every" stacks the squad at every closed door the player waits at, anything else
only at a door the player marks; the row's value, the setting's text after one step, its help (squad_on: an AI squad asked for) */
int ae_campaign_rows_door_is_every(char const *setting);
char const *ae_campaign_rows_door_value(int every);
char const *ae_campaign_rows_door_next(int every);
char const *ae_campaign_rows_door_help(int every, int squad_on);
/* FORMATIONS (game.companions_formations): the squad forms up by itself (ON) or makes no formation of its own (OFF); the row's
value and its help (squad_on: an AI squad asked for) */
char const *ae_campaign_rows_formations_value(int on);
char const *ae_campaign_rows_formations_help(int on, int squad_on);

/* the case of CONTINUE with this screen's seats and squad */
enum
{
	AE_CAMPAIGN_ROWS_CONTINUE_PLAIN = 0,   /* no squad asked for: the saved game as it always went on */
	AE_CAMPAIGN_ROWS_CONTINUE_SQUAD,       /* the saved game takes the squad */
	AE_CAMPAIGN_ROWS_CONTINUE_SOLO_ONLY,   /* the squad cannot join the saved game: A opens the dialog */
	AE_CAMPAIGN_ROWS_CONTINUE_SPLIT        /* split screen: solo only, as it always was (the row's reason) */
};
int ae_campaign_rows_continue_case(int squad_count, int players, int save_takes_squad);
/* the row's value: base ("LEVEL 3 \xE2\x80\xA2 HEROIC") with the case's tail; the row's help */
void ae_campaign_rows_continue_value(char const *base, int continue_case, int squad_count, char *text, int size);
void ae_campaign_rows_continue_help(int continue_case, int squad_count, int workshop, char *text, int size);
/* whether the SOLO ONLY dialog is on in this build: one rule, off (the squad joins a new mission, not a saved game) */
int ae_campaign_rows_dialog_enabled(void);
/* whether a saved game takes the AI squad on CONTINUE: not yet (AE_CAMPAIGN_SAVE_TAKES_SQUAD 0, CONTINUE says SOLO ONLY); and
whether a CONTINUE with the squad asked for hosts it (only where the save takes it): else the saved game is played alone */
#define AE_CAMPAIGN_SAVE_TAKES_SQUAD 0
int ae_campaign_rows_continue_hosts_squad(int squad_asked, int save_takes_squad);
/* A on CONTINUE opens the dialog: only with the dialog on, and for the SOLO ONLY case */
int ae_campaign_rows_continue_asks(int continue_case);
/* the SOLO ONLY help of the build with the dialog on (units keep its words) */
void ae_campaign_rows_continue_help_dialog(int squad_count, char *text, int size);

/* the starts that bring no AI squad: a Workshop campaign (a plain local game) and split screen (the pads' local co-op game); the
squad joins only a hosted game of the stock campaign started with one player. AI SQUAD, SQUAD GEAR, PLAYER DOWN and DOOR STACK
are dimmed there */
enum { AE_CAMPAIGN_ROWS_SQUAD_OK = 0, AE_CAMPAIGN_ROWS_SQUAD_WORKSHOP, AE_CAMPAIGN_ROWS_SQUAD_SPLIT };
int ae_campaign_rows_squad_cause(int workshop, int players);
char const *ae_campaign_rows_squad_reason(int cause);
char const *ae_campaign_rows_squad_help(int cause);
/* AI SQUAD's help where the squad joins: only a new mission started alone */
char const *ae_campaign_rows_squad_ai_help(void);
/* the squad MY PARTY counts: none while a CONTINUE that plays alone is focused (the squad count otherwise) */
int ae_campaign_rows_party_squad(int squad_count, int continue_focused, int continue_case);
char const *ae_campaign_rows_workshop_reason(void);
char const *ae_campaign_rows_workshop_help(void);

/* the dialog: its title, its body for the saved level's name and difficulty, its two choices */
char const *ae_campaign_rows_dialog_title(void);
void ae_campaign_rows_dialog_body(char const *level_name, int difficulty, char *text, int size);
enum { AE_CAMPAIGN_ROWS_CHOICES = 2 };
char const *ae_campaign_rows_dialog_choice(int choice);
enum { AE_CAMPAIGN_ROWS_ACT_NONE = 0, AE_CAMPAIGN_ROWS_ACT_CONTINUE_ALONE, AE_CAMPAIGN_ROWS_ACT_START_WITH_SQUAD };
/* what a picked choice does (-1: B / Esc, nothing) */
int ae_campaign_rows_dialog_action(int choice);

/* MY PARTY's count: "1/4" with no squad, "1 + 3 AI" with one (players on this PC besides the squad) */
void ae_campaign_rows_party_count(int players, int players_max, int squad_count, char *text, int size);

#endif
