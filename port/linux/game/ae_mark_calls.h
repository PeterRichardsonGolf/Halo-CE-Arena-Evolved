/*
AE_MARK_CALLS.H

CALLS (ae_mark_calls.c): the wheel's calls and the answers to a mark, through the host. Messages 100 (a client to the host)
and 101 (the host to the marker's team) carry them; 98 and 99 keep carrying the plain mark for builds without calls.
Include after cseries.h, math/real_math.h and game/players.h.

What the two files share is here: ae_marks.c keeps the marks and the checks on a spot, ae_mark_calls.c the messages and
the answers.
*/

#ifndef __AE_MARK_CALLS_H
#define __AE_MARK_CALLS_H

#define MARK_PLAYERS 16
#define MARK_NAME_SIZE 24
#define MARK_ABOVE 0.3f
#define MARK_CLEAR AE_MARK_KIND_COUNT

/* (the words after HERE are newer than the plain messages 98 and 99: an older build drops a word it does not know, so those carry
HERE, and the detail on 100 and 101 the word) */
enum
{
	MARK_WORD_ENEMY, MARK_WORD_WEAPON, MARK_WORD_VEHICLE, MARK_WORD_ITEM, MARK_WORD_HERE,
	MARK_WORD_FIGHT, MARK_WORD_HILL, MARK_WORD_TELEPORTER, MARK_WORD_BOMB_SITE,
	MARK_WORD_COUNT
};

/* entry: the wheel's entry (AE_WHEEL_*) the mark is of, or NONE for a plain mark whose detail has not come */
struct mark
{
	boolean active;
	short kind;
	short word;
	long object_index;
	long team_index;
	long born;
	real_point3d spot;
	wchar_t name[MARK_NAME_SIZE];
	unsigned short responders;	/* who has said OK: a bit for each absolute player index */
	signed char dibs;			/* the absolute index of who has dibs, or -1 */
	short entry;
	short repeats;				/* the same call made again: 0, 1 or 2 */
	short timer_plus_one;		/* a fact's timer: the power item's place among the item timers, plus one; 0 for none */
	short place;				/* a place's id; 0 for the distance alone */
	short serial;				/* the host's count of this player's marks: which mark a reply is for */
	long ends;					/* the tick the mark ends at; 0 for born + the fixed life */
};

/* what crosses on 100 and 101. player_index: who acts (the marker for a mark; who answers for an answer); target_player:
whose mark an answer is for, and for a mark how many times over it has been made. kind and word are a mark's, with the spot.
Entry and kind are what the sender means; the host takes the kind of a wheel entry from ae_mark_old_build_kind. */
struct distributed_call
{
	byte player_index;
	byte verb;
	byte entry;
	byte target_player;
	byte timer_plus_one;
	byte place;
	byte kind;
	byte word;
	float x, y, z;
};

/* ---------- ae_marks.c's, for ae_mark_calls.c */

struct mark *ae_marks_slot(long absolute);
/* a slot with nothing in it (dibs is -1, not 0) */
void ae_marks_clear_slot(struct mark *mark);
void ae_marks_item_name(struct mark *mark, long object_index);
void ae_marks_tone(short kind);
boolean ae_marks_heard_here(long team_index);
boolean ae_marks_muted_here(struct player_datum const *player);
boolean ae_marks_death_spot(struct player_datum const *player, real_point3d *spot);
/* (the host) the object of the kind nearest to the spot, or NONE */
long ae_marks_find(struct player_datum const *player, short kind, real_point3d const *spot);
/* (the host) whether the marker's head reaches the spot; an enemy needs a line, to the object found or else to the point */
boolean ae_marks_host_reach(struct player_datum const *player, short kind, real_point3d const *spot, long found);
/* (the host) whether the guard lets the marker's call go; a double tap is free just after the tap before it */
boolean ae_marks_host_guard(long absolute, boolean double_tap);
/* a mark has come to this machine (made here, or sent by the team): the feed's line for each local player on its team */
void ae_marks_feed(long absolute, struct mark const *mark);
/* the power item's spawn a place at this spot is, as its place among the item timers plus one; 0 for none (and in a game with
no timers) */
short ae_marks_timer_for(real_point3d const *spot);
/* (the host) the client machines with a player on the team */
short ae_marks_team_machines(long team_index, long *machines, short maximum);
/* the plain mark, or its clearing, as the older builds have it (messages 98 and 99); the host to its team, a client to the host */
void ae_marks_share_plain(long absolute, short kind, short word, real_point3d const *spot, long team_index);
void ae_marks_share_clear(long absolute, long team_index);

/* ---------- ae_mark_calls.c's, for ae_marks.c */

/* a new game */
void ae_mark_calls_reset(void);
/* each tick: marks over, and answers from players who left or changed team, are let go */
void ae_mark_calls_tick(void);
/* a mark of this player's is made here (verb AE_CALL_MARK or AE_CALL_WHEEL, with the wheel's entry): the host sends it to its
team, a client to the host (a tap on the plain messages alone) */
void ae_mark_calls_share(long absolute, int verb, struct mark const *mark);
/* (the host) the detail of a mark made some other way (the plain messages), to its team */
void ae_mark_calls_send_detail(long absolute, struct mark const *mark);
/* whether the host has sent detail on 101: then its plain marks (99) are the same marks, and are let be */
boolean ae_mark_calls_detail_seen(void);

#endif
