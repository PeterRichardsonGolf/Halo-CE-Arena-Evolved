/* The revive's HUD rules (see the header) */
#include <math.h>

#include "ae_revive_hud_rules.h"
#include "ae_round_rules.h"

static float const spans[AE_REVIVE_SEGMENTS][2] = {
	{ 0.0f, 45.0f }, { 45.0f, 90.0f }, { 90.0f, 160.0f }, { 200.0f, 270.0f }, { 270.0f, 315.0f }, { 315.0f, 360.0f } };

int ae_revive_segment_span(int index, float *from_degrees, float *to_degrees)
{
	if (index < 0 || index >= AE_REVIVE_SEGMENTS || !from_degrees || !to_degrees)
		return 0;
	*from_degrees = spans[index][0] + AE_REVIVE_SEGMENT_TRIM;
	*to_degrees = spans[index][1] - AE_REVIVE_SEGMENT_TRIM;
	return 1;
}

int ae_revive_white_segments(int progress)
{
	if (progress <= 0)
		return 0;
	if (progress >= 255)
		return AE_REVIVE_SEGMENTS;
	return progress * AE_REVIVE_SEGMENTS / 255;
}

int ae_revive_bleed_lit(int left_seconds, int total_seconds)
{
	int lit;

	if (left_seconds <= 0 || total_seconds <= 0)
		return 0;
	if (left_seconds >= total_seconds)
		return AE_REVIVE_SEGMENTS;
	lit = (left_seconds * AE_REVIVE_SEGMENTS + total_seconds - 1) / total_seconds;
	return lit < 1 ? 1 : lit > AE_REVIVE_SEGMENTS ? AE_REVIVE_SEGMENTS : lit;
}

int ae_revive_segment_is_lit(int index, int lit)
{
	return index >= 0 && index < AE_REVIVE_SEGMENTS && index >= AE_REVIVE_SEGMENTS - lit;
}

int ae_revive_metres(float world_units)
{
	float metres = world_units * AE_REVIVE_METRES_PER_UNIT;

	if (!(metres > 0.0f))
		return 0;
	if (metres > 9999.0f)
		return 9999;
	return (int)(metres + 0.5f);
}

int ae_revive_in_reach(float dx, float dy, float dz)
{
	return dx * dx + dy * dy <= AE_ROUND_REVIVE_REACH * AE_ROUND_REVIVE_REACH && fabsf(dz) <= AE_ROUND_REVIVE_REACH_HEIGHT;
}

/* (wide text from digits by hand: no swprintf, so a test and the game agree on every platform's wchar_t) */
static int put_text(wchar_t *out, int size, int at, char const *text)
{
	while (*text && at < size - 1)
		out[at++] = (wchar_t)(unsigned char)*text++;
	out[at] = 0;
	return at;
}

static int put_number(wchar_t *out, int size, int at, int value)
{
	char digits[12];
	int count = 0;

	if (value < 0)
		value = 0;
	do
		digits[count++] = (char)('0' + value % 10), value /= 10;
	while (value && count < 11);
	while (count && at < size - 1)
		out[at++] = (wchar_t)digits[--count];
	out[at] = 0;
	return at;
}

void ae_revive_bleed_text(int seconds, wchar_t *out, int size)
{
	int at;

	if (!out || size < 1)
		return;
	if (seconds < 0)
		seconds = 0;
	at = put_text(out, size, 0, "BLEED-OUT ");
	at = put_number(out, size, at, seconds / 60);
	at = put_text(out, size, at, ":");
	if (seconds % 60 < 10)
		at = put_text(out, size, at, "0");
	put_number(out, size, at, seconds % 60);
}

void ae_revive_nearest_text(int metres, wchar_t *out, int size)
{
	int at;

	if (!out || size < 1)
		return;
	at = put_text(out, size, 0, "NEAREST TEAMMATE ");
	at = put_number(out, size, at, metres);
	put_text(out, size, at, "m");
}

void ae_revive_distance_text(int metres, wchar_t *out, int size)
{
	int at;

	if (!out || size < 1)
		return;
	at = put_number(out, size, 0, metres);
	put_text(out, size, at, "m");
}

int ae_revive_self_mode(int reviver)
{
	return reviver >= 0 && reviver < 16 ? AE_REVIVE_SELF_REVIVING : AE_REVIVE_SELF_WAITING;
}
