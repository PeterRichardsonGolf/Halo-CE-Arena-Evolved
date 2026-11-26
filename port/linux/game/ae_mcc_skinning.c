/*
AE_MCC_SKINNING.C

An MCC model of more nodes than the renderer skins (mcc_geometry.c): its vertices' and centroids' bones named by the
model's node index, a bone past the skinned ones on its nearest skinned ancestor (ae_mcc_skinning_rules.c). Each move
is logged once a model and bone: "mcc models: a model of 47 nodes draws bone 44 on 38".
*/

#include "cseries.h"
#include "errors.h"
#include "models/model_definitions.h"

#include <stddef.h>

#include "ae_mcc_skinning.h"
#include "ae_mcc_skinning_rules.h"

typedef char ae_mcc_node_parent_offset[
	offsetof(struct model_node, parent_node_index) == AE_MCC_NODE_PARENT_OFFSET ? 1 : -1];

/* the moves logged: a model's node block and the bone */
static struct
{
	void const *nodes;
	short node;
} logged[64];
static long logged_count;

short ae_mcc_model_skinned_node(void const *nodes, long count, short node)
{
	short skinned = ae_mcc_skinned_node((unsigned char const *)nodes, sizeof(struct model_node), count, node);
	long index;

	if (skinned == node)
		return skinned;
	for (index = 0; index < logged_count; index++)
	{
		if (logged[index].nodes == nodes && logged[index].node == node)
			return skinned;
	}
	if (logged_count < (long)NUMBEROF(logged))
	{
		logged[logged_count].nodes = nodes;
		logged[logged_count].node = node;
		logged_count++;
		error(_error_silent, "mcc models: a model of %ld nodes draws bone %d on %d", count, node, skinned);
	}
	return skinned;
}
