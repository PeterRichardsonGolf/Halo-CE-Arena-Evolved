/*
AE_MCC_SKINNING_RULES_TEST.C

A model's vertex bones as the renderer skins them (port/linux/game/ae_mcc_skinning_rules.c), over a fake node block
laid out as a model's (156 bytes a node, the parent's index at 0x24) with the bones of a 47-node biped: the first 43
nodes skinned as they are; the fingers' tips past them (43..46) on their knuckles; a chain of two past them on the
first skinned ancestor; no nodes, a node past the block, a parent loop or a parent after its child on the root; a
negative node on the root.
*/

#include <stdio.h>
#include <string.h>

#include "../game/ae_mcc_skinning_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

#define STRIDE 156
#define NODES 64

static unsigned char block[NODES * STRIDE];

static void set_parent(short node, short parent)
{
	memcpy(block + node * STRIDE + AE_MCC_NODE_PARENT_OFFSET, &parent, sizeof(parent));
}

int main(void)
{
	short node;

	/* a chain: each node's parent the one before, the root's none */
	set_parent(0, -1);
	for (node = 1; node < NODES; node++)
		set_parent(node, (short)(node - 1));
	/* the biped's fingers' tips: left thumb, right index, ring, thumb */
	set_parent(43, 37);
	set_parent(44, 38);
	set_parent(45, 39);
	set_parent(46, 40);

	for (node = 0; node < AE_MCC_SKINNED_NODES; node++)
		CHECK(ae_mcc_skinned_node(block, STRIDE, 47, node) == node);
	CHECK(ae_mcc_skinned_node(block, STRIDE, 47, 43) == 37);
	CHECK(ae_mcc_skinned_node(block, STRIDE, 47, 44) == 38);
	CHECK(ae_mcc_skinned_node(block, STRIDE, 47, 45) == 39);
	CHECK(ae_mcc_skinned_node(block, STRIDE, 47, 46) == 40);

	/* two past the skinned ones: 48's parent 47, 47's 42 */
	set_parent(47, 42);
	set_parent(48, 47);
	CHECK(ae_mcc_skinned_node(block, STRIDE, 49, 48) == 42);

	/* no nodes, a node past the block, a negative node */
	CHECK(ae_mcc_skinned_node(NULL, STRIDE, 47, 44) == 0);
	CHECK(ae_mcc_skinned_node(NULL, STRIDE, 47, 12) == 12);
	CHECK(ae_mcc_skinned_node(block, STRIDE, 47, 50) == 0);
	CHECK(ae_mcc_skinned_node(block, STRIDE, 47, -1) == 0);
	/* a stride too short to hold the parent */
	CHECK(ae_mcc_skinned_node(block, 8, 47, 44) == 0);

	/* a parent after its child, a node its own parent */
	set_parent(50, 51);
	CHECK(ae_mcc_skinned_node(block, STRIDE, 64, 50) == 0);
	set_parent(52, 52);
	CHECK(ae_mcc_skinned_node(block, STRIDE, 64, 52) == 0);

	if (failures)
	{
		printf("ae_mcc_skinning_rules_test: %d failed\n", failures);
		return 1;
	}
	printf("ae_mcc_skinning_rules_test: all passed\n");
	return 0;
}
