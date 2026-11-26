/* ae_pve_rules.h */

#include "ae_pve_rules.h"

int ae_pve_rules_records(int campaign, int firefight)
{
	return campaign != 0 || firefight != 0;
}

int ae_pve_rules_stats_socket(int consented)
{
	return consented != 0;
}

int ae_pve_rules_stats_upload(int campaign, int firefight, int consented)
{
	return consented != 0 && ae_pve_rules_records(campaign, firefight);
}
