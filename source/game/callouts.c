/*
CALLOUTS.C

port: CALLOUTS (game.callouts, Settings > Game Options): spoken calls of the
power items' spawns and of the match clock in a multiplayer game, in this
machine's voice pack (game.callout_voice: port/linux/src/callout_voice.c),
as Halo 1: NHE's maps' voice timer says them. From game time alone, which
every machine has as the host's, so nothing is networked: each machine says
its own.

ITEMS, in any gametype: the power items (rockets, sniper, overshield, camo)
spawning at one tick are a WAVE, called 10 seconds before each of its
spawns after the game's start, from their real periods (item_timers.c),
and at the spawn. An item at both bases (RED / BLUE before its name,
item_timers.c) is called with its side ("red sniper") when the pack has
both sides' clips; a spawn point of overshield or camo at random (Blood
Gulch's OS/CAMO) is "overshield or camo in ten", and at the spawn the item
it spawned (item_timers.c sees it on the map) is up, waited for up to half
a second, else "powerup is up". CALLOUT DETAIL (game.callout_detail) says
how much:
- MINIMAL: one line a wave: an item alone its "<item> in ten", more "power
  weapons in ten", "power items in ten" or "weapons and power items in
  ten" by their kinds; at the spawn the first item's "<item> is up" only.
  No beeps, no "five" .. "one" before the rockets.
- STANDARD: the item beep, then an item alone its "<item> in ten", an
  overshield and a camo "overshield and camo in ten", three items or more
  the line of their kinds (and at the spawn its "... are up"), else each
  item's line in turn; at the spawn each item's "<item> is up" (the
  overshield and the camo as one). "five" .. "one" before the rockets.
- VERBOSE: as STANDARD, but every item of a wave named, from clips:
  "<item>, <item> and <item> in ten" (the overshield, whose clip says "in
  ten" itself, and the OS/CAMO spot last), and at the spawn each item's own
  "<item> is up" in turn. A wave this can't say, or not within 8 seconds,
  is said as STANDARD's.
A pack without a line makes the calls it has instead.

The items' calls are planned first, so that the clock yields to them: an
item alone is said on its second (its beep before it), a wave of more
starts earlier so that its calls end by the 10 seconds mark (up to 6
seconds earlier, 8 for VERBOSE; never later while there is room), and each
spawn's "is up" calls are said on the spawn's tick.

ITEMS + CLOCK adds NHE's talking timer (its nhe_combined_timer script),
which counts each minute of game time: a beep on the minute and "N minutes"
half a second later (1 .. 30, then 1 again), beeps at :20, :30 and :40 with
"thirty seconds left" and "twenty seconds" half a second after the last two,
"ten" at :50 and "nine" .. "one" at :51 .. :59 (no beep at :10 or :50, as
NHE's). Each beep is the pack's beep for its moment (beep_minute, beep_tick,
beep_item), else its beep. MINIMAL's clock says the minutes and "thirty
seconds left" only.

The clock fits around the items' calls: an item's "in ten" due on the
clock's "ten" says it instead (the clock's "nine" .. "one" go on); a beep
an item's call is on is left out; a number moved later is said only if it
ends before the next one is due; the clock's words wait (a spawn's "is up"
first, then the minute). One call at a time, planned ahead from the clips'
lengths; a call moved later for the call before it is said as soon as that
one ends. The same word due at the same tick is said once (the rockets'
five to one before a minute are the clock's). A call that is late anyway
(the mixer, a slow machine) is dropped rather than said late: a count over
a second late or overtaken by a later count, a clock call by a later clock
call. Nothing on Halo 1: NHE's maps (their scripts talk), during the
PRE-GAME COUNTDOWN, while the game is paused or once it is over; the clip
playing then stops. Each call said or dropped is logged with its game tick
and its length in ticks, and each wave as it is planned.
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

/* CALLOUT DETAIL (game.callout_detail) */
enum callout_detail
{
	_callout_detail_minimal = 0,
	_callout_detail_standard,
	_callout_detail_verbose,
	NUMBER_OF_CALLOUT_DETAILS
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
	_callout_and,	/* VERBOSE's lists: "<item>, <item> and <item> in ten" */
	_callout_in_ten,
	_callout_powerup_up,	/* an OS/CAMO spot's item not seen */
	_callout_weapons_and_power_items_in_ten,	/* the waves' lines by their kinds */
	_callout_weapons_and_power_items_up,
	_callout_weapons_and_powerups_in_ten,
	_callout_weapons_and_powerups_up,
	_callout_power_weapons_in_ten,
	_callout_power_weapons_up,
	_callout_power_items_in_ten,
	_callout_power_items_up,
	NUMBER_OF_CALLOUT_CLIPS
};

/* how a call is planned and when it is dropped */
enum callout_kind
{
	_callout_kind_count = 0,	/* a number or the clock's beep: at its moment */
	_callout_kind_clock,	/* the clock's words: at their moment or later */
	_callout_kind_item_beep,	/* the beep before items' calls */
	_callout_kind_item,	/* "<item> in ten": its moment or earlier */
	_callout_kind_item_up,	/* "<item> is up": its moment or later */
	_callout_kind_item_up_group	/* a spawn's "is up" calls, made as it is due
		(callouts_say_up_group) */
};

