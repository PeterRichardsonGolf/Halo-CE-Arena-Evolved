/*
AE_MCC_CRC_REUSE_RULES_TEST.C

A validation's resource CRC kept for the load (port/linux/game/ae_mcc_crc_reuse_rules.c), over fake files whose stamps
the test sets: kept and taken when nothing changed; refused (the load reads the files) for a file written during the
validation or after it, a file put in its place, another map, another header checksum, another path, a resource the
map uses or stops using, a file with no stamp, a path too long, a cancelled validation; and a refused one dropped.
*/

#include <stdio.h>
#include <string.h>

#include "../game/ae_mcc_crc_reuse_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

/* fake files: a path and its stamp; a file with no stamp is one not there */
static struct
{
	char path[128];
	struct ae_mcc_file_stamp stamp;
	int there;
} files[4];

static int fake_stamp(char const *path, struct ae_mcc_file_stamp *stamp)
{
	int index;

	memset(stamp, 0, sizeof(*stamp));
	for (index = 0; index < 4; index++)
	{
		if (files[index].there && !strcmp(files[index].path, path))
		{
			*stamp = files[index].stamp;
			return 1;
		}
	}
	return 0;
}

static void files_reset(void)
{
	memset(files, 0, sizeof(files));
	strcpy(files[0].path, "/mcc/ws/1/halo1/maps/bitmaps.map");
	strcpy(files[1].path, "/mcc/ws/1/halo1/maps/sounds.map");
	strcpy(files[2].path, "/mcc/halo1/maps/bitmaps.map");
	files[0].there = files[1].there = files[2].there = 1;
	files[0].stamp.size_low = 890000000u;
	files[0].stamp.written_seconds = 100;
	files[0].stamp.changed_seconds = 100;
	files[1].stamp.size_low = 132000000u;
	files[1].stamp.written_seconds = 200;
	files[1].stamp.changed_seconds = 200;
	files[2].stamp.size_low = 178000000u;
	files[2].stamp.changed_seconds = 300;
}

static char validated[2][128];
static char const *loaded[2];
static int job_a, job_b;

static void both_paths(void)
{
	memset(validated, 0, sizeof(validated));
	strcpy(validated[0], files[0].path);
	strcpy(validated[1], files[1].path);
	loaded[0] = files[0].path;
	loaded[1] = files[1].path;
}

/* a validation of 5@ws1 (header checksum 0xcbc0e094) with both resources, its CRC 0x1234 */
static int validate(struct ae_mcc_crc_reuse *reuse)
{
	ae_mcc_crc_reuse_rule_begin(reuse, &job_a, "5@ws1", 0xcbc0e094u, validated, fake_stamp);
	return ae_mcc_crc_reuse_rule_end(reuse, &job_a, 0x1234u, fake_stamp);
}

static void kept_and_taken(void)
{
	struct ae_mcc_crc_reuse reuse;
	uint32_t crc = 0;

	memset(&reuse, 0, sizeof(reuse));
	files_reset();
	both_paths();
	CHECK(validate(&reuse));
	CHECK(ae_mcc_crc_reuse_rule_take(&reuse, "5@ws1", 0xcbc0e094u, 3, loaded, fake_stamp, &crc) && crc == 0x1234u);
	/* (taken again for the next load of the same files) */
	crc = 0;
	CHECK(ae_mcc_crc_reuse_rule_take(&reuse, "5@ws1", 0xcbc0e094u, 3, loaded, fake_stamp, &crc) && crc == 0x1234u);
	CHECK(reuse.kept == 1 && reuse.taken == 2 && reuse.refused == 0);
	/* (a map that uses only its bitmaps) */
	memset(&reuse, 0, sizeof(reuse));
	validated[1][0] = 0;
	CHECK(validate(&reuse));
	CHECK(ae_mcc_crc_reuse_rule_take(&reuse, "5@ws1", 0xcbc0e094u, 1, loaded, fake_stamp, &crc) && crc == 0x1234u);
}

