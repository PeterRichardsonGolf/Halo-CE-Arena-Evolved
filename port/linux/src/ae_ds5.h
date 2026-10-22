/*
AE_DS5.H

Arena Evolved: a PS5 DualSense's (and DualSense Edge's) adaptive triggers, lightbar and player lights, driven from the
game (port/linux/game/ae_ds5_game.c says what each player wants: lightbar colour, player number, R2 and L2 effects).
Settings > Gamepads: ADAPTIVE TRIGGERS (input.adaptive_triggers, on), TRIGGER STRENGTH (input.trigger_strength,
normal) and LIGHTBAR (input.lightbar: player_color, the lightbar in the player's colour and the player lights the
split-screen number; shield, the lightbar the player's shield meter as the HUD draws it; off). Other pads are never touched. Three ways to the pad:
- Windows: SDL's PS5 driver reads the pad, and the same whole output reports as the hidraw way's (USB 0x02, Bluetooth
  0x31 with its CRC) are written to the pad's own HID device, opened beside SDL's through SDL's hidapi (SDL's path to
  it), by a writer thread of its own (a write waits for the pad). ONE WRITER as on Linux: the game's rumble goes in
  our reports (SDL is given zeros), no SDL effect or LED calls; our lightbar and player lights are sent again once a
  second, as SDL's driver sets its own now and then. Through SDL's queue the trigger reports were merged and replaced
  (a kick replaced by the held shape before it went out: owner 10-21, "hardly there"). Not opened: SDL's way.
- SDL's own PS5 driver (macOS; Windows when the HID device cannot be opened; Linux where SDL reads the pad's hidraw
  node): SDL_SendGamepadEffect with the 47-byte effects body, SDL_SetGamepadLED, SDL_SetGamepadPlayerIndex.
- Linux with the kernel's hid-playstation driver (AE's default, ae_gamepads.c): the input stays the kernel's, and
  the output reports are written to the pad's own /dev/hidraw node when this user may write it (udev rules: Steam's
  60-steam-input.rules, or the Linux downloads' 70-ae-dualsense.rules). Only the trigger, lightbar and player light
  enable bits are set, so the kernel's own reports and ours leave each other's fields alone; the game's rumble
  goes in our reports too (ONE WRITER: SDL is given zeros for that pad, so the kernel sends no rumble reports).
ONE WRITER, coalesced (ae_ds5_rules.h's link): a pad gets at most one report a frame, only of the fields that changed,
and on Bluetooth at most one every 50 ms (the changes in between merged, the latest sent as the window opens). A pad
whose hidraw node goes (ENODEV, EIO: the Bluetooth link lost) has it closed, looked for again each second and, back,
everything sent again; a full queue (EAGAIN) keeps the report for the next window.
A kick is a trigger's event shape (a round, the reload's magazine going in, the weapon readied, the shotgun's pump, a
grenade's throw on L2) sent once, then the held one again after its time: two reports an event. An event's rumble
pulse (human weapons the left motor, Covenant ones the right) goes into the game's own rumble. The
triggers are sent again though unchanged, while both are up, after the game's rumble changes and once a second.
debug.txt says how each DualSense is driven (or why not, with the udev rule's line); debug.ds5_log adds the bytes
written, the effects' changes and each minute a pad's reports written, merged, deferred and failed. The triggers are let go (and the lightbar given back) at exit, when a pad leaves
and when a setting is turned off.
*/

#ifndef __AE_DS5_H
#define __AE_DS5_H

#include "ae_ds5_rules.h"

struct SDL_Gamepad;

/* the settings (read again when one was written) */
struct ae_ds5_settings
{
	int triggers; /* ADAPTIVE TRIGGERS */
	int strength; /* TRIGGER STRENGTH, percent (ae_ds5_strength_percent) */
	int lights;   /* LIGHTBAR not OFF: the lightbar and the player lights */
	int shield;   /* LIGHTBAR SHIELD: the lightbar the shield meter */
	int log;      /* debug.ds5_log */
};

/* what one controller port's player wants on its pad (the game side, each frame it has a player) */
struct ae_ds5_wish
{
	unsigned char red, green, blue;
	/* the player lights' bits (ae_ds5_player_lights; 0: none) */
	unsigned char player_lights;
	unsigned char right[AE_DS5_EFFECT_SIZE];
	unsigned char left[AE_DS5_EFFECT_SIZE];
	/* each trigger's last event kick (ms 0: none) and its events so far (a change = a kick) */
	unsigned char kick_right[AE_DS5_EFFECT_SIZE];
	int kick_right_ms;
	unsigned long kicks_right;
	unsigned char kick_left[AE_DS5_EFFECT_SIZE];
	int kick_left_ms;
	unsigned long kicks_left;
	/* the last event's rumble pulse (0-255 a motor; 0 ms none) and the pulses so far (a change = a pulse) */
	struct ae_ds5_rumble rumble;
	unsigned long rumbles;
	/* for debug.ds5_log: the weapon and the effects in words */
	char what[256];
};

struct ae_ds5_settings ae_ds5_settings_get(void);
/* (0.3.7) a port's player's own (each player's DUALSENSE rows); ae_ds5_settings_get is port 1's */
struct ae_ds5_settings ae_ds5_settings_port(int port);
/* LIGHTBAR's mode in words, for debug.txt */
char const *ae_ds5_lightbar_mode_name(struct ae_ds5_settings const *settings);
/* whether either feature is on */
int ae_ds5_on(void);
/* whether controller port `port`'s pad is a DualSense this machine drives (the game works out a wish only then) */
int ae_ds5_port_active(int port);
/* a rumble pulse (0-255 a motor, for ms) on controller port `port`'s pad, whatever pad it is: merged into the game's
motors as a DualSense's event pulses are (the battle rifle's round on a pad that is not a DualSense) */
void ae_ds5_pulse(int port, int left, int right, int ms);
/* the game: controller port `port`'s player wants this (copied) */
void ae_ds5_wish_set(int port, struct ae_ds5_wish const *wish);
/* xinput_sdl.c XInputSetState: the SDL pad that drives controller port `port` (NULL: none), and the game's motors,
which a DualSense's event pulse raises while it lasts (each motor the larger of the game's and the pulse's). Through
SDL's driver the pad's rumble stays the game's own SDL_RumbleGamepad; on the hidraw and HID ways the motors go in our reports
and are given back zero, so SDL_RumbleGamepad sends nothing (one writer) */
void ae_ds5_port(int port, struct SDL_Gamepad *gamepad, unsigned short *left_motor, unsigned short *right_motor);
/* sdl_platform.c: once a frame, after the events (sends what changed) */
void ae_ds5_frame(void);
/* sdl_platform.c: a pad is about to be closed (NULL: nothing) */
void ae_ds5_removed(struct SDL_Gamepad *gamepad);

#endif
