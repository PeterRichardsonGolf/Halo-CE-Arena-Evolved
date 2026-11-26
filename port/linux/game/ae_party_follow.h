/*
AE_PARTY_FOLLOW.H

The glue that carries out what the party service says (ae_party_follow_rules.h): once a frame (ae_screen_postgame_tick) it joins the
server a QUICK PLAY found, or the leader's hosted game, from the main menu; and the post-game's CONTINUE of a party asks the
service for the next game. Does nothing while network.party_service is empty. Log prefix "ae party:". Menus' thread only.
*/

#ifndef __AE_PARTY_FOLLOW_H
#define __AE_PARTY_FOLLOW_H

/* once a frame */
void ae_party_follow_tick(void);
/* the post-game's CONTINUE on a playlist: 1 when the party carries on by itself (a leader asked the service for the next game on the
playlist, a member waits for the leader's: the caller opens no lobby of its own), 0 when this PC is in no online party */
int ae_party_follow_continue(const char *playlist);
/* the player pressed something in the party UI: an automatic follow paused after failures may go on */
void ae_party_follow_resume(void);
/* the follow's state as one line (DEV MODE `party online status`): "idle", "joining play.example:10660", the last stop's reason */
void ae_party_follow_status(char *text, int size);

#endif
