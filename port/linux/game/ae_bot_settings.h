/*
AE_BOT_SETTINGS.H

The bot settings the AE menus show as rows (ae_bot_settings.c): BOTS (bots.count: OFF, 1-31, then 47, 63, 95 and 127
in the menus, any count up to 127 typed, past 31 with a warning), BOT SKILL (bots.skill), and LOBBY SETUP's four (bots.shooting_skill, bots.movement_skill,
bots.playfight, bots.teams). Each is written to config.toml at once as it is stepped, and the bots follow it at once,
in the lobby and in game (ae_bots_live.c). CREATE GAME's lobby panel and LOBBY SETUP show them, and so does the pause
menu's BOTS (0.4.0): one wording, one place.
*/

#ifndef __AE_BOT_SETTINGS_H
#define __AE_BOT_SETTINGS_H

#include <stddef.h>

enum
{
	AE_BOT_COUNT, AE_BOT_SKILL, AE_BOT_SHOOTING, AE_BOT_MOVEMENT, AE_BOT_PLAYFIGHT, AE_BOT_TEAMS,
	AE_BOT_SETTINGS
};

/* bots.count as the menus take it (0 to 127) */
long ae_bot_count(void);
/* the row's label ("BOTS", "BOT SKILL", "SHOOTING SKILL", "MOVEMENT SKILL", "PLAYFIGHT", "BOT TEAMS") */
const char *ae_bot_setting_label(int kind);
/* its value now ("OFF", "7"; "NORMAL"; "AS BOT SKILL"; "ON"; "EVEN") */
const char *ae_bot_setting_text(int kind);
/* its help line (PLAYFIGHT's and BOT TEAMS' say what the value now does; BOTS past 31 warns) */
const char *ae_bot_setting_help(int kind);
/* whether its value now is one the menus warn about (BOTS past 31): its help drawn in the warning colour */
int ae_bot_setting_warns(int kind);
/* steps it (wrapping, as menu rows do: BOTS through OFF, 1-31, 47, 63, 95, 127) and writes it; the cursor sound, or the
failure's when nothing changed; logs "<log_prefix>: bots 7", "...: bot skill NORMAL", "...: shooting skill EASY" */
int ae_bot_setting_step(int kind, short direction, const char *log_prefix);
/* a BOTS count typed into its menu row: digits only (three at most), clamped to 127 */
int ae_bot_count_typing_ok(const char *typed);
int ae_bot_count_typed(const char *typed, char *value, size_t size);
/* writes a typed BOTS count; the cursor sound and log are the step's */
int ae_bot_setting_typed(const char *typed, const char *log_prefix);

#endif
