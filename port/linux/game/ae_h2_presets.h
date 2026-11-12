/* AE's NEW MODES preset gametypes in play: the table is ae_h2_presets_rules.c's. arena_gametypes.c builds each preset
as its engine's builder and calls ae_h2_preset_apply; a chosen gametype's AE block comes from the table by its stored
name (an AE hook in player_ui_set_game_variant) and from the built-in file for the editor. */
#ifndef AE_H2_PRESETS_H
#define AE_H2_PRESETS_H

#include "cseries.h"
#include "game/game_engine.h"
#include "ae_h2_presets_rules.h"

/* the preset's rules over its engine's builder's variant and the options made from it */
void ae_h2_preset_apply(int index, struct game_variant *variant, struct game_variant_options *options);
/* the stored name of the AE seed a preset keeps the rules of (NULL for none), and whether it takes the vehicles out */
char const *ae_h2_preset_base(int index);
int ae_h2_preset_no_vehicles(int index);
/* player_ui_set_game_variant: the chosen gametype's AE block, a preset's or the default */
void ae_h2_preset_choose_variant(struct game_variant const *variant);
/* playlist_profile.c: the AE block of a built-in preset's file (written after the options), by profile number
within the built-in AE modes; the block of a default file otherwise cleared */
void ae_h2_preset_file_block(struct game_variant const *variant, unsigned char *block);

#endif
