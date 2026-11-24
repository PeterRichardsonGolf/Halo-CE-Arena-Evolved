/*
AE_GAMETYPE_AMMO.H

INFINITE AMMO and BOTTOMLESS CLIP as the gametype's rules (the bits:
ae_weapon_rules.h): what the AE hook lines in source/items/weapons.c and
game_engine.c call (port/linux/game/ae_hooks.txt lists them), beside the
cheats cheat_infinite_ammo / cheat_bottomless_clip. Included after cseries.h.
*/

#ifndef __AE_GAMETYPE_AMMO_H
#define __AE_GAMETYPE_AMMO_H

/* the running game's gametype has INFINITE AMMO / BOTTOMLESS CLIP (FALSE with no game: the campaign, the menus) */
boolean ae_gametype_infinite_ammo(void);
boolean ae_gametype_bottomless_clip(void);

/* MAP GRENADES OFF (ae_weapon_rules.h): TRUE when the running game's gametype has it off and definition_index is a
grenade pickup (equipment of the grenade powerup type): game_engine.c's item spawn skips it. FALSE with no game */
boolean ae_gametype_skips_map_item(long definition_index);
/* the same, asked by the item timers (item_timers.c: no timer, waypoint or callout for an item the game never
spawns); logged: debug.weapon_log's grenade lines, the spawn's alone */
boolean ae_gametype_map_item_skipped(long definition_index, boolean logged);

/* the "AE ammo:" line of the game's rules, after its "AE rules:" one (from the variant's byte) */
void ae_gametype_ammo_log_rules(unsigned char pack_loadout);

/* a weapon fired (weapons.c): with debug.weapon_log and either rule on, its magazine after the shot in the log
("ae ammo: tick N, <weapon> fired, L loaded, R in reserve"), the first few hundred a game */
void ae_gametype_ammo_fired(long weapon_index, short magazine_index);

#endif
