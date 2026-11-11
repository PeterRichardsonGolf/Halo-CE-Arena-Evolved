/*
AE_BOMB_PACK.H

SEARCH AND DESTROY's bomb model (plan notes/plans/2026-10-28-ae-snd-bomb.md, step 8): a pack of tags only,
packs\ae_bomb.aepack, holds the bomb's model (the owner's E1-S: three rocket cases strapped together, a timer console
and its wires), with its shaders and bitmaps. ae_weapon_pack.c reads it at start and adds it to a multiplayer map
when the game about to be played is AE SND; while that game shows SEARCH AND DESTROY (ae_bomb_snd_shown) the map's
oddball ball, which is the bomb, is drawn with the pack's model: carried on the back (ae_backpack_weapons.c), loose on
the ground and planted. Its physics, pickup and everything sent over the network stay the ball's. A machine without
the pack sees the ball. Included after cseries.h.
*/

#ifndef __AE_BOMB_PACK_H
#define __AE_BOMB_PACK_H

#define AE_BOMB_PACK_ID "ae_bomb"

/* each tick, every machine: the ball drawn with the pack's model while SEARCH AND DESTROY is shown, its own otherwise */
void ae_bomb_pack_update(void);
/* the pack let go from the map (ae_weapon_pack.c, before the map's tags are): the ball's own model back */
void ae_bomb_pack_let_go(void);
/* every machine's, when SEARCH AND DESTROY's planted bomb goes off (game_engine_bomb.c, in place of Chupa's ring of
grenade explosions): the pack's blast at the bomb (debug.bomb_blast_look), without the pack the frag's; the host's
area damage about it. FALSE (nothing done) when the game is not SEARCH AND DESTROY */
boolean ae_bomb_pack_blast(union real_point3d const *at, long scorer_index);

#endif