static void refused_after_changes(void)
{
	struct ae_mcc_crc_reuse reuse;
	uint32_t crc = 0;

	/* (written while the validation read it: nothing kept) */
	memset(&reuse, 0, sizeof(reuse));
	files_reset();
	both_paths();
	ae_mcc_crc_reuse_rule_begin(&reuse, &job_a, "5@ws1", 0xcbc0e094u, validated, fake_stamp);
	files[1].stamp.written_seconds++;
	CHECK(!ae_mcc_crc_reuse_rule_end(&reuse, &job_a, 0x1234u, fake_stamp));
	CHECK(!ae_mcc_crc_reuse_rule_take(&reuse, "5@ws1", 0xcbc0e094u, 3, loaded, fake_stamp, &crc));
	/* (written after it, its write time put back: the change time moved) */
	memset(&reuse, 0, sizeof(reuse));
	files_reset();
	CHECK(validate(&reuse));
	files[0].stamp.changed_fraction = 7;
	CHECK(!ae_mcc_crc_reuse_rule_take(&reuse, "5@ws1", 0xcbc0e094u, 3, loaded, fake_stamp, &crc));
	/* (the refused one dropped: the files put back as they were, still nothing taken until a validation) */
	files_reset();
	CHECK(!ae_mcc_crc_reuse_rule_take(&reuse, "5@ws1", 0xcbc0e094u, 3, loaded, fake_stamp, &crc));
	CHECK(validate(&reuse));
	CHECK(ae_mcc_crc_reuse_rule_take(&reuse, "5@ws1", 0xcbc0e094u, 3, loaded, fake_stamp, &crc));
	/* (another file put in its place: another index on Windows, its own size here) */
	files[1].stamp.index_low = 99;
	CHECK(!ae_mcc_crc_reuse_rule_take(&reuse, "5@ws1", 0xcbc0e094u, 3, loaded, fake_stamp, &crc));
	files_reset();
	CHECK(validate(&reuse));
	files[0].stamp.size_low--;
	CHECK(!ae_mcc_crc_reuse_rule_take(&reuse, "5@ws1", 0xcbc0e094u, 3, loaded, fake_stamp, &crc));
	/* (a file gone) */
	files_reset();
	CHECK(validate(&reuse));
	files[1].there = 0;
	CHECK(!ae_mcc_crc_reuse_rule_take(&reuse, "5@ws1", 0xcbc0e094u, 3, loaded, fake_stamp, &crc));
}

static void refused_for_another_load(void)
{
	struct ae_mcc_crc_reuse reuse;
	uint32_t crc = 0;
	char const *other[2];

	memset(&reuse, 0, sizeof(reuse));
	files_reset();
	both_paths();
	/* (another map; the same map with another header checksum) */
	CHECK(validate(&reuse));
	CHECK(!ae_mcc_crc_reuse_rule_take(&reuse, "6@ws1", 0xcbc0e094u, 3, loaded, fake_stamp, &crc));
	CHECK(ae_mcc_crc_reuse_rule_take(&reuse, "5@ws1", 0xcbc0e094u, 3, loaded, fake_stamp, &crc));
	CHECK(!ae_mcc_crc_reuse_rule_take(&reuse, "5@ws1", 0xcbc0e095u, 3, loaded, fake_stamp, &crc));
	/* (another bitmaps.map chosen for the load) */
	CHECK(validate(&reuse));
	other[0] = files[2].path;
	other[1] = files[1].path;
	CHECK(!ae_mcc_crc_reuse_rule_take(&reuse, "5@ws1", 0xcbc0e094u, 3, other, fake_stamp, &crc));
	/* (the load uses a resource the validation did not, or stops using one) */
	CHECK(validate(&reuse));
	CHECK(!ae_mcc_crc_reuse_rule_take(&reuse, "5@ws1", 0xcbc0e094u, 1, loaded, fake_stamp, &crc));
	memset(&reuse, 0, sizeof(reuse));
	validated[1][0] = 0;
	CHECK(validate(&reuse));
	CHECK(!ae_mcc_crc_reuse_rule_take(&reuse, "5@ws1", 0xcbc0e094u, 3, loaded, fake_stamp, &crc));
	/* (no resources: nothing to take; a mask out of range) */
	CHECK(!ae_mcc_crc_reuse_rule_take(&reuse, "5@ws1", 0xcbc0e094u, 0, loaded, fake_stamp, &crc));
	CHECK(!ae_mcc_crc_reuse_rule_take(&reuse, "5@ws1", 0xcbc0e094u, 4, loaded, fake_stamp, &crc));
}

