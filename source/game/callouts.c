/*
CALLOUTS.C

port: CALLOUTS (game.callouts, Settings > Game Options): spoken calls of the
power items' spawns and of the match clock in a multiplayer game, in this
machine's voice pack (game.callout_voice: port/linux/src/callout_voice.c),
as Halo 1: NHE's maps' voice timer says them. From game time alone, which
every machine has as the host's, so nothing is networked: each machine says
its own.

ITEMS, in any gametype: each power item's call (rockets, sniper, overshield,
camo) 10 seconds before each of its spawns after the game's start, from its
real period (item_timers.c): "<item> in ten" when the pack has it, else the
name, after the pack's item beep; an item at both bases (RED / BLUE before
its name, item_timers.c) its side's "red <item>" / "blue <item>" when the
pack has both, each side's said. "five" .. "one" in the last 5 seconds
before the rockets' spawn, and at each spawn "<item> is up" when the pack
has it.

Items spawning together are called together when the pack has the line,
before the separate calls: an overshield and a camo, "overshield and camo
in ten" and "overshield and camo are up"; three items or more, "powerups
in ten", then each one's "is up" (the line names none of them, so the
spawn does), the overshield and camo still as one. An item called with its
side keeps its own call before the spawn ("is up" has no side). A spawn
point of overshield or camo at random (Blood Gulch's OS/CAMO) is
"overshield or camo in ten", and at the spawn the item it spawned
(item_timers.c sees it on the map) is up, waited for up to a second, else
not called. A pack without a line makes the calls it has instead.

ITEMS + CLOCK adds NHE's talking timer (its nhe_combined_timer script),
which counts each minute of game time: a beep on the minute and "N minutes"
half a second later (1 .. 30, then 1 again), beeps at :20, :30 and :40 with
"thirty seconds left" and "twenty seconds" half a second after the last two,
"ten" at :50 and "nine" .. "one" at :51 .. :59 (no beep at :10 or :50, as
NHE's). Each beep is the pack's beep for its moment (beep_minute, beep_tick,
beep_item), else its beep.

One call at a time, planned ahead from the clips' lengths: the counts (the
numbers and the clock's beeps) and the clock's words keep their moment;
an item's call that would meet one moves earlier, as NHE's are staggered
before :50, with the items spawning together back to back (else in order
around the calls between them, else said together as "powerups in ten" or
without their sides); an "is up" waits for the clock's words before it. The same word due at the same tick
is said once (the rockets' five to one before a minute are the clock's).
A call that is late anyway (the mixer, a slow machine) is dropped rather
than said late: a count over a second late or overtaken by a later count,
a clock call by a later clock call. Nothing on Halo 1: NHE's maps (their
scripts talk), during the PRE-GAME COUNTDOWN, while the game is paused or
once it is over; the clip playing then stops. Each call said or dropped is
logged with its game tick.
*/

/* ---------- headers */

#include "cseries/cseries.h"

#include "game/callouts.h"
#include "game/game.h"
#include "game/game_engine.h"
#include "game/item_timers.h"
#include "hs/hs.h"

/* ---------- constants */

enum callout_mode
{
	_callouts_off = 0,
	_callouts_items,
	_callouts_items_and_clock
};

/* the voice pack's clips (voices/<pack>/<name>.wav) */
enum callout_clip
{
	_callout_one = 0,	/* .. _callout_one + 9: "ten" */
	_callout_ten = _callout_one + 9,
	_callout_twenty_seconds,
	_callout_thirty_seconds_left,
	_callout_first_minute,	/* .. + 29: "thirty minutes" */
	_callout_rockets = _callout_first_minute + 30,	/* the names, as enum item_timer_class */
	_callout_sniper,
	_callout_overshield,
	_callout_camo,
	_callout_beep,
	_callout_beep_minute,
	_callout_beep_tick,
	_callout_beep_item,
	_callout_rockets_in_ten,	/* "<item> in ten", as enum item_timer_class */
	_callout_sniper_in_ten,
	_callout_overshield_in_ten,
	_callout_camo_in_ten,
	_callout_rockets_up,	/* "<item> is up", as enum item_timer_class */
	_callout_sniper_up,
	_callout_overshield_up,
	_callout_camo_up,
	_callout_red_rockets,	/* "red <item>", as enum item_timer_class */
	_callout_red_sniper,
	_callout_red_overshield,
	_callout_red_camo,
	_callout_blue_rockets,	/* "blue <item>", as enum item_timer_class */
	_callout_blue_sniper,
	_callout_blue_overshield,
	_callout_blue_camo,
	_callout_overshield_camo_in_ten,	/* the two spawning together */
	_callout_overshield_camo_up,
	_callout_powerups_in_ten,	/* three items or more spawning together */
	_callout_overshield_or_camo_in_ten,	/* a spawn point of either at random */
	NUMBER_OF_CALLOUT_CLIPS
};

/* how a call is planned and when it is dropped */
enum callout_kind
{
	_callout_kind_count = 0,	/* a number or the clock's beep: at its moment */
	_callout_kind_clock,	/* the clock's words: at their moment */
	_callout_kind_item_beep,	/* the beep before items' calls */
	_callout_kind_item,	/* "<item> in ten": its moment or earlier */
	_callout_kind_item_up,	/* "<item> is up": its moment or later */
	_callout_kind_item_up_group	/* a spawn's "is up" calls, made as it is due
		(callouts_say_up_group) */
};

