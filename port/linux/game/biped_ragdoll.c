/*
BIPED_RAGDOLL.C

A body falls as a ragdoll (game.ragdoll_players, game.ragdoll_ai), in a
campaign or a multiplayer game, in place of its dying animation and the limp
body it lies as afterwards (bipeds.c, biped_limp_noodle.c). Each choice is a
row of ragdoll_modes:
- "spaghetti_western": the ragdoll below, from the tick its unit dies.
- "exaggerated": from the tick it dies too, a heavy torso, limbs with a
particle midway along each that keep out of the torso and each other, joints
that drag and twist less, and blasts that throw it further.
- "modern": after the later game's ragdolls (rigid bodies of some size,
with friction in their joints). Limp from the tick it dies, whatever killed
it (no stock death animation at all), turned about its middle by the killing
blow and popped up. A torso of one rigid piece, heavy legs and light
arms, joints that bend only so far (looser and draggier for a moment after
it goes limp), more gravity, almost no drag in the air, and friction and a
little bounce on the ground.
- "mixed": the stock death animation as it plays (its fall, its pose) for
most of its length, then limp, as it nears its end or once it lies on the
ground, with modern's body and physics to settle it; blasts push it a
little less, and nothing turns it as it goes limp. A blast's kill goes limp
at once, pushed as modern's is. (Kept simple: no more than that.)

Each of the model's nodes is a particle, and the head has one more at its
top. Bones keep their lengths, the joints where a bone branches (the hips,
the chest) are rigid, and a joint bends no further than it can (the spine and
neck a little, the limbs a long way). Each tick, in two halves: gravity and
the particles' speeds move them, the bones pull them back together, and every
move is clipped against the world (the structure and the objects a dead
biped's physics collides with). The nodes' rotations follow their bones,
twisting no more than their parents do. A body that lies still for half a
second, or has fallen for ten, sleeps until a blast moves it again.

The killing blow pushes the body hardest nearest where it hit (an
explosion's center, a bullet's impact), so a body spins and folds as it was
hit, and a later blast moves it too. A body that waits on its death
animation was pushed by the stock game already, and goes limp at the speeds
its pose had.

It is drawn, not played. On a client, or in a game of one machine, the
ragdoll moves the body itself: its nodes, its origin and its bounds, in place
of its physics and its dying animation. A client decides nothing of the game
(the host does), so the host's word on where the body is doesn't move it
(network_objects.c). The host of a network game, whose bodies' places and
poses decide what its shots hit and what it sends its clients, leaves its
bodies as the stock game moves and poses them, and only draws them as their
ragdolls lie (render_interpolation.c). So a host sends its clients what any
build expects, and machines with and without ragdolls (the setting off, or a
build without it, at the same network version) play together, each drawing
bodies its own way. Every machine runs its own ragdolls from its own copy of
the killing blow (the host's, replayed on the clients: network_damage.c).
The weapons it drops are the host's, as before.
*/

#include "cseries.h"
#include "cutscene/cinematics.h"
#include "game/game.h"
#include "game/players.h"
#include "math/real_math.h"
#include "models/model_animation_definitions.h"
#include "models/model_definitions.h"
#include "objects/objects.h"
#include "objects/object_definitions.h"
#include "physics/collision_features.h"
#include "physics/collision_usage.h"
#include "physics/collisions.h"
#include "physics/physics.h"
#include "units/biped_definitions.h"
#include "units/bipeds.h"
#include "units/unit_definitions.h"
#include "units/units.h"
#include "biped_ragdoll.h"
#include "ae_ragdoll_rules.h"
void platform_log(const char *format, ...);
#include <stdlib.h>

/* port/linux/src/port_config.c */
const char *config_string(const char *name);
unsigned long config_changes(void);
/* port/linux/game/object_bounds_cache.c */
void object_bounds_cache_update(long object_index, real_point3d const *center, real radius);

/* ---------- constants */

enum
{
	MAXIMUM_RAGDOLLS = 32,
	MAXIMUM_RAGDOLL_TIPS = 4,
	MAXIMUM_RAGDOLL_MIDDLES = 16,
	MAXIMUM_RAGDOLL_PARTICLES = MAXIMUM_NODES_PER_MODEL + MAXIMUM_RAGDOLL_TIPS + MAXIMUM_RAGDOLL_MIDDLES,
	MAXIMUM_RAGDOLL_CONSTRAINTS = 6 * MAXIMUM_RAGDOLL_PARTICLES,
	MAXIMUM_RAGDOLL_BLOWS = 16,
	MAXIMUM_RAGDOLL_JOINTS = 16,

	RAGDOLL_SUBSTEPS = 2,
	RAGDOLL_ITERATIONS = 6,
	/* still this long, it sleeps; falling this long, it sleeps anyway */
	RAGDOLL_SLEEP_TICKS = TICKS_PER_SECOND / 2,
	RAGDOLL_MAXIMUM_TICKS = 10 * TICKS_PER_SECOND,
	/* a blow this many ticks before it starts pushes it */
	RAGDOLL_BLOW_TICKS = 2,
};

/* game.ragdoll_players and game.ragdoll_ai: how a body falls (a row of
ragdoll_modes each, but none) */
enum
{
	_ragdoll_style_none = 0,
	/* light, even limbs, swinging freely within their joints */
	_ragdoll_style_spaghetti_western,
	/* a heavy torso, limbs of some thickness that keep out of it and of each
	other, joints that drag and twist little, thrown further by blasts */
	_ragdoll_style_exaggerated,
	/* rigid bodies' ragdolls: limp from the tick it dies */
	_ragdoll_style_modern,
	/* the death animation's fall, then modern's ragdoll */
	_ragdoll_style_mixed,
	NUMBER_OF_RAGDOLL_STYLES
};

/* when a body goes limp */
enum
{
	/* the tick it dies */
	_ragdoll_start_at_death = 0,
	/* its death animation played for a moment, or as it lands from the air */
	_ragdoll_start_after_moment,
	/* its death animation near its end, or once it lies on the ground (or
	lands) after a moment */
	_ragdoll_start_near_end,
};

/* the parts of the body, by their nodes' names (as Halo's bipeds name them) */
enum
{
	_ragdoll_part_other = 0,
	/* the pelvis, the spine and the neck */
	_ragdoll_part_torso,
	_ragdoll_part_clavicle,
	_ragdoll_part_head,
	_ragdoll_part_thigh,
	_ragdoll_part_upper_arm,
	_ragdoll_part_calf,
	_ragdoll_part_forearm,
	/* a foot, a toe */
	_ragdoll_part_foot,
	/* a hand, a finger */
	_ragdoll_part_hand,
	NUMBER_OF_RAGDOLL_PARTS
};

enum
{
	_ragdoll_constraint_length = 0,
	/* no shorter than its length: how far a joint bends */
	_ragdoll_constraint_minimum,
};

enum
{
	/* bends one way, in one plane: a knee, an elbow */
	_ragdoll_joint_hinge = 0,
	/* swings within a cone: a hip, a shoulder */
	_ragdoll_joint_cone,
};

enum
{
	/* the longest a body waits on its death animation */
	RAGDOLL_LONGEST_WAIT_TICKS = 3 * TICKS_PER_SECOND,
	/* a body limp this soon after the killing blow is kicked by it (a
	tenth of a second) */
	RAGDOLL_KICK_TICKS = 3,
	/* how long a body's joints take to tighten to their limits (1.25
	seconds) */
	RAGDOLL_LOOSE_TICKS = 5 * TICKS_PER_SECOND / 4,
};

/* a particle's distance from the world (world units: a node's thickness,
less than an ankle's height off the ground) */
#define RAGDOLL_RADIUS 0.025f
/* world units a tick */
#define RAGDOLL_MAXIMUM_SPEED 0.5f
#define RAGDOLL_SLEEP_SPEED 0.002f
/* the fastest a node of a death animation's pose starts at (world units a
tick: a jump in the pose is no throw) */
#define RAGDOLL_POSE_MAXIMUM_SPEED 0.2f
/* how far from a blow's epicenter its push falls to half */
#define RAGDOLL_BLOW_RADIUS 0.12f
#define RAGDOLL_HIT_RADIUS 0.05f
/* the head's top, from the neck */
#define RAGDOLL_TIP_MINIMUM_LENGTH 0.04f
#define RAGDOLL_TIP_MAXIMUM_LENGTH 0.08f
/* how far the spine, neck and head bend from straight */
#define RAGDOLL_STIFF_BEND_COSINE 0.819f /* 35 degrees */
/* how much of the way back a joint is put each time */
#define RAGDOLL_JOINT_STIFFNESS 0.35f
/* how far limbs keep out of the torso and each other */
#define RAGDOLL_TORSO_CLEARANCE 0.055f
#define RAGDOLL_LIMB_CLEARANCE 0.03f
/* a killing blow this strong (its damage's acceleration, world units a
second) is a blast's, and one stronger than the next a big blast's */
#define RAGDOLL_BLAST_ACCELERATION 2.f
#define RAGDOLL_BIG_BLAST_ACCELERATION 5.f
/* a landing faster than this into the ground bounces (world units a tick) */
#define RAGDOLL_BOUNCE_SPEED 0.02f
/* the start's kick: how hard it turns the body (world units a second,
before the random part), and the random parts of the turn and the pop up */
#define RAGDOLL_KICK_SCALE 0.7f
#define RAGDOLL_KICK_MINIMUM 1.5f
#define RAGDOLL_KICK_MAXIMUM 3.25f
#define RAGDOLL_KICK_LIFT_MINIMUM 0.75f
#define RAGDOLL_KICK_LIFT_MAXIMUM 1.5f
/* what the kick gives the far side of the body, and the part hit */
#define RAGDOLL_KICK_FAR_SIDE 0.65f
#define RAGDOLL_KICK_HIT_PART 2.f
/* (MODERN) the body's acceleration scale for those pushes, and the push
(wu/s) above which a blow tumbles the body */
#define RAGDOLL_CATEGORY_PUSH_SCALE 1.2f
#define RAGDOLL_TUMBLE_SPEED 1.4f
/* lying on the ground: the body's nodes within this part of the height they
spanned when it died */
#define RAGDOLL_LYING_SPAN 0.3f

/* ---------- structures */

/* how a body falls, one row of ragdoll_modes a style */
struct ragdoll_mode
{
	/* when it goes limp (_ragdoll_start_...): after this many ticks of its
	death animation, or once this part of it has played */
	short start;
	short wait_ticks;
	real played;
	/* gravity, as the game's times this; what a particle's speed keeps each
	half tick */
	real gravity;
	real damping;
	/* along what it lies on: what its speed keeps each half tick, or, when
	coulomb is not 0, less by coulomb times how hard it presses (a dry
	friction); and how much of a landing's speed it bounces back with */
	real friction;
	real coulomb;
	real restitution;
	/* a part's speed about its parent's each half tick: less by this part of
	it (a drag), and by this much (world units a tick: a dry friction) */
	real joint_drag;
	real joint_friction;
	/* how hard blasts push it: the killing blow, a later blast, and both
	times big_blast again above RAGDOLL_BIG_BLAST_ACCELERATION; the whole
	body's push the way any killing blow came and the shot's own where it
	hit (world units a tick: most bullets' damage pushes nothing) */
	real kill_blast;
	real blast;
	real big_blast;
	real blow_speed;
	real hit_speed;
	/* MODERN's pushes: limp within RAGDOLL_KICK_TICKS of its killing
	blow, turned about a level axis the way the blow came and popped up; a
	bullet's or a melee's push by its category (CE's tags push them 0); a hard
	blow's tumble (ae_ragdoll_rules.h) */
	boolean kick;
	/* the pelvis, spine, neck and clavicles one rigid piece, the hips' and
	shoulders' joints fixed in it */
	boolean rigid_torso;
	/* the parts weighted (inverse_masses), a particle midway along each long
	bone, the limbs kept out of the torso and each other */
	boolean weighted;
	/* joints looser and draggier as it goes limp, tightening over
	RAGDOLL_LOOSE_TICKS */
	boolean loosens;
	/* the furthest a knee, an elbow and any other joint (but the spine's,
	neck's and head's) bends from straight, as cosines; how far a knee and
	an elbow twist out of their plane (a part of the bone's length) */
	real knee_bend;
	real elbow_bend;
	real bend;
	real knee_twist;
	real elbow_twist;
	/* how far a hip and a shoulder swing from where they were (the cone's
	cosine), round a middle turned forward by this part of the bone */
	real hip_cone;
	real hip_tilt;
	real shoulder_cone;
	real shoulder_tilt;
	/* how readily each part moves when the bones pull it (as an inverse:
	the torso's least), by _ragdoll_part_... */
	real inverse_masses[NUMBER_OF_RAGDOLL_PARTS];
	/* the most speed (world units a tick) the corrections the joints and
	the world make to a particle's move add to its own (0: no limit, as a
	position-based body has) */
	real correction_limit;
};

