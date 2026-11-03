/*
AE_RIFLE_MENU.C

BATTLE RIFLE in the gametype editor (ae_rifle_menu.h): the last choice of ITEM OPTIONS' PRIMARY and SECONDARY
starting weapons (a custom loadout). Stored as the gametype's weapon pack and which loadout slots the pack fills; a
slot the pack fills keeps an assault rifle as its own weapon, the one played where the pack is not (a machine with
WEAPON PACKS off, a map the pack cannot be added to).
*/

#include <string.h>

#include "cseries.h"
#include "cseries/cseries_windows.h"
#include "game/game_engine.h"
#include "interface/ui_widget.h"
#include "interface/ui_widget_instance.h"

#include "ae_rifle_menu.h"
#include "ae_weapon_slots.h"

void platform_log(char const *format, ...);

/* ITEM OPTIONS' two rows of a custom loadout; BATTLE RIFLE is the choice after the game's own weapons */
#define SPINNER_PRIMARY "primary_weapon_spinner"
#define SPINNER_SECONDARY "secondary_weapon_spinner"
#define PACK_CHOICE NUMBER_OF_LOADOUT_WEAPONS
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

/* one slot's row: shown from the gametype, or (save) written to it; the row's own code has run before this (it
knows the game's weapons only: the pack's choice it took for the last of them) */
static void slot_row(struct widget_instance *spinner, struct universal_variant *universal, byte *weapon, short slot,
	boolean custom, boolean save)
{
	if (!spinner)
		return;
	if (!save)
	{
		/* (the choice is the battle rifle's: the slot's weapon the BR's pack, ae_weapon_slots.h) */
		if (game_variant_pack_loadout_weapon_slot(universal->weapon_pack, universal->pack_loadout, universal->flags, universal->ball_melee,
			slot) ==
			_weapon_pack_battle_rifle)
		{
			spinner->parameters.list.selected_index = PACK_CHOICE;
		}
		return;
	}
	/* (a slot that names another pack keeps it unless the BR's choice is taken: the editor has no rows for the
	other packs yet, and their slot shows the weapon played without the pack) */
	{
		short named = game_variant_pack_loadout_weapon_slot(universal->weapon_pack, universal->pack_loadout,
			universal->flags, universal->ball_melee, slot);

		if (named != _weapon_pack_none && named != _weapon_pack_battle_rifle &&
			!(custom && spinner->parameters.list.selected_index == PACK_CHOICE))
		{
			return;
		}
	}
	ae_weapon_slots_rank_set(&universal->pack_loadout, &universal->flags, &universal->ball_melee, (unsigned int)slot, 0);
	if (custom && spinner->parameters.list.selected_index == PACK_CHOICE)
	{
		/* (the slot's own weapon is what is played where the pack is not: an assault rifle) */
		*weapon = _loadout_weapon_assault_rifle;
		universal->pack_loadout |= SLOT_PACK(slot);
	}
}

void ae_rifle_menu_rows(struct widget_instance *list, struct game_variant *variant, struct game_variant_options *options,
	boolean save)
{
	struct widget_instance *primary = spinner_named(list, SPINNER_PRIMARY);
	struct widget_instance *secondary = spinner_named(list, SPINNER_SECONDARY);
	struct universal_variant *universal = variant ? &variant->universal_variant : NULL;
	boolean custom;

	/* (only the screen with the loadout's rows: ITEM OPTIONS) */
	if (!universal || !options || (!primary && !secondary))
		return;
	custom = options->loadout == _loadout_custom;
	slot_row(primary, universal, &options->primary_weapon, _pack_loadout_primary, custom, save);
	slot_row(secondary, universal, &options->secondary_weapon, _pack_loadout_secondary, custom, save);
	if (!save)
		return;
	/* (the gametype uses the BR's pack while a slot, or the map's spawns, take its weapon; the other packs it
	names, ae_weapon_slots.h, are kept: a gametype naming them has no editor rows yet, and a choice here sets the
	BR's bit alone) */
	if (game_variant_weapon_pack(universal->weapon_pack) & ~_weapon_pack_battle_rifle)
		universal->weapon_pack = (byte)(game_variant_weapon_pack(universal->weapon_pack) |
			((universal->pack_loadout & SLOTS_MASK) ? _weapon_pack_battle_rifle : 0));
	else
		universal->weapon_pack = (universal->pack_loadout & (SLOTS_MASK | PACK_MAP_SPAWNS_BIT)) ?
			_weapon_pack_battle_rifle : _weapon_pack_none;
	platform_log("menus: the gametype's battle rifle: primary %s, secondary %s",
		(universal->pack_loadout & SLOT_PACK(_pack_loadout_primary)) ? "pack" : "stock",
		(universal->pack_loadout & SLOT_PACK(_pack_loadout_secondary)) ? "pack" : "stock");
}
