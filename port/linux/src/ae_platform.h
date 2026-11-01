/*
AE_PLATFORM.H

What AE's menus read from the platform layer themselves (ae_platform.c).
Plain types only: the game's units call it.
*/

#ifndef __AE_PLATFORM_H
#define __AE_PLATFORM_H

/* keys AE reads itself: the game's menu keys (xinput_sdl.c keyboard_gamepad, driving the first controller) have no
Q, Page Up or Page Down, and make E X and Tab Y, where AE's menus make E and Q tabs and Tab a focus step; so R (or Y) is
AE's Y and F (or X) its X, the keys its prompts show for them (ae_prompt_key) */
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
	AE_KEY_RIGHT = 1 << 9,
	AE_KEY_R = 1 << 10,                 /* R or Y */
	AE_KEY_F = 1 << 11                  /* F or X */
};

/* the AE_KEY_* held now (0 without a focused window). SDL reports the keys where they are, so the numeric keypad's
Page Up, Page Down and arrows are its 9, 3, 8, 2, 4 and 6: with Num Lock off they count as those keys (Page Up and
Page Down as AE_KEY_PAGE_UP / _DOWN, the arrows as AE_KEY_UP...); its Enter is the game's Enter already (A) */
int ae_platform_keys(void);
/* (0.3.9) the modifiers held now, either key of each (0 without a focused window): SETTINGS' number rows step 0.01
with Shift and the arrows, Ctrl and the wheel. A test's token holds them too: "key:Right+shift", "wheel:1+ctrl"
(ae_platform_test_pad_press reads the token; xinput_sdl.c's own reading of it skips the unknown name) */
enum { AE_MODIFIER_SHIFT = 1 << 0, AE_MODIFIER_CONTROL = 1 << 1 };
int ae_platform_modifiers(void);
/* the menus' cursor height in pixels (sdl_platform.c menu_cursor_fit: height, OpenCE's, is 32 of the menus' 480 lines,
72 pixels at 1080p): with Arena Evolved's menus (display.arena_menus) half that, 36 pixels at 1080p, never under 16, so
the arrow sits with AE's rows; OpenCE's otherwise */
int ae_platform_menu_cursor_height(int lines, int height);
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
/* the device the player last used: 0 the keyboard (or mouse), 1 an Xbox-like pad, 2 a PlayStation pad, 3 a
Nintendo pad (1 without the game browser, whose prompts it is) */
int ae_platform_input_scheme(void);
/* the test word of debug.ae_message_test that forces the device (2 "playstation", 3 "nintendo"), or -1 */
int ae_platform_test_scheme(void);
/* whether the keyboard and mouse were used after any pad (the HUD's key prompts, port/linux/game/ae_key_prompts.c):
AE's own record, for which a mouse's buttons and motion count too (xinput_sdl.c's scheme takes its keys only, and
stays as it is for the rest of the game); before either was used, the scheme's (0: the keyboard). xinput_sdl.c's AE
hooks note the keyboard's keys and mouse buttons each poll and a pad's use; ae_glue_mouse.c the mouse's motion */
int ae_platform_keyboard_last_used(void);
void ae_platform_note_keyboard_input(const unsigned char *keys, const unsigned char *mouse_buttons, int mouse_live);
void ae_platform_note_keyboard_used(void);
void ae_platform_note_pad_used(void);
/* whether the keyboard or mouse was used at all this session (AE's main menu: a lone pad's START then signs it in as
player 2, the keyboard staying player 1's) */
int ae_platform_keyboard_used(void);
/* whether any gamepad holds its START now: a START of controller 1 that is a pad's, not the keyboard's Enter */
int ae_platform_pad_start_held(void);
/* a controller's back buttons (a Switch 2 Pro Controller's GR and GL, an Elite's or DualSense Edge's upper paddles)
pressed as the Xbox buttons input.back_right and input.back_left name, so they follow the profile's layout: merged into
an Xbox pad's state (xinput_sdl.c: its wButtons and bAnalogButtons) */
struct SDL_Gamepad;
void ae_platform_back_buttons(struct SDL_Gamepad *gamepad, unsigned short *buttons, unsigned char *analog_buttons);
/* TEST RUN (ae_test_run_input.c), from xinput_sdl.c for each port's pad as it is read: records which chords it holds (the reader reads them here,
as the game's copy has them swallowed), and while the reader is on, takes the chord's buttons out of the pad (BACK
held with A, B, X, Y, LB, RB or the d-pad's up), so the game does not also see them;
while the note box is open it takes the whole pad (sticks too) and keeps it until each
button is released after the box closes. */
struct _XINPUT_GAMEPAD;
void ae_test_run_input_pad(short port, struct _XINPUT_GAMEPAD *pad);
/* TEST RUN (ae_test_run_input.c), from xinput_sdl.c each frame the keyboard's port is read: the keyboard's bound actions
held (0 while the console, chat or note box types), menus up or not. While a run is on and RELOAD (F1) is held, takes the
menus' BACK out of the keyboard's part of the pad (buttons), so RELOAD is not also BACK. */
void ae_test_run_input_keys(unsigned long long held, unsigned short *buttons);
/* TEST RUN (ae_test_run_note.c): the note box has the keyboard (xinput_sdl.c keeps the keys from the game and passes them
as keystrokes) */
int ae_test_run_typing(void);
/* TEST RUN (ae_test_run.c): a run is on */
int ae_test_run_active(void);
/* xinput_sdl.c's sdl_gamepads (an AE hook): the connected pads (SDL's order, count of them) put in their ports, a pad
keeping its port while connected and a new one taking the first free port (SDL lists HIDAPI pads first, so a pad
seen late went in front and moved every other); ports without a pad NULL; returns the highest port with a pad plus one */
int ae_platform_stable_ports(struct SDL_Gamepad **gamepads, int count, int ports);
/* the LAN lobby's controller picker (the stable ports; split screen's goes through the seats): a port's pad by its
SDL id (0 none); a pad's port (-1); a pad given to a port, swapped with the port that had it (that port, the port
itself when no swap, -1 refused); ASSIGN: the next press on any pad is the port's (ESC or 10 s end it), no pad
driving the game until then and until that press is let go; its update each lobby frame gives AE_SEATS_* bits
(ae_seats.h) and the change (the port, the other of a swap or -1) */
unsigned long ae_platform_port_pad(int port);
int ae_platform_port_of_pad(unsigned long pad_id);
int ae_platform_port_give(int port, unsigned long pad_id);
void ae_platform_port_pick_wait(int port);
void ae_platform_port_pick_cancel(void);
int ae_platform_port_pick_waiting(int *left_ms);
unsigned ae_platform_port_pick_update(int *port, int *other);
/* (xinput_sdl.c's) the pads' buttons held back from the game: a picker's ASSIGN waiting, or its press not let go */
int ae_platform_input_held_back(void);
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
the presses of a real lone gamepad are tested, through the game's own mapping of pads to ports (ae_platform.c); a
"type:<key>" token types that key once as a real key event (KEY BINDINGS' PRESS A KEY takes it) */
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

/* the maps folder check's (sdl_platform.c) with no Xbox ui.map: MCC found and game.mcc_use not "no", the main menu
is read from MCC's ui.map (ae_mcc_boot.h) and the game goes on (logged); 0 otherwise, and the check says what is
missing as before */
int ae_mcc_boot_without_xbox(void);

/* the web build: the page hands the game its maps (the disc image the player chose), and a game with none goes on
to say so through the page; the desktop's own check of the maps folder, with its message box, is not made there */
#ifdef __EMSCRIPTEN__
#define AE_PLATFORM_MAPS_FROM_PAGE 1
#else
#define AE_PLATFORM_MAPS_FROM_PAGE 0
#endif

#endif