struct ragdoll_constraint
{
	short a;
	short b;
	short type;
	real length;
};

struct ragdoll_rotation
{
	real_vector3d columns[3];
};

/* a joint's limit, held in the frame of the rigid part it hangs from (the
hips for a leg, the chest for an arm), which turns as the body does: the
hinge's axis, or the cone's middle */
struct ragdoll_joint
{
	short type;
	short frame;
	short grandparent;
	short parent;
	short child;
	real_vector3d axis;
	/* the limit held now (a hinge's twist, a cone's cosine), and the limit
	itself, which loosens and tightens again as the body goes limp */
	real limit;
	real tight_limit;
	/* a hinge's bend: its constraint (or NONE), the furthest it bends from
	straight (radians), its bones' lengths, and how close the ends were
	allowed when it was made */
	short bend;
	real bend_angle;
	real upper_length;
	real lower_length;
	real bend_floor;
};

struct ragdoll_blow
{
	long unit_index;
	long time;
	real_point3d epicenter;
	real_vector3d direction;
	real speed;
	/* its damage's category (ae_ragdoll_rules.h) */
	short category;
};

struct ragdoll
{
	long object_index;
	long start_time;
	short node_count;
	short particle_count;
	short constraint_count;
	short ticks;
	short still_ticks;
	boolean asleep;
	/* hosting a network game: drawn only, its body left as the stock game
	moves and poses it (and as the host sends it); else the ragdoll moves
	the body itself */
	boolean drawn_only;
	/* game.ragdoll's, as it was when the body died */
	short style;
	/* playing its death animation still, not limp yet: the nodes where they
	were last tick (to start from at their speeds), how many; whether it has
	been in the air since it died, a blast killed it, and how high its nodes
	spanned when it died */
	boolean waiting;
	boolean was_airborne;
	boolean blast;
	short previous_count;
	real_point3d previous_nodes[MAXIMUM_NODES_PER_MODEL];
	real standing_span;
	/* the blow that killed it, if it was noted */
	boolean has_blow;
	struct ragdoll_blow blow;
	/* when it went limp, how much looser its joints are than their limits
	now, and its own random numbers' seed (none of the game's) */
	long limp_time;
	real looseness;
	unsigned long seed;
	/* the nodes, parents first */
	short order[MAXIMUM_NODES_PER_MODEL];
	short parents[MAXIMUM_RAGDOLL_PARTICLES];
	/* the particles a node's rotation follows: its bone's end, and another
	where it branches */
	short primary[MAXIMUM_NODES_PER_MODEL];
	short secondary[MAXIMUM_NODES_PER_MODEL];
	real_point3d positions[MAXIMUM_RAGDOLL_PARTICLES];
	real_vector3d velocities[MAXIMUM_RAGDOLL_PARTICLES];
	/* how readily each moves when the bones pull it (1: as any other) */
	real inverse_masses[MAXIMUM_RAGDOLL_PARTICLES];
	struct ragdoll_constraint constraints[MAXIMUM_RAGDOLL_CONSTRAINTS];
	short joint_count;
	struct ragdoll_joint joints[MAXIMUM_RAGDOLL_JOINTS];
	/* the nodes as drawn, put back over the body's own each tick */
	real_matrix4x3 pose[MAXIMUM_NODES_PER_MODEL];
	/* the trace of a body that flies off (ragdoll_trace): its parts' fastest speed as it went limp, the body's own
	speed then, the killing blow's push, whether it started from its death animation's pose; its fastest part in its
	first two seconds and when, where its first particle started, whether it was told */
	real trace_start;
	real trace_body;
	real trace_blow;
	boolean trace_posed;
	real trace_peak;
	short trace_peak_tick;
	real_point3d trace_origin;
	boolean traced;
};

/* ---------- globals */

/* by style, from spaghetti_western (none has no ragdoll) */
static struct ragdoll_mode const ragdoll_modes[NUMBER_OF_RAGDOLL_STYLES - 1] =
{
	/* spaghetti_western */
	{
		_ragdoll_start_at_death, 0, 0.f,
		1.f, 0.995f, /* gravity, damping */
		0.6f, 0.f, 0.f, /* friction, coulomb, restitution */
		0.f, 0.f, /* joint drag, joint friction */
		1.f, 1.f, 1.f, 0.02f, 0.02f, /* kill blast, blast, big blast, blow speed, hit speed */
		FALSE, FALSE, FALSE, FALSE, /* kick, rigid torso, weighted, loosens */
		-0.866f, -0.866f, -0.866f, /* knee, elbow and other bends: 150 degrees */
		0.2f, 0.45f, /* knee and elbow twist */
		0.087f, 0.6f, /* hip: 85 degrees round a middle tilted forward 35 (forward to 120, back to 50) */
		-0.5f, 0.f, /* shoulder: 120 degrees */
		/* other, torso, clavicle, head, thigh, upper arm, calf, forearm, foot, hand */
		{ 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f },
		0.f, /* correction limit: none */
	},
	/* exaggerated */
	{
		_ragdoll_start_at_death, 0, 0.f,
		1.f, 0.995f,
		0.75f, 0.f, 0.f,
		0.06f, 0.f,
		1.5f, 1.5f, 1.f, 0.02f, 0.02f,
		FALSE, FALSE, TRUE, FALSE,
		-0.866f, -0.866f, -0.866f,
		0.1f, 0.3f,
		0.087f, 0.6f,
		-0.17f, 0.4f, /* shoulder: 100 degrees */
		{ 1.f, 0.35f, 0.8f, 0.6f, 0.8f, 0.8f, 1.f, 1.f, 1.3f, 1.3f },
		0.07f, /* correction limit (the trace runs of 11-10; above the 0.065 a joint takes back of a hand swung at 0.1) */
	},
	/* modern */
	{
		_ragdoll_start_at_death, 0, 0.f,
		1.3f, 0.9995f,
		1.f, 0.8f, 0.25f,
		0.f, 0.0005f,
		1.f, 1.f, 0.7f, 0.f, 0.f,
		TRUE, TRUE, TRUE, TRUE, /* the kick (a level turn: no pirouette), pushes and tumble; limp at death, no stock animation */
		0.174f, 0.342f, RAGDOLL_STIFF_BEND_COSINE, /* knee 80 degrees, elbow 70, the hands and feet riding on their limbs */
		0.03f, 0.05f,
		0.85f, 0.15f, /* hip: 32 degrees */
		0.5f, 0.2f, /* shoulder: 60 degrees */
		/* (heavy legs, light arms) */
		{ 1.f, 0.35f, 0.35f, 0.85f, 0.5f, 1.4f, 1.2f, 2.5f, 1.2f, 2.5f },
		0.07f,
	},
	/* mixed: the stock animation for most of its length (or until it lies
	down after 0.4 seconds), then modern's body with no kick, blasts that
	push it a little less (but the killing blast as modern's) */
	{
		_ragdoll_start_near_end, 4 * TICKS_PER_SECOND / 10, 0.7f,
		1.3f, 0.9995f,
		1.f, 0.8f, 0.25f,
		0.f, 0.0005f,
		1.f, 0.75f, 0.7f, 0.f, 0.f,
		FALSE, TRUE, TRUE, TRUE,
		0.174f, 0.342f, RAGDOLL_STIFF_BEND_COSINE,
		0.03f, 0.05f,
		0.85f, 0.15f,
		0.5f, 0.2f,
		{ 1.f, 0.35f, 0.35f, 0.85f, 0.5f, 1.4f, 1.2f, 2.5f, 1.2f, 2.5f },
		0.07f,
	},
};

static struct ragdoll ragdolls[MAXIMUM_RAGDOLLS];
static struct ragdoll_blow ragdoll_blows[MAXIMUM_RAGDOLL_BLOWS];
static long ragdoll_next_blow;
static struct collision_feature_list ragdoll_features;
static boolean ragdolls_initialized = FALSE;

/* ---------- private code */

/* a setting's style; none leaves the original death animations. The test
build's name for exaggerated (a word starting "halo") is still read as it */
static short ragdoll_named(
	char const *name)
{
	/* (owner 11-07 named them; the test build's names "woody" and "faithful" still read as theirs) */
	return !strcmp(name, "spaghetti_western") || !strcmp(name, "woody") ? _ragdoll_style_spaghetti_western :
		!strcmp(name, "exaggerated") || !strncmp(name, "halo", 4) ? _ragdoll_style_exaggerated :
		!strcmp(name, "modern") || !strcmp(name, "faithful") ? _ragdoll_style_modern :
		!strcmp(name, "mixed") ? _ragdoll_style_mixed : _ragdoll_style_none;
}

/* a style's row (none's: spaghetti_western's, though it has no ragdoll) */
static struct ragdoll_mode const *ragdoll_mode_get(
	short style)
{
	return &ragdoll_modes[style > _ragdoll_style_none && style < NUMBER_OF_RAGDOLL_STYLES ?
		style - 1 : _ragdoll_style_spaghetti_western - 1];
}

/* whether a biped is a player's: its unit's player_index while it lives, and
after its death (the game clears that at the kill) the players' unit and dead
unit indices (ae_ragdoll_rules.h) */
static boolean ragdoll_is_player_body(
	long biped_index,
	struct biped_datum const *biped)
{
	long units[32];
	long dead_units[32];
	long count = 0;
	struct data_iterator iterator;
	struct player_datum *player;

	if (biped->unit.player_index != NONE)
		return TRUE;
	/* (a living AI's body is not looked up) */
	if (!TEST_FLAG(biped->object.damage_flags, _object_dead_bit))
		return FALSE;
	data_iterator_new(&iterator, player_data);
	while ((player = (struct player_datum *)data_iterator_next(&iterator)) != NULL && count < 32)
	{
		units[count] = player->unit_index;
		dead_units[count] = player->dead_unit_index;
		count++;
	}
	return ae_ragdoll_is_player_body(biped->unit.player_index, biped_index, units, dead_units, count) != 0;
}

/* the body's style: a player's (game.ragdoll_players) or the AI's
(game.ragdoll_ai). none for anything that is not a biped */
static short ragdoll_style(
	long biped_index)
{
	static unsigned long changes = (unsigned long)-1;
	static short players = _ragdoll_style_none;
	static short ai = _ragdoll_style_none;
	struct biped_datum *biped;

	if (changes != config_changes())
	{
		changes = config_changes();
		players = ragdoll_named(config_string("game.ragdoll_players"));
		ai = ragdoll_named(config_string("game.ragdoll_ai"));
	}
	biped = biped_try_and_get(biped_index);
	if (!biped)
		return _ragdoll_style_none;
	return ragdoll_is_player_body(biped_index, biped) ? players : ai;
}

static void ragdolls_initialize(
	void)
{
	long index;

	if (ragdolls_initialized)
		return;
	ragdolls_initialized = TRUE;
	for (index = 0; index < MAXIMUM_RAGDOLLS; index++)
		ragdolls[index].object_index = NONE;
	for (index = 0; index < MAXIMUM_RAGDOLL_BLOWS; index++)
		ragdoll_blows[index].unit_index = NONE;
	ragdoll_next_blow = 0;
}

static struct ragdoll *ragdoll_find(
	long object_index)
{
	long index;

	ragdolls_initialize();
	if (object_index == NONE)
		return NULL;
	for (index = 0; index < MAXIMUM_RAGDOLLS; index++)
	{
		if (ragdolls[index].object_index == object_index)
			return &ragdolls[index];
	}
	return NULL;
}

/* the dead biped a ragdoll moves, or NULL once it is gone (deleted, another
object in its place, or alive again) */
static struct biped_datum *ragdoll_body(
	struct ragdoll const *ragdoll)
{
	struct biped_datum *biped = (struct biped_datum *)object_try_and_get_and_verify_type(
		ragdoll->object_index, _object_mask_biped);

	return biped && TEST_FLAG(biped->object.damage_flags, _object_dead_bit) ? biped : NULL;
}

static boolean name_contains(
	char const *name,
	char const *part)
{
	long start;

	for (start = 0; name[start]; start++)
	{
		long offset;

		for (offset = 0; part[offset]; offset++)
		{
			char c = name[start + offset];

			if (c >= 'A' && c <= 'Z')
				c = (char)(c - 'A' + 'a');
			if (c != part[offset])
				break;
		}
		if (!part[offset])
			return TRUE;
	}
	return FALSE;
}

static real ragdoll_clamp(
	real value,
	real minimum,
	real maximum)
{
	return value < minimum ? minimum : value > maximum ? maximum : value;
}

