/*
CAPTURE_NAME.H

The check of a capture's explicit name (capture_request_screenshot_as and capture_request_recording_as, capture.h),
apart from capture.c so that a test builds it alone: a path under config.toml's folder, so never absolute, never with a
".." in it.
*/

#ifndef __HALO_LINUX_CAPTURE_NAME_H
#define __HALO_LINUX_CAPTURE_NAME_H

#include <string.h>

#define CAPTURE_NAME_RELATIVE_MAXIMUM 400

/* 1 if relative is "folder/name<extension>" made of letters, digits, "-", "_", "." and "/" only: not empty, no empty
or "." or ".." part, not starting with "/", ending in extension with a name before it; else 0 */
static int capture_name_valid(const char *relative, const char *extension)
{
	size_t length, extension_length = strlen(extension), index, part = 0;

	if (!relative || !*relative)
		return 0;
	length = strlen(relative);
	if (length >= CAPTURE_NAME_RELATIVE_MAXIMUM || length <= extension_length ||
		strcmp(relative + length - extension_length, extension))
		return 0;
	for (index = 0; index <= length; index++)
	{
		char c = relative[index];

		if (c == '/' || c == 0)
		{
			/* (a part of no characters, ".", or "..") */
			if (index == part || (index - part == 1 && relative[part] == '.') ||
				(index - part == 2 && relative[part] == '.' && relative[part + 1] == '.'))
				return 0;
			part = index + 1;
		}
		else if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' ||
			c == '_' || c == '.'))
		{
			return 0;
		}
	}
	/* (the name before the extension is not itself empty: "dir/.png") */
	return relative[length - extension_length - 1] != '/';
}

#endif
