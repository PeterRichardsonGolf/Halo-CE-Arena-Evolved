/*
FORGE_VOLUMES.H

Forge's volumes (port/forge/src/forge_volumes.c): places in a map that hurt
or kill whoever is in them, placed from the palette's VOLUMES tab as boxes
(a cube at first), cylinders and spheres, each sized on its three axes; and
the map's own deadly trigger volumes (death_volumes.c's), which keep their
shape (a box of any size) and can be moved, sized and set too.

A volume is a marker-only live instance ('fvol', no object), standing at its
middle; what it is (shape, effect, damage, size) is kept here by its live
index. Its effect:

- APPLY DAMAGE: an amount (a percentage of the unit's health, shields first)
  once, as a unit comes into it;
- CONSTANT DAMAGE: the amount each second while a unit is in it;
- INSTA KILL: a unit in it dies;
- and a map's volume's own MAP SCRIPT (the map's scripts do as they did), or
  OFF;
- GRAVITY: whatever is in it falls towards the volume's own down (its
  bottom face, as it is turned), at STRENGTH times the world's gravity (x0
  to x10): turned over, things fall up; on its side, sideways. Where gravity
  volumes overlap their pulls add up; out of every one, the world's gravity
  again. Players and AI (in the air, or pulled off the ground they stand on),
  vehicles, dropped weapons and equipment, grenades and other arcing
  projectiles, and ragdolls (forge_volumes_gravity_at, asked by the engine's
  physics). Every machine runs it on its own copy of the volumes;
- ONE WAY: its top face (its own up, as it is turned; a sphere's that of
  the cylinder around it) is a floor bipeds stand on when they come down on
  it, and pass through from under it, or from its sides
  (forge_volumes_one_way_floor, asked by bipeds.c's biped_update_physics
  after the pill's move): a grav lift's top. Bipeds alone: vehicles, items
  and projectiles pass through it, and AI find no path on it.

Any volume also has (FORGE_VOLUME_FLAG_*):

- FALL DAMAGE: VANILLA (as the game does), LANDING (a biped that was in it
  takes none until it next stands on the ground: a launcher's), or EXIT (a
  biped takes none while it is in it: a landing zone's)
  (forge_volumes_fall_protected, asked by bipeds.c's biped_falling_damage);
- REMOVE INERTIA: a biped coming into it stops dead, and moves on from
  there as its gravity and its legs take it (forge_volumes_stop_inertia,
  asked by bipeds.c's biped_update_physics).

Every unit is a biped here: one in a vehicle is in a volume if it is. The
host (or a game with no network) hurts them, each tick of a game on the map,
in Forge or not; the network carries the units' health.

A saved map keeps them on VOLUME| lines of its trailer (forge_cache_builder.c),
read as the level's saved state is (forge_runtime_tags.c); a Forge session
sends a volume's settings as !FW: (forge_network.c).

Placed emitters (fire, plasma fire) keep their own damage, which their
effects deal: their Object Settings' DEAL DAMAGE turns it off (no_damage on
the live instance; effects.c asks forge_volumes_effect_damage_blocked).
*/

#ifndef __FORGE_VOLUMES_H
#define __FORGE_VOLUMES_H

#include <stdint.h>

/* ---------- constants */

#define FORGE_GROUP_VOLUME 'fvol'

#define FORGE_VOLUME_BOX_PATH "volume\\box"
#define FORGE_VOLUME_CYLINDER_PATH "volume\\cylinder"
#define FORGE_VOLUME_SPHERE_PATH "volume\\sphere"
/* (a map's own trigger volume, adopted: forge_volumes_adopt_map) */
#define FORGE_VOLUME_MAP_PATH "volume\\map"

/* (world units: each axis's size, from a hand's width to a valley's) */
#define FORGE_VOLUME_SIZE_DEFAULT 2.0f
#define FORGE_VOLUME_SIZE_MIN 0.25f
#define FORGE_VOLUME_SIZE_MAX 200.0f

enum forge_volume_shape
{
	_forge_volume_box = 0,
	_forge_volume_cylinder,
	_forge_volume_sphere,
	/* a map's trigger volume: a box of its own size, oriented as it is */
	_forge_volume_map_box,
	NUMBER_OF_FORGE_VOLUME_SHAPES
};

