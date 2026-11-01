/*
AE_MCC_MEMORY.H

(Arena Evolved) where an MCC map's tags go in the 64-bit builds' Xbox address space (cseries/xbox_address.h): a
version-13 map's tag data points at its own linked base, 0x50000000 for the maps MCC's tools build, and is read
there (mcc_memory.c, GulchCE's). That base lies inside the Xbox heap's range (xbox_heap.c, 0x42000000 up), so the
heap leaves those 64 MB to it: an allocation that would cross them is placed past them.
*/

#ifndef __AE_MCC_MEMORY_H
#define __AE_MCC_MEMORY_H

#include <stddef.h>

/* the MCC tag window, in Xbox addresses (GulchCE's mcc_memory.c: a 64 MiB window at the map's linked base) */
#define AE_MCC_WINDOW_BASE 0x50000000U
#define AE_MCC_WINDOW_SIZE 0x04000000U

/* (xbox_heap.c's carve) where an allocation of size bytes at the heap's next byte goes: next, or the window's end
when it would overlap the window */
unsigned char *ae_mcc_heap_skip(unsigned char *next, size_t size);

#endif
