/*
AE_JOIN_DIAG.H

Join diagnostics for a host (ae_join_diag_rules.h): log lines in debug.txt, each starting "join-diag:". They change
nothing that is sent or decided. Machines are numbered by slot (machine_index); a slot out of range is ignored.
*/

#ifndef __AE_JOIN_DIAG_H
#define __AE_JOIN_DIAG_H

struct network_game_server_client_machine;
/* the slot of a client machine of the host's table (NONE for none): the message handler cannot see the table's struct */
short ae_join_diag_machine_slot(struct network_game_server_client_machine const *machine);
/* a connection took the slot as a joined machine: its counts start over */
void ae_join_diag_machine_joined(long machine_index);
/* the host heard a game update datagram from the machine (and the UDP port it came from) / a message over its stream */
void ae_join_diag_datagram(long machine_index, unsigned short port);
void ae_join_diag_stream(long machine_index);
/* the machine's silence timed it out: what the host heard from it since it joined */
void ae_join_diag_timed_out(long machine_index);
/* the start of a machine that joins a game in progress, around its sends */
void ae_join_diag_late_begin(long machine_index);
void ae_join_diag_late_end(long machine_index);
/* a send to the machine's stream (message: the encoded message, size its bytes); returns ok so a send can return it */
int ae_join_diag_sent(long machine_index, void const *message, long size, int ok);
/* the machine's stream is closed (once per join): its state */
void ae_join_diag_stream_closed(long machine_index, int joined, int loaded, int started_late);

#endif
