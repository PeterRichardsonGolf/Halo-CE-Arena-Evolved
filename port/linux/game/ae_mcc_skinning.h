#ifndef AE_MCC_SKINNING_H
#define AE_MCC_SKINNING_H

/* (mcc_geometry.c, a model of more nodes than the renderer skins) the node a vertex or centroid bone is drawn on:
ae_mcc_skinning_rules.h. `nodes`: the model's node block (struct model_node), or NULL; `count` its nodes. A bone
moved to an ancestor is logged once a model. */
short ae_mcc_model_skinned_node(void const *nodes, long count, short node);

#endif
