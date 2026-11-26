/*
NETWORK_REPLAY_PLAYBACK.C

A replay's playback: the game a host recorded (network_replay.c), watched through any player's eyes or from a free
camera.

It plays as a client of the distributed netcode does, with no player of its own and no host. The whole file is read
and its records inflated into memory first, and every record is checked (ae_replay_stream_check) before any is
played: a file that fails is refused with one line, and none of its bytes reaches a handler. Then the global client is
made one joined to the game the replay's first record holds (network_game_client_port_replay_begin), which loads it as
a client loads the game it joins; its clock starts at the replay's first tick, and before each tick the records of the
ticks reached go to what a client hands its host's messages to: the distributed netcode's
(network_distributed_handle_message, the datagrams marked unreliable and the rest reliable, as they came) and the
game's own (network_game_client_handle_message). The distributed netcode's messages pass through a denylist: every
message a client takes from its host is handed over (to handlers written for a hostile host) but those of map and
package sharing, voice, the map vote, MCC admission and loading, the server's admin pages, the map check and aim assist
(network_distributed_replay_allows); of the game's own, an allowlist: only a player joining or leaving, the game's
update and its end (playback_game_message_allowed). The client sends
nothing (network_game_client_write).

The view (network_replay_camera, window 0's: main.c) is the watched player's eyes (where their unit's camera is,
looking where it looks), a free camera placed behind and above them, the view behind them or Content Evolved's cinematic
camera (ae_replay_views.inc). Speed (game_time_set_speed), pause, a seek
(ahead: played to at speed; back: the replay begun again and played to it) and the end, back to the menus, are the
DEV MODE replay command's (ae_dev_cmd.c). Medals and service records are not counted while it plays
(network_replay_playing).
*/

#include "cseries.h"
#include "cseries/errors.h"
#include "game/game.h"
#include "game/game_engine.h"
#include "game/players.h"
#include "main/main.h"
#include "camera/observer.h"
#include "networking/network_game_globals.h"
#include "networking/network_game_manager.h"
#include "networking/network_client_manager.h"
#include "networking/network_client_message_handler.h"
#include "bungie_net/common/message_header.h"
#include "bungie_net/network/transport.h"
#include "networking/network_messages.h"
#include "networking/network_connection.h"
#include "cache/cache_files.h"
#include "objects/objects.h"
#include "render/render.h"
#include "physics/collisions.h"
#include "scenario/scenario.h"
#include "scenario/scenario_definitions.h"
#include "units/units.h"
#include "units/unit_definitions.h"
#include "network_distributed.h"
#include "network_replay.h"
#include "ae_weapon_cache.h"
#include "ae_theater_rules.h"
#include "input/input.h"
#include "sound/sound_manager.h"
#include "halo_port_limits.h"
#include "../src/posix.h"
#include "cseries/cseries_windows.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "zlib.h"

#undef fopen

void platform_log(char const *format, ...);
void network_distributed_handle_message(long machine_index, word const *message, word size);
void network_distributed_replay_facings(byte const *data, long size, boolean own_actions,
	void (*found)(void *context, short player_index, long tick, short yaw, short pitch), void *context);
real distributed_angle_unpack(short value, boolean signed_angle);
const char *platform_data_root(void);
boolean network_distributed_replay_allows(word const *message, word size);
boolean network_game_client_port_game_check(struct network_game *game);
short ui_widget_port_multiplayer_maps(char const *const **names, short *last_used);
struct game_input_state *input_abstraction_get_input_state(short local_player_index);
int halo_linux_mouse_look(short gamepad_index, float *yaw, float *pitch);
int ae_seats_keyboard_controller(void);
float main_get_seconds_elapsed(void);
void first_person_weapon_port_state(short local_player_index, long *unit_index, long *weapon_index, boolean *visible);
short ui_widget_port_gametypes(long *indices, short maximum, short *last_used);
void network_connection_port_set_last_read_unreliable(boolean unreliable);
boolean network_game_client_port_replay_begin(struct network_game const *game);
boolean network_game_client_port_in_postgame(struct network_game_client *client);
void network_game_abort(void);
boolean create_global_network_game_client(void);
void dispose_global_network_game_client(void);
void dispose_global_network_game_server(void);

/* the level setup record's bytes (network_replay.c) */
#define PLAYBACK_SETUP_SIZE AE_REPLAY_SETUP_SIZE
/* the most of a replay's records held in memory: the format's most, half that in a 32-bit build */
#define PLAYBACK_INFLATED_MOST (sizeof(void *) > 4 ? (unsigned long)AE_REPLAY_INFLATED_MOST : \
	(unsigned long)AE_REPLAY_INFLATED_MOST / 2)
/* the most records handed over in one feed: the rest go in the next (a file whose records all carry one tick plays
on, slowly, instead of stopping the game while they all go) */
#define PLAYBACK_RECORDS_A_FEED 4096
/* how long the playback goes on after its last record, before it ends (ticks) */
#define PLAYBACK_END_TICKS (2 * TICKS_PER_SECOND)
/* a seek's speed (Content Evolved's REPLAY_SKIP_SPEED: the game played to the second asked for at 50 times), the most
ticks it runs in a frame (REPLAY_SKIP_TICKS_PER_FRAME); the sound is paused while it goes, as Content Evolved's skip does */
#define PLAYBACK_SEEK_SPEED 50.0f
#define PLAYBACK_SEEK_TICKS_PER_FRAME 60
/* the least time between two presses of the stick click that places the camera again (milliseconds) */
#define PLAYBACK_CAMERA_RESET_MILLISECONDS 150
/* the free camera's eased move and the walls it keeps off (Content Evolved's network_replay_playback.c, FREE_CAMERA_*):
how its speed follows the stick's tilt (the tilt to this power, from still to full in the first time, to still from full
in the second, seconds), how long a trigger held takes its rise to full, what it does not fly through (the level, its
scenery and machines), how far off them it keeps, and how many walls a frame's move slides along */
#define PLAYBACK_FREE_MOVE_POWER 2.0f
#define PLAYBACK_FREE_ACCELERATION_TIME 0.2f
#define PLAYBACK_FREE_BRAKE_TIME 0.08f
#define PLAYBACK_FREE_RISE_TIME 0.25f
#define PLAYBACK_FREE_COLLISION_FLAGS (FLAG(_collision_test_front_facing_surfaces_bit) | \
	FLAG(_collision_test_structure_bit) | FLAG(_collision_test_objects_bit) | \
	FLAG(_collision_test_objects_scenery_bit) | FLAG(_collision_test_objects_machines_bit))
#define PLAYBACK_FREE_WALL_MARGIN 0.15f
#define PLAYBACK_FREE_SLIDES 3
/* the eyes' glide over the host's corrections (CE's EYES_CORRECTION_*): the least a frame's move may differ from the
player's speed's to be one, the most (more is a teleport, taken at once), and how fast what is left of one goes (seconds
of the game to go to a third) */
#define PLAYBACK_EYES_CORRECTION_MINIMUM 0.02f
#define PLAYBACK_EYES_CORRECTION_MAXIMUM 2.0f
#define PLAYBACK_EYES_CORRECTION_TIME 0.1f
/* the eyes view's aim (Content Evolved's EYES_FACING_*): the most ticks between two of a player's facings it turns smoothly
across (a far player's come every second to sixth tick, one lost between them doubling that), and the most the facing may be
from the unit's aim and be the view's (radians: more, and it is not the unit's: a dead player's, a respawn's) */
#define PLAYBACK_FACING_MAXIMUM_TICKS 13
#define PLAYBACK_FACING_MAXIMUM_TURN (_pi / 2.0f)
#define PLAYBACK_FIELD_OF_VIEW 1.2217305f
#define PLAYBACK_PATH_SIZE 1024

/* the views Y goes round (Content Evolved's order) */
enum
{
	_playback_view_behind,
	_playback_view_eyes,
	_playback_view_free,
	_playback_view_cinematic,
	NUMBER_OF_PLAYBACK_VIEWS
};

/* a player's facing as the client had it: the tick the record that brought it was read at, and its yaw and pitch as the wire has them */
struct playback_facing
{
	long tick;
	short yaw;
	short pitch;
};

