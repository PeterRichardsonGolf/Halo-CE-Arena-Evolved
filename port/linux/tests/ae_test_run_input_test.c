/* ae_test_run_input_test.c: TEST RUN's verdict chords. A pad's BACK + A is PASS and never SKIP when its BACK also sends
the Tab key (Y in the menus). The chord is read from the pad's own buttons, not the keyboard-merged pad. The test also
checks the keyboard's F7 and a scripted run-by-hand chord (ae_test_run_input_press). */

#include <stdio.h>
#include <string.h>

/* (the SDK stand-in's pad, as port/include/xdk/xdk_pdb.h has it, without the rest of that header: the unit compiles on any
machine) */
#define HALO_XDK_PDB_H
typedef struct _XINPUT_GAMEPAD
{
	unsigned short wButtons;
	unsigned char bAnalogButtons[8];
	short sThumbLX, sThumbLY, sThumbRX, sThumbRY;
} XINPUT_GAMEPAD;

#include "../game/ae_test_run_input.c"

static int failures;
#define CHECK(x) do { if (!(x)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #x); failures++; } } while (0)

/* the platform's and the note box's, as the game has them: no box open, an Xbox pad, no swap */
int ae_test_run_typing(void) { return 0; }
void ae_platform_note_keyboard_used(void) { }
int ae_platform_keyboard_last_used(void) { return 0; }
int ae_platform_input_scheme(void) { return 1; }
int ae_platform_ab_swap(int player) { (void)player; return 0; }
int ae_platform_button_prompts(int player) { (void)player; return 0; }
unsigned long system_milliseconds(void) { static unsigned long now; return now += 16; }
int ae_repeat_update(struct ae_repeat *repeat, int held, unsigned long now_ms)
{
	(void)repeat; (void)held; (void)now_ms;
	return 0;
}
void ae_repeat_seed(struct ae_repeat *repeat, int held, unsigned long now_ms) { (void)repeat; (void)held; (void)now_ms; }

enum { BACK = 0x0020, A = 0, Y = 3, PRESSED = 0xff };

/* one frame: the keyboard's actions, then port 0's pad (own: what it holds; merged: with the keyboard's Tab as Y), then
the reader */
static enum ae_test_run_key frame(unsigned long long keys, int back, int a, int tab_as_y)
{
	XINPUT_GAMEPAD own, merged;
	unsigned short buttons = 0;

	memset(&own, 0, sizeof(own));
	if (back)
		own.wButtons |= BACK;
	if (a)
		own.bAnalogButtons[A] = PRESSED;
	merged = own;
	if (tab_as_y)
		merged.bAnalogButtons[Y] = PRESSED;
	ae_test_run_input_keys(keys, &buttons);
	ae_test_run_input_pad_own(0, &own, &merged);
	return ae_test_run_input_pressed();
}

int main(void)
{
	int index;

	ae_test_run_input_enable(1);
	/* (a pad whose BACK also sends Tab, which the menus count as Y) */
	CHECK(frame(0, 1, 0, 1) == AE_TEST_RUN_KEY_NONE);
	CHECK(frame(0, 1, 1, 1) == AE_TEST_RUN_KEY_PASS);
	CHECK(frame(0, 0, 0, 0) == AE_TEST_RUN_KEY_NONE);
	/* (BACK and A in the same frame: PASS) */
	CHECK(frame(0, 1, 1, 1) == AE_TEST_RUN_KEY_PASS);
	frame(0, 0, 0, 0);
	/* (the keyboard's F7) */
	CHECK(frame(1ULL << HALO_KEYBOARD_TEST_PASS, 0, 0, 0) == AE_TEST_RUN_KEY_PASS);
	frame(0, 0, 0, 0);
	/* (the scripted chord: pressed on port 0 where a real pad is read, PASS the next read) */
	for (index = 0; index < 8; index++)
		frame(0, 0, 0, 0);
	ae_test_run_input_press(AE_TEST_RUN_KEY_PASS);
	CHECK(frame(0, 0, 0, 0) == AE_TEST_RUN_KEY_PASS);
	for (index = 0; index < 8; index++)
		CHECK(frame(0, 0, 0, 0) != AE_TEST_RUN_KEY_SKIP);
	if (failures)
		printf("%d failures\n", failures);
	return failures != 0;
}
