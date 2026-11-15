/*
AE_BOTS_OBJECTIVE_RULES.H

The bots' objective play (ae_bots_objective.c): a small utility choice per bot,
each tick, between fighting (bots.c's own brain) and the goals of the game's
mode, the way bots choose between their behaviours. Pure numbers
only (no engine types), unit-tested in port/linux/tests/ae_bots_objective_rules_test.c.

- Each behaviour's utility is 0 to 1 from what the bot knows: how far each goal
  is (a negative distance: that goal is not there now), its own health, whether
  it fights an enemy in sight and how near, and its team role.
- The objective's own carrying wins over everything (a flag carrier runs home
  hurt or not, the ball's holder keeps it: the sacrifice plays); a goal
  near outweighs a fight far, a fight close outweighs a goal far.
- Hysteresis: the behaviour chosen keeps a bonus for a while (about two
  seconds), so that a bot does not flip between two close utilities.
- Team roles in CTF keep one defender (two from six up). SEARCH AND DESTROY
  keeps all but at most one HEROIC-or-higher defender at spread site angles.
- Bomb (game_engine_bomb.c): one neutral bomb; its carrier takes it to the
  enemy goal and stands there until it is armed; teammates escort it; the
  other team hunts the carrier, keeps its own goal and steps onto a bomb
  planted there to save it.
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
	/* oddball's juggernaut (stock, or AE JUGGER's rules): the juggernaut scores by kills */
	AE_BOTS_MODE_JUGGERNAUT,
	AE_BOTS_MODE_KING,
	AE_BOTS_MODE_RACE,
	/* Bomb and Grifball (game_engine_bomb.c): the neutral bomb to the enemy goal */
	AE_BOTS_MODE_BOMB,
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
	/* bomb: go to the bomb lying loose; carry it to the enemy goal and stand there while it arms; keep near the own
	goal; step onto the bomb planted in the own goal to save it (the enemy carrier: BALL_CHASE; a teammate carrier:
	ESCORT) */
	AE_BOTS_BOMB_TAKE,
	AE_BOTS_BOMB_CARRY,
	AE_BOTS_BOMB_DEFEND,
	AE_BOTS_BOMB_SAVE,
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
	/* bomb: to the bomb lying loose (one anyone may take now); to the enemy goal (where its carrier arms it); to the
	own goal; to a bomb planted in either goal (ally_carrier, enemy_carrier: its carrier; defender: the bot assigned
	to hold the site). SEARCH AND DESTROY says whether its extra team rules apply, bomb_attacking which side this bot
	is on, and bomb_defuser which one living defender takes a planted bomb while its teammates cover. */
	float bomb;
	float enemy_goal;
	float own_goal;
	float bomb_planted_own;
	float bomb_planted_enemy;
	int bomb_snd;
	int bomb_attacking;
	int bomb_defuser;
	/* this bot is the stable nearest teammate assigned beside the carrier */
	int wingman;
	/* SEARCH AND DESTROY: this round's role (AE_BOTS_SND_ROLE_*), and for a forward defender whether it has a point
	down the lane to hold */
	int snd_role;
	int snd_forward_point;
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
/* a team with a human, up to 8 players: one defender, so the human's squad and two flag crews have bots enough */
int ae_bots_ctf_defenders_with_human(int team_size);

/* whether the behaviour moves the bot to its goal even while it fights (it still shoots and strikes what it sees
from bots.c's fight, but does not chase it): the carrier, the bot going for a flag or ball it chose over a fight,
the hill, the race, and a SEARCH AND DESTROY bomb escort or guard; FALSE for those that fight as bots.c does when
they meet an enemy (an ordinary defend or escort, chase, hunt). bomb_snd is true only for that Bomb ruleset */
int ae_bots_objective_runs_while_fighting(int behaviour, int bomb_snd);

enum ae_bots_carrier_choice
{
	AE_BOTS_CARRIER_KEEP = 0,
	AE_BOTS_CARRIER_DROP,
	AE_BOTS_CARRIER_WAIT,
	AE_BOTS_CARRIER_RECOVER
};

