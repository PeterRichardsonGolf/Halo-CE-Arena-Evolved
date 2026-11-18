/*
AE_HEAD_CHECK_RULES.H

HEAD CHECK (network.head_check, the host's): a client's reported shot that names a head material counts as a head hit
only when its point is near the head where the host has the target's head. The client decides what its bullet hit on
its own copy of the target; the host keeps that copy's position (network_damage.c's history) but not its pose, so the
test takes the head node's place on the host's target now, moved to where the host had the target when the shooter saw
it, and allows for the pose's difference. A point further off is a body hit: the report's material is swapped for the
target's body material, and the shot still counts. These pure functions give the test and the body material
(port/linux/tests/ae_head_check_rules_test.c); ae_head_check.c applies them.
*/

#ifndef AE_HEAD_CHECK_RULES_H
#define AE_HEAD_CHECK_RULES_H

struct ae_head_check_point
{
	float x, y, z;
};

enum
{
	/* how far a head hit's point may be from the head node's centre, world units x100 (the head's own size, about
	0.05, and room for a pose the host draws a little differently: a torso hit lies further down) */
	AE_HEAD_CHECK_REACH_100 = 12,
};

/* whether the reported point is on the head: hit is the report's point, seen where the host had the target's origin
when the shooter saw it, head_offset the head node's place on the target now less the target's origin now */
int ae_head_check_on_head(struct ae_head_check_point const *hit, struct ae_head_check_point const *seen,
	struct ae_head_check_point const *head_offset);

/* the same, with the head anywhere from below under to above over that place (the pose the host cannot know: a target
that crouched or stood up between the shot and its report had its head that much lower or higher when the shot hit):
the point's distance to that upright span, against the reach. Negative or not-a-number spans count as 0 */
int ae_head_check_on_head_span(struct ae_head_check_point const *hit, struct ae_head_check_point const *seen,
	struct ae_head_check_point const *head_offset, float below, float above);

/* the material a head hit off the head becomes: of the target's count materials (head[i] nonzero for a head material,
type[i] its material type), the first that is not a head material and is of the head material's type, else the
indirect damage material (indirect, or -1 for none) if it is not a head material, else the first that is not a head
material; -1 when every material is a head material (the hit then stays as reported) */
int ae_head_check_body_material(int count, int const *head, int const *type, int head_material, int indirect);

#endif
