/*
AE_SHRINK_WEAPONS.H

SHRINK EMPTY WEAPONS (display.shrink_empty_weapons, Settings > HUD): a
weapon lying on the ground with no ammo (no rounds loaded or in reserve in
any magazine; an energy weapon's battery spent) is drawn at 60% of its size,
about its middle, so it reads as empty from afar. Drawn only: the weapon,
its pickup and its physics are the game's. An idea of Chimera's, written
here from its description only.
*/

#ifndef AE_SHRINK_WEAPONS_H
#define AE_SHRINK_WEAPONS_H

/* (render_objects.c, an object's node matrices for drawing: an AE hook line) the matrices given, or a copy drawn
smaller for an empty weapon on the ground (kept to the end of the frame's drawing) */
struct real_matrix4x3 const *ae_shrink_empty_weapon(long object_index, struct real_matrix4x3 const *nodes);
/* whether a weapon is empty: magazines (count of them, each loaded + in reserve) or, with none, its battery's age
(1 spent) (pure) */
int ae_weapon_is_empty(int magazine_count, const int *rounds, float age);

#endif