static void ragdoll_clamp_speed(
	real_vector3d *velocity)
{
	real speed = magnitude3d(velocity);

	if (speed > RAGDOLL_MAXIMUM_SPEED)
		scale_vector3d(velocity, RAGDOLL_MAXIMUM_SPEED / speed, velocity);
}

/* a point moved as far towards where it is going as the world lets it;
TRUE when it hit something, the last thing's normal in *normal */
static boolean ragdoll_move(
	real_point3d *position,
	real_point3d const *destination,
	real_vector3d *normal)
{
	struct collision_plane collisions[4];
	real_vector3d motion;
	real_vector3d clipped_motion;
	real_point3d moved;
	short count;

	vector_from_points3d(position, destination, &motion);
	if (fabs(motion.i) < _real_epsilon && fabs(motion.j) < _real_epsilon && fabs(motion.k) < _real_epsilon)
		return FALSE;
	count = collision_move_point(position, &motion, &ragdoll_features, &moved, &clipped_motion,
		(short)NUMBEROF(collisions) - 1, collisions);
	*position = moved;
	if (count > 0 && normal)
		*normal = collisions[count - 1].plane.n;
	return count > 0;
}

/* the particles' middle, and how far round it they reach (and will have
moved, with their speeds: a tick's worth of the world to collide with) */
static void ragdoll_bounds(
	struct ragdoll const *ragdoll,
	boolean with_speeds,
	real_point3d *center,
	real *radius)
{
	real reach = 0.f;
	short index;

	center->x = center->y = center->z = 0.f;
	for (index = 0; index < ragdoll->particle_count; index++)
	{
		center->x += ragdoll->positions[index].x;
		center->y += ragdoll->positions[index].y;
		center->z += ragdoll->positions[index].z;
	}
	center->x /= ragdoll->particle_count;
	center->y /= ragdoll->particle_count;
	center->z /= ragdoll->particle_count;
	for (index = 0; index < ragdoll->particle_count; index++)
	{
		real distance = distance3d(center, &ragdoll->positions[index]);

		if (with_speeds)
			distance += magnitude3d(&ragdoll->velocities[index]);
		if (distance > reach)
			reach = distance;
	}
	*radius = reach;
}

static void ragdoll_gather_features(
	struct ragdoll const *ragdoll,
	long biped_index)
{
	real_point3d center;
	real radius;

	ragdoll_bounds(ragdoll, TRUE, &center, &radius);
	collision_get_features_in_sphere(
		_collision_test_for_bipeds_dead_flags | FLAG(_collision_test_ignore_invisible_surfaces_bit),
		&center,
		radius + 2.f * RAGDOLL_RADIUS,
		0.f,
		RAGDOLL_RADIUS,
		biped_index,
		&ragdoll_features);
}

/* ---------- rotations */

static void rotation_identity(
	struct ragdoll_rotation *rotation)
{
	set_real_vector3d(&rotation->columns[0], 1.f, 0.f, 0.f);
	set_real_vector3d(&rotation->columns[1], 0.f, 1.f, 0.f);
	set_real_vector3d(&rotation->columns[2], 0.f, 0.f, 1.f);
}

static void rotation_transform(
	struct ragdoll_rotation const *rotation,
	real_vector3d const *vector,
	real_vector3d *result)
{
	real_vector3d out;

	out.i = rotation->columns[0].i * vector->i + rotation->columns[1].i * vector->j + rotation->columns[2].i * vector->k;
	out.j = rotation->columns[0].j * vector->i + rotation->columns[1].j * vector->j + rotation->columns[2].j * vector->k;
	out.k = rotation->columns[0].k * vector->i + rotation->columns[1].k * vector->j + rotation->columns[2].k * vector->k;
	*result = out;
}

/* the shortest turn from one direction to another (both normal) */
static void rotation_between(
	real_vector3d const *from,
	real_vector3d const *to,
	struct ragdoll_rotation *rotation)
{
	real_vector3d axis;
	real sine;
	real cosine = dot_product3d(from, to);
	real versine;
	long column;

	cross_product3d(from, to, &axis);
	sine = magnitude3d(&axis);
	if (sine < 1e-6f || cosine < -0.999f)
	{
		rotation_identity(rotation);
		return;
	}
	scale_vector3d(&axis, 1.f / sine, &axis);
	versine = 1.f - cosine;
	/* (Rodrigues: each column the turn of an axis) */
	for (column = 0; column < 3; column++)
	{
		real_vector3d unit;
		real_vector3d across;
		real along;

		set_real_vector3d(&unit, column == 0 ? 1.f : 0.f, column == 1 ? 1.f : 0.f, column == 2 ? 1.f : 0.f);
		cross_product3d(&axis, &unit, &across);
		along = dot_product3d(&axis, &unit) * versine;
		rotation->columns[column].i = unit.i * cosine + across.i * sine + axis.i * along;
		rotation->columns[column].j = unit.j * cosine + across.j * sine + axis.j * along;
		rotation->columns[column].k = unit.k * cosine + across.k * sine + axis.k * along;
	}
}

/* an orthonormal basis from a direction and another across it; FALSE when
they are (nearly) the same direction */
static boolean basis_from_directions(
	real_vector3d const *primary,
	real_vector3d const *secondary,
	struct ragdoll_rotation *basis)
{
	real_vector3d across;
	real along;

	basis->columns[0] = *primary;
	if (normalize3d(&basis->columns[0]) < 1e-6f)
		return FALSE;
	across = *secondary;
	along = dot_product3d(&across, &basis->columns[0]);
	basis->columns[1].i = across.i - basis->columns[0].i * along;
	basis->columns[1].j = across.j - basis->columns[0].j * along;
	basis->columns[1].k = across.k - basis->columns[0].k * along;
	if (normalize3d(&basis->columns[1]) < 1e-4f)
		return FALSE;
	cross_product3d(&basis->columns[0], &basis->columns[1], &basis->columns[2]);
	return TRUE;
}

/* the turn that takes one basis to another */
static void rotation_between_bases(
	struct ragdoll_rotation const *from,
	struct ragdoll_rotation const *to,
	struct ragdoll_rotation *rotation)
{
	long column;

	/* (to times from's transpose: each world axis put in from's terms, then
	taken out of to's) */
	for (column = 0; column < 3; column++)
	{
		real_vector3d unit;
		real_vector3d local;

		set_real_vector3d(&unit, column == 0 ? 1.f : 0.f, column == 1 ? 1.f : 0.f, column == 2 ? 1.f : 0.f);
		local.i = dot_product3d(&from->columns[0], &unit);
		local.j = dot_product3d(&from->columns[1], &unit);
		local.k = dot_product3d(&from->columns[2], &unit);
		rotation_transform(to, &local, &rotation->columns[column]);
	}
}

/* the frame of a rigid part: its node to its two first ends (the hips, the
chest); FALSE if they lie in a line */
static boolean ragdoll_frame(
	struct ragdoll const *ragdoll,
	short node,
	struct ragdoll_rotation *frame)
{
	real_vector3d primary;
	real_vector3d secondary;

	if (node == NONE || ragdoll->primary[node] == NONE || ragdoll->secondary[node] == NONE)
		return FALSE;
	vector_from_points3d(&ragdoll->positions[node], &ragdoll->positions[ragdoll->primary[node]], &primary);
	vector_from_points3d(&ragdoll->positions[node], &ragdoll->positions[ragdoll->secondary[node]], &secondary);
	return basis_from_directions(&primary, &secondary, frame);
}

/* a world direction in a frame's terms */
static void rotation_untransform(
	struct ragdoll_rotation const *rotation,
	real_vector3d const *vector,
	real_vector3d *result)
{
	real_vector3d out;

	out.i = dot_product3d(&rotation->columns[0], vector);
	out.j = dot_product3d(&rotation->columns[1], vector);
	out.k = dot_product3d(&rotation->columns[2], vector);
	*result = out;
}

/* the rigid part a node hangs from: its nearest ancestor that branches */
static short ragdoll_frame_node(
	struct ragdoll const *ragdoll,
	short node)
{
	short count;

	for (count = 0; node != NONE && count < MAXIMUM_RAGDOLL_PARTICLES; count++)
	{
		if (node < ragdoll->node_count && ragdoll->secondary[node] != NONE)
			return node;
		node = ragdoll->parents[node];
	}
	return NONE;
}

/* a hinge at parent, between the bones from grandparent and to child,
which bends towards bend_direction (a world direction across the upper
bone), as the frame it hangs from has it now (NULL if there is none) */
static struct ragdoll_joint *ragdoll_add_hinge(
	struct ragdoll *ragdoll,
	short grandparent,
	short parent,
	short child,
	real_vector3d const *bend_direction,
	real twist)
{
	short frame_node = ragdoll_frame_node(ragdoll, grandparent);
	struct ragdoll_rotation frame;
	struct ragdoll_joint *joint;
	real_vector3d upper;
	real_vector3d axis;

	if (ragdoll->joint_count >= MAXIMUM_RAGDOLL_JOINTS || !ragdoll_frame(ragdoll, frame_node, &frame))
		return NULL;
	vector_from_points3d(&ragdoll->positions[grandparent], &ragdoll->positions[parent], &upper);
	cross_product3d(&upper, bend_direction, &axis);
	if (normalize3d(&axis) < 1e-4f)
		return NULL;
	joint = &ragdoll->joints[ragdoll->joint_count++];
	joint->type = _ragdoll_joint_hinge;
	joint->frame = frame_node;
	joint->grandparent = grandparent;
	joint->parent = parent;
	joint->child = child;
	rotation_untransform(&frame, &axis, &joint->axis);
	joint->limit = joint->tight_limit = twist;
	joint->bend = NONE;
	return joint;
}

/* a cone the bone from parent to child swings within, round where it
points now, turned towards forward by tilt (a fraction of its own length) */
static void ragdoll_add_cone(
	struct ragdoll *ragdoll,
	short parent,
	short child,
	real_vector3d const *forward,
	real tilt,
	real cosine)
{
	short frame_node = ragdoll_frame_node(ragdoll, ragdoll->parents[parent]);
	struct ragdoll_rotation frame;
	struct ragdoll_joint *joint;
	real_vector3d middle;

	if (ragdoll->joint_count >= MAXIMUM_RAGDOLL_JOINTS || !ragdoll_frame(ragdoll, frame_node, &frame))
		return;
	vector_from_points3d(&ragdoll->positions[parent], &ragdoll->positions[child], &middle);
	if (normalize3d(&middle) < 1e-4f)
		return;
	middle.i += forward->i * tilt;
	middle.j += forward->j * tilt;
	middle.k += forward->k * tilt;
	if (normalize3d(&middle) < 1e-4f)
		return;
	joint = &ragdoll->joints[ragdoll->joint_count++];
	joint->type = _ragdoll_joint_cone;
	joint->frame = frame_node;
	joint->grandparent = NONE;
	joint->parent = parent;
	joint->child = child;
	rotation_untransform(&frame, &middle, &joint->axis);
	joint->limit = joint->tight_limit = cosine;
	joint->bend = NONE;
}

/* a joint put back within its limit, most of the way: the child moved, the
bone's length kept */
static void ragdoll_hold_joint(
	struct ragdoll *ragdoll,
	struct ragdoll_joint const *joint)
{
	struct ragdoll_rotation frame;
	real_point3d *parent = &ragdoll->positions[joint->parent];
	real_point3d *child = &ragdoll->positions[joint->child];
	real_vector3d axis;
	real_vector3d bone;
	real_vector3d held;
	real length;

	if (!ragdoll_frame(ragdoll, joint->frame, &frame))
		return;
	rotation_transform(&frame, &joint->axis, &axis);
	vector_from_points3d(parent, child, &bone);
	length = magnitude3d(&bone);
	if (length < 1e-4f)
		return;

