/*
AE_VIEW_MODEL.H

The first-person weapon's own placement in the game (ae_view_model_rules.h has the numbers and the math): the node
matrices of the first-person weapon moved as its numbers say, where the first-person draw builds them, for each view
on its own. A weapon without numbers is drawn as its model is.
*/

#ifndef __AE_VIEW_MODEL_H
#define __AE_VIEW_MODEL_H

#include "cseries.h"
#include "math/real_math.h"
#include "ae_view_model_rules.h"

/* the weapon's first-person node matrices (count of them, the gun's root first) moved by its numbers, if it has
some (first_person_weapons.c: the model's, and the markers' copy of the animation's) */
void ae_view_model_apply(long weapon_index, struct real_matrix4x3 *nodes, short count);

/* a weapon definition's key: a pack weapon's name, else its tag's last name (the weapon HUD mover keys its numbers the
same way: ae_hud_offsets.h) */
void ae_view_model_key(long definition_index, char *key, int size);

/* DEV MODE's viewmodel command: the weapon in the first local player's hands, its key and numbers; 0 with no
weapon in hand */
int ae_view_model_held(long player_index, char *key, int key_size, struct ae_view_model *out);
/* sets a weapon's numbers (1, or 0 when they are refused), brings its shipped numbers back, keeps the table's own
numbers in the config folder's view_models.txt (1 when written) */
int ae_view_model_set(char const *key, struct ae_view_model const *model);
void ae_view_model_reset(char const *key);
int ae_view_model_save(void);

/* the arms and gun placed as one piece (the weapon's place numbers), where the first-person pose is built: every node of
the pose (count), turned and scaled about the pivot node (the gun's root), in the camera's frame; and, once after
ae_view_model_probe_request, where the gun's markers sit in the camera's frame before it, written to the log */
void ae_view_model_place(long weapon_index, struct real_matrix4x3 *nodes, short count, short pivot_node,
	union real_point3d const *camera_position, union real_vector3d const *camera_forward,
	union real_vector3d const *camera_up);
/* DEV MODE's viewmodel probe: the next frame's held weapon's markers (and the pivot) in the camera's frame, logged */
void ae_view_model_probe_request(void);

#endif
