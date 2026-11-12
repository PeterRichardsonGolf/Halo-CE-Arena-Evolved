/*
AE_MASTER_BACKOFF_RULES.C

See ae_master_backoff_rules.h.
*/

#include "ae_master_backoff_rules.h"

#include <string.h>

unsigned long ae_master_backoff_seconds(int failures)
{
	unsigned long seconds = 60;

	if (failures <= 0)
		return 0;
	while (--failures > 0 && seconds < 900)
		seconds *= 2;
	return seconds < 900 ? seconds : 900;
}

int ae_master_publish_pass(int failures, int pass)
{
	return pass ? failures > 0 : failures <= 0;
}

int ae_master_answer_failed(int error, int status, const char *code)
{
	if (error || status >= 500)
		return 1;
	if (!code)
		return 0;
	return !strncmp(code, "status_", 7) || !strcmp(code, "unknown_key") || !strcmp(code, "publisher_pending") ||
		!strcmp(code, "publisher_suspended");
}
