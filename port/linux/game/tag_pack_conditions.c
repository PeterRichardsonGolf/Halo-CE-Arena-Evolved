/*
TAG_PACK_CONDITIONS.C

The names a tag pack's manifest may want it by at start-up
("condition:<name>"), beside "always" and "setting:<key>" (tag_pack.c):
each a question only code can answer. This fork's; another build lists its
own here.
*/

#include "cseries.h"

#include "tag_pack.h"
#include "ae_firefight_doors.h"

/* skulls to be found, or Outbreak's Flood (skulls.c) */
int skulls_pack_wanted(void);

/* Firefight's data (the scenery's pictures: ae_firefight.pack) is built at the main menu: game.firefight_pack */
static int firefight_pack_wanted(void)
{
	return ae_firefight_doors_pack_on();
}

/* the Forge menu opened on this machine, once: this build has no Forge */
static int forge_pack_wanted(void)
{
	return 0;
}

struct tag_pack_condition const tag_pack_conditions[] =
{
	{ "skulls", skulls_pack_wanted },
	{ "firefight", firefight_pack_wanted },
	{ "forge", forge_pack_wanted },
	{ NULL, NULL },
};
