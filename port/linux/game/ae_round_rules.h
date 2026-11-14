/*
AE_ROUND_RULES.H

The round modes' rules (the gametype's mode ELIMINATION on the Slayer engine: ae_variant_ext_rules.h), as pure
decisions: the round's phases, the wipe judge, the match's end, the sides' switch, revive times, and the settings read
from the AEXV block. No game dependency: unit-tested (port/linux/tests/ae_round_rules_test.c). The game side:
ae_round.c.

ELIMINATION: each player has one life a round. A round is PRE (everyone at their side's start spawns, a frozen
countdown: turning only), then LIVE (a side whose players are all down or out loses the round; the last side alive
wins it; both wiped on one tick is a tied round), then POST (4 s), then the next round, until a side has the wins the
match asks for (first to 5 of 9; RESOLVE TIES plays one more round at a time when the rounds are spent level). The
Slayer team score is the side's round wins. From the halfway round on (round 5 of 9) the teams start at each other's
start spawns. A player who joins during a round waits for the next.
*/
#ifndef AE_ROUND_RULES_H
#define AE_ROUND_RULES_H

#include <string.h>
#include "ae_pure_types.h"
#include "ae_variant_ext_rules.h"

#define AE_ROUND_TICKS_PER_SECOND 30
/* the most players a round mode's game holds (the wire's player entries: AE_ROUND_WIRE_PLAYERS): the lobby refuses more */
#define AE_ROUND_PLAYER_LIMIT 16
/* whether a game with this many players (bots too) has no room for another machine's player */
int ae_round_players_full(int players);
/* the bots a round mode's game holds: the wanted number, no more than the room humans leave (a game always holds at
least one human) */
long ae_round_bot_cap(long wanted, long humans);
/* how near a teammate stands to a downed player's spot to revive them (world units, across; and up or down) */
#define AE_ROUND_REVIVE_REACH 1.0f
#define AE_ROUND_REVIVE_REACH_HEIGHT 1.5f
/* how far a bot goes to revive a teammate (world units) */
#define AE_ROUND_BOT_REVIVE_RANGE 25.0f
/* ticks without an enemy in sight before a bot breaks off to revive: 3 s for a bot, 1 s for a person */
#define AE_ROUND_BOT_SAFE_TICKS (3 * 30)
#define AE_ROUND_BOT_HUMAN_SAFE_TICKS (1 * 30)
int ae_round_bot_revive_safe(long ticks_since_enemy_seen, int downed_is_human);

/* a bot's call on a downed teammate's orb (no look at what it sees: PLAYFIGHT bots see enemies for ever). It goes when the orb
is within AE_ROUND_BOT_REVIVE_RANGE, it was not hurt in the last AE_ROUND_REVIVE_FIRE_TICKS, no enemy is within
AE_ROUND_REVIVE_ENEMY_RANGE, and no nearer free bot of its team has the orb; the answer is the reason */
#define AE_ROUND_REVIVE_FIRE_TICKS 45
#define AE_ROUND_REVIVE_ENEMY_RANGE 5.0f
enum ae_round_revive_reason
{
	AE_ROUND_REVIVE_GO = 0,
	AE_ROUND_REVIVE_NO_ORB,
	AE_ROUND_REVIVE_FAR,
	AE_ROUND_REVIVE_UNDER_FIRE,
	AE_ROUND_REVIVE_ENEMY_NEAR,
	AE_ROUND_REVIVE_NOT_NEAREST,
	AE_ROUND_REVIVE_REASON_COUNT
};
/* orb_distance < 0 none; ticks_since_hurt < 0 never hurt; enemy_distance < 0 no enemy */
/* walking: this bot was already on its way (the last call was GO): a hit then stops it only at the orb (within
AE_ROUND_REVIVE_REACH, the hold); on the walk only an enemy within AE_ROUND_REVIVE_ENEMY_RANGE stops it */
int ae_round_bot_revive_decide(real orb_distance, long ticks_since_hurt, real enemy_distance, int nearer_free_bots,
	int walking);
/* the orb a bot goes to: the one it had (pinned, an index into the distances; -1 none) while it is still eligible and
in range, else the nearest eligible (a distance < 0 is not eligible); -1 when none. Two orbs down never flip it */
int ae_round_revive_orb_pick(int pinned, int count, real const *distances, real range);
/* the current reviver keeps the orb unless another free bot is this much nearer (wu); stops two bots swapping on a near tie */
#define AE_ROUND_REVIVE_HOLD_MARGIN 3.0f
/* TRUE when a free mate counts as nearer to the orb: nearer by distance (a tie goes to the lower index); the bot that is
already going (is_reviver) is only displaced by a mate at least AE_ROUND_REVIVE_HOLD_MARGIN nearer */
int ae_round_revive_mate_is_nearer(real mate_distance, real my_distance, int mate_has_lower_index, int is_reviver);
/* TRUE when a nearer mate really takes the revive from a bot: it goes to the SAME orb and no enemy is within
AE_ROUND_REVIVE_ENEMY_RANGE of it (a mate that heads for another orb, or has to fight first, would leave this orb
unrevived). A negative enemy distance means none seen. */
int ae_round_revive_mate_competes(int mate_on_same_orb, real mate_enemy_distance);
char const *ae_round_revive_reason_name(int reason);

