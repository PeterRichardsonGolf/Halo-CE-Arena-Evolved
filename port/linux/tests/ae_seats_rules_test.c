#include <stdio.h>
#include <string.h>
#include "ae_seats.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

int main(void)
{
	struct ae_seats seats;
	int moved[AE_SEATS_MAXIMUM];
	char long_key[200];

	/* the first to press is player 1, whatever the device: a pad first, then the keyboard, then another pad */
	ae_seats_clear(&seats);
	CHECK(ae_seats_join(&seats, AE_SEAT_PAD, 7, "pad-a") == 0);
	CHECK(ae_seats_join(&seats, AE_SEAT_KEYBOARD, 0, NULL) == 1);
	CHECK(ae_seats_join(&seats, AE_SEAT_PAD, 9, "pad-b") == 2);
	CHECK(ae_seats_keyboard(&seats) == 1 && ae_seats_of_pad(&seats, 9) == 2 && ae_seats_of_pad(&seats, 8) == -1);
	/* pressing again keeps the seat */
	CHECK(ae_seats_join(&seats, AE_SEAT_PAD, 7, "pad-a") == 0);
	CHECK(ae_seats_join(&seats, AE_SEAT_KEYBOARD, 0, NULL) == 1);
	CHECK(ae_seats_taken(&seats) == 3 && ae_seats_present(&seats) == 3);
	/* a fourth, then a fifth device: full */
	CHECK(ae_seats_join(&seats, AE_SEAT_PAD, 11, "pad-c") == 3);
	CHECK(ae_seats_join(&seats, AE_SEAT_PAD, 12, "pad-d") == -1);
	/* nonsense: no kind, a pad without an id */
	CHECK(ae_seats_join(&seats, AE_SEAT_EMPTY, 0, NULL) == -1);
	CHECK(ae_seats_join(&seats, AE_SEAT_PAD, 0, "x") == -1);
	CHECK(ae_seats_join(NULL, AE_SEAT_KEYBOARD, 0, NULL) == -1);

	/* the keyboard leaves: a gap, later seats keep their numbers; the next to join takes it */
	ae_seats_leave(&seats, 1);
	CHECK(seats.seat[1].kind == AE_SEAT_EMPTY && ae_seats_of_pad(&seats, 9) == 2);
	CHECK(ae_seats_join(&seats, AE_SEAT_PAD, 12, "pad-d") == 1);
	ae_seats_leave(&seats, 1);
	ae_seats_leave(&seats, 9);
	ae_seats_leave(&seats, -1);

	/* a pad unplugged: its seat held; the same pad back (a new id, the same key) takes it by itself */
	CHECK(ae_seats_unplugged(&seats, 9) == 2);
	CHECK(seats.seat[2].held && seats.seat[2].pad_id == 0);
	CHECK(ae_seats_taken(&seats) == 3 && ae_seats_present(&seats) == 2);
	CHECK(ae_seats_unplugged(&seats, 99) == -1);
	CHECK(ae_seats_plugged(&seats, 21, "pad-x") == -1);
	CHECK(ae_seats_plugged(&seats, 21, "pad-b") == 2);
	CHECK(!seats.seat[2].held && seats.seat[2].pad_id == 21);

	/* held again; another device's START takes the held seat before the empty one (spec 7.2) */
	CHECK(ae_seats_unplugged(&seats, 21) == 2);
	CHECK(ae_seats_join(&seats, AE_SEAT_KEYBOARD, 0, NULL) == 2);
	CHECK(seats.seat[2].kind == AE_SEAT_KEYBOARD && !seats.seat[2].held && seats.seat[2].key[0] == 0);
	/* the old pad back now: no seat held for it, it is a new device (joins with START only) */
	CHECK(ae_seats_plugged(&seats, 22, "pad-b") == -1);

	/* a held pad pressing START (a new id) gets its own seat back, not another */
	ae_seats_clear(&seats);
	CHECK(ae_seats_join(&seats, AE_SEAT_PAD, 1, "one") == 0);
	CHECK(ae_seats_join(&seats, AE_SEAT_PAD, 2, "two") == 1);
	CHECK(ae_seats_unplugged(&seats, 1) == 0 && ae_seats_unplugged(&seats, 2) == 1);
	CHECK(ae_seats_join(&seats, AE_SEAT_PAD, 5, "two") == 1);

	/* the gaps closed: P1, _, P3, _ -> P1, P2 */
	ae_seats_clear(&seats);
	ae_seats_join(&seats, AE_SEAT_KEYBOARD, 0, NULL);
	ae_seats_join(&seats, AE_SEAT_PAD, 3, "b");
	ae_seats_join(&seats, AE_SEAT_PAD, 4, "c");
	ae_seats_leave(&seats, 1);
	CHECK(ae_seats_close_gaps(&seats, moved) == 2);
	CHECK(moved[0] == 0 && moved[1] == -1 && moved[2] == 1 && moved[3] == -1);
	CHECK(seats.seat[0].kind == AE_SEAT_KEYBOARD && seats.seat[1].pad_id == 4 && seats.seat[2].kind == AE_SEAT_EMPTY);
	CHECK(ae_seats_close_gaps(NULL, moved) == 0);

	/* a key longer than the seat keeps: cut, and still matched by the same long key */
	memset(long_key, 'k', sizeof(long_key) - 1);
	long_key[sizeof(long_key) - 1] = 0;
	ae_seats_clear(&seats);
	CHECK(ae_seats_join(&seats, AE_SEAT_PAD, 5, long_key) == 0);
	CHECK(strlen(seats.seat[0].key) == AE_SEAT_KEY_LENGTH - 1);
	CHECK(ae_seats_unplugged(&seats, 5) == 0 && ae_seats_plugged(&seats, 6, long_key) == 0);
	/* no key: never matched as "the same pad" */
	ae_seats_clear(&seats);
	CHECK(ae_seats_join(&seats, AE_SEAT_PAD, 5, "") == 0);
	CHECK(ae_seats_unplugged(&seats, 5) == 0 && ae_seats_plugged(&seats, 6, "") == -1);

	/* the picker: a free pad into a seat; a taken one swaps; never taken from a player to fill an empty seat */
	ae_seats_clear(&seats);
	CHECK(ae_seats_join(&seats, AE_SEAT_KEYBOARD, 0, NULL) == 0);
	CHECK(ae_seats_join(&seats, AE_SEAT_PAD, 3, "c") == 1);
	CHECK(ae_seats_assign(&seats, 1, AE_SEAT_PAD, 4, "d") == 1);
	CHECK(seats.seat[1].pad_id == 4 && ae_seats_of_pad(&seats, 3) == -1 && !strcmp(seats.seat[1].key, "d"));
	CHECK(ae_seats_assign(&seats, 1, AE_SEAT_PAD, 4, "d") == 1);
	CHECK(ae_seats_assign(&seats, 0, AE_SEAT_PAD, 4, "d") == 1);
	CHECK(seats.seat[0].pad_id == 4 && seats.seat[1].kind == AE_SEAT_KEYBOARD);
	CHECK(ae_seats_assign(&seats, 2, AE_SEAT_PAD, 4, "d") == -1);
	CHECK(seats.seat[2].kind == AE_SEAT_EMPTY && seats.seat[0].pad_id == 4);
	CHECK(ae_seats_assign(&seats, 2, AE_SEAT_PAD, 5, "e") == 2);
	CHECK(ae_seats_assign(&seats, 3, AE_SEAT_KEYBOARD, 0, NULL) == -1);
	/* a held seat takes a free device (its pad gone), never another player's */
	CHECK(ae_seats_unplugged(&seats, 5) == 2);
	CHECK(ae_seats_assign(&seats, 2, AE_SEAT_KEYBOARD, 0, NULL) == -1);
	CHECK(ae_seats_assign(&seats, 2, AE_SEAT_PAD, 6, "f") == 2);
	CHECK(!seats.seat[2].held && seats.seat[2].pad_id == 6);
	CHECK(ae_seats_assign(&seats, 4, AE_SEAT_PAD, 7, "g") == -1 && ae_seats_assign(&seats, 0, AE_SEAT_PAD, 0, "") == -1);
	CHECK(ae_seats_assign(NULL, 0, AE_SEAT_KEYBOARD, 0, NULL) == -1 && ae_seats_assign(&seats, 0, AE_SEAT_EMPTY, 0, NULL) == -1);

	/* short names */
	{
		char name[24];

		ae_seats_short_name(AE_SEAT_FAMILY_KEYBOARD, NULL, 2, name, sizeof(name));
		CHECK(!strcmp(name, "KEYBOARD"));
		ae_seats_short_name(AE_SEAT_FAMILY_XBOX, "Xbox Series X Controller", 0, name, sizeof(name));
		CHECK(!strcmp(name, "XBOX"));
		ae_seats_short_name(AE_SEAT_FAMILY_PLAYSTATION, "DualSense Wireless Controller", 2, name, sizeof(name));
		CHECK(!strcmp(name, "DUALSENSE 2"));
		ae_seats_short_name(AE_SEAT_FAMILY_PLAYSTATION, "PS4 Controller", 0, name, sizeof(name));
		CHECK(!strcmp(name, "DUALSHOCK"));
		ae_seats_short_name(AE_SEAT_FAMILY_NINTENDO, "Nintendo Switch Pro Controller", 1, name, sizeof(name));
		CHECK(!strcmp(name, "SWITCH PRO 1"));
		ae_seats_short_name(AE_SEAT_FAMILY_NINTENDO, "Nintendo Switch Joy-Con (L)", 0, name, sizeof(name));
		CHECK(!strcmp(name, "JOY-CON"));
		ae_seats_short_name(AE_SEAT_FAMILY_OTHER, "8BitDo Something", 3, name, sizeof(name));
		CHECK(!strcmp(name, "PAD 3"));
		ae_seats_short_name(AE_SEAT_FAMILY_NINTENDO, NULL, 0, name, 4);
		CHECK(!strcmp(name, "SWI"));
	}

	if (failures)
		return 1;
	printf("ae_seats_rules_test: all passed\n");
	return 0;
}
