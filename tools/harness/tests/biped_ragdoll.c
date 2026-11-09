#include "harness.h"
#include <math.h>

/* A ragdoll of thirteen particles (biped_ragdoll.c), standing on flat ground
at z 0: a pelvis, a spine with the head and both arms branching from it, and
two legs of thigh, calf and foot. Collision is the ground alone, raised by a
particle's radius as the game's features are (collision_move_point). */
#define MAXIMUM_NODES_PER_MODEL 64
#define TICKS_PER_SECOND 30
#define NUMBEROF(a) (sizeof(a) / sizeof((a)[0]))
#define _real_epsilon 0.0001f
#define square_root(x) ((real)sqrt(x))
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define MIN(a, b) ((a) > (b) ? (b) : (a))
#define _pi ((real)3.14159265358979323846f)
#define cosine(x) ((real)cos(x))
#define arccosine(x) ((real)acos(x))

typedef struct { real_vector3d n; real d; } real_plane3d;
typedef struct
{
	real scale;
	real_vector3d forward, left, up;
	real_point3d position;
} real_matrix4x3;
struct collision_plane { real t; real_point3d point; real_plane3d plane; long object_index; };
struct collision_feature_list { int unused; };

static real global_gravity = 0.00228f; /* (physics.c's, world units a tick each tick) */
static real_vector3d const zero_vector3d = { 0.f, 0.f, 0.f };
static real_vector3d const forward3d = { 1.f, 0.f, 0.f };
#define global_zero_vector3d (&zero_vector3d)
#define global_forward3d (&forward3d)

static real dot_product3d(real_vector3d const *a, real_vector3d const *b) { return a->i * b->i + a->j * b->j + a->k * b->k; }
static real magnitude3d(real_vector3d const *v) { return (real)sqrt(dot_product3d(v, v)); }
static real_vector3d *cross_product3d(real_vector3d const *a, real_vector3d const *b, real_vector3d *r)
{
	real_vector3d out;

	out.i = a->j * b->k - a->k * b->j;
	out.j = a->k * b->i - a->i * b->k;
	out.k = a->i * b->j - a->j * b->i;
	*r = out;
	return r;
}
static real normalize3d(real_vector3d *v)
{
	real length = magnitude3d(v);

	if (length < _real_epsilon)
		return 0.f;
	v->i /= length;
	v->j /= length;
	v->k /= length;
	return length;
}
static real_vector3d *scale_vector3d(real_vector3d const *v, real s, real_vector3d *r)
{
	r->i = v->i * s;
	r->j = v->j * s;
	r->k = v->k * s;
	return r;
}
static real_vector3d *add_vectors3d(real_vector3d const *a, real_vector3d const *b, real_vector3d *r)
{
	r->i = a->i + b->i;
	r->j = a->j + b->j;
	r->k = a->k + b->k;
	return r;
}
static real_vector3d *vector_from_points3d(real_point3d const *a, real_point3d const *b, real_vector3d *r)
{
	r->i = b->x - a->x;
	r->j = b->y - a->y;
	r->k = b->z - a->z;
	return r;
}
static real distance3d(real_point3d const *a, real_point3d const *b)
{
	real_vector3d v;

	return magnitude3d(vector_from_points3d(a, b, &v));
}
static real_point3d *point_from_line3d(real_point3d const *p, real_vector3d const *v, real t, real_point3d *r)
{
	r->x = p->x + v->i * t;
	r->y = p->y + v->j * t;
	r->z = p->z + v->k * t;
	return r;
}
static void set_real_vector3d(real_vector3d *v, real i, real j, real k) { v->i = i; v->j = j; v->k = k; }
static real_vector3d *perpendicular3d(real_vector3d const *v, real_vector3d *r) { set_real_vector3d(r, -v->j, v->i, 0.f); return r; }

#define RADIUS_OF_THE_GROUND 0.025f
static struct collision_feature_list ragdoll_features;

/* the ground: a move that would end below it stops on it, sliding on along it */
static short collision_move_point(real_point3d const *position, real_vector3d const *velocity,
	struct collision_feature_list const *features, real_point3d *clipped_position, real_vector3d *clipped_velocity,
	short maximum_collision_count, struct collision_plane *collisions)
{
	real t;

	if (position->z + velocity->k >= RADIUS_OF_THE_GROUND || velocity->k >= 0.f)
	{
		point_from_line3d(position, velocity, 1.f, clipped_position);
		*clipped_velocity = *velocity;
		return 0;
	}
	t = position->z <= RADIUS_OF_THE_GROUND ? 0.f : (RADIUS_OF_THE_GROUND - position->z) / velocity->k;
	clipped_position->x = position->x + velocity->i;
	clipped_position->y = position->y + velocity->j;
	clipped_position->z = RADIUS_OF_THE_GROUND;
	set_real_vector3d(clipped_velocity, velocity->i, velocity->j, 0.f);
	set_real_vector3d(&collisions[0].plane.n, 0.f, 0.f, 1.f);
	collisions[0].plane.d = RADIUS_OF_THE_GROUND;
	collisions[0].t = t;
	return 1;
}

