/* ae_test_run_input.c: TEST RUN's verdict controls (ae_test_run_input.h). */

#include "ae_test_run_input.h"

#include <string.h>

#include "ae_test_run_note.h"
#include "ae_ui.h"
#include "xdk_pdb.h"
#include "halo_keyboard.h"
#include "../src/ae_platform.h"

/* XINPUT_GAMEPAD's (port/include/xdk/xdk_xbox.h): the d-pad's up and BACK in wButtons; the analog buttons' indices */
enum { PAD_DPAD_UP = 0x0001, PAD_BACK = 0x0020, PAD_A = 0, PAD_B = 1, PAD_X = 2, PAD_Y = 3, PAD_BLACK = 4,
	PAD_WHITE = 5, PAD_PRESSED = 0x80, PORTS = 4, PAD_DPAD_DOWN = 0x0002, PAD_DPAD_LEFT = 0x0004, PAD_DPAD_RIGHT = 0x0008 };

/* the inputs as bits: a verdict key's is its enum value less one, in the enum's order; the hide key's is last */
enum { BIT_HIDE = 6, MASK_KEYS = (1 << BIT_HIDE) - 1 };

static int enabled;
static unsigned last;             /* the bits held at the last read */
static unsigned pad_held[PORTS];  /* each port's pad: the bits of the chord it holds */
static unsigned long polls;       /* counts the reads of the reader (a frame each) */
static unsigned long pad_polled[PORTS]; /* the count when each port's pad was last read */
static unsigned long long keys_held;   /* the keyboard's bound actions as xinput_sdl.c last saw them (menus or not) */
static unsigned long keys_polled;      /* the count when it was last given */

/* the NOTE box's pad: every button of a port as bits (the wButtons, then the analog buttons from bit 16) */
enum { ANALOG_BUTTONS = 8, ANALOG_SHIFT = 16 };
/* the left stick as four more bits (from bit 24), in the order of the d-pad's: pressed past STICK_HELD of its range, let go
under STICK_LET_GO (so a stick near the line does not chatter). A held direction steps as AE's menus do (ae_repeat_update:
at once, after 400 ms, then every 80 ms). */
enum { STICK_SHIFT = 24, STICK_HELD = 16000, STICK_LET_GO = 8000, PAD_START = 0x0010 };
/* port/linux/src/main.c's, as ae_test_run.c has it */
unsigned long system_milliseconds(void);
static struct ae_repeat note_repeat[PORTS][4]; /* each port's four directions: up, down, left, right */
static unsigned note_prev[PORTS];   /* what the port held at its last read while the box was open */
static unsigned note_swallow[PORTS]; /* held when the box closed (or was open): taken from the game until released */
static unsigned long note_polled[PORTS]; /* the count when each port was last read with the box open */
static int note_seen[PORTS];        /* the box was open at the port's last read */
static unsigned note_edges;         /* AE_TEST_RUN_PAD_* pressed since the note module last asked */

static const struct { enum halo_keyboard_action action; int bit; } keyboard[] =
{
	{ HALO_KEYBOARD_TEST_PASS, AE_TEST_RUN_KEY_PASS - 1 }, { HALO_KEYBOARD_TEST_FAIL, AE_TEST_RUN_KEY_FAIL - 1 },
	{ HALO_KEYBOARD_TEST_NOTE, AE_TEST_RUN_KEY_NOTE - 1 }, { HALO_KEYBOARD_TEST_SKIP, AE_TEST_RUN_KEY_SKIP - 1 },
	{ HALO_KEYBOARD_TEST_RELOAD, AE_TEST_RUN_KEY_RELOAD - 1 }, { HALO_KEYBOARD_TEST_BACK, AE_TEST_RUN_KEY_BACK - 1 },
	{ HALO_KEYBOARD_TEST_HIDE_CARD, BIT_HIDE },
};

/* the bits held now: the keyboard's player's keys, and every pad's chords */
static unsigned held_now(void)
{
	/* (a keyboard not read in the last frame or two holds nothing; the bits are the keyboard's seat whatever its port, menus or not) */
	unsigned long long actions = keys_polled + 1 >= polls ? keys_held : 0;
	unsigned held = 0;
	int index;

	for (index = 0; index < (int)(sizeof(keyboard) / sizeof(keyboard[0])); index++)
	{
		if (actions & (1ULL << keyboard[index].action))
		{
			held |= 1u << keyboard[index].bit;
			/* (a verdict key is the keyboard used: AE's record, for the hints and the HUD's prompts, as a mouse move is) */
			ae_platform_note_keyboard_used();
		}
	}
	/* (a port not read in the last frame or two, a pad unplugged with a chord held, holds nothing) */
	for (index = 0; index < PORTS; index++)
	{
		if (pad_polled[index] + 1 >= polls)
			held |= pad_held[index];
	}
	return held;
}