#define MAXIMUM_PLANNED_CALLOUTS 64
/* the items' calls due together: the item beep, a combined call, of each
class its red, blue and plain, and a mixed entry's */
#define MAXIMUM_SPAWNING_CALLOUTS (3 * NUMBER_OF_ITEM_TIMER_POWER_CLASSES + 3)
/* the mixed entries (OS/CAMO) spawning together whose item is called */
#define MAXIMUM_MIXED_CALLOUTS 4
/* the longest an "is up" waits for a mixed entry's item to be seen */
#define CALLOUT_MIXED_WAIT_TICKS TICKS_PER_SECOND
/* the power classes (bits of enum item_timer_class) */
#define CALLOUT_POWER_CLASSES ((unsigned short)(FLAG(NUMBER_OF_ITEM_TIMER_POWER_CLASSES) - 1))
#define CALLOUT_OVERSHIELD_CAMO ((unsigned short)(FLAG(_item_timer_overshield) | FLAG(_item_timer_camo)))
/* an item's call this long before its spawn; the rockets' count from five */
#define CALLOUT_ITEM_WARNING_TICKS (10 * TICKS_PER_SECOND)
#define CALLOUT_ITEM_COUNT_TICKS (5 * TICKS_PER_SECOND)
/* the clock's words this long after their beep, as NHE's */
#define CALLOUT_CLOCK_SPEECH_TICKS (TICKS_PER_SECOND / 2)
/* the latest a call is said after its planned tick, else it is dropped */
#define CALLOUT_LATEST_ITEM_TICKS (5 * TICKS_PER_SECOND)
#define CALLOUT_LATEST_CLOCK_TICKS (3 * TICKS_PER_SECOND)
#define CALLOUT_LATEST_COUNT_TICKS TICKS_PER_SECOND
/* a jump in game time longer than this (a machine joining, a new game) is
not caught up on */
#define CALLOUT_CATCH_UP_TICKS (2 * TICKS_PER_SECOND)
/* the plan: the calls with a moment this far ahead are planned; an item's
call this much later, when every call with a moment up to that far after
its own is known (no group of calls is this long) */
#define CALLOUT_PLAN_AHEAD_TICKS (30 * TICKS_PER_SECOND)
#define CALLOUT_PLACE_AFTER_TICKS (10 * TICKS_PER_SECOND)
/* the most an item's call moves earlier (NHE's: up to 6 s) */
#define CALLOUT_MOVE_EARLIER_TICKS (15 * TICKS_PER_SECOND)
/* the silence after each clip, which also covers the mixer's buffer */
#define CALLOUT_GAP_TICKS 3

/* ---------- structures */

struct callout
{
	long due;	/* its moment */
	long start;	/* the tick it is planned for */
	long end;	/* start + its length + CALLOUT_GAP_TICKS */
	long spawn;	/* the item's spawn, NONE for the clock's */
	short clip;	/* NONE for an "is up" group */
	short item_index;	/* the item timer's (item_timers_get), NONE for the clock's */
	short kind;	/* enum callout_kind */
	unsigned short covers;	/* a combined call's classes (bits of enum item_timer_class), else 0 */
	/* an "is up" group's: the classes known to spawn and an entry of each
	(NONE), and the mixed entries whose item is seen at the spawn */
	unsigned short up_classes;
	short class_items[NUMBER_OF_ITEM_TIMER_POWER_CLASSES];
	short mixed_count;
	short mixed_items[MAXIMUM_MIXED_CALLOUTS];
};

/* ---------- prototypes */

void platform_log(char const *format, ...);
char const *config_string(char const *name);
unsigned long config_changes(void);

/* the voice pack (port/linux/src/callout_voice.c) */
int platform_callout_voice_load(char const *pack, char const *const *names, int count);
void platform_callout_voice_unload(void);
int platform_callout_voice_play(int clip);
long platform_callout_voice_milliseconds(int clip);
void platform_callout_voice_stop(void);
int platform_callout_voice_busy(void);

/* ---------- globals */

static char const *const callout_clip_names[NUMBER_OF_CALLOUT_CLIPS] =
{
	"one", "two", "three", "four", "five", "six", "seven", "eight", "nine", "ten",
	"twenty_seconds",
	"thirty_seconds_left",
	"one_minute", "two_minutes", "three_minutes", "four_minutes", "five_minutes",
	"six_minutes", "seven_minutes", "eight_minutes", "nine_minutes", "ten_minutes",
	"eleven_minutes", "twelve_minutes", "thirteen_minutes", "fourteen_minutes", "fifteen_minutes",
	"sixteen_minutes", "seventeen_minutes", "eighteen_minutes", "nineteen_minutes", "twenty_minutes",
	"twenty_one_minutes", "twenty_two_minutes", "twenty_three_minutes", "twenty_four_minutes",
	"twenty_five_minutes", "twenty_six_minutes", "twenty_seven_minutes", "twenty_eight_minutes",
	"twenty_nine_minutes", "thirty_minutes",
	"rockets", "sniper", "overshield", "camo",
	"beep", "beep_minute", "beep_tick", "beep_item",
	"rockets_in_ten", "sniper_in_ten", "overshield_in_ten", "camo_in_ten",
	"rockets_up", "sniper_up", "overshield_up", "camo_up",
	"red_rockets", "red_sniper", "red_overshield", "red_camo",
	"blue_rockets", "blue_sniper", "blue_overshield", "blue_camo",
	"overshield_camo_in_ten", "overshield_camo_up", "powerups_in_ten", "overshield_or_camo_in_ten",
};

/* the power classes' clips (enum item_timer_class; OS/CAMO's is the
overshield's) */
static short const callout_item_clips[NUMBER_OF_ITEM_TIMER_POWER_CLASSES] =
{
	_callout_rockets,
	_callout_sniper,
	_callout_overshield,
	_callout_camo,
};

/* the power classes' names in the log (enum item_timer_class) */
static char const *const callout_class_names[NUMBER_OF_ITEM_TIMER_POWER_CLASSES] =
{
	"ROCKETS",
	"SNIPER",
	"OVERSHIELD",
	"CAMO",
};

/* the order of the items' calls due together, NHE's (rocket, camo,
overshield, sniper before a minute) */
static short const callout_item_order[NUMBER_OF_ITEM_TIMER_POWER_CLASSES] =
{
	_item_timer_rockets,
	_item_timer_camo,
	_item_timer_overshield,
	_item_timer_sniper,
};

