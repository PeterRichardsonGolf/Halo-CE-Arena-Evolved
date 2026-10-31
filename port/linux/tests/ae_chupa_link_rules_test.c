/* Arena Evolved and ChupathingyCE 0.8 in one Delta game: the record's size by the host's version, the older record
widened, the rules a client plays by its host's number, the host's HELLO check (port/linux/game/ae_chupa_link_rules.c) */
#include <stdio.h>
#include <string.h>

#include "ae_chupa_link_rules.h"

static int failures;
#define AE_CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

static void test_sizes(void)
{
	/* (ChupathingyCE 0.8's hosts announce 32: the older record; OpenCE's and this build's 36 the newer) */
	AE_CHECK(ae_chupa_link_settings_size(32) == 13120);
	AE_CHECK(ae_chupa_link_settings_size(34) == 13120);
	AE_CHECK(ae_chupa_link_settings_size(35) == 13136);
	AE_CHECK(ae_chupa_link_settings_size(36) == 13136);
	AE_CHECK(ae_chupa_link_settings_size(AE_CHUPA_LINK_VERSION_UNKNOWN) == 0);
	AE_CHECK(ae_chupa_link_settings_size_taken(32, 13120) && !ae_chupa_link_settings_size_taken(32, 13136));
	AE_CHECK(ae_chupa_link_settings_size_taken(36, 13136) && !ae_chupa_link_settings_size_taken(36, 13120));
	AE_CHECK(ae_chupa_link_settings_size_taken(AE_CHUPA_LINK_VERSION_UNKNOWN, 13120));
	AE_CHECK(ae_chupa_link_settings_size_taken(AE_CHUPA_LINK_VERSION_UNKNOWN, 13136));
	AE_CHECK(!ae_chupa_link_settings_size_taken(AE_CHUPA_LINK_VERSION_UNKNOWN, 1936));
	AE_CHECK(!ae_chupa_link_settings_size_taken(36, 0));
}

static void test_widen(void)
{
	static unsigned char record[AE_CHUPA_LINK_SETTINGS_EXTENDED_SIZE];
	unsigned char further[AE_CHUPA_LINK_EXTENDED_OPTIONS_SIZE];
	int index;

	for (index = 0; index < AE_CHUPA_LINK_SETTINGS_EXTENDED_SIZE; index++)
		record[index] = index < AE_CHUPA_LINK_SETTINGS_SIZE ? (unsigned char)(index * 7) : 0xEE;
	for (index = 0; index < AE_CHUPA_LINK_EXTENDED_OPTIONS_SIZE; index++)
		further[index] = (unsigned char)(0xA0 + index);
	ae_chupa_link_settings_widen(record, further);
	/* (everything before the old local data as it was) */
	for (index = 0; index < AE_CHUPA_LINK_SETTINGS_SIZE - 4; index++)
	{
		if (record[index] != (unsigned char)(index * 7))
		{
			AE_CHECK(!"the record's front changed");
			break;
		}
	}
	/* (then the further options, then the 4 bytes of local data, moved 16 on) */
	AE_CHECK(!memcmp(record + 13116, further, 16));
	for (index = 0; index < 4; index++)
		AE_CHECK(record[13132 + index] == (unsigned char)((13116 + index) * 7));
}

static void test_rules(void)
{
	/* (a client: by its host's number; not known, this build's own 36) */
	AE_CHECK(!ae_chupa_link_rules_of_version(34, 1, 32, 36));
	AE_CHECK(!ae_chupa_link_rules_of_version(34, 1, 33, 36));
	AE_CHECK(ae_chupa_link_rules_of_version(34, 1, 34, 36));
	AE_CHECK(ae_chupa_link_rules_of_version(34, 1, 36, 36));
	AE_CHECK(ae_chupa_link_rules_of_version(34, 1, AE_CHUPA_LINK_VERSION_UNKNOWN, 36));
	/* (a host, or a game of its own: its own number, whatever a client's) */
	AE_CHECK(ae_chupa_link_rules_of_version(34, 0, 32, 36));
	AE_CHECK(!ae_chupa_link_rules_of_version(34, 0, 36, 32));
}

static void test_host(void)
{
	/* (ChupathingyCE 0.8.0f and this build say 36; 0.8.0e and older, Arena Evolved 0.3.6 to 0.3.8, 32) */
	AE_CHECK(ae_chupa_link_host_keeps(36, 36));
	AE_CHECK(ae_chupa_link_host_keeps(36, 35));
	AE_CHECK(ae_chupa_link_host_keeps(36, 37));
	AE_CHECK(!ae_chupa_link_host_keeps(36, 32));
	AE_CHECK(!ae_chupa_link_host_keeps(36, 34));
	/* (no HELLO yet, or an OpenCE machine, of the announced number) */
	AE_CHECK(ae_chupa_link_host_keeps(36, 0));
	/* (a host under 35 sends every machine the older record) */
	AE_CHECK(ae_chupa_link_host_keeps(32, 32));
	AE_CHECK(ae_chupa_link_host_keeps(34, 36));
}

int main(void)
{
	test_sizes();
	test_widen();
	test_rules();
	test_host();
	if (failures)
		return 1;
	printf("ae_chupa_link_rules: all passed\n");
	return 0;
}
