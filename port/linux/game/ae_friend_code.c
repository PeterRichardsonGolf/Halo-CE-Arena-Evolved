/*
AE_FRIEND_CODE.C

A friend code from a player key, and typed codes read back (ae_friend_code.h). Pure.
*/

#include <string.h>

#include "ae_friend_code.h"
#include "mcc_sha256.h"

static const char alphabet[] = "0123456789ABCDEFGHJKMNPQRSTVWXYZ";

/* the value of a typed character, -1 for none (I, L read as 1; O as 0; U and the symbols are not code characters) */
static int digit_value(char c)
{
	const char *at;

	if (c >= 'a' && c <= 'z')
		c = (char)(c - 'a' + 'A');
	if (c == 'I' || c == 'L')
		c = '1';
	else if (c == 'O')
		c = '0';
	at = c ? strchr(alphabet, c) : NULL;
	return at ? (int)(at - alphabet) : -1;
}

int ae_friend_code_from_key(const unsigned char pub[32], char out[AE_FRIEND_CODE_SIZE])
{
	struct mcc_sha256 context;
	unsigned char digest[32];
	unsigned long long bits = 0;
	int index, at = 0;

	if (!pub || !out)
		return 0;
	mcc_sha256_begin(&context);
	mcc_sha256_add(&context, pub, 32);
	mcc_sha256_end(&context, digest);
	for (index = 0; index < 8; index++)
		bits = bits << 8 | digest[index];
	bits >>= 4;
	for (index = 0; index < AE_FRIEND_CODE_DIGITS; index++)
	{
		if (index && index % 4 == 0)
			out[at++] = '-';
		out[at++] = alphabet[(bits >> (55 - 5 * index)) & 31];
	}
	out[at] = 0;
	return 1;
}

int ae_friend_code_count(const char *text)
{
	int count = 0;

	for (; text && *text; text++)
		if (digit_value(*text) >= 0)
			count++;
	return count;
}

int ae_friend_code_parse(const char *text, char out[AE_FRIEND_CODE_SIZE])
{
	int digits[AE_FRIEND_CODE_DIGITS], count = 0, index, at = 0;

	if (out)
		out[0] = 0;
	if (!text || !out)
		return 0;
	for (; *text; text++)
	{
		int value;

		if (*text == '-' || *text == ' ' || *text == '\t' || *text == '\r' || *text == '\n')
			continue;
		value = digit_value(*text);
		if (value < 0 || count == AE_FRIEND_CODE_DIGITS)
			return 0;
		digits[count++] = value;
	}
	if (count != AE_FRIEND_CODE_DIGITS)
		return 0;
	for (index = 0; index < AE_FRIEND_CODE_DIGITS; index++)
	{
		if (index && index % 4 == 0)
			out[at++] = '-';
		out[at++] = alphabet[digits[index]];
	}
	out[at] = 0;
	return 1;
}
