/*
AE_RIFLE_MENU.H

BATTLE RIFLE, a choice of the gametype editor's ITEM OPTIONS starting weapons (the weapon pack's storage: game_engine.h's
universal_variant.weapon_pack and pack_loadout). Included after cseries.h.
*/

#ifndef __AE_RIFLE_MENU_H
#define __AE_RIFLE_MENU_H

struct game_variant;
struct game_variant_options;
struct widget_instance;

/* the two starting weapon rows' spinners shown from the gametype, or (save) written to it; called after the
screen's own rows are */
void ae_rifle_menu_rows(struct widget_instance *list, struct game_variant *variant, struct game_variant_options *options,
	boolean save);

#endif /* __AE_RIFLE_MENU_H */
