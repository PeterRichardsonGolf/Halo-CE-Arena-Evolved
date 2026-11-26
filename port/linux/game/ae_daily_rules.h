/*
AE_DAILY_RULES.H

The words and rules behind the DAILY CHALLENGE screens (ae_screen_daily.c): how a day is named (TODAY, YESTERDAY, 11-23), the
countdown to the next challenge, the crumb that says where the day and its board come from, the PLAY row's words, and the
settings the screens' rows write. Pure: no engine includes (unit tests: port/linux/tests/ae_daily_rules_test.c).

Where the day comes from is the player's choice and starts off: this PC makes the day and keeps the board (Content Evolved's
offline mode, nothing leaves the PC), or, with COMMUNITY SERVICES on and ONLINE DAY AND BOARDS on, the community server sets it.
*/

#ifndef __AE_DAILY_RULES_H
#define __AE_DAILY_RULES_H

#include <stddef.h>

enum { AE_DAILY_HISTORY_DAYS = 30 };

/* a day (YYYY-MM-DD) as the rows name it, against today (YYYY-MM-DD): TODAY, YESTERDAY, else MM-DD; "" for a day that is not one */
void ae_daily_day_label(const char *day, const char *today, char *text, size_t size);
/* the day before or after a day (YYYY-MM-DD), as text; 0 and nothing written for a day that is not one */
int ae_daily_day_shift(const char *day, int days, char *out, size_t size);
/* how many days from a day to today (0 today, 1 yesterday); -1 when either is not a day or the day is after today */
int ae_daily_days_ago(const char *day, const char *today);

/* the seconds a day still takes scores: "A NEW CHALLENGE IN 9 H 12 MIN"; with none left, "THIS DAY IS CLOSED" */
void ae_daily_countdown_text(long seconds, int today, char *text, size_t size);

/* the crumb: where the day and its board come from. online: the community server's; unreachable: it was asked and did not answer */
const char *ae_daily_crumb(int online, int unreachable);
/* the right side's line under an offline board (the day and its board are this PC's) */
const char *ae_daily_offline_note(void);
/* the board's heading: TODAY'S BOARD (a day's: ITS BOARD) with where it comes from */
void ae_daily_board_head(int today, int online, char *text, size_t size);

/* the primary row: PLAY TODAY'S CHALLENGE, PLAY YESTERDAY'S CHALLENGE, PLAY 11-23'S CHALLENGE (label from ae_daily_day_label) */
void ae_daily_play_text(const char *label, char *text, size_t size);
/* the randomizer's part of a challenge as one line, from the challenge's detail lines (ENEMIES: ..., ENEMY WEAPONS: ..., PICKUPS: ...,
GO CRAZY: ON): "ENEMIES \xE2\x80\xA2 PICKUPS"; a line that says NOT RANDOMIZED is left out; "OFF" when no line is given */
void ae_daily_randomizer_summary(char const (*details)[64], int count, char *text, size_t size);

/* DAY's row help: what left and right do */
const char *ae_daily_day_help(void);
/* a difficulty's words (0 to 3: EASY, NORMAL, HEROIC, LEGENDARY); "" outside */
const char *ae_daily_difficulty_name(int difficulty);

/* ---------- the settings (SETTINGS > CAMPAIGN > DAILY CHALLENGE) */

/* whether the online day and boards would be used with these settings: community.enabled on and network.community_services "yes" */
int ae_daily_online_on(int enabled, const char *services);
/* the words on the ONLINE rows: what is sent when they are on (the player's consent) */
const char *ae_daily_online_help(void);
const char *ae_daily_share_help(void);
/* SERVER ADDRESS: a typed address as the community server's setting keeps it. Spaces around it are dropped, an address with no scheme
is taken as https://, a trailing slash goes; only https is taken (another scheme, a space inside, a missing host: 0, out empty) */
enum { AE_DAILY_SERVER_MAXIMUM = 100 };
int ae_daily_server_clean(const char *typed, char *out, size_t size);
/* the SERVER row's help and the SERVER ADDRESS row's value ("NOT SET" for a custom server with no address, "USES DEFAULT" with the default one) */
const char *ae_daily_server_help(void);
const char *ae_daily_server_address_help(void);
void ae_daily_server_address_shown(const char *mode, const char *address, char *text, size_t size);
/* 1 for the settings keys these screens' rows write (nothing a multiplayer game reads) */
int ae_daily_key_allowed(const char *key);
/* 1 for the exact names a community challenge may set for its run (the randomizer's and the skulls' of the offline challenge); 0 for anything
else, the skull progress settings among them */
int ae_daily_challenge_key_allowed(const char *key);
/* 1 for text that is a day as the server names it (YYYY-MM-DD, digits, a real month and date range); anything else, and none, 0 */
int ae_daily_day_valid(const char *day);
/* 1 when a challenge's settings stay as this map loads: the menus' own map (ui) and the challenge's level; 0 for any other map, and for none */
int ae_daily_map_keeps_settings(const char *map_key, const char *challenge_key);

/* ---------- the window of a server not reached (ONLINE on, the community server did not answer) */

/* the choices, in order: TRY THE DEFAULT SERVER (only with a custom server set), CHANGE THE ADDRESS, PLAY OFFLINE, TRY AGAIN (the same server), BACK */
enum { AE_DAILY_UNREACHED_DEFAULT, AE_DAILY_UNREACHED_ADDRESS, AE_DAILY_UNREACHED_OFFLINE, AE_DAILY_UNREACHED_RETRY, AE_DAILY_UNREACHED_BACK, AE_DAILY_UNREACHED_CHOICES };
/* whether the window is asked at all: only with ONLINE on, a day that was asked for and not reached, and not asked already for this ask */
int ae_daily_unreached_wanted(int online, int unreachable, int asked);
/* the choices as ids and words (count returned, at most AE_DAILY_UNREACHED_CHOICES); custom: a custom server is in use */
int ae_daily_unreached_choices(int custom, int *ids, const char **words);
/* why the server was not reached, from the community code's status line: no answer, refused, bad answer */
const char *ae_daily_unreached_reason(const char *status);
/* the window's body: the server tried, the reason, and what each choice does */
void ae_daily_unreached_body(const char *server, const char *status, int custom, char *text, size_t size);
/* the PLAY row says RETRY (and asks the server again) when the day was asked for online and not given */
int ae_daily_retry_shown(int online, int state);
/* what a choice writes: the setting and its value, or NULL for none (CHANGE THE ADDRESS and BACK write nothing here) */
int ae_daily_unreached_write(int id, const char **key, const char **value);

#endif
