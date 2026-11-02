/*
AE_PAD_PORTS.H

Which controller port each connected pad has. A pad keeps its port while it is
connected; a new pad takes the first free port. SDL lists HIDAPI pads (Nintendo
Switch controllers) before kernel-driver ones (DualSense, 8BitDo), so a pad
that appeared late went in front of the others and moved every later pad to the
next port: a joined player's controller became another's, and the new pad's
START hit a player who was already playing (ae_platform.c gathers the pads,
xinput_sdl.c's sdl_gamepads reads the ports).
*/

#ifndef AE_PAD_PORTS_H
#define AE_PAD_PORTS_H

#define AE_PAD_PORTS_MAXIMUM 8

/* port_ids: each port's pad (its SDL id; 0 none), kept from call to call and changed in place. ids: the pads
connected now, in SDL's order, and at_rest: whether each has no button held and its sticks and triggers at rest.
split: two or more players on this machine (pc_menu_split_players); lone_first: debug.lone_pad_first.
- a pad gone frees its port;
- a new pad takes the first free port; while split, port 1 last (it is the keyboard's player's, and a pad that joins
  presses START on its own port);
- one player: when port 1 has no pad, the pad of the lowest port moves there once at rest (a pad drives the menus);
- split with one pad, on port 1: it moves to port 2 once at rest (its own controller, not the keyboard's player's;
  not with lone_first). A pad moves only at rest: a button held across a move would be pressed again on the other
  port.
Returns how many pads changed port or took one (0: nothing changed). */
int ae_pad_ports_assign(unsigned long *port_ids, int ports, unsigned long const *ids, unsigned char const *at_rest,
	int count, int split, int lone_first);

#endif