#include "under_test.inc"

enum { PELVIS, SPINE, HEAD, L_ARM, L_HAND, R_ARM, R_HAND, L_THIGH, L_CALF, L_FOOT, R_THIGH, R_CALF, R_FOOT, PARTICLES };

static short const parents[PARTICLES] = { NONE, PELVIS, SPINE, SPINE, L_ARM, SPINE, R_ARM, PELVIS, L_THIGH, L_CALF,
	PELVIS, R_THIGH, R_CALF };
static real const standing[PARTICLES][3] = {
	{ 0.f, 0.f, 0.35f }, { 0.f, 0.f, 0.5f }, { 0.f, 0.f, 0.62f },
	{ 0.f, 0.09f, 0.55f }, { 0.02f, 0.11f, 0.37f }, { 0.f, -0.09f, 0.55f }, { 0.02f, -0.11f, 0.37f },
	{ 0.f, 0.05f, 0.33f }, { 0.01f, 0.05f, 0.18f }, { 0.f, 0.05f, 0.04f },
	{ 0.f, -0.05f, 0.33f }, { 0.01f, -0.05f, 0.18f }, { 0.f, -0.05f, 0.04f } };

static struct ragdoll ragdoll;
#define spaghetti_western ragdoll_mode_get(_ragdoll_style_spaghetti_western)
#define exaggerated ragdoll_mode_get(_ragdoll_style_exaggerated)
#define modern ragdoll_mode_get(_ragdoll_style_modern)
#define mixed ragdoll_mode_get(_ragdoll_style_mixed)
static real_matrix4x3 matrices[PARTICLES];
static real lengths[PARTICLES];

static void stand(void)
{
	short index;
	short children[PARTICLES][3];
	short counts[PARTICLES];

	memset(&ragdoll, 0, sizeof(ragdoll));
	memset(counts, 0, sizeof(counts));
	ragdoll.node_count = ragdoll.particle_count = PARTICLES;
	for (index = 0; index < PARTICLES; index++)
	{
		ragdoll.positions[index].x = standing[index][0];
		ragdoll.positions[index].y = standing[index][1];
		ragdoll.positions[index].z = standing[index][2];
		ragdoll.parents[index] = parents[index];
		ragdoll.order[index] = index;
		matrices[index].scale = 1.f;
		set_real_vector3d(&matrices[index].forward, 1.f, 0.f, 0.f);
		set_real_vector3d(&matrices[index].left, 0.f, 1.f, 0.f);
		set_real_vector3d(&matrices[index].up, 0.f, 0.f, 1.f);
		matrices[index].position = ragdoll.positions[index];
		if (parents[index] != NONE)
		{
			children[parents[index]][counts[parents[index]]++] = index;
			lengths[index] = distance3d(&ragdoll.positions[index], &ragdoll.positions[parents[index]]);
			ragdoll_add_constraint(&ragdoll, parents[index], index, _ragdoll_constraint_length, lengths[index]);
		}
	}
	/* (as ragdoll_build has them: rigid where a bone branches, bending
	where it doesn't, the spine and neck stiffly) */
	ragdoll_add_constraint(&ragdoll, L_THIGH, R_THIGH, _ragdoll_constraint_length, 0.1f);
	ragdoll_add_constraint(&ragdoll, L_THIGH, SPINE, _ragdoll_constraint_length,
		distance3d(&ragdoll.positions[L_THIGH], &ragdoll.positions[SPINE]));
	ragdoll_add_constraint(&ragdoll, R_THIGH, SPINE, _ragdoll_constraint_length,
		distance3d(&ragdoll.positions[R_THIGH], &ragdoll.positions[SPINE]));
	ragdoll_add_constraint(&ragdoll, HEAD, L_ARM, _ragdoll_constraint_length,
		distance3d(&ragdoll.positions[HEAD], &ragdoll.positions[L_ARM]));
	ragdoll_add_constraint(&ragdoll, HEAD, R_ARM, _ragdoll_constraint_length,
		distance3d(&ragdoll.positions[HEAD], &ragdoll.positions[R_ARM]));
	ragdoll_add_constraint(&ragdoll, L_ARM, R_ARM, _ragdoll_constraint_length, 0.18f);
	ragdoll_add_bend(&ragdoll, PELVIS, L_THIGH, L_CALF, spaghetti_western->bend);
	ragdoll_add_bend(&ragdoll, L_THIGH, L_CALF, L_FOOT, spaghetti_western->knee_bend);
	ragdoll_add_bend(&ragdoll, PELVIS, R_THIGH, R_CALF, spaghetti_western->bend);
	ragdoll_add_bend(&ragdoll, R_THIGH, R_CALF, R_FOOT, spaghetti_western->knee_bend);
	for (index = 0; index < PARTICLES; index++)
	{
		ragdoll.primary[index] = counts[index] > 0 ? children[index][0] : NONE;
		ragdoll.secondary[index] = counts[index] > 1 ? children[index][1] : NONE;
		ragdoll.velocities[index] = zero_vector3d;
		ragdoll.inverse_masses[index] = 1.f;
	}
	ragdoll.style = _ragdoll_style_spaghetti_western;
	ragdoll.looseness = 1.f;
	/* (the knees bend back, the hips swing within their cones) */
	{
		real_vector3d back = { -1.f, 0.f, 0.f };

		ragdoll_add_hinge(&ragdoll, L_THIGH, L_CALF, L_FOOT, &back, spaghetti_western->knee_twist);
		ragdoll_add_hinge(&ragdoll, R_THIGH, R_CALF, R_FOOT, &back, spaghetti_western->knee_twist);
		ragdoll_add_cone(&ragdoll, L_THIGH, L_CALF, &forward3d, spaghetti_western->hip_tilt, spaghetti_western->hip_cone);
		ragdoll_add_cone(&ragdoll, R_THIGH, R_CALF, &forward3d, spaghetti_western->hip_tilt, spaghetti_western->hip_cone);
		CHECK(ragdoll.joint_count == 4, "%d joints", ragdoll.joint_count);
	}
}

