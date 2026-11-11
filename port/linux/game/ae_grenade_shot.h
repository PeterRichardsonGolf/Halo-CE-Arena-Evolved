/*
AE_GRENADE_SHOT.H

SHOOTABLE GRENADES as the gametype's rule (ae_grenade_shot_rules.h): a bullet's travel that passes a live thrown grenade or
a grenade pickup sets it off, the shooter owning the blast. Every machine has its own copy of a grenade, so the host
decides (a bullet it simulates, or the report of the shooter's machine, checked) and tells the rest in message 118; a build
without the messages never sees them, and its copy goes off at its fuse. Include after cseries.h.
*/

#ifndef __AE_GRENADE_SHOT_H
#define __AE_GRENADE_SHOT_H

/* projectiles.c's projectile_update, before the bullet's collision test: the segment from..to is looked over for a grenade
to set off (the host and an offline game decide; a client reports its own players' bullets to the host) */
void ae_grenade_shot(long projectile_index, real_point3d const *from, real_point3d const *to);

/* the network's messages 117 (a client's report to the host) and 118 (the host's blast to every client); the host's and
a client's copies cleared with a new game */
int ae_grenade_shot_handles_message(unsigned short const *message, unsigned short size);
void ae_grenade_shot_handle_message(long machine_index, unsigned short const *message, unsigned short size,
	int from_stream);
void ae_grenade_shot_reset(void);

#endif
