/*
TAG_PACK_CONDITIONS.C

The names a tag pack's manifest may want it by at start-up
("condition:<name>"), beside "always" and "setting:<key>" (tag_pack.c):
each a question only code can answer. This fork's; another build lists its
own here.
*/

#include "cseries.h"

#include "tag_pack.h"

/* skulls to be found, or their question not answered yet: this build has no skulls */
static int skulls_pack_wanted(void)
{
	return 0;
}

/* the Forge menu opened on this machine, once: this build has no Forge */
static int forge_pack_wanted(void)
{
	return 0;
}

struct tag_pack_condition const tag_pack_conditions[] =
{
	{ "skulls", skulls_pack_wanted },
	{ "forge", forge_pack_wanted },
	{ NULL, NULL },
};