static struct
{
	/* every player's facings through the replay (playback_index_facings), each player's (by absolute index) together, in the
	order they were read: the first of them and how many */
	struct playback_facing *facings;
	long facing_first[HALO_PORT_MAXIMUM_NETWORK_PLAYERS];
	long facing_count[HALO_PORT_MAXIMUM_NETWORK_PLAYERS];
	/* whether a player's facings ever change: a player whose input is the host's (a bot's is not relayed as one) has facings that
	never do, and the unit's aim is theirs */
	boolean facing_live[HALO_PORT_MAXIMUM_NETWORK_PLAYERS];
	/* the records, inflated, and what the check found in them */
	byte *records;
	unsigned long size;
	struct ae_replay_stream stream;
	struct ae_replay_header header;
	char name[AE_REPLAY_NAME_SIZE];
	/* where the next record is */
	unsigned long next;
	boolean starting;
	boolean playing;
	boolean setting_up;
	boolean synchronized;
	boolean leaving;
	boolean paused;
	long speed_percent;
	/* a seek's tick (NONE: none), played to at the seek's speed, and whether the sound is paused for it */
	long seek_tick;
	boolean seek_muted;
	/* the tick the last record was played at, and the end's */
	long last_fed_tick;
	/* the view: whose eyes (a player's absolute index, NONE: the first with a unit), or the free camera's place */
	short view;
	short watched;
	real_point3d free_position;
	struct observer_result camera;
	boolean camera_valid;
	/* the watched unit's eyes: the unit, the last tick its aim was read on and the aim read then; the turn drawn (from
	the aim drawn when the last new aim came to it, over as many ticks as came between the two before: the recording
	holds a player's aim as the host sent it, a new one every few ticks), and the aim the last frame drew */
	long eyes_unit;
	long eyes_tick;
	real_vector3d eyes_aim_seen;
	real_vector3d eyes_aim_from;
	real_vector3d eyes_aim_to;
	long eyes_aim_tick;
	long eyes_aim_span;
	real_vector3d eyes_aim_drawn;
	real_vector3d eyes_aim_raw;
	/* the eyes' place, glided over the host's corrections of where the player is (playback_eyes_glide): the unit it was
	last worked out for, the moment of the game (in ticks) and the place drawn then, and what is left of the corrections */
	long glide_unit;
	real glide_time;
	real_point3d glide_drawn;
	real_vector3d glide_offset;
	/* the free camera's move as eased (playback_free_camera_speed: right, forward; playback_free_camera_rise) */
	real free_move_x;
	real free_move_y;
	real free_rise;
	/* behind (ae_replay_views.inc): flown round the player (the right stick touched since the view was chosen), its pitch
	then, the left stick's zoom (a scale of the distance behind), the turn it follows the player's facing at and whether
	that has started from it */
	boolean behind_orbiting;
	boolean behind_started;
	real behind_pitch;
	real behind_zoom;
	real yaw;
	/* the cinematic camera, as Content Evolved's free camera tracking has it: chosen and not yet placed, tracking (always),
	not moved since placed (zooming on the player as they go further), and its aim, view's width and framing with their
	spring's speeds; where the player is (kept once they die) */
	boolean free_placing;
	/* the time (system_milliseconds) the free or cinematic camera was last placed again by the pad */
	unsigned long free_reset_at;
	boolean free_tracking;
	boolean free_still;
	boolean free_focus_valid;
	real_point3d free_focus;
	real free_yaw;
	real free_pitch;
	real free_yaw_speed;
	real free_pitch_speed;
	real free_field_of_view_speed;
	real free_field_of_view;
	real free_frame_yaw;
	real free_frame_yaw_speed;
	real free_frame_pitch;
	real free_frame_pitch_speed;
	/* ... how long the player has been hidden from it and in sight of it since (seconds), how long this shot has lasted,
	whether they have died since it was placed, and what is left of the time after they spawned */
	real cinematic_hidden;
	real cinematic_clear;
	real cinematic_shot;
	boolean cinematic_died;
	real cinematic_spawned;
	/* ... its moving (playback_cinematic_move): whether it is, a tripod otherwise; the tether's spring's speed; the player's
	velocity smoothed (valid once it has one), its spring's speeds and their focus the frame before; where they were when it
	last stood still; how long they have gone slower than PLAYBACK_CINEMATIC_STILL_SPEED; how much of their speed it goes with
	them; the left stick's scale of the distance it follows them at; and the arc: its spring's speed, how long they have moved
	or turned and not, whether it is going, to which side of behind them, and their aim's yaw the frame before */
	boolean cinematic_moving;
	real cinematic_tether_speed;
	real_vector3d cinematic_velocity;
	real_vector3d cinematic_velocity_speed;
	boolean cinematic_velocity_valid;
	real_point3d cinematic_last_focus;
	real_point3d cinematic_anchor;
	real cinematic_slow;
	real cinematic_with;
	real cinematic_distance;
	real cinematic_orbit_speed;
	real cinematic_orbit_moving;
	real cinematic_orbit_still;
	boolean cinematic_orbiting;
	real cinematic_orbit_side;
	real cinematic_orbit_aim;
	boolean cinematic_orbit_aim_valid;
	/* ... whoever last hurt the player watched: the tick of the hit (NONE, none), the player watched then, that player, their
	object if they have one and where it last was; and the view's width it wants (0, none) */
	long threat_tick;
	long threat_watched;
	long threat_player;
	long threat_object;
	real_point3d threat_point;
	real threat_field_of_view;
	/* for the status line: frames drawn through moving eyes and those of them the view did not move in; frames drawn
	while the aim turned and those of them it did not turn in; ticks the aim was read on and those it was new on */
	unsigned long eyes_moving_frames;
	unsigned long eyes_still_frames;
	unsigned long eyes_turning_frames;
	unsigned long eyes_unturned_frames;
	/* ... and those the aim as recorded (the view taken as the last tick left it) did not turn in */
	unsigned long eyes_raw_unturned_frames;
	unsigned long eyes_ticks;
	unsigned long eyes_turns;
	/* the message handed over, aligned for the handlers */
	word message[(AE_REPLAY_RECORD_MOST + 1) / sizeof(word) + 1];
} playback = { NULL };

/* the playback's pace and the eyes' turn, measured for the replay's end line: the ticks run in each half second of real time
(only at the speed 1 and unpaused, where 15 is steady), and the turn of the eyes' aim from the frame before to this one
(radians; a frame where the player turned and the aim did not is the stepping a player sees as janky) */
#define PLAYBACK_PACE_WINDOWS 2400
#define PLAYBACK_AIM_FRAMES 40000
static struct
{
	unsigned long window_start;
	long window_ticks;
	boolean window_valid;
	long windows[PLAYBACK_PACE_WINDOWS];
	long window_count;
	float aim[PLAYBACK_AIM_FRAMES];
	long aim_count;
	/* frames the eyes' aim was the player's smooth facing, and those it was the unit's */
	long facing_frames;
	long unit_frames;
	/* frames the moment drawn (the tick and the fraction into it) was the one the frame before drew */
	long same_moment_frames;
	real last_moment;
} playback_pace;

static int playback_compare_long(const void *a, const void *b)
{
	long x = *(const long *)a, y = *(const long *)b;

	return x < y ? -1 : x > y;
}

static int playback_compare_float(const void *a, const void *b)
{
	float x = *(const float *)a, y = *(const float *)b;

	return x < y ? -1 : x > y;
}

/* a frame: the window of half a second closes, counted only if the replay ran at its speed 1 all through it */
static void playback_pace_frame(void)
{
	unsigned long now = system_milliseconds();
	boolean steady = !playback.paused && playback.seek_tick == NONE && playback.speed_percent == 100 && !playback.starting;

	if (!steady)
	{
		playback_pace.window_valid = FALSE;
		playback_pace.window_ticks = 0;
		playback_pace.window_start = now;
		return;
	}
	if (!playback_pace.window_valid)
	{
		playback_pace.window_valid = TRUE;
		playback_pace.window_ticks = 0;
		playback_pace.window_start = now;
	}
	else if (now - playback_pace.window_start >= 500)
	{
		if (playback_pace.window_count < PLAYBACK_PACE_WINDOWS)
			playback_pace.windows[playback_pace.window_count++] = playback_pace.window_ticks;
		playback_pace.window_ticks = 0;
		playback_pace.window_start = now;
	}
}

static void playback_pace_report(void)
{
	if (playback_pace.window_count > 0)
	{
		long count = playback_pace.window_count, burst = 0, stall = 0, index;

		qsort(playback_pace.windows, (size_t)count, sizeof(long), playback_compare_long);
		for (index = 0; index < count; index++)
		{
			burst += playback_pace.windows[index] > 17;
			stall += playback_pace.windows[index] < 13;
		}
		platform_log("replay: pace: %ld half seconds at 1x, ticks in each: min %ld, median %ld, max %ld (15 is steady); %ld over 17, %ld under 13",
			count, playback_pace.windows[0], playback_pace.windows[count / 2], playback_pace.windows[count - 1], burst, stall);
	}
	if (playback_pace.aim_count > 0)
	{
		long count = playback_pace.aim_count, zero = 0, index;

		qsort(playback_pace.aim, (size_t)count, sizeof(float), playback_compare_float);
		for (index = 0; index < count && playback_pace.aim[index] == 0.0f; index++)
			zero++;
		platform_log("replay: eyes' aim from the facings on %ld frames, from the unit on %ld; turn per frame over %ld frames, milliradians: "
			"median %.2f, 95th %.2f, max %.2f; %ld frames did not turn at all; %ld frames drew the moment the frame before drew",
			playback_pace.facing_frames, playback_pace.unit_frames, count, (double)playback_pace.aim[count / 2] * 1000.0, (double)playback_pace.aim[count * 95 / 100] * 1000.0,
			(double)playback_pace.aim[count - 1] * 1000.0, zero, playback_pace.same_moment_frames);
	}
	csmemset(&playback_pace, 0, sizeof(playback_pace));
}

/* a replay stopped: for the menus to open the Theater again */
static boolean playback_theater_wanted;
/* the lines over the screen hidden (B while playing: the controls below) */
static boolean playback_lines_hidden;

/* the menus' multiplayer map name while a replay plays (the replay's map's is set, as a client joining a game has it
set, for the scoreboard's header and the postgame's name): put back when it stops */
static struct
{
	boolean saved;
	char map_name[256];
} playback_menus;

static void playback_menus_restore(
	void)
{
	if (playback_menus.saved)
		main_set_multiplayer_map_name(playback_menus.map_name);
	playback_menus.saved = FALSE;
}

/* the main menu's LAST GAME card's map and game type (CREATE GAME's last picks), as a replay found them: a replay
must leave them as they were, which its end logs */
static struct
{
	boolean noted;
	short map;
	short gametype;
} playback_last_game = { FALSE, NONE, NONE };

static void playback_last_game_read(
	short *map,
	short *gametype)
{
	char const *const *names = NULL;
	long gametypes[64];

	*map = NONE;
	*gametype = NONE;
	ui_widget_port_multiplayer_maps(&names, map);
	ui_widget_port_gametypes(gametypes, (short)NUMBEROF(gametypes), gametype);
}

static void playback_free(
	void)
{
	if (playback.seek_muted)
		sound_pause(FALSE);
	if (playback.records)
		free(playback.records);
	if (playback.facings)
		free(playback.facings);
	csmemset(&playback, 0, sizeof(playback));
	playback.seek_tick = NONE;
	playback.watched = NONE;
	playback.eyes_unit = NONE;
	playback.threat_tick = NONE;
	playback.cinematic_distance = 1.0f;
	playback.behind_zoom = 1.0f;
}

/* the file's bytes after its header, inflated into playback.records; FALSE and why if not */
static boolean playback_inflate(
	FILE *file,
	char *why,
	long why_size)
{
	z_stream stream;
	byte input[0x10000];
	unsigned long capacity = 0x100000;
	int result = Z_OK;

	csmemset(&stream, 0, sizeof(stream));
	if (!(playback.records = malloc(capacity)))
	{
		snprintf(why, (size_t)why_size, "there is not memory enough to play it");
		return FALSE;
	}
	if (inflateInit(&stream) != Z_OK)
	{
		snprintf(why, (size_t)why_size, "there is not memory enough to play it");
		return FALSE;
	}
	playback.size = 0;
	while (result != Z_STREAM_END)
	{
		size_t read = fread(input, 1, sizeof(input), file);

		if (!read)
			break;
		stream.next_in = input;
		stream.avail_in = (uInt)read;
		while (stream.avail_in && result != Z_STREAM_END)
		{
			if (capacity - playback.size < 0x10000)
			{
				byte *grown;

				if (capacity >= PLAYBACK_INFLATED_MOST ||
					!(grown = realloc(playback.records, MIN(capacity * 2, PLAYBACK_INFLATED_MOST))))
				{
					inflateEnd(&stream);
					snprintf(why, (size_t)why_size, "it is too large to play");
					return FALSE;
				}
				playback.records = grown;
				capacity = MIN(capacity * 2, PLAYBACK_INFLATED_MOST);
			}
			stream.next_out = playback.records + playback.size;
			stream.avail_out = (uInt)(capacity - playback.size);
			result = inflate(&stream, Z_SYNC_FLUSH);
			playback.size = capacity - stream.avail_out;
			/* (a file cut short, or damaged past its last sync: played to what inflated) */
			if (result != Z_OK && result != Z_STREAM_END && result != Z_BUF_ERROR)
			{
				platform_log("replay: its stream ends damaged after %lu bytes: played to there", playback.size);
				result = Z_STREAM_END;
			}
		}
	}
	inflateEnd(&stream);
	return TRUE;
}

