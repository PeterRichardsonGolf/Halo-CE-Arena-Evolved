/*
AE_MARK_RULES.H

MARKS (ae_marks.c): the press, the mark's life and its place on the screen, as
pure rules (port/linux/tests/ae_mark_rules_test.c runs them).
*/

#ifndef __AE_MARK_RULES_H
#define __AE_MARK_RULES_H

#include <wchar.h>

enum
{
	AE_MARK_KIND_ENEMY,
	AE_MARK_KIND_ITEM,
	AE_MARK_KIND_PLACE,
	AE_MARK_KIND_COUNT
};

/* all in game ticks, 30 a second */
#define AE_MARK_HOLD_TICKS 8	/* a press this long on the shared button is the flashlight's */
#define AE_MARK_APPEAR_TICKS 4
#define AE_MARK_FADE_AT_TICKS 132
#define AE_MARK_LIFE_TICKS 150

struct ae_mark_press
{
	int down;
	int taken;
	int held;
	long since;
};

/* a tick of the button the mark shares with the flashlight: down, whether it is down; taken, that a press starting
now belongs to something else (the zoom's steps). *light says whether the flashlight sees the button this tick.
Returns 1 on the tick a tap ends: mark. */
int ae_mark_press_step(struct ae_mark_press *press, int down, int taken, long now, int *light);

/* how opaque a mark of this age is, 0 to 1: 0 when it is over */
float ae_mark_alpha(long age);
/* its size against its resting size: it settles from a little larger as it appears */
float ae_mark_appear_scale(long age);

/* x and y from the view's middle: when off_screen, or outside the ellipse of these radii, moves them along their
direction onto it and returns 1. Marks stay in view for their short life. */
int ae_mark_clamp(float *x, float *y, float radius_x, float radius_y, int off_screen);

/* whether a new press means the marker's own mark: same object, or a place this near (squared world units) */
int ae_mark_same(int kind_a, long object_a, int kind_b, long object_b, float distance_squared);

/* (the host) whether a mark a client sends may stand: all kinds within this many world units of the marker's head, and an
enemy only with a clear line to it. distance_squared from the marker's head to the spot. */
#define AE_MARK_HOST_REACH 100.0f

int ae_mark_host_allows(int kind, float distance_squared, int line_clear);

/* (the host) whether a line to the spot is to be tested at all: an enemy's, and only within range. A distance out of range,
infinite or not a number never gets a ray. */
int ae_mark_host_line_wanted(int kind, float distance_squared);

/* whether a spot a client sent is three finite numbers */
int ae_mark_spot_finite(float x, float y, float z);

/* the spam guard: marks inside the window; the warning's mark is let through, the limit's and those after are not */
#define AE_MARK_SPAM_LIMIT 15
#define AE_MARK_SPAM_WARN_AT 13
#define AE_MARK_SPAM_WINDOW_TICKS 600
#define AE_MARK_SPAM_BLOCK_TICKS 900

enum { AE_MARK_SPAM_OK, AE_MARK_SPAM_WARN, AE_MARK_SPAM_NOW_BLOCKED, AE_MARK_SPAM_STILL_BLOCKED };

struct ae_mark_spam
{
	long times[AE_MARK_SPAM_LIMIT];
	int next;
	int used;
	long blocked_until;
	int blocked;
};

/* a new mark at this tick: what the guard says (OK and WARN: it may go) */
int ae_mark_spam_take(struct ae_mark_spam *spam, long now);

/* CALLS (ae_mark_calls.c): what a message of the call kind says. A press and the wheel make marks; the four answers are
a teammate's (OK, NEVER MIND, DIBS) and the marker's own (CANCEL) */
enum { AE_CALL_MARK, AE_CALL_OK, AE_CALL_NEVER_MIND, AE_CALL_DIBS, AE_CALL_CANCEL, AE_CALL_WHEEL };

/* each OK adds this much life to the mark, to no more than this from its birth */
#define AE_MARK_OK_TICKS 90
#define AE_MARK_LIFE_MOST_TICKS 300
/* a double tap is the tap's call made again: free of the guard's count only this soon after a counted mark */
#define AE_MARK_DOUBLE_FREE_TICKS 45

/* whether an answer may stand. mark_active: the mark is there; same_team: the answerer is on the marker's team (for CANCEL:
is the marker); already_said: the answerer's OK stands (for DIBS: someone has dibs). OK and DIBS once; DIBS on an item
that has its object; NEVER MIND only after an OK. */
int ae_mark_reply_allowed(int verb, int mark_active, int same_team, int mark_kind, int has_object, int already_said);

/* the life a mark has after an OK at now: 90 ticks more, to 300 from birth; a mark already over keeps the life it had */
long ae_mark_life_after_ok(long life_ticks, long born, long now);

/* the kind a build without calls is sent for a wheel entry (AE_WHEEL_*): -1 for the entry that keeps the cast's own */
int ae_mark_old_build_kind(int entry);