	if (joint->type == _ragdoll_joint_hinge)
	{
		real_vector3d upper;
		real_vector3d bend;
		real along;
		real across;
		real twist;
		real limit = joint->limit * length;

		vector_from_points3d(&ragdoll->positions[joint->grandparent], parent, &upper);
		if (normalize3d(&upper) < 1e-4f)
			return;
		/* (the axis square to the upper bone, and the way it bends square
		to both) */
		along = dot_product3d(&axis, &upper);
		axis.i -= upper.i * along;
		axis.j -= upper.j * along;
		axis.k -= upper.k * along;
		if (normalize3d(&axis) < 1e-4f)
			return;
		cross_product3d(&axis, &upper, &bend);
		along = dot_product3d(&bone, &upper);
		across = dot_product3d(&bone, &bend);
		twist = dot_product3d(&bone, &axis);
		if (across >= 0.f && twist >= -limit && twist <= limit)
			return;
		/* (bent the wrong way: straight; twisted: back to the limit) */
		if (across < 0.f)
			across = 0.f;
		twist = ragdoll_clamp(twist, -limit, limit);
		held.i = upper.i * along + bend.i * across + axis.i * twist;
		held.j = upper.j * along + bend.j * across + axis.j * twist;
		held.k = upper.k * along + bend.k * across + axis.k * twist;
	}
	else
	{
		real cosine;
		real sine;
		real_vector3d across;

		if (normalize3d(&axis) < 1e-4f)
			return;
		scale_vector3d(&bone, 1.f / length, &bone);
		cosine = dot_product3d(&bone, &axis);
		if (cosine >= joint->limit)
			return;
		/* (turned towards the cone's middle, onto its edge) */
		across.i = bone.i - axis.i * cosine;
		across.j = bone.j - axis.j * cosine;
		across.k = bone.k - axis.k * cosine;
		if (normalize3d(&across) < 1e-4f)
			perpendicular3d(&axis, &across);
		sine = square_root(MAX(0.f, 1.f - joint->limit * joint->limit));
		held.i = axis.i * joint->limit + across.i * sine;
		held.j = axis.j * joint->limit + across.j * sine;
		held.k = axis.k * joint->limit + across.k * sine;
	}
	if (normalize3d(&held) < 1e-4f)
		return;
	child->x += (parent->x + held.i * length - child->x) * RAGDOLL_JOINT_STIFFNESS;
	child->y += (parent->y + held.j * length - child->y) * RAGDOLL_JOINT_STIFFNESS;
	child->z += (parent->z + held.k * length - child->z) * RAGDOLL_JOINT_STIFFNESS;
}

/* ---------- building */

/* the constraint's index, or NONE when there is no room */
static short ragdoll_add_constraint(
	struct ragdoll *ragdoll,
	short a,
	short b,
	short type,
	real length)
{
	struct ragdoll_constraint *constraint;

	if (ragdoll->constraint_count >= MAXIMUM_RAGDOLL_CONSTRAINTS || a == b)
		return NONE;
	constraint = &ragdoll->constraints[ragdoll->constraint_count];
	constraint->a = a;
	constraint->b = b;
	constraint->type = type;
	constraint->length = length;
	return ragdoll->constraint_count++;
}

/* how far apart the ends of two bones are, bent from straight by an angle
whose cosine this is */
static real ragdoll_bent_distance(
	real a,
	real b,
	real bend_cosine)
{
	real squared = a * a + b * b + 2.f * a * b * bend_cosine;

	return squared > 0.f ? square_root(squared) : 0.f;
}

/* a joint that bends no further than its limit from straight, nor less
than it is bent now: the grandparent and the child no closer (the
constraint's index, or NONE) */
static short ragdoll_add_bend(
	struct ragdoll *ragdoll,
	short grandparent,
	short parent,
	short child,
	real bend_cosine)
{
	real a = distance3d(&ragdoll->positions[grandparent], &ragdoll->positions[parent]);
	real b = distance3d(&ragdoll->positions[parent], &ragdoll->positions[child]);
	real current = distance3d(&ragdoll->positions[grandparent], &ragdoll->positions[child]);
	real limit = ragdoll_bent_distance(a, b, bend_cosine);

	return ragdoll_add_constraint(ragdoll, grandparent, child, _ragdoll_constraint_minimum,
		current < limit ? current : limit);
}

/* how much looser a body's joints are than their limits (and draggier), so
many ticks after it went limp: two or three times at first, a little
different body to body, tightening to them over RAGDOLL_LOOSE_TICKS */
static real ragdoll_looseness(
	long object_index,
	long ticks)
{
	static real const extra[4] = { 2.f, 1.f, 1.333f, 1.666f };

	if (ticks < 0 || ticks >= RAGDOLL_LOOSE_TICKS)
		return 1.f;
	return 1.f + (1.f - (real)ticks / RAGDOLL_LOOSE_TICKS) * extra[object_index & 3];
}

/* the joints' limits loosened this much (1: as tight as they are) */
static void ragdoll_loosen(
	struct ragdoll *ragdoll,
	real looseness)
{
	short index;

	ragdoll->looseness = looseness;
	for (index = 0; index < ragdoll->joint_count; index++)
	{
		struct ragdoll_joint *joint = &ragdoll->joints[index];

		if (joint->type == _ragdoll_joint_hinge)
		{
			joint->limit = joint->tight_limit * looseness;
		}
		else
		{
			real angle = arccosine(ragdoll_clamp(joint->tight_limit, -1.f, 1.f)) * looseness;

			joint->limit = cosine(MIN(angle, _pi));
		}
		if (joint->bend != NONE)
		{
			real limit = ragdoll_bent_distance(joint->upper_length, joint->lower_length,
				cosine(MIN(joint->bend_angle * looseness, _pi)));

			ragdoll->constraints[joint->bend].length = MIN(limit, joint->bend_floor);
		}
	}
}

static boolean ragdoll_stiff_node(
	struct model_node const *node)
{
	return name_contains(node->name, "spine") || name_contains(node->name, "neck") ||
		name_contains(node->name, "pelvis") || name_contains(node->name, "head");
}

/* the model's nodes, linked as they say (a map's counts and indices); the
nodes parents first in order[] */
static boolean ragdoll_order_nodes(
	struct model *model,
	short node_count,
	short *order)
{
	short count = 1;
	short head = 0;

	order[0] = 0;
	while (head < count)
	{
		struct model_node *node = TAG_BLOCK_GET_ELEMENT(&model->nodes, order[head], struct model_node);
		short child = node->first_child_node_index;

		head++;
		while (child != NONE)
		{
			struct model_node *child_node;

			if (!VALID_INDEX(child, node_count) || count >= node_count)
				return FALSE;
			child_node = TAG_BLOCK_GET_ELEMENT(&model->nodes, child, struct model_node);
			if (child_node->parent_node_index != order[head - 1])
				return FALSE;
			order[count++] = child;
			child = child_node->next_sibling_node_index;
		}
	}
	return count == node_count;
}

static short ragdoll_node_part(
	struct model_node const *node)
{
	if (name_contains(node->name, "pelvis") || name_contains(node->name, "spine") ||
		name_contains(node->name, "neck"))
	{
		return _ragdoll_part_torso;
	}
	if (name_contains(node->name, "head"))
		return _ragdoll_part_head;
	if (name_contains(node->name, "clavicle"))
		return _ragdoll_part_clavicle;
	if (name_contains(node->name, "thigh"))
		return _ragdoll_part_thigh;
	if (name_contains(node->name, "upperarm"))
		return _ragdoll_part_upper_arm;
	if (name_contains(node->name, "calf"))
		return _ragdoll_part_calf;
	if (name_contains(node->name, "forearm"))
		return _ragdoll_part_forearm;
	if (name_contains(node->name, "foot") || name_contains(node->name, "toe"))
		return _ragdoll_part_foot;
	if (name_contains(node->name, "hand") || name_contains(node->name, "finger"))
		return _ragdoll_part_hand;
	return _ragdoll_part_other;
}

/* a limb's upper part (a midway particle along it), its lower (kept out of
the torso and the other limbs), or neither */
static boolean ragdoll_upper_part(
	short part)
{
	return part == _ragdoll_part_clavicle || part == _ragdoll_part_thigh || part == _ragdoll_part_upper_arm;
}

static boolean ragdoll_lower_part(
	short part)
{
	return part == _ragdoll_part_calf || part == _ragdoll_part_forearm || part == _ragdoll_part_foot ||
		part == _ragdoll_part_hand;
}

/* how many particles apart two are along the skeleton, up to limit */
static short ragdoll_separation(
	struct ragdoll const *ragdoll,
	short a,
	short b,
	short limit)
{
	short up_a;
	short steps_a;

	for (up_a = a, steps_a = 0; up_a != NONE && steps_a <= limit; up_a = ragdoll->parents[up_a], steps_a++)
	{
		short up_b;
		short steps_b;

		for (up_b = b, steps_b = 0; up_b != NONE && steps_a + steps_b <= limit;
			up_b = ragdoll->parents[up_b], steps_b++)
		{
			if (up_a == up_b)
				return (short)(steps_a + steps_b);
		}
	}
	return (short)(limit + 1);
}

/* a weighted body: the parts' weights; a particle midway along each long
bone (a limb's and the spine's), which gives them thickness against the
world; the limbs kept out of the torso and of each other; and, for a rigid
torso, the pelvis, spine, neck and clavicles held as one piece, with the
hips' and shoulders' joints in it */
static void ragdoll_build_weighted(
	struct ragdoll *ragdoll,
	struct model *model,
	struct ragdoll_mode const *mode)
{
	short parts[MAXIMUM_RAGDOLL_PARTICLES];
	short node_count = ragdoll->node_count;
	short particle_count = ragdoll->particle_count;
	short middles = 0;
	short index;

	for (index = 0; index < particle_count; index++)
	{
		short part = index < node_count ?
			ragdoll_node_part(TAG_BLOCK_GET_ELEMENT(&model->nodes, index, struct model_node)) :
			_ragdoll_part_head; /* (the head's top) */

		parts[index] = part;
		ragdoll->inverse_masses[index] = mode->inverse_masses[part];
	}

	if (mode->rigid_torso)
	{
		short rigid[MAXIMUM_NODES_PER_MODEL];
		short rigid_count = 0;
		short first;

		for (index = 0; index < node_count; index++)
		{
			short parent = ragdoll->parents[index];
			boolean torso = parts[index] == _ragdoll_part_torso || parts[index] == _ragdoll_part_clavicle;
			/* (a hip or a shoulder: where a thigh or an upper arm hangs from it) */
			boolean joint = parent != NONE && ragdoll_upper_part(parts[index]) &&
				parts[index] != _ragdoll_part_clavicle &&
				(parts[parent] == _ragdoll_part_torso || parts[parent] == _ragdoll_part_clavicle);

			if (torso || joint)
				rigid[rigid_count++] = index;
		}
		for (first = 0; first < rigid_count; first++)
		{
			short second;

			for (second = first + 1; second < rigid_count; second++)
			{
				ragdoll_add_constraint(ragdoll, rigid[first], rigid[second], _ragdoll_constraint_length,
					distance3d(&ragdoll->positions[rigid[first]], &ragdoll->positions[rigid[second]]));
			}
		}
	}

	/* (midway along a bone from a limb's or the spine's node to its one
	child, held there by the bone's two halves) */
	for (index = 1; index < node_count && middles < MAXIMUM_RAGDOLL_MIDDLES; index++)
	{
		short parent = ragdoll->parents[index];
		short middle = ragdoll->particle_count;
		real length;

		if (parent == NONE || ragdoll->primary[parent] != index || ragdoll->secondary[parent] != NONE ||
			(!ragdoll_upper_part(parts[parent]) && parts[parent] != _ragdoll_part_calf &&
				parts[parent] != _ragdoll_part_forearm && parts[parent] != _ragdoll_part_torso))
		{
			continue;
		}
		length = distance3d(&ragdoll->positions[parent], &ragdoll->positions[index]);
		if (length < 4.f * RAGDOLL_RADIUS)
			continue;
		ragdoll->positions[middle].x = 0.5f * (ragdoll->positions[parent].x + ragdoll->positions[index].x);
		ragdoll->positions[middle].y = 0.5f * (ragdoll->positions[parent].y + ragdoll->positions[index].y);
		ragdoll->positions[middle].z = 0.5f * (ragdoll->positions[parent].z + ragdoll->positions[index].z);
		ragdoll->parents[middle] = parent;
		ragdoll->velocities[middle] = *global_zero_vector3d;
		ragdoll->inverse_masses[middle] = ragdoll->inverse_masses[parent];
		parts[middle] = parts[parent];
		ragdoll_add_constraint(ragdoll, parent, middle, _ragdoll_constraint_length, 0.5f * length);
		ragdoll_add_constraint(ragdoll, middle, index, _ragdoll_constraint_length, 0.5f * length);
		ragdoll->particle_count++;
		middles++;
	}

	/* (no closer than they are now, if that is already closer) */
	for (index = 0; index < ragdoll->particle_count; index++)
	{
		short other;

		if (!ragdoll_lower_part(parts[index]))
			continue;
		for (other = 0; other < ragdoll->particle_count; other++)
		{
			real clearance;
			real now;

			if (other == index)
				continue;
			if (parts[other] == _ragdoll_part_torso || parts[other] == _ragdoll_part_head)
				clearance = RAGDOLL_TORSO_CLEARANCE;
			else if (ragdoll_lower_part(parts[other]) && other > index)
				clearance = RAGDOLL_LIMB_CLEARANCE;
			else
				continue;
			if (ragdoll_separation(ragdoll, index, other, 3) <= 3)
				continue;
			now = distance3d(&ragdoll->positions[index], &ragdoll->positions[other]);
			ragdoll_add_constraint(ragdoll, index, other, _ragdoll_constraint_minimum,
				now < clearance ? now : clearance);
		}
	}
}

