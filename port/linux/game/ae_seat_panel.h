/*
AE_SEAT_PANEL.H

THE SEAT PANEL, HUD STYLE AE's, for a driver: who rides, their shields and health, and the open seats, in the ammo panel's place
(ae_panels.c draws it). The rules are ae_seat_panel_rules.c's.
*/

#ifndef __AE_SEAT_PANEL_H
#define __AE_SEAT_PANEL_H

/* the vehicle whose panel this view draws: the viewer drives it and it has other seats; NONE if not */
long ae_seat_panel_vehicle(short local_player_index);
/* (hud_unit.c) whether this unit's HUD is another rider's seat's, whose turn the panel takes */
boolean ae_seat_panel_takes_hud(long hud_unit_index, long own_unit_index);
/* how many lines it shows: a seat each, or with more than three seats, the open seats of one kind in one line (GUNNER OPEN, PASSENGER OPEN x6) */
short ae_seat_panel_count(long vehicle_index);
/* the panel, from left and top, and its box as x0, y0, x1, y1: it holds its place, not drawn, while an open wheel would touch it */
void ae_seat_panel_draw(short local_player_index, long vehicle_index, real left, real top, real scale, real view_width, real box[4]);

#endif