static struct
{
	short mode;	/* enum callout_mode */
	unsigned long settings_read_at;
	char voice[64];	/* game.callout_voice as read */
	long planned_to;	/* the last tick whose fixed calls are planned, NONE for none */
	long plan_began;	/* the tick the plan began at: no item call due before it */
	long last_tick;	/* the last update's */
	boolean playing;	/* a clip was started since the last flush */
	short planned_count;
	struct callout plan[MAXIMUM_PLANNED_CALLOUTS];	/* by start */
} callout_globals = { _callouts_off, (unsigned long)-1, "", NONE, NONE, NONE, FALSE, 0 };

/* ---------- private code */

/* a count marks a moment: a number or the clock's beep */
static boolean callout_is_count(
	struct callout const *callout)
{
	return callout->kind == _callout_kind_count;
}

/* a clip's length in ticks, 0 when the pack has none */
static long callout_clip_ticks(
	short clip)
{
	long milliseconds = clip != NONE ? platform_callout_voice_milliseconds(clip) : 0;

	if (milliseconds <= 0)
		return 0;
	return (milliseconds * TICKS_PER_SECOND + 999) / 1000;
}

/* a moment's beep (beep_minute, beep_tick, beep_item), else the pack's
beep; NONE for neither */
static short callout_beep(
	short clip)
{
	if (callout_clip_ticks(clip) > 0)
		return clip;
	if (callout_clip_ticks(_callout_beep) > 0)
		return _callout_beep;
	return NONE;
}

/* a call due at due (start there), of the item spawning at spawn (NONE
for the clock's) */
static void callout_new(
	struct callout *callout,
	long due,
	long spawn,
	short clip,
	short item_index,
	short kind)
{
	short index;

	csmemset(callout, 0, sizeof(*callout));
	callout->due = due;
	callout->start = due;
	callout->end = due;
	callout->spawn = spawn;
	callout->clip = clip;
	callout->item_index = item_index;
	callout->kind = kind;
	for (index = 0; index < NUMBER_OF_ITEM_TIMER_POWER_CLASSES; index++)
		callout->class_items[index] = NONE;
}

static void callouts_flush(
	void)
{
	callout_globals.planned_count = 0;
	callout_globals.planned_to = NONE;
	callout_globals.last_tick = NONE;
	/* (the clip playing stops: the game is over or paused, or CALLOUTS
	changed) */
	if (callout_globals.playing)
	{
		platform_callout_voice_stop();
		callout_globals.playing = FALSE;
	}
}

/* CALLOUTS (game.callouts: "off", "items", "items_clock"), read again when
Settings changes it; and the voice pack read when it is on and its name
changed (callout_voice.c reads it again only then), its clips freed when it
is turned off. A change of either plans again */
static short callouts_mode(
	boolean load_voice)
{
	if (callout_globals.settings_read_at != config_changes())
	{
		char const *value = config_string("game.callouts");
		char const *voice = config_string("game.callout_voice");
		short mode;

		callout_globals.settings_read_at = config_changes();
		if (value && !csstrcmp(value, "items"))
			mode = _callouts_items;
		else if (value && !csstrcmp(value, "items_clock"))
			mode = _callouts_items_and_clock;
		else
			mode = _callouts_off;
		if (!voice)
			voice = "";
		if (mode != callout_globals.mode || csstrcmp(voice, callout_globals.voice))
		{
			callouts_flush();
			if (mode == _callouts_off)
				platform_callout_voice_unload();
		}
		callout_globals.mode = mode;
		csstrncpy(callout_globals.voice, voice, sizeof(callout_globals.voice) - 1);
		callout_globals.voice[sizeof(callout_globals.voice) - 1] = 0;
		load_voice = TRUE;
	}
	if (load_voice && callout_globals.mode != _callouts_off)
	{
		platform_callout_voice_load(callout_globals.voice, callout_clip_names, NUMBER_OF_CALLOUT_CLIPS);
	}

	return callout_globals.mode;
}

/* a clip's name, the group's for an "is up" group */
static char const *callout_clip_name(
	short clip)
{
	return clip == NONE ? "is_up_group" : callout_clip_names[clip];
}

static void callout_label(
	short item_index,
	char *label,
	short size)
{
	struct item_timer const *timer = item_index != NONE ? item_timers_get(item_index) : NULL;
	short index = 0;

	if (timer)
	{
		while (index < size - 1 && timer->label[index])
		{
			label[index] = (char)timer->label[index];
			index++;
		}
	}
	label[index] = 0;
}

static void callout_log(
	char const *what,
	struct callout const *callout,
	long now)
{
	char planned[40];

	planned[0] = 0;
	if (callout->start != callout->due)
		csprintf(planned, ", planned for tick %ld", callout->start);
	if (callout->item_index != NONE)
	{
		char label[32];
		char covers[64];
		short timer_class;

		callout_label(callout->item_index, label, sizeof(label));
		/* (a combined call: the items it says) */
		covers[0] = 0;
		for (timer_class = 0; timer_class < NUMBER_OF_ITEM_TIMER_POWER_CLASSES; timer_class++)
		{
			if (TEST_FLAG(callout->covers, timer_class))
			{
				csprintf(covers + csstrlen(covers), "%s%s", covers[0] ? " + " : "; for ",
					callout_class_names[timer_class]);
			}
		}
		platform_log("callouts: tick %ld %s%s (due at tick %ld%s; item %d %s spawns at tick %ld%s)", now, what,
			callout_clip_name(callout->clip), callout->due, planned, callout->item_index, label,
			callout->spawn, covers);
	}
	else
	{
		platform_log("callouts: tick %ld %s%s (due at tick %ld%s; clock)", now, what,
			callout_clip_name(callout->clip), callout->due, planned);
	}
}

/* the planned call [start, end) overlaps, else NULL */
static struct callout const *callout_planned_overlap(
	long start,
	long end)
{
	short index;

	for (index = 0; index < callout_globals.planned_count; index++)
	{
		struct callout const *other = &callout_globals.plan[index];

		if (start < other->end && other->start < end)
			return other;
	}

	return NULL;
}