enum forge_volume_effect
{
	_forge_volume_apply_damage = 0,
	_forge_volume_constant_damage,
	_forge_volume_insta_kill,
	/* (a map's volume's alone: its scripts kill in it, as they did) */
	_forge_volume_map_script,
	_forge_volume_off,
	/* (after the others: saved maps and older peers keep their numbers) */
	_forge_volume_gravity,
	_forge_volume_one_way,
	/* AI PATROL: no effect on what is in it; the ground in it is where the AI
	spawns linked to it (their GOAL's PATROL VOLUME) roam, by its number, and
	`damage` is its PURSUIT RANGE (world units past its sides they chase out
	to) */
	_forge_volume_ai_patrol,
	NUMBER_OF_FORGE_VOLUME_EFFECTS
};

/* the rows of a volume's Object Settings (forge_3d_mode.c) */
enum forge_volume_row
{
	_forge_volume_row_shape = 0,
	_forge_volume_row_effect,
	_forge_volume_row_damage,
	/* (FALL DAMAGE: FORGE_VOLUME_FALL_*; REMOVE INERTIA) */
	_forge_volume_row_fall_damage,
	_forge_volume_row_inertia,
	_forge_volume_row_size_x,
	_forge_volume_row_size_y,
	_forge_volume_row_size_z,
	_forge_volume_row_rot_snap,
	_forge_volume_row_floor,
	_forge_volume_row_delete,
	/* (the pages: EFFECT > and SIZE > on the first, and theirs) */
	_forge_volume_row_effect_page,
	_forge_volume_row_size_page,
	/* (SIZE's KEEP RATIO, and its FACTOR, the sizes it was turned on with
	times it) */
	_forge_volume_row_keep_ratio,
	_forge_volume_row_factor,
	_forge_volume_row_back,
	NUMBER_OF_FORGE_VOLUME_ROWS
};

#define FORGE_VOLUME_ROW_IS_PAGE_BUTTON(row) ((row) == _forge_volume_row_effect_page || \
	(row) == _forge_volume_row_size_page || (row) == _forge_volume_row_back)
/* forge_volume_row_step's results past changed (1) */
#define FORGE_VOLUME_PRESS_PAGE_OPENED 2
#define FORGE_VOLUME_PRESS_PAGE_CLOSED 3

/* a volume's flags (forge_volume.flags) */
enum
{
	/* its FALL DAMAGE (the low two bits): VANILLA; LANDING, none for a biped
	that was in it until it lands (1, a GRAVITY volume's FALL DAMAGE OFF of
	before); EXIT, none while it is in it */
	FORGE_VOLUME_FALL_MASK = 3,
	FORGE_VOLUME_FALL_VANILLA = 0,
	FORGE_VOLUME_FALL_LANDING = 1,
	FORGE_VOLUME_FALL_EXIT = 2,
	NUMBER_OF_FORGE_VOLUME_FALL_MODES = 3,
	/* REMOVE INERTIA: a biped coming into it stops */
	FORGE_VOLUME_FLAG_STOP_INERTIA = 1 << 2,
	/* an AI PATROL volume's number (1 to 31, given as it becomes one, never
	changed; 0: none), in the top five bits, so the saved line and !FW: carry
	it as they carry the flags */
	FORGE_VOLUME_PATROL_SHIFT = 3,
	FORGE_VOLUME_PATROL_MASK = 0x1F << 3,
	FORGE_VOLUME_PATROL_MAX = 31,
	FORGE_VOLUME_FLAGS_ALL = FORGE_VOLUME_FALL_MASK | FORGE_VOLUME_FLAG_STOP_INERTIA | FORGE_VOLUME_PATROL_MASK
};

#define FORGE_VOLUME_PATROL_NUMBER(flags) ((((int)(flags)) & FORGE_VOLUME_PATROL_MASK) >> FORGE_VOLUME_PATROL_SHIFT)
/* (an AI PATROL volume's PURSUIT RANGE: a new one's, and the most) */
#define FORGE_VOLUME_PURSUIT_DEFAULT 10.0f
#define FORGE_VOLUME_PURSUIT_MAX 100.0f