void ae_test_run_input_keys(unsigned long long held, unsigned short *buttons)
{
	keys_held = held;
	keys_polled = polls;
	/* (F1 is the menus' BACK too: while a run is on, RELOAD held is not also that BACK) */
	if (enabled && (held & (1ULL << HALO_KEYBOARD_TEST_RELOAD)))
		*buttons &= (unsigned short)~PAD_BACK;
}

void ae_test_run_input_enable(int on)
{
	on = on != 0;
	if (on && !enabled)
		last = held_now();
	enabled = on;
	if (!on)
		last = 0;
}

enum ae_test_run_key ae_test_run_input_pressed(void)
{
	unsigned held, pressed;
	int key;

	polls++;
	if (!enabled)
		return AE_TEST_RUN_KEY_NONE;
	held = held_now();
	pressed = held & ~last & MASK_KEYS;
	last = held;
	for (key = AE_TEST_RUN_KEY_PASS; key <= AE_TEST_RUN_KEY_BACK; key++)
	{
		if (pressed & (1u << (key - 1)))
			return (enum ae_test_run_key)key;
	}
	return AE_TEST_RUN_KEY_NONE;
}

int ae_test_run_input_hide_held(void)
{
	return enabled && (held_now() & (1u << BIT_HIDE)) != 0;
}

/* every button of the pad as bits */
static unsigned pad_bits(const unsigned short *buttons, const unsigned char *analog_buttons)
{
	unsigned bits = *buttons;
	int index;

	for (index = 0; index < ANALOG_BUTTONS; index++)
	{
		if (analog_buttons[index] >= PAD_PRESSED)
			bits |= 1u << (ANALOG_SHIFT + index);
	}
	return bits;
}

/* the whole pad taken: buttons, triggers and sticks */
static void pad_clear(XINPUT_GAMEPAD *pad)
{
	pad->wButtons = 0;
	memset(pad->bAnalogButtons, 0, ANALOG_BUTTONS);
	pad->sThumbLX = pad->sThumbLY = pad->sThumbRX = pad->sThumbRY = 0;
}

/* only the buttons in mask taken (a new press of another button gets through) */
static void pad_clear_bits(unsigned short *buttons, unsigned char *analog_buttons, unsigned mask)
{
	int index;

	*buttons &= (unsigned short)~mask;
	for (index = 0; index < ANALOG_BUTTONS; index++)
	{
		if (mask & (1u << (ANALOG_SHIFT + index)))
			analog_buttons[index] = 0;
	}
}

/* (0.4.2) a face button's analog index by its label: a Nintendo pad's A is the east button, its B the south, its X the
north and its Y the west, as AE menus read them (ae_input.c button_action) */
static int face_button(int analog)
{
	static const int nintendo[4] = { PAD_B, PAD_A, PAD_Y, PAD_X };

	if (analog >= PAD_A && analog <= PAD_Y && ae_platform_input_scheme() == AE_DEVICE_NINTENDO)
		return nintendo[analog];
	return analog;
}