/* the parts weighted as a style weighs them */
static void weigh(short style)
{
	struct ragdoll_mode const *mode = ragdoll_mode_get(style);
	static short const parts[PARTICLES] = { _ragdoll_part_torso, _ragdoll_part_torso, _ragdoll_part_head,
		_ragdoll_part_upper_arm, _ragdoll_part_hand, _ragdoll_part_upper_arm, _ragdoll_part_hand,
		_ragdoll_part_thigh, _ragdoll_part_calf, _ragdoll_part_foot, _ragdoll_part_thigh, _ragdoll_part_calf,
		_ragdoll_part_foot };
	short index;

	ragdoll.style = style;
	for (index = 0; index < PARTICLES; index++)
		ragdoll.inverse_masses[index] = mode->inverse_masses[parts[index]];
}

static real fastest(void)
{
	real result = 0.f;
	short index;

	for (index = 0; index < PARTICLES; index++)
	{
		real speed = magnitude3d(&ragdoll.velocities[index]);

		if (speed > result)
			result = speed;
	}
	return result;
}

/* a tick as ragdoll_step has it, without the world's features and the body */
static void tick(void)
{
	real_point3d previous[PARTICLES];
	short index;

	memcpy(previous, ragdoll.positions, sizeof(previous));
	for (index = 0; index < RAGDOLL_SUBSTEPS; index++)
		ragdoll_substep(&ragdoll);
	ragdoll_pose(&ragdoll, previous, matrices);
}

static void check_body(int tick_index)
{
	short index;

	for (index = 0; index < PARTICLES; index++)
	{
		real_vector3d left;

		CHECK(ragdoll.positions[index].z > RADIUS_OF_THE_GROUND - 0.001f, "particle %d at %f in the ground (tick %d)",
			index, ragdoll.positions[index].z, tick_index);
		if (parents[index] != NONE)
		{
			real length = distance3d(&ragdoll.positions[index], &ragdoll.positions[parents[index]]);

			CHECK(fabs(length - lengths[index]) < 0.15f * lengths[index] + 0.003f,
				"bone %d %f long, not %f (tick %d)", index, length, lengths[index], tick_index);
		}
		CHECK(fabs(magnitude3d(&matrices[index].forward) - 1.f) < 0.001f &&
			fabs(magnitude3d(&matrices[index].up) - 1.f) < 0.001f &&
			fabs(dot_product3d(&matrices[index].forward, &matrices[index].up)) < 0.001f,
			"node %d not a rotation (tick %d)", index, tick_index);
		cross_product3d(&matrices[index].up, &matrices[index].forward, &left);
		CHECK(dot_product3d(&left, &matrices[index].left) > 0.999f, "node %d mirrored (tick %d)", index, tick_index);
	}
}

