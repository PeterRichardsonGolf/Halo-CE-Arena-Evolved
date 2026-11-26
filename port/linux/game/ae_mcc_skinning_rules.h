#ifndef AE_MCC_SKINNING_RULES_H
#define AE_MCC_SKINNING_RULES_H

/* A model's vertex bones as the renderer skins them. The vertex shader holds AE_MCC_SKINNED_NODES nodes' matrices
(rasterizer.h's RASTERIZER_MAXIMUM_NODES_PER_MODEL - 1, models.c), and every vertex names a node of the model by its
index there. A model of more nodes (an MCC model may have 64) has a vertex bone past them drawn on its nearest
ancestor among them: a finger's tip on its knuckle. Nodes come parents first, so an ancestor has a lower index. */

#define AE_MCC_SKINNED_NODES 43
/* (the parent's index in a model node: model_definitions.h's struct model_node, checked in ae_mcc_skinning.c) */
#define AE_MCC_NODE_PARENT_OFFSET 0x24

/* the node a vertex bone is skinned on: `node` itself when it is one of the first AE_MCC_SKINNED_NODES, else its
nearest such ancestor, else 0 (the root). `nodes`: the model's `count` nodes, `stride` bytes apart; NULL draws a node
past the skinned ones on the root. */
short ae_mcc_skinned_node(unsigned char const *nodes, unsigned long stride, long count, short node);

#endif
