/*
AE_GLUE_FIREFIGHT.H

What the FIREFIGHT lobby's MAP CONTENT and OBJECTIVES screens read and set of the game (ae_glue_firefight.c), beside the copy's own rules
(ae_glue_setup_options.h): a map's own content (the enemies, weapons and vehicles a Custom Edition or MCC map carries), the host's
choices of it kept for the map (Firefight's own file), the map's player health rules, and SCALE WITH PLAYERS. A map is named as the
game's map list names it. Menus' thread only. Include after cseries.h.
*/

#ifndef __AE_GLUE_FIREFIGHT_H
#define __AE_GLUE_FIREFIGHT_H

/* (DEV MODE's staged scenes) a map of no file with content of every kind and a player of its own rules, held in memory */
#define AE_FFG_DEV_MAP "dev:firefight"
enum { AE_FFG_DEV_KINDS = 3, AE_FFG_DEV_TAGS = 6, AE_FFG_DEV_NAME = 96 };

/* what the map has of its own (1 enemies, 2 weapons, 4 vehicles: FIREFIGHT_MAP_CONTENT_HAS_*); 0 for the game's own maps */
unsigned char ae_ffg_map_kinds(const char *map);
/* the map's own of a kind (0 enemies, 1 weapons, 2 vehicles): how many, each one's tag name, whether it is on for the game */
int ae_ffg_tag_count(const char *map, int kind);
const char *ae_ffg_tag_name(const char *map, int kind, int index);
int ae_ffg_tag_on(const char *map, int kind, const char *tag_name);
void ae_ffg_tag_set(const char *map, int kind, const char *tag_name, int on);
/* PLAYER LOADOUT: the map's weapon the players spawn with in a slot (0 primary, 1 secondary), NULL for the game's; set (NULL: the game's) */
const char *ae_ffg_loadout_get(const char *map, int slot);
void ae_ffg_loadout_set(const char *map, int slot, const char *tag_name);
/* HEALTH RULES: whether the map's player has rules of its own (bit 1 no shields, bit 2 shields that never come back), 0 for none;
the host's rule for the map (SHIELDS, SHIELD REGEN: 0 the map's, 1 on, 2 off; HEALTH REGEN: 0 off, 1 on) */
unsigned char ae_ffg_health_detect(const char *map);
void ae_ffg_health_get(const char *map, unsigned char rules[3]);
void ae_ffg_health_set(const char *map, unsigned char const rules[3]);
/* whether the map has vehicles for the players: the game's, or its own where the content byte takes them (VEHICLES' row shows) */
int ae_ffg_vehicles_shown(const char *map, unsigned char content);
/* MAP CONTENT as last kept for the map: 1 and the byte, else 0; the byte kept (only a map with content of its own keeps one) */
int ae_ffg_content_saved(const char *map, unsigned char *content);
void ae_ffg_content_save(const char *map, unsigned char content);
/* SCALE WITH PLAYERS (percent): the game's, kept for the next games */
int ae_ffg_scale_get(void);
void ae_ffg_scale_set(int percent);

#endif
