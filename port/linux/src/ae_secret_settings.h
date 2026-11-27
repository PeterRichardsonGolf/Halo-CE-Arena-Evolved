/*
AE_SECRET_SETTINGS.H

Which settings hold secrets, one rule for every place that shows or logs a setting's value: the server's console log
hides a command line that sets one (server_console.c's server_console_line_is_secret), and the settings' answers show
only "(set)" or "(none)" for one (server_commands.c's server_commands_setting_secret). A secret: a name holding
"password" (the server's and the web roles' passwords), "publisher_key" (the publisher key and its file),
"webhook" (the alert webhook's URL carries its token) or "token" (the event feed's bearer token,
network.events_token), or a name ending in ".key" (community.key, the install's own key on the community server).
Names compare without case. DEV MODE's config command (ae_dev_cmd.c) logs a secret's line without its value.
*/

#ifndef __AE_SECRET_SETTINGS_H
#define __AE_SECRET_SETTINGS_H

#include <ctype.h>
#include <string.h>

/* whether name (length characters, not NUL-ended) holds part */
static inline int ae_secret_name_holds(const char *name, int length, const char *part)
{
	int size = (int)strlen(part), start, index;

	for (start = 0; start + size <= length; start++)
	{
		for (index = 0; index < size && tolower((unsigned char)name[start + index]) == part[index]; index++)
		{
		}
		if (index == size)
			return 1;
	}
	return 0;
}

/* whether name (length characters, not NUL-ended) ends in part */
static inline int ae_secret_name_ends(const char *name, int length, const char *part)
{
	int size = (int)strlen(part);

	return length >= size && ae_secret_name_holds(name + length - size, size, part);
}

/* whether the setting named name (length characters) holds a secret */
static inline int ae_setting_is_secret(const char *name, int length)
{
	return ae_secret_name_holds(name, length, "password") || ae_secret_name_holds(name, length, "publisher_key") ||
		ae_secret_name_holds(name, length, "webhook") || ae_secret_name_holds(name, length, "token") ||
		ae_secret_name_ends(name, length, ".key");
}

#endif
