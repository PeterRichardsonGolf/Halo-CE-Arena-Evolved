/*
AE_MARKS.H

MARKS: a press marks the spot, enemy or item aimed at (ae_marks.c).
*/

#ifndef __AE_MARKS_H
#define __AE_MARKS_H

#include <wchar.h>

/* player_control.c's tick, after the zoom's filter: reads the press (a tap marks, a double tap and a hold are the wheel's), and hides a tap from the flashlight */
void ae_marks_input(short local_player_index, unsigned long player_control_flags, unsigned long *unit_control_flags,
	int light_bit);

/* a call of the callout wheel (ae_wheel.h), sent by the release of its button: this machine's mark at once, and the host's
(a client's goes to the host, which checks it and sends it on to the team) */
void ae_marks_wheel_send(short local_player_index, int entry);

/* a press on a mark: verb is AE_CALL_OK, AE_CALL_NEVER_MIND, AE_CALL_DIBS or AE_CALL_CANCEL (ae_mark_rules.h), mark_player the
absolute index of whose mark. FALSE when the rules refuse it (the mark's gone, not a teammate's, said already...). The host and
a local game take it at once, a client's goes through the host: its answer shows when the host's comes back. */
int ae_marks_answer(short local_player_index, int verb, int mark_player);

/* what a press would take, by tier, looked at again every 3 ticks per local player: ae_marks_aim (ae_mark_targets.h) */

/* what a mark carries, for drawing it and for the prompt */
struct ae_mark_detail
{
	int kind;				/* AE_MARK_KIND_* */
	int entry;				/* AE_WHEEL_*: the call it is; a plain mark is AE_WHEEL_SMART */
	int repeats;			/* the same call made again, 0 to 2 */
	int timer_plus_one;		/* a fact's timer: the power item's place among the item timers, plus one; 0 for none */
	int place;				/* a place's id; 0 for the distance alone */
	int dibs;				/* the absolute index of who has dibs, or -1 */
	unsigned short responders;	/* who has said OK: a bit for each absolute player index */
	long team_index;
	int muted_here;			/* this machine has the marker muted: keep it from the screen and the sound */
	long born;
	long ends;				/* the tick it ends at */
	float x, y, z;
};
/* the host's marks on or off (network.marks); the gametype's ENEMY MARKS THROUGH WALLS */
boolean ae_marks_rule_on(void);
boolean ae_marks_rule_through_walls(void);

/* an item's or vehicle's name as a mark's row has it (the item prompt's too): its tag's own name in capitals, "fuel rod gun", "oddball",
no multiplayer word. Empty for an object that is not an item or a vehicle. At most size - 1 characters */
void ae_marks_item_label(long object_index, wchar_t *name, int size);

/* whether this player's mark is there, and its detail */
int ae_marks_get_detail(long mark_player, struct ae_mark_detail *detail);

/* game_engine.c's update, on the host and local games: a new game's reset and taken items' clearing */
void ae_marks_update(void);

/* hud.c, after the names: each mark of this view's team as a marker (ae_marker_draw.h), with its facts, its answers and the aim
prompt's row when it is the one aimed at */
void ae_marks_draw(short local_player_index);

/* the network's: what crosses (NETCODE.md's distributed messages; a build without marks drops both as unknown) */
unsigned short ae_marks_entry_size(void);
/* (the host) a client's mark, or its clearing: checked, kept and sent on to the marker's team */
void ae_marks_handle_up(long machine_index, void const *entries, int from_stream);
/* (a client) a teammate's mark from the host */
void ae_marks_handle_down(void const *entries, int from_stream);

/* messages 100 and 101 (ae_mark_calls.c): a build without them drops both as unknown, and has the plain mark on 98 and 99 */
unsigned short ae_mark_calls_entry_size(void);
/* (the host) a client's call or answer: checked, kept and sent on to the team */
void ae_mark_calls_handle_up(long machine_index, void const *entries, int from_stream);
/* (a client) a teammate's call or answer, from the host */
void ae_mark_calls_handle_down(void const *entries, int from_stream);

#endif
