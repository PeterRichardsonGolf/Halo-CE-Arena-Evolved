/*
AE_THEATER_RULES.C

See ae_theater_rules.h.
*/

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "ae_theater_rules.h"

static const long speeds[AE_THEATER_SPEEDS] = { 25, 50, 100, 200, 400 };

void ae_theater_time_text(long seconds, char *text, size_t size)
{
	if (!text || !size)
		return;
	if (seconds < 0)
		seconds = 0;
	if (seconds >= 3600)
		snprintf(text, size, "%ld:%02ld:%02ld", seconds / 3600, seconds / 60 % 60, seconds % 60);
	else
		snprintf(text, size, "%ld:%02ld", seconds / 60, seconds % 60);
}

void ae_theater_title_text(const char *file_name, char *text, size_t size)
{
	size_t index, length = 0;
	char map[64];
	int stamp = 1;

	if (!text || !size)
		return;
	text[0] = 0;
	if (!file_name)
		return;
	/* (YYYYMMDD-HHMMSS_map.ocreplay) */
	for (index = 0; index < 15 && stamp; index++)
		stamp = index == 8 ? file_name[index] == '-' : isdigit((unsigned char)file_name[index]) != 0;
	if (!stamp || file_name[15] != '_')
	{
		snprintf(text, size, "%s", file_name);
		return;
	}
	for (index = 16; file_name[index] && file_name[index] != '.' && length + 1 < sizeof(map); index++)
		map[length++] = file_name[index] == '_' ? ' ' : (char)toupper((unsigned char)file_name[index]);
	map[length] = 0;
	snprintf(text, size, "%s  %.4s-%.2s-%.2s %.2s:%.2s", length ? map : "MAP", file_name, file_name + 4, file_name + 6,
		file_name + 9, file_name + 11);
}

long ae_theater_speed_step(long percent, int direction)
{
	int nearest = 0, index;

	for (index = 1; index < AE_THEATER_SPEEDS; index++)
	{
		long near_gap = percent > speeds[nearest] ? percent - speeds[nearest] : speeds[nearest] - percent;
		long gap = percent > speeds[index] ? percent - speeds[index] : speeds[index] - percent;

		if (gap < near_gap)
			nearest = index;
	}
	/* (an unlisted speed first goes to its nearest step; a listed one steps from itself) */
	if (speeds[nearest] != percent)
		return speeds[nearest];
	nearest += direction > 0 ? 1 : direction < 0 ? -1 : 0;
	if (nearest < 0)
		nearest = 0;
	if (nearest >= AE_THEATER_SPEEDS)
		nearest = AE_THEATER_SPEEDS - 1;
	return speeds[nearest];
}

void ae_theater_speed_text(long percent, char *text, size_t size)
{
	if (!text || !size)
		return;
	if (percent == 25)
		snprintf(text, size, "0.25x");
	else if (percent == 50)
		snprintf(text, size, "0.5x");
	else if (percent % 100 == 0)
		snprintf(text, size, "%ldx", percent / 100);
	else
		snprintf(text, size, "%ld%%", percent);
}

long ae_theater_skip_target(long seconds, int direction, long length)
{
	long target = seconds + (direction > 0 ? AE_THEATER_SKIP_SECONDS : direction < 0 ? -AE_THEATER_SKIP_SECONDS : 0);

	if (length < 0)
		length = 0;
	return target < 0 ? 0 : target > length ? length : target;
}

float ae_theater_progress(long position, long length)
{
	if (length <= 0 || position <= 0)
		return 0.0f;
	return position >= length ? 1.0f : (float)position / (float)length;
}

int ae_theater_next_player(const int *players, int count, int current, int backwards)
{
	int index;

	if (!players || count <= 0)
		return -1;
	for (index = 0; index < count; index++)
		if (players[index] == current)
			return backwards ? players[(index + count - 1) % count] : players[(index + 1) % count];
	/* (not in the list: the first after current going forward, the last before it going back) */
	if (current < 0)
		return backwards ? players[count - 1] : players[0];
	if (backwards)
	{
		for (index = count - 1; index >= 0; index--)
			if (players[index] < current)
				return players[index];
		return players[count - 1];
	}
	for (index = 0; index < count; index++)
		if (players[index] > current)
			return players[index];
	return players[0];
}
