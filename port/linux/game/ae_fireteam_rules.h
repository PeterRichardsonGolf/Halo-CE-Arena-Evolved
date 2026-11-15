/*
AE_FIRETEAM_RULES.H

FIRETEAMS (ae_fireteam.c): which bots belong to which fireteam, as pure rules (port/linux/tests/ae_fireteam_rules_test.c
runs them). A team's bots are split evenly over its human leaders, five a fireteam at most; the bots left over form
bot-led squads of three to five. Every bot (a brain, bots.h's index) is in one fireteam or none, never two. A fireteam's
leader is a player (its index, as the engine names it) or none: a bot-led squad, led by its lowest brain. No engine
includes: the table is plain arrays.
*/

#ifndef __AE_FIRETEAM_RULES_H
#define __AE_FIRETEAM_RULES_H

/* one more than the bots' brains (bots.h's BOTS_BRAINS) and the most fireteams of a game, both teams together */
#define AE_FIRETEAM_BRAINS 127
#define AE_FIRETEAM_MAX 64

/* the most a split puts in a fireteam, and the fewest a leftover squad starts with */
#define AE_FIRETEAM_CAP 5
#define AE_FIRETEAM_SQUAD_MIN 3

/* the standing orders (a fireteam has one at a time) */
enum { _ae_order_none, _ae_order_follow, _ae_order_regroup, _ae_order_attack, _ae_order_defend, _ae_order_watch,
	_ae_order_go, AE_ORDER_COUNT };

/* the constants block: FOLLOW's leash, how close a REGROUP member must come before it counts as arrived */
#define AE_FIRETEAM_LEASH 10.0f
#define AE_FIRETEAM_REFORM_RADIUS 6.0f
/* DEFEND and WATCH hold this far around their point; ATTACK is pressed again past this many leashes from its point;
GOING lets go this many seconds after the first member stood at its point */
#define AE_FIRETEAM_ANCHOR_RADIUS 8.0f
#define AE_FIRETEAM_ATTACK_DRIFT 1.5f
#define AE_FIRETEAM_GO_HOLD_SECONDS 15.0f
/* REGROUP and GOING hold until the members arrive; they let go only when no member can (all dead, seated, stuck) for this long
(a REGROUP every member reached is not let go so: a person's is held) */
#define AE_FIRETEAM_STALL_SECONDS 60.0f

struct ae_fireteam
{
	short team;		/* -1: the slot is free */
	long leader;		/* a player's index, or -1: a bot-led squad */
	short members;		/* bots in it */
	short order_kind;	/* the standing order, _ae_order_none with none */
	float order_point[3];	/* where it points (attack, defend, watch, go, a regroup spot), none: the leader */
	long order_object;	/* what it points at, or -1 */
	short order_has_point;
	float go_hold_start;	/* GOING: the second the first member stood at the point, or -1 */
	short order_explicit;	/* the order was a leader's own (a call), not the squad's role: it holds against the role */
	short role_slot;	/* 0: the squad's rank among its team's bot squads; k > 0: its place k - 1 in the team's role mix (swapped by wipes) */
	short role;		/* AE_ROLE_*, the role the order came from (none before the first role pass) */
	long order_serial;	/* counts the orders set (ae_fireteam_set_order): a new serial is a new order, even of the same kind */
};

struct ae_fireteam_table
{
	short count;		/* slots in use or free, up to AE_FIRETEAM_MAX */
	long generation;	/* counts the rebuilds (a split): the roles are handed out again when it moved */
	struct ae_fireteam fireteams[AE_FIRETEAM_MAX];
	short arena_rules;	/* 1: small-game rules, a person's squad plays roles, stays loosely near the person and follows only on a FOLLOW ME; 0 (clear's value): big team, it plays roles free and follows only on a regroup or FOLLOW ME */
	short brain_fireteam[AE_FIRETEAM_BRAINS];	/* -1: in none */
	char brain_arrived[AE_FIRETEAM_BRAINS];	/* REGROUP: the bot came within the re-form radius */
};

void ae_fireteam_clear(struct ae_fireteam_table *table);

