/*
AE_BOMB_FEEDBACK.C

SEARCH AND DESTROY's pickup and drop feedback (ae_bomb_feedback.h).
*/

#include "cseries.h"
#include "cseries/errors.h"
#include "game/game.h"
#include "game/game_engine.h"
#include "game/game_globals.h"
#include "scenario/scenario.h"
#include "game/players.h"
#include "interface/hud_messaging.h"
#include "items/item_definitions.h"
#include "memory/data.h"
#include "objects/objects.h"
#include "sound/game_sound.h"

#include <stdio.h>
#include <string.h>

#include "ae_bomb_assault.h"
#include "ae_bomb_feedback.h"

void platform_log(char const *format, ...);
char const *config_string(char const *name);
long tag_loaded(long group_tag, const char *name);

#define AE_BOMB_FEEDBACK_SOUND_GROUP 0x736E6421 /* 'snd!' */

/* the pickup's tone (owner 10-28, by ear: not the stock maps' "gong", the hill's move, nor other stock or library
sounds): the bomb pack's keypad chirp-chirp, our own; without the pack, the planted bomb's own beep (the multiplayer
globals' countdown timer, every map has it) twice fast, at these ticks after the pickup (debug.bomb_pickup_beeps,
ticks apart, tries others) */
#define AE_BOMB_FEEDBACK_PACK_SOUND "ae\\packs\\bomb\\bomb\\sounds\\pickup"
#define AE_BOMB_FEEDBACK_BEEP_SOUND 0x1A
#define AE_BOMB_FEEDBACK_MAXIMUM_BEEPS 8
static char const ae_bomb_feedback_beeps_default[] = "0 4";
/* a carrier gone waits this long (half a second: a client's fuse comes in the host's state every 6-12 ticks) before
it is a drop: a plant or a new round in it says nothing */
#define AE_BOMB_FEEDBACK_DROP_WAIT_TICKS 15
/* the dropped bomb's thud, from the bomb's own collision sound */
#define AE_BOMB_FEEDBACK_THUD_SCALE 0.8f

static struct
{
	boolean on;
	long beeps_start;
	short beeps_played;
	short beep_count;
	long beep_ticks[AE_BOMB_FEEDBACK_MAXIMUM_BEEPS];
	long carrier;
	long round;
	long drop_player;
	long drop_time;
	long drop_round;
	long last_tick;
} ae_bomb_feedback = { FALSE, NONE, 0, 0, { 0 }, NONE, 0, NONE, NONE, 0, NONE };

/* a line to one local player: ASCII text around a player's name (the build's wchar_t is two bytes: no swprintf) */
static void say(short local_player_index, char const *before, long name_player_index, char const *after)
{
	wchar_t line[64];
	short at = 0;
	struct player_datum *named = name_player_index != NONE ? player_try_and_get(name_player_index) : NULL;
	char const *c;

	for (c = before; *c && at < (short)NUMBEROF(line) - 1; c++)
		line[at++] = (wchar_t)*c;
	if (named)
	{
		short n;

		for (n = 0; n < (short)NUMBEROF(named->name) && named->name[n] && at < (short)NUMBEROF(line) - 1; n++)
			line[at++] = named->name[n];
	}
	for (c = after; *c && at < (short)NUMBEROF(line) - 1; c++)
		line[at++] = (wchar_t)*c;
	line[at] = 0;
	hud_print_message(local_player_index, line);
}

/* each local attacker its line (the subject's own: you_before, else the others'); TRUE if any is an attacker */
static boolean tell_attackers(long subject_player_index, char const *you, char const *before, char const *after)
{
	long attackers = ae_bomb_assault_attacking_team();
	struct data_iterator iterator;
	struct player_datum *player;
	boolean any = FALSE;

	if (attackers == NONE)
		return FALSE;
	data_iterator_new(&iterator, player_data);
	while ((player = (struct player_datum *)data_iterator_next(&iterator)) != NULL)
	{
		if (player->local_player_index == NONE || player->team_index != attackers)
			continue;
		any = TRUE;
		if (iterator.datum_index == subject_player_index)
			say(player->local_player_index, you, NONE, "");
		else
			say(player->local_player_index, before, subject_player_index, after);
	}
	return any;
}

