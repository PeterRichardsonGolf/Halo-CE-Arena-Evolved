/*
AE_FORGE_ENGINE.C

The engine side of Forge's volumes and map settings (ae_forge_engine.h). Forge's functions are in port/forge/src/forge_volumes.c and
forge_map_settings.c, in the 64-bit Linux build alone (OPENCE_ENABLE_FORGE); any other build answers as the game does.
*/

#include "cseries.h"

#include "game/game.h"
#include "game/game_engine.h"
#include "game/game_globals.h"
#include "networking/network_connection.h"
#include "objects/objects.h"
#include "physics/collision_features.h"
#include "physics/physics.h"
#include "scenario/scenario.h"
#include "render/render_cameras.h"
#include "units/units.h"

#include "ae_forge_engine.h"
#include "ae_forge_rules.h"
#include "ae_set_body.h"
#include "opence_features.h"
#include "tag_files/tag_groups.h"

void platform_log(const char *format, ...);

#if OPENCE_ENABLE_FORGE

int forge_volumes_gravity_at(float x, float y, float z, float out_pull[3]);
int forge_volumes_fall_protected(long biped_index, float x, float y, float z, int airborne);
int forge_volumes_stop_inertia(long biped_index, float x, float y, float z);
int forge_volumes_one_way_floor(float const from[3], float to[3], float velocity[3], float radius, float out_normal[3],
	float *out_d);
long forge_map_settings_player_biped(long player_index, long unit_definition_index);
long forge_map_settings_hands(long unit_index, long hands_index);
void forge_map_settings_players_body(long profile, char const *biped, char const *hands, int movement_own);
void forge_map_settings_player_control(long unit_index, struct unit_control_data const *control);
int forge_map_settings_damage_ignored(long owner_object_index, long object_index);
int forge_map_settings_fog(short local_player_index, int indoors, struct render_fog *fog);

int ae_forge_gravity_pull(real_point3d const *at, float pull[3])
{
	return forge_volumes_gravity_at(at->x, at->y, at->z, pull);
}

void ae_forge_gravity_correct(real_vector3d *vector, float const pull[3], real amount)
{
	float moved[3];

	moved[0] = vector->i;
	moved[1] = vector->j;
	moved[2] = vector->k;
	ae_forge_rules_gravity_correct(moved, pull, amount);
	vector->i = moved[0];
	vector->j = moved[1];
	vector->k = moved[2];
}

int ae_forge_gravity_correct_at(real_vector3d *vector, real_point3d const *at, real amount)
{
	float pull[3];

	if (!forge_volumes_gravity_at(at->x, at->y, at->z, pull))
		return 0;
	ae_forge_gravity_correct(vector, pull, amount);
	return 1;
}

real ae_forge_physics_gravity(real_point3d const *at, real mass, real gravity, real_vector3d *force)
{
	float pull[3];

	if (!forge_volumes_gravity_at(at->x, at->y, at->z, pull))
		return global_gravity;
	force->i = mass * gravity * pull[0];
	force->j = mass * gravity * pull[1];
	force->k = mass * gravity * pull[2];
	return ae_forge_rules_ground_gravity(global_gravity, pull);
}

int ae_forge_biped_fall_protected(long biped_index, real_point3d const *at, int airborne)
{
	return forge_volumes_fall_protected(biped_index, at->x, at->y, at->z, airborne);
}

int ae_forge_biped_stop_inertia(long biped_index, real_point3d const *at)
{
	return forge_volumes_stop_inertia(biped_index, at->x, at->y, at->z);
}

int ae_forge_biped_lifted(float const pull[3], real_vector3d const *ground_normal)
{
	float normal[3];

	normal[0] = ground_normal->i;
	normal[1] = ground_normal->j;
	normal[2] = ground_normal->k;
	return ae_forge_rules_pull_lifts(pull, normal);
}

int ae_forge_biped_one_way(real_point3d const *from, real_point3d *to, real_vector3d *velocity, real width,
	struct collision_plane *collision)
{
	float start[3], end[3], moved[3], normal[3], d;

