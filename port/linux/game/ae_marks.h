/*
AE_MARKS.H

MARKS: a press marks the spot, enemy or item aimed at (ae_marks.c).
*/

#ifndef __AE_MARKS_H
#define __AE_MARKS_H

/* player_control.c's tick, after the zoom's filter: reads the press, and hides a tap from the flashlight */
void ae_marks_input(short local_player_index, unsigned long player_control_flags, unsigned long *unit_control_flags,
	int light_bit);

/* game_engine.c's update, on the host and local games: a new game's reset and taken items' clearing */
void ae_marks_update(void);

/* hud.c, after the names */
void ae_marks_draw(short local_player_index);

/* the network's: what crosses (NETCODE.md's distributed messages; a build without marks drops both as unknown) */
unsigned short ae_marks_entry_size(void);
/* (the host) a client's mark, or its clearing: checked, kept and sent on to the marker's team */
void ae_marks_handle_up(long machine_index, void const *entries, int from_stream);
/* (a client) a teammate's mark from the host */
void ae_marks_handle_down(void const *entries, int from_stream);

#endif
