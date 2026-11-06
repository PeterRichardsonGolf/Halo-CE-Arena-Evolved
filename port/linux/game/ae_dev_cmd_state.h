/*
AE_DEV_CMD_STATE.H

DEV MODE's command state that outlives a command (ae_dev_cmd.c): which game a reset and the rules option kept belong
to, and the settings bots and config changed with the values to put back. Pure: string only, no engine headers.

A game is a generation, counted up at each new game's map set-up (game_engine_initialize_for_new_map): a reset or an
option snapshot of another generation never finishes or restores, whatever the game clock says (a new game's first
tick is 0, as an old one's may have been).
*/

#ifndef AE_DEV_CMD_STATE_H
#define AE_DEV_CMD_STATE_H

#define AE_DEV_SETTINGS 16
#define AE_DEV_SETTING_KEY 64
#define AE_DEV_SETTING_VALUE 256

struct ae_dev_game_state
{
	unsigned long generation;
	unsigned long reset_generation;   /* the game the pending reset was asked in */
	unsigned long options_generation; /* the game the rules were kept in */
	int reset_pending, options_saved;
};

/* a new game: what was pending or kept for the last one goes */
void ae_dev_game_new(struct ae_dev_game_state *state);
/* a reset asked in this game */
void ae_dev_game_reset_begin(struct ae_dev_game_state *state);
/* 1 when a reset is pending and was asked in this game (it may finish); a stale one is dropped (0) */
int ae_dev_game_reset_current(struct ae_dev_game_state *state);
void ae_dev_game_reset_end(struct ae_dev_game_state *state);
/* the rules kept (at this game's first option): 1 when they were not kept yet (the caller keeps them now) */
int ae_dev_game_options_keep(struct ae_dev_game_state *state);
/* 1 when this game's kept rules are to be put back now (then no longer kept); 0 for none or another game's */
int ae_dev_game_options_take(struct ae_dev_game_state *state);
/* forget the kept rules */
void ae_dev_game_options_drop(struct ae_dev_game_state *state);

struct ae_dev_settings
{
	short count;
	struct
	{
		char key[AE_DEV_SETTING_KEY];
		char value[AE_DEV_SETTING_VALUE];
	} item[AE_DEV_SETTINGS];
};

/* whether a change of key is to be kept: not kept yet, room left, a key that fits, and never system.dev_mode (putting
it back would turn DEV MODE on again) */
int ae_dev_settings_wants(struct ae_dev_settings const *settings, char const *key);
/* key's value before its first change kept (when ae_dev_settings_wants); 1 when kept */
int ae_dev_settings_keep(struct ae_dev_settings *settings, char const *key, char const *value);
/* every kept setting put back through set (in the order kept), then none kept; how many */
int ae_dev_settings_back(struct ae_dev_settings *settings, int (*set)(char const *key, char const *value, void *context),
	void *context);

#endif
