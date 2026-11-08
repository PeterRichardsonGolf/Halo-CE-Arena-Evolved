/*
AE_SCREEN_RECORD.H

SERVICE RECORD (0.7.6): the page of what this PC has seen of one player over the games it played (ae_service_record.h):
games, wins and the win rate, kills, deaths, assists and K/D, the same by the kind of game, and the medals by kind. On
one's own record it lists one's rivals with the kills each way; on a rival's it adds the line of one against them. It is this
PC's view only: another PC may have seen more or less of the same player.

Two doors: a player's card on the post-game report (SERVICE RECORD, ae_screen_pgcr.c) and the PROFILES list (the right
shoulder on a profile, ae_screen_profiles.c). B leaves.

Log: "ae record: page <name> (<n> games)" and "ae record: page closed".
*/

#ifndef __AE_SCREEN_RECORD_H
#define __AE_SCREEN_RECORD_H

/* opens the page of the player called name over the stack; viewer is the local player whose rivals count (a profile's
name; may be empty). 0 when the stack is full */
int ae_screen_record_open(short owner, char const *name, char const *viewer);

#endif
