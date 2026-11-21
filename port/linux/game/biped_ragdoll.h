/*
BIPED_RAGDOLL.H

A body falls as a ragdoll (biped_ragdoll.c, game.ragdoll_players and
game.ragdoll_ai), in a campaign or a multiplayer game.
*/

#ifndef BIPED_RAGDOLL_H
#define BIPED_RAGDOLL_H

/* a dead biped's tick (bipeds.c): TRUE when its ragdoll moves the body, in
place of its physics and its dying animation (FALSE where it is only drawn:
hosting a network game); it starts one for a biped that has just died in a
game (not during a cinematic), at once or, as its style has it, once its
death animation has played a while (FALSE until then) */
boolean biped_ragdoll_update(long biped_index);
/* whether this machine has a ragdoll for the body, which the host's word on
where it is doesn't move (network_objects.c) */
boolean biped_ragdoll_active(long object_index);
/* a blow's push on a body (damage.c), hardest nearest the epicenter, which
wakes it if it lay still: TRUE when its ragdoll moves the body, and takes the
push in place of the body's own physics */
boolean biped_ragdoll_accelerate(long biped_index, real_point3d const *epicenter,
	real_vector3d const *acceleration);
/* a body only drawn as a ragdoll (hosting a network game): its nodes as the
ragdoll lies, for the renderer (render_interpolation.c); NULL for any other
object */
real_matrix4x3 *biped_ragdoll_drawn_nodes(long object_index);
/* the blow that kills a unit (units.c), which pushes the ragdoll it falls as;
category: its damage's (MODERN pushes bullets and melee by it) */
void biped_ragdoll_note_killing_blow(long unit_index, real_point3d const *epicenter,
	real_vector3d const *direction, real instantaneous_acceleration, short category);
/* nothing of the last map's bodies (game.c) */
void biped_ragdoll_reset(void);

#endif
