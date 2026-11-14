#include <string.h>
#include "ae_server_sec_rules.h"

int ae_sec_login_check(
	struct ae_sec_login_budget *address,
	struct ae_sec_login_budget *global,
	unsigned long minute,
	int local)
{
	if (address->minute != minute)
	{
		address->minute = minute;
		address->used = 0;
	}
	if (address->used >= (local ? AE_SEC_LOGIN_PER_ADDRESS_LOCAL : AE_SEC_LOGIN_PER_ADDRESS))
		return 1;
	address->used++;
	if (local)
		return 0;
	if (global->minute != minute)
	{
		global->minute = minute;
		global->used = 0;
	}
	if (++global->used > AE_SEC_LOGIN_GLOBAL)
		return 2;
	return 0;
}

static int ascii_lower(int c)
{
	return c >= 'A' && c <= 'Z' ? c + 32 : c;
}

/* a dotted quad of decimals */
static int is_ipv4(char const *text, int length)
{
	int dots = 0, digits = 0, index;

	for (index = 0; index < length; index++)
	{
		if (text[index] == '.')
		{
			if (!digits)
				return 0;
			dots++;
			digits = 0;
		}
		else if (text[index] >= '0' && text[index] <= '9')
		{
			if (++digits > 3)
				return 0;
		}
		else
		{
			return 0;
		}
	}
	return dots == 3 && digits;
}

int ae_sec_host_allowed(
	char const *host,
	char const *names)
{
	char name[256];
	int length;
	char const *cursor;

	if (!host || !host[0])
		return 1;
	length = (int)strlen(host);
	if (length >= (int)sizeof(name))
		return 0;
	if (host[0] == '[')
		return strchr(host, ']') != NULL;
	memcpy(name, host, (size_t)length + 1);
	/* (a port: after the last colon of a name that has one colon) */
	{
		char *colon = strchr(name, ':');

		if (colon)
		{
			if (strchr(colon + 1, ':'))
				return 1; /* (an IPv6 address without brackets: no name) */
			*colon = 0;
			length = (int)(colon - name);
		}
	}
	if (!length)
		return 0;
	if (is_ipv4(name, length))
		return 1;
	{
		int index;

		for (index = 0; index < length; index++)
			name[index] = (char)ascii_lower(name[index]);
	}
	if (!strcmp(name, "localhost"))
		return 1;
	cursor = names ? names : "";
	while (*cursor)
	{
		char const *end = strchr(cursor, ',');
		int size = end ? (int)(end - cursor) : (int)strlen(cursor);
		int begin = 0, stop = size, index;

		while (begin < stop && (cursor[begin] == ' ' || cursor[begin] == '\t'))
			begin++;
		while (stop > begin && (cursor[stop - 1] == ' ' || cursor[stop - 1] == '\t'))
			stop--;
		if (stop - begin == length)
		{
			for (index = 0; index < length && ascii_lower(cursor[begin + index]) == name[index]; index++)
				;
			if (index == length)
				return 1;
		}
		cursor += size + (end ? 1 : 0);
	}
	return 0;
}

int ae_sec_ws_admit(
	int total,
	int signed_in,
	int signed_in_maximum,
	int unauthenticated_from_address,
	int new_is_signed_in)
{
	if (total >= AE_SEC_WS_TOTAL)
		return 2;
	if (new_is_signed_in)
		return signed_in >= signed_in_maximum ? 3 : 0;
	return unauthenticated_from_address >= AE_SEC_WS_UNAUTHENTICATED_PER_ADDRESS ? 1 : 0;
}

int ae_sec_command_word(
	char const *word,
	char *out,
	int size)
{
	int length = (int)strlen(word), index, quote;

	for (index = 0; index < length; index++)
	{
		if (word[index] == '"' || (unsigned char)word[index] < 32 || word[index] == 127)
			return -1;
	}
	quote = !length || strchr(word, ' ') != NULL;
	if (length + 1 + (quote ? 2 : 0) >= size)
		return -1;
	out[0] = ' ';
	if (quote)
		out[1] = '"';
	memcpy(out + 1 + quote, word, (size_t)length);
	if (quote)
		out[1 + 1 + length] = '"';
	length = 1 + quote + length + quote;
	out[length] = 0;
	return length;
}

int ae_sec_bind_allowed(
	int loopback,
	int has_trusted_proxy,
	int insecure_http)
{
	return loopback || has_trusted_proxy || insecure_http;
}

int ae_sec_password_minimum(
	int loopback,
	int has_trusted_proxy)
{
	return loopback || has_trusted_proxy ? 8 : 12;
}
