/*
AE_OBJECTIVE_LOG_RULES.H

The objective log's format as pure rules (ae_objective_log.c writes the lines, tools/ae_test/obj_report.py reads them):
the levels of debug.objective_log, the mode words, every event's key list (the one table, in the .c, that the unit test
pins and obj_report.py --check reads), and the builder of "ae obj:" lines.

A line:  ae obj: t=<tick> host=<1|0> mode=<word> ev=<event> <the event's keys in the table's order>
Values have no spaces (a space is "_"), a real is "%.1f", a position is x= y= z=, a player is its absolute slot index
(never a name), -1 is none. Unit-tested (port/linux/tests/ae_objective_log_rules_test.c).
*/
#ifndef AE_OBJECTIVE_LOG_RULES_H
#define AE_OBJECTIVE_LOG_RULES_H

#include <stddef.h>

/* debug.objective_log */
enum ae_obj_level
{
	AE_OBJ_OFF = 0,
	/* the engine's events, job changes and blocked objectives */
	AE_OBJ_EVENTS,
	/* + the one-a-second state line (ev=snap) */
	AE_OBJ_SNAP,
	/* + every player's line a second (ev=ps) */
	AE_OBJ_FULL
};

/* the gametype family as the log names it */
enum ae_obj_mode
{
	AE_OBJ_MODE_SLAYER = 0,
	AE_OBJ_MODE_CTF,
	AE_OBJ_MODE_CTF1,
	AE_OBJ_MODE_ASSAULT,
	AE_OBJ_MODE_ODDBALL,
	AE_OBJ_MODE_TAG,
	AE_OBJ_MODE_JUGG,
	AE_OBJ_MODE_KOTH,
	AE_OBJ_MODE_TERR,
	AE_OBJ_MODE_RACE,
	AE_OBJ_MODE_BOMB,
	AE_OBJ_MODE_SND,
	AE_OBJ_MODE_ELIM,
	AE_OBJ_MODE_EXTERM,
	AE_OBJ_MODE_ATTRIT,
	AE_OBJ_MODES
};

/* the setting's text ("", off, events, snap, full; anything else is off); debug.bot_events on implies full */
int ae_obj_rules_level(char const *text, int bot_events);
char const *ae_obj_rules_mode_name(int mode);

/* the events and their keys (after t host mode ev): NULL for an event that is not in the table */
char const *ae_obj_rules_event_keys(char const *ev);
int ae_obj_rules_event_count(void);
char const *ae_obj_rules_event_name(int index);
/* the words the bot-event line of a stuck bot gets appended (ev=stuck; its existing keys unchanged) */
char const *ae_obj_rules_stuck_keys(void);

/* the bomb's own log line of a send home ("reset: out of bounds at (x y z)"): 1 with its reason word (out_of_bounds |
out_of_reach | untouched) and where, 0 for any other line */
int ae_obj_rules_bomb_reset(char const *message, char *why, size_t size, double *x, double *y, double *z);
/* whether a picked up item (its tag's last name) is a pickup the log names: not a flag, a ball or the bomb */
int ae_obj_rules_item_logged(char const *name);
/* the seat's word: 0 driver, 1 gunner, else passenger */
char const *ae_obj_rules_seat_name(int seat);

/* a field builder: add keys in the event's order; a value's spaces become "_"; reals are "%.1f"; out of room is
dropped whole (the line ends at a whole key) */
struct ae_obj_fields
{
	char text[640];
	size_t length;
};

void ae_obj_fields_begin(struct ae_obj_fields *fields);
void ae_obj_fields_long(struct ae_obj_fields *fields, char const *key, long value);
void ae_obj_fields_real(struct ae_obj_fields *fields, char const *key, double value);
void ae_obj_fields_text(struct ae_obj_fields *fields, char const *key, char const *value);
/* x= y= z= */
void ae_obj_fields_point(struct ae_obj_fields *fields, double x, double y, double z);

/* the whole line; returns its length */
size_t ae_obj_rules_line(char *out, size_t size, long tick, int host, int mode, char const *ev,
	struct ae_obj_fields const *fields);
/* whether a line's keys after "ev=<event>" are exactly the table's for that event, in order (the check of
obj_report.py --check, in C for the unit test); 0 for an unknown event */
int ae_obj_rules_line_conforms(char const *line);

#endif
