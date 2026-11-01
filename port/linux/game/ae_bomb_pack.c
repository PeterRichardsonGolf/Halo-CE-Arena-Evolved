/*
AE_BOMB_PACK.C

SEARCH AND DESTROY's bomb model (ae_bomb_pack.h). The map's ball is the bomb (game_engine_bomb.c); while the pack is
on the map and the game shows SEARCH AND DESTROY, the ball's definition draws the pack's model, found by name, in
place of its own, and a render bounding radius that holds it. Every object of the ball, wherever it is, then shows
the bomb: the one carried on the back, the one on the ground, the planted one. The ball's own model and radius come
back when the game stops showing it, and when the pack is let go from the map. Nothing here is sent over the network.

The timer (owner 10-28): the console's 4 bars and the beacon's bulb have shaders of their own, whose self-illumination
colour and base map this machine sets each tick from the fuse it knows (there is one bomb, so the tags hold its
state): carried or loose, the bars lit green and the beacon dark; planted, the bars go red right to left, one each
fifth of the fuse (the last fifth all red), and the beacon flashes with each beep (ae_bomb_snd_beep_age).

The blast (plan step 6): SEARCH AND DESTROY's goes off at the planted bomb, one big human explosion from the pack (CMT's
TSC:E, credited; debug.bomb_blast_look picks A the tank shell, B the super rocket or C the warthog's fireball for the
owner's test), without the pack the map's frag explosion; the host's falloff past the lethal core
game_engine_bomb.c's blast kills in (each player's share of a life by distance, an enemy's credited to the
planter).
Owner's pick 10-28: C, bigger, filling a large area (the effect at the bomb and on a ring about it).
*/

#include "cseries.h"
#include "cseries/errors.h"
#include "real_math.h"
#include "effects/effects.h"
#include "game/game.h"
#include "game/game_engine.h"
#include "game/game_globals.h"
#include "game/players.h"
#include "memory/data.h"
#include "items/projectile_definitions.h"
#include "networking/network_game_globals.h"
#include "objects/damage.h"
#include "objects/objects.h"
#include "objects/object_definitions.h"
#include "scenario/scenario.h"
#include "tag_files/tag_files.h"
#include "tag_files/tag_groups.h"

#include <math.h>

#include "ae_bomb_assault.h"
#include "ae_bomb_pack.h"
#include "ae_weapon_pack.h"

long tag_loaded(long group_tag, const char *name);
long config_integer(char const *name);