/* BIG TEAM OR ARENA (owner 11-06, "btb is where fireteams should behave like that"): the rules a game's fireteams play by.
BIG: a person's squad follows its leader by default (FOLLOW, leash). ARENA: it plays its per-gametype role like a bot-led
squad and follows only after the leader's FOLLOW ME (SMART call); other orders work in both. The rule: an AE gametype is
BIG when it is in the AE BTB / BTB+ category (and ARENA in any other category); a gametype not of AE's (stock, the player's
own) is BIG when a team holds more than AE_FIRETEAM_BIG_TEAM players, bots included. reason (may be NULL) gets a short text */
#define AE_FIRETEAM_BIG_TEAM 4
int ae_fireteam_rules_are_arena(int ae_gametype, int btb_category, int largest_team, char const **reason);
void ae_fireteam_set_rules(struct ae_fireteam_table *table, int arena);

/* ARENA ORDERS ARE TEMPORARY (owner 11-06, "they should go back to roles smartly"): in an arena game a leader's order (REGROUP,
FOLLOW ME, a point call) holds the squad only so long. It is released back to the squad's role when (a) the order is fulfilled
(REGROUP: every able member arrived; GOING and ATTACK: a member stands at the spot) and the squad has been out of combat for
AE_FIRETEAM_ARENA_CALM_SECONDS, (b) the leader is dead, (c) an objective event happened (flag taken, dropped, captured; hill
moved; bomb planted; round reset), or (d) the order is older than its cap (REGROUP 30 s, any other 45 s) with no new one.
The members leave one by one, AE_FIRETEAM_ARENA_STAGGER_SECONDS apart. Big team games never release */
#define AE_FIRETEAM_ARENA_CALM_SECONDS 5.0f
#define AE_FIRETEAM_ARENA_REGROUP_CAP 30.0f
#define AE_FIRETEAM_ARENA_ORDER_CAP 45.0f
#define AE_FIRETEAM_ARENA_STAGGER_SECONDS 0.6f
enum { AE_RELEASE_NONE, AE_RELEASE_FULFILLED, AE_RELEASE_LEADER_DEAD, AE_RELEASE_OBJECTIVE, AE_RELEASE_CAP };
/* why the leader's explicit order should end now (AE_RELEASE_*), AE_RELEASE_NONE to hold it. age: seconds since it was given;
calm: seconds since any member last fought */
int ae_fireteam_arena_release(short order_kind, int explicit_order, float age, float calm, int fulfilled, int leader_alive,
	int objective_event);
char const *ae_fireteam_release_name(int reason);
/* every member of a REGROUP has come within the re-form radius (a person's squad: the order stays, held or until released) */
int ae_fireteam_regroup_complete(struct ae_fireteam_table const *table, int fireteam);
/* end the fireteam's order (the role gives the next) */
void ae_fireteam_release_order(struct ae_fireteam_table *table, int fireteam);

/* rebuild a team's fireteams from its human leaders (in join order) and its bots (brain indices). The team's old
fireteams go; a bot of the list that was in another team's fireteam leaves it */
void ae_fireteam_split(struct ae_fireteam_table *table, short team, long const *leader_players, short leaders,
	short const *bot_brains, short bots);

/* the fireteam of a bot, or -1 */
int ae_fireteam_of_bot(struct ae_fireteam_table const *table, short brain);

/* a bot of another fireteam of its team joins this one. 0 when it would leave its fireteam empty, or the bot or the
fireteam is none or of another team; 1 when it moved */
int ae_fireteam_recruit(struct ae_fireteam_table *table, short brain, int to_fireteam);

/* recruiting by a leader's mark: why a bot is or is not pulled into a fireteam */
enum { AE_RECRUIT_OK, AE_RECRUIT_SAME, AE_RECRUIT_NONE, AE_RECRUIT_OBJECTIVE, AE_RECRUIT_SEAT, AE_RECRUIT_LAST };

/* whether ae_fireteam_recruit would move the bot, and the reason when not: NONE for a bot or fireteam that is not there
(or of another team), SAME for the fireteam it is in, OBJECTIVE for a carrier of the flag, ball or bomb, SEAT for a seated
driver, LAST for the last member of its fireteam. The refusals OBJECTIVE, SEAT and LAST are answered "can't right now" */
int ae_fireteam_recruit_check(struct ae_fireteam_table const *table, short brain, int to_fireteam, int carrying_objective,
	int seated_driver);

