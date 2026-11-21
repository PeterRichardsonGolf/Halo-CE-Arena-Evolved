/*
AE_DECIMAL.H

A decimal number of up to 64 bits read from text (a Steam Workshop item id): the game's files build as C89 on macOS,
where <stdlib.h> declares no strtoull. Digits from the start of the text; 0 when there are none.
*/

#ifndef __AE_DECIMAL_H
#define __AE_DECIMAL_H

static inline unsigned long long ae_decimal_u64(char const *text)
{
	unsigned long long value = 0;

	while (text && *text >= '0' && *text <= '9')
		value = value * 10 + (unsigned long long)(*text++ - '0');
	return value;
}

#endif
