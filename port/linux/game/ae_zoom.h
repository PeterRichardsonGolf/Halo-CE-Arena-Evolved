/*
AE_ZOOM.H

The zoom action's hold mode of Arena Evolved, as a pure state machine (no engine
or config calls: port/linux/tests/ae_zoom_test.c runs it). input.zoom_mode picks
how the zoom action works, for the mouse and the gamepad alike:

  TOGGLE  each press steps the zoom level, as Halo does (the engine's own code
          runs; ae_zoom_step is its model here, never called in this mode)
  HOLD    the press zooms to the first level, the release returns to unzoomed;
          no second level
  BOTH    a tap (released within the hold time) steps the level as TOGGLE does;
          a hold zooms to the first level when it began unzoomed, keeps the
          level when it began zoomed, and the release returns to unzoomed

Anything that forces the zoom off while the action is down (a weapon switch, a
reload, a melee, a hit, a vehicle, a death) ends the press: its release does
nothing, and the next press is a fresh one. A press that cannot act when it is
made (the weapon reloading, no camera control) is also left alone until its
release.

The zoom level is the engine's: -1 unzoomed, 0 the first level, up to count - 1
(count is the weapon's zoom level count).
*/

#ifndef __AE_ZOOM_H
#define __AE_ZOOM_H

enum
{
	AE_ZOOM_TOGGLE = 0,
	AE_ZOOM_HOLD = 1,
	AE_ZOOM_BOTH = 2
};

#define AE_ZOOM_HOLD_TIME_DEFAULT 0.25
#define AE_ZOOM_HOLD_TIME_MINIMUM 0.1
#define AE_ZOOM_HOLD_TIME_MAXIMUM 1.0

struct ae_zoom_state
{
	int phase;
	int was_held;
	long press_ms;
	short start_level;
	short expected;
	/* the press has been a hold from the start or has become one (a step, or the tap time gone) */
	int promoted;
	/* HOLD: a tap was released at tap_release_ms on the weapon tap_key (a double press follows it) */
	int tap_valid;
	long tap_release_ms;
	long tap_key;
};

/* input.zoom_reset: where a hold from unzoomed starts */
enum
{
	AE_ZOOM_RESET_FIRST = 0,
	AE_ZOOM_RESET_LAST = 1
};

/* what ae_zoom_step_ex looks at in one tick */
struct ae_zoom_in
{
	int mode;
	long hold_ms;
	int held;
	long now_ms;
	short level;
	short count;
	int blocked;
	/* the weapon in hand (any number that tells weapons apart), for a double press */
	long key;
	int reset;
	/* the last level used on this weapon (-1 none) */
	short remembered;
	/* the zoom level input: the wheel up (+1) or down (-1) since the last tick, the flashlight pressed */
	int wheel;
	int flash;
};

/* bits of the input blob's player control flags past the engine's (player_control.c uses 0 to 5): the zoom button is
down; the flashlight button is down */
#define AE_ZOOM_HELD_BIT 12
#define AE_ZOOM_FLASH_BIT 13
#define AE_ZOOM_PLAYERS 4

/* a machine for each local player, and the mode they run in */
#define AE_ZOOM_MEMORY 4
struct ae_zoom_bank
{
	/* each player's mode and reset (0.3.7: each player has their own, their input's: split screen mixes them) */
	int modes[AE_ZOOM_PLAYERS];
	int resets[AE_ZOOM_PLAYERS];
	struct ae_zoom_state states[AE_ZOOM_PLAYERS];
	/* the last level used on each of a player's weapons (the weapon's object index: a new life has new objects, so what
	is left of an old one never matches), the oldest replaced first */
	long memory_weapon[AE_ZOOM_PLAYERS][AE_ZOOM_MEMORY];
	short memory_level[AE_ZOOM_PLAYERS][AE_ZOOM_MEMORY];
	int memory_next[AE_ZOOM_PLAYERS];
	/* the flashlight press that the zoom took, down until the button is let go of; its step to be made */
	int flash_latch[AE_ZOOM_PLAYERS];
	int flash_pending[AE_ZOOM_PLAYERS];
	int flash_was_down[AE_ZOOM_PLAYERS];
};

/* "toggle", "hold" or "both" (any case); anything else toggle */
int ae_zoom_mode_from_text(char const *text);
/* the hold time in seconds (0.1 to 1, 0.25 when not a number) as milliseconds */
long ae_zoom_hold_ms(double seconds);

