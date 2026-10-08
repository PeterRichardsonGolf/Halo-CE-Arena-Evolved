/* ae_ui.h: Arena Evolved menus, the UI core (screen stack, owners, event routing, focus memory, key repeat).
 *
 * Pure C with no engine includes, so the unit tests build it on its own (port/linux/tests/ae_ui_test.c).
 *
 * Screens: a stack of up to AE_MAXIMUM_SCREENS; the top screen gets the input, every screen draws from the
 * bottom of the stack to the top (dialogs draw over the screens under them).
 *
 * Owners: a screen belongs to one local player (0-3) or to anyone (AE_OWNER_ANY). An event's player is the local
 * player its controller drives (ae_input_rules.h ae_input_player_of_controller, from the game's own bindings), or
 * AE_PLAYER_NONE for a controller no player has. Input from a player who doesn't own the top screen is dropped, except START on a screen whose class sets start_from_anyone (lobbies:
 * join / your own settings). An unhandled BACK from the owner pops the screen.
 *
 * Focus memory: each screen's `focus` is opaque to the core and survives while screens above it come and go,
 * so BACK returns to the row the player left. List screens keep their list's focus (ae_list.focus) as the
 * live value: they copy it into screen->focus in `leave` and restore it from screen->focus in `enter`.
 *
 * Last device: the device of the last event that passed the owner filter (prompts show its glyphs); another
 * player's dropped input doesn't change it.
 *
 * Pointer (mouse, touch): it belongs to the player of the first controller (the keyboard's), named in the pointer.
 * It reaches the top screen's `pointer` handler when that screen is that player's or anyone's; moving the mouse or
 * clicking makes the keyboard and mouse the last device (a touch doesn't: the prompts stay as they were).
 *
 * Key repeat: a held direction steps once on the press, again after 400 ms, then every 80 ms, every 40 ms after
 * it has been held 1 s. At most one step per call: after a stall the missed ticks are skipped, not replayed. A repeat
 * seeded as held (ae_repeat_seed) steps only after it is let go of and pressed again.
 */
#ifndef __AE_UI_H
#define __AE_UI_H

enum ae_action { AE_ACTION_NONE, AE_ACTION_UP, AE_ACTION_DOWN, AE_ACTION_LEFT, AE_ACTION_RIGHT, AE_ACTION_ACCEPT,
	AE_ACTION_BACK, AE_ACTION_TAB_PREVIOUS, AE_ACTION_TAB_NEXT, AE_ACTION_X, AE_ACTION_Y, AE_ACTION_START,
	AE_ACTION_SELECT, AE_ACTION_PAGE_UP, AE_ACTION_PAGE_DOWN, AE_NUMBER_OF_ACTIONS };
enum ae_device { AE_DEVICE_KEYBOARD_MOUSE, AE_DEVICE_XBOX, AE_DEVICE_PLAYSTATION, AE_DEVICE_NINTENDO };
enum { AE_OWNER_ANY = -1, AE_PLAYER_NONE = -2, AE_MAXIMUM_PLAYERS = 4, AE_MAXIMUM_SCREENS = 16 };
/* repeat: a held direction's repeat step (not its press): the cursor sound plays at most every 80 ms for those */
struct ae_event { short player; unsigned char action; unsigned char device; unsigned char repeat; };
/* the pointer this frame, in layout units of the whole frame (ae_layout.h) */
struct ae_pointer { float x, y; unsigned char moved, left_clicks, right_clicks, touch; signed char wheel_steps;
	short player;
	/* (M2) the left button held now (a thumb drag); a change of it is an event of its own */
	unsigned char left_held; };
struct ae_screen;
struct ae_screen_class
{
	const char *name;
	void (*enter)(struct ae_screen *screen);
	void (*leave)(struct ae_screen *screen);
	/* nonzero: handled; BACK not handled pops the screen */
	int (*handle)(struct ae_screen *screen, struct ae_event const *event);
	void (*draw)(struct ae_screen *screen);
	/* nonzero: START from a player who doesn't own the screen still reaches handle (lobbies: join / your settings) */
	int start_from_anyone;
	/* the pointer (hover, clicks, wheel); NULL: the screen takes none */
	void (*pointer)(struct ae_screen *screen, struct ae_pointer const *pointer);
	/* (M2; appended: initialisers that stop earlier zero them) nonzero: a popover (dialog, picker, keyboard): fades
	and scales over the screens under it, which keep drawing; else a screen (slides) */
	int popover;
	/* (M2) once a frame for the top screen, before input (text fields); NULL: none */
	void (*update)(struct ae_screen *screen);
};
struct ae_screen { struct ae_screen_class const *screen_class; short owner; short focus; void *data; };

/* empties the stack (no leave calls) and forgets the last device; then the reset hook, if any (M2: text editing's,
which ends typing mode) */
void ae_ui_reset(void);
void ae_ui_set_reset_hook(void (*hook)(void));
/* pushes a screen (focus 0) and calls its enter; 0 when the stack is full */
int ae_ui_push(struct ae_screen_class const *screen_class, short owner, void *data);
/* calls the top screen's leave and removes it */
void ae_ui_pop(void);
struct ae_screen *ae_ui_top(void);
int ae_ui_depth(void);
/* (M2) whether a screen on the stack has this data (a widget's state: open or left behind by a reset) */
int ae_ui_holds(void const *data);
/* routes one event to the top screen (owner filter, START from anyone, BACK pops) */
void ae_ui_dispatch(struct ae_event const *event);
/* routes the pointer to the top screen, if it is the pointer's player's or anyone's */
void ae_ui_dispatch_pointer(struct ae_pointer const *pointer);
/* draws every screen, bottom to top */
void ae_ui_draw(void);
/* called before each screen's draw (M1 review M3: no view, clip or alpha leaks between screens): the hooks set
it to reset the view to the whole frame and apply the screen's motion (offset_x_u, alpha, scale: 0, 1, 1 until
Task 5's motion); NULL: none */
typedef void (*ae_ui_before_draw)(struct ae_screen const *screen, int index, float offset_x_u, float alpha,
	float scale);
void ae_ui_set_before_draw(ae_ui_before_draw before);
/* the top screen's update (ae_hooks calls it before ae_input_poll) */
void ae_ui_update(void);
enum ae_device ae_ui_last_device(void);
/* key repeat: steps due now for a held direction (400 ms, then 80 ms, 40 ms after 1 s held) */
struct ae_repeat { unsigned long since, next; int held; };
int ae_repeat_update(struct ae_repeat *repeat, int held, unsigned long now_ms);
/* starts a repeat as held (a direction already held when a screen opens, or after a stall): no step until it is let
go of and pressed again; not held: as new */
void ae_repeat_seed(struct ae_repeat *repeat, int held, unsigned long now_ms);

#endif