/* how a spawn's "is up" calls are made (callout_compose_up) */
enum callout_up_style
{
	_callout_up_first = 0,	/* MINIMAL's: the first item's */
	_callout_up_combined,	/* STANDARD's: each, the overshield and camo as one */
	_callout_up_each,	/* VERBOSE's: each item's own */
	_callout_up_line	/* the wave's line of its kinds ("... are up") */
};

#define MAXIMUM_PLANNED_CALLOUTS 128
/* a wave's items: of each class its plain, red and blue, and the OS/CAMO spot */
#define MAXIMUM_WAVE_ITEMS (3 * NUMBER_OF_ITEM_TIMER_POWER_CLASSES + 1)
/* a wave's calls: the item beep, and VERBOSE's list of names, "and" and "in
ten" (more than any other detail's) */
#define MAXIMUM_SPAWNING_CALLOUTS (MAXIMUM_WAVE_ITEMS + 3)
/* the mixed entries (OS/CAMO) spawning together whose item is called */
#define MAXIMUM_MIXED_CALLOUTS 4
/* the longest an "is up" waits for a mixed entry's item to be seen, before
"powerup is up" */
#define CALLOUT_MIXED_WAIT_TICKS (TICKS_PER_SECOND / 2)
/* the power classes (bits of enum item_timer_class) */
#define CALLOUT_POWER_CLASSES ((unsigned short)(FLAG(NUMBER_OF_ITEM_TIMER_POWER_CLASSES) - 1))
#define CALLOUT_OVERSHIELD_CAMO ((unsigned short)(FLAG(_item_timer_overshield) | FLAG(_item_timer_camo)))
/* an item's call this long before its spawn; the rockets' count from five */
#define CALLOUT_ITEM_WARNING_TICKS (10 * TICKS_PER_SECOND)
#define CALLOUT_ITEM_COUNT_TICKS (5 * TICKS_PER_SECOND)
/* the most a wave's calls start before its 10 seconds mark (ending by it) */
#define CALLOUT_WAVE_EARLIER_TICKS (6 * TICKS_PER_SECOND)
#define CALLOUT_WAVE_EARLIER_VERBOSE_TICKS (8 * TICKS_PER_SECOND)
/* the clock's words this long after their beep, as NHE's */
#define CALLOUT_CLOCK_SPEECH_TICKS (TICKS_PER_SECOND / 2)
/* the latest a call is said after its planned tick, else it is dropped */
#define CALLOUT_LATEST_ITEM_TICKS (5 * TICKS_PER_SECOND)
#define CALLOUT_LATEST_CLOCK_TICKS (3 * TICKS_PER_SECOND)
#define CALLOUT_LATEST_COUNT_TICKS TICKS_PER_SECOND
/* the clock's words moved later than this for the calls before them are
left out */
#define CALLOUT_LATEST_CLOCK_LEFT_OUT_TICKS (10 * TICKS_PER_SECOND)
/* a jump in game time longer than this (a machine joining, a new game) is
not caught up on */
#define CALLOUT_CATCH_UP_TICKS (2 * TICKS_PER_SECOND)
/* the plan: the clock's calls with a moment this far ahead are planned;
the items' calls of a spawn this much further ahead, before any of the
clock's they could meet (a wave's start before its spawn is under this) */
#define CALLOUT_PLAN_AHEAD_TICKS (30 * TICKS_PER_SECOND)
#define CALLOUT_ITEMS_AHEAD_TICKS (20 * TICKS_PER_SECOND)
/* the most an item's call moves earlier for room */
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
	/* an "is up" group's: how its calls are made (enum callout_up_style)
	and its line's clip, the classes known to spawn and an entry of each
	(NONE), and the mixed entries whose item is seen at the spawn */
	short up_style;
	short up_clip;
	unsigned short up_classes;
	short class_items[NUMBER_OF_ITEM_TIMER_POWER_CLASSES];
	short mixed_count;
	short mixed_items[MAXIMUM_MIXED_CALLOUTS];
};

/* an item of a wave, as called */
struct callout_wave_item
{
	short item_index;
	short timer_class;	/* enum item_timer_class */
	short side_clip;	/* "red <item>" / "blue <item>" (callout_side_clip), else NONE */
	boolean either;	/* an OS/CAMO spot called "overshield or camo in ten" */
};

