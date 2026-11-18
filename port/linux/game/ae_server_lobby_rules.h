#ifndef AE_SERVER_LOBBY_RULES_H
#define AE_SERVER_LOBBY_RULES_H

/*
AE_SERVER_LOBBY_RULES.H

The pure rules of a dedicated server's lobby page (0.7.7; the approved mockups-077-server/server-lobby option A): the
server's public "lobby info" message on the wire (distributed message 115: from a dedicated server to every machine,
reliably, while its lobby is up; a build that does not know it drops it), and the words and choices the page draws from
it. No game, socket or file dependency: tools/test_ae_units.py compiles ae_server_lobby_rules_test.c on its own.

What the message holds that a joined machine does not know otherwise: the server's region, who can join it (its join
word), its message of the day, the people it waits for (min_players), the bots it seats on each team, the games the
lobby plays (the one it is set up for, the next ones everyone present can play, and the map vote's choices: each with
its gametype's name and a line of its rules), the team rule of its lobby, and which chat commands it takes.
*/

enum
{
	AE_SL_WIRE_VERSION = 1,
	AE_SL_REGION = 17,                    /* server.region: 16 characters and the end */
	AE_SL_MOTD = 128,                     /* server.motd: 127 characters and the end */
	AE_SL_MAP = 64,                       /* a rotation's map: "bloodgulch", "pat_prisoner@pro", "<name>@ws<item>" */
	AE_SL_GAMETYPE = 24,                  /* its gametype as the rotation (and the vote) writes it: "ae_br_ts" */
	AE_SL_NAME = 24,                      /* the gametype's name: "AE BR TS" */
	AE_SL_FACTS = 72,                     /* "4 V 4 \xE2\x80\xA2 BR START \xE2\x80\xA2 50 KILLS \xE2\x80\xA2 15 MIN" */
	AE_SL_ENTRIES = 12,                   /* the lobby's game, three to come, eight vote choices */
	AE_SL_NEXT = 3,                       /* the games to come the page lists ("AFTER THAT") */
	AE_SL_HEAD = 1 + 10 + AE_SL_REGION + AE_SL_MOTD,
	AE_SL_ENTRY = 1 + AE_SL_MAP + AE_SL_GAMETYPE + AE_SL_NAME + AE_SL_FACTS,
	AE_SL_WIRE_MAXIMUM = AE_SL_HEAD + AE_SL_ENTRIES * AE_SL_ENTRY,

	/* the flags: the chat commands the server takes now, and its map vote */
	AE_SL_FLAG_NOMINATE = 1, AE_SL_FLAG_RTV = 2, AE_SL_FLAG_EXTEND = 4, AE_SL_FLAG_MAP_VOTE = 8,
	AE_SL_FLAGS = 15,

	/* the lobby's team rule (network.team_change on the server): off (the server puts players on teams itself),
	balanced (a change that keeps the people on the teams within the margin), allow */
	AE_SL_TEAMS_LOCKED = 0, AE_SL_TEAMS_BALANCED = 1, AE_SL_TEAMS_OPEN = 2,

	/* who can join (the browser's words): any Halo CE PC build, Arena Evolved, Arena Evolved with MCC's files */
	AE_SL_JOIN_ANYONE = 0, AE_SL_JOIN_AE = 1, AE_SL_JOIN_AE_MCC = 2,

	/* an entry's roles (more than one at once: the lobby's game is a vote choice too, SAME AGAIN) */
	AE_SL_ENTRY_NOW = 1, AE_SL_ENTRY_NEXT = 2, AE_SL_ENTRY_VOTE = 4, AE_SL_ENTRY_ROLES = 7,
};

struct ae_sl_entry
{
	unsigned char roles;                    /* AE_SL_ENTRY_* */
	char map[AE_SL_MAP];
	char gametype[AE_SL_GAMETYPE];
	char name[AE_SL_NAME];
	char facts[AE_SL_FACTS];
};

struct ae_sl_wire
{
	unsigned char flags;                    /* AE_SL_FLAG_* */
	unsigned char team_rule;                /* AE_SL_TEAMS_* */
	unsigned char join;                     /* AE_SL_JOIN_* */
	unsigned char min_players, max_players;
	unsigned char bots_red, bots_blue;      /* a team game's bots on each team */
	unsigned char bots;                     /* every bot seated (a free for all's) */
	unsigned char vote_seconds;             /* how long a map vote runs (server.map_vote_seconds) */
	unsigned char count;                    /* the entries sent */
	char region[AE_SL_REGION];
	char motd[AE_SL_MOTD];
	struct ae_sl_entry entries[AE_SL_ENTRIES];
};

