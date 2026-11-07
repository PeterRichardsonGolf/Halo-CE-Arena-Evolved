#include <string.h>
#include "ae_h2_presets_rules.h"

#include "ae_h2_presets_data.inc"

int ae_h2_preset_count(void)
{
	return (int)(sizeof(ae_h2_presets) / sizeof(ae_h2_presets[0]));
}

struct ae_h2_preset const *ae_h2_preset_get(int index)
{
	return index >= 0 && index < ae_h2_preset_count() ? &ae_h2_presets[index] : 0;
}

static int upper(int c)
{
	return c >= 'a' && c <= 'z' ? c - 'a' + 'A' : c;
}

static int find_exact(char const *stored)
{
	int index, at;

	for (index = 0; index < ae_h2_preset_count(); index++)
	{
		for (at = 0; at < 12; at++)
		{
			int a = at < 11 ? upper((unsigned char)stored[at]) : 0;
			int b = (unsigned char)ae_h2_presets[index].stored[at];

			if (a != b)
				break;
			if (!a)
				return index;
		}
	}
	return -1;
}

int ae_h2_preset_find(char const *stored)
{
	int index, at;
	char own[12];

	if (!stored)
		return -1;
	index = find_exact(stored);
	if (index >= 0)
		return index;
	/* a start of a preset (BR / EV / PS + the preset's name after "AE ") plays the preset's block */
	if (stored[2] == ' ' && ((upper((unsigned char)stored[0]) == 'B' && upper((unsigned char)stored[1]) == 'R') ||
		(upper((unsigned char)stored[0]) == 'E' && upper((unsigned char)stored[1]) == 'V') ||
		(upper((unsigned char)stored[0]) == 'P' && upper((unsigned char)stored[1]) == 'S')))
	{
		own[0] = 'A'; own[1] = 'E'; own[2] = ' ';
		for (at = 3; at < 11 && stored[at]; at++)
			own[at] = stored[at];
		own[at] = 0;
		return find_exact(own);
	}
	return -1;
}

int ae_h2_preset_find_wide(wchar_t const *stored)
{
	char narrow[12];
	int at;

	if (!stored)
		return -1;
	for (at = 0; at < 11 && stored[at]; at++)
	{
		if (stored[at] > 126)
			return -1;
		narrow[at] = (char)stored[at];
	}
	narrow[at] = 0;
	return ae_h2_preset_find(narrow);
}

void ae_h2_preset_ext(int index, struct ae_variant_ext *ext)
{
	ae_variant_ext_default(ext);
	if (index >= 0 && index < ae_h2_preset_count())
	{
		memcpy(ext, ae_h2_presets[index].ext, sizeof(*ext));
		ae_variant_ext_sanitize(ext);
	}
}
