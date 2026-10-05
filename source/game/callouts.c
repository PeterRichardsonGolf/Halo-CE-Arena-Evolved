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
name, after the pack's item beep. "five" .. "one" in the last 5 seconds
before the rockets' spawn, and at each spawn "<item> is up" when the pack
has it.

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
before :50, with the items spawning together back to back; an "is up"
waits for the clock's words before it. The same word due at the same tick
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
};

#define MAXIMUM_PLANNED_CALLOUTS 64
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
	short clip;
	short item_index;	/* the item timer's (item_timers_get), NONE for the clock's */
	short kind;	/* enum callout_kind */
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
	long milliseconds = platform_callout_voice_milliseconds(clip);

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

static void callout_label(
	struct callout const *callout,
	char *label,
	short size)
{
	struct item_timer const *timer = callout->item_index != NONE ? item_timers_get(callout->item_index) : NULL;
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

		callout_label(callout, label, sizeof(label));
		platform_log("callouts: tick %ld %s%s (due at tick %ld%s; item %d %s spawns at tick %ld)", now, what,
			callout_clip_names[callout->clip], callout->due, planned, callout->item_index, label,
			callout->spawn);
	}
	else
	{
		platform_log("callouts: tick %ld %s%s (due at tick %ld%s; clock)", now, what,
			callout_clip_names[callout->clip], callout->due, planned);
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
			callout_clip_names[callout->clip]);
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

	callout.due = due;
	callout.start = due;
	callout.spawn = spawn;
	callout.clip = clip;
	callout.item_index = item_index;
	callout.kind = kind;
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

/* the power entries spawning at this tick, one of each class (OS/CAMO's
being the overshield's), in NHE's order: rocket, camo, overshield, sniper
(its calls before a minute); their count */
static short callout_items_spawning(
	long spawn,
	long minimum_period,
	short *indices)
{
	short count = item_timers_count();
	short found = 0;
	short order;

	for (order = 0; order < NUMBER_OF_ITEM_TIMER_POWER_CLASSES; order++)
	{
		short index;

		for (index = 0; index < count; index++)
		{
			struct item_timer const *timer = item_timers_get(index);

			if (timer && timer->timer_class == callout_item_order[order] && timer->period_ticks > 0 &&
				timer->period_ticks >= minimum_period && spawn % timer->period_ticks == 0)
			{
				indices[found++] = index;
				break;
			}
		}
	}

	return found;
}

/* the items' calls with their moment at this tick, placed when the plan is
known as far as known. "<item> in ten" (else the name), 10 s before each
spawn: the items spawning together back to back after one item beep, as
late as they fit by their moment, else earlier (NHE's staggered calls
before :50). "<item> is up" at the spawn, or as soon after it as there is
room */
static void callouts_plan_items(
	long tick,
	long now,
	long known)
{
	short indices[NUMBER_OF_ITEM_TIMER_POWER_CLASSES];
	short found;
	short index;
	struct callout group[NUMBER_OF_ITEM_TIMER_POWER_CLASSES];
	short group_count = 0;
	long length = 0;
	short beep = callout_beep(_callout_beep_item);

	/* the calls 10 s before a spawn (the next: none for an item spawning
	more often) */
	found = callout_items_spawning(tick + CALLOUT_ITEM_WARNING_TICKS, CALLOUT_ITEM_WARNING_TICKS, indices);
	for (index = 0; index < found; index++)
	{
		short item_clip = callout_item_clips[item_timers_get(indices[index])->timer_class];
		short clip = (short)(_callout_rockets_in_ten + (item_clip - _callout_rockets));

		if (callout_clip_ticks(clip) <= 0)
			clip = item_clip;
		if (callout_clip_ticks(clip) <= 0)
			continue;
		group[group_count].due = tick;
		group[group_count].spawn = tick + CALLOUT_ITEM_WARNING_TICKS;
		group[group_count].clip = clip;
		group[group_count].item_index = indices[index];
		group[group_count].kind = _callout_kind_item;
		length += callout_clip_ticks(clip) + CALLOUT_GAP_TICKS;
		group_count++;
	}
	if (group_count > 0)
	{
		long beep_length = beep != NONE ? callout_clip_ticks(beep) + CALLOUT_GAP_TICKS : 0;
		long earliest = tick - beep_length - CALLOUT_MOVE_EARLIER_TICKS;
		long start = callout_plan_room(tick - beep_length, earliest > now ? earliest : now + 1,
			tick + CALLOUT_LATEST_ITEM_TICKS, beep_length + length, known);

		if (start == NONE)
		{
			platform_log("callouts: tick %ld no room for %d item calls due at tick %ld", now, group_count, tick);
		}
		else
		{
			if (beep != NONE)
			{
				struct callout callout = group[0];

				callout.clip = beep;
				callout.kind = _callout_kind_item_beep;
				callout.start = start;
				callout.end = start + beep_length;
				callout_plan_insert(&callout);
				start = callout.end;
			}
			for (index = 0; index < group_count; index++)
			{
				group[index].start = start;
				group[index].end = start + callout_clip_ticks(group[index].clip) + CALLOUT_GAP_TICKS;
				callout_plan_insert(&group[index]);
				start = group[index].end;
			}
		}
	}

	/* the calls at a spawn */
	found = callout_items_spawning(tick, 1, indices);
	for (index = 0; index < found; index++)
	{
		struct callout callout;
		short item_clip = callout_item_clips[item_timers_get(indices[index])->timer_class];
		short clip = (short)(_callout_rockets_up + (item_clip - _callout_rockets));
		long first = tick > now ? tick : now + 1;

		if (callout_clip_ticks(clip) <= 0)
			continue;
		callout.due = tick;
		callout.spawn = tick;
		callout.clip = clip;
		callout.item_index = indices[index];
		callout.kind = _callout_kind_item_up;
		length = callout_clip_ticks(clip) + CALLOUT_GAP_TICKS;
		callout.start = callout_plan_room(first, first, tick + CALLOUT_LATEST_ITEM_TICKS, length, known);
		if (callout.start == NONE)
		{
			platform_log("callouts: tick %ld no room for %s due at tick %ld", now, callout_clip_names[clip], tick);
			continue;
		}
		callout.end = callout.start + length;
		callout_plan_insert(&callout);
	}
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

/* the next call planned by now, once the last one has been said */
static void callouts_say_next(
	long now)
{
	while (callout_globals.planned_count > 0 && callout_globals.plan[0].start <= now &&
		!platform_callout_voice_busy())
	{
		struct callout callout = callout_globals.plan[0];
		short index;

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