#define AE_BOMB_PACK_SHADER_GROUP 0x736F736F /* 'soso' */
#define AE_BOMB_PACK_EFFECT_GROUP 0x65666665 /* 'effe' */
#define AE_BOMB_PACK_DAMAGE_GROUP 0x6A707421 /* 'jpt!' */
/* a shader_model's self-illumination animation colours, lower and upper bound (rasterizer_xbox_models.c's struct
shader_model_definition, whose offsets it checks: the upper bound at 0x84) */
#define AE_BOMB_PACK_GLOW_LOWER_OFFSET 0x78
#define AE_BOMB_PACK_GLOW_UPPER_OFFSET 0x84
/* and its base map's tag index (the same struct: model.base_map.index at 0xB0). Owner 10-28 (TEST RUN snd-tone-1028):
the glow written alone left the bars static on screen, so the bars and the beacon swap their base map too, which
every draw reads: the export's green (the bars) or dark (the beacon), or the LED's red */
#define AE_BOMB_PACK_BASE_MAP_OFFSET 0xB0
#define AE_BOMB_PACK_BITMAP_GROUP 0x6269746D /* 'bitm' */
#define AE_BOMB_PACK_RED_BITMAP "ae\\packs\\bomb\\bomb\\bitmaps\\bomb_led"
/* the timer's shaders (the export's roles): the bars left to right, then the beacon's bulb */
#define AE_BOMB_PACK_TIMER_SHADERS 5
#define AE_BOMB_PACK_BEACON 4
static char const *const ae_bomb_pack_timer_names[AE_BOMB_PACK_TIMER_SHADERS] =
{
	"ae\\packs\\bomb\\bomb\\shaders\\bomb_bar_a",
	"ae\\packs\\bomb\\bomb\\shaders\\bomb_bar_b",
	"ae\\packs\\bomb\\bomb\\shaders\\bomb_bar_c",
	"ae\\packs\\bomb\\bomb\\shaders\\bomb_bar_d",
	"ae\\packs\\bomb\\bomb\\shaders\\bomb_beacon",
};
/* the beacon's flash after each beep */
#define AE_BOMB_PACK_BEACON_FLASH_TICKS 4
/* the blast looks (debug.bomb_blast_look 1-3; 0, the default, the owner's pick: C, TEST RUN snd-blast-1028): the
pack's names of CMT's effects */
#define AE_BOMB_PACK_BLAST_LOOK 2
static char const *const ae_bomb_pack_blast_names[] =
{
	"ae\\packs\\bomb\\vehicles\\h1_spirit\\scorpion\\weapons\\scorpion_cannon\\effects\\scorpion_cannon_shell_explosion",
	"ae\\packs\\bomb\\weapons\\evolved\\_egg\\rocket_launcher_super\\effects\\rocket_launcher_super_rocket_explosion",
	"ae\\packs\\bomb\\vehicles\\_shared\\warthog\\_warthog_chaingun\\effects\\warthog_chaingun_body_depleted",
};
static char const *const ae_bomb_pack_blast_looks[] = { "A tank shell", "B super rocket", "C warthog fireball" };
/* the host's falloff past the lethal core (100 down to 20 % of a life from the core's edge to the blast's), each
player hit by the stock rocket's explosion (every map has it) scaled to that share of a multiplayer life (shield 75 +
body 75; the rocket's 300-330) */
#define AE_BOMB_PACK_BLAST_DAMAGE "weapons\\rocket launcher\\explosion"
#define AE_BOMB_PACK_LIFE 150.0f
#define AE_BOMB_PACK_ROCKET_DAMAGE 315.0f
/* the look filling a large area (owner 10-28: "make it bigger and fill a large area"): the effect at the bomb and
again on a ring about it */
#define AE_BOMB_PACK_BLAST_RING 2.5f
#define AE_BOMB_PACK_RING_POINTS 6

#define AE_BOMB_PACK_MODEL_GROUP 0x6D6F6465 /* 'mode' */
/* the pack's prefix (ae\packs\bomb\) and the model's name in the donor (ae_pack\bomb\bomb, its ae_pack\ dropped) */
#define AE_BOMB_PACK_MODEL_NAME "ae\\packs\\bomb\\bomb\\bomb"
/* the bomb's render radius: half its longest side (0.185 wu) and some */
#define AE_BOMB_PACK_RENDER_RADIUS 0.12f

/* the ball's definition while it draws the pack's model (NONE: none does), and what it had */
static void ae_bomb_pack_log_tilt(void);

static long ae_bomb_pack_definition = NONE;
static long ae_bomb_pack_original_model = NONE;
static real ae_bomb_pack_original_radius = 0.0f;
/* the timer's shaders while the model is drawn (NONE: the pack has none), and the red bars last logged */
static long ae_bomb_pack_timer_shaders[AE_BOMB_PACK_TIMER_SHADERS] = { NONE, NONE, NONE, NONE, NONE };
/* their own base maps (green, dark) and the LED's red */
static long ae_bomb_pack_timer_bases[AE_BOMB_PACK_TIMER_SHADERS] = { NONE, NONE, NONE, NONE, NONE };
static long ae_bomb_pack_red_bitmap = NONE;
static long ae_bomb_pack_logged_reds = NONE;

/* a timer shader lit (its glow this colour, its base map the LED's red) or not (its own base map, its glow this) */
static void ae_bomb_pack_glow(short which_shader, boolean lit, real red, real green, real blue)
{
	long shader_index = ae_bomb_pack_timer_shaders[which_shader];
	byte *shader;
	real_rgb_color *bound;
	short which;

	if (shader_index == NONE)
		return;
	shader = (byte *)tag_get(AE_BOMB_PACK_SHADER_GROUP, shader_index);
	if (ae_bomb_pack_timer_bases[which_shader] != NONE)
	{
		*(long *)(shader + AE_BOMB_PACK_BASE_MAP_OFFSET) = lit && ae_bomb_pack_red_bitmap != NONE ?
			ae_bomb_pack_red_bitmap : ae_bomb_pack_timer_bases[which_shader];
	}
	for (which = 0; which < 2; which++)
	{
		bound = (real_rgb_color *)(shader + (which ? AE_BOMB_PACK_GLOW_UPPER_OFFSET : AE_BOMB_PACK_GLOW_LOWER_OFFSET));
		bound->red = red;
		bound->green = green;
		bound->blue = blue;
	}
}

