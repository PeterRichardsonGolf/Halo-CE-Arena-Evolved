/*
COMMUNITY.H

The community server's online leaderboards and daily challenges
(community.c), over the platform layer's requests (port/linux/src/
community_net.c): what the menus show (menu_functions.c), the scores sent
as levels are won (campaign_score.c), and the daily challenge's settings,
which are this run's only (config_override), and its seed (randomizer.c).
*/

#ifndef __COMMUNITY_H
#define __COMMUNITY_H

/* what a request the menus wait on has come to */
enum
{
	/* community.enabled is off */
	_community_off = 0,
	_community_loading,
	_community_ready,
	/* no answer: the server is down, or this machine is offline */
	_community_unavailable,
	/* the server answered with an error (community_status_text says which) */
	_community_refused,
};

#define COMMUNITY_DAY_LENGTH 11
#define COMMUNITY_LEVEL_KEY_LENGTH 8
#define COMMUNITY_NAME_LENGTH 33
#define COMMUNITY_MAXIMUM_SKULLS 16
#define COMMUNITY_MAXIMUM_DETAILS 4
#define COMMUNITY_DETAIL_LENGTH 64
#define COMMUNITY_BOARD_ROWS 10
#define COMMUNITY_HISTORY_DAYS 30

/* a row of a leaderboard: rank 0 for none */
struct community_row
{
	long rank;
	wchar_t name[COMMUNITY_NAME_LENGTH];
	long score;
	long seconds;
	long difficulty;
	long skulls;
	long medals;
};

/* a day's challenge, as the server set it (or this machine, offline) */
struct community_challenge
{
	/* the server's (FALSE: this machine's, offline); a random run's (made
	here at random, no leaderboard's) */
	boolean online;
	boolean random;
	char day[COMMUNITY_DAY_LENGTH];
	char level[COMMUNITY_LEVEL_KEY_LENGTH];
	/* (its index in the campaign, NONE for a level this build has not) */
	short level_index;
	short difficulty;
	long version;
	unsigned long seed;
	short skull_count;
	char skulls[COMMUNITY_MAXIMUM_SKULLS][24];
	short detail_count;
	char details[COMMUNITY_MAXIMUM_DETAILS][COMMUNITY_DETAIL_LENGTH];
	/* still taking scores, and for how long (seconds, as of the answer) */
	boolean open;
	long seconds_left;
	unsigned long answered_at;
	/* this install's best of it */
	boolean has_mine;
	long mine_score;
	long mine_seconds;
	long mine_attempts;
};

/* a past day, as the history lists it */
struct community_history_day
{
	char day[COMMUNITY_DAY_LENGTH];
	/* (the server's today, or this machine's offline) */
	boolean today;
	short level_index;
	short difficulty;
	short skull_count;
	/* (its skulls, halo_skulls.h's: NONE for one this build has not) */
	short skulls[COMMUNITY_MAXIMUM_SKULLS];
	boolean open;
	long players;
	boolean has_best;
	long best_score;
	wchar_t best_name[COMMUNITY_NAME_LENGTH];
	boolean has_mine;
	long mine_score;
	/* this install's place on the day's leaderboard (offline, its profile's
	among this machine's) */
	long mine_rank;
};

/* a run of a level won, as campaign_score.c records it */
struct community_run
{
	long score;
	long seconds;
	long difficulty;
	long skulls;
	long skulls_percent;
	long medals;
	long kills;
	long deaths;
	/* the game's players (co-op: split screen's, or the network's) */
	long players;
};

/* (main.c's main loop) each frame: answers picked up, and a challenge's
settings taken away when the main menu is back after its level */
void community_update(boolean main_menu);

/* community.enabled: online (else the daily challenge and its
leaderboards are made on this machine) */
boolean community_enabled(void);
/* game.campaign_score: with it off no score is kept, sent, asked for or shown */
boolean community_scoring(void);

/* ---------- the daily challenge (the main menu's DAILY CHALLENGE) */