/* A Bomb SEARCH AND DESTROY or stock Oddball carrier's choice between the objective and its gun. Skill level is
RECRUIT=0 through DEMON=5. Distances are negative when there is no such place or teammate. */
struct ae_bots_carrier_situation
{
	int mode;
	int skill_level;
	int bomb_snd;
	int carrying;
	int dropped;
	int enemy_engaging;
	int under_fire;
	int gun_loaded;
	int last_alive;
	int in_cover;
	float enemy_distance;
	float gun_range;
	float teammate_distance;
	float goal_distance;
	float cover_distance;
	float clear_seconds;
	float drop_cooldown_seconds;
};

/* WAIT leaves a remembered dropped objective on the ground while the fight lasts; RECOVER fetches it afterward. */
int ae_bots_carrier_choice(struct ae_bots_carrier_situation const *situation);

struct ae_bots_wingman_candidate
{
	int eligible;
	float distance;
};

/* The nearest eligible bot, keeping the current one while it is no farther than switch_margin beyond the nearest. */
int ae_bots_wingman_pick(struct ae_bots_wingman_candidate const *candidates, int count, int current,
	float switch_margin);

enum ae_bots_recovery_choice
{
	AE_BOTS_RECOVERY_WAIT = 0,
	AE_BOTS_RECOVERY_GO,
	AE_BOTS_RECOVERY_RESET,
	AE_BOTS_RECOVERY_HANDOFF,
	AE_BOTS_RECOVERY_ABANDON_TAKEN,
	AE_BOTS_RECOVERY_ABANDON_DEAD,
	AE_BOTS_RECOVERY_ABANDON_GONE,
	AE_BOTS_RECOVERY_ABANDON_NO_GOAL
};

struct ae_bots_recovery_situation
{
	int dropper_alive;
	int team_game;
	int near_teammate;
	int remembered_free;
	int replacement_free;
	int objective_carried;
	int goal_valid;
	int enemy_engaging;
	float clear_seconds;
	float missing_seconds;
};

/* A deliberately dropped ball or bomb: keep fighting, fetch it, adopt its reset replacement, hand it to a teammate,
or give a countable reason that it cannot be recovered. */
int ae_bots_recovery_choice(struct ae_bots_recovery_situation const *situation);

/* SEARCH AND DESTROY roles. A side's bots hold one role a round, chosen from the team's bot slots and the round, so
the same bot is not the entry every round; when a role's bot dies the others are promoted (an anchor and a support are
never left empty while a teammate lives). */
enum
{
	AE_BOTS_SND_ROLE_NONE,
	AE_BOTS_SND_ROLE_CARRIER,
	AE_BOTS_SND_ROLE_ENTRY,		/* attack: first through its lane, clears the site */
	AE_BOTS_SND_ROLE_SUPPORT,	/* attack: one step behind the carrier */
	AE_BOTS_SND_ROLE_LURKER,	/* attack: the second lane, arrives late; after the plant it hunts */
	AE_BOTS_SND_ROLE_ANCHOR,	/* defence: the tight ring about the site, never leaves it */
	AE_BOTS_SND_ROLE_HOLDER,	/* defence: a wider angle on the site */
	AE_BOTS_SND_ROLE_FORWARD,	/* defence: holds a point down the lane for early contact, falls back */
	AE_BOTS_SND_ROLES
};

/* Roles of a side of `size` bot slots (slot 0 first), the living ones in alive_mask (bit per slot), for round number
`round`. roles[slot] is NONE for a dead slot. Forward defenders: none under three living, one to five, two beyond. */
void ae_bots_snd_roles(int attacking, int size, unsigned long alive_mask, long round, int roles[]);
char const *ae_bots_snd_role_name(int role);

/* Credit for a nearby navigation point used as this bot's first lane toward a distant goal. Negative rejects it.
lane is -1, 0 or 1; another bot's point within three units makes this candidate unavailable. */
float ae_bots_lane_score(float start_x, float start_y, float target_x, float target_y,
	float point_x, float point_y, int lane, float nearest_taken_squared);

/* the behaviour's name for halo.log ("take", "carry", ...) */
char const *ae_bots_behaviour_name(int behaviour);
/* the mode's name for halo.log ("ctf", "oddball", ...) */
char const *ae_bots_mode_name(int mode);

#endif
