/*
AE_CROUCH_AIR_RULES.C

See ae_crouch_air_rules.h.
*/

#include <ctype.h>
#include <string.h>

#include "ae_crouch_air_rules.h"

static int ae_starts_with_word(const char *text, const char *word)
{
	size_t length = strlen(word);
	size_t index;

	for (index = 0; index < length; index++)
	{
		if (tolower((unsigned char)text[index]) != tolower((unsigned char)word[index]))
			return 0;
	}
	return text[length] == ' ';
}

int ae_crouch_air_seat(const char *seat_label)
{
	return seat_label && !strcmp(seat_label, "crouch");
}

int ae_crouch_air_name_is_own(const char *class_label, const char *animation_name)
{
	if (!class_label || !animation_name || !class_label[0])
		return 0;
	if (!ae_starts_with_word(animation_name, "crouch"))
		return 0;
	return ae_starts_with_word(animation_name + strlen("crouch "), class_label);
}
