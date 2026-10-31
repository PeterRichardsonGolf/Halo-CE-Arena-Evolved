/*
AE_DELTA_WIRE.H

Arena Evolved on a Delta wire of its own, chupa-38z (delta.h's DELTA_WIRE:
OpenCE's network 38), with a range from ChupathingyCE 0.8's chupa-32a
(AE_DELTA_WIRE_OLDEST) to it (ae_delta_wire.c). So an Arena Evolved host
advertises its range as chupa-32a to chupa-38z and its old slot chupa-32a:
ChupathingyCE 0.8's clients join it, and the host keeps those whose HELLO
says a network version older than 35 out (ae_chupa_link.c: they can't read
its game settings record); ChupathingyCE 0.7.5 (chupa-29a) never joins. An
Arena Evolved client joins a Delta host whose range has chupa-38z, or whose
wire is chupa-32a (ChupathingyCE 0.8.0f, which plays by the host's version,
and older 0.8 builds), whose game it plays by the number the host announces
(ae_chupa_link.c). Pure C (delta_compat.h only), so the unit tests build it
(port/linux/tests/ae_delta_wire_test.c).
*/

#ifndef AE_DELTA_WIRE_H
#define AE_DELTA_WIRE_H

/* the oldest wire Arena Evolved plays: ChupathingyCE 0.8's (network 32,
and OpenCE's 33 to 36 by the host's number from 0.8.0f) */
#define AE_DELTA_WIRE_OLDEST "chupa-32a"
/* (0.3.9) ChupathingyCE 0.8.0h's wire: OpenCE's 33 to 38 by the number it announces (its table's), its range from
chupa-28a to it, below this build's chupa-38z: joined as AE_DELTA_WIRE_OLDEST is (crossplay test 2026-10-24) */
#define AE_DELTA_WIRE_CHUPA_38 "chupa-38a"

/* an advertisement delta_compat_advertise wrote for wire (DELTA_WIRE) at
major, its range set to AE_DELTA_WIRE_OLDEST to the wire: its old slot the
oldest wire, its range's oldest that, the range's check byte made again.
Nothing when the advertisement has no range bytes of the wire. */
void ae_delta_wire_narrow(unsigned char *reserved, int size, int major, const char *wire);

/* what this build (wire own, at major) decides of a host's advertisement,
from delta_compat_decide's decided (1 join, 0 refuse, -1 not said) and
theirs (its wire as text, when refused): 1 only for a host whose range
has own, or whose wire is AE_DELTA_WIRE_OLDEST or AE_DELTA_WIRE_CHUPA_38
(without range bytes: whose slot is one of them); 0 for another Delta host (theirs set to its wire); -1 as
delta_compat_decide says */
int ae_delta_wire_decide(int decided, const unsigned char *reserved, int size, int major, const char *own,
	char *theirs, int theirs_size);

/* whether an advertisement's reserved bytes are a Delta host's: DELTA_ADVERTISED_FLAG and, after it, a major and
a wire's name. OpenCE's network 39 sets the same bit (0x04) for a dedicated server and leaves the wire bytes zero,
so the flag alone is either (0.5.0: ChupathingyCE main's tagged advertisement, which ends the clash, comes with
its merge) */
int ae_delta_wire_advertised(const unsigned char *reserved, int size);

#endif
