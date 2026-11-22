#include <stdio.h>
#include <string.h>

#include "ae_glue_career.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

static char disk_text[AE_CAREER_TEXT];
static long disk_length;
static int crash_mark;
static int crash_mark_supported = 1;
static int writes_succeed = 1;

void platform_log(char const *format, ...)
{
	(void)format;
}

long ae_private_file_read(const char *name, char *text, long size)
{
	if (strcmp(name, "career.txt") || disk_length < 0 || disk_length >= size)
		return -1;
	memcpy(text, disk_text, (size_t)disk_length + 1);
	return disk_length;
}

/* (the profile import's recovery of a cut off write: none here) */
void ae_profile_move_recover(void)
{
}

int ae_profile_move_pending(void)
{
	return 0;
}

int ae_private_file_write(const char *name, const char *text, long length)
{
	if (!strcmp(name, "career.crashed"))
	{
		crash_mark = 1;
		return 1;
	}
	if (strcmp(name, "career.txt") || !writes_succeed || length < 0 || length >= (long)sizeof(disk_text))
		return 0;
	memcpy(disk_text, text, (size_t)length);
	disk_text[length] = 0;
	disk_length = length;
	return 1;
}

int ae_private_file_exists(const char *name)
{
	return !strcmp(name, "career.crashed") && crash_mark;
}

int ae_private_file_remove(const char *name)
{
	if (strcmp(name, "career.crashed") || !crash_mark)
		return 0;
	crash_mark = 0;
	return 1;
}

int ae_private_file_set_aside(const char *name)
{
	(void)name;
	return 0;
}

int ae_private_crash_mark_arm(const char *name)
{
	(void)name;
	return crash_mark_supported;
}

int ae_private_crash_mark_supported(void)
{
	return crash_mark_supported;
}

void ae_private_crash_mark_disarm(void)
{
}

static void pending_file(void)
{
	struct ae_career career;

	ae_career_init(&career);
	CHECK(ae_career_pending_set(&career, "chief", "team-slayer", 10));
	disk_length = ae_career_write(&career, disk_text, sizeof(disk_text));
	CHECK(disk_length > 0);
	crash_mark = 0;
	crash_mark_supported = 1;
	writes_succeed = 1;
	ae_career_store_forget();
}

static void test_failed_settlement_keeps_pending_and_mark(void)
{
	struct ae_career *career;
	struct ae_career persisted;

	pending_file();
	crash_mark = 1;
	writes_succeed = 0;
	career = ae_career_store();
	CHECK(career->pending_count == 1);
	CHECK(crash_mark);
	/* A later successful write must not persist the failed settlement. */
	writes_succeed = 1;
	CHECK(ae_career_store_save());
	CHECK(ae_career_read(&persisted, disk_text) == AE_CAREER_READ_OK);
	CHECK(persisted.pending_count == 1);
	CHECK(ae_career_social_find(&persisted, "chief", "team-slayer") == NULL);
	ae_career_store_forget();
	career = ae_career_store();
	CHECK(career->pending_count == 0);
	CHECK(ae_career_social_find(career, "chief", "team-slayer") == NULL);
}

static void test_platform_without_crash_mark_forgives(void)
{
	struct ae_career *career;

	pending_file();
	crash_mark_supported = 0;
	career = ae_career_store();
	CHECK(career->pending_count == 0);
	CHECK(ae_career_social_find(career, "chief", "team-slayer") == NULL);
}

static void test_successful_settlement_removes_mark(void)
{
	struct ae_career *career;

	pending_file();
	crash_mark = 1;
	career = ae_career_store();
	CHECK(career->pending_count == 0);
	CHECK(!crash_mark);
}

int main(void)
{
	test_failed_settlement_keeps_pending_and_mark();
	test_platform_without_crash_mark_forgives();
	test_successful_settlement_removes_mark();
	if (failures)
		return 1;
	printf("ok\n");
	return 0;
}