enum
{
	AE_ROUND_PRE_TICKS = 5 * AE_ROUND_TICKS_PER_SECOND,
	AE_ROUND_POST_TICKS = 4 * AE_ROUND_TICKS_PER_SECOND,
	/* LIVE's first ticks are not judged (a spawn late by the spawn's own tick gate is not a wipe) */
	AE_ROUND_GRACE_TICKS = 2 * AE_ROUND_TICKS_PER_SECOND,
	/* the most sides a round judges: teams, or players without teams */
	AE_ROUND_SIDES = 16,
	/* a match's rounds when the gametype says none (0: the gametype's own) */
	AE_ROUND_DEFAULT_ROUNDS = 9,
	AE_ROUND_DEFAULT_TO_WIN = 5,
	AE_ROUND_DEFAULT_REVIVE_10THS = 30,
	/* the most revives a round counts for a player's next revive time (1.5 to that power) */
	AE_ROUND_REVIVE_GROWTH_LIMIT = 8,
};

enum ae_round_phase
{
	AE_ROUND_PHASE_OFF = 0,
	AE_ROUND_PHASE_PRE,
	AE_ROUND_PHASE_LIVE,
	AE_ROUND_PHASE_POST,
	AE_ROUND_PHASE_OVER,
	AE_ROUND_PHASE_COUNT
};

/* a player's state in the round */
enum ae_round_player_state
{
	AE_ROUND_PLAYER_NONE = 0,
	AE_ROUND_PLAYER_ALIVE,
	/* dead, with a revive spot: a teammate may bring them back */
	AE_ROUND_PLAYER_DOWN,
	/* dead for the rest of the round */
	AE_ROUND_PLAYER_OUT,
	/* joined during the round: plays from the next */
	AE_ROUND_PLAYER_WAITING,
	/* revived, about to spawn at their spot */
	AE_ROUND_PLAYER_REVIVED,
	AE_ROUND_PLAYER_STATE_COUNT
};

/* the wipe judge's verdicts */
enum
{
	AE_ROUND_GOES_ON = 0,
	AE_ROUND_WON,
	AE_ROUND_TIED
};

/* the last round's result as the wire and the log tell it: a side, or these */
enum
{
	AE_ROUND_RESULT_NONE = -1,
	AE_ROUND_RESULT_TIE = -2
};

/* the wipe judge: sides of members[] players in the round and alive[] of them standing (alive, or revived and about
to spawn). A side with no members is not judged. Returns AE_ROUND_WON with *winner the one side left standing among two
or more judged, AE_ROUND_TIED when no judged side stands, else AE_ROUND_GOES_ON (*winner -1) */
int ae_round_judge(int sides, int const *members, int const *alive, int *winner);

/* after a round: 1 when the match is over. max_rounds the match's rounds, to_win the wins that end it at once (0:
every round is played), resolve_ties whether rounds spent level play on one at a time, played the rounds played,
best and runner_up the two leading sides' wins */
int ae_round_match_over(int max_rounds, int to_win, int resolve_ties, int played, int best, int runner_up);

/* the round (1 up) from which the teams start at each other's spawns: the to-win round (5 of 9), else past half */
int ae_round_halftime(int max_rounds, int to_win);
/* whether the round plays with the sides switched */
int ae_round_sides_switched(int round, int max_rounds, int to_win, int switch_on);

/* a revive's ticks: the hold (tenths of a second) times 1.5 for each revive of that player this round before it */
long ae_round_revive_ticks(int tenths, int revives_before);

/* a revive's progress after one tick: one more while a teammate in reach holds, else back to 0 */
long ae_round_revive_step(long progress, int holding);


/* ---- EXTERMINATION and ATTRITION (the round machine's respawn rounds; the same machine, other rules) */