/* the players' facings through the whole replay, as the client read them: the host's relayed input (a far player's every second to
sixth tick), for the eyes view to turn smoothly between (playback_eyes_facing). Counted, then kept; a facing older than one the
player already had is dropped, as the client drops it, and a second one read at the same tick takes the first's place (Content
Evolved's playback_index_facings, over this file's records) */
struct playback_facing_scan
{
	boolean counting;
	long tick;
	long counts[HALO_PORT_MAXIMUM_NETWORK_PLAYERS];
	long newest[HALO_PORT_MAXIMUM_NETWORK_PLAYERS];
	boolean any[HALO_PORT_MAXIMUM_NETWORK_PLAYERS];
};

static void playback_facing_found(
	void *context,
	short player_index,
	long tick,
	short yaw,
	short pitch)
{
	struct playback_facing_scan *scan = (struct playback_facing_scan *)context;
	struct playback_facing *facing;
	long count;

	if (player_index < 0 || player_index >= HALO_PORT_MAXIMUM_NETWORK_PLAYERS)
		return;
	if (scan->any[player_index] && tick <= scan->newest[player_index])
		return;
	scan->any[player_index] = TRUE;
	scan->newest[player_index] = tick;
	if (scan->counting)
	{
		scan->counts[player_index]++;
		return;
	}
	count = playback.facing_count[player_index];
	facing = playback.facings + playback.facing_first[player_index] + count;
	if (count > 0 && facing[-1].tick == scan->tick)
		facing--;
	else if (count < scan->counts[player_index])
		playback.facing_count[player_index]++;
	else
		return;
	facing->tick = scan->tick;
	facing->yaw = yaw;
	facing->pitch = pitch;
}

static void playback_scan_facings(
	struct playback_facing_scan *scan)
{
	unsigned long next = (unsigned long)playback.stream.play_from;

	csmemset(scan->newest, 0, sizeof(scan->newest));
	csmemset(scan->any, 0, sizeof(scan->any));
	while (next < (unsigned long)playback.stream.end)
	{
		struct ae_replay_record record;
		byte const *bytes = playback.records + next;

		/* (checked whole before this: a record here is one) */
		if (ae_replay_record_take(bytes, playback.stream.end - next, 0, 0xFFFFFFFFUL, &record) != AE_REPLAY_RECORD_OK)
			break;
		scan->tick = (long)record.tick;
		if (record.kind == _network_replay_datagram || record.kind == _network_replay_reliable ||
			record.kind == _network_replay_reliable_to_machine)
		{
			network_distributed_replay_facings(bytes + AE_REPLAY_RECORD_HEADER_SIZE, (long)record.size, FALSE,
				playback_facing_found, scan);
		}
		next += AE_REPLAY_RECORD_HEADER_SIZE + record.size;
	}
}

static void playback_index_facings(
	void)
{
	struct playback_facing_scan scan;
	long total = 0;
	short player_index;

	csmemset(&scan, 0, sizeof(scan));
	scan.counting = TRUE;
	playback_scan_facings(&scan);
	for (player_index = 0; player_index < HALO_PORT_MAXIMUM_NETWORK_PLAYERS; player_index++)
	{
		playback.facing_first[player_index] = total;
		playback.facing_count[player_index] = 0;
		total += scan.counts[player_index];
	}
	/* (none: the eyes view turns with the players' ticks) */
	if (!total || !(playback.facings = (struct playback_facing *)malloc((size_t)total * sizeof(struct playback_facing))))
		return;
	scan.counting = FALSE;
	playback_scan_facings(&scan);
	platform_log("replay: %ld facings of the players kept for the eyes view (%lu KB)", total,
		(unsigned long)(total * sizeof(struct playback_facing) / 1024));
	/* (each player's: how many of their facings differ from the one before, as a measure of how much they turn) */
	for (player_index = 0; player_index < HALO_PORT_MAXIMUM_NETWORK_PLAYERS; player_index++)
	{
		struct playback_facing const *facings = playback.facings + playback.facing_first[player_index];
		long index, changed = 0;

		for (index = 1; index < playback.facing_count[player_index]; index++)
			changed += facings[index].yaw != facings[index - 1].yaw || facings[index].pitch != facings[index - 1].pitch;
		playback.facing_live[player_index] = changed > 0;
		if (playback.facing_count[player_index])
			platform_log("replay: player %d: %ld facings, %ld turned from the one before", player_index + 1,
				playback.facing_count[player_index], changed);
	}
}

/* the recorded game record: the checks a host's settings record gets on a client (network_client_manager.c's, which
also ends its strings in their fields), and this file's own (ae_replay_rules: the counts, each machine's players, a
map name the maps folders may be searched for); FALSE and why if it fails one */
static boolean playback_game_check(
	struct network_game *game,
	char *why,
	long why_size)
{
	long machines[NUMBEROF(game->players)], controllers[NUMBEROF(game->players)];
	short index;

	if (!ae_replay_map_name_safe(game->map.name, sizeof(game->map.name)))
	{
		snprintf(why, (size_t)why_size, "its game's map name is not one the game loads");
		return FALSE;
	}
	if (!ae_replay_game_counts_check(game->machine_count, game->player_count, game->maximum_players,
		game->difficulty, HALO_PORT_MAXIMUM_NETWORK_MACHINES, HALO_PORT_MAXIMUM_NETWORK_PLAYERS, NUMBER_OF_GAME_DIFFICULTY_LEVELS,
		why, (size_t)why_size))
		return FALSE;
	for (index = 0; index < (short)NUMBEROF(game->players); index++)
	{
		machines[index] = game->players[index].machine_index;
		controllers[index] = game->players[index].controller_index;
	}
	if (!ae_replay_players_check(machines, controllers, (int)NUMBEROF(game->players), HALO_PORT_MAXIMUM_NETWORK_MACHINES,
		MAXIMUM_LOCAL_PLAYERS, why, (size_t)why_size))
		return FALSE;
	if (!network_game_client_port_game_check(game))
	{
		snprintf(why, (size_t)why_size, "its game record is damaged");
		return FALSE;
	}
	return TRUE;
}

/* the map the replay's game plays is on this machine; FALSE and why (with the map's name) if not */
static boolean playback_map_here(
	struct network_game const *game,
	byte const *setup,
	char *why,
	long why_size)
{
	char const *base = strrchr(game->map.name, '\\');
	char path[PLAYBACK_PATH_SIZE];
	char const *root = platform_data_root();
	struct posix_file_information information;
	static char const *const folders[] = { "maps", "custom_maps", "maps_dev", NULL };
	short index;

	base = base ? base + 1 : game->map.name;
	/* (never a path that leaves the maps folders: no '/', "..", ':' or device name, whoever calls this) */
	if (!ae_replay_map_name_safe(game->map.name, sizeof(game->map.name)))
	{
		snprintf(why, (size_t)why_size, "its game's map name is not one the game loads");
		return FALSE;
	}
	/* (a map of a pack, MCC's or the Workshop's: not played back yet. The level setup's byte 1 says MCC tags) */
	if (strchr(game->map.name, '@') || (setup && setup[1]))
	{
		snprintf(why, (size_t)why_size, "its map %s is a map pack's or MCC's, which the playback does not load", base);
		return FALSE;
	}
	/* (its weapons: the weapon cache it played with must be this machine's, or every pack weapon would be another) */
	if (setup && !ae_replay_setup_cache_matches(setup, ae_weapon_cache_serial(), ae_weapon_cache_id(), why,
		(size_t)why_size))
		return FALSE;
	for (index = 0; folders[index]; index++)
	{
		snprintf(path, sizeof(path), "%s/%s/%s.map", root ? root : ".", folders[index], base);
		if (!posix_stat(path, &information))
		{
			char build[0x20];

			/* (as a client joining checks its host's map: this machine's of the version recorded, of a build that
			plays with others) */
			if (!cache_files_map_present(game->map.name, (unsigned long)game->map.version))
			{
				snprintf(why, (size_t)why_size, "its map %s is another version than this machine's", base);
				return FALSE;
			}
			if (!cache_files_map_plays_multiplayer(game->map.name, build))
			{
				snprintf(why, (size_t)why_size, "its map %s is of a build that does not play with others", base);
				return FALSE;
			}
			return TRUE;
		}
	}
	snprintf(why, (size_t)why_size, "its map %s is not on this machine", base);
	return FALSE;
}

