/*
AE_DELTA_WIRE.H

Arena Evolved on a Delta wire of its own, chupa-36z (delta.h's DELTA_WIRE:
OpenCE's network 36, which no ChupathingyCE release plays yet), with a range
of that wire alone (ae_delta_wire.c). So an Arena Evolved host advertises its
range as chupa-36z to chupa-36z and its old slot chupa-36z (ChupathingyCE
0.8's clients, chupa-32a, and 0.7.5's exact slot match never join), and an
Arena Evolved client joins a Delta host only when the host's range has
chupa-36z. (0.3.6 to 0.3.8 played ChupathingyCE 0.8's chupa-32a, and 0.3.5
its own ae-32a: network 32, which this build does not play.) Pure C
(delta_compat.h only), so the unit tests build it
(port/linux/tests/ae_delta_wire_test.c).
*/

#ifndef AE_DELTA_WIRE_H
#define AE_DELTA_WIRE_H

/* an advertisement delta_compat_advertise wrote for wire (DELTA_WIRE) at
major, narrowed to the wire alone: its old slot the wire itself, its range's
oldest the wire, the range's check byte made again. Nothing when the
advertisement has no range bytes of the wire. */
void ae_delta_wire_narrow(unsigned char *reserved, int size, int major, const char *wire);

/* what this build (wire own, at major) decides of a host's advertisement,
from delta_compat_decide's decided (1 join, 0 refuse, -1 not said) and
theirs (its wire as text, when refused): 1 only for a host whose range
has own (or, without range bytes, whose slot is own); 0 for another Delta host (theirs set to its wire);
-1 as delta_compat_decide says */
int ae_delta_wire_decide(int decided, const unsigned char *reserved, int size, int major, const char *own,
	char *theirs, int theirs_size);

#endif
