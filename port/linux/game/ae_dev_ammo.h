/*
AE_DEV_AMMO.H

DEV MODE's INFINITE AMMO and BOTTOMLESS CLIP (owner 2026-11-11: "dev mode weapons had unlimited ammo and bottomless
clip option"): SETTINGS > SYSTEM > DEV's two rows (system.dev_infinite_ammo, system.dev_bottomless_clip). INFINITE AMMO:
a reload takes nothing from the reserve and an energy weapon never ages; BOTTOMLESS CLIP: a shot takes nothing from the
magazine and adds no heat. The rule (DEV MODE on, a player's weapon, never a bot's): ae_dev_ammo_rules.h. Read live,
so a TEST RUN set-up line "config system.dev_infinite_ammo true" turns it on for the run.
*/

#ifndef AE_DEV_AMMO_H
#define AE_DEV_AMMO_H

/* weapons.c: whether this weapon's reserve (magazine) is DEV MODE's infinite (bottomless) one */
int ae_dev_ammo_infinite(long weapon_index);
int ae_dev_ammo_bottomless(long weapon_index);

#endif