static boolean playback_play(
	char const *name,
	char *why,
	long why_size)
{
	char path[PLAYBACK_PATH_SIZE];
	unsigned char head[AE_REPLAY_HEADER_SIZE];
	struct posix_file_information information;
	struct network_game game;
	unsigned long long bytes;
	FILE *file;

	why[0] = 0;
	playback_free();
	if (!ae_replay_name_is_safe(name))
	{
		snprintf(why, (size_t)why_size, "no such recording");
		return FALSE;
	}
	network_replay_folder(path, sizeof(path));
	snprintf(path + strlen(path), sizeof(path) - strlen(path), "/%s", name);
	if (posix_stat(path, &information) != 0)
	{
		snprintf(why, (size_t)why_size, "no such recording");
		return FALSE;
	}
	bytes = (unsigned long long)information.size_high << 32 | information.size_low;
	if (bytes > AE_REPLAY_FILE_MOST)
	{
		snprintf(why, (size_t)why_size, "it is too large to play");
		return FALSE;
	}
	file = fopen(path, "rb");
	if (!file)
	{
		snprintf(why, (size_t)why_size, "it cannot be opened");
		return FALSE;
	}
	if (ae_replay_header_read(head, fread(head, 1, sizeof(head), file), HALO_PORT_NETWORK_VERSION, &playback.header,
		why, (size_t)why_size) != AE_REPLAY_HEADER_OK || !playback_inflate(file, why, why_size))
	{
		fclose(file);
		playback_free();
		return FALSE;
	}
	fclose(file);
	/* (every record checked before any is played) */
	if (!ae_replay_stream_check(playback.records, playback.size, playback.header.start_tick,
		sizeof(struct network_game), PLAYBACK_SETUP_SIZE, sizeof(message_header), DATAGRAM_MAXIMUM_SIZE,
		HALO_PORT_MAXIMUM_NETWORK_MESSAGE_SIZE, &playback.stream, why,
		(size_t)why_size))
	{
		playback_free();
		return FALSE;
	}
	playback_index_facings();
	csmemcpy(&game, playback.records + playback.stream.game_at, sizeof(game));
	if (!playback_game_check(&game, why, why_size) || !playback_map_here(&game, playback.stream.play_from > playback.stream.game_at + sizeof(game) ?
		playback.records + playback.stream.game_at + sizeof(game) + AE_REPLAY_RECORD_HEADER_SIZE : NULL, why, why_size))
	{
		playback_free();
		return FALSE;
	}
	playback.next = (unsigned long)playback.stream.play_from;
	snprintf(playback.name, sizeof(playback.name), "%s", name);
	playback.speed_percent = 100;
	/* (as a client joins a game: none of this machine's own network game is left) */
	playback.setting_up = TRUE;
	dispose_global_network_game_server();
	dispose_global_network_game_client();
	if (!create_global_network_game_client() || !network_game_client_port_replay_begin(&game))
	{
		snprintf(why, (size_t)why_size, "its game cannot be set up");
		dispose_global_network_game_client();
		playback.setting_up = FALSE;
		playback_free();
		return FALSE;
	}
	playback.setting_up = FALSE;
	playback.starting = TRUE;
	if (!playback_last_game.noted)
	{
		playback_last_game_read(&playback_last_game.map, &playback_last_game.gametype);
		playback_last_game.noted = TRUE;
	}
	if (!playback_menus.saved)
	{
		snprintf(playback_menus.map_name, sizeof(playback_menus.map_name), "%s", main_get_multiplayer_map_name());
		playback_menus.saved = TRUE;
	}
	main_set_multiplayer_map_name(game.map.name);
	{
		char const *last = strrchr(game.map.name, '\\');

		platform_log("replay: the scoreboard and the postgame name its map, %s (the menus' own put back when it stops)",
			last ? last + 1 : game.map.name);
	}
	platform_log("replay: playing %s (%s, %s), %lu records from tick %lu%s", name, game.map.name,
		playback.header.variant, playback.stream.records, playback.header.start_tick,
		playback.stream.cut ? ", cut short" : "");
	return TRUE;
}

boolean network_replay_play(
	char const *name,
	char *why,
	long why_size)
{
	boolean was_playing = network_replay_playing();

	if (playback_play(name, why, why_size))
		return TRUE;
	/* (a replay asked for over one playing, a seek back among them, that fails: the old one's records are gone, so its
	game is left too, back to the menus, not kept with the playback's hooks off) */
	if (was_playing)
	{
		platform_log("replay: %s refused (%s): the replay playing ends", name, why);
		/* (back to the Theater, as a replay that stops is) */
		playback_last_game.noted = FALSE;
		playback_theater_wanted = TRUE;
		playback_menus_restore();
		game_time_set_speed(1.0f);
		network_game_abort();
	}
	return FALSE;
}

boolean network_replay_playing(
	void)
{
	return playback.records != NULL && (playback.starting || playback.playing);
}

boolean network_replay_playback_starting(
	void)
{
	return playback.records != NULL && playback.starting;
}

void network_replay_playback_started(
	void)
{
	playback.starting = FALSE;
	playback.playing = TRUE;
	game_time_set_distributed((long)playback.header.start_tick);
	/* (a seek back begun it again: played to the seek's second at the seek's speed) */
	game_time_set_speed(playback.seek_tick != NONE ? PLAYBACK_SEEK_SPEED :
		playback.paused ? 0.0f : (real)playback.speed_percent / 100.0f);
	platform_log("replay: the game is loaded; playing from tick %lu", playback.header.start_tick);
}

/* whether a game message of this kind is one a playback hands over: a game in progress's (players joining and
leaving, the game's end), not the lobby's, the connection's or a new game's */
static boolean playback_game_message_allowed(
	word const *message,
	long size)
{
	byte type;

	if (size <= (long)sizeof(message_header) || GET_MESSAGE_SIZE(message[0]) != size ||
		GET_MESSAGE_TYPE(message[0]) != _message_type_packet)
	{
		return FALSE;
	}
	type = ((byte const *)message)[size - 1];
	return type == _message_server_add_player_ingame || type == _message_server_remove_player_ingame ||
		type == _message_server_game_over || type == _message_server_game_update;
}

/* the records of the ticks the clock has reached, to the client's handlers as it reads them from its host */
static void playback_feed(
	struct network_game_client *client)
{
	struct transport_address host;
	long fed = 0;

	csmemset(&host, 0, sizeof(host));
	while (playback.next < playback.stream.end && fed++ < PLAYBACK_RECORDS_A_FEED)
	{
		struct ae_replay_record record;
		byte const *bytes = playback.records + playback.next;

		/* (checked whole before playing: a record here is one) */
		if (ae_replay_record_take(bytes, playback.stream.end - playback.next, 0, 0xFFFFFFFFUL, &record) !=
			AE_REPLAY_RECORD_OK)
		{
			playback.next = (unsigned long)playback.stream.end;
			break;
		}
		if ((long)record.tick > game_time_get())
			break;
		/* (the game over: none of the records after it, the host's next pregame among them) */
		if (network_game_client_port_in_postgame(client))
			break;
		playback.next += AE_REPLAY_RECORD_HEADER_SIZE + record.size;
		playback.last_fed_tick = (long)record.tick;
		csmemcpy(playback.message, bytes + AE_REPLAY_RECORD_HEADER_SIZE, record.size);
		switch (record.kind)
		{
		case _network_replay_datagram:
		case _network_replay_reliable:
		case _network_replay_reliable_to_machine:
			network_connection_port_set_last_read_unreliable(record.kind == _network_replay_datagram);
			network_distributed_handle_message(NONE, playback.message, (word)record.size);
			break;
		case _network_replay_game_message:
			if (!playback_game_message_allowed(playback.message, (long)record.size))
				break;
			network_connection_port_set_last_read_unreliable(FALSE);
			network_game_client_handle_message(client, playback.message, (short)record.size, &host);
			break;
		}
	}
	network_connection_port_set_last_read_unreliable(FALSE);
}

void network_replay_playback_tick(
	void)
{
	struct network_game_client *client = global_network_game_client_get();

	if (playback.playing && client)
		playback_feed(client);
	playback_pace.window_ticks++;
}

/* the player of an absolute index (a seek back makes the players again: their indices stay, their datums' salts
need not); NONE: none */
static long playback_player_at(
	short absolute)
{
	struct data_iterator iterator;

	data_iterator_new(&iterator, player_data);
	while (data_iterator_next(&iterator) != NULL)
	{
		if (DATUM_INDEX_TO_ABSOLUTE_INDEX(iterator.datum_index) == absolute)
			return iterator.datum_index;
	}
	return NONE;
}

/* the player watched: the one asked for while they have a unit (NONE while they wait to spawn or to respawn), else
the first with a living unit; NONE when none has one */
static long playback_watched(
	void)
{
	struct data_iterator iterator;
	struct player_datum *player;

	if (playback.watched != NONE)
	{
		long player_index = playback_player_at(playback.watched);

		player = player_index != NONE ? player_try_and_get(player_index) : NULL;
		return player && player->unit_index != NONE ? player_index : NONE;
	}
	data_iterator_new(&iterator, player_data);
	while ((player = data_iterator_next(&iterator)) != NULL)
	{
		if (player->unit_index != NONE)
			return iterator.datum_index;
	}
	return NONE;
}

/* whether a unit rides in a seat its player sees from behind (the seat's third person camera, as the director has
it: a vehicle's driver or passenger) */
static boolean playback_rides_seen_from_behind(
	struct unit_datum const *unit)
{
	struct object_datum const *parent;
	struct unit_seat const *seat;
	struct unit_definition const *definition;

	if (!unit || unit->object.parent_object_index == NONE)
		return FALSE;
	parent = object_get(unit->object.parent_object_index);
	if (!TEST_FLAG(_object_mask_unit, parent->object.type))
		return FALSE;
	definition = unit_definition_get(parent->definition_index);
	if (unit->unit.parent_seat_index < 0 || unit->unit.parent_seat_index >= definition->unit.seats.count)
		return FALSE;
	seat = TAG_BLOCK_GET_ELEMENT(&definition->unit.seats, unit->unit.parent_seat_index, struct unit_seat);
	return TEST_FLAG(seat->flags, _unit_seat_third_person_camera_bit);
}

/* the player seen through their eyes: NONE in the free camera, with none watched, or while the one watched rides in a
seat seen from behind (their body drawn, no arms) */
long network_replay_eyes_player(
	void)
{
	long player_index;
	struct player_datum *player;

	if (!playback.playing || playback.view != _playback_view_eyes || !game_in_progress())
		return NONE;
	player_index = playback_watched();
	player = player_index != NONE ? player_try_and_get(player_index) : NULL;
	if (!player || player->unit_index == NONE || playback_rides_seen_from_behind(unit_try_and_get(player->unit_index)))
		return NONE;
	return player_index;
}

/* the game's input for a controller this frame, as input_abstraction.c keeps it (its controls bound by the player's
own settings, the keyboard's keys among them): twelve buttons (each the ticks held; jump the first, crouch the
eleventh), then the movement and look sticks */
struct playback_input
{
	byte buttons[12];
	real forward;
	real strafe;
	real yaw;
	real pitch;
};

#define PLAYBACK_INPUT_JUMP 0
#define PLAYBACK_INPUT_CROUCH 10
/* world units a second at full stick, radians a second at full look */
#define PLAYBACK_FREE_SPEED 6.0f
#define PLAYBACK_FREE_TURN 2.5f
#define PLAYBACK_FREE_PITCH_MOST 1.5f

/* the free camera's move from the movement stick's tilt (right, forward: -1 to 1): the tilt to PLAYBACK_FREE_MOVE_POWER,
which the move follows over the seconds, a full speed's change in PLAYBACK_FREE_ACCELERATION_TIME speeding up and in
PLAYBACK_FREE_BRAKE_TIME slowing down (Content Evolved's playback_free_camera_speed) */
static void playback_free_camera_speed(
	real move_x,
	real move_y,
	real seconds)
{
	real tilt = (real)sqrt(move_x * move_x + move_y * move_y);
	real target_x = 0.0f;
	real target_y = 0.0f;
	real difference_x;
	real difference_y;
	real difference;
	real step;

	if (tilt > 0.0f)
	{
		real speed = (real)pow(MIN(tilt, 1.0f), PLAYBACK_FREE_MOVE_POWER);

		target_x = move_x / tilt * speed;
		target_y = move_y / tilt * speed;
	}
	difference_x = target_x - playback.free_move_x;
	difference_y = target_y - playback.free_move_y;
	difference = (real)sqrt(difference_x * difference_x + difference_y * difference_y);
	step = seconds / (target_x * target_x + target_y * target_y >=
		playback.free_move_x * playback.free_move_x + playback.free_move_y * playback.free_move_y ?
		PLAYBACK_FREE_ACCELERATION_TIME : PLAYBACK_FREE_BRAKE_TIME);
	if (difference <= step)
	{
		playback.free_move_x = target_x;
		playback.free_move_y = target_y;
	}
	else
	{
		playback.free_move_x += difference_x / difference * step;
		playback.free_move_y += difference_y / difference * step;
	}
}

