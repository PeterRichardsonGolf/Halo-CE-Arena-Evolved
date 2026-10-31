/*
AE_MCC_ADDRESS.H

(Arena Evolved) an MCC map's addresses in every build. The tags' pointers are 32-bit Xbox addresses: in the 32-bit
builds those are the host's pointers (GulchCE's MCC code reads and writes them as such), in the 64-bit builds
they are addresses in the Xbox address space (cseries/xbox_address.h) and are converted. The MCC code goes through
these two for every pointer it stores into, or reads out of, tag data.
*/

#ifndef __AE_MCC_ADDRESS_H
#define __AE_MCC_ADDRESS_H

#include <stdint.h>
#if defined(HALO_64BIT) || defined(HALO_WINDOW_REBASE)
#include "cseries/xbox_address.h"
#endif
/* (a build whose Xbox addresses are its pointers, and the MCC tests that build these files alone: the identity) */
#ifndef XBOX_ADDRESS
#define XPTR(type) type *
#define XBOX_NULL NULL
#define xbox_pointer(address) ((void *)(uintptr_t)(address))
#define XBOX_POINTER(type, address) ((type *)xbox_pointer(address))
#define XBOX_ADDRESS(pointer) ((void *)(pointer))
#endif

/* a host pointer as the 32-bit address tag data holds (NULL is 0) */
static __inline__ uint32_t mcc_address(void const *pointer)
{
	return (uint32_t)(uintptr_t)XBOX_ADDRESS(pointer);
}

/* a 32-bit address read out of tag data as a uint32_t, as a host pointer (0 is NULL; an XPTR field of a
structure goes through xbox_pointer, which takes both kinds) */
static __inline__ void *mcc_host(uint32_t address)
{
	return xbox_pointer(address);
}

/* (the 64-bit builds) MCC's buffers that tag data points at (vertex and index payloads, generated pause widgets,
their art, the copied tag table) are allocated in the Xbox heap, inside the Xbox address space; a file whose every
allocation is such a buffer maps malloc and free onto these (MCC_XBOX_ALLOCATIONS) */
#ifdef HALO_64BIT
#include <stddef.h>
void *xbox_heap_allocate(size_t size, int zero);
void xbox_heap_free(void *pointer);
#define mcc_xbox_allocate(size) xbox_heap_allocate((size_t)(size), 0)
#define mcc_xbox_free(pointer) xbox_heap_free(pointer)
#ifdef MCC_XBOX_ALLOCATIONS
#include <stdlib.h>
#define malloc(size) mcc_xbox_allocate(size)
#define free(pointer) mcc_xbox_free(pointer)
#endif
#else
#define mcc_xbox_allocate(size) malloc(size)
#define mcc_xbox_free(pointer) free(pointer)
#endif

#endif
