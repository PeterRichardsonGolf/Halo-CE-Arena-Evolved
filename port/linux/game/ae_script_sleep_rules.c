/*
AE_SCRIPT_SLEEP_RULES.C

See ae_script_sleep_rules.h.
*/

#include <string.h>

#include "ae_script_sleep_rules.h"

void ae_sleeps_clear(struct ae_script_sleeps *sleeps)
{
	memset(sleeps, 0, sizeof(*sleeps));
}

int ae_sleeps_mark(struct ae_script_sleeps *sleeps, long script)
{
	unsigned char bit;

	if (script < 0 || script >= AE_SLEEP_SCRIPTS)
		return 0;
	bit = (unsigned char)(1u << (script % 8));
	if (sleeps->bits[script / 8] & bit)
		return 0;
	sleeps->bits[script / 8] |= bit;
	return 1;
}

int ae_sleeps_test(struct ae_script_sleeps const *sleeps, long script)
{
	if (script < 0 || script >= AE_SLEEP_SCRIPTS)
		return 0;
	return (sleeps->bits[script / 8] & (1u << (script % 8))) != 0;
}

int ae_sleeps_any(struct ae_script_sleeps const *sleeps)
{
	long i;

	for (i = 0; i < AE_SLEEP_BYTES; i++)
		if (sleeps->bits[i])
			return 1;
	return 0;
}

long ae_sleeps_next_pending(struct ae_script_sleeps const *host, struct ae_script_sleeps const *applied, long from)
{
	long script;

	for (script = from < 0 ? 0 : from; script < AE_SLEEP_SCRIPTS; script++)
		if (ae_sleeps_test(host, script) && !ae_sleeps_test(applied, script))
			return script;
	return -1;
}

int ae_sleeps_entry_valid(long entry_scripts, long local_scripts)
{
	return entry_scripts > 0 && entry_scripts == local_scripts;
}
