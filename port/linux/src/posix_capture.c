/*
POSIX_CAPTURE.C

The capture's leftovers cleared (capture_child.h, capture_folder_clean), on
Linux and macOS, with the C library's own ABI: struct stat is laid out
otherwise under the platform layer's (posix.h). The capture folder is
opened once (O_NOFOLLOW: a link is refused), and every file is looked at
(fstatat) and removed (unlinkat) relative to it, never following a link.
Windows' is in port/windows/src/win32_capture.c.
*/

#include "capture_child.h"

#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

int capture_folder_clean(const char *folder, int stale_seconds, int (*owned)(const char *name))
{
	int descriptor = open(folder, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
	DIR *stream;
	struct dirent *entry;
	time_t now = time(NULL);
	int removed = 0;

	if (descriptor < 0)
		return -1;
	stream = fdopendir(descriptor);
	if (!stream)
	{
		close(descriptor);
		return -1;
	}
	while ((entry = readdir(stream)) != NULL)
	{
		struct stat information;
		int kind = owned(entry->d_name);

		if (!kind)
			continue;
		if (fstatat(dirfd(stream), entry->d_name, &information, AT_SYMLINK_NOFOLLOW) != 0 ||
			!S_ISREG(information.st_mode))
			continue;
		if (kind == 2 && information.st_size != 0)
			continue;
		if (now - information.st_mtime < (time_t)stale_seconds)
			continue;
		if (unlinkat(dirfd(stream), entry->d_name, 0) == 0)
			removed++;
	}
	closedir(stream);
	return removed;
}
