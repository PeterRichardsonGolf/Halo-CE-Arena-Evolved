/*
AE_FIREFIGHT_DOORS_RULES.C

The rules of ae_firefight_doors_rules.h.
*/

#include "ae_firefight_doors_rules.h"

int ae_firefight_doors_engine_allowed(int dev_mode_setting, int network_client, int lobby_open)
{
	return dev_mode_setting != 0 || network_client != 0 || lobby_open != 0;
}

int ae_firefight_doors_engine_index(int engine_index, int firefight_index, int fallback, int allowed)
{
	return engine_index == firefight_index && !allowed ? fallback : engine_index;
}

int ae_firefight_doors_pack_build(int pack_setting, int dev_mode_setting, int lobby_open)
{
	return lobby_open != 0 || (pack_setting != 0 && dev_mode_setting != 0);
}

int ae_firefight_doors_data_state(int maps_present, int ready, int writing, int failed)
{
	if (writing)
		return AE_FFD_DATA_PREPARING;
	if (ready)
		return AE_FFD_DATA_READY;
	if (maps_present <= 0)
		return AE_FFD_DATA_NEEDS_MAPS;
	return failed ? AE_FFD_DATA_FAILED : AE_FFD_DATA_PREPARING;
}

const char *ae_firefight_doors_data_reason(int state)
{
	switch (state)
	{
	case AE_FFD_DATA_PREPARING: return "PREPARING";
	case AE_FFD_DATA_NEEDS_MAPS: return "NEEDS THE XBOX MAPS";
	case AE_FFD_DATA_FAILED: return "DATA NOT BUILT";
	}
	return "";
}

const char *ae_firefight_doors_data_help(int state)
{
	switch (state)
	{
	case AE_FFD_DATA_PREPARING:
		return "Firefight copies its enemies, ships and weapons from the campaign maps once. It is preparing them now.";
	case AE_FFD_DATA_NEEDS_MAPS:
		return "Firefight copies its enemies, ships and weapons from the Xbox campaign maps, and this PC has none of them yet.";
	case AE_FFD_DATA_FAILED:
		return "Firefight's data could not be built from the campaign maps. Is the disk full?";
	}
	return "Waves of enemies, alone or together: survive as long as you can.";
}

int ae_firefight_doors_data_startable(int state)
{
	return state == AE_FFD_DATA_READY;
}

int ae_firefight_doors_skulls_send(int skulls_setting)
{
	return skulls_setting != 0;
}

int ae_firefight_doors_skulls_pack_early(int skulls_setting, int dev_mode_setting)
{
	return skulls_setting != 0 || dev_mode_setting != 0;
}
