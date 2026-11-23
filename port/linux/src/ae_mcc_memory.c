/*
AE_MCC_MEMORY.C

(Arena Evolved) the Xbox heap's gap for MCC maps' tag window (ae_mcc_memory.h).
*/

#include "ae_mcc_memory.h"

#include "platform.h"

#ifdef HALO_64BIT
unsigned char *ae_mcc_heap_skip(unsigned char *next, size_t size)
{
	unsigned char *window = xbox_pointer(AE_MCC_WINDOW_BASE);
	unsigned char *window_end = window + AE_MCC_WINDOW_SIZE;

	/* (the window's bytes stay the heap's mapping, unused by it: mcc_memory.c maps them afresh for a map) */
	if (next < window_end && next + size > window)
		return window_end;
	return next;
}
#endif
