/*
AE_SEATS_RULES.C

Split screen's seats: who sits where (ae_seats.h). No SDL here.
*/

#include <string.h>

#include "ae_seats.h"

static int valid(
	int seat)
{
	return seat >= 0 && seat < AE_SEATS_MAXIMUM;
}

void ae_seats_clear(
	struct ae_seats *seats)
{
	if (seats)
		memset(seats, 0, sizeof(*seats));
}

int ae_seats_keyboard(
	struct ae_seats const *seats)
{
	int seat;

	for (seat = 0; seats && seat < AE_SEATS_MAXIMUM; seat++)
	{
		if (seats->seat[seat].kind == AE_SEAT_KEYBOARD)
			return seat;
	}
	return -1;
}

int ae_seats_of_pad(
	struct ae_seats const *seats,
	unsigned long pad_id)
{
	int seat;

	for (seat = 0; seats && pad_id && seat < AE_SEATS_MAXIMUM; seat++)
	{
		if (seats->seat[seat].kind == AE_SEAT_PAD && seats->seat[seat].pad_id == pad_id)
			return seat;
	}
	return -1;
}

int ae_seats_held_by_key(
	struct ae_seats const *seats,
	char const *key)
{
	int seat;

	for (seat = 0; seats && key && key[0] && seat < AE_SEATS_MAXIMUM; seat++)
	{
		if (seats->seat[seat].kind == AE_SEAT_PAD && seats->seat[seat].held &&
			!strncmp(seats->seat[seat].key, key, AE_SEAT_KEY_LENGTH - 1))
			return seat;
	}
	return -1;
}

static void seat_set(
	struct ae_seat *seat,
	int kind,
	unsigned long pad_id,
	char const *key)
{
	memset(seat, 0, sizeof(*seat));
	seat->kind = kind;
	if (kind != AE_SEAT_PAD)
		return;
	seat->pad_id = pad_id;
	if (key)
		strncpy(seat->key, key, AE_SEAT_KEY_LENGTH - 1);
}

int ae_seats_join(
	struct ae_seats *seats,
	int kind,
	unsigned long pad_id,
	char const *key)
{
	int seat;

	if (!seats || (kind != AE_SEAT_KEYBOARD && kind != AE_SEAT_PAD) || (kind == AE_SEAT_PAD && !pad_id))
		return -1;
	seat = kind == AE_SEAT_KEYBOARD ? ae_seats_keyboard(seats) : ae_seats_of_pad(seats, pad_id);
	if (seat >= 0)
		return seat;
	/* (its own held seat first: the same pad back) */
	if (kind == AE_SEAT_PAD && (seat = ae_seats_held_by_key(seats, key)) >= 0)
	{
		seats->seat[seat].pad_id = pad_id;
		seats->seat[seat].held = 0;
		return seat;
	}
	for (seat = 0; seat < AE_SEATS_MAXIMUM; seat++)
	{
		if (seats->seat[seat].kind == AE_SEAT_PAD && seats->seat[seat].held)
		{
			seat_set(&seats->seat[seat], kind, pad_id, key);
			return seat;
		}
	}
	for (seat = 0; seat < AE_SEATS_MAXIMUM; seat++)
	{
		if (seats->seat[seat].kind == AE_SEAT_EMPTY)
		{
			seat_set(&seats->seat[seat], kind, pad_id, key);
			return seat;
		}
	}
	return -1;
}

void ae_seats_leave(
	struct ae_seats *seats,
	int seat)
{
	if (seats && valid(seat))
		memset(&seats->seat[seat], 0, sizeof(seats->seat[seat]));
}

int ae_seats_unplugged(
	struct ae_seats *seats,
	unsigned long pad_id)
{
	int seat = ae_seats_of_pad(seats, pad_id);

	if (seat < 0)
		return -1;
	seats->seat[seat].pad_id = 0;
	seats->seat[seat].held = 1;
	return seat;
}

int ae_seats_plugged(
	struct ae_seats *seats,
	unsigned long pad_id,
	char const *key)
{
	int seat;

	if (!seats || !pad_id || ae_seats_of_pad(seats, pad_id) >= 0)
		return -1;
	seat = ae_seats_held_by_key(seats, key);
	if (seat < 0)
		return -1;
	seats->seat[seat].pad_id = pad_id;
	seats->seat[seat].held = 0;
	return seat;
}

int ae_seats_taken(
	struct ae_seats const *seats)
{
	int seat, count = 0;

	for (seat = 0; seats && seat < AE_SEATS_MAXIMUM; seat++)
		count += seats->seat[seat].kind != AE_SEAT_EMPTY;
	return count;
}

int ae_seats_present(
	struct ae_seats const *seats)
{
	int seat, count = 0;

	for (seat = 0; seats && seat < AE_SEATS_MAXIMUM; seat++)
		count += seats->seat[seat].kind != AE_SEAT_EMPTY && !seats->seat[seat].held;
	return count;
}

int ae_seats_close_gaps(
	struct ae_seats *seats,
	int moved[AE_SEATS_MAXIMUM])
{
	struct ae_seats closed;
	int seat, next = 0;

	if (!seats)
		return 0;
	memset(&closed, 0, sizeof(closed));
	for (seat = 0; seat < AE_SEATS_MAXIMUM; seat++)
	{
		if (moved)
			moved[seat] = -1;
		if (seats->seat[seat].kind == AE_SEAT_EMPTY)
			continue;
		closed.seat[next] = seats->seat[seat];
		if (moved)
			moved[seat] = next;
		next++;
	}
	*seats = closed;
	return next;
}