int main(int argc, char **argv)
{
	char const *case_name = argc > 1 ? argv[1] : "";
	int tick_index;

	/* tipped forward, it falls, lies on the ground in one piece and comes
	to rest */
	CASE("falls_and_rests")
	{
		short index;

		stand();
		for (index = 0; index < PARTICLES; index++)
			ragdoll.velocities[index].i = 0.002f * standing[index][2] / 0.6f;
		for (tick_index = 0; tick_index < 6 * TICKS_PER_SECOND; tick_index++)
		{
			tick();
			check_body(tick_index);
		}
		CHECK(ragdoll.positions[HEAD].z < 0.2f, "the head still up at %f", ragdoll.positions[HEAD].z);
		CHECK(fastest() < RAGDOLL_SLEEP_SPEED, "still moving at %f", fastest());
		return 0;
	}
	/* the nodes turn with their bones: a node's forward keeps its angle to
	its bone as the body falls */
	CASE("nodes_follow_bones")
	{
		real before;
		real_vector3d bone;

		stand();
		vector_from_points3d(&ragdoll.positions[L_CALF], &ragdoll.positions[L_FOOT], &bone);
		normalize3d(&bone);
		before = dot_product3d(&bone, &matrices[L_CALF].forward);
		for (tick_index = 0; tick_index < 2 * TICKS_PER_SECOND; tick_index++)
			ragdoll.velocities[HEAD].i += 0.002f, tick();
		vector_from_points3d(&ragdoll.positions[L_CALF], &ragdoll.positions[L_FOOT], &bone);
		normalize3d(&bone);
		CHECK(fabs(dot_product3d(&bone, &matrices[L_CALF].forward) - before) < 0.02f,
			"the calf's node turned away from its bone (%f, was %f)",
			dot_product3d(&bone, &matrices[L_CALF].forward), before);
		CHECK(dot_product3d(&matrices[L_CALF].forward, &forward3d) < 0.99f, "the calf's node never turned");
		return 0;
	}
	/* a push at the head moves the head more than the feet: the body turns */
	CASE("push_turns")
	{
		real_point3d epicenter = { -0.05f, 0.f, 0.62f };
		real_vector3d speed = { 0.05f, 0.f, 0.f };

		stand();
		ragdoll_push(&ragdoll, &epicenter, &speed, RAGDOLL_BLOW_RADIUS, TRUE);
		CHECK(ragdoll.velocities[HEAD].i > 2.f * ragdoll.velocities[L_FOOT].i, "the head %f, the feet %f",
			ragdoll.velocities[HEAD].i, ragdoll.velocities[L_FOOT].i);
		CHECK(ragdoll.velocities[L_FOOT].i > 0.f, "the feet not pushed at all");
		return 0;
	}
	/* a body thrown hard comes down in one piece, no faster than it can go */
	CASE("thrown")
	{
		short index;

		stand();
		for (index = 0; index < PARTICLES; index++)
			set_real_vector3d(&ragdoll.velocities[index], 3.f, 0.f, 2.f);
		for (tick_index = 0; tick_index < 4 * TICKS_PER_SECOND; tick_index++)
		{
			tick();
			CHECK(fastest() <= RAGDOLL_MAXIMUM_SPEED * 1.5f, "%f a tick (tick %d)", fastest(), tick_index);
			check_body(tick_index);
		}
		return 0;
	}
	/* feet kicked forward bend the knees no further than straight, and
	keep them in their plane */
	CASE("knees_bend_one_way")
	{
		stand();
		ragdoll.velocities[L_FOOT].i = ragdoll.velocities[R_FOOT].i = 0.2f;
		for (tick_index = 0; tick_index < TICKS_PER_SECOND; tick_index++)
		{
			short joint;

			tick();
			for (joint = 0; joint < 2; joint++)
			{
				struct ragdoll_joint const *hinge = &ragdoll.joints[joint];
				struct ragdoll_rotation frame;
				real_vector3d axis, upper, lower, bent;

				CHECK(ragdoll_frame(&ragdoll, hinge->frame, &frame), "no hips");
				rotation_transform(&frame, &hinge->axis, &axis);
				vector_from_points3d(&ragdoll.positions[hinge->grandparent], &ragdoll.positions[hinge->parent], &upper);
				vector_from_points3d(&ragdoll.positions[hinge->parent], &ragdoll.positions[hinge->child], &lower);
				normalize3d(&upper);
				normalize3d(&lower);
				/* (the axis square to the thigh, as the hinge holds it) */
				scale_vector3d(&upper, dot_product3d(&axis, &upper), &bent);
				set_real_vector3d(&axis, axis.i - bent.i, axis.j - bent.j, axis.k - bent.k);
				normalize3d(&axis);
				cross_product3d(&upper, &lower, &bent);
				CHECK(dot_product3d(&bent, &axis) > -0.1f, "knee %d bent backwards (%f, tick %d)", joint,
					dot_product3d(&bent, &axis), tick_index);
				CHECK(fabs(dot_product3d(&lower, &axis)) < spaghetti_western->knee_twist + 0.1f, "knee %d twisted (%f, tick %d)",
					joint, dot_product3d(&lower, &axis), tick_index);
			}
		}
		return 0;
	}
	/* a leg thrown up swings no further than its hip lets it */
	CASE("hips_swing_so_far")
	{
		stand();
		set_real_vector3d(&ragdoll.velocities[L_CALF], 0.3f, 0.f, 0.3f);
		set_real_vector3d(&ragdoll.velocities[L_FOOT], 0.3f, 0.f, 0.3f);
		for (tick_index = 0; tick_index < TICKS_PER_SECOND / 2; tick_index++)
		{
			struct ragdoll_joint const *cone = &ragdoll.joints[2];
			struct ragdoll_rotation frame;
			real_vector3d middle, thigh;

			tick();
			CHECK(ragdoll_frame(&ragdoll, cone->frame, &frame), "no hips");
			rotation_transform(&frame, &cone->axis, &middle);
			vector_from_points3d(&ragdoll.positions[L_THIGH], &ragdoll.positions[L_CALF], &thigh);
			normalize3d(&thigh);
			CHECK(dot_product3d(&thigh, &middle) > spaghetti_western->hip_cone - 0.15f, "the hip swung to %f (tick %d)",
				dot_product3d(&thigh, &middle), tick_index);
		}
		return 0;
	}
	/* exaggerated's body (a heavy torso, dragging joints, less friction on
	the ground) falls in one piece and comes to rest too */
	CASE("exaggerated_falls")
	{
		short index;

		stand();
		weigh(_ragdoll_style_exaggerated);
		for (index = 0; index < PARTICLES; index++)
			ragdoll.velocities[index].i = 0.002f * standing[index][2] / 0.6f;
		for (tick_index = 0; tick_index < 6 * TICKS_PER_SECOND; tick_index++)
		{
			tick();
			check_body(tick_index);
		}
		CHECK(ragdoll.positions[HEAD].z < 0.2f, "the head still up at %f", ragdoll.positions[HEAD].z);
		CHECK(fastest() < RAGDOLL_SLEEP_SPEED, "still moving at %f", fastest());
		return 0;
	}
	/* exaggerated: a bone stretched between a heavy torso and a light limb
	pulls the limb most of the way */
	CASE("torso_heavier")
	{
		real_point3d torso = { 0.f, 0.f, 1.f }, limb = { 0.3f, 0.f, 1.f };

		memset(&ragdoll, 0, sizeof(ragdoll));
		ragdoll.particle_count = 2;
		ragdoll.positions[0] = torso;
		ragdoll.positions[1] = limb;
		ragdoll.inverse_masses[0] = exaggerated->inverse_masses[_ragdoll_part_torso];
		ragdoll.inverse_masses[1] = exaggerated->inverse_masses[_ragdoll_part_hand];
		ragdoll_add_constraint(&ragdoll, 0, 1, _ragdoll_constraint_length, 0.2f);
		ragdoll_solve(&ragdoll);
		CHECK(fabs(distance3d(&ragdoll.positions[0], &ragdoll.positions[1]) - 0.2f) < 0.001f, "not 0.2 apart");
		CHECK(distance3d(&torso, &ragdoll.positions[0]) < 0.5f * distance3d(&limb, &ragdoll.positions[1]),
			"the torso moved %f, the limb %f", distance3d(&torso, &ragdoll.positions[0]),
			distance3d(&limb, &ragdoll.positions[1]));
		return 0;
	}
	/* exaggerated: a hand in the chest is put back out of it, and a
	dragging joint slows a limb's swing about it */
	CASE("limbs_keep_out")
	{
		real swing;

		memset(&ragdoll, 0, sizeof(ragdoll));
		ragdoll.particle_count = 2;
		set_real_vector3d((real_vector3d *)&ragdoll.positions[0], 0.f, 0.f, 1.f);
		set_real_vector3d((real_vector3d *)&ragdoll.positions[1], 0.f, 0.01f, 1.f);
		ragdoll.inverse_masses[0] = exaggerated->inverse_masses[_ragdoll_part_torso];
		ragdoll.inverse_masses[1] = exaggerated->inverse_masses[_ragdoll_part_hand];
		ragdoll_add_constraint(&ragdoll, 1, 0, _ragdoll_constraint_minimum, RAGDOLL_TORSO_CLEARANCE);
		ragdoll_solve(&ragdoll);
		CHECK(distance3d(&ragdoll.positions[0], &ragdoll.positions[1]) > RAGDOLL_TORSO_CLEARANCE * 0.9f,
			"the hand in the chest (%f)", distance3d(&ragdoll.positions[0], &ragdoll.positions[1]));
		stand();
		ragdoll.style = _ragdoll_style_exaggerated;
		set_real_vector3d(&ragdoll.velocities[L_HAND], 0.f, 0.f, 0.1f);
		ragdoll_substep(&ragdoll);
		swing = ragdoll.velocities[L_HAND].k;
		stand();
		set_real_vector3d(&ragdoll.velocities[L_HAND], 0.f, 0.f, 0.1f);
		ragdoll_substep(&ragdoll);
		CHECK(swing < ragdoll.velocities[L_HAND].k - 0.0002f, "the hand kept %f of its swing, freely %f", swing,
			ragdoll.velocities[L_HAND].k);
		return 0;
	}
	/* the setting's words, the test build's old one for exaggerated too */
	CASE("setting_names")
	{
		char old[8];

		sprintf(old, "%s%d", "halo", 2);
		CHECK(ragdoll_named("none") == _ragdoll_style_none, "none");
		CHECK(ragdoll_named("spaghetti_western") == _ragdoll_style_spaghetti_western, "spaghetti_western");
		CHECK(ragdoll_named("exaggerated") == _ragdoll_style_exaggerated, "exaggerated");
		CHECK(ragdoll_named(old) == _ragdoll_style_exaggerated, "%s not read as exaggerated", old);
		CHECK(ragdoll_named("modern") == _ragdoll_style_modern, "modern");
		CHECK(ragdoll_named("woody") == _ragdoll_style_spaghetti_western, "the old name woody");
		CHECK(ragdoll_named("faithful") == _ragdoll_style_modern, "the old name faithful");
		CHECK(ragdoll_named("mixed") == _ragdoll_style_mixed, "mixed");
		CHECK(ragdoll_named("") == _ragdoll_style_none && ragdoll_named("ragdoll") == _ragdoll_style_none,
			"an unknown word is not none");
		CHECK(ragdoll_mode_get(_ragdoll_style_none) == spaghetti_western, "none's row is not spaghetti_western's");
		CHECK(ragdoll_mode_get(_ragdoll_style_mixed) == &ragdoll_modes[NUMBER_OF_RAGDOLL_STYLES - 2], "mixed's row");
		return 0;
	}
	/* the rows: every weight and number in range; modern's legs heavier
	than its arms, its torso the heaviest, its joints stiffer than spaghetti_western's;
	mixed modern's but for its start, its kick and its later blasts;
	exaggerated as it was */
	CASE("mode_table")
	{
		short style;
		short part;

		for (style = _ragdoll_style_spaghetti_western; style < NUMBER_OF_RAGDOLL_STYLES; style++)
		{
			struct ragdoll_mode const *mode = ragdoll_mode_get(style);

			CHECK(mode->gravity >= 1.f && mode->gravity <= 1.5f, "style %d gravity %f", style, mode->gravity);
			CHECK(mode->damping > 0.99f && mode->damping <= 1.f, "style %d damping %f", style, mode->damping);
			CHECK(mode->friction > 0.f && mode->friction <= 1.f && mode->coulomb >= 0.f && mode->coulomb <= 1.f,
				"style %d friction", style);
			CHECK(mode->restitution >= 0.f && mode->restitution < 0.5f, "style %d restitution", style);
			CHECK(mode->kill_blast > 0.f && mode->blast > 0.f && mode->big_blast > 0.f && mode->big_blast <= 1.f,
				"style %d blasts", style);
			CHECK(mode->hip_cone > -1.f && mode->hip_cone < 1.f && mode->shoulder_cone > -1.f &&
				mode->shoulder_cone < 1.f && mode->knee_bend > -1.f && mode->elbow_bend > -1.f, "style %d limits", style);
			for (part = 0; part < NUMBER_OF_RAGDOLL_PARTS; part++)
			{
				CHECK(mode->inverse_masses[part] > 0.1f && mode->inverse_masses[part] <= 3.f,
					"style %d part %d inverse mass %f", style, part, mode->inverse_masses[part]);
				CHECK(mode->inverse_masses[_ragdoll_part_torso] <= mode->inverse_masses[part],
					"style %d: part %d heavier than the torso", style, part);
			}
		}
		CHECK(modern->start == _ragdoll_start_after_moment && modern->wait_ticks == 9, "modern's start");
		CHECK(modern->gravity == 1.3f && modern->damping == 0.9995f && modern->coulomb == 0.8f, "modern's physics");
		CHECK(modern->inverse_masses[_ragdoll_part_thigh] < modern->inverse_masses[_ragdoll_part_upper_arm] &&
			modern->inverse_masses[_ragdoll_part_calf] < modern->inverse_masses[_ragdoll_part_forearm] &&
			modern->inverse_masses[_ragdoll_part_thigh] < modern->inverse_masses[_ragdoll_part_calf] &&
			modern->inverse_masses[_ragdoll_part_clavicle] == modern->inverse_masses[_ragdoll_part_torso],
			"modern's weights");
		CHECK(modern->knee_bend > spaghetti_western->knee_bend && modern->elbow_bend > spaghetti_western->elbow_bend &&
			modern->hip_cone > spaghetti_western->hip_cone && modern->shoulder_cone > spaghetti_western->shoulder_cone &&
			modern->knee_twist < spaghetti_western->knee_twist && modern->elbow_twist < spaghetti_western->elbow_twist,
			"modern's joints no stiffer than spaghetti_western's");
		CHECK(modern->kick && modern->rigid_torso && modern->weighted && modern->loosens, "modern's body");
		CHECK(mixed->start == _ragdoll_start_near_end && mixed->wait_ticks == 12 && mixed->played == 0.7f,
			"mixed's start");
		CHECK(!mixed->kick && mixed->blast == 0.75f * modern->blast && mixed->kill_blast == modern->kill_blast,
			"mixed's pushes");
		CHECK(mixed->gravity == modern->gravity && mixed->damping == modern->damping &&
			mixed->coulomb == modern->coulomb && mixed->restitution == modern->restitution &&
			mixed->joint_friction == modern->joint_friction && mixed->rigid_torso && mixed->loosens &&
			mixed->knee_bend == modern->knee_bend && mixed->hip_cone == modern->hip_cone &&
			!memcmp(mixed->inverse_masses, modern->inverse_masses, sizeof(mixed->inverse_masses)),
			"mixed's physics not modern's");
		CHECK(exaggerated->start == _ragdoll_start_at_death && exaggerated->kill_blast == 1.5f &&
			exaggerated->blast == 1.5f && exaggerated->joint_drag == 0.06f && exaggerated->friction == 0.75f &&
			exaggerated->knee_twist == 0.1f && exaggerated->shoulder_cone == -0.17f &&
			exaggerated->inverse_masses[_ragdoll_part_head] == 0.6f, "exaggerated changed");
		CHECK(spaghetti_western->start == _ragdoll_start_at_death && !spaghetti_western->weighted && spaghetti_western->gravity == 1.f, "spaghetti_western changed");
		return 0;
	}
	/* when a body goes limp */
	CASE("limp_timing")
	{
		CHECK(ragdoll_limp_now(spaghetti_western, FALSE, 0, 0.f, FALSE, FALSE), "spaghetti_western waits");
		CHECK(ragdoll_limp_now(exaggerated, FALSE, 0, 0.f, FALSE, FALSE), "exaggerated waits");
		/* modern: a moment, sooner as it lands; a blast at once */
		CHECK(!ragdoll_limp_now(modern, FALSE, 0, 0.f, FALSE, FALSE), "modern at once");
		CHECK(!ragdoll_limp_now(modern, FALSE, modern->wait_ticks - 1, 0.2f, FALSE, FALSE), "modern early");
		CHECK(ragdoll_limp_now(modern, FALSE, modern->wait_ticks, 0.2f, FALSE, FALSE), "modern after a moment");
		CHECK(ragdoll_limp_now(modern, FALSE, 2, 0.f, TRUE, FALSE), "modern landed");
		CHECK(ragdoll_limp_now(modern, TRUE, 0, 0.f, FALSE, FALSE), "modern's blast waits");
		/* mixed: most of the animation, or lying down after a moment */
		CHECK(!ragdoll_limp_now(mixed, FALSE, 20, 0.5f, FALSE, FALSE), "mixed at half its animation");
		CHECK(ragdoll_limp_now(mixed, FALSE, 20, 0.7f, FALSE, FALSE), "mixed near its animation's end");
		CHECK(!ragdoll_limp_now(mixed, FALSE, mixed->wait_ticks - 1, 0.3f, FALSE, TRUE), "mixed lying too soon");
		CHECK(ragdoll_limp_now(mixed, FALSE, mixed->wait_ticks, 0.3f, FALSE, TRUE), "mixed lying");
		CHECK(ragdoll_limp_now(mixed, FALSE, mixed->wait_ticks, 0.3f, TRUE, FALSE), "mixed landed");
		CHECK(ragdoll_limp_now(mixed, TRUE, 0, 0.f, FALSE, FALSE), "mixed's blast waits");
		CHECK(ragdoll_limp_now(mixed, FALSE, RAGDOLL_LONGEST_WAIT_TICKS, 0.f, FALSE, FALSE), "mixed waits forever");
		return 0;
	}
	/* how hard blasts push: the killing blast and later ones, less above a
	big blast's */
	CASE("blast_scales")
	{
		CHECK(ragdoll_blast_scale(exaggerated, 6.f, TRUE) == 1.5f && ragdoll_blast_scale(exaggerated, 6.f, FALSE) == 1.5f,
			"exaggerated's blasts");
		CHECK(ragdoll_blast_scale(modern, 4.f, TRUE) == 1.f, "modern's grenade %f", ragdoll_blast_scale(modern, 4.f, TRUE));
		CHECK(fabs(ragdoll_blast_scale(modern, 6.f, TRUE) - 0.7f) < 0.001f, "modern's rocket %f",
			ragdoll_blast_scale(modern, 6.f, TRUE));
		CHECK(ragdoll_blast_scale(mixed, 4.f, TRUE) == ragdoll_blast_scale(modern, 4.f, TRUE), "mixed's killing blast");
		CHECK(fabs(ragdoll_blast_scale(mixed, 4.f, FALSE) - 0.75f * ragdoll_blast_scale(modern, 4.f, FALSE)) < 0.001f &&
			fabs(ragdoll_blast_scale(mixed, 6.f, FALSE) - 0.75f * ragdoll_blast_scale(modern, 6.f, FALSE)) < 0.001f,
			"mixed's later blasts");
		return 0;
	}
	/* modern's joints start looser (two or three times, by the body) and
	tighten to their limits */
	CASE("loosens")
	{
		long object_index;
		short bend;
		real tight;

		for (object_index = 0; object_index < 4; object_index++)
		{
			real start = ragdoll_looseness(object_index, 0);

			CHECK(start >= 2.f && start <= 3.f, "body %ld starts %f loose", object_index, start);
			CHECK(ragdoll_looseness(object_index, RAGDOLL_LOOSE_TICKS / 2) < start &&
				ragdoll_looseness(object_index, RAGDOLL_LOOSE_TICKS / 2) > 1.f, "body %ld halfway", object_index);
			CHECK(ragdoll_looseness(object_index, RAGDOLL_LOOSE_TICKS) == 1.f, "body %ld never tight", object_index);
		}
		stand();
		bend = ragdoll_add_bend(&ragdoll, L_THIGH, L_CALF, L_FOOT, modern->knee_bend);
		ragdoll_link_bend(&ragdoll, &ragdoll.joints[0], bend, modern->knee_bend);
		CHECK(ragdoll.joints[0].bend == bend && bend != NONE, "no bend linked");
		tight = ragdoll.constraints[bend].length;
		ragdoll_loosen(&ragdoll, 2.f);
		CHECK(fabs(ragdoll.joints[0].limit - 2.f * ragdoll.joints[0].tight_limit) < 0.0001f, "the knee's twist %f",
			ragdoll.joints[0].limit);
		CHECK(ragdoll.joints[2].limit < ragdoll.joints[2].tight_limit - 0.1f, "the hip's cone %f, tight %f",
			ragdoll.joints[2].limit, ragdoll.joints[2].tight_limit);
		CHECK(ragdoll.constraints[bend].length < tight - 0.01f, "the knee bends no further: %f, tight %f",
			ragdoll.constraints[bend].length, tight);
		ragdoll_loosen(&ragdoll, 1.f);
		CHECK(fabs(ragdoll.joints[0].limit - ragdoll.joints[0].tight_limit) < 0.0001f &&
			fabs(ragdoll.joints[2].limit - ragdoll.joints[2].tight_limit) < 0.0001f &&
			fabs(ragdoll.constraints[bend].length - tight) < 0.0001f, "not tight again");
		return 0;
	}
	/* modern's kick turns the body about its middle (the part hit one
	way, the far side the other) and pops every part up */
	CASE("kick_turns")
	{
		real_point3d epicenter = { -0.05f, 0.f, 0.62f };
		real_vector3d direction = { 1.f, 0.f, 0.f };
		short index;

		stand();
		weigh(_ragdoll_style_modern);
		ragdoll_kick(&ragdoll, &epicenter, &direction, 0.05f, 0.03f);
		CHECK(ragdoll.velocities[HEAD].i > 0.05f, "the head %f", ragdoll.velocities[HEAD].i);
		CHECK(ragdoll.velocities[L_FOOT].i < 0.f && ragdoll.velocities[R_FOOT].i < 0.f, "the feet %f",
			ragdoll.velocities[L_FOOT].i);
		CHECK(ragdoll.velocities[L_FOOT].i > -ragdoll.velocities[HEAD].i, "the far side as hard as the part hit");
		for (index = 0; index < PARTICLES; index++)
		{
			CHECK(ragdoll.velocities[index].k > 0.02f && fabs(ragdoll.velocities[index].j) < 0.0001f,
				"particle %d not popped up (%f)", index, ragdoll.velocities[index].k);
		}
		{
			unsigned long seed = 7;
			real a = ragdoll_random(&seed, 1.5f, 3.25f);
			real b = ragdoll_random(&seed, 1.5f, 3.25f);

			CHECK(a >= 1.5f && a < 3.25f && b >= 1.5f && b < 3.25f && a != b, "random %f %f", a, b);
		}
		return 0;
	}
	/* modern's body (more gravity, little drag, dry friction on the
	ground and in its joints, a small bounce, joints that start loose)
	falls in one piece and comes to rest */
	CASE("faithful_falls")
	{
		short index;

		stand();
		weigh(_ragdoll_style_modern);
		for (index = 0; index < PARTICLES; index++)
			ragdoll.velocities[index].i = 0.002f * standing[index][2] / 0.6f;
		for (tick_index = 0; tick_index < 6 * TICKS_PER_SECOND; tick_index++)
		{
			ragdoll_loosen(&ragdoll, ragdoll_looseness(1, tick_index));
			tick();
			check_body(tick_index);
		}
		CHECK(ragdoll.positions[HEAD].z < 0.2f, "the head still up at %f", ragdoll.positions[HEAD].z);
		CHECK(fastest() < RAGDOLL_SLEEP_SPEED, "still moving at %f", fastest());
		return 0;
	}
	fprintf(stderr, "no such case: %s\n", case_name);
	return 2;
}
