/*
AE_RIFLE_MENU.H

BATTLE RIFLE, the gametype editor's ARENA OPTIONS row (the weapon pack's storage: game_engine.h's
universal_variant.weapon_pack and pack_loadout). Included after cseries.h.
*/

#ifndef __AE_RIFLE_MENU_H
#define __AE_RIFLE_MENU_H

struct game_variant;
struct game_variant_options;
struct widget_instance;

/* the row's spinner shown from the gametype, or (save) written to it */
void ae_rifle_menu_rows(struct widget_instance *list, struct game_variant *variant, struct game_variant_options *options,
	boolean save);

#endif /* __AE_RIFLE_MENU_H */