/* which rules a gametype's mode plays */
enum ae_round_kind
{
	AE_ROUND_KIND_NONE = 0,
	AE_ROUND_KIND_ELIMINATION,
	/* respawns (15 s); a round ends when a whole team is dead or waiting to respawn at once; 2 minute rounds, the
	round's kills decide a time-out (a tie replays once) */
	AE_ROUND_KIND_EXTERMINATION,
	/* a pool of lives a team (2 a player, 10 s respawns while it lasts), then the dead go DOWN (revive as ELIMINATION);
	the zone waits until both pools are empty or 10 minutes */
	AE_ROUND_KIND_ATTRITION,
	AE_ROUND_KIND_COUNT
};
enum
{
	AE_ROUND_EXTERMINATION_RESPAWN_SECONDS = 15,
	AE_ROUND_EXTERMINATION_ROUND_SECONDS = 120,
	AE_ROUND_ATTRITION_RESPAWN_SECONDS = 10,
	AE_ROUND_ATTRITION_ZONE_LATEST_SECONDS = 600,
};
/* the kind of an AE_EXT_MODE_* (none for another mode) */
int ae_round_kind_of_mode(int mode);
/* whether the kind respawns the dead during a round (EXTERMINATION, ATTRITION) */
int ae_round_kind_respawns(int kind);

/* how many of a side count as still in the round: the living, and (ATTRITION only) the dead whose life is paid and wait
to respawn. EXTERMINATION counts a player waiting to respawn as gone: a team all dead at once is wiped */
int ae_round_standing(int kind, int living, int paid_respawning);

/* the time-out of an EXTERMINATION round (0:00) with no team wiped (owner 11-04, the reference rule): a TIED round.
After AE_ROUND_TIMEOUT_TIES such ties in a match, the team with more kills over the match wins the match; level kills,
the match goes on (the next time-out tie decides again). Returns 1 when the match is decided, *winner the side (0 or 1);
0 otherwise, *winner -1 */
enum { AE_ROUND_TIMEOUT_TIES = 3 };
int ae_round_timeout_decides(int ties, long match_kills_0, long match_kills_1, int *winner);

/* whether the round plays with the sides switched: ELIMINATION from its halfway round, EXTERMINATION every other round
(round 2, 4 ...), ATTRITION as ELIMINATION */
int ae_round_kind_switched(int kind, int round, int max_rounds, int to_win, int switch_on);

/* ATTRITION's pool: 2 lives a player of the team (at least 2) */
int ae_round_pool_size(int team_players);
/* a death: spends a life when one is left (returns 1: the player respawns), else 0 (the player goes down) */
int ae_round_pool_spend(int *pool);
/* the warning a spend raises: 0 none, 1 LIVES LOW (the pool fell to AE_ROUND_POOL_LOW or under: 3 of 8; half and one for
other sizes), 2 NO RESPAWNS (the pool reached 0). before, after the spend */
int ae_round_pool_low_mark(int start);
enum { AE_ROUND_POOL_WARN_NONE = 0, AE_ROUND_POOL_WARN_LOW, AE_ROUND_POOL_WARN_EMPTY };
int ae_round_pool_warning(int start, int before, int after);

/* ATTRITION's zone: it starts when both pools are empty or AE_ROUND_ATTRITION_ZONE_LATEST_SECONDS have gone (1 start) */
int ae_round_zone_trigger(int pool_0, int pool_1, long elapsed_ticks);
/* its phases' length from when it started: 120 s if by 1:00, a straight line down to 30 s at 3:00 and later */
int ae_round_zone_closing_seconds(long start_elapsed_ticks);

/* the phase at now: PRE from round_start for AE_ROUND_PRE_TICKS, then LIVE; the countdown's ticks left (0 none) */
long ae_round_countdown_left(long round_start, long now);

/* the settings (an ELIMINATION gametype's AEXV block): the match's rounds and the wins that end it (the gametype's
own rounds are 9, first to 5), revive on (teams only), the zone on, the sides' switch on, the revive hold in tenths of
a second, the zone's wait and least closing seconds */
int ae_round_max_rounds(struct ae_variant_ext const *ext);
int ae_round_to_win(struct ae_variant_ext const *ext);
int ae_round_revive_on(struct ae_variant_ext const *ext, int teams);
int ae_round_zone_on(struct ae_variant_ext const *ext);
int ae_round_switch_on(struct ae_variant_ext const *ext, int teams);
int ae_round_revive_tenths(struct ae_variant_ext const *ext);
/* BLEEDOUT: the seconds a downed player's marker lasts before they are out (0: it lasts the round) */
int ae_round_bleedout_seconds(struct ae_variant_ext const *ext);
int ae_round_zone_wait_seconds(struct ae_variant_ext const *ext);
int ae_round_zone_shrink_seconds(struct ae_variant_ext const *ext);

/* a phase's and a player state's name, for the log and the debug line */
char const *ae_round_phase_name(int phase);
char const *ae_round_player_state_name(int state);

