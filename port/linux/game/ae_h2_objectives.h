/* The second-generation game's objective carrier rules (the AE block, ae_variant_ext_rules.h): the juggernaut's and
carriers' speed, the ball's waypoint, and the seats a carrier may take. Nothing changes with the default block. */
#ifndef AE_H2_OBJECTIVES_H
#define AE_H2_OBJECTIVES_H

/* the oddball engine's speed for a ball carrier (the juggernaut's JUGGERNAUT MOVEMENT): 0 when the gametype's own */
float ae_h2_oddball_speed(long player_index);
/* the ball's waypoint: 1 when BALL WAYPOINT hides it now (held: a player carries it) */
int ae_h2_ball_waypoint_hidden(int held);
/* unit_can_enter_seat: 1 when the unit's objective keeps it out of that seat (a ball carrier out of every
vehicle with CARRIER IN VEHICLES off; a flag or bomb carrier out of a driver's seat with DRIVE WITH off) */
int ae_h2_seat_refused(long unit_index, long target_unit_index, short seat_index);
/* a flag or bomb carrier's speed (SLOW CARRIER): the multiplier, 1 when not slowed */
float ae_h2_carrier_speed(long player_index);

/* CTF: a loose flag's return time (FLAG RESET TIME, else the game's), and whether a team sees its own flag now
(OWN FLAG WAYPOINT: -1 the game's own rule, which shows it to the enemy only) */
long ae_h2_ctf_return_ticks(long default_ticks);
int ae_h2_ctf_own_flag_shows(int away);
/* the time limit waits while an objective is in play (SUDDEN DEATH): a flag or the bomb carried, the bomb armed */
int ae_h2_sudden_death_holds(void);
/* a melee blow by an objective's carrier kills (FLAG MELEE KILLS, BOMB MELEE KILLS) */
int ae_h2_objective_melee_lethal(long damaging_player_index);

/* the bomb (Bomb engine): STICKY ARMING keeps the arming when the carrier steps out; BOMB TOUCH RETURN sends a loose
bomb back when a player who may not carry it (a defender) touches it away from its spawn (the host; 1: reset it);
ENEMY BOMB INDICATOR narrows the markers the defenders see (the carrier's and the loose bomb's goals) */
int ae_h2_bomb_sticky(void);
int ae_h2_bomb_touch_returns(long unit_index, long weapon_index);
void ae_h2_bomb_indicator(short carrier_enemies_goal, short bomb_goal, int planted);

#endif
