/*
AE_DELTA_WIRE.H

Arena Evolved on ChupathingyCE 0.8's Delta wire, chupa-32a, with a range of
that wire alone (ae_delta_wire.c). ChupathingyCE's own chupa-32a hosts play
with 28 to 31 too (delta.h's DELTA_WIRE_COMPATIBILITY, its 30 to 32 being
additive); Arena Evolved's network 30 to 32 are OpenCE build-242/244's,
which do not play with 29's machines. So an Arena Evolved host advertises
its range as chupa-32a to chupa-32a, its old slot chupa-32a (released
ChupathingyCE clients compare the slot for an exact match: 0.7.5's
chupa-29a never joins), and an Arena Evolved client joins a Delta host only
when the host plays chupa-32a (its range has it), or when the host is
Arena Evolved 0.3.5 (its own wire ae-32a, OpenCE build-244's network 32 with
Delta, as this one's). Pure C (delta_compat.h only), so the unit tests build
it (port/linux/tests/ae_delta_wire_test.c).
*/

#ifndef AE_DELTA_WIRE_H
#define AE_DELTA_WIRE_H

/* Arena Evolved 0.3.5's wire, joined as this build's own */
#define AE_DELTA_WIRE_0_3_5 "ae-32a"

/* an advertisement delta_compat_advertise wrote for wire (DELTA_WIRE) at
major, narrowed to the wire alone: its old slot the wire itself, its range's
oldest the wire, the range's check byte made again. Nothing when the
advertisement has no range bytes of the wire. */
void ae_delta_wire_narrow(unsigned char *reserved, int size, int major, const char *wire);

/* what this build (wire own, at major) decides of a host's advertisement,
from delta_compat_decide's decided (1 join, 0 refuse, -1 not said) and
theirs (its wire as text, when refused): 1 only for a host whose range
has own (or, without range bytes, whose slot is own), or whose slot is
AE_DELTA_WIRE_0_3_5; 0 for another Delta host (theirs set to its wire);
-1 as delta_compat_decide says */
int ae_delta_wire_decide(int decided, const unsigned char *reserved, int size, int major, const char *own,
	char *theirs, int theirs_size);

#endif
