/* The post-game report's kept summaries (see the header). */
#include <string.h>

#include "ae_pgcr_data.h"

static struct ae_pgcr_summary summaries[AE_PGCR_PLAYERS];
static int count;
static long report_number;

void ae_pgcr_data_reset(void)
{
	memset(summaries, 0, sizeof(summaries));
	count = 0;
}

int ae_pgcr_data_set(struct ae_pgcr_summary const *summary)
{
	if (!summary->valid || summary->slot < 0 || summary->slot >= AE_PGCR_PLAYERS)
		return 0;
	if (!summaries[summary->slot].valid)
		count++;
	if (count == 1 && !summaries[summary->slot].valid)
		report_number++;
	summaries[summary->slot] = *summary;
	return 1;
}

struct ae_pgcr_summary const *ae_pgcr_data_get(int slot)
{
	return slot >= 0 && slot < AE_PGCR_PLAYERS && summaries[slot].valid ? &summaries[slot] : NULL;
}

int ae_pgcr_data_have_summary(void)
{
	return count > 0;
}

long ae_pgcr_data_report_number(void)
{
	return report_number;
}
