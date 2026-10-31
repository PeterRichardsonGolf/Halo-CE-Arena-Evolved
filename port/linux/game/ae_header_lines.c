/*
AE_HEADER_LINES.C

The scoreboard header's text (ae_header_lines.h). Pure: no C library wide
string calls (the game's wchar_t is 16-bit).
*/

#include <string.h>

#include "ae_header_lines.h"

static long wlength(const wchar_t *text)
{
	long length = 0;

	while (text && text[length])
		length++;
	return length;
}

/* appends to out (count characters with the NUL), cut, never overrun */
static void append(wchar_t *out, long count, const wchar_t *text)
{
	long length = wlength(out);

	while (text && *text && length < count - 1)
		out[length++] = *text++;
	out[length] = 0;
}

static void append_ascii(wchar_t *out, long count, const char *text)
{
	long length = wlength(out);

	while (text && *text && length < count - 1)
		out[length++] = (wchar_t)(unsigned char)*text++;
	out[length] = 0;
}

static void append_long(wchar_t *out, long count, long value)
{
	char digits[24];
	char reversed[24];
	unsigned long magnitude = value < 0 ? (unsigned long)(-(value + 1)) + 1 : (unsigned long)value;
	long n = 0, i;

	do
	{
		reversed[n++] = (char)('0' + (int)(magnitude % 10));
		magnitude /= 10;
	} while (magnitude && n < 22);
	i = 0;
	if (value < 0)
		digits[i++] = '-';
	while (n > 0)
		digits[i++] = reversed[--n];
	digits[i] = 0;
	append_ascii(out, count, digits);
}

static void append_time(wchar_t *out, long count, long seconds)
{
	char tail[4];

	if (seconds < 0)
		seconds = 0;
	append_long(out, count, seconds / 60);
	tail[0] = ':';
	tail[1] = (char)('0' + (int)(seconds % 60 / 10));
	tail[2] = (char)('0' + (int)(seconds % 10));
	tail[3] = 0;
	append_ascii(out, count, tail);
}

/* whether text begins with prefix, then a space */
static boolean starts_word(const wchar_t *text, const wchar_t *prefix)
{
	long n = wlength(prefix);
	long i;

	if (n == 0)
		return FALSE;
	for (i = 0; i < n; i++)
		if (text[i] != prefix[i])
			return FALSE;
	return (boolean)(text[n] == L' ');
}

/* the mode: "<FFA|TEAM> <set> <mode>" with "<set> " taken out (and "<FFA|TEAM> AE <set> <mode>", a set whose
names carry AE first: "FFA AE OCTAGON BR" in set OCTAGON is "FFA BR"); any other name whole */
static void split_mode(const wchar_t *display, const wchar_t *set, wchar_t *mode, long count)
{
	static const wchar_t *const kinds[] = { L"FFA", L"TEAM" };
	unsigned k;

	mode[0] = 0;
	if (!display)
		return;
	for (k = 0; k < sizeof(kinds) / sizeof(kinds[0]); k++)
	{
		long kind = wlength(kinds[k]);

		if (starts_word(display, kinds[k]) && set && set[0] && starts_word(display + kind + 1, set))
		{
			append(mode, count, kinds[k]);
			append(mode, count, display + kind + 1 + wlength(set));
			return;
		}
		if (starts_word(display, kinds[k]) && set && set[0] && starts_word(display + kind + 1, L"AE") &&
			starts_word(display + kind + 4, set))
		{
			append(mode, count, kinds[k]);
			append(mode, count, display + kind + 4 + wlength(set));
			return;
		}
	}
	append(mode, count, display);
}

