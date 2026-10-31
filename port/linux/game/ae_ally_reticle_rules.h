/*
AE_ALLY_RETICLE_RULES.H

ALLY RETICLE (ae_ally_reticle.c): who counts as an ally under the reticle, as
pure rules (port/linux/tests/ae_ally_reticle_rules_test.c runs them).

An ally is a living unit that is not yourself, in a game where sides exist:
a team game (the same team, and not an enemy as the game's allegiances say), or
a co-op or campaign game (anyone the allegiances do not call an enemy: the
other players, the marines). In a free for all game nobody is an ally.
*/

#ifndef __AE_ALLY_RETICLE_RULES_H
#define __AE_ALLY_RETICLE_RULES_H

/* the colour of an ally's reticle as 0xAARRGGBB with the reticle's own alpha
kept: the friendly green */
#define AE_ALLY_RETICLE_RGB 0x0000FF00ul

/* whether the unit under the reticle is an ally: is_self, it is the viewer's
own unit; dead, it is dead; team_game, a multiplayer game with teams;
side_game, a co-op or campaign game; same_team, it is on the viewer's team;
engine_enemy, the game's allegiances call its team an enemy of the viewer's */
int ae_ally_reticle_rules_is_ally(int is_self, int dead, int team_game, int side_game, int same_team,
	int engine_enemy);

/* whether the reticle piece turns green: the setting is on, the target is an
ally, and the piece is drawn as the plain one (frame 0: the red frame means
the game's aim assist found an enemy, which wins) */
int ae_ally_reticle_rules_green(int setting_on, int ally, int aim_frame);

/* the reticle's colour with the friendly green in place of its red, green and
blue and its own alpha kept */
unsigned long ae_ally_reticle_rules_color(unsigned long color);

#endif // __AE_ALLY_RETICLE_RULES_H