/* the free camera's rise (up 1, down -1): no more than a button held has taken it to, from none to full in
PLAYBACK_FREE_RISE_TIME; let go, or the other one, and it starts from none (CE's playback_free_camera_rise) */
static real playback_free_camera_rise(
	real rise,
	real seconds)
{
	real held;

	if (rise == 0.0f || (rise > 0.0f) != (playback.free_rise > 0.0f))
		playback.free_rise = 0.0f;
	if (rise == 0.0f)
		return 0.0f;
	held = MIN((real)fabs(playback.free_rise) + seconds / PLAYBACK_FREE_RISE_TIME, 1.0f);
	playback.free_rise = rise > 0.0f ? held : -held;
	return rise > 0.0f ? MIN(rise, held) : MAX(rise, -held);
}

/* the free camera moved by move, but never nearer a wall than PLAYBACK_FREE_WALL_MARGIN (further than the view's near
clip, so it never sees into one): each part of the move is looked along as far again as the margin, and goes only as far
as keeps it the margin off the wall it finds there, the rest of it along the wall, as far as the next. Only the walls facing
it stop it, so one that starts inside something can still leave it (CE's playback_free_camera_move) */
static void playback_free_camera_move(
	real_vector3d const *move)
{
	real_vector3d left = *move;
	short slide;

	for (slide = 0; slide < PLAYBACK_FREE_SLIDES; slide++)
	{
		struct collision_result collision;
		real_vector3d const *normal;
		real_vector3d direction;
		real_vector3d probe;
		real length = (real)sqrt(left.i * left.i + left.j * left.j + left.k * left.k);
		real approach;
		real distance;
		real travel;
		real across;

		if (length < 1.0e-4f)
			return;
		direction.i = left.i / length;
		direction.j = left.j / length;
		direction.k = left.k / length;
		probe.i = direction.i * (length + PLAYBACK_FREE_WALL_MARGIN);
		probe.j = direction.j * (length + PLAYBACK_FREE_WALL_MARGIN);
		probe.k = direction.k * (length + PLAYBACK_FREE_WALL_MARGIN);
		normal = &collision.plane.n;
		if (!collision_test_vector(PLAYBACK_FREE_COLLISION_FLAGS, &playback.free_position, &probe, NONE, &collision) ||
			(approach = -(direction.i * normal->i + direction.j * normal->j + direction.k * normal->k)) < 1.0e-4f)
		{
			playback.free_position.x += left.i;
			playback.free_position.y += left.j;
			playback.free_position.z += left.k;
			return;
		}
		/* (how far off the wall it is, and how far along the move it may go to be the margin off it: none if it is
		nearer already) */
		distance = (playback.free_position.x - collision.point.x) * normal->i +
			(playback.free_position.y - collision.point.y) * normal->j +
			(playback.free_position.z - collision.point.z) * normal->k;
		travel = PIN((distance - PLAYBACK_FREE_WALL_MARGIN) / approach, 0.0f, length);
		playback.free_position.x += direction.i * travel;
		playback.free_position.y += direction.j * travel;
		playback.free_position.z += direction.k * travel;
		/* (what is left of the move, along the wall: none of it into it) */
		left.i = direction.i * (length - travel);
		left.j = direction.j * (length - travel);
		left.k = direction.k * (length - travel);
		across = left.i * normal->i + left.j * normal->j + left.k * normal->k;
		if (across < 0.0f)
		{
			left.i -= normal->i * across;
			left.j -= normal->j * across;
			left.k -= normal->k * across;
		}
	}
}

/* frames drawn while playing (window 0's camera asked), and the last of them the watched player's body was left out of
(render_objects.c asks network_replay_hides_object), for the status line */
static unsigned long playback_body_hidden_frame, playback_frames;

/* the place a view takes with no unit to take it from: the map's first player starting location, at a standing
player's eyes, looking its way; FALSE (the origin) for a map with none */
#define PLAYBACK_EYE_HEIGHT 0.62f

static boolean playback_start_place(
	real_point3d *position,
	real_vector3d *forward)
{
	struct scenario *scenario = global_scenario_get();
	struct player_starting_location const *location;

	csmemset(position, 0, sizeof(*position));
	forward->i = 1.0f;
	forward->j = 0.0f;
	forward->k = 0.0f;
	if (!scenario || scenario->players.count <= 0)
		return FALSE;
	location = TAG_BLOCK_GET_ELEMENT(&scenario->players, 0, struct player_starting_location);
	*position = location->position;
	position->z += PLAYBACK_EYE_HEIGHT;
	forward->i = (real)cos(location->facing);
	forward->j = (real)sin(location->facing);
	return TRUE;
}

/* the watched unit's eyes as this frame draws them: where its camera is, moved with the unit to where the frame draws
it (render_interpolation_object_point), as the world around it is drawn. Its aim turns to each new aim the recording
holds over as many ticks as came between the last two (render_interpolation_fraction into the tick), from the aim
drawn when it came. (Taken as the last tick left it, the view stepped once a tick, and once every new aim, a few
ticks apart, while the world, the unit's body and weapon among it, glided.) */
#define PLAYBACK_AIM_SPAN_MOST 6

/* a rider's view in a seat with a third-person camera: the game's own (following_camera_unit_view: the seat's camera
track and the unit's aim), moved as the frame draws the vehicle and kept on this side of walls (Content Evolved's
playback_seat_camera) */
void following_camera_unit_view(long unit_index, real_point3d *focus, real_vector3d *forward, real *distance);

static void playback_seat_camera(
	long unit_index,
	struct observer_result *camera)
{
	long vehicle_index = object_get(unit_index)->object.parent_object_index;
	real_point3d focus;
	real_point3d origin;
	real_point3d drawn;
	real_vector3d forward;
	real_vector3d back;
	real distance;
	struct collision_result collision;

	following_camera_unit_view(unit_index, &focus, &forward, &distance);
	object_get_origin(vehicle_index, &origin);
	drawn = origin;
	render_interpolation_object_point(vehicle_index, &drawn);
	focus.x += drawn.x - origin.x;
	focus.y += drawn.y - origin.y;
	focus.z += drawn.z - origin.z;
	back.i = -forward.i * distance;
	back.j = -forward.j * distance;
	back.k = -forward.k * distance;
	if (collision_test_vector(FLAG(_collision_test_structure_bit) | FLAG(_collision_test_front_facing_surfaces_bit),
		&focus, &back, NONE, &collision))
	{
		real kept = collision.t * distance > PLAYBACK_FREE_WALL_MARGIN ?
			(collision.t * distance - PLAYBACK_FREE_WALL_MARGIN) / distance : 0.0f;

		back.i *= kept;
		back.j *= kept;
		back.k *= kept;
	}
	camera->position.x = focus.x + back.i;
	camera->position.y = focus.y + back.j;
	camera->position.z = focus.z + back.k;
	camera->forward = forward;
}

/* the eyes moved past the host's corrections of where the player is (Content Evolved's playback_eyes_glide): what a
frame's move differs from the player's speed (or the vehicle's they ride) by is a correction the client was sent, glided
over (PLAYBACK_EYES_CORRECTION_TIME) rather than jumped (a far player's come every second to sixth tick); a larger one, a
teleport or a respawn, is taken at once, as is any after another player or view, or a jump in the replay's time (a seek) */
static void playback_eyes_glide(
	long unit_index,
	real_point3d *eyes)
{
	real time = (real)game_time_get() - 1.0f + render_interpolation_fraction();
	real elapsed = time - playback.glide_time;
	struct object_datum *mover = object_get(object_get_ultimate_parent(unit_index));
	real_vector3d error;
	real distance, kept;

	if (playback.glide_unit != unit_index || elapsed < 0.0f || elapsed > 3.0f)
	{
		playback.glide_unit = unit_index;
		playback.glide_time = time;
		playback.glide_drawn = *eyes;
		csmemset(&playback.glide_offset, 0, sizeof(playback.glide_offset));
		return;
	}
	error.i = eyes->x - playback.glide_drawn.x - mover->object.translational_velocity.i * elapsed;
	error.j = eyes->y - playback.glide_drawn.y - mover->object.translational_velocity.j * elapsed;
	error.k = eyes->z - playback.glide_drawn.z - mover->object.translational_velocity.k * elapsed;
	distance = (real)sqrt(error.i * error.i + error.j * error.j + error.k * error.k);
	playback.glide_time = time;
	playback.glide_drawn = *eyes;
	if (distance >= PLAYBACK_EYES_CORRECTION_MAXIMUM)
		csmemset(&playback.glide_offset, 0, sizeof(playback.glide_offset));
	else if (distance > PLAYBACK_EYES_CORRECTION_MINIMUM)
	{
		playback.glide_offset.i -= error.i;
		playback.glide_offset.j -= error.j;
		playback.glide_offset.k -= error.k;
	}
	/* (to a third in PLAYBACK_EYES_CORRECTION_TIME of the game) */
	kept = (real)exp(-elapsed / (PLAYBACK_EYES_CORRECTION_TIME * TICKS_PER_SECOND));
	playback.glide_offset.i *= kept;
	playback.glide_offset.j *= kept;
	playback.glide_offset.k *= kept;
	eyes->x += playback.glide_offset.i;
	eyes->y += playback.glide_offset.j;
	eyes->z += playback.glide_offset.k;
}

