/*
AE_BOTS_OBJECTIVE_RULES.H

The bots' objective play (ae_bots_objective.c): a small utility choice per bot,
each tick, between fighting (bots.c's own brain) and the goals of the game's
mode, as Halo Infinite's bots choose between their behaviours. Pure numbers
only (no engine types), unit-tested in port/linux/tests/ae_bots_objective_rules_test.c.

- Each behaviour's utility is 0 to 1 from what the bot knows: how far each goal
  is (a negative distance: that goal is not there now), its own health, whether
  it fights an enemy in sight and how near, and its team role.
- The objective's own carrying wins over everything (a flag carrier runs home
  hurt or not, the ball's holder keeps it: Infinite's sacrifice plays); a goal
  near outweighs a fight far, a fight close outweighs a goal far.
- Hysteresis: the behaviour chosen keeps a bonus for a while (about two
  seconds), so that a bot does not flip between two close utilities.
- Team roles in CTF: a team of three players or more keeps one defender (two
  from six up); the rest attack.
*/

#ifndef __AE_BOTS_OBJECTIVE_RULES_H
#define __AE_BOTS_OBJECTIVE_RULES_H

/* the game's mode as the bots play it */
enum ae_bots_mode
{
	AE_BOTS_MODE_SLAYER = 0,
	AE_BOTS_MODE_CTF,
	/* oddball's ball (the stock ball type) */
	AE_BOTS_MODE_ODDBALL,
	/* oddball's reverse tag: the one who is "it" (a kill passes it on) scores by time */
	AE_BOTS_MODE_TAG,
	/* oddball's juggernaut (stock, or AE JUGGER's Halo 2 rules): the juggernaut scores by kills */
	AE_BOTS_MODE_JUGGERNAUT,
	AE_BOTS_MODE_KING,
	AE_BOTS_MODE_RACE,
	AE_BOTS_MODES
};

/* what a bot does */
enum ae_bots_behaviour
{
	/* bots.c's own brain: fight what it sees, else hunt and roam */
	AE_BOTS_FIGHT = 0,
	/* CTF: go to the enemy flag; bring it home; touch (or hunt the carrier of) the own flag away from its stand;
	keep near the own flag's stand */
	AE_BOTS_CTF_TAKE,
	AE_BOTS_CTF_CARRY,
	AE_BOTS_CTF_RETURN,
	AE_BOTS_CTF_DEFEND,
	/* follow a teammate who carries the flag, holds the ball or is the team's juggernaut */
	AE_BOTS_ESCORT,
	/* oddball: go to the free ball; hold it (keep moving, away from enemies); go for the enemy who holds it (and in
	reverse tag, the one who is "it") */
	AE_BOTS_BALL_TAKE,
	AE_BOTS_BALL_HOLD,
	AE_BOTS_BALL_CHASE,
	/* king of the hill: go to the hill; hold it (stay in it, fight from it) */
	AE_BOTS_HILL_GO,
	AE_BOTS_HILL_HOLD,
	/* juggernaut: go for the juggernaut; the juggernaut, badly hurt, breaks away */
	AE_BOTS_JUGG_HUNT,
	AE_BOTS_JUGG_FLEE,
	/* race: the next flag it may touch */
	AE_BOTS_RACE_NEXT,
	AE_BOTS_BEHAVIOURS
};

/* what a bot knows this tick (distances in world units; a negative one: that goal is not there now) */
struct ae_bots_situation
{
	int mode;
	/* its shields and body together, 0 to 1 */
	float health;
	/* an enemy in sight, this near (negative: none) */
	int fighting;
	float enemy_distance;
	/* CTF: its team's defender */
	int defender;
	/* it carries the enemy flag, holds the ball, is "it" or is the juggernaut */
	int carrying;
	/* CTF: to the enemy flag (free: at its stand or dropped); to the own flag away from its stand (dropped, or
	carried: own_flag_carried); to the own flag's stand */
	float enemy_flag;
	float own_flag_away;
	int own_flag_carried;
	float own_stand;
	/* to a teammate who carries (the flag, the ball, the juggernaut's role) */
	float ally_carrier;
	/* oddball: to the nearest free ball; to an enemy who holds it (or is "it", or the juggernaut) */
	float ball;
	float enemy_carrier;
	/* king: to the hill's centre; in the hill now */
	float hill;
	int in_hill;
	/* race: to the next flag */
	float race_flag;
};

/* each behaviour's utility, 0 to 1 (0: not open to it now) */
void ae_bots_objective_utilities(struct ae_bots_situation const *situation, float utility[AE_BOTS_BEHAVIOURS]);

/* the behaviour chosen: the most useful, the current one with `bonus` while it has been held fewer than
`hysteresis_ticks` (a tie keeps the current one, else the lower behaviour); the current one is dropped when its
utility is 0 */
int ae_bots_objective_pick(float const utility[AE_BOTS_BEHAVIOURS], int current, long held_ticks,
	long hysteresis_ticks, float bonus);

/* how many of a CTF team's players defend (team_size counts people and bots): none under 3, one from 3, two from 6 */
int ae_bots_ctf_defenders(int team_size);

/* whether the behaviour moves the bot to its goal even while it fights (it still shoots and strikes what it sees
from bots.c's fight, but does not chase it): the carrier, the bot going for a flag or ball it chose over a fight,
the hill, the race; FALSE for those that fight as bots.c does when they meet an enemy (defend, escort, chase,
hunt) */
int ae_bots_objective_runs_while_fighting(int behaviour);

/* the behaviour's name for halo.log ("take", "carry", ...) */
char const *ae_bots_behaviour_name(int behaviour);
/* the mode's name for halo.log ("ctf", "oddball", ...) */
char const *ae_bots_mode_name(int mode);

#endif
