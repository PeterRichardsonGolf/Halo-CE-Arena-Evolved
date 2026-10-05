/*
CALLOUTS.C

port: CALLOUTS (game.callouts, Settings > Game Options): spoken calls of the
power items' spawns and of the match clock in a multiplayer game, in this
machine's voice pack (game.callout_voice: port/linux/src/callout_voice.c),
as Halo 1: NHE's maps' voice timer says them. From game time alone, which
every machine has as the host's, so nothing is networked: each machine says
its own.

ITEMS: each power item's name (rockets, sniper, overshield, camo) 10
seconds before each of its spawns after the game's start, from its real
period (item_timers.c), and "five" .. "one" in the last 5 seconds before the
rockets'. Only when the gametype has TIMERS or TRAINING, as NHE's power
calls need its NHE & Powerups or Training mode.

ITEMS + CLOCK adds NHE's talking timer (its nhe_combined_timer script),
which counts each minute of game time: a beep on the minute and "N minutes"
half a second later (1 .. 30, then 1 again), beeps at :20, :30 and :40 with
"thirty seconds left" and "twenty seconds" half a second after the last two,
"ten" at :50 and "nine" .. "one" at :51 .. :59 (no beep at :10 or :50, as
NHE's).

One call at a time: calls due together queue, the items' ahead of the
clock's, and the same word due at the same tick is said once (the rockets'
five to one before a minute are the clock's). A call that waits until a
later one is due is dropped (a count, a number or a beep, by a later count;
a clock call by a later clock call), so the count never runs late; so is a
call too late to mean anything (a count over a second late). Nothing on Halo 1: NHE's maps (their scripts talk), during
the PRE-GAME COUNTDOWN or once the game is over. Each call said or dropped
is logged with its game tick.
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
	_callout_rockets = _callout_first_minute + 30,
	_callout_sniper,
	_callout_overshield,
	_callout_camo,
	_callout_beep,
	NUMBER_OF_CALLOUT_CLIPS
};

#define MAXIMUM_QUEUED_CALLOUTS 16
/* an item's name this long before its spawn; the rockets' count from five */
#define CALLOUT_ITEM_WARNING_TICKS (10 * TICKS_PER_SECOND)
#define CALLOUT_ITEM_COUNT_TICKS (5 * TICKS_PER_SECOND)
/* the clock's words this long after their beep, as NHE's */
#define CALLOUT_CLOCK_SPEECH_TICKS (TICKS_PER_SECOND / 2)
/* the latest a call is said after it was due, else it is dropped */
#define CALLOUT_LATEST_ITEM_TICKS (5 * TICKS_PER_SECOND)
#define CALLOUT_LATEST_CLOCK_TICKS (3 * TICKS_PER_SECOND)
#define CALLOUT_LATEST_COUNT_TICKS TICKS_PER_SECOND
/* a jump in game time longer than this (a machine joining, a new game) is
not caught up on */
#define CALLOUT_CATCH_UP_TICKS (2 * TICKS_PER_SECOND)

/* ---------- structures */

struct callout
{
	long due;
	long spawn;	/* the item's spawn, NONE for the clock's */
	short clip;
	short item_index;	/* the item timer's (item_timers_get), NONE for the clock's */
};

/* ---------- prototypes */

void platform_log(char const *format, ...);
char const *config_string(char const *name);
unsigned long config_changes(void);

/* the voice pack (port/linux/src/callout_voice.c) */
int platform_callout_voice_load(char const *pack, char const *const *names, int count);
int platform_callout_voice_play(int clip);
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
	"rockets",
	"sniper",
	"overshield",
	"camo",
	"beep",
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

static struct
{
	short mode;	/* enum callout_mode */
	unsigned long settings_read_at;
	long last_tick;	/* the last tick scheduled, NONE for none */
	short queued_count;
	struct callout queue[MAXIMUM_QUEUED_CALLOUTS];
} callout_globals = { _callouts_off, (unsigned long)-1, NONE, 0 };

