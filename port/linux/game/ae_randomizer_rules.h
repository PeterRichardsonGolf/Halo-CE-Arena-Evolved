/*
AE_RANDOMIZER_RULES.H

Which games the campaign randomizer (randomizer.c) plays in: a campaign level played alone, a Firefight game, and the
firing range's level played alone. Never a multiplayer game's level, a network game's other than Firefight, or the menus'.
No tag pack is written, no tag copied, nothing logged for any other game.
*/

#ifndef __AE_RANDOMIZER_RULES_H
#define __AE_RANDOMIZER_RULES_H

/* whether the game about to be played is a Firefight game: its variant's engine (game.c's game_variant_global, set before its
map loads, never the variant of the game played before) is Firefight's, or the network game's variant is */
int ae_randomizer_rules_firefight_game(int next_variant_is_firefight, int network_variant_is_firefight);

/* firefight: ae_randomizer_rules_firefight_game; firing_range: the range's level; network: a game with other machines (a
host's or a client's); solo_scenario: the level is a campaign (single player) level. 1 when the randomizer may play this
level. The range plays alone: never with other machines */
int ae_randomizer_rules_plays(int firefight, int firing_range, int network, int solo_scenario);

#endif /* __AE_RANDOMIZER_RULES_H */
