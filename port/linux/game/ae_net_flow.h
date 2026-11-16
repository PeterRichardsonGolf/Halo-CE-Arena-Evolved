/*
AE_NET_FLOW.H

Which machine a datagram is from when machines share an address (ae_net_flow_rules.h): the host's token to each
machine over its stream, the machine's bind datagram back, the table of the machines' datagram ports.
*/

#ifndef AE_NET_FLOW_H
#define AE_NET_FLOW_H

#include "ae_net_flow_rules.h"

/* the message (_distributed_message_ae_flow): from the host over a stream, the machine's token; from a machine in a
datagram, the same token, which says what port the datagram is from */
struct ae_flow_entry
{
	unsigned long token;
};

/* the machines' slots (host: by machine index) */
struct ae_flow_slot *ae_net_flow_slots(void);
/* a machine joined the game, or left it */
void ae_net_flow_machine_joined(long machine_index);
void ae_net_flow_machine_left(long machine_index);
/* every frame: the host tells each machine its token now and then; a client sends its bind once a second */
void ae_net_flow_update(void);
/* (a client) the host's token */
void ae_net_flow_client_token(void const *entries, long count);
/* (the host) whether a datagram is a bind: the token it carries, and its size is a bind's */
int ae_net_flow_is_bind(void const *message, unsigned short size, unsigned long *token);
/* whether an address is a tunnel agent's (server.tunnel_addresses): no one's identity */
int ae_net_flow_address_is_tunnel(unsigned long address);

/* an address as an identity (a ban's, a kick's): none for a tunnel agent's, whose every player comes from it */
#define AE_FLOW_IDENTITY(address) (ae_net_flow_address_is_tunnel(address) ? 0UL : (unsigned long)(address))

#endif