/* a hinge's bend constraint (if both were made), which loosens with it */
static void ragdoll_link_bend(
	struct ragdoll *ragdoll,
	struct ragdoll_joint *joint,
	short constraint,
	real bend_cosine)
{
	struct ragdoll_constraint const *bend;

	if (!joint || constraint == NONE)
		return;
	bend = &ragdoll->constraints[constraint];
	joint->bend = constraint;
	joint->bend_angle = arccosine(ragdoll_clamp(bend_cosine, -1.f, 1.f));
	joint->upper_length = distance3d(&ragdoll->positions[joint->grandparent], &ragdoll->positions[joint->parent]);
	joint->lower_length = distance3d(&ragdoll->positions[joint->parent], &ragdoll->positions[joint->child]);
	joint->bend_floor = distance3d(&ragdoll->positions[bend->a], &ragdoll->positions[bend->b]);
}

static boolean ragdoll_build(
	struct ragdoll *ragdoll,
	long biped_index)
{
	struct biped_datum *biped = biped_get(biped_index);
	struct biped_definition *definition = biped_definition_get(biped->definition_index);
	struct model *model;
	real_matrix4x3 *node_matrices;
	short node_count;
	short tips = 0;
	short index;
	short children[MAXIMUM_RAGDOLL_PARTICLES][3];
	short child_counts[MAXIMUM_RAGDOLL_PARTICLES];
	/* the bend constraint of a joint at each particle */
	short bends[MAXIMUM_RAGDOLL_PARTICLES];
	struct ragdoll_mode const *mode = ragdoll_mode_get(ragdoll->style);

	if (definition->object.model.index == NONE)
		return FALSE;
	model = model_definition_get(definition->object.model.index);
	node_count = (short)model->nodes.count;
	if (node_count < 2 || node_count > MAXIMUM_NODES_PER_MODEL ||
		node_count > biped->object.node_matrices.size / (long)sizeof(real_matrix4x3))
	{
		return FALSE;
	}
	if (!ragdoll_order_nodes(model, node_count, ragdoll->order))
		return FALSE;

	ragdoll->looseness = 1.f;
	node_matrices = object_get_node_matrices(biped_index);
	csmemcpy(ragdoll->pose, node_matrices, node_count * sizeof(real_matrix4x3));
	ragdoll->node_count = node_count;
	ragdoll->particle_count = node_count;
	ragdoll->constraint_count = 0;
	csmemset(child_counts, 0, sizeof(child_counts));
	for (index = 0; index < MAXIMUM_RAGDOLL_PARTICLES; index++)
		bends[index] = NONE;
	for (index = 0; index < node_count; index++)
	{
		struct model_node *node = TAG_BLOCK_GET_ELEMENT(&model->nodes, index, struct model_node);

		ragdoll->positions[index] = node_matrices[index].position;
		ragdoll->parents[index] = index ? node->parent_node_index : NONE;
		if (index && child_counts[node->parent_node_index] < 3)
			children[node->parent_node_index][child_counts[node->parent_node_index]++] = index;
	}

	/* the head's top, beyond the neck as the neck goes */
	for (index = 1; index < node_count && tips < MAXIMUM_RAGDOLL_TIPS; index++)
	{
		struct model_node *node = TAG_BLOCK_GET_ELEMENT(&model->nodes, index, struct model_node);
		short tip = ragdoll->particle_count;
		real_vector3d direction;
		real length;

		if (node->first_child_node_index != NONE || !name_contains(node->name, "head"))
			continue;
		vector_from_points3d(&ragdoll->positions[node->parent_node_index], &ragdoll->positions[index], &direction);
		length = normalize3d(&direction);
		if (length < 1e-4f)
			direction = node_matrices[index].forward;
		length = ragdoll_clamp(length * 1.5f, RAGDOLL_TIP_MINIMUM_LENGTH, RAGDOLL_TIP_MAXIMUM_LENGTH);
		point_from_line3d(&ragdoll->positions[index], &direction, length, &ragdoll->positions[tip]);
		ragdoll->parents[tip] = index;
		children[index][child_counts[index]++] = tip;
		ragdoll->particle_count++;
		tips++;
	}

	for (index = 0; index < ragdoll->particle_count; index++)
	{
		short parent = ragdoll->parents[index];
		short grandparent = parent != NONE ? ragdoll->parents[parent] : NONE;

		/* the bones */
		if (parent != NONE)
		{
			ragdoll_add_constraint(ragdoll, parent, index, _ragdoll_constraint_length,
				distance3d(&ragdoll->positions[parent], &ragdoll->positions[index]));
		}
		/* rigid where a bone branches (hips, chest, hands): its ends, and
		them with where it comes from */
		if (child_counts[index] >= 2)
		{
			short first;

			for (first = 0; first < child_counts[index]; first++)
			{
				short second;

				for (second = first + 1; second < child_counts[index]; second++)
				{
					ragdoll_add_constraint(ragdoll, children[index][first], children[index][second],
						_ragdoll_constraint_length,
						distance3d(&ragdoll->positions[children[index][first]],
							&ragdoll->positions[children[index][second]]));
				}
				if (parent != NONE)
				{
					ragdoll_add_constraint(ragdoll, parent, children[index][first], _ragdoll_constraint_length,
						distance3d(&ragdoll->positions[parent], &ragdoll->positions[children[index][first]]));
				}
			}
		}
		/* how far a joint bends, where it doesn't branch */
		if (grandparent != NONE && child_counts[parent] == 1)
		{
			struct model_node *parent_node = parent < node_count ?
				TAG_BLOCK_GET_ELEMENT(&model->nodes, parent, struct model_node) : NULL;
			real bend = mode->bend;

			if (index >= node_count || (parent_node && ragdoll_stiff_node(parent_node)))
				bend = RAGDOLL_STIFF_BEND_COSINE;
			else if (parent_node && name_contains(parent_node->name, "calf"))
				bend = mode->knee_bend;
			else if (parent_node && name_contains(parent_node->name, "forearm"))
				bend = mode->elbow_bend;
			bends[parent] = ragdoll_add_bend(ragdoll, grandparent, parent, index, bend);
		}
	}

	for (index = 0; index < node_count; index++)
	{
		short other;
		real squarest = 2.f;

		ragdoll->primary[index] = child_counts[index] > 0 ? children[index][0] : NONE;
		ragdoll->secondary[index] = NONE;
		/* (of the others, the end most nearly square to it: not the hips'
		two sides, nearly in a line through the pelvis) */
		for (other = 1; other < child_counts[index]; other++)
		{
			real_vector3d first;
			real_vector3d second;
			real cosine;

			vector_from_points3d(&ragdoll->positions[index], &ragdoll->positions[children[index][0]], &first);
			vector_from_points3d(&ragdoll->positions[index], &ragdoll->positions[children[index][other]], &second);
			if (normalize3d(&first) < 1e-4f || normalize3d(&second) < 1e-4f)
				continue;
			cosine = (real)fabs(dot_product3d(&first, &second));
			if (cosine < squarest)
			{
				squarest = cosine;
				ragdoll->secondary[index] = children[index][other];
			}
		}
	}

	/* the knees bend back and the elbows forward (as the body faces, or as
	an elbow is bent already), and the hips and shoulders swing only so far */
	ragdoll->joint_count = 0;
	for (index = 1; index < node_count; index++)
	{
		struct model_node *node = TAG_BLOCK_GET_ELEMENT(&model->nodes, index, struct model_node);
		short parent = node->parent_node_index;
		short grandparent = parent ? ragdoll->parents[parent] : NONE;
		struct model_node *parent_node = TAG_BLOCK_GET_ELEMENT(&model->nodes, parent, struct model_node);
		real_vector3d bend;

		if (name_contains(parent_node->name, "thigh"))
			ragdoll_add_cone(ragdoll, parent, index, &biped->object.forward, mode->hip_tilt, mode->hip_cone);
		else if (name_contains(parent_node->name, "upperarm"))
			ragdoll_add_cone(ragdoll, parent, index, &biped->object.forward, mode->shoulder_tilt, mode->shoulder_cone);
		if (grandparent == NONE || child_counts[parent] != 1)
			continue;
		if (name_contains(parent_node->name, "calf"))
		{
			scale_vector3d(&biped->object.forward, -1.f, &bend);
			ragdoll_link_bend(ragdoll, ragdoll_add_hinge(ragdoll, grandparent, parent, index, &bend, mode->knee_twist),
				bends[parent], mode->knee_bend);
		}
		else if (name_contains(parent_node->name, "forearm"))
		{
			real_vector3d upper;
			real_vector3d lower;

			vector_from_points3d(&ragdoll->positions[grandparent], &ragdoll->positions[parent], &upper);
			vector_from_points3d(&ragdoll->positions[parent], &ragdoll->positions[index], &lower);
			normalize3d(&upper);
			normalize3d(&lower);
			if (dot_product3d(&upper, &lower) < 0.94f) /* (bent more than 20 degrees) */
				bend = lower;
			else
				bend = biped->object.forward;
			ragdoll_link_bend(ragdoll, ragdoll_add_hinge(ragdoll, grandparent, parent, index, &bend, mode->elbow_twist),
				bends[parent], mode->elbow_bend);
		}
	}

	for (index = 0; index < ragdoll->particle_count; index++)
		ragdoll->inverse_masses[index] = 1.f;
	if (mode->weighted)
		ragdoll_build_weighted(ragdoll, model, mode);

	/* where the world lets each particle be, reached from its parent's (an
	animated pose can reach into a wall or the ground) */
	{
		for (index = 0; index < ragdoll->particle_count; index++)
			ragdoll->velocities[index] = *global_zero_vector3d;
		ragdoll_gather_features(ragdoll, biped_index);
		for (index = 0; index < ragdoll->particle_count; index++)
		{
			short particle = index < node_count ? ragdoll->order[index] : index;
			short parent = ragdoll->parents[particle];
			real_point3d desired = ragdoll->positions[particle];
			real_point3d start = parent != NONE ? ragdoll->positions[parent] : biped->object.position;

			ragdoll->positions[particle] = start;
			ragdoll_move(&ragdoll->positions[particle], &desired, NULL);
		}
	}
	return TRUE;
}

/* a push on the body, hardest nearest the epicenter: the same in all as a
push on the whole body, it turns the body too */
static void ragdoll_push(
	struct ragdoll *ragdoll,
	real_point3d const *epicenter,
	real_vector3d const *speed,
	real blow_radius,
	boolean spread)
{
	real weights[MAXIMUM_RAGDOLL_PARTICLES];
	real total = 0.f;
	short index;

	for (index = 0; index < ragdoll->particle_count; index++)
	{
		real distance = distance3d(epicenter, &ragdoll->positions[index]) / blow_radius;

		weights[index] = 1.f / (1.f + distance * distance);
		total += weights[index];
	}
	if (total <= 0.f)
		return;
	for (index = 0; index < ragdoll->particle_count; index++)
	{
		real scale = spread ?
			ragdoll_clamp(weights[index] * ragdoll->particle_count / total, 0.25f, 3.f) :
			weights[index];
		real_vector3d push;

		scale_vector3d(speed, scale, &push);
		add_vectors3d(&ragdoll->velocities[index], &push, &ragdoll->velocities[index]);
		ragdoll_clamp_speed(&ragdoll->velocities[index]);
	}
}

/* a number from minimum to maximum, from a ragdoll's own seed (the game's
random numbers are left as they are: every machine draws its own) */
static real ragdoll_random(
	unsigned long *seed,
	real minimum,
	real maximum)
{
	*seed = (*seed * 1664525UL + 1013904223UL) & 0xFFFFFFFFUL;
	return minimum + (maximum - minimum) * (real)(*seed >> 8) / (real)0x1000000;
}

/* the start's kick, turned about a level axis (ae_ragdoll_level_kick:
the part hit hardest, the far side less, no shove as a whole), every part
popped up (world units a tick) */
static void ragdoll_kick(
	struct ragdoll *ragdoll,
	real_point3d const *epicenter,
	real_vector3d const *direction,
	real strength,
	real lift)
{
	short index;

	ae_ragdoll_level_kick((float const (*)[3])ragdoll->positions, ragdoll->inverse_masses, ragdoll->particle_count,
		&epicenter->x, &direction->i, strength, lift, (float (*)[3])ragdoll->velocities);
	for (index = 0; index < ragdoll->particle_count; index++)
		ragdoll_clamp_speed(&ragdoll->velocities[index]);
}