/* the player's (an absolute index) facing at the moment, turned smoothly from the facing their ticks run to the next one the replay
has (playback_index_facings); FALSE when it has none of theirs yet. A facing read at a tick is what the client's next tick runs: it
is theirs from then (its tick and 1) (Content Evolved's playback_eyes_facing) */
static boolean playback_eyes_facing(
	short player_index,
	real time,
	real_euler_angles2d *facing)
{
	struct playback_facing const *facings;
	long low = 0;
	long high;
	long count;
	real from;
	real to;
	real fraction;

	if (player_index < 0 || player_index >= HALO_PORT_MAXIMUM_NETWORK_PLAYERS || !playback.facings ||
		!playback.facing_live[player_index])
		return FALSE;
	facings = playback.facings + playback.facing_first[player_index];
	count = playback.facing_count[player_index];
	if (!count)
		return FALSE;
	high = count;
	/* (the last whose moment the time has reached) */
	if ((real)(facings[0].tick + 1) > time)
		return FALSE;
	while (high - low > 1)
	{
		long middle = (low + high) / 2;

		if ((real)(facings[middle].tick + 1) <= time)
			low = middle;
		else
			high = middle;
	}
	facing->yaw = distributed_angle_unpack(facings[low].yaw, FALSE);
	facing->pitch = distributed_angle_unpack(facings[low].pitch, TRUE);
	if (low + 1 >= count || facings[low + 1].tick - facings[low].tick > PLAYBACK_FACING_MAXIMUM_TICKS)
		return TRUE;
	fraction = (time - (real)(facings[low].tick + 1)) / (real)(facings[low + 1].tick - facings[low].tick);
	fraction = PIN(fraction, 0.0f, 1.0f);
	from = facing->yaw;
	to = distributed_angle_unpack(facings[low + 1].yaw, FALSE);
	{
		real difference = (real)fmod(to - from, 2.0f * _pi);

		if (difference > _pi)
			difference -= 2.0f * _pi;
		else if (difference < -_pi)
			difference += 2.0f * _pi;
		facing->yaw = from + difference * fraction;
	}
	from = facing->pitch;
	to = distributed_angle_unpack(facings[low + 1].pitch, TRUE);
	facing->pitch = from + (to - from) * fraction;
	return TRUE;
}

/* where the player aims at the moment: their smooth facing (playback_eyes_facing), which is where a player looks, as their own view
is. Not the unit's aim: a unit turns toward its input's facing a little a tick, so it lags each new facing by ticks, and between a
far player's it would step and swing. The unit's aim where the replay has no facing of theirs, or theirs is far from it (dead, or the
facing not the unit's) (Content Evolved's playback_eyes_aim). TRUE when it is the facing */
static boolean playback_eyes_aim(
	long unit_index,
	short player_index,
	real time,
	real_vector3d *aim)
{
	struct unit_datum *unit = unit_get(unit_index);
	real_euler_angles2d smooth;
	real_vector3d facing;

	*aim = unit->unit.aiming_vector;
	if (!playback_eyes_facing(player_index, time, &smooth))
		return FALSE;
	facing.i = (real)(cos(smooth.pitch) * cos(smooth.yaw));
	facing.j = (real)(cos(smooth.pitch) * sin(smooth.yaw));
	facing.k = (real)sin(smooth.pitch);
	/* (more than PLAYBACK_FACING_MAXIMUM_TURN apart: cos 90 degrees) */
	if (facing.i * aim->i + facing.j * aim->j + facing.k * aim->k > (real)cos(PLAYBACK_FACING_MAXIMUM_TURN))
	{
		*aim = facing;
		return TRUE;
	}
	return FALSE;
}

static void playback_eyes_camera(
	long player_index,
	long unit_index,
	struct unit_datum const *unit,
	struct observer_result *camera)
{
	long now = game_time_get();
	real u;
	real_vector3d forward;
	real length;
	struct object_datum const *object = object_get(unit_index);
	real_point3d drawn_before = playback.camera.position;
	real_vector3d aim_before = playback.eyes_aim_drawn;
	real_vector3d raw_before = playback.eyes_aim_raw;
	real_vector3d const *looking = &unit->unit.looking_vector;
	boolean counted = playback.camera_valid && unit_index == playback.eyes_unit && game_time_get_speed() > 0.0f;

	if (unit_index != playback.eyes_unit)
	{
		/* (another unit: from its aim now, no turn from the one before's) */
		playback.eyes_unit = unit_index;
		playback.eyes_tick = now;
		playback.eyes_aim_seen = *looking;
		playback.eyes_aim_from = *looking;
		playback.eyes_aim_to = *looking;
		playback.eyes_aim_drawn = *looking;
		playback.eyes_aim_raw = *looking;
		playback.eyes_aim_tick = now;
		playback.eyes_aim_span = 1;
	}
	else if (now != playback.eyes_tick)
	{
		playback.eyes_tick = now;
		playback.eyes_ticks++;
		if (looking->i != playback.eyes_aim_seen.i || looking->j != playback.eyes_aim_seen.j ||
			looking->k != playback.eyes_aim_seen.k)
		{
			playback.eyes_turns++;
			playback.eyes_aim_span = PIN(now - playback.eyes_aim_tick, 1, PLAYBACK_AIM_SPAN_MOST);
			playback.eyes_aim_tick = now;
			playback.eyes_aim_seen = *looking;
			playback.eyes_aim_from = playback.eyes_aim_drawn;
			playback.eyes_aim_to = *looking;
		}
	}
	unit_get_camera_position(unit_index, &camera->position);
	render_interpolation_object_point(unit_index, &camera->position);
	playback_eyes_glide(unit_index, &camera->position);
	u = ((real)(now - playback.eyes_aim_tick) + render_interpolation_fraction()) / (real)playback.eyes_aim_span;
	u = PIN(u, 0.0f, 1.0f);
	forward.i = playback.eyes_aim_from.i + (playback.eyes_aim_to.i - playback.eyes_aim_from.i) * u;
	forward.j = playback.eyes_aim_from.j + (playback.eyes_aim_to.j - playback.eyes_aim_from.j) * u;
	forward.k = playback.eyes_aim_from.k + (playback.eyes_aim_to.k - playback.eyes_aim_from.k) * u;
	length = (real)sqrt(forward.i * forward.i + forward.j * forward.j + forward.k * forward.k);
	if (length > 0.001f)
	{
		forward.i /= length;
		forward.j /= length;
		forward.k /= length;
	}
	else
		forward = playback.eyes_aim_to;
	/* (the player's own facing, smooth, where the replay has it: Content Evolved's way; the turn over the unit's aims above is the
	fallback) */
	{
		real_vector3d facing_aim;
		real moment = (real)game_time_get() - 1.0f + render_interpolation_fraction();

		if (counted && moment == playback_pace.last_moment)
			playback_pace.same_moment_frames++;
		playback_pace.last_moment = moment;
		if (playback_eyes_aim(unit_index, (short)DATUM_INDEX_TO_ABSOLUTE_INDEX(player_index), moment, &facing_aim))
		{
			forward = facing_aim;
			playback_pace.facing_frames++;
		}
		else
		{
			playback_pace.unit_frames++;
			/* (the first misses, said once each with what they were: the moment, the player's first facing's tick) */
			if (playback_pace.unit_frames <= 3)
			{
				short absolute = (short)DATUM_INDEX_TO_ABSOLUTE_INDEX(player_index);

				platform_log("replay: no facing for player %d at %.2f: %ld kept, the first at tick %ld, the last at %ld",
					absolute + 1, (double)moment, playback.facing_count[absolute],
					playback.facing_count[absolute] ? playback.facings[playback.facing_first[absolute]].tick : -1L,
					playback.facing_count[absolute] ? playback.facings[playback.facing_first[absolute] +
					playback.facing_count[absolute] - 1].tick : -1L);
			}
		}
	}
	camera->forward = forward;
	if (counted && playback.seek_tick == NONE && playback.speed_percent == 100 && playback_pace.aim_count < PLAYBACK_AIM_FRAMES)
	{
		double a_i = playback.camera.forward.i, a_j = playback.camera.forward.j, a_k = playback.camera.forward.k;
		double b_i = forward.i, b_j = forward.j, b_k = forward.k;
		double c_i = a_j * b_k - a_k * b_j, c_j = a_k * b_i - a_i * b_k, c_k = a_i * b_j - a_j * b_i;

		/* (the angle between them from the cross product's length: acos of a dot product loses the small turns) */
		playback_pace.aim[playback_pace.aim_count++] = (float)atan2(sqrt(c_i * c_i + c_j * c_j + c_k * c_k),
			a_i * b_i + a_j * b_j + a_k * b_k);
	}
	playback.eyes_aim_drawn = forward;
	playback.eyes_aim_raw = *looking;
	/* (riding in a seat seen from behind: the view the game gives that seat's player, Content Evolved's
	playback_seat_camera) */
	if (playback_rides_seen_from_behind(unit))
		playback_seat_camera(unit_index, camera);
	/* (the status line's: frames drawn while the unit moves and those the view stood still in; frames drawn while a
	new aim is turned to and those the view did not turn in) */
	if (!counted)
		return;
	if (object->object.translational_velocity.i * object->object.translational_velocity.i +
		object->object.translational_velocity.j * object->object.translational_velocity.j > 0.0001f)
	{
		playback.eyes_moving_frames++;
		if (drawn_before.x == camera->position.x && drawn_before.y == camera->position.y &&
			drawn_before.z == camera->position.z)
		{
			playback.eyes_still_frames++;
		}
	}
	if (now - playback.eyes_aim_tick < playback.eyes_aim_span &&
		(playback.eyes_aim_from.i != playback.eyes_aim_to.i || playback.eyes_aim_from.j != playback.eyes_aim_to.j ||
		playback.eyes_aim_from.k != playback.eyes_aim_to.k))
	{
		playback.eyes_turning_frames++;
		if (aim_before.i == forward.i && aim_before.j == forward.j && aim_before.k == forward.k)
			playback.eyes_unturned_frames++;
		if (raw_before.i == looking->i && raw_before.j == looking->j && raw_before.k == looking->k)
			playback.eyes_raw_unturned_frames++;
	}
}

#include "ae_replay_views.inc"

/* the view for this frame (window 0's) */
static void playback_camera_make(
	void)
{
	long player_index = playback_watched();
	struct player_datum *player = player_index != NONE ? player_try_and_get(player_index) : NULL;
	struct unit_datum *unit = player && player->unit_index != NONE ? unit_try_and_get(player->unit_index) : NULL;
	struct observer_result camera;

	if (playback.view == _playback_view_eyes && !unit && playback.camera_valid)
	{
		/* (the player watched not in the game yet, or dead: the view held where it was till they are) */
		return;
	}
	if (playback.view != _playback_view_behind)
		playback.behind_started = FALSE;
	/* (behind, the free camera and cinematic: made as Content Evolved makes them, ae_replay_views.inc) */
	if ((playback.view == _playback_view_behind || playback.view == _playback_view_free ||
		playback.view == _playback_view_cinematic) && playback_views_camera(unit ? player->unit_index : NONE))
	{
		return;
	}
	csmemset(&camera, 0, sizeof(camera));
	camera.up.k = 1.0f;
	camera.field_of_view = PLAYBACK_FIELD_OF_VIEW;
	if (playback.view == _playback_view_eyes && unit)
		playback_eyes_camera(player_index, player->unit_index, unit, &camera);
	else
	{
		/* (no one in the game yet: where the map's first player starts, till someone is) */
		playback_start_place(&camera.position, &camera.forward);
		playback.eyes_unit = NONE;
	}
	if (camera.forward.i == 0.0f && camera.forward.j == 0.0f && camera.forward.k == 0.0f)
		camera.forward.i = 1.0f;
	scenario_location_from_point(&camera.location, &camera.position);
	playback.camera = camera;
	playback.camera_valid = TRUE;
}