void ae_zoom_reset(struct ae_zoom_state *state);
/* Halo's rotation of the level by one press: unzoomed to the first, each level to the next, the last out */
short ae_zoom_rotate(short level, short count);
/* One look at the button: held (whether it is down), the time in milliseconds, the level the game holds now and the
weapon's zoom level count; blocked when a press cannot act now. Returns the level the game should hold. A tap is a
press released after at most hold_ms milliseconds. */
short ae_zoom_step(struct ae_zoom_state *state, int mode, long hold_ms, int held, long now_ms, short level,
	short count, int blocked);

/* the held bit in the flags, set or cleared */
void ae_zoom_flags_note_held(unsigned long *flags, int held);
/* in the HOLD and BOTH modes takes press_bit (the engine's own press) out of the flags; TOGGLE leaves them alone */
void ae_zoom_flags_filter_press(int mode, unsigned long *flags, int press_bit);
/* every player's mode: a change lets go of the press in progress of each player whose mode it changes; the same mode
changes nothing */
void ae_zoom_bank_set_mode(struct ae_zoom_bank *bank, int mode);
/* one player's mode and reset: a mode change lets go of that player's press in progress only (the others' go on) */
void ae_zoom_bank_set_player(struct ae_zoom_bank *bank, int player, int mode, int reset);
/* a player's mode (TOGGLE for one out of range) */
int ae_zoom_bank_mode(struct ae_zoom_bank const *bank, int player);
/* ae_zoom_step for local player `player` with the held bit of the flags; the TOGGLE mode and a player out of range
return the level untouched */
short ae_zoom_bank_step(struct ae_zoom_bank *bank, int player, long hold_ms, unsigned long flags, long now_ms,
	short level, short count, int blocked);

/* "first" or "last" (any case); anything else first */
int ae_zoom_reset_from_text(char const *text);
/* a level pinned to the weapon's levels (0 to count - 1; -1 for a weapon that does not zoom) */
short ae_zoom_clamp_level(short level, short count);
/* ae_zoom_step with the zoom level input, the double press and the reset */
short ae_zoom_step_ex(struct ae_zoom_state *state, struct ae_zoom_in const *in);

/* the reset policy (every player's) and the memory of the last level used on a weapon */
void ae_zoom_bank_set_reset(struct ae_zoom_bank *bank, int reset);
void ae_zoom_bank_remember(struct ae_zoom_bank *bank, int player, long weapon, short level);
/* -1 when none */
short ae_zoom_bank_recall(struct ae_zoom_bank const *bank, int player, long weapon);
/* (before the step, in the same tick) the flashlight while zoom is held, in HOLD and BOTH: its press is the zoom's, not
the light's: takes light_bit out of unit_flags for that player until the button is let go of, and queues the step */
void ae_zoom_bank_filter_flash(struct ae_zoom_bank *bank, int player, unsigned long const *player_flags,
	unsigned long *unit_flags, int light_bit);
/* ae_zoom_bank_step with all of it: the wheel (+1, -1, 0) and the weapon in hand; remembers the level it ends on */
short ae_zoom_bank_step_ex(struct ae_zoom_bank *bank, int player, long hold_ms, unsigned long flags, long now_ms,
	short level, short count, int blocked, long weapon, int wheel);

/* whether the wheel is the zoom's now: HOLD or BOTH, the zoom held in the final blob, a weapon that zooms, not blocked, and
zoomed (else the wheel does what it always did) */
int ae_zoom_wheel_wanted(int mode, unsigned long flags, short count, int blocked, short level);
/* WHEEL ZOOM (input.wheel_zoom, the mouse's own, OFF by default; owner 2026-10-24, aLTis94's scroll_zoom idea): the
wheel zooms by itself, in any mode, without the zoom held. Whether the wheel is the zoom's so: on, a weapon that zooms
and a press that may act (else it switches weapons, as always) */
int ae_zoom_wheel_free(int wheel_zoom, short count, int blocked);
/* the level after one notch of the wheel by itself: up a level in (from unzoomed to the first; the last stays), down a
level out (the first to unzoomed); a weapon that does not zoom stays unzoomed */
short ae_zoom_wheel_step(short level, short count, int wheel);
/* a new game: the levels remembered, the presses and the flashlight latches forgotten (the mode and reset stay) */
void ae_zoom_bank_forget(struct ae_zoom_bank *bank);

#endif