/* the pickup's sound: debug.bomb_pickup_sound's (a sound tag of the map's, '/' or '\\' between its folders), else the
bomb pack's; NONE (the beeps) when neither is loaded */
static long pickup_sound(void)
{
	char const *setting = config_string("debug.bomb_pickup_sound");
	char name[256];
	short at;

	if (!setting || !*setting)
		return tag_loaded(AE_BOMB_FEEDBACK_SOUND_GROUP, AE_BOMB_FEEDBACK_PACK_SOUND);
	for (at = 0; setting[at] && at < (short)sizeof(name) - 1; at++)
		name[at] = setting[at] == '/' ? '\\' : setting[at];
	name[at] = 0;
	return tag_loaded(AE_BOMB_FEEDBACK_SOUND_GROUP, name);
}

/* the pickup's beeps: the pattern (debug.bomb_pickup_beeps or the default), from now */
static void start_beeps(void)
{
	char const *setting = config_string("debug.bomb_pickup_beeps");
	char const *c = setting && *setting ? setting : ae_bomb_feedback_beeps_default;
	short count = 0;

	while (*c && count < AE_BOMB_FEEDBACK_MAXIMUM_BEEPS)
	{
		long ticks = 0;
		boolean digits = FALSE;

		while (*c == ' ' || *c == ',')
			c++;
		while (*c >= '0' && *c <= '9')
		{
			ticks = ticks * 10 + (*c++ - '0');
			digits = TRUE;
		}
		if (!digits)
			break;
		ae_bomb_feedback.beep_ticks[count++] = ticks;
	}
	ae_bomb_feedback.beep_count = count;
	ae_bomb_feedback.beeps_played = 0;
	ae_bomb_feedback.beeps_start = game_time_get();
}

/* each tick: the pattern's beeps due */
static void play_beeps(void)
{
	struct game_globals *globals;
	struct game_globals_multiplayer_information *information;
	struct tag_reference *sound;
	long now = game_time_get();

	if (ae_bomb_feedback.beeps_played >= ae_bomb_feedback.beep_count ||
		now - ae_bomb_feedback.beeps_start < ae_bomb_feedback.beep_ticks[ae_bomb_feedback.beeps_played])
	{
		return;
	}
	ae_bomb_feedback.beeps_played++;
	globals = scenario_get_game_globals();
	if (!globals || globals->multiplayer_information.count <= 0)
		return;
	information = TAG_BLOCK_GET_ELEMENT(&globals->multiplayer_information, 0, struct game_globals_multiplayer_information);
	if (!information || information->sounds.count <= AE_BOMB_FEEDBACK_BEEP_SOUND)
		return;
	sound = TAG_BLOCK_GET_ELEMENT(&information->sounds, AE_BOMB_FEEDBACK_BEEP_SOUND, struct tag_reference);
	if (sound && sound->index != NONE)
		unspatialized_impulse_sound_new(sound->index, 0.8f);
}

static void picked_up(long player_index)
{
	boolean heard = tell_attackers(player_index, "You picked up the bomb", "", " has the bomb");
	long sound_index = pickup_sound();

	if (heard && sound_index != NONE)
		unspatialized_impulse_sound_new(sound_index, 1.0f);
	else if (heard)
		start_beeps();
	platform_log("ae snd: feedback: player %ld picked up the bomb (%s; tone %s%s)",
		(long)DATUM_INDEX_TO_ABSOLUTE_INDEX(player_index), heard ? "the attackers here told" : "no attacker here",
		sound_index != NONE ? (*config_string("debug.bomb_pickup_sound") ? config_string("debug.bomb_pickup_sound") :
			"the pack's chirp-chirp") : *config_string("debug.bomb_pickup_beeps") ? config_string("debug.bomb_pickup_beeps") :
			"the beeps 0 4",
		*config_string("debug.bomb_pickup_sound") && sound_index == NONE ? " (debug.bomb_pickup_sound not on this map)" : "");
}