static void nothing_kept(void)
{
	struct ae_mcc_crc_reuse reuse;
	uint32_t crc = 0;

	memset(&reuse, 0, sizeof(reuse));
	files_reset();
	both_paths();
	/* (a file with no stamp, as on macOS) */
	files[0].there = 0;
	CHECK(!validate(&reuse));
	files_reset();
	/* (cancelled: its end finds nothing) */
	ae_mcc_crc_reuse_rule_begin(&reuse, &job_a, "5@ws1", 0xcbc0e094u, validated, fake_stamp);
	ae_mcc_crc_reuse_rule_forget(&reuse, &job_a);
	CHECK(!ae_mcc_crc_reuse_rule_end(&reuse, &job_a, 0x1234u, fake_stamp));
	/* (a CRC of zero is no CRC; a name too long; a map with no resources) */
	ae_mcc_crc_reuse_rule_begin(&reuse, &job_a, "5@ws1", 0xcbc0e094u, validated, fake_stamp);
	CHECK(!ae_mcc_crc_reuse_rule_end(&reuse, &job_a, 0, fake_stamp));
	ae_mcc_crc_reuse_rule_begin(&reuse, &job_a,
		"a_name_longer_than_sixty_four_bytes_for_this_check_of_the_rules_x@ws1", 1, validated, fake_stamp);
	CHECK(!ae_mcc_crc_reuse_rule_end(&reuse, &job_a, 0x1234u, fake_stamp));
	memset(validated, 0, sizeof(validated));
	ae_mcc_crc_reuse_rule_begin(&reuse, &job_a, "5@ws1", 0xcbc0e094u, validated, fake_stamp);
	CHECK(!ae_mcc_crc_reuse_rule_end(&reuse, &job_a, 0x1234u, fake_stamp));
	/* (a path that fills the whole 128 bytes, with no end inside them) */
	memset(validated[0], 'x', sizeof(validated[0]));
	ae_mcc_crc_reuse_rule_begin(&reuse, &job_a, "5@ws1", 0xcbc0e094u, validated, fake_stamp);
	CHECK(!ae_mcc_crc_reuse_rule_end(&reuse, &job_a, 0x1234u, fake_stamp));
	/* (two validations at once, each its own; the second's end does not finish the first's) */
	both_paths();
	ae_mcc_crc_reuse_rule_begin(&reuse, &job_a, "5@ws1", 0xcbc0e094u, validated, fake_stamp);
	ae_mcc_crc_reuse_rule_begin(&reuse, &job_b, "6@ws1", 0x11u, validated, fake_stamp);
	CHECK(ae_mcc_crc_reuse_rule_end(&reuse, &job_b, 0x55u, fake_stamp));
	CHECK(!ae_mcc_crc_reuse_rule_take(&reuse, "5@ws1", 0xcbc0e094u, 3, loaded, fake_stamp, &crc));
	CHECK(ae_mcc_crc_reuse_rule_end(&reuse, &job_a, 0x1234u, fake_stamp));
	CHECK(ae_mcc_crc_reuse_rule_take(&reuse, "6@ws1", 0x11u, 3, loaded, fake_stamp, &crc) && crc == 0x55u);
	CHECK(ae_mcc_crc_reuse_rule_take(&reuse, "5@ws1", 0xcbc0e094u, 3, loaded, fake_stamp, &crc) && crc == 0x1234u);
}

int main(void)
{
	kept_and_taken();
	refused_after_changes();
	refused_for_another_load();
	nothing_kept();
	if (failures)
		return 1;
	printf("ae_mcc_crc_reuse_rules: all checks passed\n");
	return 0;
}
