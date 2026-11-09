/*
AE_SMOKE_VOLUME.H
THE AE SMOKE GRENADE's volumes (ae_smoke_rules.h says what they are). Included after cseries.h.
*/

#ifndef __AE_SMOKE_VOLUME_H
#define __AE_SMOKE_VOLUME_H

/* projectile_detonate (every machine that runs the detonation): a volume made when the projectile is the smoke
grenade (ae_smoke_grenade.c), at the point it went off; one "ae smoke:" line */
void ae_smoke_detonated(long projectile_definition_index, union real_point3d const *at);
/* bots_line_of_sight: whether a volume stands across the sight from the eye to the point (TRUE: the bot sees
nothing there) */
boolean ae_smoke_blocks_sight(union real_point3d const *eye, union real_point3d const *point);
/* how many volumes stand now (the tests and the log) */
long ae_smoke_volume_count(void);
/* every volume let go (a new map) */
void ae_smoke_volume_reset(void);

#endif
