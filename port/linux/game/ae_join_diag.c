/*
AE_JOIN_DIAG.C

The per-machine table of join diagnostics and the log lines (ae_join_diag.h).
*/

#include <stdio.h>

#include "cseries.h"
#include "networking/network_messages.h"
#include "halo_port_limits.h"
#include "ae_join_diag.h"
#include "ae_join_diag_rules.h"

unsigned long system_milliseconds(void);

static struct ae_join_diag_machine ae_join_diag_machines[HALO_PORT_MAXIMUM_NETWORK_MACHINES];

static struct ae_join_diag_machine *slot(long machine_index)
{
	if (machine_index < 0 || machine_index >= HALO_PORT_MAXIMUM_NETWORK_MACHINES)
		return NULL;
	return &ae_join_diag_machines[machine_index];
}

void ae_join_diag_machine_joined(long machine_index)
{
	struct ae_join_diag_machine *machine = slot(machine_index);

	if (machine)
		ae_join_diag_rules_reset(machine);
}

void ae_join_diag_datagram(long machine_index, unsigned short port)
{
	struct ae_join_diag_machine *machine = slot(machine_index);

	if (machine)
		ae_join_diag_rules_datagram(machine, system_milliseconds(), port);
}

void ae_join_diag_stream(long machine_index)
{
	struct ae_join_diag_machine *machine = slot(machine_index);

	if (machine)
		ae_join_diag_rules_stream(machine, system_milliseconds());
}

void ae_join_diag_timed_out(long machine_index)
{
	struct ae_join_diag_machine *machine = slot(machine_index);
	char heard[160];

	if (!machine)
		return;
	ae_join_diag_rules_heard_text(machine, system_milliseconds(), heard, (long)sizeof(heard));
	network_event("join-diag: machine #%ld timed out; heard since it joined: %s", machine_index, heard);
}

void ae_join_diag_late_begin(long machine_index)
{
	struct ae_join_diag_machine *machine = slot(machine_index);

	if (machine)
		ae_join_diag_rules_late_begin(machine);
}

void ae_join_diag_late_end(long machine_index)
{
	struct ae_join_diag_machine *machine = slot(machine_index);

	if (machine)
		ae_join_diag_rules_late_end(machine);
}

int ae_join_diag_sent(long machine_index, void const *message, long size, int ok)
{
	struct ae_join_diag_machine *machine = slot(machine_index);
	char text[200];

	if (machine && machine->late_active && message && size > 0 &&
		ae_join_diag_rules_late_send(machine, machine_index, ((unsigned char const *)message)[size - 1], size, ok, text,
			(long)sizeof(text)) > 0)
	{
		network_event("%s", text);
	}
	return ok;
}

void ae_join_diag_stream_closed(long machine_index, int joined, int loaded, int started_late)
{
	struct ae_join_diag_machine *machine = slot(machine_index);
	char heard[160];

	if (!machine || !ae_join_diag_rules_closed_say(machine))
		return;
	ae_join_diag_rules_heard_text(machine, system_milliseconds(), heard, (long)sizeof(heard));
	network_event("join-diag: machine #%ld's stream is closed (the other side closed or reset it, or a send to it failed): "
		"joined %d, level loaded %d, started late %d; heard since it joined: %s", machine_index, joined, loaded,
		started_late, heard);
}
