/*
AE_BACKPACK_WEAPONS.C

BACKPACK WEAPONS (View > BACKPACK WEAPONS, display.backpack_weapons): the
weapons a player carries but does not hold, drawn on their body: a rifle, a shotgun or a launcher slung across the back, a
sniper rifle upright on the back, a small one (a pistol, a needler, a plasma
rifle) in a holster on the right thigh (ae_backpack_rules.h says which).
Objective items (the flag, the ball) are never drawn, nor anything in a
vehicle seat, nor anything in your own first person.

Only the drawing is added. A holstered weapon stays where the game keeps it,
out of the map and unseen (units.c's unit_ready_desired_weapon); here its
model is drawn again, in its model's own resting pose, held by a node of the
body (the spine for the back, a thigh for the hip) as the skin on that node is
held, with the body's lighting and model effect (active camouflage hides it
with the body). Nothing is created, moved or sent: every machine has every
unit's inventory (network_objects.c), so a guest sees the same.

Where a weapon sits is a placement in the body's own space at rest (forward,
left, up): a point beside its node's resting place, and the ways its barrel
and its top point. They were tuned on the stock cyborg (2026-10-24); a body
without the nodes gets nothing drawn.

The idea is aLTis94's backpack_weapons.lua for Chimera (ideas only: it has no
licence, and no code of it is here).
*/

#include "cseries.h"
#include "cseries/errors.h"
#include "math/real_math.h"
#include "render/render.h"
#include "render/render_cameras_internal.h"
#include "objects/objects.h"
#include "objects/object_definitions.h"
#include "units/units.h"
#include "game/players.h"
#include "items/weapons.h"
#include "items/weapon_definitions.h"
#include "models/models.h"
#include "models/model_definitions.h"
#include "tag_files/tag_files.h"

#include "ae_backpack_rules.h"
#include "ae_backpack_weapons.h"
#include "ae_bomb_assault.h"

#include <string.h>

/* port/linux/src/ae_config_players.c: each player's own (split screen) */
int ae_config_player_boolean(int player, const char *name);
unsigned long ae_config_player_changes(void);

enum
{
	AE_BACKPACK_MAXIMUM_NODES = 64,
	/* render_objects.c's _render_model_shadow_bit, in the flags it passes */
	AE_BACKPACK_RENDER_MODEL_SHADOW_BIT = 1,
};

/* a weapon's place, in the body's space at rest: offset from its node's
resting place, and the directions of its barrel (its forward) and its top
(its up) */
struct ae_backpack_placement
{
	char const *name;
	char const *node;
	real_vector3d offset;
	real_vector3d barrel;
	real_vector3d top;
};

static struct ae_backpack_placement const ae_backpack_placements[AE_BACKPACK_PLACE_COUNT] =
{
	/* across the back, flat on the back plate on
	its side: 45 degrees, the stock by the right shoulder blade, the barrel
	down to the left hip, centred on the spine (owner 2026-10-24: not hanging
	off the side; tuned with the dev camera) */
	{ "back", "spine1", { -0.085f, -0.055f, 0.045f }, { 0.f, 0.707f, -0.707f }, { 0.f, -0.707f, -0.707f } },
	/* a second, crossed over it the other way */
	{ "back2", "spine1", { -0.1f, 0.055f, 0.045f }, { 0.f, -0.707f, -0.707f }, { 0.f, 0.707f, -0.707f } },
	/* a very long one steeper, on its side too, the stock by the left hip, the
	barrel up past the right of the head */
	{ "long", "spine1", { -0.085f, -0.04f, -0.1f }, { 0.f, -0.6f, 0.8f }, { 0.f, 0.8f, 0.6f } },
	/* in a holster on the right thigh, the barrel down, the weapon's top
	forward, its grip back */
	{ "thigh", "r thigh", { 0.f, -0.045f, -0.03f }, { 0.f, 0.f, -1.f }, { 1.f, 0.f, 0.f } },
	{ "thigh2", "l thigh", { 0.f, 0.045f, -0.03f }, { 0.f, 0.f, -1.f }, { 1.f, 0.f, 0.f } },
	/* SEARCH AND DESTROY's bomb (the bomb pack's model, ae_bomb_pack.h; its origin the middle of its bounds), upright
	and flush on the back, its top at the shoulder blades, leaning back 6 degrees, the timer console facing out: the
	owner's 2026-10-28 pick of the E1-S mockups (bomb-look/mock4), its place there measured against the cyborg's back */
	{ "bomb", "spine1", { -0.1376f, 0.f, 0.0322f }, { -0.1045f, 0.f, 0.9945f }, { -0.9945f, 0.f, -0.1045f } },
};

static real_matrix4x3 ae_backpack_matrices[AE_BACKPACK_MAXIMUM_NODES];

