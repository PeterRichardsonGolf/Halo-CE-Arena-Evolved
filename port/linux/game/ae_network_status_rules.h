/*
AE_NETWORK_STATUS_RULES.H

The rows of the TAB menu's NETWORK STATUS page, from what the game already knows (nothing here asks the network):
what a row says when it is not known is "unknown", never a guess. Pure (port/linux/tests/ae_network_status_rules_test.c);
the screen is ae_screen_network_status.c.
*/

#ifndef __AE_NETWORK_STATUS_RULES_H
#define __AE_NETWORK_STATUS_RULES_H

enum { AE_NETSTAT_ROWS = 7 };
/* the port's state, as p2p.h's P2P_PORT_* (the same numbers) */
enum { AE_NETSTAT_PORT_UNKNOWN, AE_NETSTAT_PORT_FORWARDED, AE_NETSTAT_PORT_STRICT, AE_NETSTAT_PORT_NOT_STRICT };

struct ae_netstat_facts
{
	int online;                  /* network.online */
	int brokers_only;            /* network.listings: the masters are not asked at all */
	int masters, answered, asked;/* master_list_counts */
	int listed;                  /* master_list_status: the servers the last answered round listed; -1 none */
	long seconds_since_answer;   /* ... and how long ago a master answered; -1 never this run */
	int port, port_state;        /* p2p_port_status: the tunnel's UDP port (0 none) and its state */
	int upnp_allowed;            /* network.allow_upnp */
	int network_version;
	char const *version;         /* the build's version line */
	char const *name;            /* the online name of the player ("" none) */
};

struct ae_netstat_row { char label[24]; char value[96]; };

/* the rows in the page's order: ONLINE NAME, INTERNET PLAY, MASTER SERVER, SERVERS LISTED, YOUR PORT, NETWORK VERSION,
VERSION; always AE_NETSTAT_ROWS */
int ae_netstat_rows(struct ae_netstat_facts const *facts, struct ae_netstat_row *out);

/* "3 s", "4 min", "2 h", "3 days" */
void ae_netstat_age(long seconds, char *text, int size);

#endif
