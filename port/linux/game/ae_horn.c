/*
AE_HORN.C

(spike 10-26) VEHICLE HORN (ae_horn.h). Log: "horn: <vehicle> honked by team N (local player N | bot), loudness x".
*/

#include "cseries.h"
#include "game/game.h"
#include "game/players.h"
#include "math/real_math.h"
#include "memory/data.h"
#include "objects/objects.h"
#include "units/units.h"

#include "ae_horn.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void platform_log(const char *format, ...);
const char *platform_data_root(void);
void platform_ui_voice_start(short const *samples, unsigned long frames, unsigned long channels,
	unsigned long sample_rate);
int platform_callout_voice_busy(void);

#define AE_HORN_COOLDOWN (TICKS_PER_SECOND * 6 / 5)
/* (owner 10-26: "volume lowered a bit", then "still a bit less loud, it blocks the announcer") */
#define AE_HORN_VOLUME 0.35f
#define AE_HORN_HEARD 60.0f
#define AE_HORN_REMEMBERED (3 * TICKS_PER_SECOND)
#define AE_HORN_LOCAL_PLAYERS 4

static struct
{
	int read;
	short *samples, *played;
	unsigned long frames, channels, rate;
	boolean held[AE_HORN_LOCAL_PLAYERS];
	long vehicle_index, tick;
	short team_index;
} ae_horn = { 0, NULL, NULL, 0, 0, 0, { 0 }, NONE, -1000, NONE };

static unsigned long ae_horn_le(unsigned char const *at, int bytes)
{
	unsigned long value = 0;

	while (bytes--)
		value = (value << 8) | at[bytes];
	return value;
}

/* a 16-bit PCM WAV: its data chunk */
static void ae_horn_read(void)
{
	char const *path = getenv("HALO_HORN_WAV");
	char shipped[1024];
	FILE *file;
	long size;
	unsigned char *bytes, *at;

	if (ae_horn.read)
		return;
	ae_horn.read = 1;
	/* HALO_HORN_WAV, else the one the game ships (sounds/ in the data root, beside voices/), else one in the folder the
	game runs in (the spike's) */
	snprintf(shipped, sizeof(shipped), "%s/sounds/horn.wav", platform_data_root());
	file = fopen(path && *path ? path : shipped, "rb");
	if (!file && !(path && *path))
		file = fopen("horn.wav", "rb");
	if (!file)
	{
		platform_log("horn: no horn.wav (HALO_HORN_WAV, %s): silent", shipped);
		return;
	}
	fseek(file, 0, SEEK_END);
	size = ftell(file);
	fseek(file, 0, SEEK_SET);
	bytes = malloc((size_t)size);
	if (!bytes || fread(bytes, 1, (size_t)size, file) != (size_t)size || size < 44 || memcmp(bytes, "RIFF", 4) ||
		memcmp(bytes + 8, "WAVE", 4))
	{
		fclose(file);
		free(bytes);
		platform_log("horn: horn.wav unreadable");
		return;
	}
	fclose(file);
	for (at = bytes + 12; at + 8 <= bytes + size; at += 8 + ae_horn_le(at + 4, 4) + (ae_horn_le(at + 4, 4) & 1))
	{
		unsigned long length = ae_horn_le(at + 4, 4);

		if (!memcmp(at, "fmt ", 4) && length >= 16)
		{
			ae_horn.channels = ae_horn_le(at + 10, 2);
			ae_horn.rate = ae_horn_le(at + 12, 4);
			if (ae_horn_le(at + 22, 2) != 16)
				break;
		}
		else if (!memcmp(at, "data", 4) && ae_horn.channels && at + 8 + length <= bytes + size)
		{
			ae_horn.frames = length / (2 * ae_horn.channels);
			ae_horn.samples = malloc(length);
			ae_horn.played = malloc(length);
			if (ae_horn.samples && ae_horn.played)
				memcpy(ae_horn.samples, at + 8, length);
			break;
		}
	}
	free(bytes);
	platform_log("horn: horn.wav %lu frames, %lu channels, %lu Hz", ae_horn.frames, ae_horn.channels, ae_horn.rate);
}

/* the nearest local player's distance to the vehicle (none: far) */
static real ae_horn_listener_distance(struct object_datum const *vehicle)
{
	struct data_iterator iterator;
	struct player_datum *player;
	real best = AE_HORN_HEARD * 2.0f;
	int seen = 0, local = 0;

	data_iterator_new(&iterator, player_data);
	while ((player = (struct player_datum *)data_iterator_next(&iterator)) != NULL)
	{
		struct object_datum *unit;

		seen++;
		if (player->local_player_index == NONE || player->unit_index == NONE)
			continue;
		local++;
		unit = object_try_and_get_and_verify_type(player->unit_index, _object_mask_unit);
		/* (a seated listener is where its vehicle is: in it, at the horn itself) */
		if (unit && unit->object.parent_object_index != NONE)
		{
			if (object_get(unit->object.parent_object_index) == vehicle)
				return 0.0f;
			unit = object_get(unit->object.parent_object_index);
		}
		if (unit)
			best = MIN(best, distance3d(&unit->object.position, &vehicle->object.position));
	}
	if (best >= AE_HORN_HEARD * 2.0f)
		platform_log("horn: no listener found (%d players, %d of this PC's)", seen, local);
	return best;
}