static void callout_plan_insert(
	struct callout const *callout)
{
	struct callout *plan = callout_globals.plan;
	short count = callout_globals.planned_count;
	short at;
	short index;

	if (count >= MAXIMUM_PLANNED_CALLOUTS)
	{
		platform_log("callouts: tick %ld %s not planned: the plan is full", callout->due,
			callout_clip_name(callout->clip));
		return;
	}
	for (at = count; at > 0 && plan[at - 1].start > callout->start; at--)
		;
	for (index = count; index > at; index--)
		plan[index] = plan[index - 1];
	plan[at] = *callout;
	callout_globals.planned_count++;
}

/* a call at its moment (a count or the clock's words): once (the same word
due at the same tick is said once), after any call already planned there */
static void callout_plan_fixed(
	long due,
	short clip,
	short kind,
	short item_index,
	long spawn)
{
	struct callout callout;
	struct callout const *other;
	long ticks = callout_clip_ticks(clip);
	short index;

	if (ticks <= 0)
		return;
	for (index = 0; index < callout_globals.planned_count; index++)
	{
		if (callout_globals.plan[index].clip == clip && callout_globals.plan[index].due == due)
			return;
	}

	callout_new(&callout, due, spawn, clip, item_index, kind);
	while ((other = callout_planned_overlap(callout.start, callout.start + 1)) != NULL)
		callout.start = other->end;
	callout.end = callout.start + ticks + CALLOUT_GAP_TICKS;
	callout_plan_insert(&callout);
}

/* the latest start from preferred back to earliest, else the earliest from
just after preferred on to latest, at which length ticks are free of the
plan and end by known (every call with a moment up to then is planned);
NONE for none */
static long callout_plan_room(
	long preferred,
	long earliest,
	long latest,
	long length,
	long known)
{
	long start;

	for (start = preferred; start >= earliest; start--)
	{
		if (start + length <= known && !game_engine_pregame_countdown_covers(start) &&
			!callout_planned_overlap(start, start + length))
		{
			return start;
		}
	}
	for (start = (preferred + 1 > earliest ? preferred + 1 : earliest); start <= latest; start++)
	{
		if (start + length <= known && !game_engine_pregame_countdown_covers(start) &&
			!callout_planned_overlap(start, start + length))
		{
			return start;
		}
	}

	return NONE;
}

/* an item's side clip ("red_rockets", "blue_rockets") when it has RED /
BLUE before its name (item_timers.c) and the pack has both sides' clips
for it, else NONE (one plain call for both) */
static short callout_side_clip(
	struct item_timer const *timer)
{
	short red;
	short blue;

	if (!timer->side_prefix || timer->timer_class < 0 || timer->timer_class >= NUMBER_OF_ITEM_TIMER_POWER_CLASSES)
		return NONE;
	red = (short)(_callout_red_rockets + timer->timer_class);
	blue = (short)(_callout_blue_rockets + timer->timer_class);
	if (callout_clip_ticks(red) <= 0 || callout_clip_ticks(blue) <= 0)
		return NONE;
	if (timer->side == _item_timer_side_red)
		return red;
	if (timer->side == _item_timer_side_blue)
		return blue;

	return NONE;
}

/* the power classes an entry can spawn (bits of enum item_timer_class) */
static unsigned short callout_power_classes(
	struct item_timer const *timer)
{
	return (unsigned short)(timer->classes & CALLOUT_POWER_CLASSES);
}

/* an entry that spawns one of more than one power class at random
(OS/CAMO): its item is called as it is seen on the map */
static boolean callout_item_mixed(
	struct item_timer const *timer)
{
	unsigned short classes = callout_power_classes(timer);

	return (classes & (classes - 1)) != 0;
}

/* whether an entry spawns at this tick, as a power item with a period of
minimum_period or more */
static boolean callout_item_spawns(
	struct item_timer const *timer,
	long spawn,
	long minimum_period)
{
	return timer && timer->timer_class >= 0 && timer->timer_class < NUMBER_OF_ITEM_TIMER_POWER_CLASSES &&
		timer->period_ticks > 0 && timer->period_ticks >= minimum_period && spawn % timer->period_ticks == 0;
}

/* the power entries spawning at a tick (callout_items_spawning), as their
calls 10 s before it have them */
struct callout_spawning
{
	short plain[NUMBER_OF_ITEM_TIMER_POWER_CLASSES];	/* an entry of each class called by its name, NONE */
	short sided[NUMBER_OF_ITEM_TIMER_POWER_CLASSES][2];	/* of each, the red and blue called with their side, NONE */
	short either;	/* an OS/CAMO entry called "overshield or camo in ten", NONE */
};

/* the power entries spawning at this tick (with a period of 10 s or more:
none for an item spawning more often): of each class one called by its
name; one with its side (callout_side_clip) for each side; and an OS/CAMO
entry when the pack has "overshield or camo in ten" (else it is the
overshield's, as its class) */
static void callout_items_spawning(
	long spawn,
	struct callout_spawning *spawning)
{
	short count = item_timers_count();
	short index;

	for (index = 0; index < NUMBER_OF_ITEM_TIMER_POWER_CLASSES; index++)
	{
		spawning->plain[index] = NONE;
		spawning->sided[index][0] = NONE;
		spawning->sided[index][1] = NONE;
	}
	spawning->either = NONE;
	for (index = 0; index < count; index++)
	{
		struct item_timer const *timer = item_timers_get(index);
		short side_clip;

		if (!callout_item_spawns(timer, spawn, CALLOUT_ITEM_WARNING_TICKS))
			continue;
		if (callout_power_classes(timer) == CALLOUT_OVERSHIELD_CAMO &&
			callout_clip_ticks(_callout_overshield_or_camo_in_ten) > 0)
		{
			if (spawning->either == NONE)
				spawning->either = index;
			continue;
		}
		side_clip = callout_side_clip(timer);
		if (side_clip != NONE)
		{
			short side = timer->side == _item_timer_side_red ? 0 : 1;

			if (spawning->sided[timer->timer_class][side] == NONE)
				spawning->sided[timer->timer_class][side] = index;
			continue;
		}
		if (spawning->plain[timer->timer_class] == NONE)
			spawning->plain[timer->timer_class] = index;
	}
}

