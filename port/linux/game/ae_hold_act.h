/*
AE_HOLD_ACT.H

HOLD TO ACT (ae_hold_act.c): the action button HELD to get in a vehicle, take a seat from someone or flip it, as the
second game does it; a tap reloads (owner 2026-11-01). Halo CE got in on a tap and already held for a weapon.
*/

#ifndef __AE_HOLD_ACT_H
#define __AE_HOLD_ACT_H

/* players.c's update (AE hook), each player's action this tick before it is acted on: with the rule on and a vehicle
action offered, a tap becomes a reload and the hold (the weapon swap's flag, held past the map's
minimum_weapon_swap_ticks) acts, once a hold */
void ae_hold_act_control(long player_index, unsigned long *control_flags);
/* hud_messaging.c (AE hook): a state message's text; the vehicle prompts' "Press " reads "Hold " with the rule on
(message_index: the state message's index in the HUD messages, NONE for another message) */
wchar_t const *ae_hold_act_prompt(long message_index, wchar_t const *text);
/* player_control.c's input (AE hook), each local player's poll: the action button's ticks held and the map's ticks a hold takes
(minimum_weapon_swap_ticks); the item prompt's ring round the button follows it */
void ae_hold_act_note_hold(short local_player_index, long ticks_held, long ticks_for_hold);
/* the ring's filled segments (0 to 6) for a local player: the hold so far of the hold it takes; 0 with the button up */
int ae_hold_act_ring_segments(short local_player_index);
/* whether the running game asks for the hold (VEHICLES DESTRUCTIBLE on): the AE HUD's own item prompt says "Hold" then */
int ae_hold_act_active(void);

#endif
