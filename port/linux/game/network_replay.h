/*
NETWORK_REPLAY.H

The host's replay recording (network_replay.c): what a client that sees every player would be sent, written to a file
in the save root's replays folder instead. The file's format and its pure rules are ae_replay_rules.h's.
*/

#ifndef __NETWORK_REPLAY_H
#define __NETWORK_REPLAY_H

#include "ae_replay_rules.h"

struct observer_result;
struct network_game_client;
boolean network_replay_playback_idle(struct network_game_client *client);

/* the record kinds, as the netcode names them */
enum
{
	_network_replay_datagram = AE_REPLAY_RECORD_DATAGRAM,
	_network_replay_reliable = AE_REPLAY_RECORD_RELIABLE,
	_network_replay_reliable_to_machine = AE_REPLAY_RECORD_RELIABLE_TO_MACHINE,
	_network_replay_game_message = AE_REPLAY_RECORD_GAME_MESSAGE,
	_network_replay_game = AE_REPLAY_RECORD_GAME,
	_network_replay_own_actions = AE_REPLAY_RECORD_OWN_ACTIONS,
	_network_replay_level_setup = AE_REPLAY_RECORD_LEVEL_SETUP,
	NUMBER_OF_NETWORK_REPLAY_RECORDS = AE_REPLAY_RECORD_KINDS
};

/* (the host) whether a replay is being recorded */
boolean network_replay_recording(void);
/* (the host) once a tick, before anything is sent: starts the recording at a multiplayer game's first tick when
network.record_replays is on, and hands what was recorded to the writer every few seconds */
void network_replay_host_tick(void);
/* (the host) a record of the kind, at this tick, when recording */
void network_replay_record(byte kind, void const *data, long size);
/* (the host) a message of the game's own sent every machine in the game, when recording */
void network_replay_record_game_message(void const *message, long size);
/* the recording's end: the map is unloaded, or a new world starts */
void network_replay_stop(void);

/* the replays folder (the save root's replays), into path */
void network_replay_folder(char *path, long size);
/* the recordings in the folder, newest first, at most `maximum` names of AE_REPLAY_NAME_SIZE bytes; how many */
long network_replay_list(char (*names)[AE_REPLAY_NAME_SIZE], long maximum);
/* the recording of that name in the folder: its header read (an AE_REPLAY_HEADER_* answer), its size, and why it
cannot be played in a line */
int network_replay_describe(char const *name, struct ae_replay_header *header, unsigned long long *bytes, char *why,
	long why_size);
/* a recording of the folder removed (its KEPT marker too); 1 when it was there and is gone */
int network_replay_delete(char const *name);
/* a recording KEPT (on) or not: a kept one is never removed to make room for a new one, nor counted against the caps. 1 when done */
int network_replay_keep(char const *name, int on);
int network_replay_kept(char const *name);
/* (DEV MODE `replay stage <0-8>`) the test recordings the command made removed, then that many made: a header and no records, so they list
(75, 150, ... seconds long, on a day far back) and refuse to play; the number made, -1 when one could not be */
long network_replay_stage(long count);
/* (DEV MODE `replay keep`) the recording being written is kept even if it ends under 15 seconds; FALSE when none is written */
boolean network_replay_keep_short(void);
/* the recording being written now (empty when none), and the game thread's cost so far */
void network_replay_status(char *name, long size, struct ae_replay_cost *cost);


/* ---------- the playback (network_replay_playback.c) */

/* the recording of that name played: FALSE and why in a line if it cannot be (every record checked first) */
boolean network_replay_play(char const *name, char *why, long why_size);
/* whether a replay is being played (loading or playing) */
boolean network_replay_playing(void);
/* the playback's view for window 0, NULL when none */
struct observer_result const *network_replay_camera(void);
/* the view: a player's eyes (their name, lower case, or their number 1-9; NULL or "" the one watched), or the free
camera placed behind and above the one watched (with no unit in the game yet, by the map's first starting location);
FALSE when there is no such player. A player not in the game yet, or dead, is watched once they are: the view held where
it was till then */
boolean network_replay_view_eyes(char const *player);
void network_replay_view_free(void);
/* the player watched through their eyes (whose arms and weapon local player 0's view draws), NONE in the free camera
or with none alive */
long network_replay_eyes_player(void);
/* (render_objects.c) whether an object is left out of window 0's frame: the body of the player watched through their
eyes, as a player's own in first person */
boolean network_replay_hides_object(long object_index);
void network_replay_set_paused(boolean paused);
/* 25 to 400 */
void network_replay_set_speed(long percent);
/* (a playback strip's) paused; the speed asked for (100 when none plays); the free camera on, not a player's eyes */
boolean network_replay_paused(void);
long network_replay_speed_percent(void);
boolean network_replay_view_is_free(void);
/* the view behind the player watched, turned with them (the right stick flies it round them, the left one's up and down zooms
it), and the cinematic one: a tripod placed behind them that frames them and follows them when they go on. Y goes round the
four (the next one's name and the present one's, in title case or capitals, for the Theater's lines) */
void network_replay_view_behind(void);
void network_replay_view_cinematic(void);
void network_replay_view_next(void);
char const *network_replay_view_name(boolean next, boolean capitals);
/* the view being watched as the camera bar words it: 0 behind, 1 eyes, 2 free camera, 3 cinematic (-1 when none plays); and
whether the free camera tracks the player now (the stick click toggles it) */
int network_replay_camera_view(void);
boolean network_replay_camera_tracking(void);
long network_replay_position_seconds(void);
long network_replay_length_seconds(void);
/* to a second of the game: FALSE when not playing */
boolean network_replay_seek(long seconds);
/* (game_time.c) the ticks a frame runs: while a seek plays, up to 60 and none past its second */
long network_replay_ticks_per_frame(long ticks);
/* whether a seek plays to its second now */
boolean network_replay_seeking(void);
/* while one does (also while the replay loads again for a seek back): its second and how much of the way is played (0 to 1) */
boolean network_replay_seek_progress(long *target_seconds, float *progress);
/* (game_engine.c) whether the game's scoreboard stays shut: a replay's pause card or seek card is over the view */
boolean network_replay_scores_hidden(void);
/* back to the menus */
void network_replay_end(void);
/* once a frame (ae_hooks.c, from the main loop): the pad's and keyboard's controls of a playing replay: Start pauses and resumes (Back too),
B hides the lines over the screen (leaves, in the pause), the shoulders change the speed, the d-pad skips ten seconds, A and X pick the next
and the previous player, Y the next view (behind, eyes, the free camera, cinematic) */
void network_replay_playback_frame(void);
/* whether B hid the lines over the screen */
boolean network_replay_lines_hidden(void);
/* the player watched through their eyes: their number from 1 (0 in the free camera or with none alive; the view is behind, their eyes or cinematic) and, in name, their name in capitals */
long network_replay_watched_number(char *name, long size);
/* a line saying what plays, where, through whose eyes, at what speed */
void network_replay_playback_status(char *text, long size);
/* a replay stopped since the last ask: the menus open the Theater again */
boolean network_replay_theater_wanted(void);
/* (the netcode's hooks) */
boolean network_replay_playback_starting(void);
void network_replay_playback_started(void);
void network_replay_playback_tick(void);
void network_replay_playback_stop(void);

#endif
