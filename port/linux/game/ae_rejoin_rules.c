/*
AE_REJOIN_RULES.C

REJOIN AFTER A DROP: the pure rules (ae_rejoin_rules.h). No engine includes.
*/

#include "ae_rejoin_rules.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int ae_rejoin_wanted(int enabled, int armed, int lost_error, int host_gave_reason)
{
	return enabled && armed && lost_error && !host_gave_reason;
}

unsigned long ae_rejoin_next_try(unsigned long lost_at, int tries_done, unsigned long last_failed_at)
{
	return tries_done <= 0 ? lost_at + AE_REJOIN_FIRST_MS : last_failed_at + AE_REJOIN_GAP_MS;
}

int ae_rejoin_give_up(unsigned long now, unsigned long lost_at, int tries_done, int trying)
{
	if (now - lost_at >= AE_REJOIN_WINDOW_MS)
		return 1;
	return tries_done >= AE_REJOIN_TRIES && !trying;
}

int ae_rejoin_try_due(unsigned long now, unsigned long lost_at, unsigned long next_try)
{
	/* (by differences from the drop: the clock may wrap) */
	return now - lost_at >= next_try - lost_at && now - lost_at < AE_REJOIN_WINDOW_MS;
}

void ae_rejoin_body(char *out, int size, int dedicated, char const *where, int try_number)
{
	if (!out || size <= 0)
		return;
	if (try_number < 1)
		try_number = 1;
	if (try_number > AE_REJOIN_TRIES)
		try_number = AE_REJOIN_TRIES;
	snprintf(out, (size_t)size, "The connection to the %s was lost. Rejoining %s (try %d of %d).",
		dedicated ? "server" : "host", where && where[0] ? where : "the game", try_number, AE_REJOIN_TRIES);
}

int ae_rejoin_replaces(int same_name, char const *joining_hardware_id, char const *old_hardware_id,
	unsigned long joining_address, unsigned long old_address, unsigned long silent_ms)
{
	int same_machine;

	if (!same_name || silent_ms < AE_REJOIN_STALE_MS)
		return 0;
	/* (both told a hardware id: it decides; else the addresses, both known) */
	if (joining_hardware_id && joining_hardware_id[0] && old_hardware_id && old_hardware_id[0])
		same_machine = !strcmp(joining_hardware_id, old_hardware_id);
	else
		same_machine = joining_address && joining_address == old_address;
	return same_machine;
}

int ae_rejoin_same_name(unsigned short const *a, unsigned short const *b, int length)
{
	int index;

	if (!a || !b || length <= 0 || !a[0])
		return 0;
	for (index = 0; index < length; index++)
	{
		if (a[index] != b[index])
			return 0;
		if (!a[index])
			return 1;
	}
	return 1;
}

int ae_rejoin_test_drop_parse(char const *text, double *seconds, int *silent)
{
	char *end;
	double value;

	if (!text || !text[0])
		return 0;
	value = strtod(text, &end);
	if (end == text || *end != ':' || !(value > 0.0) || value > 3600.0)
		return 0;
	end++;
	if (!strcmp(end, "reset"))
	{
		if (silent)
			*silent = 0;
	}
	else if (!strcmp(end, "silent"))
	{
		if (silent)
			*silent = 1;
	}
	else
		return 0;
	if (seconds)
		*seconds = value;
	return 1;
}
