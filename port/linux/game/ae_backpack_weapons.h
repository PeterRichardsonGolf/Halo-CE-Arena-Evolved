/*
AE_BACKPACK_WEAPONS.H

BACKPACK WEAPONS (ae_backpack_weapons.c): what the AE hook line in
render_objects.c calls (port/linux/game/ae_hooks.txt lists it). Included after
cseries.h.
*/

#ifndef __AE_BACKPACK_WEAPONS_H
#define __AE_BACKPACK_WEAPONS_H

struct render_lighting;
struct render_model_effect;

/* after a biped's body is drawn (and its shadow): the weapons it carries but
does not hold, drawn on its back and thigh with the body's lighting, model
effect (NULL: the shadow pass) and render_model flags */
void ae_backpack_weapons_render(long unit_index, struct render_lighting const *lighting,
	struct render_model_effect const *model_effect, unsigned long flags);

#endif // __AE_BACKPACK_WEAPONS_H