/* (MODERN) the push of a killing bullet or melee, which CE's tags push 0:
its category's (ae_ragdoll_category_push) x 1.2, lifted as
damage.c lifts (24 degrees), the whole body at once; and a hard blow's tumble
(over 1.4 wu/s: random(0..1) x |push| x pi/2 a second, end over end). Returns
the push (world units a tick) for the trace */
static real ragdoll_category_push(
	struct ragdoll *ragdoll,
	struct ragdoll_blow const *blow,
	real blast_speed)
{
	real_vector3d direction = blow->direction;
	real speed = blow->speed > 0.f ? blast_speed :
		ae_ragdoll_category_push(blow->category) * RAGDOLL_CATEGORY_PUSH_SCALE / TICKS_PER_SECOND;
	short index;

	if (speed <= 0.f)
		return 0.f;
	direction.k += 0.45f;
	if (normalize3d(&direction) <= 0.f)
		return 0.f;
	if (blow->speed <= 0.f)
	{
		real_vector3d push;

		scale_vector3d(&direction, speed, &push);
		for (index = 0; index < ragdoll->particle_count; index++)
			add_vectors3d(&ragdoll->velocities[index], &push, &ragdoll->velocities[index]);
	}
	if (speed * TICKS_PER_SECOND > RAGDOLL_TUMBLE_SPEED)
	{
		real_vector3d push;
		/* (random(0..1) x |push| x pi/2 a second, a tick's: |push| is a tick's already) */
		real rate = ragdoll_random(&ragdoll->seed, 0.f, 1.f) * speed * 1.5707964f;

		scale_vector3d(&direction, speed, &push);
		ae_ragdoll_tumble((float const (*)[3])ragdoll->positions, ragdoll->inverse_masses, ragdoll->particle_count,
			&push.i, rate, (float (*)[3])ragdoll->velocities);
	}
	for (index = 0; index < ragdoll->particle_count; index++)
		ragdoll_clamp_speed(&ragdoll->velocities[index]);
	return speed;
}

/* how hard a blast pushes a body: as its killing blow or as a later blast,
by its damage's acceleration (world units a second) */
static real ragdoll_blast_scale(
	struct ragdoll_mode const *mode,
	real acceleration,
	boolean killing)
{
	real scale = killing ? mode->kill_blast : mode->blast;

	return acceleration > RAGDOLL_BIG_BLAST_ACCELERATION ? scale * mode->big_blast : scale;
}

/* whether a body playing its death animation goes limp this tick: so many
ticks after it died, killed by a blast or not, with this part of its death
animation played (1: none to play), and whether it has landed from the air
or lies on the ground */
static boolean ragdoll_limp_now(
	struct ragdoll_mode const *mode,
	boolean blast,
	long ticks,
	real played,
	boolean landed,
	boolean lying)
{
	if (mode->start == _ragdoll_start_at_death || blast || ticks >= RAGDOLL_LONGEST_WAIT_TICKS)
		return TRUE;
	if (mode->start == _ragdoll_start_after_moment)
		return ticks >= mode->wait_ticks || landed || played >= 1.f;
	return played >= mode->played || (ticks >= mode->wait_ticks && (landed || lying));
}

static struct ragdoll_blow const *ragdoll_killing_blow(
	long unit_index)
{
	long now = game_time_get();
	long index;

	for (index = 0; index < MAXIMUM_RAGDOLL_BLOWS; index++)
	{
		struct ragdoll_blow const *blow = &ragdoll_blows[index];

		if (blow->unit_index == unit_index && blow->time <= now && now - blow->time <= RAGDOLL_BLOW_TICKS)
			return blow;
	}
	return NULL;
}

/* lying still: its pose kept as it is; a body the ragdoll moves comes to rest
as a limp body that has finished relaxing, so that the stock game's code
finds its pose fixed where it lies (should it lose its ragdoll) */
static void ragdoll_sleep(
	struct ragdoll *ragdoll)
{
	struct biped_datum *biped = ragdoll_body(ragdoll);
	short index;

	ragdoll->asleep = TRUE;
	for (index = 0; index < ragdoll->particle_count; index++)
		ragdoll->velocities[index] = *global_zero_vector3d;
	if (!biped || ragdoll->drawn_only)
		return;
	biped->object.translational_velocity = *global_zero_vector3d;
	biped->object.angular_velocity = *global_zero_vector3d;
	SET_FLAG(biped->object.flags, _object_at_rest_bit, TRUE);
	SET_FLAG(biped->biped.flags, _biped_airborne_bit, FALSE);
	SET_FLAG(biped->biped.flags, _biped_limp_body_physics_active_bit, TRUE);
	SET_FLAG(biped->unit.animation.flags, _unit_animation_ignore_translation_bit, TRUE);
	biped->biped.limp_body_current_relaxation_iterations = biped->biped.limp_body_max_relaxation_iterations;
}

static void ragdoll_wake(
	struct ragdoll *ragdoll,
	struct biped_datum *biped)
{
	ragdoll->asleep = FALSE;
	ragdoll->still_ticks = 0;
	ragdoll->ticks = 0;
	if (ragdoll->drawn_only)
		return;
	SET_FLAG(biped->object.flags, _object_at_rest_bit, FALSE);
	SET_FLAG(biped->object.flags, _object_do_not_recompute_node_matrices_bit, TRUE);
	SET_FLAG(biped->biped.flags, _biped_limp_body_physics_active_bit, FALSE);
}

/* a body that loses its ragdoll is drawn as the stock game draws it: by its
animation, or as a limp body if it lies as one */
static void ragdoll_release(
	struct ragdoll *ragdoll)
{
	struct biped_datum *biped = ragdoll_body(ragdoll);

	if (biped && !ragdoll->waiting && !ragdoll->drawn_only &&
		!TEST_FLAG(biped->biped.flags, _biped_limp_body_physics_active_bit))
	{
		SET_FLAG(biped->object.flags, _object_do_not_recompute_node_matrices_bit, FALSE);
	}
	ragdoll->object_index = NONE;
}

/* a ragdoll for a body: a free one, one whose body is gone, or the longest
asleep (or else the oldest), whose body the stock game draws again */
static struct ragdoll *ragdoll_new(
	long object_index)
{
	struct ragdoll *chosen = NULL;
	long index;

	ragdolls_initialize();
	for (index = 0; index < MAXIMUM_RAGDOLLS; index++)
	{
		struct ragdoll *ragdoll = &ragdolls[index];

		if (ragdoll->object_index == NONE || !ragdoll_body(ragdoll))
		{
			chosen = ragdoll;
			break;
		}
		if (!chosen ||
			(ragdoll->asleep && !chosen->asleep) ||
			(ragdoll->asleep == chosen->asleep && ragdoll->start_time < chosen->start_time))
		{
			chosen = ragdoll;
		}
	}
	if (chosen->object_index != NONE)
		ragdoll_release(chosen);
	csmemset(chosen, 0, sizeof(*chosen));
	chosen->object_index = object_index;
	chosen->start_time = game_time_get();
	return chosen;
}

static boolean ragdoll_should_start(
	long biped_index)
{
	struct biped_datum *biped = biped_get(biped_index);
	struct biped_definition *definition = biped_definition_get(biped->definition_index);
	long now = game_time_get();

	/* a cinematic, and a body the scene marks as an actor, keep the
	scripted death */
	if (cinematic_in_progress() || TEST_FLAG(biped->object.flags, _object_movie_star_bit))
		return FALSE;
	return ragdoll_style(biped_index) != _ragdoll_style_none &&
		TEST_FLAG(biped->object.damage_flags, _object_dead_bit) &&
		biped->object.parent_object_index == NONE &&
		biped->unit.time_of_death != NONE &&
		biped->unit.time_of_death <= now &&
		now - biped->unit.time_of_death <= RAGDOLL_BLOW_TICKS &&
		!TEST_FLAG(definition->unit.flags, _unit_is_destroyed_after_dying_bit);
}

/* how far along its death animation a dead body is (1: done, or none to
play) */
static real ragdoll_death_played(
	struct biped_datum const *biped)
{
	struct animation_graph *graph;
	struct animation *animation;
	short index = biped->object.animation.state.index;

	if (TEST_FLAG(biped->biped.flags, _biped_limp_body_physics_active_bit))
		return 1.f;
	if (biped->unit.animation.state != _unit_state_dying && biped->unit.animation.state != _unit_state_dying_airborne)
		return 0.f;
	if (biped->object.animation.animation_graph_index == NONE)
		return 1.f;
	graph = animation_graph_definition_get(biped->object.animation.animation_graph_index);
	if (!VALID_INDEX(index, graph->animations.count))
		return 1.f;
	animation = TAG_BLOCK_GET_ELEMENT(&graph->animations, index, struct animation);
	if (animation->frame_count <= 0)
		return 1.f;
	/* (falling, it plays no further: it lands first) */
	if (biped->unit.animation.state == _unit_state_dying_airborne)
		return 0.f;
	return (real)biped->object.animation.state.frame_index / animation->frame_count;
}

/* the lowest and highest of a body's nodes */
static void ragdoll_node_heights(
	real_matrix4x3 const *nodes,
	short node_count,
	real *low,
	real *high)
{
	short index;

	*low = *high = nodes[0].position.z;
	for (index = 1; index < node_count; index++)
	{
		if (nodes[index].position.z < *low)
			*low = nodes[index].position.z;
		if (nodes[index].position.z > *high)
			*high = nodes[index].position.z;
	}
}

static short ragdoll_node_count(
	struct biped_datum const *biped)
{
	long count = biped->object.node_matrices.size / (long)sizeof(real_matrix4x3);

	return (short)(count > MAXIMUM_NODES_PER_MODEL ? MAXIMUM_NODES_PER_MODEL : count);
}

/* a body that has just died: the blow that killed it, and how high it
stood */
static void ragdoll_note_death(
	struct ragdoll *ragdoll,
	long biped_index)
{
	struct biped_datum *biped = biped_get(biped_index);
	struct ragdoll_blow const *blow = ragdoll_killing_blow(biped_index);
	short node_count = ragdoll_node_count(biped);
	real low;
	real high;

	ragdoll->waiting = TRUE;
	ragdoll->has_blow = blow != NULL;
	if (blow)
		ragdoll->blow = *blow;
	ragdoll->blast = blow && blow->speed * TICKS_PER_SECOND >= RAGDOLL_BLAST_ACCELERATION;
	ragdoll->seed = (unsigned long)biped_index * 2654435761UL ^ (unsigned long)game_time_get();
	if (node_count > 0)
	{
		ragdoll_node_heights(object_get_node_matrices(biped_index), node_count, &low, &high);
		ragdoll->standing_span = high - low;
	}
}

/* a body playing its death animation: TRUE when it goes limp now; else its
nodes are kept as they are, to start from at their speeds */
static boolean ragdoll_wait(
	struct ragdoll *ragdoll,
	long biped_index)
{
	struct biped_datum *biped = biped_get(biped_index);
	real_matrix4x3 *nodes = object_get_node_matrices(biped_index);
	short node_count = ragdoll_node_count(biped);
	boolean airborne = TEST_FLAG(biped->biped.flags, _biped_airborne_bit);
	boolean lying = FALSE;
	short index;

	if (node_count > 0 && !airborne)
	{
		real low;
		real high;

		ragdoll_node_heights(nodes, node_count, &low, &high);
		lying = high - low < RAGDOLL_LYING_SPAN * ragdoll->standing_span;
	}
	if (ragdoll_limp_now(ragdoll_mode_get(ragdoll->style), ragdoll->blast,
		game_time_get() - biped->unit.time_of_death, ragdoll_death_played(biped),
		ragdoll->was_airborne && !airborne, lying))
	{
		return TRUE;
	}
	if (airborne)
		ragdoll->was_airborne = TRUE;
	for (index = 0; index < node_count; index++)
		ragdoll->previous_nodes[index] = nodes[index].position;
	ragdoll->previous_count = node_count;
	return FALSE;
}

/* each particle at the speed its pose had (where a node is less where it
was last tick), a midway particle and the head's top at their parent's */
static void ragdoll_pose_speeds(
	struct ragdoll *ragdoll)
{
	short index;

	for (index = 0; index < ragdoll->particle_count; index++)
	{
		real_vector3d *velocity = &ragdoll->velocities[index];

		if (index < ragdoll->node_count)
		{
			real speed;

			vector_from_points3d(&ragdoll->previous_nodes[index], &ragdoll->pose[index].position, velocity);
			speed = magnitude3d(velocity);
			if (speed > RAGDOLL_POSE_MAXIMUM_SPEED)
				scale_vector3d(velocity, RAGDOLL_POSE_MAXIMUM_SPEED / speed, velocity);
		}
		else
		{
			*velocity = ragdoll->velocities[ragdoll->parents[index]];
		}
	}
}

