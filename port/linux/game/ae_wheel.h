/*
AE_WHEEL.H

The CALLOUT WHEEL's input (ae_wheel.c): the mark button held opens it, the stick or the mouse picks an entry, the
release sends it, and the view holds still while it is open. The rules are ae_wheel_rules.c's. No drawing here: the
wheel is drawn from ae_wheel_open, ae_wheel_focus and ae_wheel_open_part. Include after cseries.h and math/real_math.h.

The button: on the pad it is the left bumper (white), free in the two AE layouts it is built for, whose
flashlight is on the d-pad; on the keyboard it is the mark key. Other layouts keep the flashlight button's mark
(ae_mark_rules.h) and have no wheel.
*/

#ifndef __AE_WHEEL_H
#define __AE_WHEEL_H

/* the input blob's player control flags past the engine's (ae_zoom.h has 12 and 13): the left bumper is down */
#define AE_MARK_PAD_BIT 14

/* (player_control.c, where the blob's flashlight bit is set) records whether the pad's left bumper is down */
void ae_wheel_note_pad(unsigned long *player_control_flags, int gamepad_index);
/* whether the layout leaves the left bumper to the mark and the wheel */
int ae_wheel_own_button(short button_preset);
/* whether this local player's layout does (their profile's) */
int ae_wheel_pad_owns(short local_player_index);

/* (player_control.c, once the blob is final) while the wheel is open the view holds still: only the facing is zeroed,
so moving, firing, grenades, melee, jump and the zoom's button stay live */
void ae_wheel_filter(short local_player_index, real_euler_angles2d *facing_delta);

/* (ae_marks.c, a new game) every press forgotten; one held across is let go of before it counts */
void ae_wheel_reset(void);

/* (ae_marks.c, each tick) one tick of the button. pad_down: the left bumper, for a layout that owns it; key_down: the
mark key, of the keyboard's player; keyboard: this is the keyboard's player (the mouse's counts are theirs);
interrupted: the wheel cannot be (a menu, the scoreboard, a cinematic, or this tick's reset); dead: the player has no
unit. Returns AE_PRESS_*; on AE_PRESS_SEND, *entry is the call to send. A change of dead while a press is down ends it. */
int ae_wheel_step(short local_player_index, int pad_down, int key_down, int keyboard, int interrupted, int dead, long now,
	int *entry);

/* (the draw) whether the wheel is open for this local player, which entry is focused (AE_WHEEL_NONE for none),
and how far its 80 ms opening has come, 0 to 1 */
boolean ae_wheel_open(short local_player_index);
int ae_wheel_focus(short local_player_index);
real ae_wheel_open_part(short local_player_index);
/* (the draw) whether the open wheel is a dead player's (three entries) */
boolean ae_wheel_is_dead(short local_player_index);
/* (the draw) a send's flash and closing, after the wheel is no longer open: the entry sent, whether it was a dead
player's, the flash to white (0 to 1) and the ring's alpha; FALSE once it is over (ae_wheel_layout.h's motion) */
boolean ae_wheel_closing(short local_player_index, int *entry, boolean *dead, real *flash, real *alpha);
/* calls this local player has sent from the wheel, kept in their profile and counted to AE_WHEEL_LEGEND_CALLS (the legend of
hints goes after that many) */
int ae_wheel_calls_sent(short local_player_index);

/* (xinput_sdl.c's mouse read, through ae_platform.h) the mouse's raw counts of this read: no sensitivity */
void ae_wheel_note_mouse(float x, float y);

#endif
