/*
AE_GAMEPADS.H

Arena Evolved: gamepads on the platform layer (sdl_platform.c's AE hooks;
DamnationCE v0.3.41, CC0).

- Linux: a PlayStation pad (DualSense, DualShock 4) goes through the kernel's
  driver of it (hid-playstation since Linux 5.12, or hid-sony) where the kernel
  has one, not SDL's own (HIDAPI). SDL's reads the pad's /dev/hidraw node,
  which only udev rules (Steam's) open to the player; without them the pad was
  not found at all. The kernel's gives its buttons, sticks, triggers and
  rumble through the input event nodes a player can read. The driver counts
  when it is loaded, or a module this kernel loads when the pad is plugged in,
  or built in. Other platforms are unchanged.
- debug.txt says each pad's kind (ps5, ps4, xboxone, ...) and the device SDL
  reads it through (on Linux a hidraw node is SDL's own driver, an event node
  the kernel's), so a player whose pad misbehaves can say which.
*/

#ifndef __AE_GAMEPADS_H
#define __AE_GAMEPADS_H

struct SDL_Gamepad;

/* the SDL hints for the pads, before SDL's gamepad subsystem starts */
void ae_gamepads_hints(void);
/* a pad opened (SDL_EVENT_GAMEPAD_ADDED): its kind and device logged */
void ae_gamepads_log(struct SDL_Gamepad *gamepad);

#endif
