/*
AE_FORGE_ENGINE.H

The engine side of Forge's volumes and map settings (ae_forge_rules.h has the numbers and the math): what the hook lines of
the physics, the damage, the player and the render files ask. A build with no Forge (only the 64-bit Linux build has it) answers
as the game does: no volume, no map setting.

A GRAVITY volume's pull is in units of the world's gravity, the vector the way things fall: straight down in the world's own
(0, 0, -1). Where the engine takes a gravity off a velocity, the hook leaves that code as it was and asks for the difference
(ae_forge_gravity_correct_at), which is nothing out of every volume.
*/

#ifndef __AE_FORGE_ENGINE_H
#define __AE_FORGE_ENGINE_H

#include "cseries.h"
#include "math/real_math.h"

struct collision_plane;
struct game_globals_first_person_interface;
struct render_fog;

/* the pull at a point: 0 out of every GRAVITY volume (pull left as it was), else 1 and the pull */
int ae_forge_gravity_pull(real_point3d const *at, float pull[3]);

/* a vector the world's gravity moved by `amount` (what a tick took off it) moved as the volumes' pull at a point moves it:
1 and the difference added when the point is in a volume, else 0 and nothing changed */
int ae_forge_gravity_correct_at(real_vector3d *vector, real_point3d const *at, real amount);
/* the same for a pull asked for already (a projectile's, at the start of its tick) */
void ae_forge_gravity_correct(real_vector3d *vector, float const pull[3], real amount);

/* a physics object's force of gravity (the world's, mass * gravity straight down, already in `force`) in the pull at a point
(`gravity` the object's: the world's times its scale): the force set to the pull's; and how hard the ground holds it up, the
world's gravity (global_gravity) as it was, or as strong as the pull is */
real ae_forge_physics_gravity(real_point3d const *at, real mass, real gravity, real_vector3d *force);

/* a biped's tick: whether it is held off falling damage (volumes with FALL DAMAGE LANDING or EXIT), and whether it comes into one
with REMOVE INERTIA (once, as it does) */
int ae_forge_biped_fall_protected(long biped_index, real_point3d const *at, int airborne);
int ae_forge_biped_stop_inertia(long biped_index, real_point3d const *at);
/* a biped on the ground in a pull that takes it off it (turned over, or past a side) is in the air: 1 when the pull and the ground
plane's normal say so */
int ae_forge_biped_lifted(float const pull[3], real_vector3d const *ground_normal);
/* ONE WAY volumes against a biped's move (its pill's bottom sphere, `width` its radius): 1 when the top of one stops it coming down
on it: the move's end, and its velocity, set on it, and `collision` the floor as one more of the move's collisions */
int ae_forge_biped_one_way(real_point3d const *from, real_point3d *to, real_vector3d *velocity, real width,
	struct collision_plane *collision);

/* the host's reading of a client's vehicle in the air (network_objects.c): in a volume now, or lately (the note kept in
last_vehicle and last_time, the grace in ticks): no ceiling for it */
int ae_forge_vehicle_pulled(real_point3d const *at, long vehicle_index, long *last_vehicle, long *last_time, long grace);
/* the host's reading of a client's unit on foot (network_distributed.c): pulled by a volume since it last stood on the ground
out of every one (kept in noted): its speed and its rise and fall are not bounded */
int ae_forge_unit_pulled(real_point3d const *at, int grounded, boolean *noted);

/* the arms of a local player: the first-person arms as Forge's map has its hands for the unit (a team's hands), the
globals' own when it sets none. `build` is the arms being set up for the unit (what is built is kept: stale says when the hands
wanted are not those); else the arms drawn, with what was built. The arms given is returned when its hands are the ones */
struct game_globals_first_person_interface *ae_forge_hands_interface(short local_player_index, long unit_index,
	struct game_globals_first_person_interface *arms, boolean build);
/* whether a unit is a player's body in Forge's Monitor mode (the Monitor is drawn in its place: nothing of the body, nor what is drawn with it,
shows or casts a shadow); 0 in a game that is no Forge session */
int ae_forge_unit_in_monitor_mode(long unit_index);
/* a level loading or going: what the arms were built with is forgotten (a tag index is of one level), and the level has no Forge AI */
void ae_forge_level_forget(void);
/* whether the arms of a local player were built for other hands than the unit's now wanted */
boolean ae_forge_hands_stale(short local_player_index, long unit_index);

/* a campaign level's body for every player (a set's YOUR BODY, ae_set_body.c): the biped's path, its hands' (NULL: those of the biped's
map in the set's profile that fit it best; "": the campaign's own) and whether it moves as its tag has it, else as the campaign's
player does; an empty biped gives the layer back empty. A build with no Forge has no layer */
void ae_forge_players_body(long profile, char const *biped, char const *hands, int movement_own);

/* the biped a player spawns as: the map's setting for the player's team, else the one given */
long ae_forge_player_biped(long player_index, long unit_definition_index);
/* a local player's unit's controls each tick, which a test spawn's copy mirrors */
struct unit_control_data;
void ae_forge_player_control(long unit_index, struct unit_control_data const *control);
/* whether damage is the test spawn's copy's, which hurts no one and is not hurt */
int ae_forge_damage_ignored(long owner_object_index, long object_index);
/* the atmosphere's fog and sky over the map's own: 1 when the sky is hidden by it */
int ae_forge_fog(short local_player_index, int indoors, struct render_fog *fog);

/* ---------- AI (forge_ai.c): the hooks of the AI's files ask here. A level has Forge AI once forge_ai_encounters_apply built it from spawns (a
Forge session's, or a saved Forge map's); until then, and in every other level, each answers as the game does and does nothing more than
test that flag */

struct actor_starting_location;
/* whether the level has Forge AI (set as it is built, cleared as a level loads or goes) */
extern int ae_forge_ai_on;
void ae_forge_ai_set(int on);
/* ai_initialize_for_new_map: the level's encounters as the spawns have them */
void ae_forge_ai_encounters(void);
/* an actor placed: its own weapon, cloaking and height */
void ae_forge_ai_unit_placed(long unit_index, struct actor_starting_location const *location);
/* where a unit of a squad starts: the location given, or `place` made as the spawn lays its units out */
struct actor_starting_location *ae_forge_ai_location(struct actor_starting_location *location, boolean spawning,
	struct actor_starting_location *place);
/* the palette entry of a squad's next unit: the one given, or a random spawn's any of its ranks */
short ae_forge_ai_palette(void const *squad, short palette_index);
/* a goal's unit goes at a fight's pace out of one */
boolean ae_forge_ai_runs(long actor_index);
/* a ROUTE or PATROL's next move position: *handled FALSE (and NONE) when it is none of Forge's */
short ae_forge_ai_route(long actor_index, short move_position_order, short current_position_index, boolean *handled);
/* ALL PLAYERS' spawns, friends of every player: NONE, else whether the two are enemies */
int ae_forge_ai_team_enmity(short our_team_index, short other_team_index);

#endif /* __AE_FORGE_ENGINE_H */
