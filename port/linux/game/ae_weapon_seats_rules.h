/*
AE_WEAPON_SEATS_RULES.H

Which seats of the map's player decide whether a weapon pack's weapon can be held by its label (ae_weapon_pack.c's
every-seat check): the seats of the animation graph that hold weapons, each needing a weapon type of the label, but
the stock player's ALERT seat, a stance only actors take, never the player. Without that the pistol's label (hp), which
the alert seat lacks (it has the rifle's and the shotgun's only), was refused for every pack and a pistol was held by
the rifle's two-handed label. Pure: no engine headers (port/linux/tests/ae_weapon_seats_rules_test.c).
*/

#ifndef AE_WEAPON_SEATS_RULES_H
#define AE_WEAPON_SEATS_RULES_H

/* whether a seat takes part in the check (its name; the alert seat does not) */
int ae_weapon_seats_counted(char const *name);
/* the seats of a graph against one label: seat i is named names[i], holds[i] is nonzero when one of its weapon types has
a label (it holds weapons at all), has[i] when one of them has the weapon's label. How many counted seats lack the label
(a seat that holds no weapons and has not the label is not counted: nothing is held there); *holding is how many seats
were counted, *first the index of the first that lacks it (-1 for none) */
short ae_weapon_seats_lacking(short count, char const *const *names, unsigned char const *holds,
	unsigned char const *has, short *holding, short *first);

#endif