/* ---------- private code */

/* a count marks a moment: a number or a beep */
static boolean callout_is_count(
	short clip)
{
	return (clip >= _callout_one && clip <= _callout_ten) || clip == _callout_beep;
}

/* CALLOUTS (game.callouts: "off", "items", "items_clock"), read again when
Settings changes it; and the voice pack read when it is on and its name
changed (callout_voice.c reads it again only then) */
static short callouts_mode(
	boolean load_voice)
{
	if (callout_globals.settings_read_at != config_changes())
	{
		char const *value = config_string("game.callouts");

		callout_globals.settings_read_at = config_changes();
		if (value && !csstrcmp(value, "items"))
			callout_globals.mode = _callouts_items;
		else if (value && !csstrcmp(value, "items_clock"))
			callout_globals.mode = _callouts_items_and_clock;
		else
			callout_globals.mode = _callouts_off;
		load_voice = TRUE;
	}
	if (load_voice && callout_globals.mode != _callouts_off)
	{
		platform_callout_voice_load(config_string("game.callout_voice"), callout_clip_names,
			NUMBER_OF_CALLOUT_CLIPS);
	}

	return callout_globals.mode;
}

static void callouts_flush(
	void)
{
	callout_globals.queued_count = 0;
	callout_globals.last_tick = NONE;
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
	if (callout->item_index != NONE)
	{
		char label[32];

		callout_label(callout, label, sizeof(label));
		platform_log("callouts: tick %ld %s%s (due at tick %ld; item %d %s spawns at tick %ld)", now, what,
			callout_clip_names[callout->clip], callout->due, callout->item_index, label, callout->spawn);
	}
	else
	{
		platform_log("callouts: tick %ld %s%s (due at tick %ld; clock)", now, what,
			callout_clip_names[callout->clip], callout->due);
	}
}

/* a call due at this tick: once (the same word due at the same tick is said
once), the items' ahead of the clock's */
static void callout_queue(
	long due,
	short clip,
	short item_index,
	long spawn)
{
	struct callout *queue = callout_globals.queue;
	short count = callout_globals.queued_count;
	short index;
	short at = count;

	for (index = 0; index < count; index++)
	{
		if (queue[index].clip == clip && queue[index].due == due)
			return;
	}
	if (count >= MAXIMUM_QUEUED_CALLOUTS)
	{
		platform_log("callouts: tick %ld %s not queued: the queue is full", due, callout_clip_names[clip]);
		return;
	}
	if (item_index != NONE)
	{
		for (at = 0; at < count && queue[at].item_index != NONE; at++)
			;
	}
	for (index = count; index > at; index--)
		queue[index] = queue[index - 1];
	queue[at].due = due;
	queue[at].spawn = spawn;
	queue[at].clip = clip;
	queue[at].item_index = item_index;
	callout_globals.queued_count++;
}

/* the power items' calls due at this tick, from each entry's own spawns:
every period from the game's start (item_timers.c) */
static void callouts_schedule_items(
	long tick)
{
	short count = item_timers_count();
	short index;

	for (index = 0; index < count; index++)
	{
		struct item_timer const *timer = item_timers_get(index);
		long ticks_left;

		if (!timer || timer->timer_class < 0 || timer->timer_class >= NUMBER_OF_ITEM_TIMER_POWER_CLASSES ||
			timer->period_ticks <= 0)
		{
			continue;
		}
		/* (as item_timer_ticks_left, at this tick: 1..period) */
		ticks_left = timer->period_ticks - tick % timer->period_ticks;
		if (ticks_left == CALLOUT_ITEM_WARNING_TICKS)
		{
			callout_queue(tick, callout_item_clips[timer->timer_class], index, tick + ticks_left);
		}
		else if (timer->timer_class == _item_timer_rockets && ticks_left <= CALLOUT_ITEM_COUNT_TICKS &&
			ticks_left % TICKS_PER_SECOND == 0)
		{
			callout_queue(tick, (short)(_callout_one + ticks_left / TICKS_PER_SECOND - 1), index,
				tick + ticks_left);
		}
	}
}

