/*
AE_PVE_MENUS_RULES.H

The words and rules behind the campaign's rule screens (ae_screen_skulls.c, ae_screen_skull_rules.c, ae_screen_scoring.c,
ae_screen_coop_options.c, ae_screen_saved_games.c, ae_screen_leaderboard.c) and the end-of-mission card
(ae_screen_end_card.c). Pure: no engine includes (unit tests: port/linux/tests/ae_pve_menus_rules_test.c).

Every setting a screen here writes goes through ae_pve_menus_key_allowed: the skulls' own keys, the campaign score's, the
co-op options'. A multiplayer game's settings (game.multiplayer_*, network.vehicle_riding, the score HUD's colour) are
never among them, so no screen of this family can change a multiplayer game.
*/

#ifndef __AE_PVE_MENUS_RULES_H
#define __AE_PVE_MENUS_RULES_H

#include <stddef.h>

/* ---------- the settings these screens may write */

/* 1 when key is one of the settings the campaign rule screens write (and read back) */
int ae_pve_menus_key_allowed(const char *key);

/* ---------- skulls: how they are had (game.skulls_unlock_mode, game.skulls_all_unlocked, game.skulls_instant_unlock) */

enum { AE_PVEM_FOUND_ALL, AE_PVEM_FOUND_LEVELS, AE_PVEM_FOUND_PICKUP, AE_PVEM_FOUND_COUNT };

/* the way the keys say: all_unlocked wins, then instant_unlock (a skull picked up is had at once) */
int ae_pve_menus_found_from_keys(int all_unlocked, int instant_unlock);
/* the keys' values for a way: mode is always "play"; all and instant are "true" / "false" */
void ae_pve_menus_found_to_keys(int found, const char **mode, const char **all_unlocked, const char **instant_unlock);
const char *ae_pve_menus_found_name(int found);
const char *ae_pve_menus_found_help(int found);
/* LOSE ON DEATH's help for its value */
const char *ae_pve_menus_lose_help(int lose);

/* SKULLS PER LEVEL: 0 is by difficulty, else 1 to 8 (the value's words) */
void ae_pve_menus_count_text(long count, char *text, size_t size);
/* DROP CHANCE: the percent (25, 50, 100, 200, 400) and its words; an unknown percent shows as the nearest */
enum { AE_PVEM_CHANCES = 5 };
long ae_pve_menus_chance_percent(int index);
const char *ae_pve_menus_chance_name(int index);
int ae_pve_menus_chance_index(long percent);

/* FEWEST and MOST SKULLS (0 to 40): the one moved pushes the other, so MOST is never below FEWEST. moved: 0 FEWEST, 1 MOST */
long ae_pve_menus_random_limit(long value);
void ae_pve_menus_random_clamp(long *fewest, long *most, int moved);
/* how many skulls the roll turns on: between fewest and most, neither more than the skulls had, from r (any number) */
long ae_pve_menus_random_pick(long fewest, long most, long had, unsigned long r);

/* the lobby's SKULLS value: OFF, or "n ON" */
void ae_pve_menus_skulls_value(int on_count, char *text, size_t size);
/* the lobby's RULES fact: NONE, or "n SKULLS" and "SCORING ON" joined with a separator dot (U+2022) */
void ae_pve_menus_rules_fact(int skulls_on, int scoring_on, char *text, size_t size);
/* a skull's help line: "<NAME>: <description>." then the A word (or the not-found line) */
void ae_pve_menus_skull_help(const char *name, const char *description, int on, int found, char *text, size_t size);

/* ---------- co-op options (network.coop_*) */

struct ae_pve_menus_option
{
	const char *key;
	const char *standard;       /* the setting's own default, as the config file writes it */
};
enum { AE_PVEM_COOP_OPTIONS = 9 };
struct ae_pve_menus_option const *ae_pve_menus_coop_option(int index);
/* 1 when every option reads its default; get(context, key) gives a setting's text ("true", "50", "per_player") */
int ae_pve_menus_coop_is_standard(const char *(*get)(void *context, const char *key), void *context);

/* a row's values as words (the config's) and as shown; index of a config value among them, -1 when none */
enum { AE_PVEM_COOP_FRIENDLY, AE_PVEM_COOP_WIPE, AE_PVEM_COOP_MODE, AE_PVEM_COOP_GROWTH, AE_PVEM_COOP_FIXED, AE_PVEM_COOP_KINDS };
int ae_pve_menus_coop_value_count(int kind);
const char *ae_pve_menus_coop_value_word(int kind, int index);
const char *ae_pve_menus_coop_value_label(int kind, int index);
int ae_pve_menus_coop_value_index(int kind, const char *word);
/* the live fact for a number of players: "1.5x" style factor text of the squads (mode word, growth percent, fixed times);
the factor itself in hundredths */
long ae_pve_menus_coop_factor(const char *mode, long growth_percent, long fixed_times, int players);
void ae_pve_menus_coop_factor_text(long hundredths, char *text, size_t size);
/* the help of EXTRA ENEMIES for a mode */
const char *ae_pve_menus_coop_mode_help(const char *mode);
const char *ae_pve_menus_coop_wipe_help(const char *word);

/* ---------- scoring */

enum { AE_PVEM_SCORING_ROW_ON, AE_PVEM_SCORING_ROW_TEAM, AE_PVEM_SCORING_ROW_MEDALS, AE_PVEM_SCORING_ROW_REVERT };
/* the reason a row waits ("NEEDS SCORING"), NULL when it is live; the first row is always live */
const char *ae_pve_menus_scoring_reason(int row, int scoring_on);

/* ---------- the leaderboard */

struct ae_pve_menus_run { long score, seconds, difficulty, skulls, medals; int profile; };
/* the runs best first: by score (the shorter time first among equals), or by time (a run with no time last) */
void ae_pve_menus_runs_sort(struct ae_pve_menus_run *runs, int count, int by_time);
/* "m:ss" or "h:mm:ss"; "-" for no time */
void ae_pve_menus_time_text(long seconds, char *text, size_t size);
/* a number with thousands commas */
void ae_pve_menus_number_text(long value, char *text, size_t size);

/* ---------- saved games */

/* a calendar day (year, month 0-11, day of the month) as a number of days since 1970-01-01, for telling today from yesterday */
long ae_pve_menus_day(int year, int month, int month_day);
/* when a saved game was kept: "TODAY 21:14", "YESTERDAY", else "12 NOV 2026" (day numbers are days since any fixed day) */
void ae_pve_menus_when_text(long day, long today, int hour, int minute, int month, int year, int month_day, char *text, size_t size);

/* ---------- the end card's presses */

/* what A does on the shown card: the next player's card, the next mission, or the finish */
enum { AE_PVEM_CARD_NEXT_PLAYER, AE_PVEM_CARD_NEXT_MISSION, AE_PVEM_CARD_FINISH };
struct ae_pve_menus_press { int primed, held; };
/* one frame: down is whether A or B is held. 1 once on a new press (a button held when the card appeared is not one) */
int ae_pve_menus_press_step(struct ae_pve_menus_press *press, int down);
/* the action of A on the card index (0 first) of count, in the last mission or not */
int ae_pve_menus_card_a(int index, int count, int last_mission);
const char *ae_pve_menus_card_a_word(int action);
/* the title's sub-line: "<MISSION> * <DIFFICULTY> * n SKULLS" with the separator dot */
void ae_pve_menus_card_sub(const char *mission, const char *difficulty, int skulls, char *text, size_t size);

#endif
