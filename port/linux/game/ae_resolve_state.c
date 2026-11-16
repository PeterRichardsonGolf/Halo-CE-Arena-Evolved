/* ae_resolve_state.c: see ae_resolve_state.h */
#include <string.h>

#include "ae_resolve_state.h"

void ae_resolve_state_init(struct ae_resolve_state *state)
{
	memset(state, 0, sizeof(*state));
}

unsigned long ae_resolve_state_begin(struct ae_resolve_state *state)
{
	state->ticket++;
	state->active = 1;
	state->done = 0;
	state->address = 0;
	return state->ticket;
}

void ae_resolve_state_finish(struct ae_resolve_state *state, unsigned long ticket, unsigned long address)
{
	if (!state->active || state->done || ticket != state->ticket)
		return;
	state->done = 1;
	state->address = address;
}

int ae_resolve_state_poll(struct ae_resolve_state *state, unsigned long *address)
{
	if (!state->active)
		return -1;
	if (!state->done)
		return 0;
	if (address)
		*address = state->address;
	state->active = 0;
	state->done = 0;
	return 1;
}

void ae_resolve_state_cancel(struct ae_resolve_state *state)
{
	state->ticket++;
	state->active = 0;
	state->done = 0;
	state->address = 0;
}
