/*
AE_CAMPAIGN_WEAPONS.H

AE WEAPONS IN LEVELS (game.campaign_ae_weapons, off by default) and the AI SQUAD'S GEAR (game.companions_loadout): a campaign
level of this machine's own (a game alone, or the host of co-op) has a share of its weapons swapped for AE's by kind
(ae_campaign_weapons_rules.h). Two places:
- the weapons lying in the level, as the level places them (objects.c): the weapon placement's definition is another;
- the weapons the level's AI carry, as an actor's unit is made (actors.c).
The objects are the host's own and reach every machine as any object does, with the weapon pack's tags at the same indices
(the pack is added to the campaign map as to any). A level whose pack was refused has none of the weapons (the weapon pack
lines say why): nothing is swapped and one line says so. A weapon a script names, and a weapon that is the pack's already,
stay as they are.
*/

#ifndef AE_CAMPAIGN_WEAPONS_H
#define AE_CAMPAIGN_WEAPONS_H

#include "ae_campaign_weapons_rules.h"

/* the definition a scenario weapon placement (scenario_object: struct scenario_object_datum) places: the level's own, or an
AE weapon's */
long ae_campaign_weapons_definition(long definition_index, void *scenario_object);

/* gives an AI actor's unit an AE weapon in place of the actor's own weapon (weapon_definition): 1 when it did, 0 when the
actor's own weapon is to be given */
int ae_campaign_weapons_actor_weapon(long weapon_definition, long unit_index);

/* the AI squad member's (slot 0-3) body gets the AE primary and sidearm of its slot: how many were given, 0 for the standard
loadout (the setting, or a weapon the level has not) */
int ae_campaign_weapons_give_squad(long unit_index, int slot, int reset_equipment);

/* whether a game's guests need the weapon cache (campaign_game: no game engine runs it): AE WEAPONS on, or an AE squad asked for */
int ae_campaign_weapons_game_needs_cache(int campaign_game);

/* whether game.companions_loadout is ae */
int ae_campaign_weapons_squad_is_ae(void);

/* DEV MODE campaign_weapons: a line saying what the loaded level was given */
void ae_campaign_weapons_status(char *text, unsigned int size);

/* the nth (1 up) weapon the level swapped: where it lies; the number swapped so far (0 when none) */
int ae_campaign_weapons_spot(int number, float *x, float *y, float *z);

#endif /* AE_CAMPAIGN_WEAPONS_H */
