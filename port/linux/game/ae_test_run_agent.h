/*
AE_TEST_RUN_AGENT.H

TEST RUN's live agent on the card (DEV MODE): what the agent (tools/ae_test/live.py) says of the test being played, as it
judges the event stream: each check's verdict and a reason, and its latest message. It comes in by the dev command
"agent" (ae_dev_cmd.c) over the console or the telnet console. Cleared when a test is applied. Pure: string only.
*/

#ifndef AE_TEST_RUN_AGENT_H
#define AE_TEST_RUN_AGENT_H

#define AE_TEST_RUN_AGENT_CHECKS 3
#define AE_TEST_RUN_AGENT_REASON 120
#define AE_TEST_RUN_AGENT_MESSAGE 160

enum ae_test_run_agent_verdict
{
	AE_TEST_RUN_AGENT_WAITING = 0, /* watching, nothing seen yet (also: no agent) */
	AE_TEST_RUN_AGENT_PASS,
	AE_TEST_RUN_AGENT_FAIL,
	AE_TEST_RUN_AGENT_LOOK        /* no rule: the owner's eye decides */
};

struct ae_test_run_agent
{
	int verdict[AE_TEST_RUN_AGENT_CHECKS];
	char reason[AE_TEST_RUN_AGENT_CHECKS][AE_TEST_RUN_AGENT_REASON];
	unsigned long verdict_at[AE_TEST_RUN_AGENT_CHECKS]; /* when it last changed (milliseconds), 0 never */
	char message[AE_TEST_RUN_AGENT_MESSAGE];
	unsigned long message_at;                           /* 0: no message */
	int heard;                                          /* an agent has spoken since the test was applied */
};

/* the agent's state of the test being played */
struct ae_test_run_agent const *ae_test_run_agent_get(void);
/* forgets everything (a test applied) */
void ae_test_run_agent_clear(void);
/* check n (1 to 3) is verdict ("pass", "fail", "look", "waiting") with reason (may be "" or NULL), at now: 1 done, 0
for a bad check or verdict */
int ae_test_run_agent_check(int check, const char *verdict, const char *reason, unsigned long now);
/* the agent's message, at now */
void ae_test_run_agent_say(const char *text, unsigned long now);
/* a verdict's word: "WAITING", "PASS", "FAIL", "LOOK" */
const char *ae_test_run_agent_word(int verdict);

#endif