/* SMART CALL's context for a leader: the aim is on a bot of the caller's own fireteam (FOLLOW), on a bot of another fireteam
of its team (RECRUIT), else the ground within this many wu of the caller (FOLLOW), else the plain mark */
#define AE_FIRETEAM_SMART_GROUND_RADIUS 5.0f
enum { AE_SMART_PLAIN, AE_SMART_FOLLOW, AE_SMART_RECRUIT };
int ae_fireteam_smart_resolve(int on_friendly_bot, int bot_in_own_fireteam, int on_ground, float distance_to_caller);
/* the standing order a SMART CALL gives its fireteam once resolved: FOLLOW for FOLLOW; for the plain mark (a place farther than
the radius: SMART's HERE) GOING, so the squad walks there, as ATTACK or DEFEND HERE do; none for a recruit (_ae_order_none) */
short ae_fireteam_smart_order(int resolved);

/* what a wheel call does to a fireteam on the host (the wire is as it was: entry 3, a 0.7.5 build's NEED WEAPON, is REGROUP).
The standing order a call gives its caller's fireteam (_ae_order_none: none); only a fireteam's leader gives one, from the
fireteam argument (-1: the caller leads none, which is every call with fireteams off) */
short ae_fireteam_call_order(int fireteam, int wheel_entry);
/* 1 when a leader's SMART CALL on a place mark is tried against its fireteam (a bot, or the ground by its feet) first */
int ae_fireteam_call_smart(int fireteam, int wheel_entry, int mark_is_place);
/* 0 when the call asks nothing of the team's bots beyond the fireteam (REGROUP: a plain chat line for anyone who does not
lead a fireteam); 1 for every other call, which shares as the marks always did */
int ae_fireteam_call_shares(int wheel_entry);

/* the player who leads a fireteam, or -1 for a bot-led squad (or no such fireteam) */
long ae_fireteam_leader(struct ae_fireteam_table const *table, int fireteam);

/* a squad's lead: the lowest brain index in the fireteam, or -1 with none */
short ae_fireteam_lead_brain(struct ae_fireteam_table const *table, int fireteam);

/* a leading player quit: its fireteam's bots go to a bot-led squad of the team (else the team's smallest fireteam);
alone on its team, the fireteam stays as a bot-led squad */
void ae_fireteam_leader_left(struct ae_fireteam_table *table, long player);

/* the fireteam's one standing order: a new one replaces the old. point may be NULL (the leader is the point). Does
nothing for a free fireteam. A wipe changes nothing here: members and order stay, the bots take it up when they spawn */
void ae_fireteam_set_order(struct ae_fireteam_table *table, int fireteam, short kind, float const *point, long object);
short ae_fireteam_order_kind(struct ae_fireteam_table const *table, int fireteam);
/* 1 and the point with a fixed point, else 0 */
int ae_fireteam_order_point(struct ae_fireteam_table const *table, int fireteam, float *point);
long ae_fireteam_order_object(struct ae_fireteam_table const *table, int fireteam);
/* the serial of the fireteam's standing order (0 before any was set, or after a rebuild kept it): the host times a spent REGROUP
or GOING by it */
long ae_fireteam_order_serial(struct ae_fireteam_table const *table, int fireteam);

/* FOLLOW: 1 when the bot should go to the leader: outside the leash. Inside it a bot is left alone, fighting or not
(a fight outside the leash pulls it back in) */
int ae_fireteam_follow_wanted(float distance_to_leader);

/* REGROUP: a bot reports its distance to the spot; a bot that arrived (within the re-form radius) stays counted. A bot-led
squad's order clears itself (1 returned) once every member has arrived. A person's squad keeps it: in a big team game it is
held (below), under the arena rules the release ends it */
int ae_fireteam_regroup_note(struct ae_fireteam_table *table, int fireteam, short brain, float distance);

