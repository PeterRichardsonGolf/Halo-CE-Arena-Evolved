/*
AE_CROUCH_AIR.H

A crouched jump keeps the weapon at the ready (ae_crouch_air_rules.h): a crouch seat's airborne / land clip that is not
its weapon class's own (or none) is the stand seat's clip of the same class.
*/

#ifndef AE_CROUCH_AIR_H
#define AE_CROUCH_AIR_H

struct animation_graph;
struct animation_graph_unit_seat;
struct animation_graph_weapon_class;

/* units.c, unit_animation_set_state: the clip for a weapon class's animation (its index in the class), corrected */
short ae_crouch_air_animation(struct animation_graph *graph, struct animation_graph_unit_seat *seat,
	struct animation_graph_weapon_class *weapon_class, short class_animation, short animation_index);

#endif