/* the bars and the beacon from the fuse this machine knows */
static void ae_bomb_pack_timer_update(void)
{
	long fuse = ae_bomb_assault_fuse_ticks();
	long total = (long)(ae_bomb_assault_fuse_time(0.0) * TICKS_PER_SECOND + 0.5);
	long reds = 0, age;
	short bar;

	if (fuse > 0 && total > 0)
	{
		reds = (total - fuse) * 5 / total;
		reds = reds < 0 ? 0 : reds > 4 ? 4 : reds;
	}
	for (bar = 0; bar < 4; bar++)
	{
		if (3 - bar < reds)
			ae_bomb_pack_glow(bar, TRUE, 1.0f, 0.08f, 0.03f);
		else
			ae_bomb_pack_glow(bar, FALSE, 0.1f, 1.0f, 0.2f);
	}
	age = ae_bomb_snd_beep_age();
	if (fuse > 0 && age != NONE && age < AE_BOMB_PACK_BEACON_FLASH_TICKS)
		ae_bomb_pack_glow(AE_BOMB_PACK_BEACON, TRUE, 1.0f, 0.12f, 0.05f);
	else
		ae_bomb_pack_glow(AE_BOMB_PACK_BEACON, FALSE, 0.0f, 0.0f, 0.0f);
	if (fuse <= 0)
		reds = NONE;
	if (reds != ae_bomb_pack_logged_reds)
	{
		if (reds == NONE)
			error(_error_silent, "bomb pack: the timer idle (bars green, beacon dark)");
		else
			error(_error_silent, "bomb pack: the timer, %ld of 4 bars red (fuse %ld of %ld ticks left)", reds, fuse,
				total);
		ae_bomb_pack_logged_reds = reds;
	}
}

static void ae_bomb_pack_restore(char const *why)
{
	struct object_definition *definition;

	if (ae_bomb_pack_definition == NONE)
		return;
	definition = object_definition_get(ae_bomb_pack_definition);
	definition->object.model.index = ae_bomb_pack_original_model;
	definition->object.render_bounding_radius = ae_bomb_pack_original_radius;
	error(_error_silent, "bomb pack: the ball's own model back (%s)", why);
	ae_bomb_pack_definition = NONE;
	{
		short index;

		for (index = 0; index < AE_BOMB_PACK_TIMER_SHADERS; index++)
		{
			if (ae_bomb_pack_timer_shaders[index] != NONE && ae_bomb_pack_timer_bases[index] != NONE)
			{
				*(long *)((byte *)tag_get(AE_BOMB_PACK_SHADER_GROUP, ae_bomb_pack_timer_shaders[index]) +
					AE_BOMB_PACK_BASE_MAP_OFFSET) = ae_bomb_pack_timer_bases[index];
			}
			ae_bomb_pack_timer_shaders[index] = NONE;
			ae_bomb_pack_timer_bases[index] = NONE;
		}
		ae_bomb_pack_red_bitmap = NONE;
		ae_bomb_pack_logged_reds = NONE;
	}
}