/* the bomb's own collision sound where it lies, heard by everyone near (a sound of the world, not a message) */
static void thud(void)
{
	struct ae_bomb_state state;
	struct object_datum *bomb;
	struct sound_location location;
	long sound_index;

	ae_bomb_state_get(&state);
	if (state.weapon_index == NONE ||
		!(bomb = (struct object_datum *)object_try_and_get_and_verify_type(state.weapon_index, _object_mask_item)) ||
		bomb->object.parent_object_index != NONE)
	{
		return;
	}
	sound_index = item_definition_get(bomb->definition_index)->item.collision_sound.index;
	if (sound_index == NONE)
		return;
	object_get_origin(state.weapon_index, &location.position);
	location.forward = *global_up3d;
	location.translational_velocity = *global_zero_vector3d;
	location.game_location = bomb->object.location;
	unattached_impulse_sound_new(sound_index, &location, AE_BOMB_FEEDBACK_THUD_SCALE);
}

static void dropped(long player_index)
{
	boolean told = tell_attackers(player_index, "You dropped the bomb", "Bomb dropped by ", "");

	thud();
	platform_log("ae snd: feedback: player %ld dropped the bomb (%s, the thud)",
		(long)DATUM_INDEX_TO_ABSOLUTE_INDEX(player_index), told ? "the attackers here told" : "no attacker here");
}

void ae_bomb_feedback_update(void)
{
	struct ae_bomb_state state;
	long now = game_time_get();

	if (!ae_bomb_snd_shown())
	{
		ae_bomb_feedback.on = FALSE;
		return;
	}
	ae_bomb_state_get(&state);
	/* (a new game or round: who carries it now is where it starts, nothing said; a new game's clock starts again,
	and its first round can have the last game's number) */
	if (!ae_bomb_feedback.on || state.round != ae_bomb_feedback.round || now < ae_bomb_feedback.last_tick)
	{
		ae_bomb_feedback.last_tick = now;
		ae_bomb_feedback.beep_count = 0;
		ae_bomb_feedback.on = TRUE;
		ae_bomb_feedback.round = state.round;
		ae_bomb_feedback.carrier = state.carrier;
		ae_bomb_feedback.drop_player = NONE;
		return;
	}
	ae_bomb_feedback.last_tick = now;
	play_beeps();
	if (state.carrier != ae_bomb_feedback.carrier)
	{
		if (ae_bomb_feedback.carrier != NONE && state.carrier == NONE)
		{
			ae_bomb_feedback.drop_player = ae_bomb_feedback.carrier;
			ae_bomb_feedback.drop_time = now;
			ae_bomb_feedback.drop_round = state.round;
		}
		if (state.carrier != NONE)
		{
			ae_bomb_feedback.drop_player = NONE;
			picked_up(state.carrier);
		}
		ae_bomb_feedback.carrier = state.carrier;
	}
	if (ae_bomb_feedback.drop_player != NONE && now - ae_bomb_feedback.drop_time >= AE_BOMB_FEEDBACK_DROP_WAIT_TICKS)
	{
		long player_index = ae_bomb_feedback.drop_player;

		ae_bomb_feedback.drop_player = NONE;
		/* (planted, the round over or a new one: no drop) */
		if (ae_bomb_assault_fuse_ticks() > 0 || state.hold_ticks > 0 || state.round != ae_bomb_feedback.drop_round)
			platform_log("ae snd: feedback: the carrier let go to a plant or a round's end: nothing said");
		else
			dropped(player_index);
	}
}