/* REGROUP HELD (owner 11-07, big team: "they need to be more sticky and closer to you"; "They come and stay in close formation
with you until you give another order (FOLLOW ME, ATTACK, DEFEND, a ping) or you die. Calling REGROUP again while they are
with you sends them back to playing free"): in a big team game a person's REGROUP does not end when the squad has come. The
members keep their slots at the REGROUP tolerance, in a fight too; a ping still takes a member off and it falls back in after.
It ends on the leader's next order (any order replaces it), on the leader's death, or on a REGROUP called while the squad is
formed: every member has arrived and every living one is within the follow radius of the leader; then the squad plays free
(no order, as a big team's squad with none). A REGROUP called while it is not formed forms it again. Arena rules are as
they were (temporary orders, released below) */
enum { AE_REGROUP_FORM, AE_REGROUP_RELEASE };
/* what a person's REGROUP call does: order_kind is the squad's standing order now, all_arrived every member has arrived
under it, alive and near: its living members and those of them within the follow radius of the leader */
int ae_fireteam_regroup_call(short order_kind, int arena, int all_arrived, int alive, int near);
/* 1 when a person's held REGROUP ends of itself: a big team game's explicit REGROUP whose leader is dead */
int ae_fireteam_regroup_ends(short order_kind, int explicit_order, int arena, int leader_alive);

/* REGROUP: 1 when the bot has been within the re-form radius */
int ae_fireteam_brain_arrived(struct ae_fireteam_table const *table, short brain);
/* REGROUP or GOING: 1 to let the order go: no member is able to finish it (able_members is 0) and it has been so for
AE_FIRETEAM_STALL_SECONDS. Distance alone never lets one go; other kinds: 0 */
int ae_fireteam_gave_up(short kind, int able_members, float stalled_seconds);

/* DEFEND and WATCH are holds: a point kept, fights or not (v1: WATCH stands at its point as DEFEND does) */
int ae_fireteam_order_holds(short kind);

/* a point order (attack, defend, watch, go): 1 when the bot's goal should be set to the point now. position_distance and
goal_distance are the bot's, and its goal's, distance to the point. Holds (defend, watch, go) want it when either is
past the radius, so a goal a fight or a carried flag moved away is set back at once; ATTACK only when the bot has drifted
past AE_FIRETEAM_ATTACK_DRIFT leashes (inside, the bot is left to fight and roam). Other kinds: 0 */
int ae_fireteam_press_wanted(short kind, float position_distance, float goal_distance);

/* GOING: a bot reports its distance to the point and the time in seconds. The clock starts when the first member is within
the re-form radius; the order clears itself (1 returned) AE_FIRETEAM_GO_HOLD_SECONDS later, on any member's report */
int ae_fireteam_go_note(struct ae_fireteam_table *table, int fireteam, short brain, float distance, float now_seconds);

/* ---------- squad roles (ae_fireteam.c reads the objective's points; the roles are these rules)

A role is a standing order the mode gives a squad by default. The team's fireteams take the mode's roles round-robin, in
table order (the first squad the first role); a wiped squad takes the next role on; an objective that flipped (a flag taken
or returned, the hill moved, the bomb planted) gives the same roles their new points. A leader's own call is an explicit
order and holds against the role until the leader gives another, or is dead when the objective flips. */
enum { AE_ROLE_NONE, AE_ROLE_DEFEND_OURS, AE_ROLE_PUSH_THEIRS, AE_ROLE_ROVE, AE_ROLE_SITE_A, AE_ROLE_SITE_B, AE_ROLE_ENTRY,
	AE_ROLE_HILL, AE_ROLE_APPROACH, AE_ROLE_PROTECT, AE_ROLE_HUNT, AE_ROLE_POWER, AE_ROLE_ROAM, AE_ROLE_COUNT };
/* the modes that have roles (the engine's side names the mode of the game and, in SnD, the team's side) */
enum { AE_ROLE_MODE_NONE, AE_ROLE_MODE_CTF, AE_ROLE_MODE_SND_DEFENDERS, AE_ROLE_MODE_SND_ATTACKERS, AE_ROLE_MODE_KING,
	AE_ROLE_MODE_ODDBALL, AE_ROLE_MODE_SLAYER, AE_ROLE_MODE_COUNT };
#define AE_ROLE_TABLE_MAX 4

