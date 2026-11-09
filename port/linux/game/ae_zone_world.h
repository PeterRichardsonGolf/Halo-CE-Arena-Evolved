/*
AE_ZONE_WORLD.H

The round modes' danger zone drawn in the world: its edge as a violet wall (look 'a', the energy wall: a 10 wu band) or a
light curtain (look 'b': 5 wu, a light foot, ribs every 1.5 wu), and the next ring as a dashed ice-coloured line. Drawn in
the world pass (render.c, after the spawn markers: depth tested, alpha blended, under the HUD) through the debug
geometry's triangles, so no tag and no texture is needed; each look's pattern is made in its vertices' colours
(ae_round_zone_rules.h: ae_zone_wall_sample, unit tested). Display only, from ae_zone_view.h's state; nothing is networked.

The look: game.zone_look ("a" or "b", default "b"), or the DEV MODE command `zone look a|b` for this run.
*/
#ifndef AE_ZONE_WORLD_H
#define AE_ZONE_WORLD_H

/* the world pass's hook, per local player's view */
void ae_zone_world_draw(short local_player_index);
/* the DEV MODE command's look for this run ('a' or 'b'); 0 gives the setting back */
void ae_zone_world_set_look(int look);
/* the look now ('a' or 'b') */
int ae_zone_world_look(void);

#endif
