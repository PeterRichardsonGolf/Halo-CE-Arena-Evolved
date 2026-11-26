/*
AE_SCREEN_THEATER.H

The THEATER screen (ae_screen_theater.c): the recordings of the network games this PC recorded (network_replay.c), newest first, a row each with
its map, day and length. A watches the one chosen (network_replay.h's playback: its game loads and the menus close), X deletes it under a
dialog, Y turns recording (network.record_replays: from the next game on) on or off. On the right, what the recording is: the map, the day,
the game type, how long, how large, who recorded it and with what build, or why it cannot be played. With none, the screen says how to
make one. The playback's end brings the Theater back (network_replay_theater_wanted).
*/

#ifndef __AE_SCREEN_THEATER_H
#define __AE_SCREEN_THEATER_H

/* opens THEATER over the stack, the newest recording focused; 0 when the stack is full */
int ae_screen_theater_open(void);
/* (DEV MODE `menu theater [empty] [delete]`) the same; empty: the screen as when the folder holds none (nothing is removed); delete: the
delete dialog open on the first recording (none when there is none) */
int ae_screen_theater_open_staged(int empty, int delete_dialog);
/* the Theater's screen closed, wherever it is (a replay's game left the menus) */
void ae_screen_theater_close(void);

#endif
