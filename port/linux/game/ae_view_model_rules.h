/*
AE_VIEW_MODEL_RULES.H

The first-person weapon's own placement: an offset along the gun's axes (forward, left, up, in model units: a hundredth
of a world unit, as a model's source files count them) and a uniform scale, both about the gun's root node. A weapon's
numbers are found by its key (a pack weapon's name, "ar_modern"; another weapon's tag name, "assault rifle"); a weapon
the table does not name is drawn as its model is (the identity). The table is the shipped defaults with the player's
own file over them; its file is one weapon a line: key, offset x y z, scale ("ar_modern 1.4755 0 0.2957 0.78", "#"
starts a comment), and optionally seven more numbers for the arms and gun as one piece: right up forward, yaw pitch
roll, scale ("ar_modern 1.4755 0 0.2957 0.78 0 -0.5 1 0 2 0 1"). The weapon cache keys weapons by a global id: the key here is the weapon's name, which maps to that
id one to one. No engine includes: the unit test in port/linux/tests/ae_view_model_rules_test.c compiles these alone.
*/

#ifndef __AE_VIEW_MODEL_RULES_H
#define __AE_VIEW_MODEL_RULES_H

#define AE_VIEW_MODEL_KEY 48
#define AE_VIEW_MODEL_ENTRIES 64
/* the most an offset may be (model units) and the scale's range: DEV MODE refuses a number outside them */
#define AE_VIEW_MODEL_OFFSET_MAXIMUM 40.0f
#define AE_VIEW_MODEL_SCALE_MINIMUM 0.2f
#define AE_VIEW_MODEL_SCALE_MAXIMUM 3.0f
/* a model unit in world units */
#define AE_VIEW_MODEL_UNIT 0.01f

/* the whole first-person view's own placement (arms and gun as one piece) limits: a move up to this (model units)
either way, a turn up to this (degrees) either way */
#define AE_VIEW_MODEL_PLACE_MAXIMUM 60.0f
#define AE_VIEW_MODEL_TURN_MAXIMUM 45.0f

struct ae_view_model
{
	/* the gun alone, about its own root node, along its own axes (forward, left, up), and its scale */
	float offset[3];
	float scale;
	/* the arms and gun as one piece, in the camera's frame, about the gun's root node: moved right, up and forward
	(model units), turned by yaw (the muzzle to the left), pitch (up) and roll (its top to the right) in degrees, and
	scaled; a place_scale of 0 is 1 (a table entry that names none) */
	float place[3];
	float turn[3];
	float place_scale;
};

/* the placement of the arms and gun as one piece, worked out for a frame: a node at p goes to scale * rotation * p +
translation, its axes are turned by rotation and its scale multiplied by scale (all in world space) */
struct ae_view_model_frame
{
	float rotation[3][3];	/* rows: rotation[r][c] */
	float translation[3];
	float scale;
};

struct ae_view_model_entry
{
	char key[AE_VIEW_MODEL_KEY];
	struct ae_view_model value;
};

struct ae_view_model_table
{
	int count;
	struct ae_view_model_entry entry[AE_VIEW_MODEL_ENTRIES];
};

/* {0, 0, 0}, 1 */
struct ae_view_model ae_view_model_identity(void);
int ae_view_model_is_identity(struct ae_view_model const *model);
/* whether the numbers are finite and within the range above */
int ae_view_model_valid(struct ae_view_model const *model);

/* a node of the weapon moved as the model is: root is the gun's root node (its position, its forward/left/up unit
vectors and its scale), position and scale a node's (the root's own included). The node's orientation does not
change: the position goes to root + s * (position - root) + root scale * (forward, left, up) * offset * unit, the
scale to s * scale */
void ae_view_model_node(struct ae_view_model const *model, float const root_position[3],
	float const root_axes[3][3], float root_scale, float position[3], float *scale);

/* whether the arms and gun are placed as one piece (a move, a turn or a scale other than 1) */
int ae_view_model_is_placed(struct ae_view_model const *model);
/* the frame for the arms and gun as one piece: the camera's forward and up (world), the pivot (the gun's root node,
world); 0 (and the identity) when the model places nothing */
int ae_view_model_place_frame(struct ae_view_model const *model, float const camera_forward[3],
	float const camera_up[3], float const pivot[3], struct ae_view_model_frame *frame);
/* a node moved by a frame: its position, its three axes (forward, left, up) and its scale */
void ae_view_model_place_node(struct ae_view_model_frame const *frame, float position[3], float axes[3][3],
	float *scale);

/* the table: the shipped defaults only */
void ae_view_model_table_defaults(struct ae_view_model_table *table);
/* a weapon's numbers (case aside): 1 and the numbers, or 0 (and the identity) when the table has none */
int ae_view_model_table_get(struct ae_view_model_table const *table, char const *key, struct ae_view_model *out);
/* sets (or adds) a weapon's numbers: 1, or 0 when the key is empty or too long, the numbers are not valid or the table is full */
int ae_view_model_table_set(struct ae_view_model_table *table, char const *key, struct ae_view_model const *model);
/* the weapon's own numbers dropped; 1 when it had some */
int ae_view_model_table_clear(struct ae_view_model_table *table, char const *key);
/* a file's text over the table: how many lines it set (a line that is not one is skipped) */
int ae_view_model_table_parse(struct ae_view_model_table *table, char const *text);
/* the table as a file's text; 0 when it does not fit */
int ae_view_model_table_format(struct ae_view_model_table const *table, char *text, int size);

#endif