/* adds a call to a group made before a spawn; FALSE when it is full */
static boolean callout_group_add(
	struct callout *calls,
	short *count,
	long due,
	long spawn,
	short clip,
	short item_index,
	short kind,
	unsigned short covers)
{
	if (*count >= MAXIMUM_SPAWNING_CALLOUTS)
		return FALSE;
	callout_new(&calls[*count], due, spawn, clip, item_index, kind);
	calls[*count].covers = covers;
	(*count)++;
	return TRUE;
}

/* the calls 10 s before the spawn of the entries spawning (due at tick),
in their order: a combined call first when the pack has its line ("powerups
in ten" for three items or more called by their names, else "overshield and
camo in ten" for those two), then in NHE's order (rocket, camo, overshield,
sniper) each other item's "<item> in ten" (else its name), the item's
calls with their side after it, and the OS/CAMO entry's "overshield or camo
in ten" after the overshield's. merge_sides: none with its side (the items
at both bases are the item's, as the pack without side clips has them) and
the OS/CAMO entry one of the three for "powerups in ten"; their count */
static short callout_compose_in_ten(
	struct callout_spawning const *spawning,
	boolean merge_sides,
	long tick,
	struct callout *calls)
{
	long spawn = tick + CALLOUT_ITEM_WARNING_TICKS;
	short plain[NUMBER_OF_ITEM_TIMER_POWER_CLASSES];
	unsigned short classes = 0;
	unsigned short covered = 0;
	short either = spawning->either;
	short items;
	short combined = NONE;
	short count = 0;
	short order;

	for (order = 0; order < NUMBER_OF_ITEM_TIMER_POWER_CLASSES; order++)
	{
		plain[order] = spawning->plain[order];
		if (merge_sides && plain[order] == NONE)
			plain[order] = spawning->sided[order][0] != NONE ? spawning->sided[order][0] : spawning->sided[order][1];
		if (plain[order] != NONE)
			SET_FLAG(classes, order, TRUE);
	}
	items = (short)((TEST_FLAG(classes, 0) ? 1 : 0) + (TEST_FLAG(classes, 1) ? 1 : 0) +
		(TEST_FLAG(classes, 2) ? 1 : 0) + (TEST_FLAG(classes, 3) ? 1 : 0));
	if (merge_sides && either != NONE)
		items++;

	if (items >= 3 && callout_clip_ticks(_callout_powerups_in_ten) > 0)
	{
		combined = _callout_powerups_in_ten;
		covered = classes;
		if (merge_sides)
			either = NONE;
	}
	else if ((classes & CALLOUT_OVERSHIELD_CAMO) == CALLOUT_OVERSHIELD_CAMO &&
		callout_clip_ticks(_callout_overshield_camo_in_ten) > 0)
	{
		combined = _callout_overshield_camo_in_ten;
		covered = CALLOUT_OVERSHIELD_CAMO;
	}
	if (combined != NONE)
	{
		short item_index = NONE;

		for (order = 0; order < NUMBER_OF_ITEM_TIMER_POWER_CLASSES && item_index == NONE; order++)
		{
			if (TEST_FLAG(covered, callout_item_order[order]))
				item_index = plain[callout_item_order[order]];
		}
		if (item_index == NONE)
			item_index = spawning->either;
		callout_group_add(calls, &count, tick, spawn, combined, item_index, _callout_kind_item, covered);
	}

	for (order = 0; order < NUMBER_OF_ITEM_TIMER_POWER_CLASSES; order++)
	{
		short timer_class = callout_item_order[order];
		short side;

		if (TEST_FLAG(classes, timer_class) && !TEST_FLAG(covered, timer_class))
		{
			short clip = (short)(_callout_rockets_in_ten + timer_class);

			if (callout_clip_ticks(clip) <= 0)
				clip = callout_item_clips[timer_class];
			if (callout_clip_ticks(clip) > 0)
				callout_group_add(calls, &count, tick, spawn, clip, plain[timer_class], _callout_kind_item, 0);
		}
		for (side = 0; !merge_sides && side < 2; side++)
		{
			short item_index = spawning->sided[timer_class][side];

			if (item_index != NONE)
			{
				callout_group_add(calls, &count, tick, spawn, callout_side_clip(item_timers_get(item_index)),
					item_index, _callout_kind_item, 0);
			}
		}
		if (timer_class == _item_timer_overshield && either != NONE)
		{
			callout_group_add(calls, &count, tick, spawn, _callout_overshield_or_camo_in_ten, either,
				_callout_kind_item, 0);
		}
	}

	return count;
}

/* the "is up" calls of the classes spawning (bits of enum item_timer_class;
an entry of each in class_items), due at tick: "overshield and camo are
up" for those two together when the pack has it, first, then each other's
"<item> is up" in NHE's order; their count (the "powerups in ten" before
three or more names none of them: each is said up) */
static short callout_compose_up(
	unsigned short classes,
	short const *class_items,
	long tick,
	struct callout *calls)
{
	short count = 0;
	short order;

	if ((classes & CALLOUT_OVERSHIELD_CAMO) == CALLOUT_OVERSHIELD_CAMO &&
		callout_clip_ticks(_callout_overshield_camo_up) > 0)
	{
		callout_group_add(calls, &count, tick, tick, _callout_overshield_camo_up,
			class_items[_item_timer_overshield], _callout_kind_item_up, CALLOUT_OVERSHIELD_CAMO);
		classes &= (unsigned short)~CALLOUT_OVERSHIELD_CAMO;
	}
	for (order = 0; order < NUMBER_OF_ITEM_TIMER_POWER_CLASSES; order++)
	{
		short timer_class = callout_item_order[order];
		short clip = (short)(_callout_rockets_up + timer_class);

		if (TEST_FLAG(classes, timer_class) && callout_clip_ticks(clip) > 0)
			callout_group_add(calls, &count, tick, tick, clip, class_items[timer_class], _callout_kind_item_up, 0);
	}

