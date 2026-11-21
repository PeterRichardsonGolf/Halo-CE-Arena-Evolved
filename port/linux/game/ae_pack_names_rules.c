/*
AE_PACK_NAMES_RULES.C

The pure rules of a pack weapon's name in place of its picture (ae_pack_names_rules.h).
*/

#include "ae_pack_names_rules.h"

int ae_pack_names_no_picture(int pack_weapon, int hud_is_pack_own, int messaging_sequence)
{
	return pack_weapon && hud_is_pack_own && messaging_sequence < 0;
}

int ae_pack_names_picture(short const *const *types, int const *counts, int elements, int *element_out, int *item_out)
{
	int element, item;

	if (element_out)
		*element_out = -1;
	if (item_out)
		*item_out = -1;
	if (!types || !counts || !element_out || !item_out)
		return 0;
	for (element = 0; element < elements; element++)
	{
		for (item = 0; types[element] && item < counts[element]; item++)
		{
			if (types[element][item] & (1 << AE_PACK_NAMES_OVERLAY_ALWAYS_BIT))
			{
				*element_out = element;
				*item_out = item;
				return 1;
			}
		}
	}
	return 0;
}

int ae_pack_names_word(char const *menu_name, wchar_t *out, int size)
{
	int length = 0;

	if (!out || size < 1)
		return 0;
	if (menu_name)
	{
		for (; menu_name[length] && length < size - 1; length++)
		{
			unsigned char letter = (unsigned char)menu_name[length];

			if (letter >= 'a' && letter <= 'z')
				letter = (unsigned char)(letter - 'a' + 'A');
			out[length] = letter < 0x80 && letter >= 0x20 ? (wchar_t)letter : L'?';
		}
	}
	out[length] = 0;
	return length;
}
