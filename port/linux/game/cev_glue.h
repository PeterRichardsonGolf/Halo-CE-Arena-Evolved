/*
CEV_GLUE.H

What the Custom Edition copy (cev/) is told by this build: cev_glue.c.
*/

#ifndef __CEV_GLUE_H
#define __CEV_GLUE_H

/* a Custom Edition map (its level name) tags can be taken out of; FALSE when
the list is full or the name too long */
boolean cev_maps_add(
	char const *level_name);

/* the Custom Edition copy's structure bsp buffers (cev/custom_edition_geometry.c): a bsp whose materials carry
uncompressed vertices has them compressed and given buffers, and let go of again */
struct structure_bsp;
boolean cev_custom_edition_structure_bsp_load(
	struct structure_bsp *structure_bsp);
void cev_custom_edition_structure_bsp_unload(
	void);

#endif /* __CEV_GLUE_H */
