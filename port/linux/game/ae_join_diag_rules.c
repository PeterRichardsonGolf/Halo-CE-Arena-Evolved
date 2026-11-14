/* AE_JOIN_DIAG_RULES.C: see ae_join_diag_rules.h */

#include <stdio.h>
#include <string.h>

#include "ae_join_diag_rules.h"

void ae_join_diag_rules_reset(struct ae_join_diag_machine *machine)
{
	memset(machine, 0, sizeof(*machine));
}

void ae_join_diag_rules_datagram(struct ae_join_diag_machine *machine, unsigned long now, unsigned short port)
{
	if (machine->datagrams < 0xFFFFFFFFUL)
		machine->datagrams++;
	machine->datagram_time = now;
	machine->datagram_port = port;
}

void ae_join_diag_rules_stream(struct ae_join_diag_machine *machine, unsigned long now)
{
	if (machine->streams < 0xFFFFFFFFUL)
		machine->streams++;
	machine->stream_time = now;
}

int ae_join_diag_rules_heard_text(struct ae_join_diag_machine const *machine, unsigned long now, char *text, long size)
{
	char datagram[64], stream[48];

	if (size <= 0)
		return 0;
	if (machine->datagrams)
		snprintf(datagram, sizeof(datagram), "last %lu ms ago, from port %u", (now - machine->datagram_time) & 0xFFFFFFFFUL,
			(unsigned)machine->datagram_port);
	else
		snprintf(datagram, sizeof(datagram), "none yet");
	if (machine->streams)
		snprintf(stream, sizeof(stream), "last %lu ms ago", (now - machine->stream_time) & 0xFFFFFFFFUL);
	else
		snprintf(stream, sizeof(stream), "none yet");
	return snprintf(text, (size_t)size, "datagrams %lu (%s), stream messages %lu (%s)", machine->datagrams, datagram,
		machine->streams, stream);
}

void ae_join_diag_rules_late_begin(struct ae_join_diag_machine *machine)
{
	machine->late_active = 1;
	machine->late_sends = 0;
	machine->late_bytes = 0;
}

void ae_join_diag_rules_late_end(struct ae_join_diag_machine *machine)
{
	machine->late_active = 0;
}

int ae_join_diag_rules_late_send(struct ae_join_diag_machine *machine, long machine_index, int packet_type, long size,
	int ok, char *text, long text_size)
{
	int length;

	if (text_size > 0)
		text[0] = 0;
	if (!machine->late_active || text_size <= 0)
		return 0;
	if (ok)
	{
		length = snprintf(text, (size_t)text_size, "join-diag: late start of machine #%ld, send %ld: packet type %d, %ld bytes: ok",
			machine_index, machine->late_sends + 1, packet_type, size);
		machine->late_sends++;
		machine->late_bytes += size;
	}
	else
	{
		length = snprintf(text, (size_t)text_size, "join-diag: late start of machine #%ld, send %ld: packet type %d, %ld bytes: "
			"failed (%ld bytes of the start in %ld sends were written before it)", machine_index, machine->late_sends + 1,
			packet_type, size, machine->late_bytes, machine->late_sends);
	}
	return length;
}

int ae_join_diag_rules_closed_say(struct ae_join_diag_machine *machine)
{
	if (machine->closed_said)
		return 0;
	machine->closed_said = 1;
	return 1;
}
