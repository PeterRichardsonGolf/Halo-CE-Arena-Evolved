/* MCC cache address reservations. A file can request a free 64 MiB window;
 * MAP_FIXED_NOREPLACE never replaces an Xbox, CE or host allocation. */
#include "platform.h"
#include "ae_mcc_memory.h"
#include <sys/mman.h>
#include <stdint.h>
#include <errno.h>
#include <string.h>

/* (Arena Evolved) GulchCE's Android host reservation (host_mcc_memory_*), which AE's Android builds do not have:
 * never defined here; android64 takes the 64-bit builds' window (HALO_64BIT), the 32-bit guest a plain mapping */
#ifdef HALO_GULCHCE_ANDROID_HOST
#include "guest_host.h"
#endif

/* (Arena Evolved) the 32-bit Windows build's: the 64-bit one (windows64) keeps the window in the Xbox address space as
 * the other 64-bit builds do (HALO_64BIT below; win32_posix.c's mmap commits inside its reservation) */
#if defined(HALO_WINDOWS) && !defined(HALO_64BIT)
/* Keep the linked MCC window out of the Windows heap/driver allocator's
 * way before starting the renderer. Reserve address space, not committed
 * pages; unloading decommits the pages while retaining our reservation. */
static void *mcc_window;
static int mcc_window_active;

static void *mcc_window_hold(uint32_t address)
{
    void *wanted = (void *)(uintptr_t)address;
    void *allocation = mmap(wanted, 0x04000000u, PROT_NONE,
        MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE | MAP_FIXED_NOREPLACE, -1, 0);
    if (allocation == wanted) return allocation;
    if (allocation != MAP_FAILED) {
        munmap(allocation, 0x04000000u);
        errno = EINVAL;
    }
    return NULL;
}

__attribute__((constructor(103)))
static void mcc_window_initialize(void)
{
    mcc_window = mcc_window_hold(0x50000000u);
    if (!mcc_window)
        platform_log("mcc memory: early reservation at 50000000 failed (%s); will retry at map load", strerror(errno));
}

__attribute__((destructor))
static void mcc_window_dispose(void)
{
    if (mcc_window) munmap(mcc_window, 0x04000000u);
}
#endif

void *mcc_memory_reserve(uint32_t address, uint32_t size)
{
#ifdef HALO_GULCHCE_ANDROID_HOST
    long long result = host_mcc_memory_reserve(address, size);
    if (result < 0) {
        errno = (int)-result;
        return NULL;
    }
    return (void *)(uintptr_t)result;
#else
    void *wanted = (void *)(uintptr_t)address;
    void *allocation;
    /* Keep clear of the Xbox arena, Custom Edition window and low addresses.
     * MCC Tool's current base is 0x50000000; other non-overlapping linked
     * addresses are accepted only if the OS confirms the window is free. */
    if ((address & 0xFFFFu) || size != 0x04000000u ||
        address < 0x42000000u || address > 0x70000000u) {
        errno = EINVAL;
        return NULL;
    }
#if defined(HALO_WINDOWS) && !defined(HALO_64BIT)
    if (mcc_window_active) {
        errno = EBUSY;
        return NULL;
    }
    /* Other accepted linked bases retain their previous support. Only our
     * own inactive reservation can be released or recommitted. */
    if (mcc_window && mcc_window != wanted) {
        if (munmap(mcc_window, size)) return NULL;
        mcc_window = NULL;
    }
    if (!mcc_window) mcc_window = mcc_window_hold(address);
    if (!mcc_window) return NULL;
    allocation = mmap(mcc_window, size, PROT_READ | PROT_WRITE,
        MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE | MAP_FIXED, -1, 0);
    if (allocation == mcc_window) {
        mcc_window_active = 1;
        return allocation;
    }
#elif defined(HALO_64BIT)
    /* (Arena Evolved) the 64-bit builds' Xbox address space (cseries/xbox_address.h): the tags' addresses are Xbox
     * addresses there, at the window the Xbox heap leaves free (ae_mcc_memory.h); its pages mapped afresh */
    if (address != AE_MCC_WINDOW_BASE) {
        errno = EINVAL;
        return NULL;
    }
    wanted = xbox_pointer(address);
    allocation = mmap(wanted, size, PROT_READ | PROT_WRITE,
        MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE | MAP_FIXED, -1, 0);
    if (allocation == wanted)
        return allocation;
    return NULL;
#else
    allocation = mmap(wanted, size, PROT_READ | PROT_WRITE,
        MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE | MAP_FIXED_NOREPLACE, -1, 0);
    if (allocation == wanted)
        return allocation;
#endif
    if (allocation != MAP_FAILED) {
        munmap(allocation, size);
        errno = EINVAL;
    }
    return NULL;
#endif
}

void mcc_memory_release(void *allocation, uint32_t size)
{
#ifdef HALO_GULCHCE_ANDROID_HOST
    if (allocation) {
        int error = host_mcc_memory_release((unsigned int)(uintptr_t)allocation, size);
        if (error)
            platform_log("mcc memory: could not decommit tag window (%s)", strerror(-error));
    }
#elif defined(HALO_WINDOWS) && !defined(HALO_64BIT)
    if (allocation && allocation == mcc_window && size == 0x04000000u && mcc_window_active) {
        if (mmap(allocation, size, PROT_NONE,
            MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE | MAP_FIXED, -1, 0) == allocation)
            mcc_window_active = 0;
        else
            platform_log("mcc memory: could not decommit tag window (%s)", strerror(errno));
    }
#elif defined(HALO_64BIT)
    /* (the window stays the Xbox address space's, its pages let go: ae_mcc_memory.h) */
    if (allocation && mmap(allocation, size, PROT_READ | PROT_WRITE,
        MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE | MAP_FIXED, -1, 0) != allocation)
        platform_log("mcc memory: could not release tag window (%s)", strerror(errno));
#else
    if (allocation)
        munmap(allocation, size);
#endif
}
