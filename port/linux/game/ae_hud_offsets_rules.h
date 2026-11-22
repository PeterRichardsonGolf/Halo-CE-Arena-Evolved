/*
AE_HUD_OFFSETS_RULES.H

The weapon HUD element mover's numbers and math (ae_hud_offsets.h): for a weapon (by its key, as the first-person
placement keys it: a pack weapon's name, else its tag's last name) and one of its HUD tag's elements (the element's number:
the statics, meters, numbers and overlays of the weapon's own HUD tag counted on in that order), a move in HUD units (the
tag's placement units: 640 x 480), a scale and a hide. The table is the player's file (hud_offsets.txt in the config folder),
one element a line: key, element, dx, dy, scale, hide ("sniper_bolt 4 -12 6 1 0", "#" starts a comment). An element the table
does not name is drawn as the tag has it. No engine includes: port/linux/tests/ae_hud_offsets_rules_test.c compiles these
alone.
*/

#ifndef __AE_HUD_OFFSETS_RULES_H
#define __AE_HUD_OFFSETS_RULES_H

#define AE_HUD_KEY 48
#define AE_HUD_ELEMENTS 32
#define AE_HUD_WEAPONS 32
/* what DEV MODE accepts (HUD units, and a multiple of the tag's own scale) */
#define AE_HUD_OFFSET_MAXIMUM 600.0f
#define AE_HUD_SCALE_MINIMUM 0.1f
#define AE_HUD_SCALE_MAXIMUM 4.0f
/* what a hidden element's offset is moved by (HUD units: well off any screen, and well inside a short) */
#define AE_HUD_HIDE_OFFSET 6000

struct ae_hud_element
{
	int index;
	float dx, dy, scale;
	int hide;
};

struct ae_hud_weapon
{
	char key[AE_HUD_KEY];
	int count;
	struct ae_hud_element element[AE_HUD_ELEMENTS];
};

struct ae_hud_table
{
	int count;
	struct ae_hud_weapon weapon[AE_HUD_WEAPONS];
};

/* {index, 0, 0, 1, shown} */
struct ae_hud_element ae_hud_element_identity(int index);
int ae_hud_element_is_identity(struct ae_hud_element const *element);
/* the index in range and the numbers finite and within the limits above */
int ae_hud_element_valid(struct ae_hud_element const *element);

/* a placement's offset and scale as the element's numbers move them. corner is the HUD's anchor (0 top left, 1 top right, 2
bottom left, 3 bottom right, anything else the centre or another anchor): a move of dx is to the screen's right whichever
corner the element hangs from. A hidden element's offset goes far off the screen */
void ae_hud_element_apply(struct ae_hud_element const *element, int corner, short *offset_x, short *offset_y,
	float *scale_x, float *scale_y);

void ae_hud_table_clear_all(struct ae_hud_table *table);
/* a weapon's element (key without case): 1 and the numbers, or 0 (and the identity) when the table has none */
int ae_hud_table_get(struct ae_hud_table const *table, char const *key, int index, struct ae_hud_element *out);
/* sets an element's numbers: 1, or 0 for an empty or long key, invalid numbers or a full table; the identity drops it */
int ae_hud_table_set(struct ae_hud_table *table, char const *key, struct ae_hud_element const *element);
/* an element of the weapon dropped (index -1: every element of it); the number dropped */
int ae_hud_table_clear(struct ae_hud_table *table, char const *key, int index);
/* a file's text over the table: how many lines it set (a line that is not one is skipped) */
int ae_hud_table_parse(struct ae_hud_table *table, char const *text);
/* the table as a file's text; 0 when it does not fit */
int ae_hud_table_format(struct ae_hud_table const *table, char *text, int size);

#endif
