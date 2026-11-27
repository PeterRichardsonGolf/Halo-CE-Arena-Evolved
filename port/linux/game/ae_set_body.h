/*
AE_SET_BODY.H

YOUR BODY in a campaign level played alone: the biped the playing set's PLAY AS IT names, or a campaign swap's (the swap's choice
wins: ae_item_sets_rules.h's ae_is_body_source). Nothing in a multiplayer, online, co-op, Firefight or challenge game. A biped
that does not fit a player (ae_biped_pack_rules.h) is not used: the campaign's player stays, the log says why, and the set's file
keeps the answer for the ITEM SETS card.
*/

#ifndef __AE_SET_BODY_H
#define __AE_SET_BODY_H

/* (ae_tag_inject.c's player biped hook) the tag index of the body to spawn as, else the one given */
long ae_set_body_biped(
	long unit_definition_index);

/* the body's hands and movement as well as its biped, into the engine's players layer (Forge's map settings, which plays them for every
slot: first-person hands for any weapon, the biped's movement numbers, given back as the level goes), once the level's tags are in and
again as a player spawns. A body that cannot play empties the layer (a campaign swap's too); with no body to play the layer is not touched.
`quiet`: no log line of what plays. Nothing in a game other than a campaign level played alone */
void ae_set_body_players(int quiet);

#endif /* __AE_SET_BODY_H */
