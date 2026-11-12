/*
AE_NET_FLOW_RULES.H

Which machine a datagram is from, when machines share an address (two players behind one home router, or every
player of a server whose internet play goes through a tunnel agent on the server's own computer): the address alone
does not say, the address and the port the datagrams come from do. The host gives each machine a token over its
stream; the client sends it back in a datagram (the bind), which names the port its datagrams leave from. Pure: no
game, no sockets.
*/

#ifndef AE_NET_FLOW_RULES_H
#define AE_NET_FLOW_RULES_H

enum
{
	AE_FLOW_NONE = -1
};

struct ae_flow_slot
{
	unsigned long address; /* the address the machine joined from (host byte order; 0: none) */
	unsigned short port; /* the port its datagrams come from (0: not known yet) */
	unsigned long token; /* what the host told it (0: none) */
	int bound; /* its port came from a bind with its token: it moves only by another bind */
	int joined; /* in the game */
	int local; /* the host's own machine */
	int loaded; /* it has loaded the game: its datagrams begin */
};

/* the machine a datagram from (address, port) is of, or AE_FLOW_NONE. The port found among the machines of the
address wins; a lone machine of the address takes any port (and keeps the last) until a bind with its token has said
its port, after which only a bind moves it (a datagram from another port is nobody's); with several, a machine whose
port is not known yet takes the first datagram from a port no other holds, if it alone is such (of those that have
loaded, when more than one is not known); else none (dropped, not guessed). Such a guess (elimination) is only for
machines that never bind: a bound port never moves but by a bind. With prefer_remote the host's own machine is left out
when another machine has the address. */
long ae_flow_pick(struct ae_flow_slot *slots, long count, unsigned long address, unsigned short port, int prefer_remote);

/* a datagram carrying a machine's token, from (address, port): the machine's port is now that (and no other machine
of the address holds it). The machine, or AE_FLOW_NONE if no joined machine of the address has the token */
long ae_flow_bind(struct ae_flow_slot *slots, long count, unsigned long address, unsigned short port,
	unsigned long token);

/* a machine joining from an address another machine is at: whether it is that machine come back (its old connection
not found dead yet). Only hardware ids that both say and are the same: yes; one not told, or they differ: not (the old
connection is left to be found dead). Never from a tunnel agent's address (ae_flow_address_is_tunnel), whose every
player shares it: the host checks that before it asks */
int ae_flow_same_machine(char const *hardware_id, char const *other_hardware_id);

/* server.tunnel_addresses: "auto" (this computer's own addresses and 127.0.0.1 on a dedicated server; none on
a listen host), "" (none), or a list of IPv4 addresses separated by commas or spaces, "auto" among them. Whether an
address (host byte order) is one: a tunnel agent's, whose every player comes from it; it is no one's identity */
int ae_flow_address_is_tunnel(char const *setting, int dedicated, unsigned long address, unsigned long const *own,
	long own_count);

#endif
