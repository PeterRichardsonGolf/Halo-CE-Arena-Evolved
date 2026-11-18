/*
AE_MAP_WARM_UP.C

The PC map list's WARM-UP group (ae_map_warm_up.h). No C library wide string
calls (the game's wchar_t is 16-bit).
*/

#include <stdlib.h>
#include <string.h>

#include "ae_map_warm_up.h"

enum { WARM_UP_TEXT = 64 };

static const char *const warm_up_maps[] = { "octagon", "octagon_b" };

/* (the first length characters, any case: the C libraries' names for it differ) */
static int same_name(const char *a, const char *b, size_t length)
{
	size_t index;

	for (index = 0; index < length; index++)
	{
		char x = a[index] >= 'A' && a[index] <= 'Z' ? (char)(a[index] - 'A' + 'a') : a[index];
		char y = b[index] >= 'A' && b[index] <= 'Z' ? (char)(b[index] - 'A' + 'a') : b[index];

		if (x != y)
			return 0;
	}
	return 1;
}

int ae_map_warm_up(const char *map_name)
{
	const char *base = map_name;
	const char *each;
	size_t length, index;

	if (!map_name)
		return 0;
	for (each = map_name; *each; each++)
	{
		if (*each == '\\' || *each == '/')
			base = each + 1;
	}
	length = strcspn(base, ".@");
	for (index = 0; index < sizeof(warm_up_maps) / sizeof(warm_up_maps[0]); index++)
	{
		if (strlen(warm_up_maps[index]) == length && same_name(base, warm_up_maps[index], length))
			return 1;
	}
	return 0;
}

static void wide_set(wchar_t *out, long count, wchar_t const *text, const char *ascii)
{
	long length = 0;

	while (text && *text && length < count - 1)
		out[length++] = *text++;
	while (ascii && *ascii && length < count - 1)
		out[length++] = (wchar_t)(unsigned char)*ascii++;
	out[length] = 0;
}

wchar_t const *ae_map_warm_up_name(const char *map_name, wchar_t const *name, int lobby)
{
	static wchar_t texts[2][WARM_UP_TEXT];
	wchar_t *text = texts[lobby ? 1 : 0];

	if (!ae_map_warm_up(map_name))
		return name;
	wide_set(text, WARM_UP_TEXT, name, lobby ? " [WARM-UP]" : "\r\n[WARM-UP]");
	return text;
}

wchar_t const *ae_map_warm_up_description(const char *map_name, wchar_t const *description)
{
	static wchar_t text[WARM_UP_TEXT];

	if (!ae_map_warm_up(map_name))
		return description;
	wide_set(text, WARM_UP_TEXT, NULL, "A warm-up map\r\nfor Slayer");
	return text;
}

void ae_map_warm_up_first(void *first, long count, size_t size, size_t name_offset)
{
	unsigned char *rows = first;
	unsigned char *held;
	long index, front = 0;

	if (!rows || count <= 1 || !size || !(held = malloc(size)))
		return;
	for (index = 0; index < count; index++)
	{
		if (!ae_map_warm_up((const char *)(rows + index * size + name_offset)))
			continue;
		/* (moved up past the others before it, which keep their order) */
		if (index != front)
		{
			memcpy(held, rows + index * size, size);
			memmove(rows + (front + 1) * size, rows + front * size, (size_t)(index - front) * size);
			memcpy(rows + front * size, held, size);
		}
		front++;
	}
	free(held);
}