/* the talking timer's calls due at this tick */
static void callouts_schedule_clock(
	long tick)
{
	long second = tick / TICKS_PER_SECOND;
	long within_second = tick % TICKS_PER_SECOND;
	long minute = second / 60;
	long second_of_minute = second % 60;

	if (within_second == 0)
	{
		if ((second_of_minute == 0 && minute > 0) || second_of_minute == 20 || second_of_minute == 30 ||
			second_of_minute == 40)
		{
			callout_queue(tick, _callout_beep, NONE, NONE);
		}
		else if (second_of_minute >= 50)
		{
			/* ("ten" at :50, "nine" .. "one" at :51 .. :59) */
			callout_queue(tick, (short)(_callout_one + 59 - second_of_minute), NONE, NONE);
		}
	}
	else if (within_second == CALLOUT_CLOCK_SPEECH_TICKS)
	{
		if (second_of_minute == 0 && minute > 0)
			callout_queue(tick, (short)(_callout_first_minute + (minute - 1) % 30), NONE, NONE);
		else if (second_of_minute == 30)
			callout_queue(tick, _callout_thirty_seconds_left, NONE, NONE);
		else if (second_of_minute == 40)
			callout_queue(tick, _callout_twenty_seconds, NONE, NONE);
	}
}

/* whether a queued call is dropped now: too late, or overtaken by a later
one already due (a count by a count, a clock call by a clock call) */
static boolean callout_dropped(
	struct callout const *callout,
	long now)
{
	boolean count = callout_is_count(callout->clip);
	boolean clock = callout->item_index == NONE;
	long latest = count ? CALLOUT_LATEST_COUNT_TICKS :
		clock ? CALLOUT_LATEST_CLOCK_TICKS : CALLOUT_LATEST_ITEM_TICKS;
	short index;

	if (now - callout->due > latest)
		return TRUE;
	if (!count && !clock)
		return FALSE;
	for (index = 0; index < callout_globals.queued_count; index++)
	{
		struct callout const *other = &callout_globals.queue[index];

		if (other->due > callout->due && other->due <= now &&
			(count ? callout_is_count(other->clip) : other->item_index == NONE))
		{
			return TRUE;
		}
	}

	return FALSE;
}

/* the next call, once the last one has been said */
static void callouts_say_next(
	long now)
{
	while (callout_globals.queued_count > 0 && !platform_callout_voice_busy())
	{
		struct callout callout = callout_globals.queue[0];
		short index;

		callout_globals.queued_count--;
		for (index = 0; index < callout_globals.queued_count; index++)
			callout_globals.queue[index] = callout_globals.queue[index + 1];

		if (callout_dropped(&callout, now))
		{
			callout_log("dropped ", &callout, now);
			continue;
		}
		/* (a clip the pack does not have is skipped: callout_voice.c logs
		those once, as it reads the pack) */
		if (platform_callout_voice_play(callout.clip))
		{
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
	long tick;

	if (mode == _callouts_off || !game_engine_running() || game_engine_game_over() || hs_scenario_is_nhe())
	{
		callouts_flush();
		return;
	}

	now = game_time_get();
	/* (a new game, or a jump in game time: from now on, not caught up on) */
	if (callout_globals.last_tick == NONE || now < callout_globals.last_tick ||
		now - callout_globals.last_tick > CALLOUT_CATCH_UP_TICKS)
	{
		callout_globals.queued_count = 0;
		callout_globals.last_tick = now - 1;
	}
	for (tick = callout_globals.last_tick + 1; tick <= now; tick++)
	{
		if (tick <= 0 || game_engine_pregame_countdown_covers(tick))
			continue;
		if (game_engine_item_timers())
			callouts_schedule_items(tick);
		if (mode == _callouts_items_and_clock)
			callouts_schedule_clock(tick);
	}
	callout_globals.last_tick = now;

	callouts_say_next(now);
}
