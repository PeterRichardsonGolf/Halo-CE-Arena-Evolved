#include <stdio.h>
#include <string.h>
#include "ae_script_notice_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

int main(void)
{
	char first[AE_SCRIPT_NOTICE_LINE_SIZE], second[AE_SCRIPT_NOTICE_LINE_SIZE];

	/* nothing refused: nothing to say */
	CHECK(ae_script_notice_lines(0, first, second) == 0 && !first[0] && !second[0]);
	CHECK(ae_script_notice_lines(-3, first, second) == 0 && !first[0]);
	/* one: singular */
	CHECK(ae_script_notice_lines(1, first, second) == 2);
	CHECK(!strcmp(first, "1 of this map's scripts was switched off;"));
	CHECK(!strcmp(second, "the mission may not play correctly"));
	/* more: plural */
	CHECK(ae_script_notice_lines(12, first, second) == 2);
	CHECK(!strcmp(first, "12 of this map's scripts were switched off;"));
	/* a message holds 62 characters */
	CHECK(ae_script_notice_lines(99999, first, second) == 2 && strlen(first) < 62 && strlen(second) < 62);
	CHECK(ae_script_notice_lines(1000000, first, second) == 2 && !strncmp(first, "99999 of", 8));
	/* due: refused, unshown, a campaign level, past the first second */
	CHECK(ae_script_notice_due(12, 0, 1, 0, AE_SCRIPT_NOTICE_TICK));
	CHECK(!ae_script_notice_due(12, 0, 1, 0, AE_SCRIPT_NOTICE_TICK - 1));
	CHECK(!ae_script_notice_due(0, 0, 1, 0, 500));
	CHECK(!ae_script_notice_due(12, 1, 1, 0, 500));
	CHECK(!ae_script_notice_due(12, 0, 0, 0, 500));
	/* a cinematic or scripted scene: the HUD is hidden, the line waits */
	CHECK(!ae_script_notice_due(12, 0, 1, 1, 500));
	/* the debug setting names one script */
	CHECK(ae_script_notice_forced("mission_spasm", "mission_spasm"));
	CHECK(ae_script_notice_forced("Mission_Spasm", "mission_spasm"));
	CHECK(!ae_script_notice_forced("mission_spasm", "mission_spasm2"));
	CHECK(!ae_script_notice_forced("mission", "mission_spasm"));
	CHECK(!ae_script_notice_forced("", "mission_spasm"));
	CHECK(!ae_script_notice_forced(NULL, "mission_spasm"));
	if (failures)
		return 1;
	printf("ae_script_notice_rules: ok\n");
	return 0;
}