	start[0] = from->x;
	start[1] = from->y;
	start[2] = from->z;
	end[0] = to->x;
	end[1] = to->y;
	end[2] = to->z;
	moved[0] = velocity->i;
	moved[1] = velocity->j;
	moved[2] = velocity->k;
	if (!forge_volumes_one_way_floor(start, end, moved, width, normal, &d))
		return 0;
	to->x = end[0];
	to->y = end[1];
	to->z = end[2];
	velocity->i = moved[0];
	velocity->j = moved[1];
	velocity->k = moved[2];
	/* (the floor's plane one of the move's collisions, of no surface: the biped stands on it as on the ground) */
	collision->t = 1.0f;
	collision->plane.n.i = normal[0];
	collision->plane.n.j = normal[1];
	collision->plane.n.k = normal[2];
	collision->plane.d = d;
	collision->point.x = end[0] - normal[0] * width;
	collision->point.y = end[1] - normal[1] * width;
	collision->point.z = end[2] - normal[2] * width;
	collision->object_index = NONE;
	collision->surface_index = NONE;
	collision->flags = 0;
	collision->breakable_surface_index = 0;
	collision->material_index = NONE;
	return 1;
}

int ae_forge_vehicle_pulled(real_point3d const *at, long vehicle_index, long *last_vehicle, long *last_time, long grace)
{
	float pull[3];

	return ae_forge_rules_gravity_grace(forge_volumes_gravity_at(at->x, at->y, at->z, pull), vehicle_index,
		game_time_get(), grace, last_vehicle, last_time);
}

int ae_forge_unit_pulled(real_point3d const *at, int grounded, boolean *noted)
{
	float pull[3];

	*noted = (boolean)ae_forge_rules_gravity_noted(forge_volumes_gravity_at(at->x, at->y, at->z, pull), grounded, *noted);
	return *noted;
}

int ae_forge_ai_on;

void ae_forge_ai_set(int on)
{
	ae_forge_ai_on = on;
}

void forge_ai_encounters_apply(void);
void forge_ai_unit_placed(long unit_index, struct actor_starting_location const *location);
struct actor_starting_location *forge_ai_location_for_unit(struct actor_starting_location *location, boolean spawning,
	struct actor_starting_location *place);
short forge_ai_squad_palette_index(void const *squad, short palette_index);
boolean forge_ai_actor_runs(long actor_index);
short forge_ai_route_next_position(long actor_index, short move_position_order, short current_position_index, boolean *handled);
int forge_ai_team_enmity(short our_team_index, short other_team_index);

void ae_forge_ai_encounters(void)
{
	forge_ai_encounters_apply();
}

void ae_forge_ai_unit_placed(long unit_index, struct actor_starting_location const *location)
{
	if (ae_forge_ai_on)
		forge_ai_unit_placed(unit_index, location);
}

struct actor_starting_location *ae_forge_ai_location(struct actor_starting_location *location, boolean spawning,
	struct actor_starting_location *place)
{
	return ae_forge_ai_on ? forge_ai_location_for_unit(location, spawning, place) : location;
}

short ae_forge_ai_palette(void const *squad, short palette_index)
{
	return ae_forge_ai_on ? forge_ai_squad_palette_index(squad, palette_index) : palette_index;
}

boolean ae_forge_ai_runs(long actor_index)
{
	return ae_forge_ai_on ? forge_ai_actor_runs(actor_index) : FALSE;
}

short ae_forge_ai_route(long actor_index, short move_position_order, short current_position_index, boolean *handled)
{
	*handled = FALSE;
	return ae_forge_ai_on ? forge_ai_route_next_position(actor_index, move_position_order, current_position_index, handled) : NONE;
}

int ae_forge_ai_team_enmity(short our_team_index, short other_team_index)
{
	return ae_forge_ai_on ? forge_ai_team_enmity(our_team_index, other_team_index) : NONE;
}

