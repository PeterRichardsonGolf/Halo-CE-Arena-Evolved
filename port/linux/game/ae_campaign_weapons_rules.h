/*
AE_CAMPAIGN_WEAPONS_RULES.H

AE WEAPONS IN LEVELS (game.campaign_ae_weapons, off by default): a campaign level's weapons are swapped for AE's by kind, pure
(no engine includes; port/linux/tests/ae_campaign_weapons_rules_test.c). ae_campaign_weapons.c applies it to the weapons lying
in the level (the scenario's weapon placements) and to the weapons the level's AI carry.

The kind of a weapon is the leaf of its tag path ("weapons\assault rifle\assault rifle"). The table (registry ids,
tools/weapon_cache/registry.json), each kind's weapons taken in turn by the placement's seed:
- assault rifle: COMBAT AR, SUPPRESSED AR, TACTICAL SMG
- pistol: SIDEARM, HEAVY REVOLVER
- sniper rifle: BOLT SNIPER
- needler: NEEDLER
- rocket launcher: BRUTE SHOT
The plasma weapons, the shotgun, the flamethrower, the fuel rod gun and the sword stay the level's own. A share of the
placements (game.campaign_ae_weapons_share, 0 to 100 percent) is swapped; which ones is the seed's: the map's name and the
placement's place, the same on every machine and every time the level loads.

THE AI SQUAD'S GEAR (game.companions_loadout): standard is the level's co-op start weapons; ae gives each squad member an AE
primary and sidearm of its own (ae_campaign_weapons_squad).
*/

#ifndef AE_CAMPAIGN_WEAPONS_RULES_H
#define AE_CAMPAIGN_WEAPONS_RULES_H

enum ae_campaign_weapons_kind
{
	AE_CAMPAIGN_WEAPONS_NONE = 0,
	AE_CAMPAIGN_WEAPONS_ASSAULT_RIFLE,
	AE_CAMPAIGN_WEAPONS_PISTOL,
	AE_CAMPAIGN_WEAPONS_SNIPER_RIFLE,
	AE_CAMPAIGN_WEAPONS_NEEDLER,
	AE_CAMPAIGN_WEAPONS_ROCKET_LAUNCHER,
	AE_CAMPAIGN_WEAPONS_KIND_COUNT
};

enum
{
	AE_CAMPAIGN_WEAPONS_SHARE_DEFAULT = 60,
	AE_CAMPAIGN_WEAPONS_SQUAD_MOST = 4
};

enum ae_campaign_weapons_loadout
{
	AE_CAMPAIGN_WEAPONS_LOADOUT_STANDARD = 0,
	AE_CAMPAIGN_WEAPONS_LOADOUT_AE
};

/* the kind of a weapon tag by its path (a backslash or slash path, any case); NONE for any other leaf */
int ae_campaign_weapons_kind_of(char const *tag_name);
char const *ae_campaign_weapons_kind_name(int kind);

/* a share setting kept to 0-100 (a bad one is the default) */
int ae_campaign_weapons_share_clamp(long share);

/* the seed of a placement: the map's name and the place, a salt for the other draws */
unsigned long ae_campaign_weapons_seed(char const *map_name, float x, float y, float z, unsigned long salt);

/* whether the seed's placement is one of the share's percent */
int ae_campaign_weapons_share_takes(unsigned long seed, int percent);

/* the registry id the kind is swapped for by the seed; 0 for a kind with no swap */
unsigned int ae_campaign_weapons_pick(int kind, unsigned long seed);

/* game.companions_loadout's word: standard (also any word that is not ae) or ae */
int ae_campaign_weapons_loadout_of_word(char const *word);

/* a squad member's AE weapons (slot 0-3): the primary and the sidearm, registry ids; slots past the table wrap */
void ae_campaign_weapons_squad(int slot, unsigned int *primary, unsigned int *sidearm);

/* the AE WEAPONS row (CAMPAIGN screen): OFF, SOME (30%), MOST (60%), ALL (100%) */
enum
{
	AE_CAMPAIGN_WEAPONS_LEVEL_OFF = 0,
	AE_CAMPAIGN_WEAPONS_LEVEL_SOME,
	AE_CAMPAIGN_WEAPONS_LEVEL_MOST,
	AE_CAMPAIGN_WEAPONS_LEVEL_ALL,
	AE_CAMPAIGN_WEAPONS_LEVELS
};

/* the row's step of the setting: off with any share is OFF; on, the nearest step at or above the share (a hand-written 45 is MOST) */
int ae_campaign_weapons_level_of(int enabled, int share);
/* the share a step writes (0 for OFF) */
int ae_campaign_weapons_level_share(int level);
char const *ae_campaign_weapons_level_name(int level);
/* the next step (direction 1 or -1), round */
int ae_campaign_weapons_level_step(int level, int direction);

/* whether a game's guests need the weapon cache to join: a campaign game (no game engine) with AE WEAPONS on, or with the squad's
gear AE and a squad asked for (the squad's weapons are objects every machine draws). A multiplayer game's own rules decide
otherwise */
int ae_campaign_weapons_join_needs_cache(int campaign_game, int weapons_on, int squad_is_ae, int squad_count);

#endif /* AE_CAMPAIGN_WEAPONS_RULES_H */
