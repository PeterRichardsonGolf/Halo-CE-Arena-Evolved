/*
AE_ROUND_INTERNAL.H

What the round modes' three game files share (ae_round.c, ae_round_zone.c, ae_round_net.c): the host's state and the
view both kinds of machine read (the host's own, a client's from message 106).
*/
#ifndef AE_ROUND_INTERNAL_H
#define AE_ROUND_INTERNAL_H

#include "ae_round.h"
#include "ae_round_rules.h"
#include "ae_round_wire.h"
#include "ae_round_zone_rules.h"

enum
{
	/* the per-player slots: absolute player indices */
	AE_ROUND_SLOTS = AE_ROUND_WIRE_PLAYERS,
};

struct ae_round_slot
{
	long player_index;          /* the player in this slot, NONE none */
	short state;                /* enum ae_round_player_state */
	boolean in_round;           /* there when the round began (else WAITING) */
	boolean action_held;        /* this tick (ae_round_note_action) */
	boolean zone_death;         /* the zone's damage is killing them: no revive spot */
	boolean zone_capped;        /* outside the zone: no shield or health back past what its damage left */
	real zone_shield, zone_body;
	long deaths_seen;
	long deaths_base;           /* deaths when the round began, and one more for each revive */
	real_point3d last_position; /* while alive */
	real_point3d spot;          /* DOWN's */
	short revives;              /* this round */
	long progress;              /* ticks of the revive under way */
	long reviver;               /* the player reviving, NONE */
	long down_since;            /* the tick they went down (BLEEDOUT counts from it) */
	long wins;                  /* rounds won (no teams) */
	long kills_base;            /* the kills the player had when the round began (EXTERMINATION's tally) */
	boolean paid;               /* ATTRITION: died with a life from the pool, waits to respawn */
	short dev_down;             /* a dev command's down as they stand up: 1, 2 beside the host's player */
};

struct ae_round_host
{
	boolean running;
	long last_time;
	short phase;                /* enum ae_round_phase */
	short round;                /* 1 up */
	short played;
	long round_start, live_start, post_end;
	long team_wins[AE_ROUND_SIDES];
	short last_result;          /* a side, AE_ROUND_RESULT_NONE or _TIE */
	boolean teams, switched, revive, zone;
	short kind;                 /* enum ae_round_kind */
	int pool[2], pool_start[2];   /* ATTRITION: each team's lives left, and at the round's start */
	boolean last_wipe;          /* the round just judged ended by a team wiped (not the clock) */
	boolean replayed, replay_next; /* (unused since 0.7.8: a time-out is a tie, no replay) */
	short timeout_ties;         /* EXTERMINATION: rounds of this match tied by a time-out */
	long match_kills[2];        /* EXTERMINATION: each team's kills over the match's finished rounds */
	short kills_winner;         /* EXTERMINATION: the team the kills decided the match for after the ties, else -1 */
	boolean zone_triggered;     /* ATTRITION: the zone's clock has begun */
	short max_rounds, to_win;
	struct ae_round_slot slots[AE_ROUND_SLOTS];

	/* the zone this round */
	boolean zone_disabled;      /* dev: zone off */
	boolean zone_ready;
	real_point3d zone_centre;
	struct ae_zone_plan zone_plan;
	long zone_start;            /* the tick its timing counts from (LIVE's start) */
	short zone_phase;
	unsigned long random;

	/* the state as sent (ae_round_net.c) */
	struct ae_round_wire view;
	long sent_at;
	int sent_size;
	unsigned char sent[AE_ROUND_WIRE_BUFFER];
};

/* ae_round_pool_spend writes an int: a narrower pool let it write over the next team's (the lives never ran out) */
typedef char ae_round_pool_is_int[sizeof(((struct ae_round_host *)0)->pool[0]) == sizeof(int) &&
	sizeof(((struct ae_round_host *)0)->pool_start[0]) == sizeof(int) ? 1 : -1];

extern struct ae_round_host ae_round_host;

/* the state this machine reads: the host's own view, or a client's last message (NULL before one) */
struct ae_round_wire const *ae_round_view(void);
/* the slot of a player (absolute index), NULL for none */
struct ae_round_slot *ae_round_slot_of(long player_index);
/* a living player's unit (NONE dead or none) */
long ae_round_living_unit(long player_index);

/* ae_round_zone.c: the round's zone chosen (at its start), its LIVE tick (phases, damage), its radius now, whether a
point is inside, the zone's starting now (dev) */
void ae_round_zone_new_round(void);
void ae_round_zone_tick(long now);
real ae_round_zone_radius(long now);
boolean ae_round_zone_inside(real_point3d const *point, long now);
void ae_round_zone_start_now(long now, boolean small);

/* ae_round_net.c: the view built from the host's state, and sent when it changes (every two seconds anyway) */
void ae_round_net_host_tick(long now);

/* the kills a side (0 red, 1 blue) made this round: EXTERMINATION's time-out tally */
int ae_round_net_side_kills(long side);

/* the host's random numbers (the zone's picks): 0 to count - 1 */
long ae_round_random(long count);

/* an event of the dev stream, and a line in debug.txt (a TEST RUN's expect reads both) */
void ae_round_log(char const *format, ...);

#endif