struct observer_result const *network_replay_camera(
	void)
{
	if (!playback.playing || !game_in_progress())
		return NULL;
	playback_frames++;
	playback_pace_frame();
	playback_camera_make();
	return playback.camera_valid ? &playback.camera : NULL;
}

boolean network_replay_playback_idle(
	struct network_game_client *client)
{
	if (!playback.playing || playback.leaving)
		return TRUE;
	playback_feed(client);
	/* (the host's objects, all of them, came at the game's first tick: as a client is told once the host has sent it
	them all, its own objects the host has not are let go) */
	if (!playback.synchronized && game_in_progress())
	{
		playback.synchronized = TRUE;
		network_objects_handle_synchronized();
	}
	/* (a seek: played to at its speed, then at the speed asked) */
	if (playback.seek_tick != NONE && game_time_get() >= playback.seek_tick)
	{
		playback.seek_tick = NONE;
		if (playback.seek_muted)
		{
			sound_pause(FALSE);
			playback.seek_muted = FALSE;
		}
		game_time_set_speed(playback.paused ? 0.0f : (real)playback.speed_percent / 100.0f);
		platform_log("replay: at tick %ld", game_time_get());
	}
	/* (its end: the records played and a moment more, or the game over) */
	if ((playback.next >= playback.stream.end && game_time_get() > playback.last_fed_tick + PLAYBACK_END_TICKS) ||
		network_game_client_port_in_postgame(client))
	{
		platform_log("replay: %s ended at tick %ld", playback.name, game_time_get());
		playback.leaving = TRUE;
		network_game_abort();
	}
	return TRUE;
}

void network_replay_playback_stop(
	void)
{
	if (playback.setting_up)
		return;
	if (playback.records)
	{
		short map, gametype;

		playback_last_game_read(&map, &gametype);
		playback_pace_report();
		platform_log("replay: through the eyes, %lu of %lu frames drawn while the unit moved stood still, %lu of %lu "
			"drawn while its aim turned did not turn (as recorded, %lu would not have); a new aim on %lu of %lu ticks",
			playback.eyes_still_frames, playback.eyes_moving_frames, playback.eyes_unturned_frames,
			playback.eyes_turning_frames, playback.eyes_raw_unturned_frames, playback.eyes_turns, playback.eyes_ticks);
		platform_log("replay: stopped; the LAST GAME card's map and game type %s (map %d, game type %d; before %d, %d)",
			map == playback_last_game.map && gametype == playback_last_game.gametype ? "are as before" : "CHANGED",
			map, gametype, playback_last_game.map, playback_last_game.gametype);
		playback_last_game.noted = FALSE;
		playback_theater_wanted = TRUE;
		playback_lines_hidden = FALSE;
		playback_menus_restore();
		game_time_set_speed(1.0f);
	}
	playback_free();
}

boolean network_replay_theater_wanted(
	void)
{
	boolean wanted = playback_theater_wanted;

	playback_theater_wanted = FALSE;
	return wanted;
}

/* ---------- the controls on a pad or the keyboard (Content Evolved's map, as the keyboard's menu keys drive controller 0:
arrows are the d-pad, Space and Enter A, E X, Tab Y, C the right shoulder, Q the left, Escape and Backspace B, F1 Back) */

static struct
{
	/* every button's state at the last frame (a press is a change to down) */
	boolean held[NUMBER_OF_GAMEPAD_BUTTONS];
} playback_pad;

/* whether any controller's button went down since the last frame */
static boolean playback_pressed(
	short button)
{
	boolean down = FALSE;
	boolean pressed;
	short controller;

	for (controller = 0; controller < MAXIMUM_GAMEPADS; controller++)
	{
		struct gamepad_state const *gamepad = input_has_gamepad(controller) ? input_get_gamepad_state(controller) : NULL;

		down |= gamepad && gamepad->buttons[button] > 0;
	}
	pressed = down && !playback_pad.held[button];
	playback_pad.held[button] = down;
	return pressed;
}

/* the absolute indices of the players with a unit, ascending */
static int playback_players_with_units(
	int *players,
	int maximum)
{
	struct data_iterator iterator;
	struct player_datum *player;
	int count = 0, index;

	data_iterator_new(&iterator, player_data);
	while ((player = data_iterator_next(&iterator)) != NULL && count < maximum)
	{
		if (player->unit_index != NONE)
			players[count++] = (int)DATUM_INDEX_TO_ABSOLUTE_INDEX(iterator.datum_index);
	}
	/* (the data's order is the slots': ascending, but sorted here so the rule's list is whatever it is) */
	for (index = 1; index < count; index++)
	{
		int value = players[index], before = index - 1;

		while (before >= 0 && players[before] > value)
		{
			players[before + 1] = players[before];
			before--;
		}
		players[before + 1] = value;
	}
	return count;
}

/* the eyes of the next player with a unit (or the one before), as A and X do */
static void playback_step_player(
	boolean backwards)
{
	int players[HALO_PORT_MAXIMUM_NETWORK_PLAYERS];
	int count = playback_players_with_units(players, HALO_PORT_MAXIMUM_NETWORK_PLAYERS);
	long player_index = playback_watched();
	int current = player_index != NONE ? (int)DATUM_INDEX_TO_ABSOLUTE_INDEX(player_index) : -1;
	int next = ae_theater_next_player(players, count, current, backwards);

	if (next < 0)
		return;
	playback.watched = (short)next;
	platform_log("replay: player %d watched", next + 1);
}

/* the seek a skip asks for: from the second the playback is at, or, while one is on its way, from where that goes (taps add up) */
static void playback_skip(
	int direction)
{
	long from = playback.seek_tick != NONE ?
		(playback.seek_tick - (long)playback.header.start_tick) / TICKS_PER_SECOND : network_replay_position_seconds();

	if (!network_replay_seek(ae_theater_skip_target(from, direction, network_replay_length_seconds())))
		platform_log("replay: a skip was refused");
}

void network_replay_playback_frame(
	void)
{
	if (!playback.playing || playback.starting || playback.leaving || !game_in_progress())
	{
		csmemset(playback_pad.held, 0, sizeof(playback_pad.held));
		return;
	}
	/* (Start, or Back on a keyboard, which has no Start: pause and resume; in the pause B leaves, as the pause menu's does) */
	if (playback_pressed(_gamepad_binary_button_start) | playback_pressed(_gamepad_binary_button_back))
		network_replay_set_paused(!playback.paused);
	if (playback_pressed(FIRST_GAMEPAD_ANALOG_BUTTON + _gamepad_analog_button_b))
	{
		if (playback.paused)
		{
			network_replay_end();
			return;
		}
		playback_lines_hidden = !playback_lines_hidden;
	}
	/* (the shoulders: the right one faster, the left one slower; a change of speed in the pause keeps it paused) */
	if (playback_pressed(FIRST_GAMEPAD_ANALOG_BUTTON + _gamepad_analog_button_black))
		network_replay_set_speed(ae_theater_speed_step(playback.speed_percent, 1));
	if (playback_pressed(FIRST_GAMEPAD_ANALOG_BUTTON + _gamepad_analog_button_white))
		network_replay_set_speed(ae_theater_speed_step(playback.speed_percent, -1));
	if (playback_pressed(_gamepad_binary_button_dpad_right))
		playback_skip(1);
	if (playback_pressed(_gamepad_binary_button_dpad_left))
		playback_skip(-1);
	if (playback_pressed(FIRST_GAMEPAD_ANALOG_BUTTON + _gamepad_analog_button_a))
		playback_step_player(FALSE);
	if (playback_pressed(FIRST_GAMEPAD_ANALOG_BUTTON + _gamepad_analog_button_x))
		playback_step_player(TRUE);
	if (playback_pressed(FIRST_GAMEPAD_ANALOG_BUTTON + _gamepad_analog_button_y))
		network_replay_view_next();
	/* (the right stick's click, or the d-pad down: behind turns with the player again, at its zoom; the free camera tracks
	them from where it is, or holds its aim. Both asked, so each one's press is seen) */
	if (playback_pressed(_gamepad_binary_button_right_thumb) | playback_pressed(_gamepad_binary_button_dpad_down))
	{
		if (playback.view == _playback_view_behind)
			playback.behind_orbiting = FALSE;
		else if (playback.view == _playback_view_free && !playback.free_placing)
			playback.free_tracking = !playback.free_tracking;
	}
	/* (the left stick's click: the free or cinematic camera placed behind the player again, as when it is chosen, once
	a press, the next heard no sooner than PLAYBACK_CAMERA_RESET_MILLISECONDS after) */
	if (playback_pressed(_gamepad_binary_button_left_thumb) &&
		(playback.view == _playback_view_free || playback.view == _playback_view_cinematic) && !playback.free_placing &&
		system_milliseconds() - playback.free_reset_at >= PLAYBACK_CAMERA_RESET_MILLISECONDS)
	{
		playback.free_placing = TRUE;
		playback.free_reset_at = system_milliseconds();
	}
}

boolean network_replay_lines_hidden(
	void)
{
	return playback_lines_hidden;
}

/* the player watched: their number (1 to 9, 0 for none) and their name in capitals into name */
long network_replay_watched_number(
	char *name,
	long size)
{
	long player_index = playback_watched();
	struct player_datum *player = player_index != NONE ? player_try_and_get(player_index) : NULL;
	short index;

	if (name && size > 0)
		name[0] = 0;
	if (!playback.playing || (playback.view == _playback_view_free && (playback.free_placing || !playback.free_tracking)) || !player)
		return 0;
	if (name && size > 0)
	{
		for (index = 0; index < (short)NUMBEROF(player->name) && index + 1 < size && player->name[index]; index++)
			name[index] = (char)toupper(player_name_character_ascii(player->name[index]));
		name[index] = 0;
	}
	return (long)DATUM_INDEX_TO_ABSOLUTE_INDEX(player_index) + 1;
}

/* ---------- the controls (DEV MODE's replay command; the Theater screen's) */