/* RAGDOLL TRACE (owner 11-09: on EXAGGERATED and MIXED "some bullets kills make them fly off like an explosion"): a
body not killed by a blast whose parts move faster than RAGDOLL_TRACE_SPEED in its first two seconds limp, or whose first particle ends
farther than RAGDOLL_TRACE_TRAVEL from where it went limp, is told in the log once, with what it started with; with
HALO_RAGDOLL_TRACE=1 every body is told (the trace runs of 11-10 found the corrections' push: correction_limit) */
#define RAGDOLL_TRACE_SPEED 0.3f /* world units a tick: 9 a second */
#define RAGDOLL_TRACE_TRAVEL 3.f
#define RAGDOLL_TRACE_TICKS (2 * TICKS_PER_SECOND)

static char const *const ragdoll_trace_styles[NUMBER_OF_RAGDOLL_STYLES] =
	{ "none", "spaghetti_western", "exaggerated", "modern", "mixed" };

static void ragdoll_trace_start(
	struct ragdoll *ragdoll,
	struct biped_datum const *biped,
	struct ragdoll_blow const *blow,
	boolean posed)
{
	short index;

	ragdoll->trace_start = 0.f;
	for (index = 0; index < ragdoll->particle_count; index++)
	{
		real speed = magnitude3d(&ragdoll->velocities[index]);

		if (speed > ragdoll->trace_start)
			ragdoll->trace_start = speed;
	}
	ragdoll->trace_body = magnitude3d(&biped->object.translational_velocity);
	ragdoll->trace_blow = blow ? blow->speed : 0.f;
	ragdoll->trace_posed = posed;
	ragdoll->trace_peak = ragdoll->trace_start;
	ragdoll->trace_peak_tick = 0;
	ragdoll->trace_origin = ragdoll->positions[0];
	ragdoll->traced = FALSE;
}

static void ragdoll_trace_step(
	struct ragdoll *ragdoll,
	real fastest,
	boolean ending)
{
	real travel;

	if (ragdoll->traced)
		return;
	if (fastest > ragdoll->trace_peak)
	{
		ragdoll->trace_peak = fastest;
		ragdoll->trace_peak_tick = ragdoll->ticks;
	}
	if (!ending && ragdoll->ticks < RAGDOLL_TRACE_TICKS)
		return;
	ragdoll->traced = TRUE;
	travel = distance3d(&ragdoll->trace_origin, &ragdoll->positions[0]);
	if ((!ragdoll->blast && (ragdoll->trace_peak > RAGDOLL_TRACE_SPEED || travel > RAGDOLL_TRACE_TRAVEL)) ||
		getenv("HALO_RAGDOLL_TRACE"))
	{
		platform_log("ragdoll: body %ld style %s: fastest %.3f (tick %d), traveled %.2f; at limp: parts %.3f, body "
			"%.3f, blow %.3f, %s, blast %d", ragdoll->object_index,
			ragdoll->style > 0 && ragdoll->style < NUMBER_OF_RAGDOLL_STYLES ? ragdoll_trace_styles[ragdoll->style] : "?",
			ragdoll->trace_peak, (int)ragdoll->trace_peak_tick, travel, ragdoll->trace_start, ragdoll->trace_body,
			ragdoll->trace_blow, ragdoll->trace_posed ? "posed" : "unposed", (int)ragdoll->blast);
	}
}

static void ragdoll_start(
	struct ragdoll *ragdoll,
	long biped_index)
{
	struct biped_datum *biped = biped_get(biped_index);
	struct object_definition *definition = object_definition_get(biped->definition_index);
	struct ragdoll_mode const *mode = ragdoll_mode_get(ragdoll->style);
	struct ragdoll_blow const *blow = ragdoll->has_blow ? &ragdoll->blow : NULL;
	real_vector3d velocity = biped->object.translational_velocity;
	long now = game_time_get();
	/* (it played its death animation, which the blow had pushed) */
	boolean posed = ragdoll->previous_count > 0 && ragdoll->previous_count >= ragdoll->node_count;
	real category_push = 0.f;
	short index;

	ragdoll->waiting = FALSE;
	ragdoll->limp_time = now;
	if (posed)
	{
		ragdoll_pose_speeds(ragdoll);
	}
	else
	{
		ragdoll_clamp_speed(&velocity);
		for (index = 0; index < ragdoll->particle_count; index++)
			ragdoll->velocities[index] = velocity;
	}

	if (blow && !posed)
	{
		real_vector3d direction = blow->direction;
		real_vector3d speed;

		/* (as the damage pushes a body, damage.c: up a little; half of it
		already in the body's speed, from when it was alive) */
		real blast_speed = 0.f;

		if (blow->speed > 0.f && definition->object.acceleration_scale > _real_epsilon)
		{
			direction.k += 0.45f;
			if (normalize3d(&direction) > 0.f)
			{
				scale_vector3d(&direction, 0.5f * blow->speed * definition->object.acceleration_scale *
					ragdoll_blast_scale(mode, blow->speed * TICKS_PER_SECOND, TRUE), &speed);
				ragdoll_push(ragdoll, &blow->epicenter, &speed, RAGDOLL_BLOW_RADIUS, TRUE);
				/* (the whole push, the half the body had alive with it) */
				blast_speed = blow->speed * definition->object.acceleration_scale *
					ragdoll_blast_scale(mode, blow->speed * TICKS_PER_SECOND, TRUE);
			}
		}
		if (mode->kick)
			category_push = ragdoll_category_push(ragdoll, blow, blast_speed);
		/* the whole body pushed the way the blow came, turned by where it
		hit, and the shot's own push there */
		direction = blow->direction;
		if (normalize3d(&direction) > 0.f && (mode->blow_speed > 0.f || mode->hit_speed > 0.f))
		{
			scale_vector3d(&direction, mode->blow_speed, &speed);
			ragdoll_push(ragdoll, &blow->epicenter, &speed, RAGDOLL_BLOW_RADIUS, TRUE);
			scale_vector3d(&direction, mode->hit_speed, &speed);
			ragdoll_push(ragdoll, &blow->epicenter, &speed, RAGDOLL_HIT_RADIUS, FALSE);
		}
	}
	if (blow && mode->kick && blow->time <= now && now - blow->time <= RAGDOLL_KICK_TICKS)
	{
		real_vector3d direction = blow->direction;

		if (normalize3d(&direction) > 0.f)
		{
			ragdoll_kick(ragdoll, &blow->epicenter, &direction,
				RAGDOLL_KICK_SCALE * ragdoll_random(&ragdoll->seed, RAGDOLL_KICK_MINIMUM, RAGDOLL_KICK_MAXIMUM) /
					TICKS_PER_SECOND,
				ragdoll_random(&ragdoll->seed, RAGDOLL_KICK_LIFT_MINIMUM, RAGDOLL_KICK_LIFT_MAXIMUM) / TICKS_PER_SECOND);
		}
	}
	ragdoll_trace_start(ragdoll, biped, blow, posed);
	/* (the trace's blow: the category's push where it stood in for CE's 0) */
	if (category_push > ragdoll->trace_blow)
		ragdoll->trace_blow = category_push;
	if (mode->loosens)
		ragdoll_loosen(ragdoll, ragdoll_looseness(ragdoll->object_index, 0));

	ragdoll->drawn_only = game_connection() == _game_connection_network_server;
	if (!ragdoll->drawn_only)
	{
		SET_FLAG(biped->object.flags, _object_do_not_recompute_node_matrices_bit, TRUE);
		SET_FLAG(biped->object.flags, _object_at_rest_bit, FALSE);
		SET_FLAG(biped->biped.flags, _biped_limp_body_physics_active_bit, FALSE);
	}
}

/* ---------- simulation */

static void ragdoll_solve(
	struct ragdoll *ragdoll)
{
	long iteration;

	for (iteration = 0; iteration < RAGDOLL_ITERATIONS; iteration++)
	{
		short index;

		/* (the joints first: the bones' lengths have the last word) */
		for (index = 0; index < ragdoll->joint_count; index++)
			ragdoll_hold_joint(ragdoll, &ragdoll->joints[index]);
		for (index = 0; index < ragdoll->constraint_count; index++)
		{
			struct ragdoll_constraint const *constraint = &ragdoll->constraints[index];
			real_point3d *a = &ragdoll->positions[constraint->a];
			real_point3d *b = &ragdoll->positions[constraint->b];
			real_vector3d offset;
			real distance;
			real correction;
			real weight_a;
			real weight_b;

			vector_from_points3d(a, b, &offset);
			distance = magnitude3d(&offset);
			if (distance < 1e-6f)
				continue;
			if (constraint->type == _ragdoll_constraint_minimum && distance >= constraint->length)
				continue;
			/* (each end moved as readily as it moves: the lighter further) */
			weight_a = ragdoll->inverse_masses[constraint->a];
			weight_b = ragdoll->inverse_masses[constraint->b];
			if (weight_a + weight_b <= 0.f)
				continue;
			correction = (distance - constraint->length) / (distance * (weight_a + weight_b));
			a->x += offset.i * correction * weight_a;
			a->y += offset.j * correction * weight_a;
			a->z += offset.k * correction * weight_a;
			b->x -= offset.i * correction * weight_b;
			b->y -= offset.j * correction * weight_b;
			b->z -= offset.k * correction * weight_b;
		}
	}
}

/* half a tick: gravity and the speeds move the particles as far as the world
lets them, the bones pull them back together, as far as the world lets
them, and what they ran into slows them */
/* a particle's speed after a substep: what the corrections added to the speed it came with, at most limit (0: no
limit) */
static void ragdoll_limit_correction(
	real_vector3d *velocity,
	real_vector3d const *incoming,
	real limit)
{
	real_vector3d added;
	real size;

	if (limit <= 0.f)
		return;
	vector_from_points3d((real_point3d const *)incoming, (real_point3d const *)velocity, &added);
	size = magnitude3d(&added);
	if (size > limit)
	{
		velocity->i = incoming->i + added.i * limit / size;
		velocity->j = incoming->j + added.j * limit / size;
		velocity->k = incoming->k + added.k * limit / size;
	}
}

static void ragdoll_substep(
	struct ragdoll *ragdoll)
{
	real_point3d starts[MAXIMUM_RAGDOLL_PARTICLES];
	real_point3d moved[MAXIMUM_RAGDOLL_PARTICLES];
	real_vector3d normals[MAXIMUM_RAGDOLL_PARTICLES];
	real_vector3d incoming[MAXIMUM_RAGDOLL_PARTICLES];
	boolean hits[MAXIMUM_RAGDOLL_PARTICLES];
	struct ragdoll_mode const *mode = ragdoll_mode_get(ragdoll->style);
	real const step = 1.f / RAGDOLL_SUBSTEPS;
	real const gravity = global_gravity * mode->gravity * step;
	short index;

	for (index = 0; index < ragdoll->particle_count; index++)
	{
		real_vector3d *velocity = &ragdoll->velocities[index];
		real_point3d destination;

		velocity->k -= gravity;
		scale_vector3d(velocity, mode->damping, velocity);
		ragdoll_clamp_speed(velocity);
		incoming[index] = *velocity;
		starts[index] = ragdoll->positions[index];
		point_from_line3d(&starts[index], velocity, step, &destination);
		hits[index] = ragdoll_move(&ragdoll->positions[index], &destination, &normals[index]);
		moved[index] = ragdoll->positions[index];
	}

	ragdoll_solve(ragdoll);

	for (index = 0; index < ragdoll->particle_count; index++)
	{
		real_point3d solved = ragdoll->positions[index];
		real_vector3d normal;
		real_vector3d *velocity = &ragdoll->velocities[index];

		ragdoll->positions[index] = moved[index];
		if (ragdoll_move(&ragdoll->positions[index], &solved, &normal))
		{
			hits[index] = TRUE;
			normals[index] = normal;
		}
		vector_from_points3d(&starts[index], &ragdoll->positions[index], velocity);
		scale_vector3d(velocity, 1.f / step, velocity);
		/* (the speed the solver's and the world's corrections add to the particle's own, the part of its move
		beyond where its own speed took it, at most correction_limit: a joint or a wall put right at once is no
		push. Uncapped, a body killed by a bullet whipped a limb off at 0.4 world units a tick a second after it
		fell, owner 11-09: "some bullets kills make them fly off like an explosion"; the small pulls, a joint's
		drag, a limb carried along, are under the cap and kept whole) */
		ragdoll_limit_correction(velocity, &incoming[index], mode->correction_limit);
		if (hits[index])
		{
			real_vector3d const *normal = &normals[index];
			real into = dot_product3d(velocity, normal);
			real landing = dot_product3d(&incoming[index], normal);
			real away;

			/* (nothing into what it lies on, and less along it) */
			if (into < 0.f)
			{
				velocity->i -= normal->i * into;
				velocity->j -= normal->j * into;
				velocity->k -= normal->k * into;
			}
			away = dot_product3d(velocity, normal);
			if (mode->coulomb > 0.f)
			{
				/* (slower along it by how hard it presses, never past still,
				and a hard landing bounced back a little) */
				real_vector3d along;
				real slowing = mode->coulomb * (MAX(0.f, -landing) + gravity * MAX(0.f, normal->k));
				real speed;

				along.i = velocity->i - normal->i * away;
				along.j = velocity->j - normal->j * away;
				along.k = velocity->k - normal->k * away;
				speed = magnitude3d(&along);
				scale_vector3d(&along, speed > slowing ? (speed - slowing) / speed : 0.f, &along);
				if (-landing > RAGDOLL_BOUNCE_SPEED && away < -landing * mode->restitution)
					away = -landing * mode->restitution;
				velocity->i = normal->i * away + along.i;
				velocity->j = normal->j * away + along.j;
				velocity->k = normal->k * away + along.k;
			}
			else
			{
				velocity->i = normal->i * away + (velocity->i - normal->i * away) * mode->friction;
				velocity->j = normal->j * away + (velocity->j - normal->j * away) * mode->friction;
				velocity->k = normal->k * away + (velocity->k - normal->k * away) * mode->friction;
			}
		}
	}

