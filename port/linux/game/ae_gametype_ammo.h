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

/* the "AE ammo:" line of the game's rules, after its "AE rules:" one (from the variant's byte) */
void ae_gametype_ammo_log_rules(unsigned char pack_loadout);

/* a weapon fired (weapons.c): with debug.weapon_log and either rule on, its magazine after the shot in the log
("ae ammo: tick N, <weapon> fired, L loaded, R in reserve"), the first few hundred a game */
void ae_gametype_ammo_fired(long weapon_index, short magazine_index);

#endif