/* ---------- structures */

struct forge_volume
{
	uint8_t used;
	uint8_t shape;
	uint8_t effect;
	/* (moved, sized or set since the level loaded: a map's is written back
	to its trigger volume) */
	uint8_t edited;
	/* the map's trigger volume it is (NONE: one of Forge's) */
	int16_t map_index;
	/* FORGE_VOLUME_FLAG_* */
	uint8_t flags;
	uint8_t pad;
	/* each axis's size: a box's edges, a cylinder's two diameters and its
	height, a sphere's three diameters (world units) */
	float size[3];
	/* a percentage of a unit's health: once (APPLY DAMAGE) or each second
	(CONSTANT DAMAGE); GRAVITY's STRENGTH in hundredths of the world's
	gravity (100 is x1, 0 to 1000), so the saved line and !FW: carry it as
	they carry damage */
	float damage;
};

/* ---------- prototypes/FORGE_VOLUMES.C */

/* a live instance's volume (NULL: none) */
struct forge_volume const *forge_volume_get(int32_t live_idx);
int forge_volume_is(int32_t live_idx);
/* a volume made for a live instance just spawned with a volume's path (a
map's keeps what it was adopted with) */
void forge_volume_spawned(int32_t live_idx, char const *tag_path);
/* a volume moved (its map trigger volume follows) */
void forge_volume_moved(int32_t live_idx);
/* a volume's settings set, and sent to the session's others (broadcast) */
void forge_volume_set(int32_t live_idx, struct forge_volume const *volume, int broadcast);
/* a peer's !FW: fields, or a saved line's (flags: FORGE_VOLUME_FLAG_*) */
void forge_volume_apply(int32_t live_idx, int shape, int effect, float damage, float const size[3], int flags);

/* the map's deadly trigger volumes, as volumes of its own (once a level,
forge_init_map_scenario_entities, on a multiplayer map) */
void forge_volumes_adopt_map(void);
/* a level going: every volume forgotten */
void forge_volumes_level_reset(void);

/* the text of a volume's rows (forge_3d_mode.c's panel) */
void forge_volume_row_text(int32_t live_idx, int row, char *label, unsigned long label_size, char *value,
	unsigned long value_size);
/* a row stepped (`dir` -1/1); 1 when it changed something, or
FORGE_VOLUME_PRESS_PAGE_OPENED/CLOSED */
int forge_volume_row_step(int32_t live_idx, int row, int dir);
/* the rows of the page shown, in order (room for NUMBER_OF_FORGE_VOLUME_ROWS) */
int forge_volume_rows(int32_t live_idx, int *rows);
/* a row shown but not to be changed (a size while KEEP RATIO is on) */
int forge_volume_row_disabled(int32_t live_idx, int row);
/* the page row (EFFECT >, SIZE >) the last page closed came back to */
int forge_volume_page_return_row(void);
/* a page closed (back/Esc): 1 when one was open */
int forge_volume_page_back(int32_t live_idx);
/* the panel's title on its page */
char const *forge_volume_panel_title(int32_t live_idx);
/* a volume's name: what it is and does ("KILL BOX") */
void forge_volume_describe(int32_t live_idx, char *text, unsigned long size);

/* the saved map's lines (forge_cache_builder.c): how many bytes */
int forge_volumes_save_lines(char *text, unsigned long size);
/* a saved line (forge_runtime_tags.c's VOLUME|...): the volume made */
void forge_volumes_load_line(char const *line);

/* the network's (forge_network.c): a volume's !FW: packet; and every volume
to a machine joining */
int forge_volume_packet(int32_t live_idx, char *text, unsigned long size);

/* each frame of a level (forge_3d_update): the units in the volumes hurt
(the host's), the map's volumes written back */
void forge_volumes_update(void);

/* the gravity at a point (the engine's physics): FALSE when it is in no
GRAVITY volume (the world's own, straight down, applies); TRUE and the pull,
in units of the world's gravity (global_gravity), as a vector pointing the
way things fall: the GRAVITY volumes it is in, added up (out_pull is left
as it was when FALSE) */
int forge_volumes_gravity_at(float x, float y, float z, float out_pull[3]);

