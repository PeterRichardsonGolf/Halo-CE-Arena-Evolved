/*
AE_CHUPA_LINK.H

Arena Evolved and ChupathingyCE 0.8 in one Delta game: the game's side of
ae_chupa_link_rules.h (one-line AE hooks call these).
*/

#ifndef AE_CHUPA_LINK_H
#define AE_CHUPA_LINK_H

#include "cseries.h"

/* (a client) whether a game settings record piece's total size is one the
host sends (its version's; not known, either: a record of the older size
then marks the host as of 32) */
boolean ae_chupa_link_record_size_taken(unsigned long total_size);
/* (a client) a whole record of total_size, as the wire has it: an older one
made 35's in place (the buffer holds 35's), OpenCE's further options at
their defaults */
void ae_chupa_link_record_arrived(byte *record, unsigned long total_size);
/* whether the rules of the network version are played: a client by its
host's number (not known: this build's own), a host or a game of its own
by the number it announces */
boolean ae_chupa_link_plays_version(unsigned int version);
/* ... of 34: shield-only scenery and devices are static targets, and a
static target's own damage the host's to deal (network_damage.c) */
boolean ae_chupa_link_static_shields(void);
/* (the host, each frame of its Delta Peer) a Delta machine whose HELLO says a
version too old for this host's game settings record dropped, with a
notice (ae_chupa_link_host_keeps) */
void ae_chupa_link_host_frame(void);

#endif
