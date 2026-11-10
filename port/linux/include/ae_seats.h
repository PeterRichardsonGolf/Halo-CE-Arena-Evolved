/*
AE_SEATS.H

Split screen's SEATS: which device is which player on this PC (spec notes/specs/2026-10-16-split-screen-pc.md
section 5). Seat n is controller port n, the game's controller n, split screen player n+1. A seat holds one device:
the keyboard + mouse, or one pad, known by its SDL id while connected and by a key (GUID + serial, or path) to know
it again when it comes back. The first device to press START (ENTER or SPACE on the keyboard) is PLAYER 1.

The rules (ae_seats_rules.c) have no SDL; ae_seats.c reads the devices and routes their input
(port/linux/tests/ae_seats_rules_test.c).
*/

#ifndef AE_SEATS_H
#define AE_SEATS_H

#define AE_SEATS_MAXIMUM 4
#define AE_SEAT_KEY_LENGTH 72

enum
{
	AE_SEAT_EMPTY,
	AE_SEAT_KEYBOARD,
	AE_SEAT_PAD
};

struct ae_seat
{
	int kind;
	/* the pad's SDL id while it is connected (0: unplugged, or the keyboard) */
	unsigned long pad_id;
	/* the pad's own key, to know it again when it comes back */
	char key[AE_SEAT_KEY_LENGTH];
	/* a pad's seat whose pad was unplugged: kept for it (spec 7.2) */
	int held;
};

struct ae_seats
{
	struct ae_seat seat[AE_SEATS_MAXIMUM];
};

/* ---- the rules (ae_seats_rules.c) */

void ae_seats_clear(struct ae_seats *seats);
/* a device's press to join: its seat if it has one already; else the lowest held seat (keeping that seat's
player), else the lowest empty one; -1 when all four are taken. kind AE_SEAT_KEYBOARD or AE_SEAT_PAD (with its id
and key) */
int ae_seats_join(struct ae_seats *seats, int kind, unsigned long pad_id, char const *key);
/* the seat that leaves: emptied (later seats keep their numbers) */
void ae_seats_leave(struct ae_seats *seats, int seat);
/* the seat of the keyboard, of a pad by its id, of a held pad by its key: -1 if none */
int ae_seats_keyboard(struct ae_seats const *seats);
int ae_seats_of_pad(struct ae_seats const *seats, unsigned long pad_id);
int ae_seats_held_by_key(struct ae_seats const *seats, char const *key);
/* a pad unplugged: its seat held (-1: it had none). A pad connected: if a seat is held for its key, that seat
is its again (its seat, else -1) */
int ae_seats_unplugged(struct ae_seats *seats, unsigned long pad_id);
int ae_seats_plugged(struct ae_seats *seats, unsigned long pad_id, char const *key);
/* the seats taken (held ones too), and the people (not held) */
int ae_seats_taken(struct ae_seats const *seats);
int ae_seats_present(struct ae_seats const *seats);
/* the gaps closed (P1, _, P3 -> P1, P2): moved[old seat] = its new seat (-1: empty); returns the seats taken */
int ae_seats_close_gaps(struct ae_seats *seats, int moved[AE_SEATS_MAXIMUM]);
/* the controller picker (spinner or ASSIGN): this device is seat's from now on. A device another seat has is
swapped: that seat gets this seat's device (both must be taken: a device is never taken from a player to fill an
empty seat). An empty or held seat takes a free device. Returns the other seat of a swap, the seat itself when no
swap, -1 when refused */
int ae_seats_assign(struct ae_seats *seats, int seat, int kind, unsigned long pad_id, char const *key);

/* ---- the seat map (ae_seats.c): on from the JOIN screen until the machine leaves split screen; off, the ports are
the stable pad ports' (ae_pad_ports.c) and the keyboard is port 0's, as ever */

/* the family of a seat's device, for its picture and name */
enum
{
	AE_SEAT_FAMILY_NONE,
	AE_SEAT_FAMILY_KEYBOARD,
	AE_SEAT_FAMILY_XBOX,
	AE_SEAT_FAMILY_PLAYSTATION,
	AE_SEAT_FAMILY_NINTENDO,
	AE_SEAT_FAMILY_OTHER
};

/* what ae_seats_update saw this frame */
enum
{
	AE_SEATS_JOINED = 1,
	/* B or ESC from a device with no seat */
	AE_SEATS_BACK = 2,
	/* START from a fifth device */
	AE_SEATS_FULL = 4,
	/* a seat's pad unplugged, or back */
	AE_SEATS_CHANGED = 8,
	/* the picker: a device given to a seat (ASSIGN's press, a spinner); a swap between two seats; ASSIGN's wait over
	with nothing given (cancelled, or no press in time) */
	AE_SEATS_ASSIGNED = 16,
	AE_SEATS_SWAPPED = 32,
	AE_SEATS_WAIT_ENDED = 64
};

