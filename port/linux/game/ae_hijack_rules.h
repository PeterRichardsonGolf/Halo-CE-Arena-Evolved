/*
AE_HIJACK_RULES.H

HIJACK's pure rules (ae_hijack.c plays them; the gametype's byte is ae_variant_ext's hijack, AE_EXT_HIJACK_*): the
action button by the driver's seat of a slow vehicle with a player driving throws the driver out and takes the seat.
OFF: Halo CE's (a player's seat is never offered). ENEMIES: an enemy's (every other player in free for all). ANYONE: any
other player's. An enemy riding in another seat keeps a player out of a free one unless MIXED-TEAM VEHICLES is ON (the
gametype's mixed_vehicles) or a hijack is taking the seat.
*/

#ifndef AE_HIJACK_RULES_H
#define AE_HIJACK_RULES_H

/* HIJACK is held from 0.8.0: with 0 the option is hidden, a saved gametype's byte plays as OFF and no hijack happens.
Set to 1 to bring HIJACK back */
#define AE_HIJACK_SHIPPED 0

/* the thrown driver is kept out of every seat this long */
#define AE_HIJACK_LOCKOUT_TICKS 45
/* the most the hijacker and the vehicle may move apart, world units a second */
#define AE_HIJACK_SPEED_MOST 3.5f
/* the throw: world units a second, to the side away from the hijacker, and up */
#define AE_HIJACK_THROW_SIDE 3.0f
#define AE_HIJACK_THROW_UP 2.0f

/* whether that mode lets a player throw out a driver: same_player the driver is the hijacker, enemies they are
enemies; an unknown mode is OFF */
int ae_hijack_allowed(int mode, int same_player, int enemies);

/* whether the hijacker and the vehicle move slowly enough apart (world units a second) */
int ae_hijack_speed_ok(float relative_speed);

/* whether an enemy riding in another seat no longer keeps a player out of a free one: MIXED-TEAM VEHICLES is ON
(mixed_mode, AE_EXT_MIXED_VEHICLES_*), or a hijack is taking the driver's seat (taking_seat; an explicit act, whatever
MIXED-TEAM VEHICLES says); an unknown mode is OFF */
int ae_hijack_mixed_riders(int mixed_mode, int taking_seat);

/* (units.c's unit_find_nearby_seat) a seat's rank in the choice: hijack_seat a driver seat that can be hijacked, which ranks
as available_state, so the nearer of it and a free seat wins; else seat_state */
int ae_hijack_seat_rank(int seat_state, int hijack_seat, int available_state);

#endif
