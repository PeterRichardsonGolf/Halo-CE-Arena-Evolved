#include <stdio.h>
#include <string.h>
#include "ae_network_status_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

static struct ae_netstat_facts facts(void)
{
	struct ae_netstat_facts f;

	memset(&f, 0, sizeof(f));
	f.online = 1;
	f.listed = -1;
	f.seconds_since_answer = -1;
	f.network_version = 44;
	f.version = "0.9.4 (abc1234)";
	f.name = "";
	return f;
}

int main(void)
{
	struct ae_netstat_row rows[AE_NETSTAT_ROWS];
	struct ae_netstat_facts f = facts();
	char age[24];
	int index;

	/* netstat_unknowns: nothing measured says "unknown", never a guess; the version rows always say */
	CHECK(ae_netstat_rows(&f, rows) == AE_NETSTAT_ROWS);
	CHECK(!strcmp(rows[0].label, "ONLINE NAME") && !strcmp(rows[0].value, "unknown"));
	CHECK(!strcmp(rows[1].value, "on"));
	CHECK(!strncmp(rows[2].value, "unknown", 7));
	CHECK(!strcmp(rows[3].label, "SERVERS LISTED") && !strcmp(rows[3].value, "unknown"));
	CHECK(!strncmp(rows[4].value, "unknown", 7));
	CHECK(!strcmp(rows[5].label, "NETWORK VERSION") && !strcmp(rows[5].value, "44"));
	CHECK(!strcmp(rows[6].label, "VERSION") && !strcmp(rows[6].value, "0.9.4 (abc1234)"));
	/* netstat_reached: a master answered, how many and how long ago, the servers listed */
	f.masters = 2; f.asked = 2; f.answered = 1; f.listed = 7; f.seconds_since_answer = 190; f.name = "Rook";
	ae_netstat_rows(&f, rows);
	CHECK(!strcmp(rows[0].value, "Rook"));
	CHECK(!strcmp(rows[2].value, "reached (1 of 2 answered, 3 min ago)"));
	CHECK(!strcmp(rows[3].value, "7"));
	/* netstat_not_reached: all asked, none answered; the last answer kept when there was one */
	f.answered = 0; f.listed = -1; f.seconds_since_answer = -1;
	ae_netstat_rows(&f, rows);
	CHECK(!strcmp(rows[2].value, "not reached (none of 2 answered)"));
	f.seconds_since_answer = 7200;
	ae_netstat_rows(&f, rows);
	CHECK(!strcmp(rows[2].value, "not reached (last answer 2 h ago)"));
	/* netstat_asking: a round under way is not "not reached" */
	f.seconds_since_answer = -1; f.asked = 1;
	ae_netstat_rows(&f, rows);
	CHECK(!strcmp(rows[2].value, "asking..."));
	/* netstat_not_asked: not asked now, answered earlier this run */
	f.asked = 0; f.seconds_since_answer = 30;
	ae_netstat_rows(&f, rows);
	CHECK(!strcmp(rows[2].value, "reached 30 s ago (not asked now)"));
	/* netstat_off: internet play off, or the lists set to the brokers, asks no master */
	f.online = 0;
	ae_netstat_rows(&f, rows);
	CHECK(!strcmp(rows[1].value, "off") && !strncmp(rows[2].value, "not asked", 9) && strstr(rows[4].value, "off"));
	f.online = 1; f.brokers_only = 1;
	ae_netstat_rows(&f, rows);
	CHECK(!strncmp(rows[2].value, "not used", 8));
	/* netstat_port: forwarded = open, strict NAT = closed, else no strict NAT seen; the number is shown */
	f = facts();
	f.port = 43210; f.port_state = AE_NETSTAT_PORT_FORWARDED; f.upnp_allowed = 1;
	ae_netstat_rows(&f, rows);
	CHECK(!strcmp(rows[4].value, "43210, open (UPnP forwarded it)"));
	f.port_state = AE_NETSTAT_PORT_STRICT;
	ae_netstat_rows(&f, rows);
	CHECK(!strncmp(rows[4].value, "43210, closed", 13));
	f.port_state = AE_NETSTAT_PORT_NOT_STRICT; f.upnp_allowed = 0;
	ae_netstat_rows(&f, rows);
	CHECK(!strcmp(rows[4].value, "43210, no strict NAT seen (UPnP is off)"));
	/* (a state with no port is unknown) */
	f.port = 0;
	ae_netstat_rows(&f, rows);
	CHECK(!strncmp(rows[4].value, "unknown", 7));
	/* netstat_fits: every row fits the flow's 48-byte value text */
	f.name = "A very long online name that goes on and on";
	for (index = 0; index < AE_NETSTAT_ROWS; index++)
		CHECK(strlen(rows[index].label) < 24);
	/* netstat_age */
	ae_netstat_age(5, age, sizeof(age)); CHECK(!strcmp(age, "5 s"));
	ae_netstat_age(59, age, sizeof(age)); CHECK(!strcmp(age, "59 s"));
	ae_netstat_age(3600, age, sizeof(age)); CHECK(!strcmp(age, "1 h"));
	ae_netstat_age(3 * 86400, age, sizeof(age)); CHECK(!strcmp(age, "3 days"));
	ae_netstat_age(-4, age, sizeof(age)); CHECK(!strcmp(age, "0 s"));
	if (!failures)
		printf("ae_network_status_rules_test: all passed\n");
	return failures ? 1 : 0;
}