/* a mark's age as ae_mark_alpha reads it, for a mark whose life is life_ticks (0: the fixed one): it fades at its own end */
long ae_mark_effective_age(long age, long life_ticks);

/* how many times over a call is made again: the same call steps up, twice at most */
int ae_mark_repeats_after(int same, int previous);

/* (the host) whether a double tap rides the count of the tap before it. counted: the tick of the last counted mark, plus one
(0 for none) */
int ae_mark_double_free(long counted, long now);

/* TARGETS (ae_marks.c): what a press takes when several are in range. The best tier in range wins; among equals, the one
nearest the aim. A hit on a low tier does not beat a better tier in range. */
enum { AE_TIER_ENEMY = 1, AE_TIER_BODY, AE_TIER_MARKER, AE_TIER_NAMED, AE_TIER_PICKUP, AE_TIER_GROUND };

/* whether candidate a beats candidate b: a lower tier number, else (the same tier) a smaller part (how far off the aim, in
reaches). A body (AE_TIER_BODY) against an objective, a vehicle or a pickup (AE_TIER_NAMED, AE_TIER_PICKUP) goes by part alone.
A tier of 0 is no candidate: anything beats it. A tie keeps b. */
int ae_mark_candidate_better(int tier_a, float part_a, int tier_b, float part_b);

/* what a press on a marker does: your own, CANCEL; a teammate's, NEVER MIND after your OK, else DIBS on an item mark nobody
has dibs on, else OK */
int ae_mark_press_verb(int own_marker, int said_ok, int item_mark, int dibs_taken);

/* whether a target is the player's own mark's: as ae_mark_same, but an enemy only while it stands near where it was marked
(a mark does not follow; one who has moved is a new mark). distance_squared: from the mark's spot to the target's */
int ae_mark_press_is_own(int kind_a, long object_a, int kind_b, long object_b, float distance_squared);

/* a mark's serial, the host's count of a player's marks: a reply names the mark it answers by it, and only while that mark
lasts, so a late one does not land on the mark that replaced it. A cancel is the owner's, whose messages keep their order */
#define AE_MARK_SERIAL_MASK 63
int ae_mark_serial_next(int serial);
int ae_mark_reply_is_for(int verb, int reply_serial, int mark_serial, long ends, long now);
/* a mark's message holds its repeats (0 to 2, more reads as 2) and its serial in one byte */
int ae_mark_wire_counts(int repeats, int serial);
int ae_mark_wire_repeats(int counts);
int ae_mark_wire_serial(int counts);

/* ENEMY MARKS THROUGH WALLS, the gametype's (GAME OPTIONS > HUD & AIDS): universal_variant.ball_melee's bit 7, set = on, so
stock and older gametypes read as off. The byte's other bits (BALL MELEE, SPAWNS, the weapon slots' rank) are kept */
#define AE_MARKS_WALLS_BIT 0x80
int ae_marks_walls_of(int ball_melee_byte);
int ae_marks_walls_set(int ball_melee_byte, int on);

/* network.marks, the host's: a byte of its chat settings' message. An older host sends 0 there, which reads as on */
int ae_mark_rule_flags(int marks_on);
void ae_mark_rule_read(int pad_byte, int *marks_on);

/* with through walls off: a teammate's enemy mark is seen from this view by a ray each 3 ticks, and held 0.3 s after the
last that met nothing. tested_at and seen_at are game ticks, -1 for never */
#define AE_MARK_SIGHT_TEST_TICKS 3
#define AE_MARK_SIGHT_HOLD_TICKS 9
int ae_mark_sight_due(long tested_at, long now);
int ae_mark_sight_held(long seen_at, long now);

/* a place's id, the same on every machine and on the network: 0 for the distance alone; each base; each power item, by
its class (rockets, sniper, shotgun, overshield, camo) and the side named before it (0 none, 1 red, 2 blue) */
enum { AE_MARK_PLACE_NONE, AE_MARK_PLACE_RED_BASE, AE_MARK_PLACE_BLUE_BASE, AE_MARK_PLACE_ITEM_FIRST };
#define AE_MARK_PLACE_ITEM_CLASSES 5
#define AE_MARK_PLACE_BASE_REACH 12.0f	/* world units: a base's own items and roof read as the base */
#define AE_MARK_PLACE_ITEM_REACH 6.0f
struct ae_mark_place_point
{
	int id;
	float x, y, z;
};
int ae_mark_place_item_id(int item_class, int side);
/* the place's name in capitals; 0 and an empty name for 0, or for an id from nowhere */
int ae_mark_place_name(int id, wchar_t *name, int size);
/* the place a spot is called: a base in its range, the nearer of two, else the nearest item in its range, else 0 */
int ae_mark_place_pick(struct ae_mark_place_point const *points, int count, float x, float y, float z);

#endif
