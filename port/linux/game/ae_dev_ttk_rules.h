/*
AE_DEV_TTK_RULES.H

DEV MODE's time-to-kill readout (ae_dev_ttk.h): who has hurt whom in the victim's present life, as pure rules (no
engine includes: port/linux/tests/ae_dev_ttk_rules_test.c compiles these alone).

A table keeps, for each victim, the life it is counting for (the victim's unit) and for each killer the tick of the
killer's first damage to that victim, the damage events that landed and how many of them were headshots. A killing blow
reads its killer's entry and clears the victim's row. A row counted for another life (the victim's unit changed: a
death with no damage event to clear it, as the kill command's) starts over.
*/

#ifndef __AE_DEV_TTK_RULES_H
#define __AE_DEV_TTK_RULES_H

#define AE_TTK_PLAYERS 32
#define AE_TTK_TICKS_PER_SECOND 30

struct ae_ttk_hit
{
	long first_tick;
	short hits;
	short head;
};

struct ae_ttk_row
{
	/* the victim's unit the hits are for, -1 for none */
	long life;
	struct ae_ttk_hit killer[AE_TTK_PLAYERS];
};

struct ae_ttk_table
{
	struct ae_ttk_row victim[AE_TTK_PLAYERS];
};

struct ae_ttk_result
{
	/* the ticks from the killer's first damage to the kill, the damage events, the headshots among them */
	long ticks;
	int hits;
	int head;
};

void ae_ttk_clear(struct ae_ttk_table *table);
/* the victim's row emptied (a player out of the game) */
void ae_ttk_clear_victim(struct ae_ttk_table *table, int victim);
/* damage the killer did to the victim's unit `life` at `tick`: 1 when counted, 0 for a slot out of range or a hit on
oneself */
int ae_ttk_hit(struct ae_ttk_table *table, int killer, int victim, long life, long tick, int head);
/* the victim's unit `life` died at `tick`: 1 and the killer's result when the killer had hurt it in this life; the
row is cleared either way (killer -1: a death with no killer) */
int ae_ttk_kill(struct ae_ttk_table *table, int killer, int victim, long life, long tick, struct ae_ttk_result *out);
/* "ttk: <killer> -> <victim> <weapon> <seconds>s, <hits> hits (<head> head)" */
void ae_ttk_format(char *text, int size, char const *killer, char const *victim, char const *weapon,
	struct ae_ttk_result const *result);
/* the short line the killer's own HUD message list shows: "TTK <seconds>s - <hits> hits (<head> head) - <weapon>" ("1 hit"
for one; the weapon cut to 16 characters; the head count left out when 0) */
void ae_ttk_short_format(char *text, int size, char const *weapon, struct ae_ttk_result const *result);

#endif
