/*
AE_GAMETYPE_AUDIT.H

debug.arena_audit (HALO_ARENA_AUDIT): every seeded gametype's whole variant
and PC options in a TSV file, for tools/ae_gametype_audit.py. What the AE hook
line in source/saved games/playlist_profile.c calls (ae_hooks.txt lists it).
Included after cseries.h.
*/

#ifndef __AE_GAMETYPE_AUDIT_H
#define __AE_GAMETYPE_AUDIT_H

/* once, after the arena gametypes are seeded: with debug.arena_audit on,
writes the save root's z/arena_audit.tsv (one row per seeded gametype, as its
file has it, and one per stock builder) and logs where; off, nothing */
void ae_gametype_audit_write(void);

#endif
