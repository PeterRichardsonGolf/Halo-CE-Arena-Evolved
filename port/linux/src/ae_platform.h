/*
AE_PLATFORM.H

What AE's menus read from the platform layer themselves (ae_platform.c).
Plain types only: the game's units call it.
*/

#ifndef __AE_PLATFORM_H
#define __AE_PLATFORM_H

/* keys AE reads itself: the game's menu keys (xinput_sdl.c keyboard_gamepad, driving the first controller) have no
Q, Page Up or Page Down, and make E X and Tab Y, where AE's menus make E and Q tabs and Tab a focus step */
enum
{
	AE_KEY_Q = 1 << 0,
	AE_KEY_E = 1 << 1,
	AE_KEY_PAGE_UP = 1 << 2,
	AE_KEY_PAGE_DOWN = 1 << 3,
	AE_KEY_TAB = 1 << 4,
	AE_KEY_SHIFT = 1 << 5,
	/* the numeric keypad's arrows with Num Lock off (the game's menu keys take only the arrow keys) */
	AE_KEY_UP = 1 << 6,
	AE_KEY_DOWN = 1 << 7,
	AE_KEY_LEFT = 1 << 8,
	AE_KEY_RIGHT = 1 << 9
};

/* the AE_KEY_* held now (0 without a focused window). SDL reports the keys where they are, so the numeric keypad's
Page Up, Page Down and arrows are its 9, 3, 8, 2, 4 and 6: with Num Lock off they count as those keys (Page Up and
Page Down as AE_KEY_PAGE_UP / _DOWN, the arrows as AE_KEY_UP...); its Enter is the game's Enter already (A) */
int ae_platform_keys(void);
/* mouse button 4 (back) presses while the menus have the pointer (ae_back_presses.c): sdl_platform.c counts them
(an AE hook line, since its pointer branch keeps them from the controller), but only while armed: the game's side
arms the count while an AE screen is open (never with display.arena_menus off) and takes the presses (the count
starts over); disarming drops what was counted */
void ae_platform_count_back_press(void);
void ae_platform_arm_back_presses(int armed);
int ae_platform_take_back_presses(void);
/* Tab's presses for AE's menus (Tab and Shift+Tab step the focus): key downs, not the key's own repeats, counted as
SDL queues them (an event watch, ae_platform.c), so a Tab tapped and let go of within one frame still counts, where
the held keys (ae_platform_keys) never see it. forward: without Shift, backward: Shift held at the press. Counted only
while armed (the game's side arms it with the back presses, while an AE screen is open; installed at the first arming,
never with display.arena_menus off); disarming drops the count. Taking them starts the count over. None on the
dedicated server and Android (no SDL event watch there) */
void ae_platform_arm_tab_presses(int armed);
/* what a Tab key down counts as (ae_back_presses.c, pure): nothing for the key's own repeat; with Alt or the GUI key
held (Alt+Tab and Super+Tab switch windows) a press that steps nothing (AE_TAB_PRESS_IGNORED: its keyboard Y is still
dropped, ae_input_key_translate); else AE_TAB_PRESS_FORWARD, or AE_TAB_PRESS_BACKWARD with Shift */
enum { AE_TAB_PRESS_NONE, AE_TAB_PRESS_FORWARD, AE_TAB_PRESS_BACKWARD, AE_TAB_PRESS_IGNORED };
int ae_platform_tab_press_kind(int key_repeat, int shift, int alt, int gui);
/* the presses since the last take, each kind (ignored: with Alt or the GUI key) */
void ae_platform_take_tab_presses(int *forward, int *backward, int *ignored);
/* whether the mouse's left button is held now (a scrollbar thumb's drag; the menus' pointer reports presses only);
0 on the dedicated server and Android */
int ae_platform_mouse_left_held(void);
/* map sharing's key (ae_map_share_all.c): F6 held now; 0 on the dedicated server and Android */
int ae_platform_map_share_key(void);
/* the device the player last used: 0 the keyboard (or mouse), 1 an Xbox-like pad, 2 a PlayStation pad, 3 a
Nintendo pad (1 without the game browser, whose prompts it is) */
int ae_platform_input_scheme(void);
/* a controller's back buttons (a Switch 2 Pro Controller's GR and GL, an Elite's or DualSense Edge's upper paddles)
pressed as the Xbox buttons input.back_right and input.back_left name, so they follow the profile's layout: merged into
an Xbox pad's state (xinput_sdl.c: its wButtons and bAnalogButtons) */
struct SDL_Gamepad;
void ae_platform_back_buttons(struct SDL_Gamepad *gamepad, unsigned short *buttons, unsigned char *analog_buttons);
/* (debug.test_input, with display.arena_menus on, while the platform types into a field) a key: token typed as well
as held: one key down and up queued as the keyboard's events, so the typing's keystrokes see it; once a token */
void ae_platform_test_keystroke(int scancode);
/* the test hook debug.test_verdict_file (OFF while empty), sdl_platform.c's AE hook lines: F7 (PASS) and F8 (FAIL)
append "<date> <time> <debug.test_name> PASS|FAIL" to that file, once a run, and quit the game. The key's hook: 1 when
the key down was F7 or F8 and taken (the event goes no further), else 0. The pump's hook, each frame: with
debug.test_verdict_auto "PASS:<seconds>" or "FAIL:<seconds>", that verdict by itself that many seconds after the
first frame (the batch runner's own checks, no key to press). Nothing on the dedicated server */
int ae_platform_test_verdict_key(int scancode);
void ae_platform_test_verdict_auto(void);
/* (debug.test_input "menu:" with "pad:<button>" tokens, xinput_sdl.c's poll, each call) the token pressed now (NULL:
none) and its length: a virtual SDL gamepad, attached at the first call, gets its button (pad:b, pad:start, ...), so
the presses of a real lone gamepad are tested, through the game's own mapping of pads to ports (ae_platform.c) */
void ae_platform_test_pad_press(char const *token, int length);
/* (debug.lone_pad_first, for test launches: xinput_sdl.c port_gamepad and connected_gamepads) a lone gamepad stays on
port 0 with the keyboard in split screen, so it plays player 1, and port 1 is reported connected with no input
(player 2 stands); off: upstream's rule, the lone pad is player 2's */
int ae_platform_lone_pad_first(void);

/* the MCC mouse style (ae_glue_mouse.c, the game's side; xinput_sdl.c halo_linux_mouse_look): debug.test_input
"mouse:<dx>,<dy>" adds those counts to the ones read, every 30th call; ae_mouse_look_override then replaces the
classic yaw and pitch with MCC style's when input.mouse_style is mcc (nothing in classic style) */
void ae_mouse_test_counts(float *x, float *y);
void ae_mouse_look_override(float x, float y, int invert, float *yaw, float *pitch);

/* the zoom's modes (ae_glue_zoom.c; xinput_sdl.c): whether the wheel is the zoom level's now (the zoom held by the first
controller's player in HOLD or BOTH: no action takes the wheel then), and a scroll of the wheel's start (+1 up, -1 down) */
int ae_zoom_wheel_taken(void);
void ae_zoom_wheel_scrolled(int direction);

/* the web build: the page hands the game its maps (the disc image the player chose), and a game with none goes on
to say so through the page; the desktop's own check of the maps folder, with its message box, is not made there */
#ifdef __EMSCRIPTEN__
#define AE_PLATFORM_MAPS_FROM_PAGE 1
#else
#define AE_PLATFORM_MAPS_FROM_PAGE 0
#endif

#endif
