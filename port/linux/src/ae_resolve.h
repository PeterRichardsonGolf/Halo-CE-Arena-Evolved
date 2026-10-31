/*
AE_RESOLVE.H

A host's name looked up on a thread of its own (JOIN BY ADDRESS: the system's resolver may wait seconds on a name
that is not there), one lookup at a time. The menus start it, poll it each frame and may cancel it; a cancelled
lookup's answer is dropped when it comes, and its thread frees what it holds itself.
*/

#ifndef __AE_RESOLVE_H
#define __AE_RESOLVE_H

/* starts the lookup of host (an earlier one is dropped); 1 started */
int ae_resolve_start(const char *host);
/* -1 none, 0 still looking, 1 answered: address (network byte order, 0 when the name was not found) */
int ae_resolve_poll(unsigned long *address);
/* drops the lookup now under way */
void ae_resolve_cancel(void);

#endif
