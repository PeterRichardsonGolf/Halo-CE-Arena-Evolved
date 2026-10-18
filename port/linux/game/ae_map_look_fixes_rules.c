/*
AE_MAP_LOOK_FIXES_RULES.C

AE's look fixes' table and rules (ae_map_look_fixes_rules.h).
*/

#include "ae_map_look_fixes_rules.h"

#include <ctype.h>
#include <string.h>

/* ---------- constants */

#define AE_GROUP(a, b, c, d) \
	(((unsigned long)(a) << 24) | ((unsigned long)(b) << 16) | ((unsigned long)(c) << 8) | (unsigned long)(d))

enum
{
	/* shader_transparent_chicago: framebuffer_blend_function, a short after the shader's 0x28 bytes, the counter
	limit, the flags and first_map_type (rasterizer_xbox_transparent_geometry.c's
	shader_transparent_chicago_definition) */
	CHICAGO_FRAMEBUFFER_BLEND_FUNCTION_OFFSET = 0x2C,
	FRAMEBUFFER_BLEND_ALPHA_BLEND = 0,
	FRAMEBUFFER_BLEND_ADD = 3
};

/* ---------- globals */

/* h2d_coagulation (H2 Demakes, an Xbox map): the teleporter's energy sheet is an alpha-blended chicago shader
whose four maps (the field generator's dust, the sheet's streak) are all opaque DXT1, so its alpha is 1
everywhere and the sheet draws as an opaque black card behind the blue streak (on the Xbox too: the stock
engine's chicago path, which this is). The streak's bitmap asks for DXT3 and holds DXT1: its alpha was lost when
the map was built. Added, black adds nothing and the streak glows on its own, as the sheet was meant to look.
h2d_headlong's and h2d_warlock's teleporters have the same sheet (the same four maps, their "power" streak
opaque DXT1 too; warlock's alpha functions add and multiply, still 1 everywhere), seen with the same card in box
games. (h2d_elongation's alike sheet is on a vehicle the map never places: not fixed.) */
struct ae_map_look_fix const ae_map_look_fixes[] =
{
	{ "h2d_coagulation", AE_GROUP('s', 'c', 'h', 'i'), "scenery\\protected 24\\shaders\\base part1",
		CHICAGO_FRAMEBUFFER_BLEND_FUNCTION_OFFSET, FRAMEBUFFER_BLEND_ALPHA_BLEND, FRAMEBUFFER_BLEND_ADD,
		"the teleporter's sheet added, not alpha-blended (its maps have no alpha: a black card)" },
	{ "h2d_headlong", AE_GROUP('s', 'c', 'h', 'i'), "scenery\\h2_teleporter\\shaders\\generator shield",
		CHICAGO_FRAMEBUFFER_BLEND_FUNCTION_OFFSET, FRAMEBUFFER_BLEND_ALPHA_BLEND, FRAMEBUFFER_BLEND_ADD,
		"the teleporters' sheet added, not alpha-blended (its maps have no alpha: a black card)" },
	{ "h2d_warlock", AE_GROUP('s', 'c', 'h', 'i'), "scenery\\h2_teleporter\\shaders\\generator shield",
		CHICAGO_FRAMEBUFFER_BLEND_FUNCTION_OFFSET, FRAMEBUFFER_BLEND_ALPHA_BLEND, FRAMEBUFFER_BLEND_ADD,
		"the teleporters' sheet added, not alpha-blended (its maps have no alpha: a black card)" },
};

unsigned int const ae_map_look_fix_count = sizeof(ae_map_look_fixes) / sizeof(ae_map_look_fixes[0]);

/* ---------- public code */

int ae_map_look_fix_for_map(struct ae_map_look_fix const *fix, char const *map_name)
{
	unsigned int index;

	if (!fix || !fix->map_name || !map_name)
		return 0;
	for (index = 0; index < AE_MAP_LOOK_FIX_MAP_NAME_SIZE; index++)
	{
		unsigned char want = (unsigned char)fix->map_name[index];
		unsigned char have = (unsigned char)map_name[index];

		if (tolower(want) != tolower(have))
			return 0;
		if (!want)
			return 1;
	}
	/* (a name of the header's whole 32 bytes: the fix's must end there too) */
	return fix->map_name[AE_MAP_LOOK_FIX_MAP_NAME_SIZE] == '\0';
}

enum ae_map_look_fix_result ae_map_look_fix_apply(struct ae_map_look_fix const *fix, unsigned char *data,
	unsigned int size)
{
	short value;

	if (!fix || !data || size < sizeof(value) || fix->offset > size - sizeof(value))
		return AE_MAP_LOOK_FIX_OUTSIDE;
	memcpy(&value, data + fix->offset, sizeof(value));
	if (value == fix->new_value)
		return AE_MAP_LOOK_FIX_ALREADY;
	if (value != fix->old_value)
		return AE_MAP_LOOK_FIX_UNEXPECTED;
	value = fix->new_value;
	memcpy(data + fix->offset, &value, sizeof(value));
	return AE_MAP_LOOK_FIX_APPLIED;
}

void ae_bitmap_type_errors_reset(struct ae_bitmap_type_errors *errors)
{
	if (errors)
		errors->count = 0;
}

int ae_bitmap_type_errors_first(struct ae_bitmap_type_errors *errors, uintptr_t bitmap, short type)
{
	unsigned int index;

	if (!errors)
		return 0;
	for (index = 0; index < errors->count && index < AE_BITMAP_TYPE_ERRORS_KEPT; index++)
	{
		if (errors->bitmaps[index] == bitmap && errors->types[index] == type)
			return 0;
	}
	if (errors->count >= AE_BITMAP_TYPE_ERRORS_KEPT)
		return 0;
	errors->bitmaps[errors->count] = bitmap;
	errors->types[errors->count] = type;
	errors->count++;
	return 1;
}
