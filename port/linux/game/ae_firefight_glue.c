/*
AE_FIREFIGHT_GLUE.C

What Firefight (firefight*.c, from Content Evolved) leaves to this build: its
options (the game variant's options carry none of them here) and the bots'
score. Its stats, score and rank are Content Evolved's own files.
*/

#include <stddef.h>
#include <string.h>

#include "cseries/cseries.h"
#include "memory/data.h"
#include "game/players.h"
#include "saved games/player_profile.h"

#include "ae_firefight_glue.h"
#include "halo_firefight.h"

/* ---------- options */

static struct firefight_options firefight_options_current;
static boolean firefight_options_initialized;

static struct firefight_options net_options;
static boolean net_valid;

struct firefight_options const *firefight_options_get(void)
{
	/* (a client playing a game whose host sent its rules: those, for that game) */
	if (net_valid)
		return &net_options;
	if (!firefight_options_initialized)
	{
		firefight_variant_options_default(&firefight_options_current);
		firefight_options_initialized = TRUE;
	}
	return &firefight_options_current;
}

void firefight_options_set(struct firefight_options const *options)
{
	if (options)
	{
		firefight_options_current = *options;
		firefight_options_initialized = TRUE;
	}
}

/* ---------- the host's rules on a client: sent over ae_net_extensions' packet (a Firefight game only), kept for that game */

/* (every rule of firefight_options, with the bytes it takes on the wire: a field is its own width, fixed here whatever the
machine's long is) */
#define FF_NET(field, width, type) { offsetof(struct firefight_options, field), (width), sizeof(((struct firefight_options *)0)->field) }
static struct
{
	size_t offset;
	unsigned char width;
	size_t size;
} const net_fields[] =
{
	FF_NET(friendly_fire, 1, byte),
	FF_NET(loadout, 1, byte),
	FF_NET(primary_weapon, 1, byte),
	FF_NET(secondary_weapon, 1, byte),
	FF_NET(firefight_custom_skulls, 4, unsigned long),
	FF_NET(firefight_generator_health, 2, short),
	FF_NET(firefight_score_life_threshold, 2, short),
	FF_NET(firefight_enemy_health_scale, 1, byte),
	FF_NET(firefight_enemy_shield_scale, 1, byte),
	FF_NET(firefight_bonus_seconds, 1, byte),
	FF_NET(firefight_faction_diplomacy, 1, byte),
	FF_NET(firefight_tag_profile, 4, unsigned long),
	FF_NET(firefight_starting_skulls[0], 4, unsigned long),
	FF_NET(firefight_starting_skulls[1], 4, unsigned long),
	FF_NET(firefight_unit_toggles, 2, unsigned short),
	FF_NET(firefight_set_escalation, 1, byte),
	FF_NET(firefight_dropship_mode, 1, byte),
	FF_NET(firefight_next_wave_mode, 1, byte),
	FF_NET(firefight_player_faction_mode, 1, byte),
	FF_NET(firefight_vehicle_spawns, 1, byte),
	FF_NET(firefight_plasma_flinch, 1, byte),
	FF_NET(firefight_unused_vehicle, 1, byte),
	FF_NET(firefight_prep_countdown, 1, byte),
	FF_NET(firefight_skull_progression_mode, 1, byte),
	FF_NET(firefight_player_shields, 1, byte),
	FF_NET(firefight_player_speed, 1, byte),
	FF_NET(firefight_player_gravity, 1, byte),
	FF_NET(firefight_infinite_ammo, 1, byte),
	FF_NET(firefight_map_content, 1, byte),
	FF_NET(firefight_wave_size_scale, 1, byte),
	FF_NET(firefight_boss_wave_mode, 1, byte),
	FF_NET(firefight_enemy_loadout, 1, byte),
	FF_NET(firefight_friendly_ai, 1, byte),
};
#undef FF_NET
#define NET_FIELDS ((int)(sizeof(net_fields) / sizeof(net_fields[0])))
typedef char verify_net_fields_fit[NET_FIELDS <= 64 ? 1 : -1];

