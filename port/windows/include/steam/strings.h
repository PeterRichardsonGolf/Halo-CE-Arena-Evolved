/*
STRINGS.H

The POSIX names of <strings.h> the Steam client (port/android/host/steam_*.c) uses, on the Windows C runtime.
*/

#ifndef __HALO_WINDOWS_STEAM_STRINGS_H
#define __HALO_WINDOWS_STEAM_STRINGS_H

#include <string.h>

#define strcasecmp _stricmp
#define strncasecmp _strnicmp

#endif
