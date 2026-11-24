/*
DEATH_VOLUMES.H

(port) The trigger volumes a map kills in (death_volumes.c).
*/

#ifndef __DEATH_VOLUMES_H
#define __DEATH_VOLUMES_H
#pragma once

/* the deadly volume a point is in; NONE for none */
short death_volume_at(real_point3d const *point);
/* whether a point, or one up to height above it, is in a deadly volume (the
ground under one: Splash Site's sea floor under its "killme2") */
boolean death_volume_column(real_point3d const *point, real height);
/* whether a trigger volume is one the level kills in (Forge's VOLUMES
adopt them: forge_volumes.c) */
boolean death_volume_is_deadly(short volume_index);
/* whether the level has any deadly volume */
boolean death_volumes_any(void);
/* a volume found to kill as the game went on (ai_falls.c) */
void death_volume_learn(short volume_index, char const *why);
/* forgotten at the level's end */
void death_volumes_level_end(void);

#endif