boolean network_replay_view_eyes(
	char const *player_name)
{
	struct data_iterator iterator;
	struct player_datum *player;

	if (!playback.playing)
		return FALSE;
	if (!player_name || !player_name[0])
	{
		playback.view = _playback_view_eyes;
		return TRUE;
	}
	data_iterator_new(&iterator, player_data);
	while ((player = data_iterator_next(&iterator)) != NULL)
	{
		char name[NUMBEROF(player->name) + 1];
		short index;

		for (index = 0; index < (short)NUMBEROF(player->name) && player->name[index]; index++)
			name[index] = (char)tolower(player_name_character_ascii(player->name[index]));
		name[index] = 0;
		if (!strcmp(name, player_name) || (player_name[0] >= '1' && player_name[0] <= '9' && !player_name[1] &&
			DATUM_INDEX_TO_ABSOLUTE_INDEX(iterator.datum_index) == player_name[0] - '1'))
		{
			/* (not yet in the game, or dead: watched once they are, the view held till then) */
			playback.watched = (short)DATUM_INDEX_TO_ABSOLUTE_INDEX(iterator.datum_index);
			playback.view = _playback_view_eyes;
			return TRUE;
		}
	}
	return FALSE;
}

void network_replay_view_free(
	void)
{
	if (!playback.playing)
		return;
	playback.view = _playback_view_free;
	playback_views_begin();
}

void network_replay_set_paused(
	boolean paused)
{
	if (!playback.playing)
		return;
	playback.paused = paused;
	game_time_set_speed(paused ? 0.0f : (real)playback.speed_percent / 100.0f);
}

void network_replay_set_speed(
	long percent)
{
	if (!playback.playing)
		return;
	playback.speed_percent = percent < 25 ? 25 : percent > 400 ? 400 : percent;
	if (!playback.paused && playback.seek_tick == NONE)
		game_time_set_speed((real)playback.speed_percent / 100.0f);
}

boolean network_replay_paused(
	void)
{
	return network_replay_playing() && playback.paused;
}

long network_replay_speed_percent(
	void)
{
	return network_replay_playing() ? playback.speed_percent : 100;
}

boolean network_replay_view_is_free(
	void)
{
	return network_replay_playing() && playback.view == _playback_view_free;
}

/* the view as the Theater's lines name it (title case, or in capitals), and the one Y goes to next */
static char const *playback_view_name(
	short view,
	boolean capitals)
{
	static char const *const names[NUMBER_OF_PLAYBACK_VIEWS] = { "Behind", "Eyes", "Free camera", "Cinematic" };
	static char const *const capital_names[NUMBER_OF_PLAYBACK_VIEWS] = { "BEHIND", "EYES", "FREE CAMERA", "CINEMATIC" };

	view = PIN(view, 0, NUMBER_OF_PLAYBACK_VIEWS - 1);
	return capitals ? capital_names[view] : names[view];
}

char const *network_replay_view_name(
	boolean next,
	boolean capitals)
{
	return playback_view_name(next ? (short)((playback.view + 1) % NUMBER_OF_PLAYBACK_VIEWS) : playback.view, capitals);
}

/* a view chosen: the free and cinematic cameras are placed behind the player, behind's turns round behind them */
static void playback_view_choose(
	short view)
{
	playback.view = view;
	playback_views_begin();
}

void network_replay_view_behind(
	void)
{
	if (playback.playing)
		playback_view_choose(_playback_view_behind);
}

void network_replay_view_cinematic(
	void)
{
	if (playback.playing)
		playback_view_choose(_playback_view_cinematic);
}

/* Y: the next view, in Content Evolved's order (behind, eyes, free camera, cinematic) */
void network_replay_view_next(
	void)
{
	if (!playback.playing)
		return;
	playback_view_choose((short)((playback.view + 1) % NUMBER_OF_PLAYBACK_VIEWS));
	platform_log("replay: view %s", playback_view_name(playback.view, FALSE));
}

long network_replay_position_seconds(
	void)
{
	return playback.playing ? (game_time_get() - (long)playback.header.start_tick) / TICKS_PER_SECOND : 0;
}

long network_replay_length_seconds(
	void)
{
	return playback.records ? (long)(playback.stream.last_tick - playback.stream.first_tick) / TICKS_PER_SECOND : 0;
}

/* a seek to a second of the game: ahead, played to at 4 times; back, the replay begun again and played to it */
boolean network_replay_seek(
	long seconds)
{
	long target;

	if (!playback.playing)
		return FALSE;
	if (seconds < 0)
		seconds = 0;
	if (seconds > network_replay_length_seconds())
		seconds = network_replay_length_seconds();
	target = (long)playback.header.start_tick + seconds * TICKS_PER_SECOND;
	if (target < game_time_get())
	{
		char name[AE_REPLAY_NAME_SIZE];
		char why[96];
		/* (the view, its place and the speed kept: the replay begun again is the same one, watched the same way) */
		short view = playback.view, watched = playback.watched;
		real_point3d free_position = playback.free_position;
		real free_yaw = playback.free_yaw, free_pitch = playback.free_pitch;
		boolean free_tracking = playback.free_tracking, free_still = playback.free_still;
		long speed_percent = playback.speed_percent;
		boolean paused = playback.paused;

		snprintf(name, sizeof(name), "%s", playback.name);
		if (!network_replay_play(name, why, sizeof(why)))
			return FALSE;
		playback.view = view;
		playback.watched = watched;
		playback.free_position = free_position;
		playback.free_yaw = free_yaw;
		playback.free_pitch = free_pitch;
		playback.free_tracking = free_tracking;
		playback.free_still = free_still;
		playback.speed_percent = speed_percent;
		playback.paused = paused;
	}
	playback.seek_tick = target;
	if (!playback.seek_muted)
	{
		sound_pause(TRUE);
		playback.seek_muted = TRUE;
	}
	game_time_set_speed(PLAYBACK_SEEK_SPEED);
	return TRUE;
}

/* (game_time.c) the ticks a frame runs while a seek plays to its second: at most PLAYBACK_SEEK_TICKS_PER_FRAME, none past
the second (Content Evolved's network_replay_ticks_per_frame) */
long network_replay_ticks_per_frame(
	long ticks)
{
	if (!playback.playing || playback.seek_tick == NONE)
		return ticks;
	return MAX(0, MIN(PLAYBACK_SEEK_TICKS_PER_FRAME, playback.seek_tick - game_time_get()));
}

boolean network_replay_seeking(
	void)
{
	return network_replay_playing() && playback.seek_tick != NONE;
}

boolean network_replay_seek_progress(
	long *target_seconds,
	float *progress)
{
	long start = (long)playback.header.start_tick;

	if (!network_replay_seeking())
		return FALSE;
	*target_seconds = (playback.seek_tick - start) / TICKS_PER_SECOND;
	/* (loading, the replay begun again for a seek back: none of the way yet) */
	*progress = playback.starting || playback.seek_tick <= start ? 0.0f :
		PIN((float)(game_time_get() - start) / (float)(playback.seek_tick - start), 0.0f, 1.0f);
	return TRUE;
}

boolean network_replay_scores_hidden(
	void)
{
	/* (the pause card is over the view, and a seek's card covers it: the scoreboard Back opens waits) */
	return network_replay_playing() && (playback.paused || playback.seek_tick != NONE);
}

void network_replay_end(
	void)
{
	if (!network_replay_playing())
		return;
	platform_log("replay: left at tick %ld", game_time_get());
	playback.leaving = TRUE;
	network_game_abort();
}

boolean network_replay_hides_object(
	long object_index)
{
	long player_index = network_replay_eyes_player();
	struct player_datum *player = player_index != NONE ? player_try_and_get(player_index) : NULL;

	if (!player || player->unit_index == NONE || player->unit_index != object_index)
		return FALSE;
	playback_body_hidden_frame = playback_frames;
	return TRUE;
}

/* what window 0 draws in first person, short, for the status line: the arms of each local player that has any (its
unit, weapon, shown or hidden), which player local player 0 is (0: none), and whether the watched player's body was
left out of the last frame */
static void playback_first_person_describe(
	char *text,
	size_t size)
{
	short local;
	size_t used = 0;
	long local_player = local_player_get_player_index(0);

	text[0] = 0;
	for (local = 0; local < 4 && used < size; local++)
	{
		long unit_index = NONE, weapon_index = NONE;
		boolean visible = FALSE;

		first_person_weapon_port_state(local, &unit_index, &weapon_index, &visible);
		if (unit_index == NONE)
			continue;
		used += (size_t)snprintf(text + used, size - used, "; arms%d %lx/%lx %s", local, (unsigned long)unit_index,
			(unsigned long)weapon_index, visible ? "on" : "off");
	}
	if (used < size)
		snprintf(text + used, size - used, "; local0 p%ld; body %s",
			local_player == NONE ? 0L : (long)DATUM_INDEX_TO_ABSOLUTE_INDEX(local_player) + 1,
			playback_frames && playback_body_hidden_frame + 1 >= playback_frames ? "hidden" : "drawn");
}

void network_replay_playback_status(
	char *text,
	long size)
{
	char drawn[96];
	long player_index = playback.playing ? playback_watched() : NONE;
	char watched[24];

	if (player_index != NONE)
	{
		struct player_datum *player = player_try_and_get(player_index);

		snprintf(watched, sizeof(watched), "%ld%s", (long)DATUM_INDEX_TO_ABSOLUTE_INDEX(player_index) + 1,
			player && player->unit_index != NONE &&
			playback_rides_seen_from_behind(unit_try_and_get(player->unit_index)) ? " (riding)" : "");
	}
	else if (playback.watched != NONE)
		snprintf(watched, sizeof(watched), "%d (waiting)", playback.watched + 1);
	else
		snprintf(watched, sizeof(watched), "none");
	if (!playback.records)
		snprintf(text, (size_t)size, "not playing");
	else if (playback.starting)
		snprintf(text, (size_t)size, "loading %s", playback.name);
	else
	{
		playback_first_person_describe(drawn, sizeof(drawn));
		snprintf(text, (size_t)size, "%s %ld/%ld s, %s%s, %ld%%%s%s%s; still %lu/%lu frames, unturned %lu/%lu, new aim "
			"%lu/%lu ticks", playback.name, network_replay_position_seconds(), network_replay_length_seconds(),
			playback.view == _playback_view_free ? "free" : playback.view == _playback_view_eyes ? "eyes " :
			playback.view == _playback_view_behind ? "behind " : "cinematic ",
			playback.view == _playback_view_free ? "" : watched,
			playback.speed_percent, playback.paused ? ", paused" : "", playback.seek_tick != NONE ? ", seeking" : "",
			drawn, playback.eyes_still_frames, playback.eyes_moving_frames, playback.eyes_unturned_frames,
			playback.eyes_turning_frames, playback.eyes_turns, playback.eyes_ticks);
	}
}