/* what each local player's arms were built with: the hands, and a copy of the arms to draw them from (the globals' is a tag's) */
static long hands_built[MAXIMUM_NUMBER_OF_LOCAL_PLAYERS] = { NONE, NONE, NONE, NONE };
static struct game_globals_first_person_interface hands_copy[MAXIMUM_NUMBER_OF_LOCAL_PLAYERS];
/* (the unit they were built for: a new body is said in the log again) */
static long hands_unit[MAXIMUM_NUMBER_OF_LOCAL_PLAYERS] = { NONE, NONE, NONE, NONE };

struct game_globals_first_person_interface *ae_forge_hands_interface(short local_player_index, long unit_index,
	struct game_globals_first_person_interface *arms, boolean build)
{
	long hands;

	if (local_player_index < 0 || local_player_index >= MAXIMUM_NUMBER_OF_LOCAL_PLAYERS)
		return arms;
	hands = build ? forge_map_settings_hands(unit_index, arms->hands.index) : hands_built[local_player_index];
	/* (said in the log for each new body or change: which hands a player's arms are built with; the level's own only in a
	campaign level, where a set can change them) */
	if (build && (hands != hands_built[local_player_index] || unit_index != hands_unit[local_player_index]) &&
		(hands != NONE && hands != arms->hands.index ? TRUE : !game_engine_running()))
	{
		char const *name = hands != NONE && hands != arms->hands.index ? tag_get_name(hands) : NULL;

		platform_log("hands drawn: player %d: %s", (int)local_player_index + 1, name ? name : "the level's own");
	}
	if (build)
	{
		hands_built[local_player_index] = hands;
		hands_unit[local_player_index] = unit_index;
	}
	if (hands == NONE || hands == arms->hands.index)
		return arms;
	hands_copy[local_player_index] = *arms;
	hands_copy[local_player_index].hands.index = hands;
	return &hands_copy[local_player_index];
}

int forge_is_unit_in_monitor_mode(long unit_index);
extern int g_forge_session_enabled;

int ae_forge_unit_in_monitor_mode(long unit_index)
{
	return g_forge_session_enabled && forge_is_unit_in_monitor_mode(unit_index);
}

void ae_forge_level_forget(void)
{
	short each;

	ae_forge_ai_on = 0;
	for (each = 0; each < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS; each++)
		hands_built[each] = NONE;
}

boolean ae_forge_hands_stale(short local_player_index, long unit_index)
{
	struct game_globals_first_person_interface *arms = TAG_BLOCK_GET_ELEMENT(
		&scenario_get_game_globals()->first_person_interface,
		0,
		struct game_globals_first_person_interface);

	if (local_player_index < 0 || local_player_index >= MAXIMUM_NUMBER_OF_LOCAL_PLAYERS)
		return FALSE;
	return hands_built[local_player_index] != forge_map_settings_hands(unit_index, arms->hands.index);
}

void ae_forge_players_body(long profile, char const *biped, char const *hands, int movement_own)
{
	forge_map_settings_players_body(profile, biped, hands, movement_own);
}

long ae_forge_player_biped(long player_index, long unit_definition_index)
{
	/* (a set's body: its hands and movement are in the layer before the biped is asked for) */
	ae_set_body_players(TRUE);
	return forge_map_settings_player_biped(player_index, unit_definition_index);
}

void ae_forge_player_control(long unit_index, struct unit_control_data const *control)
{
	forge_map_settings_player_control(unit_index, control);
}

int ae_forge_damage_ignored(long owner_object_index, long object_index)
{
	return forge_map_settings_damage_ignored(owner_object_index, object_index);
}

int ae_forge_fog(short local_player_index, int indoors, struct render_fog *fog)
{
	return forge_map_settings_fog(local_player_index, indoors, fog);
}

#else /* OPENCE_ENABLE_FORGE */

int ae_forge_ai_on;

int ae_forge_unit_in_monitor_mode(long unit_index)
{
	(void)unit_index;
	return 0;
}

void ae_forge_ai_set(int on)
{
	(void)on;
}

void ae_forge_ai_encounters(void)
{
}

void ae_forge_ai_unit_placed(long unit_index, struct actor_starting_location const *location)
{
	(void)unit_index;
	(void)location;
}