/* a device's short name for the picker (KEYBOARD, XBOX, DUALSENSE, DUALSHOCK, SWITCH PRO, JOY-CON, PAD), with " n"
when number > 0 (two of a family are connected); name is SDL's, for the kind of a family's pad */
void ae_seats_short_name(int family, char const *name, int number, char *out, int size);

struct ae_seat_view
{
	int kind;
	int held;
	int family;
	/* the device's name as SDL has it */
	char name[64];
};

void ae_seats_begin(void);
void ae_seats_end(void);
/* off as soon as no key or button that joins or backs out is held (backing out of the JOIN screen) */
void ae_seats_end_released(void);
int ae_seats_active(void);
/* every frame while on (the JOIN screen's and the lobby's update): pads plugged and unplugged, and the presses of
devices with no seat (START / ENTER / SPACE joins, B / ESC / BACKSPACE asks to go back); AE_SEATS_* bits */
unsigned ae_seats_update(void);
/* a copy of the seats, and one seat's device */
void ae_seats_get(struct ae_seats *seats);
void ae_seats_view(int seat, struct ae_seat_view *view);
void ae_seats_leave_seat(int seat);
/* the gaps closed: moved[old seat] = new seat (-1 none); returns the seats taken */
int ae_seats_close(int moved[AE_SEATS_MAXIMUM]);

/* ---- the controller picker (the JOIN cards' CONTROLLER spinners and ASSIGN, the lobby's rows) */

#define AE_SEATS_DEVICES_MAXIMUM 17
#define AE_SEATS_ASSIGN_WAIT_MS 10000

struct ae_seat_device
{
	int kind;
	unsigned long pad_id;
	int family;
	/* the seat it has, -1 none */
	int seat;
	/* its short name (ae_seats_short_name, numbered when two share it) */
	char name[24];
};

/* the keyboard, then every pad connected (SDL's order); the number listed */
int ae_seats_devices(struct ae_seat_device *list, int maximum);
/* a seated seat's spinner: the next or previous device (direction +1 / -1), given to it at once (a swap when
another player has it); AE_SEATS_* bits */
unsigned ae_seats_cycle(int seat, int direction);
/* an empty seat's spinner: its candidate, a free device (kind AE_SEAT_EMPTY: none free); cycled; and taken (the
candidate seated: AE_SEATS_ASSIGNED, else 0) */
void ae_seats_candidate(int seat, struct ae_seat_device *device);
void ae_seats_candidate_cycle(int seat, int direction);
unsigned ae_seats_candidate_take(int seat);
/* ASSIGN: the next press on any device is the seat's (a keyboard key: the keyboard; ESC or the seat's own B
cancels); every device's input is kept from the game until then, and until the press is let go */
void ae_seats_assign_wait(int seat);
void ae_seats_assign_cancel(void);
/* the seat waiting, -1 none; and the time left in ms */
int ae_seats_assign_waiting(int *left_ms);
/* the last change (AE_SEATS_ASSIGNED / SWAPPED): the seat, and the other seat of a swap (-1) */
void ae_seats_last_change(int *seat, int *other);
/* ms since a seat's device was last pressed (any button or key), to flash its name; -1 never */
long ae_seats_pressed_ago(int seat);
/* ASSIGN waiting, or its press not yet let go: the pads' buttons held back from the game (xinput_sdl.c) */
int ae_seats_input_held_back(void);

/* (debug.test_input, ae_platform.c) its keys now: ENTER or SPACE, ESC */
void ae_seats_test_keys(int join, int back);

/* ---- xinput_sdl.c's (AE hooks) */

/* the port whose state is made with the keyboard's work (mouse, keys, screenshots): 0 when off, or when the keyboard
has no seat; its seat when it has one */
int ae_seats_keyboard_port(void);
/* the keyboard drives a controller: always when off; when on, only with a seat */
int ae_seats_keyboard_plays(void);
/* the controller the keyboard's actions and the mouse's aim are: 0 when off; its seat, or -1, when on */
int ae_seats_keyboard_controller(void);
/* the ports a seat's device is connected to (held seats are not), and port 1 unless its seat is held */
unsigned long ae_seats_connected(void);
/* (sdl_gamepads, through ae_platform_stable_ports) the seats' pads in their ports; the highest port with a pad + 1 */
struct SDL_Gamepad;
int ae_seats_fill(struct SDL_Gamepad **gamepads, int count, int ports);

#endif