/* whether the player whose view is drawn has them on (each local player's
own setting, as VIEW's other rows) */
static boolean ae_backpack_enabled(
	void)
{
	static unsigned long changes = (unsigned long)-1;
	static boolean enabled[MAXIMUM_NUMBER_OF_LOCAL_PLAYERS];
	short player = render.local_player_index;

	if (changes != ae_config_player_changes())
	{
		short index;

		changes = ae_config_player_changes();
		for (index = 0; index < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS; index++)
			enabled[index] = ae_config_player_boolean(index, "display.backpack_weapons") != 0;
	}
	return VALID_INDEX(player, MAXIMUM_NUMBER_OF_LOCAL_PLAYERS) ? enabled[player] : enabled[0];
}

/* debug.txt: each body and weapon pair's place, the first time it is drawn
(a few pairs a map: the table only fills, a full one logs no more) */
static void ae_backpack_log(
	long body_definition_index,
	long weapon_definition_index,
	short place,
	short node_index)
{
	static long logged[64][2];
	static short logged_count = 0;
	short index;

	for (index = 0; index < logged_count; index++)
	{
		if (logged[index][0] == body_definition_index && logged[index][1] == weapon_definition_index)
			return;
	}
	if (logged_count >= (short)NUMBEROF(logged))
		return;
	logged[logged_count][0] = body_definition_index;
	logged[logged_count][1] = weapon_definition_index;
	logged_count++;
	error(_error_silent, "backpack weapons: %s on %s: %s%s",
		tag_name_strip_path(tag_get_name(weapon_definition_index)),
		tag_name_strip_path(tag_get_name(body_definition_index)),
		place == AE_BACKPACK_PLACE_NONE ? "not drawn" : ae_backpack_placements[place].name,
		place != AE_BACKPACK_PLACE_NONE && node_index == NONE ? ", the body has no such node: not drawn" : "");
}

/* a holstered weapon's place (AE_BACKPACK_PLACE_NONE: an objective item, what
does not count as a weapon carried, a model-less one, or one too many) */
static short ae_backpack_place(
	long weapon_index,
	int *on_back,
	int *on_thighs)
{
	struct weapon_datum *weapon = weapon_get(weapon_index);
	struct weapon_definition *definition = weapon_definition_get(weapon->definition_index);

	if (weapon_is_flag(weapon_index) ||
		TEST_FLAG(definition->weapon.flags, _weapon_doesnt_count_toward_maximum_bit) ||
		definition->object.model.index == NONE)
	{
		return AE_BACKPACK_PLACE_NONE;
	}
	/* (a holstered weapon's own bounds are not kept: its definition's) */
	return (short)ae_backpack_rules_place(
		ae_backpack_rules_kind(tag_get_name(weapon->definition_index), definition->object.bounding_radius),
		on_back,
		on_thighs);
}

/* the body's node a placement holds to (the first whose name has the
placement's node in it, whatever its case: "bip01 spine1", "Bip01 R Thigh"),
NONE: none */
static short ae_backpack_node(
	struct model const *model,
	char const *part)
{
	size_t length = strlen(part);
	short index;

	for (index = 0; index < model->nodes.count; index++)
	{
		char const *name = TAG_BLOCK_GET_ELEMENT(&model->nodes, index, struct model_node)->name;
		short at;

		for (at = 0; name[at]; at++)
		{
			size_t letter;

			for (letter = 0; letter < length && name[at + letter]; letter++)
			{
				char c = name[at + letter];

				if ((c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : c) != part[letter])
					break;
			}
			if (letter == length)
				return index;
		}
	}
	return NONE;
}

/* where the weapon model's root goes: the body node's skin (as it is drawn
now, from its resting place) times the placement at rest */
static void ae_backpack_anchor(
	struct model const *body_model,
	real_matrix4x3 const *body_nodes,
	short node_index,
	struct ae_backpack_placement const *placement,
	real_matrix4x3 *anchor)
{
	struct model_node const *node = TAG_BLOCK_GET_ELEMENT(&body_model->nodes, node_index, struct model_node);
	real_matrix4x3 skin;
	real_matrix4x3 rest;
	real_matrix4x3 place;
	real_point3d point;
	real_vector3d barrel = placement->barrel;
	real_vector3d top = placement->top;

	matrix4x3_multiply(&body_nodes[node_index], &node->runtime_default_inverse_matrix, &skin);
	matrix4x3_inverse(&node->runtime_default_inverse_matrix, &rest);
	point.x = rest.position.x + placement->offset.i;
	point.y = rest.position.y + placement->offset.j;
	point.z = rest.position.z + placement->offset.k;
	normalize3d(&barrel);
	normalize3d(&top);
	matrix4x3_from_point_and_vectors(&place, &point, &barrel, &top);
	matrix4x3_multiply(&skin, &place, anchor);
}

