/*
AE_STEAM_HOST.C

What the Steam client (port/android/host/steam_*.c, the Android app's host library, built into the desktop game
for the Steam sign-in) asks of its host: the log, which goes to the game's own (debug.txt).
*/

#if defined(HALO_STEAM_CLIENT)

#include "host.h"
#include "ae_steam_policy.h"

#include <stdarg.h>
#include <stdio.h>

void platform_log(const char *format, ...);

void host_logf(int priority, const char *format, ...)
{
	char text[1024];
	va_list arguments;

	(void)priority;
	va_start(arguments, format);
	vsnprintf(text, sizeof(text), format, arguments);
	va_end(arguments);
	/* (the client logs Workshop titles and Steam's own words: no line break or control character of theirs may
	start a line of its own in the log) */
	steam_log_clean(text);
	platform_log("%s", text);
}

#endif
