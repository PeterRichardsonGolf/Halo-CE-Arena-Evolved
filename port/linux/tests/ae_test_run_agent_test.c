#include <stdio.h>
#include <string.h>
#include "ae_test_run_agent.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

int main(void)
{
	struct ae_test_run_agent const *agent = ae_test_run_agent_get();
	char long_reason[400];

	ae_test_run_agent_clear();
	CHECK(!agent->heard && agent->verdict[0] == AE_TEST_RUN_AGENT_WAITING && !agent->message_at);
	CHECK(ae_test_run_agent_check(2, "FAIL", "no banshee made", 100) && agent->verdict[1] == AE_TEST_RUN_AGENT_FAIL);
	CHECK(!strcmp(agent->reason[1], "no banshee made") && agent->verdict_at[1] == 100 && agent->heard);
	/* (the same verdict again keeps its time: the card's LOOK pulse does not start over) */
	CHECK(ae_test_run_agent_check(2, "fail", "no banshee made", 200) && agent->verdict_at[1] == 100);
	CHECK(ae_test_run_agent_check(2, "pass", NULL, 300) && agent->verdict_at[1] == 300 && !agent->reason[1][0]);
	CHECK(ae_test_run_agent_check(1, "look", "", 0) && agent->verdict[0] == AE_TEST_RUN_AGENT_LOOK &&
		agent->verdict_at[0] == 1);
	CHECK(!ae_test_run_agent_check(0, "pass", "", 1) && !ae_test_run_agent_check(4, "pass", "", 1));
	CHECK(!ae_test_run_agent_check(1, "maybe", "", 1) && !ae_test_run_agent_check(1, NULL, "", 1));
	memset(long_reason, 'x', sizeof long_reason - 1);
	long_reason[sizeof long_reason - 1] = 0;
	CHECK(ae_test_run_agent_check(3, "pass", long_reason, 5) &&
		strlen(agent->reason[2]) == AE_TEST_RUN_AGENT_REASON - 1);
	ae_test_run_agent_say("Try the ramp again", 400);
	CHECK(!strcmp(agent->message, "Try the ramp again") && agent->message_at == 400);
	ae_test_run_agent_say("", 500);
	CHECK(!agent->message_at);
	CHECK(!strcmp(ae_test_run_agent_word(AE_TEST_RUN_AGENT_LOOK), "LOOK") &&
		!strcmp(ae_test_run_agent_word(99), "WAITING"));
	ae_test_run_agent_clear();
	CHECK(!agent->heard && !agent->reason[1][0]);
	if (failures)
		printf("%d failures\n", failures);
	return failures != 0;
}