void ae_scoreboard_header_map_name(const char *map_path, wchar_t *out, long count)
{
	static const struct { const char *file; const char *name; } stock[] = {
		{ "beavercreek", "BATTLE CREEK" }, { "sidewinder", "SIDEWINDER" }, { "damnation", "DAMNATION" },
		{ "ratrace", "RAT RACE" }, { "prisoner", "PRISONER" }, { "hangemhigh", "HANG 'EM HIGH" },
		{ "chillout", "CHILL OUT" }, { "carousel", "DERELICT" }, { "boardingaction", "BOARDING ACTION" },
		{ "bloodgulch", "BLOOD GULCH" }, { "wizard", "WIZARD" }, { "putput", "CHIRON TL34" },
		{ "longest", "LONGEST" } };
	char file[64];
	const char *base;
	long length = 0;
	unsigned i;

	if (count <= 0)
		return;
	out[0] = 0;
	if (!map_path)
		return;
	base = strrchr(map_path, '\\');
	base = base ? base + 1 : map_path;
	while (base[length] && base[length] != '@' && length < (long)sizeof(file) - 1)
	{
		file[length] = base[length];
		length++;
	}
	file[length] = 0;
	for (i = 0; i < sizeof(stock) / sizeof(stock[0]); i++)
	{
		if (!strcmp(file, stock[i].file))
		{
			append_ascii(out, count, stock[i].name);
			return;
		}
	}
	for (i = 0; file[i]; i++)
		if (file[i] >= 'a' && file[i] <= 'z')
			file[i] = (char)(file[i] - 'a' + 'A');
		else if (file[i] == '_')
			file[i] = ' ';
	append_ascii(out, count, file);
}

void ae_scoreboard_header_lines(const struct ae_header_input *input, struct ae_header_lines *lines)
{
	wchar_t *goal = lines->goal_time;
	long count = AE_HEADER_TEXT_LENGTH;
	boolean has_goal;

	memset(lines, 0, sizeof(*lines));
	if (!input)
		return;
	split_mode(input->display_name, input->set_name, lines->mode, count);
	if (input->map_name && input->map_name[0])
		append(lines->map, count, input->map_name);
	else
		ae_scoreboard_header_map_name(input->map_path, lines->map, count);
	append(lines->tag, count, input->set_name);

	has_goal = (boolean)(input->score_to_win > 0);
	if (has_goal)
	{
		switch (input->goal)
		{
		case _ae_goal_captures:
			append_long(goal, count, input->score_to_win);
			append_ascii(goal, count, input->score_to_win == 1 ? " CAPTURE" : " CAPTURES");
			break;
		case _ae_goal_time:
			append_ascii(goal, count, "FIRST TO ");
			append_time(goal, count, input->score_to_win);
			break;
		case _ae_goal_laps:
			append_ascii(goal, count, "FIRST TO ");
			append_long(goal, count, input->score_to_win);
			append_ascii(goal, count, input->score_to_win == 1 ? " LAP" : " LAPS");
			break;
		case _ae_goal_rally_points:
			append_ascii(goal, count, "FIRST TO ");
			append_long(goal, count, input->score_to_win);
			append_ascii(goal, count, " POINTS");
			break;
		case _ae_goal_rounds:
			append_long(goal, count, input->score_to_win);
			append_ascii(goal, count, input->score_to_win == 1 ? " ROUND TO WIN" : " ROUNDS TO WIN");
			break;
		default:
			append_ascii(goal, count, "FIRST TO ");
			append_long(goal, count, input->score_to_win);
			break;
		}
	}
	if (input->goal == _ae_goal_rounds && input->round_seconds > 0)
	{
		if (has_goal)
			append(goal, count, AE_HEADER_SEPARATOR);
		append_time(goal, count, input->round_seconds);
		append_ascii(goal, count, " ROUNDS");
		return;
	}
	if (!has_goal && input->time_limit_minutes <= 0)
	{
		append_ascii(goal, count, "NO LIMIT");
		return;
	}
	if (has_goal)
		append(goal, count, AE_HEADER_SEPARATOR);
	if (input->time_limit_minutes <= 0)
		append_ascii(goal, count, "NO TIME LIMIT");
	else if (input->clock_counts_up)
		append_ascii(goal, count, "CLOCK COUNTS UP");
	else if (input->time_left_shown)
	{
		append_time(goal, count, input->time_left_seconds);
		append_ascii(goal, count, " LEFT");
	}
	else
		append_time(goal, count, input->time_limit_minutes * 60);
}