/* the mode's roles, in the order they are handed out; the count (0 for no roles) */
int ae_fireteam_role_table(int mode, short *roles);
/* the standing order a role is: defend, attack, watch, or none for a role the bots' own goals already serve */
short ae_fireteam_role_order_kind(int role);
/* the role of a fireteam in the mode: its place among its team's fireteams and its wipes read the table round-robin;
AE_ROLE_NONE for a free fireteam or a mode without roles */
int ae_fireteam_role_of(struct ae_fireteam_table const *table, int fireteam, int mode);
/* A TEAM'S ROLE MIX (owner 11-05: team 1 ended with 4 DEFEND, 2 ROVE and no PUSH, each squad reading the table at its own
turn): the roles of the team's N bot-led squads (human-led squads are not counted) are the first N places of the mode's table
read round and round, so the mix is fixed by N: CTF with 5 squads is 2 DEFEND, 2 PUSH, 1 ROVE, and with two or more squads
there is always an attacking role where the mode has one. Fills roles[] (at least AE_FIRETEAM_MAX long); returns N or 0 */
int ae_fireteam_role_mix(int mode, int squads, short *roles);
/* how many of the team's squads are bot-led (the mix's N) */
int ae_fireteam_bot_squads(struct ae_fireteam_table const *table, short team);
/* a wipe swaps the squad's place in the mix with the next bot squad's: the mix of the team stays as it was */
/* the squad was wiped out: its next role is the table's next one */
void ae_fireteam_role_wiped(struct ae_fireteam_table *table, int fireteam);
/* 1 when the role should set the fireteam's order now: it has none (a spent REGROUP or GOING, a fresh squad) or it is a role's
own (re-issued on an event). An explicit order of a leader holds (0), except that a dead leader's ends when the objective
flipped (the order is dropped and 1 returned) */
int ae_fireteam_role_wants_order(struct ae_fireteam_table *table, int fireteam, int leader_alive, int flipped);
/* set the order a role is (not explicit). point may be NULL (a role without a point has no order) */
void ae_fireteam_set_role_order(struct ae_fireteam_table *table, int fireteam, int role, float const *point);
int ae_fireteam_order_is_explicit(struct ae_fireteam_table const *table, int fireteam);
int ae_fireteam_role(struct ae_fireteam_table const *table, int fireteam);

/* 1 when the table is consistent: the counts match the bots, every bot's fireteam is in use */
int ae_fireteam_check(struct ae_fireteam_table const *table);