/* the power items spawning at one tick (callout_wave_build), in NHE's
order (rocket, camo, overshield, sniper; of each its plain, red, blue; the
OS/CAMO spot after the overshield) */
struct callout_wave
{
	long spawn;
	short count;
	boolean weapons;	/* rockets or sniper */
	boolean powerups;	/* overshield or camo */
	struct callout_wave_item items[MAXIMUM_WAVE_ITEMS];
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
	"and", "in_ten", "powerup_up",
	"weapons_and_power_items_in_ten", "weapons_and_power_items_up",
	"weapons_and_powerups_in_ten", "weapons_and_powerups_up",
	"power_weapons_in_ten", "power_weapons_up",
	"power_items_in_ten", "power_items_up",
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

static char const *const callout_detail_names[NUMBER_OF_CALLOUT_DETAILS] =
{
	"MINIMAL",
	"STANDARD",
	"VERBOSE",
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
	short detail;	/* enum callout_detail */
	unsigned long settings_read_at;
	char voice[64];	/* game.callout_voice as read */
	long planned_to;	/* the last tick whose clock calls are planned, NONE for none */
	long last_tick;	/* the last update's */
	long free_at;	/* the tick the last clip said and its gap end */
	boolean playing;	/* a clip was started since the last flush */
	short planned_count;
	struct callout plan[MAXIMUM_PLANNED_CALLOUTS];	/* by start */
} callout_globals = { _callouts_off, _callout_detail_standard, (unsigned long)-1, "", NONE, NONE, 0, FALSE, 0 };

/* ---------- private code */

/* a count marks a moment: a number or the clock's beep */
static boolean callout_is_count(
	struct callout const *callout)
{
	return callout->kind == _callout_kind_count;
}

static boolean callout_clip_is_beep(
	short clip)
{
	return clip == _callout_beep || clip == _callout_beep_minute || clip == _callout_beep_tick ||
		clip == _callout_beep_item;
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

/* the first of clips the pack has, NONE for none */
static short callout_first_clip(
	short const *clips,
	short count)
{
	short index;

	for (index = 0; index < count; index++)
	{
		if (clips[index] != NONE && callout_clip_ticks(clips[index]) > 0)
			return clips[index];
	}

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
	callout->up_clip = NONE;
	for (index = 0; index < NUMBER_OF_ITEM_TIMER_POWER_CLASSES; index++)
		callout->class_items[index] = NONE;
}

static void callouts_flush(
	void)
{
	callout_globals.planned_count = 0;
	callout_globals.planned_to = NONE;
	callout_globals.last_tick = NONE;
	callout_globals.free_at = 0;
	/* (the clip playing stops: the game is over or paused, or CALLOUTS
	changed) */
	if (callout_globals.playing)
	{
		platform_callout_voice_stop();
		callout_globals.playing = FALSE;
	}
}

/* CALLOUTS (game.callouts: "off", "items", "items_clock") and CALLOUT
DETAIL (game.callout_detail: "minimal", "standard", "verbose"), read again
when Settings changes them; and the voice pack read when it is on and its
name changed (callout_voice.c reads it again only then), its clips freed
when it is turned off. A change of any plans again */
static short callouts_mode(
	boolean load_voice)
{
	if (callout_globals.settings_read_at != config_changes())
	{
		char const *value = config_string("game.callouts");
		char const *voice = config_string("game.callout_voice");
		char const *detail_value = config_string("game.callout_detail");
		short mode;
		short detail;

		callout_globals.settings_read_at = config_changes();
		if (value && !csstrcmp(value, "items"))
			mode = _callouts_items;
		else if (value && !csstrcmp(value, "items_clock"))
			mode = _callouts_items_and_clock;
		else
			mode = _callouts_off;
		if (detail_value && !csstrcmp(detail_value, "minimal"))
			detail = _callout_detail_minimal;
		else if (detail_value && !csstrcmp(detail_value, "verbose"))
			detail = _callout_detail_verbose;
		else
			detail = _callout_detail_standard;
		if (!voice)
			voice = "";
		if (mode != callout_globals.mode || detail != callout_globals.detail ||
			csstrcmp(voice, callout_globals.voice))
		{
			callouts_flush();
			if (mode == _callouts_off)
				platform_callout_voice_unload();
		}
		callout_globals.mode = mode;
		callout_globals.detail = detail;
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
		platform_log("callouts: tick %ld %s%s [%ld ticks] (due at tick %ld%s; item %d %s spawns at tick %ld%s)", now,
			what, callout_clip_name(callout->clip), callout_clip_ticks(callout->clip), callout->due, planned,
			callout->item_index, label, callout->spawn, covers);
	}
	else
	{
		platform_log("callouts: tick %ld %s%s [%ld ticks] (due at tick %ld%s; clock)", now, what,
			callout_clip_name(callout->clip), callout_clip_ticks(callout->clip), callout->due, planned);
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

/* whether an item's call 10 s before a spawn is planned with its moment at
due (the clock's "ten" there gives way to it) */
static boolean callout_item_call_due(
	long due)
{
	short index;

	for (index = 0; index < callout_globals.planned_count; index++)
	{
		struct callout const *other = &callout_globals.plan[index];

		if ((other->kind == _callout_kind_item || other->kind == _callout_kind_item_beep) && other->due == due)
			return TRUE;
	}

	return FALSE;
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

/* a call at its moment (a count or the clock's words), around the items'
calls planned before it: once (the same word due at the same tick is said
once); the clock's "ten" left out for an item's call due then (it says "in
ten"), a beep left out where it can't be at its moment, a number where it
can't end before the next one's moment; else after any call planned there */
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
	char const *left_out = NULL;
	short index;

	if (ticks <= 0)
		return;
	for (index = 0; index < callout_globals.planned_count; index++)
	{
		if (callout_globals.plan[index].clip == clip && callout_globals.plan[index].due == due)
			return;
	}

	callout_new(&callout, due, spawn, clip, item_index, kind);
	while ((other = callout_planned_overlap(callout.start, callout.start + ticks + CALLOUT_GAP_TICKS)) != NULL)
		callout.start = other->end;
	callout.end = callout.start + ticks + CALLOUT_GAP_TICKS;
	if (clip == _callout_ten && item_index == NONE && callout_item_call_due(due))
		left_out = "the item call due then says it";
	else if (kind == _callout_kind_count && callout_clip_is_beep(clip) && callout.start != due)
		left_out = "a call is on its moment";
	else if (kind == _callout_kind_count && !callout_clip_is_beep(clip) &&
		callout.start + ticks > due + TICKS_PER_SECOND)
	{
		left_out = "a call is on it, and moved later it would meet the next";
	}
	else if (callout.start > due + CALLOUT_LATEST_CLOCK_LEFT_OUT_TICKS)
	{
		left_out = "the calls before it are too long";
	}
	if (left_out)
	{
		platform_log("callouts: %s due at tick %ld left out: %s", callout_clip_name(clip), due, left_out);
		return;
	}
	callout_plan_insert(&callout);
}

/* the latest start from preferred back to earliest, else the earliest from
just after preferred on to latest, at which length ticks are free of the
plan; NONE for none */
static long callout_plan_room(
	long preferred,
	long earliest,
	long latest,
	long length)
{
	long start;

	for (start = preferred; start >= earliest; start--)
	{
		if (!game_engine_pregame_countdown_covers(start) && !callout_planned_overlap(start, start + length))
			return start;
	}
	for (start = (preferred + 1 > earliest ? preferred + 1 : earliest); start <= latest; start++)
	{
		if (!game_engine_pregame_countdown_covers(start) && !callout_planned_overlap(start, start + length))
			return start;
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
10 s or more (none is called for an item spawning more often) */
static boolean callout_item_spawns(
	struct item_timer const *timer,
	long spawn)
{
	return timer && timer->timer_class >= 0 && timer->timer_class < NUMBER_OF_ITEM_TIMER_POWER_CLASSES &&
		timer->period_ticks >= CALLOUT_ITEM_WARNING_TICKS && spawn % timer->period_ticks == 0;
}

static void callout_wave_add(
	struct callout_wave *wave,
	short item_index,
	short timer_class,
	short side_clip,
	boolean either)
{
	struct callout_wave_item *item;

	if (item_index == NONE || wave->count >= MAXIMUM_WAVE_ITEMS)
		return;
	item = &wave->items[wave->count++];
	item->item_index = item_index;
	item->timer_class = timer_class;
	item->side_clip = side_clip;
	item->either = either;
	if (either || timer_class == _item_timer_overshield || timer_class == _item_timer_camo)
		wave->powerups = TRUE;
	else
		wave->weapons = TRUE;
}

/* the wave spawning at this tick: of each class one entry called by its
name; one with its side (callout_side_clip) for each side; and an OS/CAMO
entry when the pack has "overshield or camo in ten" (else it is the
overshield's, as its class) */
static void callout_wave_build(
	long spawn,
	struct callout_wave *wave)
{
	short plain[NUMBER_OF_ITEM_TIMER_POWER_CLASSES];
	short sided[NUMBER_OF_ITEM_TIMER_POWER_CLASSES][2];
	short either = NONE;
	short count = item_timers_count();
	short index;
	short order;

	csmemset(wave, 0, sizeof(*wave));
	wave->spawn = spawn;
	for (index = 0; index < NUMBER_OF_ITEM_TIMER_POWER_CLASSES; index++)
	{
		plain[index] = NONE;
		sided[index][0] = NONE;
		sided[index][1] = NONE;
	}
	for (index = 0; index < count; index++)
	{
		struct item_timer const *timer = item_timers_get(index);

		if (!callout_item_spawns(timer, spawn))
			continue;
		if (callout_power_classes(timer) == CALLOUT_OVERSHIELD_CAMO &&
			callout_clip_ticks(_callout_overshield_or_camo_in_ten) > 0)
		{
			if (either == NONE)
				either = index;
			continue;
		}
		if (callout_side_clip(timer) != NONE)
		{
			short side = timer->side == _item_timer_side_red ? 0 : 1;

			if (sided[timer->timer_class][side] == NONE)
				sided[timer->timer_class][side] = index;
			continue;
		}
		if (plain[timer->timer_class] == NONE)
			plain[timer->timer_class] = index;
	}

	for (order = 0; order < NUMBER_OF_ITEM_TIMER_POWER_CLASSES; order++)
	{
		short timer_class = callout_item_order[order];
		short side;

		callout_wave_add(wave, plain[timer_class], timer_class, NONE, FALSE);
		for (side = 0; side < 2; side++)
		{
			if (sided[timer_class][side] != NONE)
			{
				callout_wave_add(wave, sided[timer_class][side], timer_class,
					callout_side_clip(item_timers_get(sided[timer_class][side])), FALSE);
			}
		}
		if (timer_class == _item_timer_overshield)
			callout_wave_add(wave, either, timer_class, NONE, TRUE);
	}
}

/* an item's own line before its spawn: the OS/CAMO spot's "overshield or
camo in ten", with its side "red <item>", else "<item> in ten", else its
name; NONE for none */
static short callout_wave_item_line(
	struct callout_wave_item const *item)
{
	short clips[2];

	if (item->either)
		return callout_clip_ticks(_callout_overshield_or_camo_in_ten) > 0 ? _callout_overshield_or_camo_in_ten : NONE;
	if (item->side_clip != NONE)
		return item->side_clip;
	clips[0] = (short)(_callout_rockets_in_ten + item->timer_class);
	clips[1] = callout_item_clips[item->timer_class];
	return callout_first_clip(clips, 2);
}

/* the wave's line of its kinds, "... in ten" or at the spawn "... are up":
power weapons, power items (the powerups), or weapons and power items (else
weapons and powerups; else, before the spawn, "powerups in ten"); NONE for
none */
static short callout_wave_line(
	struct callout_wave const *wave,
	boolean up)
{
	short clips[3];

	if (wave->weapons && wave->powerups)
	{
		clips[0] = up ? _callout_weapons_and_power_items_up : _callout_weapons_and_power_items_in_ten;
		clips[1] = up ? _callout_weapons_and_powerups_up : _callout_weapons_and_powerups_in_ten;
	}
	else if (wave->weapons)
	{
		clips[0] = up ? _callout_power_weapons_up : _callout_power_weapons_in_ten;
		clips[1] = NONE;
	}
	else
	{
		clips[0] = up ? _callout_power_items_up : _callout_power_items_in_ten;
		clips[1] = NONE;
	}
	clips[2] = up ? NONE : _callout_powerups_in_ten;

	return callout_first_clip(clips, 3);
}

/* adds a call to a wave's; FALSE when it is full */
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
	if (clip == NONE || *count >= MAXIMUM_SPAWNING_CALLOUTS)
		return FALSE;
	callout_new(&calls[*count], due, spawn, clip, item_index, kind);
	calls[*count].covers = covers;
	(*count)++;
	return TRUE;
}

/* the classes of a wave's items (bits of enum item_timer_class) */
static unsigned short callout_wave_classes(
	struct callout_wave const *wave)
{
	unsigned short classes = 0;
	short index;

	for (index = 0; index < wave->count; index++)
		SET_FLAG(classes, wave->items[index].timer_class, TRUE);

	return classes;
}

/* whether VERBOSE's list says an item last, as its own "... in ten": the
overshield (whose clip says "in ten" itself, the bare word not sounding
right) and the OS/CAMO spot ("overshield or camo in ten") */
static boolean callout_list_last(
	struct callout_wave_item const *item)
{
	return item->either || (item->side_clip == NONE && item->timer_class == _item_timer_overshield);
}

/* VERBOSE's list: each item's name in the wave's order, "and" before the
last, then "in ten"; or, with an item said last (callout_list_last), "and"
before its "... in ten", and any other such item's "... in ten" after it.
FALSE when the pack can't say it (no "and" or "in ten", or a name missing) */
static boolean callout_compose_list(
	struct callout_wave const *wave,
	long tick,
	struct callout *calls,
	short *count)
{
	short names = 0;
	short named = 0;
	short lasts = 0;
	short index;
	short pass;

	if (callout_clip_ticks(_callout_and) <= 0)
		return FALSE;
	for (index = 0; index < wave->count; index++)
	{
		if (callout_list_last(&wave->items[index]))
			lasts++;
		else
			names++;
	}
	if (!lasts && callout_clip_ticks(_callout_in_ten) <= 0)
		return FALSE;

	/* (the names, then the items said last) */
	for (pass = 0; pass < 2; pass++)
	{
		for (index = 0; index < wave->count; index++)
		{
			struct callout_wave_item const *item = &wave->items[index];
			short clip;

			if (callout_list_last(item) != (pass == 1))
				continue;
			if (pass == 0)
				clip = item->side_clip != NONE ? item->side_clip : callout_item_clips[item->timer_class];
			else
				clip = item->either ? _callout_overshield_or_camo_in_ten : _callout_overshield_in_ten;
			if (callout_clip_ticks(clip) <= 0)
				return FALSE;
			named++;
			/* ("and" before the last of the list: its last name, or the
			first item said last) */
			if (named > 1 && (lasts ? named == names + 1 : named == names))
			{
				callout_group_add(calls, count, tick, wave->spawn, _callout_and, item->item_index,
					_callout_kind_item, 0);
			}
			callout_group_add(calls, count, tick, wave->spawn, clip, item->item_index, _callout_kind_item, 0);
		}
	}
	if (!lasts)
	{
		callout_group_add(calls, count, tick, wave->spawn, _callout_in_ten, wave->items[wave->count - 1].item_index,
			_callout_kind_item, 0);
	}

	return TRUE;
}

/* a wave's calls 10 s before its spawn (due at tick) at a detail, the item
beep first (STANDARD's and VERBOSE's, when the pack has one), and how its
"is up" calls are made (*up_style, *up_clip); their count, 0 when the pack
can't say it at this detail */
static short callout_compose_in_ten(
	struct callout_wave const *wave,
	short detail,
	long tick,
	struct callout *calls,
	short *up_style,
	short *up_clip)
{
	unsigned short classes = callout_wave_classes(wave);
	short beep = detail != _callout_detail_minimal ? callout_beep(_callout_beep_item) : NONE;
	short count = 0;
	short index;

	if (wave->count <= 0)
		return 0;
	*up_clip = NONE;
	if (beep != NONE)
	{
		callout_group_add(calls, &count, tick, wave->spawn, beep, wave->items[0].item_index, _callout_kind_item_beep,
			0);
	}

	if (wave->count == 1)
	{
		*up_style = detail == _callout_detail_minimal ? _callout_up_first :
			detail == _callout_detail_verbose ? _callout_up_each : _callout_up_combined;
		return callout_group_add(calls, &count, tick, wave->spawn, callout_wave_item_line(&wave->items[0]),
			wave->items[0].item_index, _callout_kind_item, 0) ? count : 0;
	}

	switch (detail)
	{
	case _callout_detail_minimal:
		/* (one line: the wave's kinds) */
		*up_style = _callout_up_first;
		return callout_group_add(calls, &count, tick, wave->spawn, callout_wave_line(wave, FALSE),
			wave->items[0].item_index, _callout_kind_item, classes) ? count : 0;

	case _callout_detail_verbose:
		*up_style = _callout_up_each;
		return callout_compose_list(wave, tick, calls, &count) ? count : 0;

	default:
		/* (an overshield and a camo as one; three or more, or weapons and
		power items, as their kinds' line; else each in turn) */
		*up_style = _callout_up_combined;
		if (wave->count == 2 && classes == CALLOUT_OVERSHIELD_CAMO && !wave->items[0].either &&
			!wave->items[1].either && wave->items[0].side_clip == NONE && wave->items[1].side_clip == NONE &&
			callout_clip_ticks(_callout_overshield_camo_in_ten) > 0)
		{
			callout_group_add(calls, &count, tick, wave->spawn, _callout_overshield_camo_in_ten,
				wave->items[0].item_index, _callout_kind_item, CALLOUT_OVERSHIELD_CAMO);
			return count;
		}
		if (wave->count >= 3 && callout_wave_line(wave, FALSE) != NONE)
		{
			*up_clip = callout_wave_line(wave, TRUE);
			if (*up_clip != NONE)
				*up_style = _callout_up_line;
			callout_group_add(calls, &count, tick, wave->spawn, callout_wave_line(wave, FALSE),
				wave->items[0].item_index, _callout_kind_item, classes);
			return count;
		}
		for (index = 0; index < wave->count; index++)
		{
			callout_group_add(calls, &count, tick, wave->spawn, callout_wave_item_line(&wave->items[index]),
				wave->items[index].item_index, _callout_kind_item, 0);
		}
		return count > (beep != NONE ? 1 : 0) ? count : 0;
	}
}

/* the "is up" calls of the classes spawning (bits of enum item_timer_class;
an entry of each in class_items) and of the mixed entries whose item is not
seen (unseen, unseen_item the first), due at tick, made as up_style says
(the wave's line up_clip; the first item's in NHE's order; each, the
overshield and camo as one; each item's own), "powerup is up" for the
unseen; their count */
static short callout_compose_up(
	short up_style,
	short up_clip,
	unsigned short classes,
	short const *class_items,
	short unseen,
	short unseen_item,
	long tick,
	struct callout *calls)
{
	short count = 0;
	short order;

	if (up_style == _callout_up_line && callout_clip_ticks(up_clip) > 0)
	{
		short item_index = unseen_item;

		for (order = NUMBER_OF_ITEM_TIMER_POWER_CLASSES - 1; order >= 0; order--)
		{
			if (TEST_FLAG(classes, callout_item_order[order]))
				item_index = class_items[callout_item_order[order]];
		}
		callout_group_add(calls, &count, tick, tick, up_clip, item_index, _callout_kind_item_up, classes);
		return count;
	}
	if (up_style == _callout_up_line)
		up_style = _callout_up_combined;

	if (up_style == _callout_up_combined && (classes & CALLOUT_OVERSHIELD_CAMO) == CALLOUT_OVERSHIELD_CAMO &&
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
		{
			callout_group_add(calls, &count, tick, tick, clip, class_items[timer_class], _callout_kind_item_up, 0);
			if (up_style == _callout_up_first)
				return count;
		}
	}
	if (unseen > 0 && (up_style != _callout_up_first || !count))
	{
		callout_group_add(calls, &count, tick, tick, _callout_powerup_up, unseen_item, _callout_kind_item_up, 0);
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
the plan; NONE for none */
static long callout_plan_next_room(
	long at,
	long latest,
	long length)
{
	while (at <= latest)
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

/* the calls planned in their order from first (the first there), each in
the first room after the one before (around the calls planned between
them), by a few seconds after the last's moment; inserted when insert.
FALSE when one has no room */
static boolean callout_plan_spread(
	struct callout *calls,
	short count,
	long first,
	long latest,
	boolean insert)
{
	long at = first;
	short index;

	for (index = 0; index < count; index++)
	{
		long length = callout_clip_ticks(calls[index].clip) + CALLOUT_GAP_TICKS;
		long start = callout_plan_next_room(at, latest, length);

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

/* a wave's calls 10 s before its spawn (due at tick): back to back from
preferred, else as late as they fit before it (never later while they fit
earlier), else later; else in their order around the calls planned between
them, the same way. FALSE for no room */
static boolean callout_plan_item_group(
	struct callout *calls,
	short count,
	long preferred,
	long tick,
	long now)
{
	long earliest = preferred - CALLOUT_MOVE_EARLIER_TICKS;
	long latest = tick + CALLOUT_LATEST_ITEM_TICKS;
	long start;
	short index;

	if (earliest <= now)
		earliest = now + 1;
	start = callout_plan_room(preferred, earliest, latest, callout_calls_ticks(calls, count));
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
	for (start = preferred; start >= earliest; start--)
	{
		if (callout_plan_spread(calls, count, start, latest, TRUE))
			return TRUE;
	}
	for (start = (preferred + 1 > earliest ? preferred + 1 : earliest); start <= latest; start++)
	{
		if (callout_plan_spread(calls, count, start, latest, TRUE))
			return TRUE;
	}

	return FALSE;
}

/* the details to try, in turn: the one asked for, then STANDARD's, then
MINIMAL's */
static short callout_detail_try(
	short detail,
	short attempt)
{
	static short const order[NUMBER_OF_CALLOUT_DETAILS][NUMBER_OF_CALLOUT_DETAILS] =
	{
		{ _callout_detail_minimal, _callout_detail_standard, NONE },
		{ _callout_detail_standard, _callout_detail_minimal, NONE },
		{ _callout_detail_verbose, _callout_detail_standard, _callout_detail_minimal },
	};

	return attempt < NUMBER_OF_CALLOUT_DETAILS ? order[detail][attempt] : NONE;
}

/* a wave's calls 10 s before its spawn (due at tick), planned when called
(its moment in the game and after now: else only how its "is up" calls are
made, *up_style and *up_clip): an item alone on its moment (its beep before
it), a wave of more ending by it, starting up to 6 s earlier (VERBOSE's 8 s:
else as STANDARD's), at the detail asked for, else, with no room, the next
(callout_detail_try) */
static void callouts_plan_in_ten(
	struct callout_wave const *wave,
	short detail,
	boolean called,
	long now,
	short *up_style,
	short *up_clip)
{
	struct callout calls[MAXIMUM_SPAWNING_CALLOUTS];
	long tick = wave->spawn - CALLOUT_ITEM_WARNING_TICKS;
	short attempt;
	short tried = NONE;

	*up_style = _callout_up_combined;
	*up_clip = NONE;
	for (attempt = 0; callout_detail_try(detail, attempt) != NONE; attempt++)
	{
		short at = callout_detail_try(detail, attempt);
		short count = callout_compose_in_ten(wave, at, tick, calls, up_style, up_clip);
		long length;
		long lead;
		long preferred;
		char names[160];
		short index;

		if (count <= 0)
			continue;
		if (!called)
			return;
		length = callout_calls_ticks(calls, count);
		lead = calls[0].kind == _callout_kind_item_beep ? callout_clip_ticks(calls[0].clip) + CALLOUT_GAP_TICKS : 0;
		if (wave->count == 1)
		{
			/* (an item alone: its call on its moment) */
			preferred = tick - lead;
		}
		else
		{
			long most = at == _callout_detail_verbose ? CALLOUT_WAVE_EARLIER_VERBOSE_TICKS : CALLOUT_WAVE_EARLIER_TICKS;

			/* (VERBOSE's list too long: as STANDARD's) */
			if (at == _callout_detail_verbose && length > most)
			{
				platform_log("callouts: tick %ld the wave at tick %ld said as STANDARD's: its VERBOSE list is "
					"%ld ticks", now, wave->spawn, length);
				continue;
			}
			preferred = tick - (length < most ? length : most);
		}
		tried = at;
		if (!callout_plan_item_group(calls, count, preferred, tick, now))
			continue;

		names[0] = 0;
		for (index = 0; index < count && csstrlen(names) + 40 < sizeof(names); index++)
		{
			csprintf(names + csstrlen(names), "%s%s", index ? " " : "", callout_clip_name(calls[index].clip));
		}
		platform_log("callouts: tick %ld wave at tick %ld: %d item%s, %s: %s, ticks %ld to %ld (its 10 s mark "
			"tick %ld)", now, wave->spawn, wave->count, wave->count == 1 ? "" : "s", callout_detail_names[at],
			names, calls[0].start, calls[count - 1].end - CALLOUT_GAP_TICKS, tick);
		return;
	}
	if (called)
	{
		platform_log("callouts: tick %ld no room for the calls of the wave at tick %ld%s", now, wave->spawn,
			tried == NONE ? " (the voice has none of its lines)" : "");
	}
}

/* the "is up" calls at a wave's spawn: planned as one group
(_callout_kind_item_up_group) at the spawn, or as soon after it as there is
room, made as it is due (callouts_say_up_group), when the mixed entries'
items are seen. Its room is the longest the calls can be, and for mixed
entries CALLOUT_MIXED_WAIT_TICKS before them */
static void callouts_plan_up(
	long tick,
	short up_style,
	short up_clip,
	long now)
{
	struct callout group;
	struct callout calls[MAXIMUM_SPAWNING_CALLOUTS];
	short count = item_timers_count();
	unsigned short possible = 0;
	unsigned short subset;
	long length = 0;
	long ticks;
	long first = tick > now ? tick : now + 1;
	short index;

	callout_new(&group, tick, tick, NONE, NONE, _callout_kind_item_up_group);
	group.up_style = up_style;
	group.up_clip = up_clip;
	for (index = 0; index < count; index++)
	{
		struct item_timer const *timer = item_timers_get(index);

		if (!callout_item_spawns(timer, tick))
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

	/* (the longest: of the classes known and any the mixed entries spawn,
	or none of theirs seen) */
	for (subset = 0; subset <= CALLOUT_POWER_CLASSES; subset++)
	{
		if (subset & (unsigned short)~possible)
			continue;
		ticks = callout_calls_ticks(calls, callout_compose_up(up_style, up_clip,
			(unsigned short)(group.up_classes | subset), group.class_items, 0, NONE, tick, calls));
		if (ticks > length)
			length = ticks;
	}
	if (group.mixed_count)
	{
		ticks = callout_calls_ticks(calls, callout_compose_up(up_style, up_clip, group.up_classes, group.class_items,
			group.mixed_count, group.mixed_items[0], tick, calls));
		if (ticks > length)
			length = ticks;
	}
	if (length <= 0)
		return;
	if (group.mixed_count && up_style != _callout_up_line)
		length += CALLOUT_MIXED_WAIT_TICKS;
	for (index = 0; index < NUMBER_OF_ITEM_TIMER_POWER_CLASSES && group.item_index == NONE; index++)
		group.item_index = group.class_items[callout_item_order[index]];
	if (group.item_index == NONE)
		group.item_index = group.mixed_items[0];
	group.start = callout_plan_room(first, first, tick + CALLOUT_LATEST_ITEM_TICKS, length);
	if (group.start == NONE)
	{
		platform_log("callouts: tick %ld no room for the is up calls due at tick %ld", now, tick);
		return;
	}
	group.end = group.start + length;
	callout_plan_insert(&group);
}

/* whether a call is a moment to plan at: in the game, past the PRE-GAME
COUNTDOWN */
static boolean callout_tick_valid(
	long tick)
{
	return tick > 0 && !game_engine_pregame_countdown_covers(tick);
}

/* the items' calls of the wave spawning at this tick: those 10 s before it
(when that is still to come) and those at it */
static void callouts_plan_items(
	long spawn,
	short detail,
	long now)
{
	struct callout_wave wave;
	long tick = spawn - CALLOUT_ITEM_WARNING_TICKS;
	short up_style;
	short up_clip;

	if (spawn <= now || !callout_tick_valid(spawn))
		return;
	callout_wave_build(spawn, &wave);
	if (!wave.count)
		return;
	callouts_plan_in_ten(&wave, detail, tick > now && callout_tick_valid(tick), now, &up_style, &up_clip);
	callouts_plan_up(spawn, up_style, up_clip, now);
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

/* the talking timer's calls with their moment at this tick (MINIMAL's:
the minutes and "thirty seconds left") */
static void callouts_plan_clock(
	long tick,
	short detail)
{
	long second = tick / TICKS_PER_SECOND;
	long within_second = tick % TICKS_PER_SECOND;
	long minute = second / 60;
	long second_of_minute = second % 60;
	boolean minimal = detail == _callout_detail_minimal;

	if (within_second == 0 && !minimal)
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
		else if (second_of_minute == 40 && !minimal)
		{
			callout_plan_fixed(tick, _callout_twenty_seconds, _callout_kind_clock, NONE, NONE);
		}
	}
}

/* the plan, on to CALLOUT_PLAN_AHEAD_TICKS from now: each tick's clock
calls as it comes into the plan, after the items' calls of the spawn
CALLOUT_ITEMS_AHEAD_TICKS after it (and when the plan begins, those of the
spawns before that, first) */
static void callouts_plan(
	short mode,
	short detail,
	long now)
{
	long known;

	if (callout_globals.planned_to == NONE)
	{
		long spawn;

		callout_globals.planned_to = now;
		for (spawn = now + 1; spawn <= now + CALLOUT_ITEMS_AHEAD_TICKS; spawn++)
			callouts_plan_items(spawn, detail, now);
	}
	for (known = callout_globals.planned_to + 1; known <= now + CALLOUT_PLAN_AHEAD_TICKS; known++)
	{
		callouts_plan_items(known + CALLOUT_ITEMS_AHEAD_TICKS, detail, now);
		if (callout_tick_valid(known))
		{
			if (detail != _callout_detail_minimal)
				callouts_plan_item_counts(known);
			if (mode == _callouts_items_and_clock)
				callouts_plan_clock(known, detail);
		}
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
after its start (one not seen by then is "powerup is up"; a wave's line
waits for none), and planned back to back from now in its room. FALSE
while it waits */
static boolean callouts_say_up_group(
	long now)
{
	struct callout group = callout_globals.plan[0];
	struct callout calls[MAXIMUM_SPAWNING_CALLOUTS];
	unsigned short classes = group.up_classes;
	boolean dropped = callout_dropped(&group, now);
	short unseen = 0;
	short unseen_item = NONE;
	short count;
	short index;
	long start;

	for (index = 0; !dropped && group.up_style != _callout_up_line && index < group.mixed_count; index++)
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

		if (group.up_style == _callout_up_line)
			break;
		callout_label(group.mixed_items[index], label, sizeof(label));
		if (timer_class < 0 || timer_class >= NUMBER_OF_ITEM_TIMER_POWER_CLASSES)
		{
			platform_log("callouts: tick %ld item %d %s spawned at tick %ld: its item not seen, said as a powerup",
				now, group.mixed_items[index], label, group.spawn);
			if (!unseen++)
				unseen_item = group.mixed_items[index];
			continue;
		}
		platform_log("callouts: tick %ld item %d %s spawned %s at tick %ld", now, group.mixed_items[index], label,
			callout_class_names[timer_class], group.spawn);
		SET_FLAG(classes, timer_class, TRUE);
		if (group.class_items[timer_class] == NONE)
			group.class_items[timer_class] = group.mixed_items[index];
	}

	count = callout_compose_up(group.up_style, group.up_clip, classes, group.class_items, unseen, unseen_item,
		group.due, calls);
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

/* the next call planned by now, once the last one has been said; a call
moved later for the one before it (start after its moment) as soon as that
one has ended */
static void callouts_say_next(
	long now)
{
	while (callout_globals.planned_count > 0 && !platform_callout_voice_busy())
	{
		struct callout callout = callout_globals.plan[0];
		short index;

		if (callout.start > now &&
			(callout.start <= callout.due || callout.due > now || now < callout_globals.free_at))
		{
			break;
		}
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
			callout_globals.free_at = now + callout_clip_ticks(callout.clip) + CALLOUT_GAP_TICKS;
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
	callouts_plan(mode, callout_globals.detail, now);
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
