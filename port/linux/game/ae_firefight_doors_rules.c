/*
AE_FIREFIGHT_DOORS_RULES.C

The rules of ae_firefight_doors_rules.h.
*/

#include "ae_firefight_doors_rules.h"

int ae_firefight_doors_engine_allowed(int dev_mode_setting, int network_client)
{
	return dev_mode_setting != 0 || network_client != 0;
}

int ae_firefight_doors_engine_index(int engine_index, int firefight_index, int fallback, int allowed)
{
	return engine_index == firefight_index && !allowed ? fallback : engine_index;
}

int ae_firefight_doors_pack_build(int pack_setting, int dev_mode_setting)
{
	return pack_setting != 0 && dev_mode_setting != 0;
}

int ae_firefight_doors_skulls_send(int skulls_setting)
{
	return skulls_setting != 0;
}

int ae_firefight_doors_skulls_pack_early(int skulls_setting, int dev_mode_setting)
{
	return skulls_setting != 0 || dev_mode_setting != 0;
}
