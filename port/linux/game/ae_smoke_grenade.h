/*
AE_SMOKE_GRENADE.H
THE AE SMOKE GRENADE's tag and rule (ae_smoke_rules.h, ae_smoke_grenade.c). Included after cseries.h.
*/

#ifndef __AE_SMOKE_GRENADE_H
#define __AE_SMOKE_GRENADE_H

struct game_variant;

/* whether a game of this variant plays with the smoke: SEARCH AND DESTROY (the classes' grenade slot), or the test
override game.smoke_grenade. Stock and Custom Edition gametypes never */
boolean ae_smoke_asked(struct game_variant const *variant);
/* weapons_new_map: the map's plasma grenade projectile found and its own numbers kept */
void ae_smoke_grenade_new_map(void);
/* game_engine_update, every machine: the grenade made the smoke (its detonation the pack's cloud, no damage, a frag's
model, nothing glowing) while the running game asks for it and the pack is on the map, the map's own back otherwise */
void ae_smoke_grenade_update(void);
/* whether this projectile definition is the smoke grenade now */
boolean ae_smoke_grenade_is_smoke(long projectile_definition_index);

#endif
