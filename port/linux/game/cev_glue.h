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

#endif /* __CEV_GLUE_H */
