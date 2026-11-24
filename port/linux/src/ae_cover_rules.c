/*
AE_COVER_RULES.C

See ae_cover_rules.h.
*/

#include "ae_cover_rules.h"

void ae_cover_uv(int picture_w, int picture_h, float box_w, float box_h, float uv[4])
{
	float picture_ratio, box_ratio, kept;

	uv[0] = 0.0f;
	uv[1] = 0.0f;
	uv[2] = 1.0f;
	uv[3] = 1.0f;
	if (picture_w <= 0 || picture_h <= 0 || !(box_w > 0.0f) || !(box_h > 0.0f))
		return;
	picture_ratio = (float)picture_w / (float)picture_h;
	box_ratio = box_w / box_h;
	if (picture_ratio > box_ratio)
	{
		/* (wider than the box: the sides are cut) */
		kept = box_ratio / picture_ratio;
		uv[0] = (1.0f - kept) * 0.5f;
		uv[2] = uv[0] + kept;
	}
	else if (picture_ratio < box_ratio)
	{
		/* (taller than the box: the top and bottom are cut) */
		kept = picture_ratio / box_ratio;
		uv[1] = (1.0f - kept) * 0.5f;
		uv[3] = uv[1] + kept;
	}
}

void ae_cover_fit_rect(int picture_w, int picture_h, float box_w, float box_h, float rect[4])
{
	float picture_ratio, box_ratio;

	rect[0] = 0.0f;
	rect[1] = 0.0f;
	rect[2] = box_w > 0.0f ? box_w : 0.0f;
	rect[3] = box_h > 0.0f ? box_h : 0.0f;
	if (picture_w <= 0 || picture_h <= 0 || !(box_w > 0.0f) || !(box_h > 0.0f))
		return;
	picture_ratio = (float)picture_w / (float)picture_h;
	box_ratio = box_w / box_h;
	if (picture_ratio > box_ratio)
	{
		/* (wider than the box: full width, room above and below) */
		rect[3] = box_w / picture_ratio;
		rect[1] = (box_h - rect[3]) * 0.5f;
	}
	else if (picture_ratio < box_ratio)
	{
		/* (taller than the box: full height, room at the sides) */
		rect[2] = box_h * picture_ratio;
		rect[0] = (box_w - rect[2]) * 0.5f;
	}
}

int ae_cover_matches(int picture_w, int picture_h, int box_w, int box_h)
{
	long long across, down, difference;

	if (picture_w <= 0 || picture_h <= 0 || box_w <= 0 || box_h <= 0)
		return 1;
	across = (long long)picture_w * box_h;
	down = (long long)picture_h * box_w;
	difference = across > down ? across - down : down - across;
	return difference * 100 <= down;
}

float ae_cover_frame_height(float frame_w)
{
	return frame_w > 0.0f ? frame_w * 9.0f / 16.0f : 0.0f;
}

int ae_cover_fills(int picture_w, int picture_h)
{
	if (picture_w <= 0 || picture_h <= 0)
		return 1;
	/* (1.2 <= w / h <= 2.0, in whole numbers) */
	return (long long)picture_w * 10 >= (long long)picture_h * 12 && (long long)picture_w <= (long long)picture_h * 2;
}

void ae_cover_frame(float box_w, float box_h, float rect[4])
{
	rect[0] = rect[1] = 0.0f;
	rect[2] = box_w > 0.0f ? box_w : 0.0f;
	rect[3] = box_h > 0.0f ? box_h : 0.0f;
	if (!(box_w > 0.0f) || !(box_h > 0.0f))
		return;
	if (box_w * 9.0f > box_h * 16.0f)
	{
		rect[2] = box_h * 16.0f / 9.0f;
		rect[0] = (box_w - rect[2]) * 0.5f;
	}
	else
	{
		rect[3] = box_w * 9.0f / 16.0f;
		rect[1] = (box_h - rect[3]) * 0.5f;
	}
}