/* the bytes to send: their count (0 if the state is not one to send) */
int ae_sl_wire_encode(const struct ae_sl_wire *state, unsigned char *bytes, int size);
/* a body read: 1 if it is whole and every field in range (texts printable and ended) */
int ae_sl_wire_decode(const unsigned char *bytes, int size, struct ae_sl_wire *state);
/* text kept printable ASCII (anything else a space), cut to size with its end */
void ae_sl_printable(const char *text, char *kept, int size);

/* ---------- the entries */

/* the entry with a role, from the first (-1 for none); the n'th of a role (NEXT's order is the play order) */
int ae_sl_entry_with(const struct ae_sl_wire *state, int role, int nth);
/* the entry of a map and gametype (a vote choice's), -1 for none */
int ae_sl_entry_of(const struct ae_sl_wire *state, const char *map, const char *gametype);
/* a rotation's gametype as the page names it when the server gave none: the rules after '+' dropped, a file's own name
for "file=<name>", "_plus" read as "+", '_' as spaces, upper case ("ae_btb_plus_ctf+score=3" -> "AE BTB+ CTF") */
void ae_sl_gametype_word(const char *gametype, char *text, int size);
/* the card's line of rules, from the gametype's facts: "4 V 4" (players a side; 0 "FREE FOR ALL"), the start ("BR
START"), the goal ("50 KILLS"), the time ("15 MINUTES" said "15 MIN"; "NO TIME LIMIT" left out); joined by dots */
void ae_sl_facts(int side, const char *start, const char *goal, const char *time, char *text, int size);

/* ---------- the page's words */

/* the page's state, from what the machine knows now (the first that holds, in this order) */
enum { AE_SL_STATE_WAITING, AE_SL_STATE_VOTE, AE_SL_STATE_RESULT, AE_SL_STATE_COUNTDOWN, AE_SL_STATE_DOWNLOAD };
int ae_sl_state(int downloading, int countdown_seconds, int vote_heard, int vote_winner);
/* "AE", "ANYONE", "AE+MCC" */
const char *ae_sl_join_word(int join);
/* the panel's head: "1 OF 8 PEOPLE \xE2\x80\xA2 7 BOTS" ("1 OF 8 PEOPLE" with none) */
void ae_sl_people_line(int people, int maximum, int bots, char *text, int size);
/* a team's bar: "1 + 3 BOTS", "2 + 1 BOT", "2" */
void ae_sl_team_count(int people, int bots, char *text, int size);
/* under a team: "+ 3 BOTS, LEAVING AS PEOPLE JOIN" ("" for none) */
void ae_sl_bots_line(int bots, char *text, int size);
/* WAITING FOR PLAYERS' line: "Starts when 2 people are in. 1 is in now." (with bots seated: "Starts when 8 players are
in, bots counted. 7 are in now."; all in: "Everyone it waits for is in. It starts in a moment.") */
void ae_sl_waiting_line(int needed, int people, int bots, char *text, int size);
/* a choice's count: "1 VOTE", "0 VOTES" */
void ae_sl_votes_word(int votes, char *text, int size);
/* WON THE VOTE's line: "3 of 4 votes. The game starts in a moment." */
void ae_sl_won_line(int votes, int total, char *text, int size);
/* the vote's pips: how many squares a row draws (the people, at least the votes and 1, at most 8) */
int ae_sl_pips(int people, int most_votes);

/* the help line under the page: what the focus does and what the server allows */
enum { AE_SL_FOCUS_LIST, AE_SL_FOCUS_TEAM, AE_SL_FOCUS_SERVER, AE_SL_FOCUS_PLAYER };
struct ae_sl_help
{
	int state;                  /* AE_SL_STATE_* */
	int focus;                  /* AE_SL_FOCUS_* */
	int team_rule;              /* AE_SL_TEAMS_* */
	int team_game;
	int split;                  /* this PC has more than one player (one vote for the PC) */
	int pack_map;               /* (DOWNLOAD) the map is a pack's the server sends */
	const char *map;            /* the card's map, as drawn ("Hektik") */
	const char *player;         /* (FOCUS_PLAYER) the focused player's name */
	int player_bot;
};
void ae_sl_help_text(const struct ae_sl_help *help, char *text, int size);

/* ---------- the title: the server's name drawn in two faces (the title face draws a 1 as an I): its runs of digits and
'#' in the numerals' face, the rest in the title's */
struct ae_sl_run { int start, length, numerals; };
int ae_sl_title_runs(const char *title, struct ae_sl_run *runs, int maximum);

#endif