void ae_bomb_pack_update(void)
{
	long ball = NONE;
	long model = NONE;
	struct object_definition *definition;

	if (ae_weapon_pack_bomb_pack_added() && ae_bomb_snd_shown())
	{
		ball = get_ball_definition_index();
		model = tag_loaded(AE_BOMB_PACK_MODEL_GROUP, AE_BOMB_PACK_MODEL_NAME);
	}
	if (ae_bomb_pack_definition != NONE)
		ae_bomb_pack_log_tilt();
	if (ae_bomb_pack_definition != NONE && (ball == NONE || model == NONE || ball != ae_bomb_pack_definition))
		ae_bomb_pack_restore("not SEARCH AND DESTROY");
	if (ae_bomb_pack_definition != NONE)
		ae_bomb_pack_timer_update();
	if (ball == NONE || model == NONE || ae_bomb_pack_definition != NONE)
		return;
	definition = object_definition_get(ball);
	ae_bomb_pack_definition = ball;
	ae_bomb_pack_original_model = definition->object.model.index;
	ae_bomb_pack_original_radius = definition->object.render_bounding_radius;
	definition->object.model.index = model;
	if (definition->object.render_bounding_radius < AE_BOMB_PACK_RENDER_RADIUS)
		definition->object.render_bounding_radius = AE_BOMB_PACK_RENDER_RADIUS;
	{
		short index, found = 0;

		ae_bomb_pack_red_bitmap = tag_loaded(AE_BOMB_PACK_BITMAP_GROUP, AE_BOMB_PACK_RED_BITMAP);
		for (index = 0; index < AE_BOMB_PACK_TIMER_SHADERS; index++)
		{
			ae_bomb_pack_timer_shaders[index] = tag_loaded(AE_BOMB_PACK_SHADER_GROUP, ae_bomb_pack_timer_names[index]);
			ae_bomb_pack_timer_bases[index] = ae_bomb_pack_timer_shaders[index] == NONE ? NONE :
				*(long *)((byte *)tag_get(AE_BOMB_PACK_SHADER_GROUP, ae_bomb_pack_timer_shaders[index]) +
					AE_BOMB_PACK_BASE_MAP_OFFSET);
			found += ae_bomb_pack_timer_shaders[index] != NONE;
		}
		error(_error_silent, "bomb pack: the ball (definition %ld) drawn with the bomb's model (tag %ld, was %ld), "
			"%d of %d timer shaders, the red bitmap %s", ball, model, ae_bomb_pack_original_model, (int)found,
			AE_BOMB_PACK_TIMER_SHADERS, ae_bomb_pack_red_bitmap != NONE ? "found" : "NOT found");
	}
	ae_bomb_pack_timer_update();
}

static void ae_bomb_pack_blast_falloff(union real_point3d const *at, long scorer_index)
{
	long damage_index = tag_loaded(AE_BOMB_PACK_DAMAGE_GROUP, AE_BOMB_PACK_BLAST_DAMAGE);
	struct player_datum *planter = scorer_index != NONE ? player_try_and_get(scorer_index) : NULL;
	real core = ae_bomb_snd_blast_radius(0.0f), reach = ae_bomb_snd_blast_reach();
	struct data_iterator iterator;
	struct player_datum *player;
	long hurt = 0;

	if (damage_index == NONE || reach <= core)
		return;
	data_iterator_new(&iterator, player_data);
	while ((player = (struct player_datum *)data_iterator_next(&iterator)) != NULL)
	{
		struct object_datum *unit;
		struct damage_data damage;
		real distance, share;
		boolean enemy;

		if (player->unit_index == NONE ||
			!(unit = (struct object_datum *)object_try_and_get_and_verify_type(player->unit_index, _object_mask_unit)) ||
			TEST_FLAG(unit->object.damage_flags, _object_dead_bit))
		{
			continue;
		}
		distance = sqrtf(distance_squared3d(at, &unit->object.position));
		if (distance <= core || distance > reach)
			continue;
		/* (200 down to 20 over the whole radius: 100 at the core's edge, a life) */
		share = (200.0f - 180.0f * distance / reach) / 100.0f;
		enemy = planter && player->team_index != planter->team_index;
		damage_data_new(&damage, damage_index);
		damage.scale = share * AE_BOMB_PACK_LIFE / AE_BOMB_PACK_ROCKET_DAMAGE;
		damage.owner_player_index = enemy ? scorer_index : NONE;
		damage.owner_object_index = enemy ? planter->unit_index : NONE;
		damage.owner_team_index = enemy ? (short)planter->team_index : NONE;
		damage.origin = *at;
		damage.epicenter = *at;
		vector_from_points3d(at, &unit->object.position, &damage.direction);
		normalize3d(&damage.direction);
		object_cause_damage(&damage, player->unit_index, NONE, NONE, NONE, NULL);
		hurt++;
	}
	error(_error_silent, "bomb pack: the blast's falloff (%.2f to %.2f wu) hurt %ld players", core, reach, hurt);
}

