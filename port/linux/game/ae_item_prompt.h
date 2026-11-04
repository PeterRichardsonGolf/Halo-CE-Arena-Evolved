/*
AE_ITEM_PROMPT.H

The ITEM PROMPT on HUD STYLE AE (owner-approved design; rules in ae_item_prompt_rules.h): the prompt of an item to pick up or swap for, a
vehicle to enter or flip, a device, and the weapon to switch to when this one is empty. The game's line for them ("Hold X to swap for
<picture>") is replaced:

- one view: a block under the reticle, the name in capitals over the action ("Hold [chip] to swap"), sized by WAYPOINT SIZE, its top
  fixed 93 s under the view's middle so a weapon's reticle never moves it, centred on the axis. It fades in over 3 ticks, holds 4 and
  fades over 4 when it stops being true, and is gone the tick the item is taken.
- two to four views: one line, "NAME Hold [chip] to swap", the first line of the message list under the ammo counter, at the list's
  text size, never wrapping.

The Classic HUD keeps the game's line. Ammunition and powerups have no prompt (they are taken on touch); respawn, the scores and help
lines stay in the state line.
*/

#ifndef __AE_ITEM_PROMPT_H
#define __AE_ITEM_PROMPT_H

#include "ae_pure_types.h"

/* (hud.c, hud_show_action_response, before its switch; an AE hook) the view's player has an item, vehicle, flip or device to act on:
the prompt is set for this tick and TRUE comes back, the game's state line left empty (one view) or reserved for the split line.
FALSE where the game's line is to be set (Classic, help text up, another kind of action, no picture of the button) */
boolean ae_item_prompt_take(short local_player_index, long player_index);
/* (hud.c, where the game would remind the player to switch to a weapon with rounds) the same for that weapon object */
boolean ae_item_prompt_switch(short local_player_index, long weapon_object_index);
/* (hud.c, with the HUD's other AE blocks) one view's block under the reticle; nothing in two or more views */
void ae_item_prompt_draw(short local_player_index);
/* (ae_hud_messages.c, the first line of the list) two or more views' line, on the open block's pen */
void ae_item_prompt_split_draw(short local_player_index);
/* (the aim prompt keeps clear of it) the box of the last frames' block in the view's pixels; FALSE when none was drawn */
boolean ae_item_prompt_box(short local_player_index, float box[4]);

#endif /* __AE_ITEM_PROMPT_H */
