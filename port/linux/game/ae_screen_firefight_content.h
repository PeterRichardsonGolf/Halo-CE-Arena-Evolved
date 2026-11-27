/*
AE_SCREEN_FIREFIGHT_CONTENT.H

Firefight's two lobby screens (ae_screen_firefight_content.c), the rows of Content Evolved's Server Setup in AE's menus:

MAP CONTENT, for the map the lobby has chosen: USE CONTENT (VANILLA ONLY, INCLUDE MAP, ONLY MAP), the kinds of the map's own it has (MAP ENEMY AI,
MAP WEAPONS, MAP VEHICLES: ALL, CUSTOM, NONE, and CHOOSE rows that turn each of the map's own on or off), PLAYER LOADOUT (the map's weapons the
players spawn with) and HEALTH RULES (SHIELDS, SHIELD REGEN, HEALTH REGEN) for a map whose player has rules of its own. Every choice is kept
for the map.

OBJECTIVES: WAVE OBJECTIVES and ROUND OBJECTIVES, each a switch, a chance and the kinds that may be rolled; Firefight's settings, the same on every map.

Their log: "ae firefight content: open (<map>)", "ae firefight content: <what> <value>", "ae firefight objectives: open", "... <key> <value>".
*/

#ifndef __AE_SCREEN_FIREFIGHT_CONTENT_H
#define __AE_SCREEN_FIREFIGHT_CONTENT_H

/* MAP CONTENT over the stack for the map (the game's map list's name) and its caption; the Firefight copy ae_glue_setup_options.h has open
holds the byte. 0 when the stack is full or no Firefight copy is open */
int ae_screen_firefight_content_open(const char *map, const char *caption);
/* OBJECTIVES over the stack; 0 when the stack is full */
int ae_screen_firefight_objectives_open(void);
/* the lobby's rows' values and helps: MAP CONTENT's summary of the map (a map with nothing of its own: ""), OBJECTIVES' */
void ae_screen_firefight_content_value(const char *map, char *text, int size);
void ae_screen_firefight_objectives_value(char *text, int size);
/* (DEV MODE `menu firefight ...`) a screen staged: "content", "content-custom", "tags", "loadout", "objectives" */
int ae_screen_firefight_open_staged(const char *scene, const char *map, const char *caption);

#endif
