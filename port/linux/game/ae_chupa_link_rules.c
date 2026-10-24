/*
AE_CHUPA_LINK_RULES.C

Arena Evolved and ChupathingyCE 0.8 in one Delta game: the rules
(ae_chupa_link_rules.h).
*/

#include "ae_chupa_link_rules.h"

#include <string.h>

int ae_chupa_link_settings_size(unsigned version)
{
	if (version == AE_CHUPA_LINK_VERSION_UNKNOWN)
		return 0;
	return version >= AE_CHUPA_LINK_EXTENDED_OPTIONS_VERSION ? AE_CHUPA_LINK_SETTINGS_EXTENDED_SIZE :
		AE_CHUPA_LINK_SETTINGS_SIZE;
}

int ae_chupa_link_settings_size_taken(unsigned version, unsigned long total_size)
{
	int size = ae_chupa_link_settings_size(version);

	if (size)
		return total_size == (unsigned long)size;
	return total_size == AE_CHUPA_LINK_SETTINGS_SIZE || total_size == AE_CHUPA_LINK_SETTINGS_EXTENDED_SIZE;
}

void ae_chupa_link_settings_widen(unsigned char *record, unsigned char const *further)
{
	/* (where the older record's local data is, after the options' first 0x1C) */
	unsigned char *at = record + AE_CHUPA_LINK_SETTINGS_SIZE - AE_CHUPA_LINK_LOCAL_DATA_SIZE;

	memmove(at + AE_CHUPA_LINK_EXTENDED_OPTIONS_SIZE, at, AE_CHUPA_LINK_LOCAL_DATA_SIZE);
	memcpy(at, further, AE_CHUPA_LINK_EXTENDED_OPTIONS_SIZE);
}

int ae_chupa_link_rules_of_version(unsigned version, int client, unsigned host_version, unsigned announce)
{
	if (client && host_version != AE_CHUPA_LINK_VERSION_UNKNOWN)
		return host_version >= version;
	return announce >= version;
}

int ae_chupa_link_host_keeps(unsigned announce, unsigned hello_version)
{
	return !hello_version || announce < AE_CHUPA_LINK_EXTENDED_OPTIONS_VERSION ||
		hello_version >= AE_CHUPA_LINK_EXTENDED_OPTIONS_VERSION;
}
