/*
AE_MASTER_BACKOFF_RULES.H

(0.8.2) A server's publishing to its master servers (port/linux/src/server_master.c): one master that did not answer
(no route, a connect that hangs) held up every other master behind it, each round, and a master that refuses the
publisher (not approved yet) was asked every round and logged it. A master that failed is asked again after a wait that
doubles, 15 minutes at most; each round asks the masters that answered last time first. Pure:
port/linux/tests/ae_master_backoff_rules_test.c.
*/

#ifndef AE_MASTER_BACKOFF_RULES_H
#define AE_MASTER_BACKOFF_RULES_H

/* the seconds before a master that failed failures times in a row is asked again (0: the usual round) */
unsigned long ae_master_backoff_seconds(int failures);
/* whether a master is asked in a round's pass (0: those that answered last time, 1: those that failed) */
int ae_master_publish_pass(int failures, int pass);

/* (review 0.8.2) whether a master's answer counts as a failure for the back-off: no answer (error), any 5xx (a JSON
error body too: a master that is down answers 500 / 503), a status the JSON did not explain (status_N), or a refusal
that lasts (publisher_pending, unknown_key, publisher_suspended). A 429 has its own wait; others reset the count */
int ae_master_answer_failed(int error, int status, const char *code);

#endif