/* a biped's falling damage held off (bipeds.c's biped_falling_damage, each
tick): TRUE while it is in a volume with FALL DAMAGE LANDING or EXIT, and,
after a LANDING one, until (and on the tick) it stands on the ground again
(`airborne` FALSE) */
int forge_volumes_fall_protected(long biped_index, float x, float y, float z, int airborne);
/* a biped's inertia taken (bipeds.c's biped_update_physics, each tick): TRUE
on the tick it comes into a volume with REMOVE INERTIA (each one it comes
into, once, till it leaves it) */
int forge_volumes_stop_inertia(long biped_index, float x, float y, float z);

/* a biped's move (its pill's bottom sphere's middle, `from` to `to`, of
`radius`) against the ONE WAY volumes' tops (bipeds.c, after the pill's
move): TRUE when one stops it coming down, `to` set on it and `velocity`'s
part into it taken away, and the top's plane (the normal, the way it is let
through, and d: n.x = d on it) */
int forge_volumes_one_way_floor(float const from[3], float to[3], float velocity[3], float radius, float out_normal[3],
	float *out_d);

/* their shapes in the world, in Forge mode (forge_runtime_tags.c's world
pass): each edge a line drawn by `draw_line`, and their faces tinted by
`draw_face` as parallelograms (a corner and its two sides); `color` is the
effect's (0 hurts, 1 kills, 2 does nothing, 3 pulls, 4 is ONE WAY, 5 is AI PATROL,
6 its PURSUIT RANGE, drawn round it), `highlighted` the one held or pointed at */
void forge_volumes_render(
	void (*draw_line)(float const *from, float const *to, int color, int highlighted),
	void (*draw_face)(float const *corner, float const *side_a, float const *side_b, int color, int highlighted));
/* the volume whose edge the view's ray passes closest to (within a margin
that widens with distance), no further along it than max_along (the first
surface it meets): its live index, and the point on that edge it was picked
at; -1 for none */
int32_t forge_volumes_pick_edge(float const origin[3], float const direction[3], float max_along, float out_point[3]);

/* ---------- AI PATROL volumes (forge_ai.c's goals) */

/* the live index of the AI PATROL volume of this number, -1 for none */
int32_t forge_volume_patrol_find(int number);
/* the AI PATROL volumes' numbers, lowest first: how many (at most `max`) */
int forge_volumes_patrol_numbers(int *numbers, int max);
/* the lowest number no AI PATROL volume has, 0 when all have one */
int forge_volume_patrol_free_number(void);
/* whether a point is in a volume of this shape, standing at `position` turned
by yaw, pitch and roll (radians) and of `size` (each axis's), each side `margin`
further out */
int forge_volume_shape_contains(int shape, float const position[3], float yaw, float pitch, float roll,
	float const size[3], float margin, float const point[3]);

/* whether an emitter's tag deals damage: one of its effects' parts is a
damage effect (DEAL DAMAGE shows for it) */
int forge_volumes_tag_deals_damage(long definition_index);
/* an effect on an object whose damage is off (effects.c): TRUE when its
damage parts must do nothing */
int forge_volumes_effect_damage_blocked(long object_index);
/* a placed emitter's DEAL DAMAGE (no_damage on its live instance), kept in
its record for the save (Object Settings, a peer's !FN:) */
void forge_3d_no_damage_apply(int32_t live_idx, int no_damage);
void forge_network_send_no_damage(int32_t live_idx, int no_damage);
/* a placed scenery's COLLISION off (no_collision on its live instance): seen,
but nothing collides with it (_object_no_collisions_bit, kept on it each
frame by forge_3d_no_collision_update; Forge's own pointing still finds it).
Saved as OBJECT_NO_COLLISION, sent as !FK: */
void forge_3d_no_collision_apply(int32_t live_idx, int no_collision);
void forge_network_send_no_collision(int32_t live_idx, int no_collision);
void forge_network_send_volume(int32_t live_idx);

#endif
