/*
AE_DELTA_WIRE.H

Arena Evolved on a Delta wire of its own, chupa-36z (delta.h's DELTA_WIRE:
OpenCE's network 36), with a range from ChupathingyCE 0.8's chupa-32a
(AE_DELTA_WIRE_OLDEST) to it (ae_delta_wire.c). So an Arena Evolved host
advertises its range as chupa-32a to chupa-36z and its old slot chupa-32a:
ChupathingyCE 0.8's clients join it, and the host keeps those whose HELLO
says a network version older than 35 out (ae_chupa_link.c: they can't read
its game settings record); ChupathingyCE 0.7.5 (chupa-29a) never joins. An
Arena Evolved client joins a Delta host whose range has chupa-36z, or whose
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

/* an advertisement delta_compat_advertise wrote for wire (DELTA_WIRE) at
major, its range set to AE_DELTA_WIRE_OLDEST to the wire: its old slot the
oldest wire, its range's oldest that, the range's check byte made again.
Nothing when the advertisement has no range bytes of the wire. */
void ae_delta_wire_narrow(unsigned char *reserved, int size, int major, const char *wire);

/* what this build (wire own, at major) decides of a host's advertisement,
from delta_compat_decide's decided (1 join, 0 refuse, -1 not said) and
theirs (its wire as text, when refused): 1 only for a host whose range
has own, or whose wire is AE_DELTA_WIRE_OLDEST (without range bytes: whose
slot is either); 0 for another Delta host (theirs set to its wire); -1 as
delta_compat_decide says */
int ae_delta_wire_decide(int decided, const unsigned char *reserved, int size, int major, const char *own,
	char *theirs, int theirs_size);

#endif