	/* (joints that drag, or hold by a dry friction: a part's swing about its
	parent slowed) */
	if (mode->joint_drag > 0.f || mode->joint_friction > 0.f)
	{
		real holding = mode->joint_friction * ragdoll->looseness;

		for (index = 0; index < ragdoll->particle_count; index++)
		{
			short parent = ragdoll->parents[index];
			real_vector3d *velocity = &ragdoll->velocities[index];
			real_vector3d swing;
			real speed;
			real slowing;

			if (parent == NONE)
				continue;
			swing.i = velocity->i - ragdoll->velocities[parent].i;
			swing.j = velocity->j - ragdoll->velocities[parent].j;
			swing.k = velocity->k - ragdoll->velocities[parent].k;
			speed = magnitude3d(&swing);
			if (speed <= 0.f)
				continue;
			/* (a drag takes a part of it, a dry friction as much as it holds,
			never more than all of it) */
			slowing = speed * mode->joint_drag;
			slowing += MIN(speed - slowing, holding);
			velocity->i -= swing.i * slowing / speed;
			velocity->j -= swing.j * slowing / speed;
			velocity->k -= swing.k * slowing / speed;
		}
	}
}

/* the nodes turned as their bones turned, parents first: a bone that
branches by its two ends, one that doesn't the shortest way (twisting as
little as it can), and a node with no bone of its own as its parent */
static void ragdoll_pose(
	struct ragdoll const *ragdoll,
	real_point3d const *previous,
	real_matrix4x3 *node_matrices)
{
	struct ragdoll_rotation turns[MAXIMUM_NODES_PER_MODEL];
	short order_index;

	for (order_index = 0; order_index < ragdoll->node_count; order_index++)
	{
		short node = ragdoll->order[order_index];
		short primary = ragdoll->primary[node];
		short secondary = ragdoll->secondary[node];
		struct ragdoll_rotation *turn = &turns[node];
		real_matrix4x3 *matrix = &node_matrices[node];
		boolean turned = FALSE;

		if (primary != NONE)
		{
			real_vector3d before;
			real_vector3d after;

			vector_from_points3d(&previous[node], &previous[primary], &before);
			vector_from_points3d(&ragdoll->positions[node], &ragdoll->positions[primary], &after);
			if (secondary != NONE)
			{
				real_vector3d before_across;
				real_vector3d after_across;
				struct ragdoll_rotation from;
				struct ragdoll_rotation to;

				vector_from_points3d(&previous[node], &previous[secondary], &before_across);
				vector_from_points3d(&ragdoll->positions[node], &ragdoll->positions[secondary], &after_across);
				if (basis_from_directions(&before, &before_across, &from) &&
					basis_from_directions(&after, &after_across, &to))
				{
					rotation_between_bases(&from, &to, turn);
					turned = TRUE;
				}
			}
			if (!turned && normalize3d(&before) > 1e-4f && normalize3d(&after) > 1e-4f)
			{
				rotation_between(&before, &after, turn);
				turned = TRUE;
			}
		}
		if (!turned)
		{
			if (node && ragdoll->parents[node] != NONE)
				*turn = turns[ragdoll->parents[node]];
			else
				rotation_identity(turn);
		}

		rotation_transform(turn, &matrix->forward, &matrix->forward);
		rotation_transform(turn, &matrix->up, &matrix->up);
		if (normalize3d(&matrix->forward) == 0.f)
			matrix->forward = *global_forward3d;
		cross_product3d(&matrix->up, &matrix->forward, &matrix->left);
		if (normalize3d(&matrix->left) == 0.f)
			perpendicular3d(&matrix->forward, &matrix->left);
		cross_product3d(&matrix->forward, &matrix->left, &matrix->up);
		normalize3d(&matrix->up);
		matrix->position = ragdoll->positions[node];
	}
}

/* a body the ragdoll moves: its nodes as the ragdoll lies, kept from being
worked out again from its animation, its origin at the root, and its bounds
round the whole of it, as the camera, the map and the renderer find it; a
ragdoll asleep keeps them */
static void ragdoll_move_body(
	struct ragdoll const *ragdoll,
	long biped_index,
	boolean moved)
{
	struct biped_datum *biped = biped_get(biped_index);
	struct object_definition *definition = object_definition_get(biped->definition_index);
	real bounds = definition->object.bounding_radius;
	real_point3d center;
	real radius;

	csmemcpy(object_get_node_matrices(biped_index), ragdoll->pose, ragdoll->node_count * sizeof(real_matrix4x3));
	SET_FLAG(biped->object.flags, _object_do_not_recompute_node_matrices_bit, TRUE);
	if (!moved)
		return;
	ragdoll_bounds(ragdoll, FALSE, &center, &radius);
	if (biped->object.scale > 0.f)
		bounds *= biped->object.scale;
	if (radius + 2.f * RAGDOLL_RADIUS > bounds)
		bounds = radius + 2.f * RAGDOLL_RADIUS;
	object_disconnect_from_map(biped_index);
	biped->object.position = ragdoll->positions[0];
	biped->object.bounding_sphere_center = center;
	biped->object.bounding_sphere_radius = bounds;
	object_bounds_cache_update(biped_index, &center, bounds);
	object_reconnect_to_map(biped_index, NULL);
	biped->object.translational_velocity = ragdoll->velocities[0];
	biped->object.angular_velocity = *global_zero_vector3d;
}

static void ragdoll_step(
	struct ragdoll *ragdoll,
	long biped_index)
{
	real_point3d previous[MAXIMUM_RAGDOLL_PARTICLES];
	real fastest = 0.f;
	short index;

	if (ragdoll_mode_get(ragdoll->style)->loosens)
	{
		real looseness = ragdoll_looseness(ragdoll->object_index, game_time_get() - ragdoll->limp_time);

		if (looseness != ragdoll->looseness)
			ragdoll_loosen(ragdoll, looseness);
	}

	match_assert(__FILE__, __LINE__, global_current_collision_user_depth < MAXIMUM_COLLISION_USER_STACK_DEPTH);
	global_current_collision_users[global_current_collision_user_depth++] = _collision_user_limp_body_physics;

	csmemcpy(previous, ragdoll->positions, ragdoll->particle_count * sizeof(real_point3d));
	ragdoll_gather_features(ragdoll, biped_index);
	for (index = 0; index < RAGDOLL_SUBSTEPS; index++)
		ragdoll_substep(ragdoll);

	match_assert(__FILE__, __LINE__, global_current_collision_user_depth > 1);
	--global_current_collision_user_depth;

	ragdoll_pose(ragdoll, previous, ragdoll->pose);

	for (index = 0; index < ragdoll->particle_count; index++)
	{
		real speed = magnitude3d(&ragdoll->velocities[index]);

		if (speed > fastest)
			fastest = speed;
	}
	ragdoll->still_ticks = fastest < RAGDOLL_SLEEP_SPEED ? ragdoll->still_ticks + 1 : 0;
	ragdoll->ticks++;
	ragdoll_trace_step(ragdoll, fastest,
		(boolean)(ragdoll->still_ticks >= RAGDOLL_SLEEP_TICKS || ragdoll->ticks >= RAGDOLL_MAXIMUM_TICKS));
	if (ragdoll->still_ticks >= RAGDOLL_SLEEP_TICKS || ragdoll->ticks >= RAGDOLL_MAXIMUM_TICKS)
		ragdoll_sleep(ragdoll);
}

boolean biped_ragdoll_update(
	long biped_index)
{
	struct ragdoll *ragdoll = ragdoll_find(biped_index);
	boolean moved;

	if (ragdoll && !ragdoll_body(ragdoll))
	{
		ragdoll_release(ragdoll);
		return FALSE;
	}
	if (!ragdoll)
	{
		if (!ragdoll_should_start(biped_index))
			return FALSE;
		ragdoll = ragdoll_new(biped_index);
		ragdoll->style = ragdoll_style(biped_index);
		ragdoll_note_death(ragdoll, biped_index);
	}
	/* (its death animation played, as the stock game plays it, until it
	goes limp) */
	if (ragdoll->waiting)
	{
		if (!ragdoll_wait(ragdoll, biped_index))
			return FALSE;
		if (!ragdoll_build(ragdoll, biped_index))
		{
			ragdoll->object_index = NONE;
			return FALSE;
		}
		ragdoll_start(ragdoll, biped_index);
	}
	moved = !ragdoll->asleep;
	if (moved)
		ragdoll_step(ragdoll, biped_index);
	if (ragdoll->drawn_only)
		return FALSE;
	ragdoll_move_body(ragdoll, biped_index, moved);
	return TRUE;
}

boolean biped_ragdoll_active(
	long object_index)
{
	struct ragdoll *ragdoll = ragdoll_find(object_index);

	return ragdoll && !ragdoll->waiting && ragdoll_body(ragdoll);
}

boolean biped_ragdoll_accelerate(
	long biped_index,
	real_point3d const *epicenter,
	real_vector3d const *acceleration)
{
	struct ragdoll *ragdoll = ragdoll_find(biped_index);
	struct biped_datum *biped;
	struct object_definition *definition;
	real_vector3d push;
	real strength;

	/* (a body playing its death animation takes the push as the stock game
	gives it) */
	if (!ragdoll || ragdoll->waiting || (biped = ragdoll_body(ragdoll)) == NULL)
		return FALSE;
	if (ragdoll->asleep)
		ragdoll_wake(ragdoll, biped);
	/* (the damage's own acceleration, world units a second: damage.c's push
	less the body's scale) */
	definition = object_definition_get(biped->definition_index);
	strength = magnitude3d(acceleration) * TICKS_PER_SECOND;
	if (definition->object.acceleration_scale > _real_epsilon)
		strength /= definition->object.acceleration_scale;
	scale_vector3d(acceleration, ragdoll_blast_scale(ragdoll_mode_get(ragdoll->style), strength, FALSE), &push);
	ragdoll_push(ragdoll, epicenter, &push, RAGDOLL_BLOW_RADIUS, TRUE);
	return !ragdoll->drawn_only;
}

real_matrix4x3 *biped_ragdoll_drawn_nodes(
	long object_index)
{
	struct ragdoll *ragdoll;
	struct object_datum *object = object_try_and_get_and_verify_type(object_index, _object_mask_biped);

	if (!object || !TEST_FLAG(object->object.damage_flags, _object_dead_bit))
		return NULL;
	ragdoll = ragdoll_find(object_index);
	if (!ragdoll || !ragdoll->drawn_only ||
		ragdoll->node_count != object->object.node_matrices.size / (long)sizeof(real_matrix4x3))
	{
		return NULL;
	}
	return ragdoll->pose;
}

void biped_ragdoll_note_killing_blow(
	long unit_index,
	real_point3d const *epicenter,
	real_vector3d const *direction,
	real instantaneous_acceleration,
	short category)
{
	struct ragdoll_blow *blow;

	if (ragdoll_style(unit_index) == _ragdoll_style_none)
		return;
	ragdolls_initialize();
	blow = &ragdoll_blows[ragdoll_next_blow];
	ragdoll_next_blow = (ragdoll_next_blow + 1) % MAXIMUM_RAGDOLL_BLOWS;
	blow->unit_index = unit_index;
	blow->time = game_time_get();
	blow->epicenter = *epicenter;
	blow->direction = *direction;
	/* (world units a tick, as damage.c pushes) */
	blow->speed = instantaneous_acceleration > _real_epsilon ? instantaneous_acceleration / TICKS_PER_SECOND : 0.f;
	blow->category = category;
}

void biped_ragdoll_reset(
	void)
{
	ragdolls_initialized = FALSE;
	ragdolls_initialize();
}