void ae_horn_honk(long vehicle_index)
{
	ae_horn_honk_heard(vehicle_index, FALSE);
}

void ae_horn_honk_heard(long vehicle_index, boolean here)
{
	struct unit_datum *vehicle = (struct unit_datum *)object_try_and_get_and_verify_type(vehicle_index, _object_mask_vehicle);
	struct object_datum *driver;
	real distance, loudness;
	unsigned long at, count;

	/* (a new game's time starts at 0 again: a honk "later" than now is an old game's) */
	if (game_time_get() < ae_horn.tick)
	{
		ae_horn.tick = -1000;
		ae_horn.vehicle_index = NONE;
	}
	if (!vehicle || vehicle->unit.driver_object_index == NONE || game_time_get() - ae_horn.tick < AE_HORN_COOLDOWN)
		return;
	driver = object_get(vehicle->unit.driver_object_index);
	ae_horn.vehicle_index = vehicle_index;
	ae_horn.tick = game_time_get();
	ae_horn.team_index = driver->object.owner_team_index;
	ae_horn_read();
	distance = here ? 0.0f : ae_horn_listener_distance(&vehicle->object);
	loudness = distance >= AE_HORN_HEARD ? 0.0f : 1.0f - 0.85f * distance / AE_HORN_HEARD;
	platform_log("horn: %lx honked by team %d, nearest player here %.1f units, loudness %.2f",
		(unsigned long)vehicle_index, (int)ae_horn.team_index, distance, loudness);
	if (!ae_horn.samples || loudness <= 0.0f)
		return;
	/* (the UI voice is the callouts' too: a clip there takes the place of the one speaking; the announcer first) */
	if (platform_callout_voice_busy())
	{
		platform_log("horn: not sounded (a callout speaks)");
		return;
	}
	count = ae_horn.frames * ae_horn.channels;
	for (at = 0; at < count; at++)
		ae_horn.played[at] = (short)((real)ae_horn.samples[at] * loudness * AE_HORN_VOLUME);
	platform_ui_voice_start(ae_horn.played, ae_horn.frames, ae_horn.channels, ae_horn.rate);
}

void ae_horn_local(short local_player_index, boolean melee)
{
	struct data_iterator iterator;
	struct player_datum *player;
	boolean pressed;

	if (local_player_index < 0 || local_player_index >= AE_HORN_LOCAL_PLAYERS)
		return;
	pressed = melee && !ae_horn.held[local_player_index];
	ae_horn.held[local_player_index] = melee;
	if (!pressed)
		return;
	data_iterator_new(&iterator, player_data);
	while ((player = (struct player_datum *)data_iterator_next(&iterator)) != NULL)
	{
		struct object_datum *unit;
		struct unit_datum *vehicle;

		if (player->local_player_index != local_player_index || player->unit_index == NONE)
			continue;
		unit = object_try_and_get_and_verify_type(player->unit_index, _object_mask_unit);
		if (!unit || unit->object.parent_object_index == NONE)
		{
			platform_log("horn: local player %d pressed on foot", (int)local_player_index);
			return;
		}
		vehicle = (struct unit_datum *)object_try_and_get_and_verify_type(unit->object.parent_object_index,
			_object_mask_vehicle);
		platform_log("horn: local player %d pressed in %lx (%s)", (int)local_player_index,
			(unsigned long)unit->object.parent_object_index,
			!vehicle ? "not a vehicle" : vehicle->unit.driver_object_index == player->unit_index ? "driving" : "riding");
		if (vehicle && vehicle->unit.driver_object_index == player->unit_index)
			ae_horn_honk_heard(unit->object.parent_object_index, TRUE);
		return;
	}
}

long ae_horn_recent(short team_index, real_point3d const *point, real reach)
{
	struct object_datum *vehicle;

	if (ae_horn.vehicle_index == NONE || game_time_get() < ae_horn.tick || game_time_get() - ae_horn.tick > AE_HORN_REMEMBERED ||
		ae_horn.team_index != team_index)
	{
		return NONE;
	}
	vehicle = object_try_and_get_and_verify_type(ae_horn.vehicle_index, _object_mask_vehicle);
	if (!vehicle || distance3d(&vehicle->object.position, point) > reach)
		return NONE;
	return ae_horn.vehicle_index;
}
