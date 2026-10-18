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
	AE_SEATS_CHANGED = 8
};

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