	return count;
}

/* calls' length, with the gap after each */
static long callout_calls_ticks(
	struct callout const *calls,
	short count)
{
	long ticks = 0;
	short index;

	for (index = 0; index < count; index++)
		ticks += callout_clip_ticks(calls[index].clip) + CALLOUT_GAP_TICKS;

	return ticks;
}

/* the first start from at on, by latest, at which length ticks are free of
the plan and end by known; NONE for none */
static long callout_plan_next_room(
	long at,
	long latest,
	long length,
	long known)
{
	while (at <= latest && at + length <= known)
	{
		struct callout const *other;

		if (game_engine_pregame_countdown_covers(at))
		{
			at++;
			continue;
		}
		other = callout_planned_overlap(at, at + length);
		if (!other)
			return at;
		at = other->end;
	}

	return NONE;
}

/* the calls planned in their order from first (the first there, by latest),
each in the first room after the one before (around the calls planned
between them), each ending by known; inserted when insert. FALSE when one
has no room */
static boolean callout_plan_spread(
	struct callout *calls,
	short count,
	long first,
	long latest,
	long known,
	boolean insert)
{
	long at = first;
	short index;

	for (index = 0; index < count; index++)
	{
		long length = callout_clip_ticks(calls[index].clip) + CALLOUT_GAP_TICKS;
		long start = callout_plan_next_room(at, index ? known : latest, length, known);

		if (start == NONE || (!index && start != first))
			return FALSE;
		calls[index].start = start;
		calls[index].end = start + length;
		at = calls[index].end;
	}
	for (index = 0; insert && index < count; index++)
		callout_plan_insert(&calls[index]);

	return TRUE;
}

/* the item beep and the calls 10 s before a spawn (calls[0] the beep when
the pack has one), due at tick: back to back, as late as they fit by their
moment, else earlier (NHE's staggered calls before :50), else later; else
in their order around the calls planned between them, the same way. FALSE
for no room */
static boolean callout_plan_item_group(
	struct callout *calls,
	short count,
	long tick,
	long now,
	long known)
{
	long lead = calls[0].kind == _callout_kind_item_beep ? callout_clip_ticks(calls[0].clip) + CALLOUT_GAP_TICKS : 0;
	long earliest = tick - lead - CALLOUT_MOVE_EARLIER_TICKS;
	long latest = tick + CALLOUT_LATEST_ITEM_TICKS;
	long start;
	short index;

	if (earliest <= now)
		earliest = now + 1;
	start = callout_plan_room(tick - lead, earliest, latest, callout_calls_ticks(calls, count), known);
	if (start != NONE)
	{
		for (index = 0; index < count; index++)
		{
			calls[index].start = start;
			calls[index].end = start + callout_clip_ticks(calls[index].clip) + CALLOUT_GAP_TICKS;
			callout_plan_insert(&calls[index]);
			start = calls[index].end;
		}
		return TRUE;
	}

	/* (no room for them back to back: around the calls between them) */
	for (start = tick - lead; start >= earliest; start--)
	{
		if (callout_plan_spread(calls, count, start, latest, known, TRUE))
			return TRUE;
	}
	for (start = (tick - lead + 1 > earliest ? tick - lead + 1 : earliest); start <= latest; start++)
	{
		if (callout_plan_spread(calls, count, start, latest, known, TRUE))
			return TRUE;
	}

	return FALSE;
}

/* the calls 10 s before a spawn, due at tick (the items' moment), placed
when the plan is known as far as known: the item beep, then the calls
(callout_compose_in_ten); when they have no room, the calls with the items
at both bases as one and the OS/CAMO entry among "powerups in ten" */
static void callouts_plan_in_ten(
	long tick,
	long now,
	long known)
{
	struct callout_spawning spawning;
	struct callout calls[MAXIMUM_SPAWNING_CALLOUTS + 1];
	short beep = callout_beep(_callout_beep_item);
	boolean merges = FALSE;
	short merge_sides;
	short count = 0;
	short index;

	callout_items_spawning(tick + CALLOUT_ITEM_WARNING_TICKS, &spawning);
	for (index = 0; index < NUMBER_OF_ITEM_TIMER_POWER_CLASSES; index++)
	{
		if (spawning.sided[index][0] != NONE || spawning.sided[index][1] != NONE)
			merges = TRUE;
	}
	if (spawning.either != NONE)
		merges = TRUE;

	for (merge_sides = FALSE; merge_sides <= (merges ? TRUE : FALSE); merge_sides++)
	{
		short offset = beep != NONE ? 1 : 0;

		count = callout_compose_in_ten(&spawning, (boolean)merge_sides, tick, calls + offset);
		if (count <= 0)
			return;
		if (beep != NONE)
		{
			calls[0] = calls[1];
			calls[0].clip = beep;
			calls[0].kind = _callout_kind_item_beep;
			calls[0].covers = 0;
		}
		if (callout_plan_item_group(calls, (short)(count + offset), tick, now, known))
		{
			if (merge_sides)
			{
				platform_log("callouts: tick %ld the item calls due at tick %ld said as %d without their sides, "
					"for room", now, tick, count);
			}
			return;
		}
	}
	platform_log("callouts: tick %ld no room for %d item calls due at tick %ld", now, count, tick);
}

