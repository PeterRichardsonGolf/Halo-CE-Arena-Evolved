/*
AE_MCC_FILE_STAMP_TEST.C

A file's stamp (port/linux/src/ae_mcc_file_stamp.c) on real files: the same stamp twice for a file left alone; another
after its bytes are written again at the same size with its write time put back (the change time moves); another for
a second file renamed over it; none for a folder or a missing file. POSIX only (the test's own files).
*/

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "../src/ae_mcc_file_stamp.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

static void write_file(char const *path, char fill)
{
	char bytes[4096];
	FILE *file = fopen(path, "wb");

	memset(bytes, fill, sizeof(bytes));
	if (file)
	{
		fwrite(bytes, 1, sizeof(bytes), file);
		fclose(file);
	}
}

static void pause_a_little(void)
{
	struct timespec wait = {0, 20 * 1000 * 1000};

	nanosleep(&wait, NULL);
}

int main(void)
{
	char folder[] = "/tmp/ae_mcc_file_stamp_XXXXXX";
	char path[256], other[256];
	struct ae_mcc_file_stamp first, again, after;
	struct stat information;
	struct timespec times[2];

	if (!mkdtemp(folder))
		return 1;
	snprintf(path, sizeof(path), "%s/bitmaps.map", folder);
	snprintf(other, sizeof(other), "%s/other.map", folder);
	write_file(path, 'a');
	CHECK(ae_mcc_file_stamp(path, &first));
	CHECK(first.size_low == 4096 && first.size_high == 0);
	CHECK(ae_mcc_file_stamp(path, &again) && !memcmp(&first, &again, sizeof(first)));
	/* (the same size written again, the write time put back) */
	CHECK(!stat(path, &information));
	pause_a_little();
	write_file(path, 'b');
	times[0] = information.st_atim;
	times[1] = information.st_mtim;
	CHECK(!utimensat(AT_FDCWD, path, times, 0));
	CHECK(ae_mcc_file_stamp(path, &after));
	CHECK(after.written_seconds == first.written_seconds && after.written_fraction == first.written_fraction);
	CHECK(memcmp(&first, &after, sizeof(first)));
	/* (another file of the same size renamed over it) */
	CHECK(ae_mcc_file_stamp(path, &first));
	pause_a_little();
	write_file(other, 'b');
	CHECK(!rename(other, path));
	CHECK(ae_mcc_file_stamp(path, &after) && memcmp(&first, &after, sizeof(first)));
	/* (no stamp: a folder, a missing file, no path) */
	CHECK(!ae_mcc_file_stamp(folder, &after));
	CHECK(!ae_mcc_file_stamp(other, &after));
	CHECK(!ae_mcc_file_stamp("", &after));
	unlink(path);
	rmdir(folder);
	if (failures)
		return 1;
	printf("ae_mcc_file_stamp: all checks passed\n");
	return 0;
}
