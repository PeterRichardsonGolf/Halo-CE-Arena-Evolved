/*
AE_BOTS_SEAT.C

Per-bot SHOOTING and MOVEMENT skill: the seats' table (ae_bots_seat.h; the rules: ae_bots_seat_rules.c). bots.c reads it
through ae_bots_seat.inc while it updates a bot.
*/

#include "cseries.h"

#include <stdio.h>
#include <string.h>

#include "ae_bots_seat.h"
#include "ae_bots_seat_rules.h"
#include "ae_bots_style_rules.h"

void platform_log(char const *format, ...);

static struct ae_seat_table seats;
static struct
{
	unsigned char set[AE_SEAT_MOST];
	struct ae_bots_style style[AE_SEAT_MOST];
} styles;

static struct
{
	int count;
	char name[AE_SEAT_MOST][AE_BOTS_SEAT_NAME];
} names;

boolean ae_bots_seat_skill_set(long seat, int shooting_step, int movement_step)
{
	if (seat < 0 || seat >= AE_SEAT_MOST || !ae_seat_set(&seats, (int)seat, shooting_step, movement_step))
		return FALSE;
	platform_log("bots: skill: seat %ld (bot %ld): shooting %s, movement %s", seat, seat + 1,
		ae_seat_step_name(shooting_step), ae_seat_step_name(movement_step));
	return TRUE;
}

void ae_bots_seat_skill_unset(long seat)
{
	if (seat < 0 || seat >= AE_SEAT_MOST)
		return;
	ae_seat_unset(&seats, (int)seat);
}

void ae_bots_seat_skill_clear(void)
{
	if (ae_seat_count(&seats))
		platform_log("bots: skill: every seat plays the settings again");
	ae_seat_clear(&seats);
}

boolean ae_bots_seat_skill_levels(long seat, int *shooting_level, int *movement_level)
{
	if (seat < 0 || seat >= AE_SEAT_MOST)
		return FALSE;
	return ae_seat_levels(&seats, (int)seat, shooting_level, movement_level) ? TRUE : FALSE;
}

int ae_bots_seat_skill_count(void)
{
	return ae_seat_count(&seats);
}

boolean ae_bots_seat_style_set(long seat, struct ae_bots_style const *style)
{
	struct ae_bots_style checked;

	if (seat < 0 || seat >= AE_SEAT_MOST || !style || !ae_bots_style_preset(style->preset, style->weapon, &checked))
		return FALSE;
	/* (the traits as given: a preset's, or the setter's own steps) */
	styles.style[seat] = *style;
	styles.set[seat] = 1;
	platform_log("bots: style: seat %ld (bot %ld): %s (aggression %+d jumpiness %+d patience %+d), favourite %s", seat,
		seat + 1, ae_bots_style_preset_name(style->preset), style->aggression, style->jumpiness, style->patience,
		ae_bots_style_weapon_name(style->weapon));
	return TRUE;
}

void ae_bots_seat_style_unset(long seat)
{
	if (seat >= 0 && seat < AE_SEAT_MOST)
		styles.set[seat] = 0;
}

void ae_bots_seat_style_clear(void)
{
	long seat;
	int count = 0;

	for (seat = 0; seat < AE_SEAT_MOST; seat++)
		count += styles.set[seat];
	if (count)
		platform_log("bots: style: every seat plays without a style again");
	csmemset(&styles, 0, sizeof(styles));
}

boolean ae_bots_seat_style_get(long seat, struct ae_bots_style *style)
{
	if (seat < 0 || seat >= AE_SEAT_MOST || !styles.set[seat])
		return FALSE;
	*style = styles.style[seat];
	return TRUE;
}

boolean ae_bots_seat_name_set(long seat, char const *name)
{
	if (seat < 0 || seat >= AE_SEAT_MOST || !name || !name[0])
		return FALSE;
	if (!names.name[seat][0])
		names.count++;
	snprintf(names.name[seat], sizeof(names.name[seat]), "%s", name);
	platform_log("bots: name: seat %ld (bot %ld): %s", seat, seat + 1, names.name[seat]);
	return TRUE;
}

void ae_bots_seat_name_clear(void)
{
	if (names.count)
		platform_log("bots: name: every seat takes the usual names again");
	memset(&names, 0, sizeof(names));
}

boolean ae_bots_seat_name_get(long seat, char *name, int size)
{
	if (seat < 0 || seat >= AE_SEAT_MOST || !names.name[seat][0] || size <= 0)
		return FALSE;
	snprintf(name, (size_t)size, "%s", names.name[seat]);
	return TRUE;
}