/* the "is up" calls at a spawn, due at tick: planned as one group
(_callout_kind_item_up_group) at the spawn, or as soon after it as there is
room, made as it is due (callouts_say_up_group), when the mixed entries'
items are seen. Its room is the longest the calls can be, and for mixed
entries CALLOUT_MIXED_WAIT_TICKS before them */
static void callouts_plan_up(
	long tick,
	long now,
	long known)
{
	struct callout group;
	struct callout calls[MAXIMUM_SPAWNING_CALLOUTS];
	short count = item_timers_count();
	unsigned short possible = 0;
	unsigned short subset;
	long length = 0;
	long first = tick > now ? tick : now + 1;
	short index;

	callout_new(&group, tick, tick, NONE, NONE, _callout_kind_item_up_group);
	for (index = 0; index < count; index++)
	{
		struct item_timer const *timer = item_timers_get(index);

		if (!callout_item_spawns(timer, tick, 1))
			continue;
		if (callout_item_mixed(timer))
		{
			if (group.mixed_count < MAXIMUM_MIXED_CALLOUTS)
			{
				group.mixed_items[group.mixed_count++] = index;
				possible |= callout_power_classes(timer);
			}
			continue;
		}
		SET_FLAG(group.up_classes, timer->timer_class, TRUE);
		if (group.class_items[timer->timer_class] == NONE)
			group.class_items[timer->timer_class] = index;
	}
	if (!group.up_classes && !group.mixed_count)
		return;

	/* (the longest: of the classes known and any the mixed entries spawn) */
	for (subset = 0; subset <= CALLOUT_POWER_CLASSES; subset++)
	{
		long ticks;

		if (subset & (unsigned short)~possible)
			continue;
		ticks = callout_calls_ticks(calls, callout_compose_up((unsigned short)(group.up_classes | subset),
			group.class_items, tick, calls));
		if (ticks > length)
			length = ticks;
	}
	if (length <= 0)
		return;
	if (group.mixed_count)
		length += CALLOUT_MIXED_WAIT_TICKS;
	for (index = 0; index < NUMBER_OF_ITEM_TIMER_POWER_CLASSES && group.item_index == NONE; index++)
		group.item_index = group.class_items[callout_item_order[index]];
	if (group.item_index == NONE)
		group.item_index = group.mixed_items[0];
	group.start = callout_plan_room(first, first, tick + CALLOUT_LATEST_ITEM_TICKS, length, known);
	if (group.start == NONE)
	{
		platform_log("callouts: tick %ld no room for the is up calls due at tick %ld", now, tick);
		return;
	}
	group.end = group.start + length;
	callout_plan_insert(&group);
}

/* the items' calls with their moment at this tick, placed when the plan is
known as far as known: those 10 s before each spawn, and those at it */
static void callouts_plan_items(
	long tick,
	long now,
	long known)
{
	callouts_plan_in_ten(tick, now, known);
	callouts_plan_up(tick, now, known);
}

/* the rockets' count with its moment at this tick */
static void callouts_plan_item_counts(
	long tick)
{
	short count = item_timers_count();
	short index;

	for (index = 0; index < count; index++)
	{
		struct item_timer const *timer = item_timers_get(index);
		long ticks_left;

		if (!timer || timer->timer_class != _item_timer_rockets || timer->period_ticks <= 0)
			continue;
		/* (as item_timer_ticks_left, at this tick: 1..period) */
		ticks_left = timer->period_ticks - tick % timer->period_ticks;
		if (ticks_left <= CALLOUT_ITEM_COUNT_TICKS && ticks_left % TICKS_PER_SECOND == 0)
		{
			callout_plan_fixed(tick, (short)(_callout_one + ticks_left / TICKS_PER_SECOND - 1), _callout_kind_count,
				index, tick + ticks_left);
		}
	}
}

/* the talking timer's calls with their moment at this tick */
static void callouts_plan_clock(
	long tick)
{
	long second = tick / TICKS_PER_SECOND;
	long within_second = tick % TICKS_PER_SECOND;
	long minute = second / 60;
	long second_of_minute = second % 60;

	if (within_second == 0)
	{
		if (second_of_minute == 0 && minute > 0)
		{
			short beep = callout_beep(_callout_beep_minute);

			if (beep != NONE)
				callout_plan_fixed(tick, beep, _callout_kind_count, NONE, NONE);
		}
		else if (second_of_minute == 20 || second_of_minute == 30 || second_of_minute == 40)
		{
			short beep = callout_beep(_callout_beep_tick);

			if (beep != NONE)
				callout_plan_fixed(tick, beep, _callout_kind_count, NONE, NONE);
		}
		else if (second_of_minute >= 50)
		{
			/* ("ten" at :50, "nine" .. "one" at :51 .. :59) */
			callout_plan_fixed(tick, (short)(_callout_one + 59 - second_of_minute), _callout_kind_count, NONE,
				NONE);
		}
	}
	else if (within_second == CALLOUT_CLOCK_SPEECH_TICKS)
	{
		if (second_of_minute == 0 && minute > 0)
		{
			callout_plan_fixed(tick, (short)(_callout_first_minute + (minute - 1) % 30), _callout_kind_clock,
				NONE, NONE);
		}
		else if (second_of_minute == 30)
		{
			callout_plan_fixed(tick, _callout_thirty_seconds_left, _callout_kind_clock, NONE, NONE);
		}
		else if (second_of_minute == 40)
		{
			callout_plan_fixed(tick, _callout_twenty_seconds, _callout_kind_clock, NONE, NONE);
		}
	}
}

/* whether a call is a moment to plan at: in the game, past the PRE-GAME
COUNTDOWN */
static boolean callout_tick_valid(
	long tick)
{
	return tick > 0 && !game_engine_pregame_countdown_covers(tick);
}

/* the plan, on to CALLOUT_PLAN_AHEAD_TICKS from now: each tick's fixed
calls as it comes into the plan, the items' with their moment
CALLOUT_PLACE_AFTER_TICKS before it */
static void callouts_plan(
	short mode,
	long now)
{
	long known;

	if (callout_globals.planned_to == NONE)
	{
		callout_globals.planned_to = now;
		callout_globals.plan_began = now;
	}
	for (known = callout_globals.planned_to + 1; known <= now + CALLOUT_PLAN_AHEAD_TICKS; known++)
	{
		long tick = known - CALLOUT_PLACE_AFTER_TICKS;

		if (callout_tick_valid(known))
		{
			callouts_plan_item_counts(known);
			if (mode == _callouts_items_and_clock)
				callouts_plan_clock(known);
		}
		if (tick > callout_globals.plan_began && callout_tick_valid(tick))
			callouts_plan_items(tick, now, known + 1);
	}
	callout_globals.planned_to = now + CALLOUT_PLAN_AHEAD_TICKS;
}

