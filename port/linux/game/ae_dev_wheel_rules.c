/*
AE_DEV_WHEEL_RULES.C

The DEV MODE weapon wheel's rules (ae_dev_wheel_rules.h).
*/

#include <math.h>
#include <string.h>

#include "ae_dev_wheel_rules.h"
#include "ae_weapon_slots.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static int has(char const *text, char const *word)
{
	return strstr(text, word) != NULL;
}

int ae_dev_wheel_category(char const *tag_name)
{
	char lower[160];
	size_t index;
	static char const *const covenant[] = { "plasma", "needler", "carbine", "covenant", "gravity", "fuel rod", "beam" };
	static char const *const human[] = { "pistol", "assault", "ma5", "smg", "shotgun", "sniper", "rocket", "magnum", "flamethrower" };

	for (index = 0; tag_name && tag_name[index] && index < sizeof(lower) - 1; index++)
	{
		char c = tag_name[index];

		lower[index] = (char)(c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : c);
	}
	lower[index] = 0;
	/* (a pack's weapon goes with the other packs, whatever kind it is: the owner looks for them together) */
	if (has(lower, "ae\\packs\\"))
		return AE_DEV_WHEEL_OTHER;
	/* (the covenant words first: a plasma pistol is a pistol too) */
	for (index = 0; index < sizeof(covenant) / sizeof(covenant[0]); index++)
	{
		if (has(lower, covenant[index]))
			return AE_DEV_WHEEL_COVENANT;
	}
	for (index = 0; index < sizeof(human) / sizeof(human[0]); index++)
	{
		if (has(lower, human[index]))
			return AE_DEV_WHEEL_HUMAN;
	}
	return AE_DEV_WHEEL_OTHER;
}

char const *ae_dev_wheel_category_label(int category)
{
	return category == AE_DEV_WHEEL_HUMAN ? "HUMAN" : category == AE_DEV_WHEEL_COVENANT ? "COVENANT" : "AE / OTHER";
}

short ae_dev_wheel_pages(short count)
{
	return (short)(count <= 0 ? 0 : (count + AE_DEV_WHEEL_SLICES - 1) / AE_DEV_WHEEL_SLICES);
}

short ae_dev_wheel_page_size(short count, short page)
{
	short pages = ae_dev_wheel_pages(count), base, extra;

	if (page < 0 || page >= pages)
		return 0;
	base = (short)(count / pages);
	extra = (short)(count % pages);
	return (short)(base + (page < extra ? 1 : 0));
}

short ae_dev_wheel_pick(short n, float x, float y, short previous)
{
	double length = sqrt((double)x * x + (double)y * y), angle, width, half, distance;
	short candidate;

	if (n <= 0 || length < (double)(AE_DEV_WHEEL_DEAD / AE_DEV_WHEEL_OUT))
		return -1;
	if (n == 1)
		return 0;
	angle = atan2((double)x, (double)y);
	if (angle < 0.0)
		angle += 2.0 * M_PI;
	width = 2.0 * M_PI / n;
	half = width / 2.0;
	candidate = (short)((long)((angle + half) / width) % n);
	if (previous >= 0 && previous < n)
	{
		distance = fabs(fmod(angle - width * previous + 3.0 * M_PI, 2.0 * M_PI) - M_PI);
		if (distance <= half + AE_DEV_WHEEL_STICKY * width)
			return previous;
	}
	return candidate;
}

float ae_dev_wheel_margin(float k, short n, short index, float dx, float dy)
{
	double radius = sqrt((double)dx * dx + (double)dy * dy), angle, width, half, distance, edge;
	float result;

	if (n <= 0 || index < 0 || index >= n)
		return -1.0f;
	angle = atan2((double)dx, (double)-dy);
	if (angle < 0.0)
		angle += 2.0 * M_PI;
	width = 2.0 * M_PI / n;
	half = width / 2.0;
	distance = fabs(fmod(angle - width * index + 3.0 * M_PI, 2.0 * M_PI) - M_PI);
	if (n == 1)
		edge = 1.0e9;
	else if (half - distance <= 0.0)
		edge = -1.0;
	else
		edge = radius * sin(half - distance < M_PI / 2 ? half - distance : M_PI / 2) - (double)AE_DEV_WHEEL_GAP * k / 2.0;
	result = (float)edge;
	if ((float)(radius - AE_DEV_WHEEL_IN * k) < result)
		result = (float)(radius - AE_DEV_WHEEL_IN * k);
	if ((float)(AE_DEV_WHEEL_OUT * k - radius) < result)
		result = (float)(AE_DEV_WHEEL_OUT * k - radius);
	return result;
}

float ae_dev_wheel_scale(float height)
{
	float k = height / 900.0f;

	return k > 1.0f ? 1.0f : k < 0.5f ? 0.5f : k;
}

int ae_dev_wheel_pack_name(char const *tag_name, char *name, int size)
{
	static char const prefix[] = "ae\\packs\\";
	char id[64];
	char const *end, *word;
	int index, length;
	short slot;

	if (name && size > 0)
		name[0] = 0;
	if (!tag_name || !name || size <= 0)
		return 0;
	for (index = 0; prefix[index]; index++)
	{
		char c = tag_name[index];

		if ((c >= 'A' && c <= 'Z' ? (char)(c - 'A' + 'a') : c) != prefix[index])
			return 0;
	}
	end = strchr(tag_name + index, '\\');
	length = end ? (int)(end - (tag_name + index)) : 0;
	if (length <= 0 || length >= (int)sizeof(id))
		return 0;
	memcpy(id, tag_name + index, (size_t)length);
	id[length] = 0;
	slot = ae_weapon_slots_of_pack(id);
	word = slot != AE_WEAPON_SLOT_NONE ? ae_weapon_slots_display(slot) : id;
	for (length = 0; word[length] && length < size - 1; length++)
	{
		char c = word[length] == '_' ? ' ' : word[length];

		name[length] = c >= 'a' && c <= 'z' ? (char)(c - 'a' + 'A') : c;
	}
	name[length] = 0;
	return 1;
}
