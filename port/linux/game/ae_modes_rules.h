/*
AE_MODES_RULES.H

Arena Evolved's modes of Slayer on OpenCE's gametypes (ae_modes.c), their
rules as pure functions (port/linux/tests/ae_modes_rules_test.c runs them).
The modes are SPCLib's ideas and numbers (Chalwk's SAPP scripts, MIT; no code
of them is here):

- ONE IN THE CHAMBER: everyone has a pistol with one round and no more; a
  player's bullet kills at once; each kill gives the killer a round (into the
  magazine while it has room, else carried).
- FRAG NATION: fists, two frag and two plasma grenades; a grenade kill gives
  the killer grenades back: a frag's explosion a frag, a plasma's explosion a
  plasma, a stuck plasma two plasmas, each kind up to what a unit can carry.
*/

#ifndef __AE_MODES_RULES_H
#define __AE_MODES_RULES_H

/* One in the Chamber: a round for a kill, a magazine's loaded and carried
rounds and its most loaded */
void ae_modes_rules_round_for_kill(short *loaded, short *carried, short loaded_maximum);

/* Frag Nation: the grenades a kill by a damage of this tag path gives (frag,
plasma; none for anything else) */
void ae_modes_rules_grenades_for_kill(char const *damage_tag_path, short *frags, short *plasmas);

/* a count given more, up to a maximum (a count already past it is kept) */
short ae_modes_rules_add_capped(short count, short add, short maximum);

#endif // __AE_MODES_RULES_H
