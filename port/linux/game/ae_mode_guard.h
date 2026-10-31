/*
AE_MODE_GUARD.H

Bug 139 (queue 139): a gametype whose mode a map cannot play never crashes the game or leaves everyone dead. When a
multiplayer map loads (game_engine_initialize_for_new_map, an AE hook line before the mode's own start, after the map's
layout file is applied: ae_layout.h), a mode that misses a netgame flag it needs (CTF on a map with no CTF flags, as the
H2 collection's Terminal) is played as Slayer, and each player is told so as the game starts; a mode with no spawn of
its own game type (Gemini's spawns are all "all except CTF") spawns players at every spawn of the map. Rules:
ae_mode_guard_rules.h. Every machine loads the same map and gametype and decides the same: a guest plays the same mode
as its host.
*/

#ifndef __AE_MODE_GUARD_H
#define __AE_MODE_GUARD_H

struct game_variant;

/* the loaded map against the variant's mode: TRUE when the variant was changed to Slayer (its mode's own settings
cleared), so that the game engine is Slayer's */
boolean ae_mode_guard_new_map(struct game_variant *variant);

/* whether a spawn's game types are ignored on this map (its mode has no spawn of its own) */
boolean ae_mode_guard_any_spawns(void);

/* the line to show each player as the game starts, NULL when the gametype plays as made */
wchar_t const *ae_mode_guard_message(void);

#endif