/* whether a planned call is dropped now: too late, or overtaken by a later
one already due (a count by a count, a clock call by a clock call) */
static boolean callout_dropped(
	struct callout const *callout,
	long now)
{
	boolean count = callout_is_count(callout);
	boolean clock = callout->kind == _callout_kind_clock;
	long latest = count ? CALLOUT_LATEST_COUNT_TICKS :
		clock ? CALLOUT_LATEST_CLOCK_TICKS : CALLOUT_LATEST_ITEM_TICKS;
	short index;

	if (now - callout->start > latest)
		return TRUE;
	if (!count && !clock)
		return FALSE;
	for (index = 0; index < callout_globals.planned_count; index++)
	{
		struct callout const *other = &callout_globals.plan[index];

		if (other->due > callout->due && other->start <= now && other->kind == callout->kind)
			return TRUE;
	}

	return FALSE;
}

/* the "is up" group first in the plan, due now: its calls, made of the
classes known and the items its mixed entries spawned as seen on the map
(item_timer_spawned_class), waited for up to CALLOUT_MIXED_WAIT_TICKS
after its start (one not seen by then is not called), and planned back to
back from now in its room. FALSE while it waits */
static boolean callouts_say_up_group(
	long now)
{
	struct callout group = callout_globals.plan[0];
	struct callout calls[MAXIMUM_SPAWNING_CALLOUTS];
	unsigned short classes = group.up_classes;
	boolean dropped = callout_dropped(&group, now);
	short count;
	short index;
	long start;

	for (index = 0; !dropped && index < group.mixed_count; index++)
	{
		struct item_timer const *timer = item_timers_get(group.mixed_items[index]);

		if (timer && item_timer_spawned_class(timer) == NONE && now - group.start < CALLOUT_MIXED_WAIT_TICKS)
			return FALSE;
	}

	callout_globals.planned_count--;
	for (index = 0; index < callout_globals.planned_count; index++)
		callout_globals.plan[index] = callout_globals.plan[index + 1];
	if (dropped)
	{
		callout_log("dropped ", &group, now);
		return TRUE;
	}

	for (index = 0; index < group.mixed_count; index++)
	{
		struct item_timer const *timer = item_timers_get(group.mixed_items[index]);
		short timer_class = timer ? item_timer_spawned_class(timer) : NONE;
		char label[32];

		callout_label(group.mixed_items[index], label, sizeof(label));
		if (timer_class < 0 || timer_class >= NUMBER_OF_ITEM_TIMER_POWER_CLASSES)
		{
			platform_log("callouts: tick %ld item %d %s spawned at tick %ld: its item not seen, not called", now,
				group.mixed_items[index], label, group.spawn);
			continue;
		}
		platform_log("callouts: tick %ld item %d %s spawned %s at tick %ld", now, group.mixed_items[index], label,
			callout_class_names[timer_class], group.spawn);
		SET_FLAG(classes, timer_class, TRUE);
		if (group.class_items[timer_class] == NONE)
			group.class_items[timer_class] = group.mixed_items[index];
	}

	count = callout_compose_up(classes, group.class_items, group.due, calls);
	start = now;
	for (index = 0; index < count; index++)
	{
		calls[index].start = start;
		calls[index].end = start + callout_clip_ticks(calls[index].clip) + CALLOUT_GAP_TICKS;
		callout_plan_insert(&calls[index]);
		start = calls[index].end;
	}

	return TRUE;
}

/* the next call planned by now, once the last one has been said */
static void callouts_say_next(
	long now)
{
	while (callout_globals.planned_count > 0 && callout_globals.plan[0].start <= now &&
		!platform_callout_voice_busy())
	{
		struct callout callout = callout_globals.plan[0];
		short index;

		if (callout.kind == _callout_kind_item_up_group)
		{
			if (!callouts_say_up_group(now))
				break;
			continue;
		}

		callout_globals.planned_count--;
		for (index = 0; index < callout_globals.planned_count; index++)
			callout_globals.plan[index] = callout_globals.plan[index + 1];

		if (callout_dropped(&callout, now))
		{
			callout_log("dropped ", &callout, now);
			continue;
		}
		/* (only the pack's clips are planned) */
		if (platform_callout_voice_play(callout.clip))
		{
			callout_globals.playing = TRUE;
			callout_log("", &callout, now);
			break;
		}
	}
}

/* ---------- public code */

void callouts_map_begin(
	void)
{
	callouts_flush();
	callouts_mode(TRUE);
}

void callouts_update(
	void)
{
	short mode = callouts_mode(FALSE);
	long now;

	if (mode == _callouts_off || !game_engine_running() || game_engine_game_over() || hs_scenario_is_nhe())
	{
		callouts_flush();
		return;
	}

	now = game_time_get();
	/* (a new game, or a jump in game time: planned again from now on, not
	caught up on) */
	if (callout_globals.last_tick == NONE || now < callout_globals.last_tick ||
		now - callout_globals.last_tick > CALLOUT_CATCH_UP_TICKS)
	{
		callouts_flush();
	}
	callout_globals.last_tick = now;
	callouts_plan(mode, now);
	callouts_say_next(now);
}

void callouts_update_non_deterministic(
	void)
{
	/* (the clip playing stops while the game is paused or once it is
	gone; planned again as it goes on) */
	if ((callout_globals.playing || callout_globals.planned_count > 0) &&
		(!game_engine_running() || game_time_get_paused()))
	{
		callouts_flush();
	}
}

/* whether CALLOUTS (as last read) says items' calls on this map: on, and
not on Halo 1: NHE's maps (their scripts talk) */
boolean callouts_items_called(
	void)
{
	return callout_globals.mode != _callouts_off && !hs_scenario_is_nhe();
}
