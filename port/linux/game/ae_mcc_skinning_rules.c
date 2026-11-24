#include "ae_mcc_skinning_rules.h"

#include <string.h>

static short parent_of(unsigned char const *nodes, unsigned long stride, short node)
{
	short parent;

	memcpy(&parent, nodes + (unsigned long)node * stride + AE_MCC_NODE_PARENT_OFFSET, sizeof(parent));
	return parent;
}

short ae_mcc_skinned_node(unsigned char const *nodes, unsigned long stride, long count, short node)
{
	long steps;

	if (node < 0)
		return 0;
	/* (at most one step a node: a parent loop ends on the root) */
	for (steps = 0; node >= AE_MCC_SKINNED_NODES && steps < count; steps++)
	{
		short parent;

		if (!nodes || node >= count || stride < AE_MCC_NODE_PARENT_OFFSET + sizeof(short))
			return 0;
		parent = parent_of(nodes, stride, node);
		/* (a parent must come first: anything else is drawn on the root) */
		if (parent < 0 || parent >= node)
			return 0;
		node = parent;
	}

	return node < AE_MCC_SKINNED_NODES ? node : 0;
}
