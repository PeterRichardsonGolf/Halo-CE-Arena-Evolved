/*
AE_RADAR_RULES.H

The gametype's PLAYERS ON RADAR value ENEMIES (game_engine.h's
_radar_players_enemies, 3; owner 2026-10-15: the AE gametypes' motion sensor
shows enemies, not allies), as pure rules (no engine includes:
port/linux/tests/ae_radar_rules_test.c runs them). The engine side is
ae_radar.c's.

ENEMIES hides, on the viewer's motion sensor, every contact CE's blip colours
call an ally (motion_sensor.c's blip_type_get): a player of his team, and a
vehicle whose gunner (else driver) is of his team. An empty vehicle shows as
before (CE draws it in the friendly vehicle colour; it is nobody's ally), the
viewer's own unit as before (CE draws none). In a free for all game every
other player is an enemy: ENEMIES plays as ALL. The variant's draw object in
motion sensor bit still decides first (ENEMIES sets it, as ALL and FRIENDS do).

Players' names over their heads follow the motion tracker
(hud.c's hud_draw_player_names): under ENEMIES allies' names still show, as
under ALL (hiding them would give nothing away; the triangles stay too).

The value travels in the PC options (playlist_profile.c's block, the network
game's variant_options): stored 0, 1 and 2 keep their meaning. A stock build
reads a saved 3 as NONE (its PIN), and over the network keeps it unread: its
filter knows only FRIENDS, so a stock joiner's sensor shows ALL players.
*/

#ifndef __AE_RADAR_RULES_H
#define __AE_RADAR_RULES_H

/* a contact's side as CE's blip colours tell it */
enum ae_radar_contact
{
	AE_RADAR_CONTACT_ENEMY = 0,
	AE_RADAR_CONTACT_ALLY,
	/* (a vehicle with nobody in its driver's or gunner's seat) */
	AE_RADAR_CONTACT_EMPTY_VEHICLE,
};

/* whether a contact the gametype's motion sensor bit lets through still shows
under PLAYERS ON RADAR ENEMIES: enemies_only, the gametype's value is ENEMIES;
team_game, a team game; viewer_known, the motion sensor's viewer is known (the
shared scan of all local players has none: each one's own pass filters);
names, for a player's name over his head (allies' show) */
int ae_radar_rules_shows(int enemies_only, int team_game, int viewer_known, int names, enum ae_radar_contact contact);

#endif
