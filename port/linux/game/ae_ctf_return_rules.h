/*
AE_CTF_RETURN_RULES.H

FLAG RETURN's pure rules (ae_ctf_return.c plays them; the gametype's byte is ae_variant_ext's ctf_return,
AE_EXT_CTF_RETURN_*):
- CE: Halo CE's own: a dropped flag goes home 17 s after its last carrier let it go; a defender's touch sends it home.
- H2 TIMER: the same, 30 s by default, but the time stands still while an enemy is by the flag.
- STAND-BY: a defender's touch does nothing; the flag goes home after 30 s by default, faster with defenders standing by
  it (each of the first two adds AE_CTF_RETURN_BOOST ticks a tick), and the count stands still while an enemy is by it.
In all three, FLAG RESET TIME (ctf_reset_time) replaces the default time, and a pickup starts the count again.
*/

#ifndef AE_CTF_RETURN_RULES_H
#define AE_CTF_RETURN_RULES_H

#define AE_CTF_RETURN_RADIUS 2.5f
#define AE_CTF_RETURN_BOOST 4
#define AE_CTF_RETURN_DEFENDERS_MOST 2

/* seconds a loose flag lies in that mode: reset_time (FLAG RESET TIME) when set, else CE 17, the others 30; an
unknown mode is CE */
long ae_ctf_return_seconds(int mode, unsigned char reset_time);

/* a tick's count toward the time, with that many defenders and enemies by the flag (CE: 0, its own clock runs) */
long ae_ctf_return_step(int mode, int defenders, int enemies);

/* whether a defender's touch sends the flag home */
int ae_ctf_return_touch_returns(int mode);

/* FLAG PICKUP (ctf_flags' AE_EXT_CTF_HOLD_PICKUP): whether a player of player_team takes the flag of flag_team by the
hold (the weapon pick-up's hold of the action button), not by touch: an enemy's flag only; a flag of no team (below 0)
is Halo CE's */
int ae_ctf_hold_pickup(unsigned char ctf_flags, int flag_team, int player_team);

/* SUDDEN DEATH's grace (once time is up, each tick): whether the game goes on, with the objective in play now (a flag
carried, or an enemy by a flag) or in play within grace_ticks before now; *last_in_play is the tick it last was (-1
never; a now before it is a new game and forgets it) */
int ae_sudden_death_hold(int in_play, long now, long *last_in_play, long grace_ticks);

#endif