/* the day's challenge asked for (NULL: today's), as its screen opens */
void community_daily_open(char const *day);
short community_daily_state(void);
/* the day's challenge was asked for online, and no community server
answered (none there, the address not one, or one in trouble) */
boolean community_daily_unreachable(void);
struct community_challenge const *community_daily(void);
/* the day shown is today's */
boolean community_daily_is_today(void);
/* what the screen says of a state not ready (the server down, the
challenge ended, ...), "" for none */
wchar_t const *community_status_text(void);
/* the challenge shown, to play: its settings set for the run (the
randomizer's, the skulls', scoring on), its seed the randomizer's; its
level's map name and difficulty. FALSE when it cannot be played here */
boolean community_daily_begin(char const **map_name, short *difficulty);
/* a random run, to play: made now at random (a level, a difficulty, the
randomizer's setup and skulls, the randomizer's and the skulls' on whatever
this machine's settings and skulls unlocked), set for its run as a daily
challenge's is, its scores no leaderboard's */
boolean community_random_begin(char const **map_name, short *difficulty);
/* (the menus) the challenge's run did not start after all */
void community_daily_cancel(void);
/* (game_load) a map is loading: a challenge's settings end unless it is the menus' map or the challenge's level */
void community_map_loading(char const *map_name);
/* a daily challenge is being played (its settings are set): this
machine's, or as a network co-op host's lobby's */
boolean community_challenge_running(void);
/* (main.c) the challenge's level was won here, its game this machine's
alone: the menus come back to it (community_daily_won_take) */
void community_daily_won(void);
/* (menu_functions.c, as the main menu opens) a challenge's level was just
won here, and whether it was a random run's: once, FALSE after */
boolean community_daily_won_take(boolean *random);
/* (Arena Evolved) the day (YYYY-MM-DD) of the challenge last won here, "" for none: the menus open its screen on it */
char const *community_daily_won_day(void);
/* the level just won is a daily challenge's: this machine's, or (a network
co-op client) its host's */
boolean community_daily_run(void);
/* (network_coop.c) the host's challenge, for its clients as the level is
won: its day (days since 1970, 0 for none) and flags (its version, and
COMMUNITY_NETWORK_ONLINE for the server's); and a client told it */
#define COMMUNITY_NETWORK_ONLINE 0x100
void community_network_challenge(long *day, long *flags);
void community_network_challenge_set(long day, long flags);
/* (randomizer.c) the roll a level's randomizer makes in place of its own:
the challenge's seed, while its level is played; 0 for none */
unsigned long community_randomizer_seed(char const *scenario_name);

/* how long the challenge shown still takes scores, in seconds (0: no more) */
long community_daily_seconds_left(void);
/* a day (YYYY-MM-DD) as the menus write it: "WEDNESDAY, OCTOBER 21, 2026",
or short, "OCT 21" */
void community_day_text(char const *day, boolean short_form, wchar_t *text, long count);

/* ---------- the past challenges */

void community_history_open(void);
short community_history_state(void);
short community_history_count(void);
struct community_history_day const *community_history_day(short index);

/* ---------- the online leaderboards (a level's, or a day's challenge's) */

/* level: a campaign level's name (levels\a10\a10); day: YYYY-MM-DD */
void community_board_open_level(char const *level_name);
void community_board_open_day(char const *day);
short community_board_state(void);
/* the top rows, and this install's own (rank 0: none) */
short community_board_count(void);
struct community_row const *community_board_row(short index);
struct community_row const *community_board_mine(void);
long community_board_players(void);
/* a day's leaderboard's top three while the day lasted (kept on top of it:
it takes runs whenever they are played), online only */
#define COMMUNITY_DAY_TOP 3
short community_board_day_top_count(void);
struct community_row const *community_board_day_top(short index);

/* ---------- scores sent */

/* (campaign_score.c) a campaign level won, a player of this machine's run,
on its profile: a daily challenge's sent to the day's leaderboard (offline:
kept in the profile's records), whoever plays it; another level's to its
online leaderboard (community.upload_scores) only by one player of a game of
this machine's alone. slot: the summary card whose result it is */
#define COMMUNITY_RESULT_SLOTS 4
void community_level_won(char const *level_name, struct community_run const *run, wchar_t const *profile_name,
	long profile_index, boolean alone, short slot);
/* what became of a card's score sent ("ONLINE: #3 OF 41"), "" for none
yet; cleared as the next level is won */
wchar_t const *community_result_text(short slot);
void community_result_clear(void);

/* ---------- the main menu's news: this fork's releases on GitHub, newest
first, once they came (0 until then, and when they could not be had: the
news is not shown) */

short community_news_count(void);
/* a release's name, its day ("2026-10-27"), its build (build-<n>'s number,
NONE for a release of another tag) and its notes as the menus' text (ASCII,
lines ended by "\n"; "" for none) */
char const *community_news_name(short index);
char const *community_news_date(short index);
long community_news_build(short index);
char const *community_news_text(short index);
/* the newest build released (NONE: no release is a build-<n>) */
long community_news_latest_build(void);
/* this build's number, 0 for one made without (tools/embed_assets.py) */
long community_build_number(void);

#endif
