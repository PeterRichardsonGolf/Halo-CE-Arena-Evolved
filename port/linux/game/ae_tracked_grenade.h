/*
AE_TRACKED_GRENADE.H

port: weapon pack (0.7.6). Hold-to-delay, release-to-detonate for a pack weapon whose trigger tracks the projectile
it fires (the trigger's "tracks fired projectile" flag; the GRENADE LAUNCHER pack): the shot's grenade is the
weapon's tracked object, its fuse does not run while the trigger is held, and letting go sets it off where it is
(ae_tracked_grenade.c; the rules are ae_tracked_grenade_rules.h's). The trigger is read from the weapon's control
(its owner's input), not the trigger state machine, which counts the weapon's busy time after a shot as let go. A
stock or map weapon with the flag keeps the game's way (the trigger tracks nothing). The hooks are single lines in
weapons.c and projectiles.c (ae_hooks.txt).

Every machine runs every player's weapon from the inputs the host relays (port/linux/NETCODE.md), so the release
reaches each machine with that player's trigger, and each sets off its own copy of the grenade; the damage is dealt
as any projectile's is (the firing player's machine's, the host's for its own players and the host's checks of a
client's report). No message of its own.

Include after cseries.h.
*/

#ifndef __AE_TRACKED_GRENADE_H
#define __AE_TRACKED_GRENADE_H

#include "ae_tracked_grenade_rules.h"

/* (weapons_initialize_for_new_map) every tracked grenade forgotten and the log's count reset: an object handle of the
last map may name another object on this one */
void ae_tracked_grenade_new_map(void);

/* (trigger_create_projectiles) a projectile was made for a shot: projectile_in_shot its place in the shot (0 the
first). A pack weapon's tracking trigger's first projectile becomes the weapon's tracked object */
void ae_tracked_grenade_fired(long weapon_index, short trigger_index, long projectile_in_shot, long projectile_object_index);

/* (projectile_update, before its fuse) this tick's action for the projectile (ae_tracked_grenade_rules.h's
AE_TRACKED_GRENADE_*): RUN for any projectile not tracked, HOLD (its fuse waits) while its weapon's trigger is held,
DETONATE once let go (the update then sets it off as its fuse would). It also clears the owner handle of any projectile whose
shooter object is gone (ae_tracked_grenade_owner) */
int ae_tracked_grenade_tick(long projectile_index);

#endif /* __AE_TRACKED_GRENADE_H */
