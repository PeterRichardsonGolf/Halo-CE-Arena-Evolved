/*
AE_GRENADE_WIRE_RULES.H

AE's grenade message (_distributed_message_ae_grenades, 108: ae_grenades_network.c), an entry's bytes, as pure
rules (port/linux/tests/ae_grenade_wire_rules_test.c). From the host to the clients, reliably, only in a game whose
grenade mask is not 0 (a host with grenades refuses the machines that do not know the message: refusal 68). Every
entry is AE_GRENADE_WIRE_ENTRY_SIZE bytes, little-endian, whatever the machine:

  0     kind: 1 a player's unit (its extra grenades and its scramble), 2 a vehicle (its stall)
  1     the host's grenade type count (its slot map's: 2-10)
  2     the host's grenade mask
  3     0
  4-7   the object's full handle (salt and index)
  8-15  the unit's grenades of slots 2-9, 0-127 each (a vehicle: 0)
  16-17 the stall's ticks left (a unit: 0)
  18-19 the scramble's ticks left (a vehicle: 0)

A client takes an entry only when the host's type count and mask are its own (the same slot map).
*/

#ifndef __AE_GRENADE_WIRE_RULES_H
#define __AE_GRENADE_WIRE_RULES_H

#define AE_GRENADE_WIRE_ENTRY_SIZE 20

enum
{
	AE_GRENADE_WIRE_UNIT = 1,
	AE_GRENADE_WIRE_VEHICLE = 2
};

struct ae_grenade_wire_entry
{
	int kind;
	int type_count;
	unsigned mask;
	unsigned long handle;
	unsigned char counts[8];
	unsigned stall_ticks;
	unsigned scramble_ticks;
};

/* the entry's bytes (ticks past 65535 are 65535, counts past 127 are 127) */
void ae_grenade_wire_put(unsigned char bytes[AE_GRENADE_WIRE_ENTRY_SIZE], struct ae_grenade_wire_entry const *entry);
/* the entry the bytes hold: 1, or 0 for bytes no host writes (a kind not known, the reserved byte not 0, a count
past 127, a type count out of 2-10, a unit's stall or a vehicle's counts or scramble) */
int ae_grenade_wire_take(unsigned char const bytes[AE_GRENADE_WIRE_ENTRY_SIZE], struct ae_grenade_wire_entry *entry);

/* the entry's handle as the game holds one: the 32 bits sign-extended (a long of any width) */
long ae_grenade_wire_handle(unsigned long handle);
/* whether a client takes the entry: the host's slot map (type count and mask) is its own */
int ae_grenade_wire_slot_map_matches(struct ae_grenade_wire_entry const *entry, int type_count, unsigned mask);
/* whether a client takes the host's count of a slot over its current one: always for another machine's unit (own 0);
for its own player's unit, a lower count of the host's, or none of its own throws lowered it (changed -1), or its
throw is older than the round trip and 5 ticks (round_trip in ticks, 6 when not known, at most 60) */
int ae_grenade_wire_count_taken(int host_count, int current, int own, long changed, long now, float round_trip);
/* the ticks of an effect the host said, here: less half the round trip (0 when spent) */
long ae_grenade_wire_effect_ticks(unsigned ticks, float round_trip);

#endif