/* STRICT FOLLOW AND FORMATION (owner 11-05, "fire team should not run off at all"; REGROUP "doesn't stick in formation"; 11-07
"they need to be more sticky and closer to you", "tighter but use formations real expert soldiers would use"): the members of
a squad a person leads keep inside the follow radius of the leader at all times (AE_FIRETEAM_FOLLOW_RADIUS, 6 wu; a FILE's
column reaches further, ae_fireteam_formation_radius). They fight from where they stand (shoot, strafe inside the radius),
never chase out of it, and take no item, vehicle or objective detour while FOLLOW or REGROUP stands. Each holds a SLOT, by
member index, of the squad's shape, turned with the leader's facing; the facing the slots are read at moves only when the
leader's turns past AE_FIRETEAM_RESLOT_TURN (radians), so they do not swim with every twitch of the aim. A member within
AE_FIRETEAM_FOLLOW_TOLERANCE of its slot stands; REGROUP (held too, once formed) holds it to AE_FIRETEAM_REGROUP_TOLERANCE.

THE SHAPES, a fire team's: a WEDGE in open ground, the leader at its point and the members echeloned back right then left,
AE_FIRETEAM_FORMATION_SIDE across and AE_FIRETEAM_FORMATION_BACK back a rank (6 m and 4.5 m: one grenade does not take two),
a fifth member trailing on the centre line behind the first two ranks; a FILE in tight ground (corridors, ramps, doors,
under roofs), one column behind the leader AE_FIRETEAM_FILE_GAP apart. The ground is read at the leader: tight under a roof
within AE_FIRETEAM_TIGHT_ROOF, or with AE_FIRETEAM_TIGHT_WALLS of eight level rays at chest height meeting a wall within
AE_FIRETEAM_TIGHT_WALL; read every AE_FIRETEAM_SHAPE_PROBE_SECONDS, the shape changes only when the new reading has held
AE_FIRETEAM_SHAPE_HOLD_SECONDS (a doorway passed does not fold the wedge). */
#define AE_FIRETEAM_FOLLOW_RADIUS 6.0f
#define AE_FIRETEAM_FOLLOW_EDGE 1.5f		/* inside the radius by this: a fight's step further out is stopped */
#define AE_FIRETEAM_FORMATION_SIDE 2.0f
#define AE_FIRETEAM_FORMATION_BACK 1.5f
#define AE_FIRETEAM_FILE_GAP 1.75f
#define AE_FIRETEAM_SLOT_ROOM 1.0f		/* the radius reaches at least this far past the farthest slot */
#define AE_FIRETEAM_FOLLOW_TOLERANCE 3.0f
#define AE_FIRETEAM_REGROUP_TOLERANCE 1.5f
#define AE_FIRETEAM_RESLOT_TURN 0.45f
#define AE_FIRETEAM_TIGHT_ROOF 8.0f
#define AE_FIRETEAM_TIGHT_WALL 3.0f
#define AE_FIRETEAM_TIGHT_WALLS 5
#define AE_FIRETEAM_SHAPE_PROBE_SECONDS 0.5f
#define AE_FIRETEAM_SHAPE_HOLD_SECONDS 1.0f
enum { AE_FORMATION_WEDGE, AE_FORMATION_FILE };
/* the slot of member 0, 1, 2.. of the shape (offsets in the world: x, y) for a leader facing (radians) */
void ae_fireteam_formation_slot(int shape, int index, float facing, float *dx, float *dy);
/* the follow radius of a squad of this many members in the shape: AE_FIRETEAM_FOLLOW_RADIUS, or its farthest slot and
AE_FIRETEAM_SLOT_ROOM when that is further (a long FILE) */
float ae_fireteam_formation_radius(int shape, int members);
/* the facing the slots are read at: the held one, until the leader's now differs by the re-slot turn */
float ae_fireteam_formation_facing(float held, float now_facing, int first);
/* the ground at the leader from the probe: roof (a roof within the roof reach), walls_near (level rays meeting a wall within
the wall reach, of eight): 1 tight, 0 open */
int ae_fireteam_tight(int roof, int walls_near);
/* a fireteam's shape and the reading that would change it, since when (seconds) */
struct ae_fireteam_shape
{
	short shape;
	short reading;
	float reading_since;
};
void ae_fireteam_shape_clear(struct ae_fireteam_shape *state);
/* a probe's reading (tight or not) at now_seconds: the shape changes once a different reading has held the hold time; first
(the squad formed afresh) takes the reading at once. 1 when the shape changed */
int ae_fireteam_shape_note(struct ae_fireteam_shape *state, int tight, float now_seconds, int first);
enum { AE_TETHER_FREE, AE_TETHER_GO, AE_TETHER_HOLD, AE_TETHER_STOP };
/* what a tethered member does this tick. leader_distance and slot_distance in wu; fighting; outward_cosine: the cosine between
its walk and the way away from the leader (1 straight out; 2 when it does not walk); tolerance: the order's; radius: the
squad's follow radius (ae_fireteam_formation_radius). GO: walk to the slot (out of the radius: whatever it fights); HOLD:
stand in the slot; STOP: a fight's step outwards is cancelled; FREE: the fight's own move */
int ae_fireteam_tether(float leader_distance, float slot_distance, int fighting, float outward_cosine, float tolerance,
	float radius);

/* LOOSE LEASH (owner 11-07): in an arena game a person's squad with no explicit order plays its role but stays near the
person: a member farther than AE_FIRETEAM_LOOSE_RADIUS from the living leader, not fighting, not carrying an objective and not
seated, walks back toward the leader; inside it, it is free. Nonzero: walk back */
#define AE_FIRETEAM_LOOSE_RADIUS 25.0f
int ae_fireteam_loose_wanted(float leader_distance, int fighting, int carrying, int seated);

/* A DEFEND OR WATCH ANCHOR is stood at, not walked to (owner log 11-05, Blood Gulch: 327 of 489 bot-squad stucks were at one flag
stand, three squad mates each within 1.6 wu, every one still walking at the very point): a member inside AE_FIRETEAM_SETTLE_RADIUS of
its anchor point, or inside the anchor radius with AE_FIRETEAM_SETTLE_CROWD or more bots (any squad) close about it (a crowd at the
point), settles: its walk goal is let go and it stands (a fight still moves it). Nonzero: settled */
#define AE_FIRETEAM_SETTLE_RADIUS 3.5f
#define AE_FIRETEAM_SETTLE_CROWD 2
int ae_fireteam_settled(short kind, float position_distance, int mates_near);

#endif
