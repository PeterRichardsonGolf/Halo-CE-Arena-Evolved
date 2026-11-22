/*
SYS/STAT.H

The C runtime's <sys/stat.h> with the names the Steam client (port/android/host/host_mcc_hash.c) takes of POSIX:
stat on its 64-bit-size structure (a map's size may pass 2 GB), and S_ISDIR.
*/

#ifndef __HALO_WINDOWS_STEAM_SYS_STAT_H
#define __HALO_WINDOWS_STEAM_SYS_STAT_H

#include_next <sys/stat.h>

#define stat _stat64
#define S_ISDIR(mode) (((mode) & _S_IFMT) == _S_IFDIR)

#endif
