/*
AE_TEST_RUN_AGENT.C

TEST RUN's live agent on the card (ae_test_run_agent.h).
*/

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "ae_test_run_agent.h"

static struct ae_test_run_agent agent;

static int same(const char *a, const char *b)
{
	for (; *a && *b; a++, b++)
	{
		if (tolower((unsigned char)*a) != tolower((unsigned char)*b))
			return 0;
	}
	return *a == *b;
}

struct ae_test_run_agent const *ae_test_run_agent_get(void)
{
	return &agent;
}

void ae_test_run_agent_clear(void)
{
	memset(&agent, 0, sizeof agent);
}

int ae_test_run_agent_check(int check, const char *verdict, const char *reason, unsigned long now)
{
	static const char *const words[] = { "waiting", "pass", "fail", "look" };
	int index, found = -1;

	if (check < 1 || check > AE_TEST_RUN_AGENT_CHECKS || !verdict)
		return 0;
	for (index = 0; index < 4; index++)
	{
		if (same(verdict, words[index]))
			found = index;
	}
	if (found < 0)
		return 0;
	if (agent.verdict[check - 1] != found || strcmp(agent.reason[check - 1], reason ? reason : ""))
		agent.verdict_at[check - 1] = now ? now : 1;
	agent.verdict[check - 1] = found;
	snprintf(agent.reason[check - 1], sizeof agent.reason[0], "%s", reason ? reason : "");
	agent.heard = 1;
	return 1;
}

void ae_test_run_agent_say(const char *text, unsigned long now)
{
	snprintf(agent.message, sizeof agent.message, "%s", text ? text : "");
	agent.message_at = agent.message[0] ? (now ? now : 1) : 0;
	agent.heard = 1;
}

const char *ae_test_run_agent_word(int verdict)
{
	switch (verdict)
	{
	case AE_TEST_RUN_AGENT_PASS:
		return "PASS";
	case AE_TEST_RUN_AGENT_FAIL:
		return "FAIL";
	case AE_TEST_RUN_AGENT_LOOK:
		return "LOOK";
	default:
		return "WAITING";
	}
}
