/*
AE_RESOLVE_STATE.H

The bookkeeping of a name lookup that runs on a thread of its own (src/ae_resolve.c; JOIN BY ADDRESS): one lookup at
a time, each with a ticket, so a lookup that was cancelled (or replaced) and answers late is dropped. Pure, no
threads, no locks: the caller holds its own lock around each call (unit test: port/linux/tests/ae_resolve_state_test.c).
*/

#ifndef __AE_RESOLVE_STATE_H
#define __AE_RESOLVE_STATE_H

struct ae_resolve_state
{
	unsigned long ticket;        /* the lookup now (or last) begun */
	int active;                  /* a lookup is under way or its answer not yet taken */
	int done;
	unsigned long address;       /* the answer (0: not found) */
};

void ae_resolve_state_init(struct ae_resolve_state *state);
/* a lookup begins (one already under way is dropped): its ticket */
unsigned long ae_resolve_state_begin(struct ae_resolve_state *state);
/* the lookup of ticket answers (from its thread): kept only when it is still the lookup now */
void ae_resolve_state_finish(struct ae_resolve_state *state, unsigned long ticket, unsigned long address);
/* -1 no lookup, 0 still going, 1 answered (address written; the lookup is over) */
int ae_resolve_state_poll(struct ae_resolve_state *state, unsigned long *address);
/* the lookup is dropped: its answer, whenever it comes, is not kept */
void ae_resolve_state_cancel(struct ae_resolve_state *state);

#endif