/* the weapon's model at the anchor, each node in its resting place */
static void ae_backpack_draw(
	long weapon_index,
	real_matrix4x3 const *anchor,
	struct render_lighting const *lighting,
	struct render_model_effect const *model_effect,
	unsigned long flags)
{
	struct weapon_datum *weapon = weapon_get(weapon_index);
	struct object_definition *definition = object_definition_get(weapon->definition_index);
	struct model *model = model_definition_get(definition->object.model.index);
	/* (its definition's bounds, where it is drawn) */
	real radius = definition->object.bounding_radius > 0.f ? definition->object.bounding_radius : 0.1f;
	real pixels;
	short index;

	if (!model || model->nodes.count <= 0 || model->nodes.count > AE_BACKPACK_MAXIMUM_NODES)
		return;
	/* (its size on the screen, as an object's (render_objects.c's
	object_get_level_of_detail_pixels); the shadow pass's lower, as the body's) */
	pixels = render_frustum_sphere_diameter_in_pixels(&render.frustum, &anchor->position, radius);
	if (TEST_FLAG(flags, AE_BACKPACK_RENDER_MODEL_SHADOW_BIT))
		pixels *= 0.3f;
	for (index = 0; index < model->nodes.count; index++)
	{
		struct model_node const *node = TAG_BLOCK_GET_ELEMENT(&model->nodes, index, struct model_node);
		real_matrix4x3 rest;

		matrix4x3_inverse(&node->runtime_default_inverse_matrix, &rest);
		matrix4x3_multiply(anchor, &rest, &ae_backpack_matrices[index]);
	}
	render_model(
		definition->object.model.index,
		pixels,
		ae_backpack_matrices,
		weapon->object.region_permutations,
		weapon->object.outgoing_change_colors,
		weapon->object.outgoing_function_values,
		lighting,
		&anchor->position,
		radius,
		model_effect,
		weapon_index,
		weapon->object.forced_shader_permutation_index,
		flags);
}

void ae_backpack_weapons_render(
	long unit_index,
	struct render_lighting const *lighting,
	struct render_model_effect const *model_effect,
	unsigned long flags)
{
	struct object_datum *object = object_try_and_get(unit_index);
	struct unit_datum *unit;
	struct object_definition *definition;
	struct model *body_model;
	real_matrix4x3 const *body_nodes;
	/* (how many are on the back, and on the thighs) */
	int on_back = 0;
	int on_thighs = 0;
	short slot;
	/* (each view's own setting for the weapons; SEARCH AND DESTROY's bomb is shown whatever it is, and while it is
	carried the back is the bomb's: the rifles stowed there are not drawn, owner 2026-10-28) */
	boolean enabled = ae_backpack_enabled();
	boolean bomb_carried = FALSE;

	if (!object || object->object.type != _object_type_biped ||
		object->object.parent_object_index != NONE ||
		TEST_FLAG(object->object.damage_flags, _object_dead_bit))
	{
		return;
	}
	unit = unit_get(unit_index);
	for (slot = 0; slot < MAXIMUM_WEAPONS_PER_UNIT; slot++)
	{
		if (slot != unit->unit.current_weapon_index && ae_bomb_snd_carried_bomb(unit->unit.weapon_object_indices[slot]))
			bomb_carried = TRUE;
	}
	if (!enabled && !bomb_carried)
		return;
	definition = object_definition_get(object->definition_index);
	if (definition->object.model.index == NONE)
		return;
	body_model = model_definition_get(definition->object.model.index);
	body_nodes = object_get_node_matrices(unit_index);
	if (!body_model || !body_nodes ||
		object->object.node_matrices.size / (long)sizeof(real_matrix4x3) < body_model->nodes.count)
	{
		return;
	}

	for (slot = 0; slot < MAXIMUM_WEAPONS_PER_UNIT; slot++)
	{
		long weapon_index = unit->unit.weapon_object_indices[slot];
		short place;
		short node_index = NONE;
		real_matrix4x3 anchor;

		if (slot == unit->unit.current_weapon_index || weapon_index == NONE || !object_try_and_get(weapon_index))
			continue;
		if (ae_bomb_snd_carried_bomb(weapon_index))
			place = AE_BACKPACK_PLACE_BOMB;
		else if (!enabled)
			continue;
		else
		{
			place = ae_backpack_place(weapon_index, &on_back, &on_thighs);
			if (bomb_carried && (place == AE_BACKPACK_PLACE_BACK || place == AE_BACKPACK_PLACE_BACK_SECOND ||
				place == AE_BACKPACK_PLACE_LONG))
			{
				place = AE_BACKPACK_PLACE_NONE;
			}
		}
		if (place != AE_BACKPACK_PLACE_NONE)
			node_index = ae_backpack_node(body_model, ae_backpack_placements[place].node);
		ae_backpack_log(object->definition_index, object_get(weapon_index)->definition_index, place, node_index);
		if (place == AE_BACKPACK_PLACE_NONE || node_index == NONE)
			continue;
		ae_backpack_anchor(body_model, body_nodes, node_index, &ae_backpack_placements[place], &anchor);
		ae_backpack_draw(weapon_index, &anchor, lighting, model_effect, flags);
	}
}
