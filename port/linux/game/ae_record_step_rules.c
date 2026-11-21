/*
AE_RECORD_STEP_RULES.C

The stepping order of the SERVICE RECORD pages (ae_record_step_rules.h).
*/

#include <string.h>

#include "ae_record_step_rules.h"

/* 1 when player a comes before player b: the viewer's own first, then the most recently seen */
static int before(struct ae_service_record const *record, char const *viewer_key, int a, int b)
{
	int a_own = viewer_key[0] && !strcmp(record->player[a].key, viewer_key);
	int b_own = viewer_key[0] && !strcmp(record->player[b].key, viewer_key);

	if (a_own != b_own)
		return a_own;
	return record->player[a].seen > record->player[b].seen;
}

int ae_rstep_order(struct ae_service_record const *record, char const *viewer_key, int *out)
{
	int count = 0, index, at;

	if (!record || !out)
		return 0;
	if (!viewer_key)
		viewer_key = "";
	/* (an insertion sort: stable, so players seen in the same game stay in the record's order) */
	for (index = 0; index < record->player_count && index < AE_SR_PLAYERS; index++)
	{
		for (at = count; at > 0 && before(record, viewer_key, index, out[at - 1]); at--)
			out[at] = out[at - 1];
		out[at] = index;
		count++;
	}
	return count;
}

int ae_rstep_target(struct ae_service_record const *record, char const *viewer_key, char const *current_key, int delta)
{
	int order[AE_SR_PLAYERS], count = ae_rstep_order(record, viewer_key, order), at;

	if (count <= 0)
		return -1;
	if (!current_key)
		current_key = "";
	for (at = 0; at < count; at++)
		if (!strcmp(record->player[order[at]].key, current_key))
			break;
	if (at == count)
		return order[delta > 0 ? 0 : count - 1];
	if (count == 1)
		return -1;
	return order[(at + (delta > 0 ? 1 : count - 1)) % count];
}
