/*
AE_JOIN_DIAG_RULES.H

Join diagnostics (log lines only, nothing that machines send each other changes): what the host heard from each
machine since it joined, the sends of a late joiner's start, and a stream that closed. The counters and the texts
are here; ae_join_diag.c holds the per-machine table and writes the lines. Pure, no engine includes (unit test:
port/linux/tests/ae_join_diag_rules_test.c).
*/

#ifndef __AE_JOIN_DIAG_RULES_H
#define __AE_JOIN_DIAG_RULES_H

struct ae_join_diag_machine
{
	unsigned long datagrams;       /* unreliable game updates heard since the machine joined */
	unsigned long streams;         /* messages heard over its stream since it joined */
	unsigned long datagram_time;   /* system milliseconds of the last of each */
	unsigned long stream_time;
	unsigned short datagram_port;  /* the UDP port of the last datagram (0 for none) */
	int late_active;               /* inside the late joiner's start */
	long late_sends;               /* sends of it that went out, and their bytes */
	long late_bytes;
	int closed_said;               /* the stream-closed line was said for this join */
};

/* the machine's slot is used (again): everything starts over */
void ae_join_diag_rules_reset(struct ae_join_diag_machine *machine);
void ae_join_diag_rules_datagram(struct ae_join_diag_machine *machine, unsigned long now, unsigned short port);
void ae_join_diag_rules_stream(struct ae_join_diag_machine *machine, unsigned long now);

/* "datagrams 12 (last 340 ms ago, from port 5151), stream messages 4 (last 120 ms ago)" into text (always ended
with a 0 when size > 0, cut at size - 1); the length it would have, as snprintf's */
int ae_join_diag_rules_heard_text(struct ae_join_diag_machine const *machine, unsigned long now, char *text,
	long size);

/* the late joiner's start begins (counts start over) or ends */
void ae_join_diag_rules_late_begin(struct ae_join_diag_machine *machine);
void ae_join_diag_rules_late_end(struct ae_join_diag_machine *machine);

/* one send of the start: its line into text, or an empty text (and 0) when the machine is not in its start. A
send that went out is counted. */
int ae_join_diag_rules_late_send(struct ae_join_diag_machine *machine, long machine_index, int packet_type, long size,
	int ok, char *text, long text_size);

/* whether the stream-closed line is said now: once per join (1 the first time after a reset) */
int ae_join_diag_rules_closed_say(struct ae_join_diag_machine *machine);

#endif
