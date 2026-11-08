/*
AE_JOINER_NOTICE_RULES.C

The joiner notice's words (ae_joiner_notice_rules.h).
*/

#include <stdio.h>
#include <string.h>

#include "ae_joiner_notice_rules.h"

int ae_joiner_notice_text(char *text, int size, char const *name, int kind, char const *build)
{
	char what[48];
	int length;

	if (!text || size <= 0)
		return 0;
	text[0] = 0;
	if (kind == 0)
		snprintf(what, sizeof(what), "OpenCE");
	else if (kind > 0 && build && build[0])
		snprintf(what, sizeof(what), "%s", build);
	else
		snprintf(what, sizeof(what), "another build");
	/* (a name of 16 characters, 48 bytes at most in UTF-8, and a 31-byte build name keep it under chat's 200) */
	length = snprintf(text, (size_t)size, "%.48s plays on %.31s: some Arena Evolved extras won't show for them. "
		"Get AE: " AE_JOINER_NOTICE_LINK, name && name[0] ? name : "A player", what);
	if (length <= 0 || length >= size)
	{
		text[0] = 0;
		return 0;
	}
	return length;
}

int ae_joiner_notice_version_text(char *text, int size, char const *name, char const *joiner, char const *host)
{
	int length;

	if (!text || size <= 0)
		return 0;
	text[0] = 0;
	if (!joiner || !joiner[0] || !host || !host[0] || !strcmp(joiner, host))
		return 0;
	length = snprintf(text, (size_t)size, "%.48s plays on %.31s and this host on %.31s: get the same version for the "
		"same game.", name && name[0] ? name : "A player", joiner, host);
	if (length <= 0 || length >= size)
	{
		text[0] = 0;
		return 0;
	}
	return length;
}
