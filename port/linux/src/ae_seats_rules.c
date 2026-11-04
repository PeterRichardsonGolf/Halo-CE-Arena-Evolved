/*
AE_SEATS_RULES.C

Split screen's seats: who sits where (ae_seats.h). No SDL here.
*/

#include <stdio.h>
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

static int seat_of_device(
	struct ae_seats const *seats,
	int kind,
	unsigned long pad_id)
{
	return kind == AE_SEAT_KEYBOARD ? ae_seats_keyboard(seats) : ae_seats_of_pad(seats, pad_id);
}

int ae_seats_assign(
	struct ae_seats *seats,
	int seat,
	int kind,
	unsigned long pad_id,
	char const *key)
{
	struct ae_seat mine;
	int other;

	if (!seats || !valid(seat) || (kind != AE_SEAT_KEYBOARD && kind != AE_SEAT_PAD) ||
		(kind == AE_SEAT_PAD && !pad_id))
	{
		return -1;
	}
	other = seat_of_device(seats, kind, pad_id);
	if (other == seat)
		return seat;
	if (other < 0)
	{
		seat_set(&seats->seat[seat], kind, pad_id, key);
		return seat;
	}
	/* (another player's: a swap, only between two players with devices) */
	if (seats->seat[seat].kind == AE_SEAT_EMPTY || seats->seat[seat].held)
		return -1;
	mine = seats->seat[seat];
	seats->seat[seat] = seats->seat[other];
	seats->seat[other] = mine;
	return other;
}

static int contains(
	char const *text,
	char const *word)
{
	size_t length = strlen(word);

	for (; text && *text; text++)
	{
		size_t i;

		for (i = 0; i < length && text[i]; i++)
		{
			char c = text[i];

			if (c >= 'a' && c <= 'z')
				c = (char)(c - 'a' + 'A');
			if (c != word[i])
				break;
		}
		if (i == length)
			return 1;
	}
	return 0;
}

void ae_seats_short_name(
	int family,
	char const *name,
	int number,
	char *out,
	int size)
{
	char const *word = "PAD";

	if (!out || size <= 0)
		return;
	switch (family)
	{
	case AE_SEAT_FAMILY_KEYBOARD:
		word = "KEYBOARD";
		number = 0;
		break;
	case AE_SEAT_FAMILY_XBOX:
		word = "XBOX";
		break;
	case AE_SEAT_FAMILY_PLAYSTATION:
		word = contains(name, "PS4") || contains(name, "DUALSHOCK") ? "DUALSHOCK" : "DUALSENSE";
		break;
	case AE_SEAT_FAMILY_NINTENDO:
		word = contains(name, "JOY-CON") || contains(name, "JOYCON") ? "JOY-CON" : "SWITCH PRO";
		break;
	}
	if (number > 0)
		snprintf(out, (size_t)size, "%s %d", word, number);
	else
		snprintf(out, (size_t)size, "%s", word);
}
