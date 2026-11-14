/* The career record's store (see the header). */
#include <stdio.h>
#include <string.h>

#include "../src/ae_private_files.h"
#include "ae_glue_career.h"

void platform_log(char const *format, ...);

#define FILE_NAME "career.txt"

static struct
{
	struct ae_career career;
	char text[AE_CAREER_TEXT];
	int read;
} store;

struct ae_career *ae_career_store(void)
{
	long length;
	int result;

	if (store.read)
		return &store.career;
	store.read = 1;
	length = ae_private_file_read(FILE_NAME, store.text, sizeof(store.text));
	if (length < 0 && length != -2)
	{
		ae_career_init(&store.career);
		platform_log("ae career: %s: none yet", FILE_NAME);
		return &store.career;
	}
	result = length == -2 ? AE_CAREER_READ_BAD : ae_career_read(&store.career, store.text);
	if (result != AE_CAREER_READ_OK)
	{
		ae_career_init(&store.career);
		if (ae_private_file_set_aside(FILE_NAME))
			platform_log("ae career: %s refused: kept as %s.bad, a fresh one started", FILE_NAME, FILE_NAME);
		else
			platform_log("ae career: %s refused (not kept aside), a fresh one started", FILE_NAME);
	}
	else
		platform_log("ae career: %s: %d career, %d online entries", FILE_NAME, store.career.count[0], store.career.count[1]);
	return &store.career;
}

int ae_career_store_save(void)
{
	long length;
	struct ae_career *career = ae_career_store();

	if (career->newer)
	{
		platform_log("ae career: the file is of a newer version: not written");
		return 0;
	}
	length = ae_career_write(career, store.text, sizeof(store.text));
	if (length < 0 || !ae_private_file_write(FILE_NAME, store.text, length))
	{
		platform_log("ae career: %s could not be written", FILE_NAME);
		return 0;
	}
	return 1;
}
