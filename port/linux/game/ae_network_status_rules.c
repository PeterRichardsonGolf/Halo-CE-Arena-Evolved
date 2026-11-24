/*
AE_NETWORK_STATUS_RULES.C

See ae_network_status_rules.h.
*/

#include <stdio.h>

#include "ae_network_status_rules.h"

void ae_netstat_age(long seconds, char *text, int size)
{
	if (seconds < 0)
		seconds = 0;
	if (seconds < 60)
		snprintf(text, (size_t)size, "%ld s", seconds);
	else if (seconds < 3600)
		snprintf(text, (size_t)size, "%ld min", seconds / 60);
	else if (seconds < 86400)
		snprintf(text, (size_t)size, "%ld h", seconds / 3600);
	else
		snprintf(text, (size_t)size, "%ld days", seconds / 86400);
}

static void put(struct ae_netstat_row *row, char const *label, char const *value)
{
	snprintf(row->label, sizeof(row->label), "%s", label);
	snprintf(row->value, sizeof(row->value), "%s", value);
}

int ae_netstat_rows(struct ae_netstat_facts const *facts, struct ae_netstat_row *out)
{
	char value[96], age[24];

	put(&out[0], "ONLINE NAME", facts->name && facts->name[0] ? facts->name : "unknown");
	put(&out[1], "INTERNET PLAY", facts->online ? "on" : "off");
	/* (the masters are asked only while the server browser is open: before that it is not asked, not unreachable) */
	age[0] = 0;
	if (facts->seconds_since_answer >= 0)
		ae_netstat_age(facts->seconds_since_answer, age, (int)sizeof(age));
	if (!facts->online)
		snprintf(value, sizeof(value), "not asked (internet play is off)");
	else if (facts->brokers_only)
		snprintf(value, sizeof(value), "not used (lists set to brokers)");
	else if (facts->answered > 0)
		snprintf(value, sizeof(value), "reached (%d of %d answered, %s ago)", facts->answered, facts->masters, age);
	else if (facts->masters > 0 && facts->asked >= facts->masters)
	{
		if (age[0])
			snprintf(value, sizeof(value), "not reached (last answer %s ago)", age);
		else
			snprintf(value, sizeof(value), "not reached (none of %d answered)", facts->masters);
	}
	else if (facts->asked > 0)
		snprintf(value, sizeof(value), "asking...");
	else if (age[0])
		snprintf(value, sizeof(value), "reached %s ago (not asked now)", age);
	else
		snprintf(value, sizeof(value), "unknown (not asked yet)");
	put(&out[2], "MASTER SERVER", value);
	if (facts->listed >= 0)
		snprintf(value, sizeof(value), "%d", facts->listed);
	else
		snprintf(value, sizeof(value), "unknown");
	put(&out[3], "SERVERS LISTED", value);
	if (facts->port_state == AE_NETSTAT_PORT_UNKNOWN || facts->port <= 0)
		snprintf(value, sizeof(value), "unknown (%s)", facts->online ? "internet play has not started" :
			"internet play is off");
	else if (facts->port_state == AE_NETSTAT_PORT_FORWARDED)
		snprintf(value, sizeof(value), "%d, open (UPnP forwarded it)", facts->port);
	else if (facts->port_state == AE_NETSTAT_PORT_STRICT)
		snprintf(value, sizeof(value), "%d, closed (a strict NAT, not forwarded)", facts->port);
	else
		snprintf(value, sizeof(value), "%d, no strict NAT seen%s", facts->port, facts->upnp_allowed ? "" :
			" (UPnP is off)");
	put(&out[4], "YOUR PORT", value);
	snprintf(value, sizeof(value), "%d", facts->network_version);
	put(&out[5], "NETWORK VERSION", value);
	put(&out[6], "VERSION", facts->version && facts->version[0] ? facts->version : "unknown");
	return AE_NETSTAT_ROWS;
}