struct actor_starting_location *ae_forge_ai_location(struct actor_starting_location *location, boolean spawning,
	struct actor_starting_location *place)
{
	(void)spawning;
	(void)place;
	return location;
}

short ae_forge_ai_palette(void const *squad, short palette_index)
{
	(void)squad;
	return palette_index;
}

boolean ae_forge_ai_runs(long actor_index)
{
	(void)actor_index;
	return FALSE;
}

short ae_forge_ai_route(long actor_index, short move_position_order, short current_position_index, boolean *handled)
{
	(void)actor_index;
	(void)move_position_order;
	(void)current_position_index;
	*handled = FALSE;
	return NONE;
}

int ae_forge_ai_team_enmity(short our_team_index, short other_team_index)
{
	(void)our_team_index;
	(void)other_team_index;
	return NONE;
}

int ae_forge_gravity_pull(real_point3d const *at, float pull[3])
{
	(void)at;
	(void)pull;
	return 0;
}

void ae_forge_gravity_correct(real_vector3d *vector, float const pull[3], real amount)
{
	(void)vector;
	(void)pull;
	(void)amount;
}

int ae_forge_gravity_correct_at(real_vector3d *vector, real_point3d const *at, real amount)
{
	(void)vector;
	(void)at;
	(void)amount;
	return 0;
}

real ae_forge_physics_gravity(real_point3d const *at, real mass, real gravity, real_vector3d *force)
{
	(void)at;
	(void)mass;
	(void)gravity;
	(void)force;
	return global_gravity;
}

int ae_forge_biped_fall_protected(long biped_index, real_point3d const *at, int airborne)
{
	(void)biped_index;
	(void)at;
	(void)airborne;
	return 0;
}

int ae_forge_biped_stop_inertia(long biped_index, real_point3d const *at)
{
	(void)biped_index;
	(void)at;
	return 0;
}

int ae_forge_biped_lifted(float const pull[3], real_vector3d const *ground_normal)
{
	(void)pull;
	(void)ground_normal;
	return 0;
}

int ae_forge_biped_one_way(real_point3d const *from, real_point3d *to, real_vector3d *velocity, real width,
	struct collision_plane *collision)
{
	(void)from;
	(void)to;
	(void)velocity;
	(void)width;
	(void)collision;
	return 0;
}

int ae_forge_vehicle_pulled(real_point3d const *at, long vehicle_index, long *last_vehicle, long *last_time, long grace)
{
	(void)at;
	(void)vehicle_index;
	(void)last_vehicle;
	(void)last_time;
	(void)grace;
	return 0;
}

int ae_forge_unit_pulled(real_point3d const *at, int grounded, boolean *noted)
{
	(void)at;
	(void)grounded;
	(void)noted;
	return 0;
}

struct game_globals_first_person_interface *ae_forge_hands_interface(short local_player_index, long unit_index,
	struct game_globals_first_person_interface *arms, boolean build)
{
	(void)local_player_index;
	(void)unit_index;
	(void)build;
	return arms;
}

void ae_forge_level_forget(void)
{
}

boolean ae_forge_hands_stale(short local_player_index, long unit_index)
{
	(void)local_player_index;
	(void)unit_index;
	return FALSE;
}

void ae_forge_players_body(long profile, char const *biped, char const *hands, int movement_own)
{
	(void)profile;
	(void)biped;
	(void)hands;
	(void)movement_own;
}

long ae_forge_player_biped(long player_index, long unit_definition_index)
{
	(void)player_index;
	return unit_definition_index;
}

void ae_forge_player_control(long unit_index, struct unit_control_data const *control)
{
	(void)unit_index;
	(void)control;
}

int ae_forge_damage_ignored(long owner_object_index, long object_index)
{
	(void)owner_object_index;
	(void)object_index;
	return 0;
}

int ae_forge_fog(short local_player_index, int indoors, struct render_fog *fog)
{
	(void)local_player_index;
	(void)indoors;
	(void)fog;
	return 0;
}

#endif /* OPENCE_ENABLE_FORGE */
