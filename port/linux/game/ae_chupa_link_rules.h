/*
AE_CHUPA_LINK_RULES.H

Arena Evolved (OpenCE's network 36) and ChupathingyCE 0.8 in one Delta game
(ae_delta_wire.h: the wires; ae_chupa_link.c: the game's side). The rules
that need no game, which the unit tests check
(port/linux/tests/ae_chupa_link_rules_test.c):

- An Arena Evolved client of a ChupathingyCE host plays by the number the host
  announces, as ChupathingyCE 0.8.0f does (its network_rules_of_version):
  ChupathingyCE's hosts announce 32, and send a 13,120-byte game settings
  record (the gametype's options 0x1C bytes) to every machine under 35. This
  build's record is 35's (0x2C bytes of options, 13,136 at the network's
  slots): the client takes the size of the host's number, and widens an older
  record before anything reads it, OpenCE's further options at their defaults
  (no mode: SWAT, Infection, Gun Game and Hardcore are off). Below 34 a
  shield-only scenery or device is no static target, and a client reports a
  static target's own damage (a tripmine going off) as its own hit.
- An Arena Evolved host announces 36 and sends 35's record to every machine.
  A Delta machine whose HELLO says a network version under 35 (ChupathingyCE
  0.8.0e and older, Arena Evolved 0.3.6 to 0.3.8) can't read it, and is
  dropped with a notice; ChupathingyCE 0.8.0f says 36.

Plain C on bytes, no game dependency.
*/

#ifndef AE_CHUPA_LINK_RULES_H
#define AE_CHUPA_LINK_RULES_H

/* the network versions that changed what a client does (docs/delta.md) */
#define AE_CHUPA_LINK_STATIC_SHIELDS_VERSION 34
#define AE_CHUPA_LINK_EXTENDED_OPTIONS_VERSION 35
/* (a host's version not known: a game joined by an invitation) */
#define AE_CHUPA_LINK_VERSION_UNKNOWN 0xFFFF
/* ... and the version taken for such a host whose record is the older one
(ChupathingyCE 0.8's hosts announce 32) */
#define AE_CHUPA_LINK_OLDER_RECORD_VERSION 32

/* the game settings record at the network's slots: 11 to 34, and 35 on (the
gametype's options 0x2C bytes, not 0x1C, in front of the record's last 4, its
local data) */
#define AE_CHUPA_LINK_SETTINGS_SIZE 13120
#define AE_CHUPA_LINK_EXTENDED_OPTIONS_SIZE 16
#define AE_CHUPA_LINK_SETTINGS_EXTENDED_SIZE (AE_CHUPA_LINK_SETTINGS_SIZE + AE_CHUPA_LINK_EXTENDED_OPTIONS_SIZE)
#define AE_CHUPA_LINK_LOCAL_DATA_SIZE 4

/* the size of the record a host of the version sends (0: not known, either) */
int ae_chupa_link_settings_size(unsigned version);
/* whether a record of total_size is one a host of the version sends */
int ae_chupa_link_settings_size_taken(unsigned version, unsigned long total_size);
/* a record of the older layout (AE_CHUPA_LINK_SETTINGS_SIZE bytes, at the
start of record, which holds AE_CHUPA_LINK_SETTINGS_EXTENDED_SIZE) made 35's in
place: its last 4 bytes moved 16 on, further (the 16 further option bytes)
in front of them */
void ae_chupa_link_settings_widen(unsigned char *record, unsigned char const *further);
/* whether the rules of the version are played: on a client, of the host's
version (not known: this build's own, announce), on a host its own */
int ae_chupa_link_rules_of_version(unsigned version, int client, unsigned host_version, unsigned announce);
/* (a host announcing announce) whether it keeps a Delta machine whose HELLO
said hello_version (0: none yet, or not Delta): one of 35's record or newer
reads this host's record; an older one can't, under a host of 35 or later */
int ae_chupa_link_host_keeps(unsigned announce, unsigned hello_version);

#endif