/* the NOTE box owns the pad while it is open, and each button it held when it closed until that button is released
(so the A or B that closed it does not reach the game). Returns 1 when the pad was taken. */
static int note_pad(short port, XINPUT_GAMEPAD *pad)
{
	unsigned short *buttons = &pad->wButtons;
	unsigned char *analog_buttons = pad->bAnalogButtons;
	short sticks[4] = { pad->sThumbLX, pad->sThumbLY, pad->sThumbRX, pad->sThumbRY };
	unsigned now = pad_bits(buttons, analog_buttons), pressed, dirs;

	if (ae_test_run_typing())
	{
		/* (the box opening: what is already held is not a press) */
		/* (a port not read in the last frame or two, a pad unplugged and back, starts again: what it holds then is not a press) */
		if (note_seen[port] && note_polled[port] + 1 < polls)
			note_seen[port] = 0;
		/* (the left stick's four directions join the d-pad's: LX and LY, up is a positive LY; a direction held keeps
		its bit down to STICK_LET_GO) */
		{
			static const struct { int axis, sign; } stick_dirs[4] = { { 1, 1 }, { 1, -1 }, { 0, -1 }, { 0, 1 } };
			int dir;

			for (dir = 0; dir < 4; dir++)
			{
				int value = sticks[stick_dirs[dir].axis] * stick_dirs[dir].sign;
				int held_before = note_seen[port] && (note_prev[port] & (1u << (STICK_SHIFT + dir)));

				if (value >= (held_before ? STICK_LET_GO : STICK_HELD))
					now |= 1u << (STICK_SHIFT + dir);
			}
		}
		pressed = note_seen[port] ? now & ~note_prev[port] : 0;
		/* (a direction steps when pressed and again as it is held; one held when the box opened, or a port read again
		after a gap, steps only once let go and pressed again) */
		{
			static const unsigned dir_buttons[4] = { PAD_DPAD_UP, PAD_DPAD_DOWN, PAD_DPAD_LEFT, PAD_DPAD_RIGHT };
			static const unsigned dir_edges[4] = { AE_TEST_RUN_PAD_UP, AE_TEST_RUN_PAD_DOWN, AE_TEST_RUN_PAD_LEFT,
				AE_TEST_RUN_PAD_RIGHT };
			unsigned long ms = system_milliseconds();
			int dir;

			for (dir = 0; dir < 4; dir++)
			{
				int down = (now & (dir_buttons[dir] | 1u << (STICK_SHIFT + dir))) != 0;

				if (!note_seen[port])
					ae_repeat_seed(&note_repeat[port][dir], down, ms);
				else if (ae_repeat_update(&note_repeat[port][dir], down, ms))
					note_edges |= dir_edges[dir];
			}
		}
		if (pressed & (1u << (ANALOG_SHIFT + face_button(PAD_A))))
			note_edges |= AE_TEST_RUN_PAD_A;
		if (pressed & (1u << (ANALOG_SHIFT + face_button(PAD_B))))
			note_edges |= AE_TEST_RUN_PAD_B;
		if (pressed & (1u << (ANALOG_SHIFT + face_button(PAD_X))))
			note_edges |= AE_TEST_RUN_PAD_X;
		if (pressed & (1u << (ANALOG_SHIFT + face_button(PAD_Y))))
			note_edges |= AE_TEST_RUN_PAD_Y;
		if (pressed & (1u << (ANALOG_SHIFT + PAD_WHITE)))
			note_edges |= AE_TEST_RUN_PAD_LB;
		if (pressed & (1u << (ANALOG_SHIFT + PAD_BLACK)))
			note_edges |= AE_TEST_RUN_PAD_RB;
		if (pressed & PAD_START)
			note_edges |= AE_TEST_RUN_PAD_START;
		note_prev[port] = now;
		note_seen[port] = 1;
		note_polled[port] = polls;
		note_swallow[port] = now & ((1u << STICK_SHIFT) - 1);
		pad_clear(pad);
		pad_held[port] = 0;
		pad_polled[port] = polls;
		return 1;
	}
	note_seen[port] = 0;
	note_swallow[port] &= now;
	if (!note_swallow[port])
		return 0;
	/* (the hold-over takes just those buttons: another button pressed meanwhile reaches the game, the sticks too) */
	pad_clear_bits(buttons, analog_buttons, note_swallow[port]);
	return 0;
}

unsigned ae_test_run_input_note_edges(void)
{
	unsigned edges = note_edges;

	note_edges = 0;
	return edges;
}

int ae_test_run_input_device(void)
{
	int scheme;

	/* (AE's own record, as the HUD's key prompts and the menus' footers read it: the keyboard and mouse (keys, buttons,
	motion) after any pad, else the pad's family as the platform names it) */
	if (!enabled || ae_platform_keyboard_last_used())
		return AE_DEVICE_KEYBOARD_MOUSE;
	scheme = ae_platform_input_scheme();
	return scheme >= AE_DEVICE_XBOX && scheme <= AE_DEVICE_NINTENDO ? scheme : AE_DEVICE_XBOX;
}

void ae_test_run_input_pad(short port, XINPUT_GAMEPAD *pad)
{
	unsigned short *buttons = &pad->wButtons;
	unsigned char *analog_buttons = pad->bAnalogButtons;
	static const struct { int analog; int bit; } chords[] =
	{
		{ PAD_A, AE_TEST_RUN_KEY_PASS - 1 }, { PAD_B, AE_TEST_RUN_KEY_FAIL - 1 }, { PAD_X, AE_TEST_RUN_KEY_NOTE - 1 },
		{ PAD_Y, AE_TEST_RUN_KEY_SKIP - 1 }, { PAD_WHITE, AE_TEST_RUN_KEY_BACK - 1 }, { PAD_BLACK, BIT_HIDE },
	};
	unsigned held = 0;
	int index;

	if (port < 0 || port >= PORTS)
		return;
	/* (the box's, and its hold-over, run whether or not the reader is on: a button held when a run ends stays taken too) */
	if (note_pad(port, pad))
		return;
	if (*buttons & PAD_BACK)
	{
		for (index = 0; index < (int)(sizeof(chords) / sizeof(chords[0])); index++)
		{
			int analog = face_button(chords[index].analog);

			if (analog_buttons[analog] >= PAD_PRESSED)
			{
				held |= 1u << chords[index].bit;
				if (enabled)
					analog_buttons[analog] = 0;
			}
		}
		if (*buttons & PAD_DPAD_UP)
		{
			held |= 1u << (AE_TEST_RUN_KEY_RELOAD - 1);
			if (enabled)
				*buttons &= (unsigned short)~PAD_DPAD_UP;
		}
	}
	pad_held[port] = held;
	pad_polled[port] = polls;
}
