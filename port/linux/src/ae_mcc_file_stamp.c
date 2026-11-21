/*
AE_MCC_FILE_STAMP.C

A file's stamp (ae_mcc_file_stamp.h). On Linux and Android through posix.h's posix_stat, whose times are the file's
last write and its status change (st_mtim, st_ctim) in nanoseconds. On macOS posix_stat gives the birth time, not
the change time, so no stamp is taken there (callers then read the file again, as without a stamp). On Windows from
the open file itself: its size, volume and file index (GetFileInformationByHandle) and its last write and change
times (GetFileInformationByHandleEx's FileBasicInfo), in 100-nanosecond steps.
*/

#include <string.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include "posix.h"
#endif

#include "ae_mcc_file_stamp.h"

int ae_mcc_file_stamp(char const *path, struct ae_mcc_file_stamp *stamp)
{
#ifdef _WIN32
	BY_HANDLE_FILE_INFORMATION information;
	FILE_BASIC_INFO basic;
	HANDLE file;
	int taken;

	memset(stamp, 0, sizeof(*stamp));
	if (!path || !path[0])
		return 0;
	file = CreateFileA(path, FILE_READ_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL,
		OPEN_EXISTING, 0, NULL);
	if (file == INVALID_HANDLE_VALUE)
		return 0;
	taken = GetFileInformationByHandle(file, &information) &&
		!(information.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) &&
		GetFileInformationByHandleEx(file, FileBasicInfo, &basic, sizeof(basic));
	CloseHandle(file);
	if (!taken)
		return 0;
	stamp->size_low = information.nFileSizeLow;
	stamp->size_high = information.nFileSizeHigh;
	stamp->written_seconds = (unsigned int)(basic.LastWriteTime.QuadPart >> 32);
	stamp->written_fraction = (unsigned int)basic.LastWriteTime.QuadPart;
	stamp->changed_seconds = (unsigned int)(basic.ChangeTime.QuadPart >> 32);
	stamp->changed_fraction = (unsigned int)basic.ChangeTime.QuadPart;
	stamp->volume = information.dwVolumeSerialNumber;
	stamp->index_low = information.nFileIndexLow;
	stamp->index_high = information.nFileIndexHigh;
	return 1;
#elif defined(__APPLE__)
	memset(stamp, 0, sizeof(*stamp));
	(void)path;
	return 0;
#else
	struct posix_file_information information;

	memset(stamp, 0, sizeof(*stamp));
	if (!path || !path[0] || posix_stat(path, &information) || (information.flags & _posix_file_is_directory))
		return 0;
	stamp->size_low = (unsigned int)information.size_low;
	stamp->size_high = (unsigned int)information.size_high;
	stamp->written_seconds = (unsigned int)information.modification_seconds;
	stamp->written_fraction = (unsigned int)information.modification_nanoseconds;
	/* (posix_files.c's creation time is st_ctim: the status change) */
	stamp->changed_seconds = (unsigned int)information.creation_seconds;
	stamp->changed_fraction = (unsigned int)information.creation_nanoseconds;
	return 1;
#endif
}