static unsigned long net_read(struct firefight_options const *options, int index)
{
	unsigned char const *at = (unsigned char const *)options + net_fields[index].offset;
	unsigned long value = 0;

	switch (net_fields[index].size)
	{
	case 1: value = *at; break;
	case 2: { unsigned short v; memcpy(&v, at, 2); value = v; break; }
	case 4: { unsigned int v; memcpy(&v, at, 4); value = v; break; }
	default: { unsigned long v; memcpy(&v, at, sizeof(v)); value = v & 0xFFFFFFFFUL; break; }
	}

	return value;
}

static void net_write(struct firefight_options *options, int index, unsigned long value)
{
	unsigned char *at = (unsigned char *)options + net_fields[index].offset;

	switch (net_fields[index].size)
	{
	case 1: *at = (unsigned char)value; break;
	case 2: { unsigned short v = (unsigned short)value; memcpy(at, &v, 2); break; }
	case 4: { unsigned int v = (unsigned int)value; memcpy(at, &v, 4); break; }
	default: { unsigned long v = value; memcpy(at, &v, sizeof(v)); break; }
	}
}

int ae_firefight_net_rules(unsigned long *values, unsigned char *widths, int maximum)
{
	struct firefight_options const *options = firefight_options_get();
	int index;

	if (maximum < NET_FIELDS)
		return 0;
	for (index = 0; index < NET_FIELDS; index++)
	{
		values[index] = net_read(options, index);
		widths[index] = net_fields[index].width;
	}

	return NET_FIELDS;
}

int ae_firefight_net_widths(unsigned char *widths, int maximum)
{
	int index;

	if (maximum < NET_FIELDS)
		return 0;
	for (index = 0; index < NET_FIELDS; index++)
		widths[index] = net_fields[index].width;

	return NET_FIELDS;
}

void ae_firefight_net_apply(unsigned long const *values, int count)
{
	struct firefight_options received;
	int index;

	if (count != NET_FIELDS)
		return;
	memset(&received, 0, sizeof(received));
	for (index = 0; index < NET_FIELDS; index++)
		net_write(&received, index, values[index]);
	/* (a host's value is only a value: held to what the game reads, as a saved variant's are) */
	received.firefight_generator_health = (short)PIN(received.firefight_generator_health, 500, 10000);
	received.firefight_score_life_threshold = (short)PIN(received.firefight_score_life_threshold, 0, 30000);
	received.firefight_enemy_health_scale = (byte)PIN(received.firefight_enemy_health_scale, 0, 4);
	received.firefight_enemy_shield_scale = (byte)PIN(received.firefight_enemy_shield_scale, 0, 4);
	received.firefight_bonus_seconds = (byte)PIN(received.firefight_bonus_seconds, 0, 120);
	if (received.firefight_unit_toggles == 0)
		received.firefight_unit_toggles = FIREFIGHT_UNIT_MASK_ALL;
	net_options = received;
	net_valid = TRUE;
}

void ae_firefight_net_clear(void)
{
	net_valid = FALSE;
}

int ae_firefight_net_active(void)
{
	return net_valid;
}

/* ---------- bots */

/* Firefight's score multiplier takes this off for the bots' skill; none here */
float bots_score_multiplier_penalty(void)
{
	return 0.0f;
}

/* ---------- skulls (the skulls' own sounds are not part of this build: no sound added to a level) */

const unsigned int skulls_party_sound[1] = { 0 };
const unsigned long skulls_party_sound_size = 0;
const unsigned int skulls_pickup_sound[1] = { 0 };
const unsigned long skulls_pickup_sound_size = 0;
const unsigned int skulls_carrier_sound[1] = { 0 };
const unsigned long skulls_carrier_sound_size = 0;
const unsigned int skulls_floor_sound[1] = { 0 };
const unsigned long skulls_floor_sound_size = 0;

/* (a multiplayer map played as Firefight by a menu's choice: this build has no such menu) */
boolean multiplayer_firefight_mode(void)
{
	return FALSE;
}

/* (a multiplayer game hosted with a tag profile's changes: this build hosts none, so the skulls' tags are sent to a
joining machine only for the randomizer and the hosted skulls) */
boolean tag_multiplayer_hosted(void)
{
	return FALSE;
}
