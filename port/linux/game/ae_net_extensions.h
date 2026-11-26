/*
AE_NET_EXTENSIONS.H

Capabilities a machine tells another by name (ae_net_extensions_rules.h has the bytes), over game packet 255 of a
machine's stream. The host announces its own to each client machine that joins, marking those its game requires;
a client answers with its own. A game that requires a capability keeps out a machine without it, with a notice. Arena
Evolved requires none: it tells AE_NETEXT_AE_ARENA_EVOLVED and needs nothing, so the exchange shows in the log and in
`netext status`. The machine-name marks and rejection codes of ae_silent_rules.h stay the join's own gates; a machine
that tells this capability also carries the AE mark in its name (ae_net_extensions.c logs it when they disagree).

A stock OpenCE machine logs the packet as one of an unknown type and keeps the connection (nothing it sends or
expects changes), so it never answers; a game that requires nothing removes no one.
*/

#ifndef __AE_NET_EXTENSIONS_H
#define __AE_NET_EXTENSIONS_H

#define AE_NETEXT_AE_ARENA_EVOLVED "ae.arena_evolved"

/* the host, each frame for each client machine of its server (ae_silent_host_machine): the machine announced to
when it has joined, forgotten when it leaves, and removed with a notice when the game requires what it has not */
void ae_net_extensions_host_machine(long machine_index, boolean remote_joined);
/* the host: a client's answer (a packet of type 255: its bytes after the message header, the type byte included) */
void ae_net_extensions_host_handle(long machine_index, void const *data, long size);
/* the client: a game joined, its host's capabilities not known until it announces them */
void ae_net_extensions_client_reset(void);
/* the client: the host's announcement, kept and answered */
void ae_net_extensions_client_handle(void const *data, long size);
/* the host, each frame for each client machine (ae_silent_host_machine): in a Firefight game, its rules sent to a machine that
has Arena Evolved when they are new to it or changed, and again once a game has ended and the lobby is back */
void ae_net_extensions_host_firefight(long machine_index, boolean remote_joined, boolean firefight_game);
/* the host: whether the machine told it the capability at that version or later; the client: whether the host did */
boolean ae_net_extensions_agreed(long machine_index, char const *key, unsigned short version);
boolean ae_net_extensions_host_has(char const *key, unsigned short version);
/* `netext status`: lines of text, the count (at most `maximum`) */
#define AE_NET_EXTENSIONS_STATUS_LINE 200
long ae_net_extensions_status(char (*lines)[AE_NET_EXTENSIONS_STATUS_LINE], long maximum);

#endif
