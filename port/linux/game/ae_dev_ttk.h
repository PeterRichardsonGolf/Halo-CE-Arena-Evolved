/*
AE_DEV_TTK.H

DEV MODE's ttk command: while it is on, every kill is answered and logged with how long the killer took, from their first
damage to the victim in the victim's life (shield or body) to the death:
"ttk: <killer> -> <victim> <weapon> <seconds>s, <hits> hits (<head> head)". The rules are ae_dev_ttk_rules.h.
*/

#ifndef __AE_DEV_TTK_H
#define __AE_DEV_TTK_H

#include "cseries.h"

/* the ttk command's state */
void ae_dev_ttk_set(boolean on);
boolean ae_dev_ttk_is_on(void);
/* a new game: nothing counted yet */
void ae_dev_ttk_new_game(void);

/* (damage.c's object_damage_aftermath's caller, once for each object a damage reaches) the damage a player (or none:
owner_player_index NONE) did to a unit with a player (victim_player_index; NONE for any other object): the damage
effect's tag, whether it was a headshot, the damage's amount, whether the blow killed. `weapon_damage` is a bullet or
projectile (the killer's weapon in hand is named), not a melee blow or an explosion (the damage's own tag is) */
void ae_dev_ttk_damage(long owner_player_index, long victim_player_index, long definition_index, boolean headshot,
	float amount, boolean killed, boolean weapon_damage);

#endif