/* the map's frag grenade's explosion (the globals' first grenade), or NONE */
static long ae_bomb_pack_frag_effect(void)
{
	struct game_globals *globals = scenario_get_game_globals();
	struct game_globals_grenade *grenade;

	if (!globals || globals->grenades.count <= 0)
		return NONE;
	grenade = TAG_BLOCK_GET_ELEMENT(&globals->grenades, 0, struct game_globals_grenade);
	return grenade->projectile.index == NONE ? NONE :
		projectile_definition_get(grenade->projectile.index)->projectile.effect.index;
}

boolean ae_bomb_pack_blast(union real_point3d const *at, long scorer_index)
{
	long look = config_integer("debug.bomb_blast_look");
	long effect_index = NONE;
	char const *marker_names[1] = { "" };
	real_point3d point;
	real_vector3d forward;

	if (!ae_bomb_snd_shown())
		return FALSE;
	look = look >= 1 && look <= (long)NUMBEROF(ae_bomb_pack_blast_names) ? look - 1 : AE_BOMB_PACK_BLAST_LOOK;
	if (ae_weapon_pack_bomb_pack_added())
		effect_index = tag_loaded(AE_BOMB_PACK_EFFECT_GROUP, ae_bomb_pack_blast_names[look]);
	error(_error_silent, "bomb pack: the blast at (%.2f %.2f %.2f): %s%s", at->x, at->y, at->z,
		effect_index != NONE ? ae_bomb_pack_blast_looks[look] : "the frag's explosion",
		effect_index != NONE || !ae_weapon_pack_bomb_pack_added() ? "" : " (the pack's effect not found)");
	if (effect_index == NONE)
		effect_index = ae_bomb_pack_frag_effect();
	if (effect_index != NONE)
	{
		short index;
		/* (the frag's alone, without the pack: one) */
		short count = ae_weapon_pack_bomb_pack_added() ? 1 + AE_BOMB_PACK_RING_POINTS : 1;

		forward = *global_up3d;
		effects_port_harmless = TRUE;
		for (index = 0; index < count; index++)
		{
			real angle = index ? 2.0f * _pi * (real)(index - 1) / AE_BOMB_PACK_RING_POINTS : 0.0f;
			real out = index ? AE_BOMB_PACK_BLAST_RING : 0.0f;

			point = *at;
			point.x += out * cosf(angle);
			point.y += out * sinf(angle);
			point.z += 0.05f;
			effect_new_unattached_from_markers(effect_index, NONE, NULL, 1, marker_names, &point, &forward, 0.0f,
				0.0f, NULL, NULL, TRUE);
		}
		effects_port_harmless = FALSE;
	}
	/* the host's falloff past the lethal core (game_engine_bomb.c's blast kills in it, credited to the planter). Owner
	10-28 (ask-me Q3): an enemy's hurt there is the planter's too, kill and all; a teammate's is no one's (no
	betrayal) */
	if (!network_game_distributed_client())
		ae_bomb_pack_blast_falloff(at, scorer_index);
	return TRUE;
}

/* (test round 10-28: the dropped bomb lies flat? one line each time a loose bomb's tilt from flat changes by more than
10 degrees, with its height) */
static void ae_bomb_pack_log_tilt(void)
{
	static long logged_weapon = NONE;
	static real logged_up = 2.0f;
	struct ae_bomb_state state;
	struct object_datum *bomb;

	ae_bomb_state_get(&state);
	if (!state.running || state.weapon_index == NONE || state.carrier != NONE ||
		!(bomb = (struct object_datum *)object_try_and_get_and_verify_type(state.weapon_index, _object_mask_item)) ||
		bomb->object.parent_object_index != NONE)
	{
		logged_weapon = NONE;
		return;
	}
	if (logged_weapon == state.weapon_index && fabsf(bomb->object.up.k - logged_up) < 0.015f)
		return;
	logged_weapon = state.weapon_index;
	logged_up = bomb->object.up.k;
	error(_error_silent, "bomb pack: the loose bomb at (%.2f %.2f %.2f), its up %.2f %.2f %.2f (1 = flat)",
		bomb->object.position.x, bomb->object.position.y, bomb->object.position.z, bomb->object.up.i,
		bomb->object.up.j, bomb->object.up.k);
}

void ae_bomb_pack_let_go(void)
{
	ae_bomb_pack_restore("the pack let go");
}