/* whether another machine's build (its Delta HELLO's build name, "Arena Evolved 0.7.6-beta") plays the round modes:
Arena Evolved 0.7.6 or later, or a -dev build of 0.7.5 (the branch's own, before VERSION moves on). Releases 0.5.0 to
0.7.5 are Arena Evolved too (their join requests carry its mark) but read the mode as none: no countdown freeze, revive
or zone, and the round state's message dropped. 1 yes, 0 no */
/* the same for the respawn modes (EXTERMINATION, ATTRITION): 0.7.8 or later (the released 0.7.7 has none), or a -dev
build of 0.7.7 (the branch's own, before VERSION moves on). A fourth number is read when there is one */
static inline int ae_round_build_has_respawn_modes(char const *build)
{
	static char const prefix[] = "Arena Evolved ";
	static int const least[4] = { 0, 7, 8, 0 };
	int part[4] = { 0, 0, 0, 0 }, at = 0, digits = 0, index;

	if (!build)
		return 0;
	for (char const *c = prefix; *c; c++, build++)
		if (*build != *c)
			return 0;
	for (; *build && at < 4; build++)
	{
		if (*build >= '0' && *build <= '9')
		{
			part[at] = part[at] * 10 + (*build - '0');
			digits++;
			if (part[at] > 9999)
				return 0;
		}
		else if (*build == '.' && digits && at < 3)
			at++, digits = 0;
		else
			break;
	}
	if (!digits || at < 2)
		return 0;
	for (index = 0; index < 4; index++)
		if (part[index] != least[index])
			return part[index] > least[index];
	return 1;
}

/* (a -dev build of 0.7.7 itself: the fix branch's own builds, which print "0.7.7-beta-dev") */
static inline int ae_round_build_has_respawn_modes_dev(char const *build)
{
	static char const exact[] = "Arena Evolved 0.7.7";
	size_t length;

	if (!build || strncmp(build, exact, sizeof(exact) - 1) || (build[sizeof(exact) - 1] >= '0' &&
		build[sizeof(exact) - 1] <= '9') || build[sizeof(exact) - 1] == '.')
	{
		return 0;
	}
	length = strlen(build);
	return length >= 4 && !strcmp(build + length - 4, "-dev");
}

static inline int ae_round_build_has_it(char const *build)
{
	static char const prefix[] = "Arena Evolved ";
	int part[3] = { 0, 0, 0 }, at = 0, digits = 0;

	if (!build)
		return 0;
	for (char const *c = prefix; *c; c++, build++)
		if (*build != *c)
			return 0;
	for (; *build && at < 3; build++)
	{
		if (*build >= '0' && *build <= '9')
		{
			part[at] = part[at] * 10 + (*build - '0');
			digits++;
			if (part[at] > 9999)
				return 0;
		}
		else if (*build == '.' && digits)
			at++, digits = 0;
		else
			break;
	}
	if (!digits && at < 3)
		return 0;
	if (part[0] != 0)
		return part[0] > 0;
	if (part[1] != 7)
		return part[1] > 7;
	if (part[2] != 5)
		return part[2] > 5;
	/* (0.7.5: only the dev builds, whose name ends -dev) */
	{
		size_t length = strlen(build);

		return length >= 4 && !strcmp(build + length - 4, "-dev");
	}
}

/* the scoreboard's word after a name (0.7.6): none, DOWN (the round's revive-pending), DEAD (out of lives, the round's OUT, SEARCH
AND DESTROY's dead till the round ends) or QUIT. A downed player of a one-life round mode is out of lives too, so DOWN is read first */
enum { AE_SCOREBOARD_STATUS_NONE, AE_SCOREBOARD_STATUS_DOWN, AE_SCOREBOARD_STATUS_DEAD, AE_SCOREBOARD_STATUS_QUIT };
static inline int ae_scoreboard_status_rule(int round_state_down, int out_of_lives, int round_state_out, int snd_dead, int quit)
{
	if (round_state_down)
		return AE_SCOREBOARD_STATUS_DOWN;
	if (out_of_lives || round_state_out || snd_dead)
		return AE_SCOREBOARD_STATUS_DEAD;
	return quit ? AE_SCOREBOARD_STATUS_QUIT : AE_SCOREBOARD_STATUS_NONE;
}

/* the ground a downed player's spot drops to (0.7.6): the look goes straight down from AE_ROUND_GROUND_ABOVE over the death spot for
AE_ROUND_GROUND_REACH under it. A floor found gives the spot's height (a death on the ground stays where it was); none found
inside the reach (a pit, the void) means no revive there: the player is out */
#define AE_ROUND_GROUND_ABOVE 0.5f
#define AE_ROUND_GROUND_REACH 30.0f
static inline int ae_round_ground_spot_rule(int hit, float hit_z, float spot_z, float *ground_z)
{
	if (!hit || hit_z > spot_z + AE_ROUND_GROUND_ABOVE + 0.001f || hit_z < spot_z - AE_ROUND_GROUND_REACH - 0.001f)
		return 0;
	*ground_z = hit_z;
	return 1;
}

#endif
