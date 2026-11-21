/*
AE_SPLIT_JOIN_RULES.H

Joining and leaving a match in progress as a split screen player: who may, and the words a refusal says. Pure: no
engine includes (unit test port/linux/tests/ae_split_join_rules_test.c). The game's calls are ae_glue_split_join.c; the
host's switch is the gametype's JOIN IN PROGRESS: SPLIT SCREEN (the AE block's split_join, ae_variant_ext_rules.h), which
travels to every machine with the rest of the block, so a guest knows it before it asks and the host decides when it
does.
*/

#ifndef __AE_SPLIT_JOIN_RULES_H
#define __AE_SPLIT_JOIN_RULES_H

/* why a pad may not join (AE_SPLIT_JOIN_OK: it may). The order is the order the reasons are checked in */
enum ae_split_join_reason
{
	AE_SPLIT_JOIN_OK = 0,
	AE_SPLIT_JOIN_NOT_MULTIPLAYER,   /* a campaign level, co-op, or no game under way */
	AE_SPLIT_JOIN_REPLAY,            /* a film plays */
	AE_SPLIT_JOIN_POSTGAME,          /* the game is over */
	AE_SPLIT_JOIN_OFF,               /* the host's JOIN IN PROGRESS: SPLIT SCREEN is off */
	AE_SPLIT_JOIN_TOURNAMENT,        /* an AE TOURNAMENT match: its roster is set */
	AE_SPLIT_JOIN_VOTE,              /* a map vote is under way */
	AE_SPLIT_JOIN_NO_PAD,            /* no controller of this PC is free (or it has no pad) */
	AE_SPLIT_JOIN_IN_GAME,           /* the controller already plays */
	AE_SPLIT_JOIN_MAX_LOCAL,         /* four players are on this PC already */
	AE_SPLIT_JOIN_FULL,              /* the game has no free player slot and no bot to make room */
	AE_SPLIT_JOIN_REASONS
};

struct ae_split_join_facts
{
	int multiplayer;      /* a multiplayer game with a network game (the host's or a guest's) is running */
	int replay;
	int postgame;
	int option_on;        /* the game's split_join, as this machine knows it */
	int tournament;
	int vote;
	int pad;              /* the controller is a pad this PC has (DEV MODE's staging says 1 whatever the PC has) */
	int controller_plays; /* the controller has a player in the game */
	int local_players;    /* the players on this PC */
	int slots_free;       /* free player slots in the game, bots that leave for a person counted as free */
};

/* AE_SPLIT_JOIN_OK or the first reason the controller may not join */
int ae_split_join_check(struct ae_split_join_facts const *facts);
/* the pads a machine joins a game in progress with ask just after its first player does, as one join: this long (ms) */
enum { AE_SPLIT_JOIN_TOGETHER_MS = 20000 };
/* the host's side: a machine that has players in the game already asks for another (a local player) only when the switch
is on and the game is no tournament match, unless it is the same join (since_first_ms, the time since this machine's first
player asked in this game; -1 when the host did not see it: the machine was in the game from its start); a machine's first
player (a machine joining the game in progress) is never this rule's. 1 allowed */
int ae_split_join_host_allows(int option_on, int tournament, int machine_players, long since_first_ms);
/* the reason's words for the menu (AE's voice: capitals, no full stop); "" for AE_SPLIT_JOIN_OK */
const char *ae_split_join_reason_text(int reason);
/* a pad may leave: it plays and another player of this PC stays (the last one's way out is QUIT) */
int ae_split_leave_allowed(int controller_plays, int local_players);
/* the controller a pad on this PC takes to join: the first of 4 that does not play and has a pad (plays and pads are
bit masks, bit c = controller c; hint: tried first, -1 for none); -1 when none is free */
int ae_split_join_controller(unsigned plays, unsigned pads, int hint);
/* the view count after a join or a leave (1-4) */
int ae_split_join_views_after(int local_players, int joining);

#endif
