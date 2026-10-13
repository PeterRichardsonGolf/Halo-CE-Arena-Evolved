/*
AE_RIFLE_MENU.C

BATTLE RIFLE (ae_rifle_menu.h): OFF, or STARTING WEAPON: players start with the weapon pack's battle rifle in place
of the assault rifle. Stored as the gametype's weapon pack and which slot of its custom loadout the pack fills; the
slot keeps its stock weapon as the fallback (a map the pack cannot be added to plays that).
*/

#include <string.h>

#include "cseries.h"
#include "cseries/cseries_windows.h"
#include "game/game_engine.h"
#include "interface/ui_widget.h"
#include "interface/ui_widget_instance.h"

#include "ae_rifle_menu.h"

void platform_log(char const *format, ...);

#define SPINNER_BATTLE_RIFLE "battle_rifle_spinner"
/* (pack_loadout's bits: two a slot, game_engine.h) */
#define SLOT_PACK(slot) ((byte)(_pack_loadout_pack << ((slot) * 2)))
#define SLOTS_MASK 0x0F

static struct widget_instance *spinner_named(struct widget_instance *list, char const *name)
{
	struct widget_instance *row;

	for (row = list ? list->child : NULL; row; row = row->next)
	{
		struct widget_instance *child;

		for (child = row->child; child; child = child->next)
		{
			if (child->type == 2 /* spinner */ && child->name && !strncmp(child->name, name, 31))
				return child;
		}
	}
	return NULL;
}

void ae_rifle_menu_rows(struct widget_instance *list, struct game_variant *variant, struct game_variant_options *options,
	boolean save)
{
	struct widget_instance *spinner = spinner_named(list, SPINNER_BATTLE_RIFLE);
	struct universal_variant *universal = variant ? &variant->universal_variant : NULL;
	boolean on;

	if (!universal || !options || !spinner)
		return;
	on = universal->weapon_pack == _weapon_pack_battle_rifle && (universal->pack_loadout & SLOTS_MASK) != 0;
	if (!save)
	{
		spinner->parameters.list.selected_index = on ? 1 : 0;
		return;
	}
	/* (left as it was: a gametype made elsewhere with both slots, say, is not rewritten by an OK) */
	if ((spinner->parameters.list.selected_index == 1) == on)
		return;
	universal->pack_loadout &= (byte)~SLOTS_MASK;
	if (spinner->parameters.list.selected_index != 1)
	{
		universal->weapon_pack = _weapon_pack_none;
		platform_log("menus: the gametype's battle rifle off");
		return;
	}
	/* (the pack fills a custom loadout's slot: a gametype on the weapon set's own starts with an assault
	rifle and a pistol, the fallbacks) */
	if (options->loadout != _loadout_custom)
	{
		options->loadout = _loadout_custom;
		options->primary_weapon = _loadout_weapon_assault_rifle;
		options->secondary_weapon = _loadout_weapon_pistol;
	}
	universal->weapon_pack = _weapon_pack_battle_rifle;
	/* (in place of the assault rifle, wherever the loadout has it; with none, the first slot) */
	if (options->secondary_weapon == _loadout_weapon_assault_rifle && options->primary_weapon != _loadout_weapon_assault_rifle)
		universal->pack_loadout |= SLOT_PACK(_pack_loadout_secondary);
	else
		universal->pack_loadout |= SLOT_PACK(_pack_loadout_primary);
	platform_log("menus: the gametype's battle rifle on (the %s slot)",
		(universal->pack_loadout & SLOT_PACK(_pack_loadout_secondary)) ? "second" : "first");
}
