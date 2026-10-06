/*
CALLOUTS_CHECK.C

A check of the callouts' plan (source/game/callouts.c), built with the
game's flags by tools/test_linux_port.py and run with the voice pack's
folder (port/assets/voices/cori): a game of Blood Gulch's power items
(overshield every minute, rockets every 90 s, sniper and camo every 2
minutes, an OS/CAMO spot every 3) played through stand-ins for game time,
the item timers and the mixer, at each CALLOUT DETAIL, with the clips'
real lengths. Checks that an item alone is called on its 10 s mark and is
up on its spawn's tick; that a wave's calls end by its 10 s mark and start
at most 6 s (VERBOSE 8 s) before it; that the clock's "ten" gives way to an
item call due then and its "nine" .. "one" go on; that "is up" comes
before the minute; MINIMAL's one line and no beeps; VERBOSE's lists; and an
OS/CAMO spot's item said up as soon as seen, else "powerup is up" half a
second later. Prints each wave's calls (spawn, start, end) and PASS, or the
failures.
*/

#define BUILDING_CSERIES
#include "cseries/cseries.h"

#include "game/callouts.h"
#include "game/game.h"
#include "game/game_engine.h"
#include "game/item_timers.h"
#include "hs/hs.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* (the C library's own, not the game's stand-ins for Xbox paths and MSVC's
formats, port/linux/include/stdio.h) */
#undef fopen
#undef snprintf
#undef vsnprintf
#undef sprintf
#undef vsprintf
#undef printf
#undef vprintf

#define CHECK_TICKS 11100
#define MAXIMUM_PLAYED 2000
#define MAXIMUM_CLIPS 128

/* ---------- stand-ins for the game */

static long check_now;
static char const *check_detail = "standard";
static unsigned long check_changes = 1;
static int check_mixed_seen = 1;	/* the OS/CAMO spot's item is seen at its spawn */
static int check_mixed_class = _item_timer_camo;
static int failures;

static struct item_timer timers[8];
static short timer_count;

/* (the game's own C library, which its headers may name in place of the
C library's) */
#undef memcmp
#undef memset
#undef memcpy
#undef strcmp
#undef strncmp
#undef strncpy
#undef strlen
#undef strcpy
long csmemcmp(const void *p1, const void *p2, unsigned long size) { return memcmp(p1, p2, size); }
void *csmemcpy(void *destination, const void *source, unsigned long size) { return memcpy(destination, source, size); }
long csstrncmp(const char *s1, const char *s2, unsigned long size) { return strncmp(s1, s2, size); }
char *csstrcpy(char *destination, const char *source) { return strcpy(destination, source); }
void *csmemset(void *buffer, long c, unsigned long size) { return memset(buffer, (int)c, size); }
long csstrcmp(const char *s1, const char *s2) { return strcmp(s1, s2); }
char *csstrncpy(char *s1, const char *s2, unsigned long size) { return strncpy(s1, s2, size); }
unsigned long csstrlen(const char *s1) { return strlen(s1); }
char *csprintf(char *buffer, char *format, ...)
{
	va_list arguments;

	va_start(arguments, format);
	vsprintf(buffer, format, arguments);
	va_end(arguments);
	return buffer;
}

long game_time_get(void) { return check_now; }
boolean game_time_get_paused(void) { return FALSE; }
boolean game_engine_running(void) { return TRUE; }
boolean game_engine_game_over(void) { return FALSE; }
boolean hs_scenario_is_nhe(void) { return FALSE; }
boolean game_engine_pregame_countdown_covers(long time) { return time < 0; }

char const *config_string(char const *name)
{
	if (!strcmp(name, "game.callouts"))
		return "items_clock";
	if (!strcmp(name, "game.callout_voice"))
		return "cori";
	if (!strcmp(name, "game.callout_detail"))
		return check_detail;
	return NULL;
}

unsigned long config_changes(void) { return check_changes; }

short item_timers_count(void) { return timer_count; }
struct item_timer const *item_timers_get(short index)
{
	return index >= 0 && index < timer_count ? &timers[index] : NULL;
}

short item_timer_spawned_class(struct item_timer const *timer)
{
	long since = check_now % timer->period_ticks;

	if (check_now < timer->period_ticks || since >= 20 * TICKS_PER_SECOND)
		return NONE;
	if ((timer->classes & (timer->classes - 1)) == 0)
		return timer->timer_class;
	return check_mixed_seen ? check_mixed_class : NONE;
}

/* ---------- the voice: the pack's WAVs' lengths, and a clip "playing"
for its length in game time */

static int clip_count;
static long clip_milliseconds[MAXIMUM_CLIPS];
static long playing_until_ms = -1;

static long wav_milliseconds(char const *folder, char const *name)
{
	char path[1024];
	unsigned char header[4096];
	FILE *file;
	size_t length;
	size_t offset;
	unsigned long rate = 0, block = 0, data = 0;

	snprintf(path, sizeof(path), "%s/%s.wav", folder, name);
	file = fopen(path, "rb");
	if (!file)
		return 0;
	length = fread(header, 1, sizeof(header), file);
	fclose(file);
	for (offset = 12; offset + 8 <= length;)
	{
		unsigned long size = header[offset + 4] | header[offset + 5] << 8 | header[offset + 6] << 16 |
			(unsigned long)header[offset + 7] << 24;

		if (!memcmp(header + offset, "fmt ", 4))
		{
			rate = header[offset + 12] | header[offset + 13] << 8 | header[offset + 14] << 16;
			block = header[offset + 20] | header[offset + 21] << 8;
		}
		else if (!memcmp(header + offset, "data", 4))
		{
			data = size;
			break;
		}
		offset += 8 + size + (size & 1);
	}
	if (!rate || !block)
		return 0;
	return (long)((double)(data / block) * 1000.0 / rate + 0.999);
}

static char const *voice_folder;

int platform_callout_voice_load(char const *pack, char const *const *names, int count)
{
	int index;

	(void)pack;
	clip_count = count < MAXIMUM_CLIPS ? count : MAXIMUM_CLIPS;
	for (index = 0; index < clip_count; index++)
		clip_milliseconds[index] = wav_milliseconds(voice_folder, names[index]);
	return clip_count;
}

void platform_callout_voice_unload(void) { }
long platform_callout_voice_milliseconds(int clip)
{
	return clip >= 0 && clip < clip_count ? clip_milliseconds[clip] : 0;
}
int platform_callout_voice_play(int clip)
{
	if (clip < 0 || clip >= clip_count || clip_milliseconds[clip] <= 0)
		return 0;
	playing_until_ms = check_now * 1000 / TICKS_PER_SECOND + clip_milliseconds[clip];
	return 1;
}
void platform_callout_voice_stop(void) { playing_until_ms = -1; }
int platform_callout_voice_busy(void) { return check_now * 1000 / TICKS_PER_SECOND < playing_until_ms; }

/* ---------- the log: each call said, as debug.txt has it */

struct played
{
	long tick;
	long ticks;
	long due;
	long spawn;	/* NONE: the clock's */
	char clip[64];
};

static struct played played[MAXIMUM_PLAYED];
static int played_count;
static int left_out_ten;

void platform_log(char const *format, ...)
{
	char line[512];
	struct played call;
	char const *spawns;
	va_list arguments;

	va_start(arguments, format);
	vsnprintf(line, sizeof(line), format, arguments);
	va_end(arguments);
	if (getenv("CALLOUTS_CHECK_LOG"))
		printf("  log: %s\n", line);
	if (!strncmp(line, "callouts: ten due at tick", 25) && strstr(line, "left out"))
		left_out_ten++;
	memset(&call, 0, sizeof(call));
	if (sscanf(line, "callouts: tick %ld %63s [%ld ticks] (due at tick %ld", &call.tick, call.clip, &call.ticks,
		&call.due) != 4 || !strcmp(call.clip, "dropped"))
	{
		return;
	}
	spawns = strstr(line, "spawns at tick ");
	call.spawn = spawns ? atol(spawns + 15) : NONE;
	if (played_count < MAXIMUM_PLAYED)
		played[played_count++] = call;
}

/* ---------- the check */

static void fail(char const *format, ...)
{
	va_list arguments;

	printf("FAIL [%s]: ", check_detail);
	va_start(arguments, format);
	vprintf(format, arguments);
	va_end(arguments);
	printf("\n");
	failures++;
}

static void timer_add(char const *label, long seconds, short timer_class, unsigned short classes)
{
	struct item_timer *timer = &timers[timer_count++];
	int index;

	memset(timer, 0, sizeof(*timer));
	timer->period_ticks = seconds * TICKS_PER_SECOND;
	timer->timer_class = timer_class;
	timer->classes = classes;
	timer->side = _item_timer_side_middle;
	for (index = 0; label[index] && index < 15; index++)
		timer->label[index] = (wchar_t)label[index];
}

static struct played const *played_at(long tick, char const *clip)
{
	int index;

	for (index = 0; index < played_count; index++)
	{
		if (played[index].tick == tick && (!clip || !strcmp(played[index].clip, clip)))
			return &played[index];
	}
	return NULL;
}

static struct played const *played_due(long due, char const *clip)
{
	int index;

	for (index = 0; index < played_count; index++)
	{
		if (played[index].due == due && !strcmp(played[index].clip, clip))
			return &played[index];
	}
	return NULL;
}

static int is_beep(char const *clip)
{
	return !strncmp(clip, "beep", 4);
}

static void play(char const *detail, int mixed_seen)
{
	check_detail = detail;
	check_mixed_seen = mixed_seen;
	check_changes++;
	played_count = 0;
	left_out_ten = 0;
	callouts_map_begin();
	for (check_now = 1; check_now <= CHECK_TICKS; check_now++)
		callouts_update();
}

/* each wave's calls before its spawn (spawn - 10 s their moment) and at it */
static void waves_print_and_check(long most_earlier)
{
	long spawn;

	printf("%-9s %-6s %-36s %6s %6s %6s\n", "detail", "spawn", "call", "due", "start", "end");
	for (spawn = 1800; spawn <= CHECK_TICKS - 300; spawn += 900)
	{
		long mark = spawn - 10 * TICKS_PER_SECOND;
		long first = -1, last = -1;
		int names = 0, beeps = 0;
		int index;

		for (index = 0; index < played_count; index++)
		{
			struct played const *call = &played[index];

			if (call->spawn != spawn)
				continue;
			printf("%-9s %-6ld %-36s %6ld %6ld %6ld\n", check_detail, spawn, call->clip, call->due, call->tick,
				call->tick + call->ticks);
			if (call->due != mark)
				continue;
			if (first < 0)
				first = call->tick;
			last = call->tick + call->ticks;
			if (is_beep(call->clip))
				beeps++;
			else
				names++;
		}
		if (first < 0)
			continue;
		if (names == 1)
		{
			/* (one line: on its mark when the wave is one item, else ending
			by it) */
			struct played const *line = NULL;

			for (index = 0; index < played_count; index++)
			{
				if (played[index].spawn == spawn && played[index].due == mark && !is_beep(played[index].clip))
					line = &played[index];
			}
			if (line->tick != mark && line->tick + line->ticks > mark + 1)
				fail("spawn %ld: its call %s at tick %ld, not on its mark %ld nor ending by it", spawn, line->clip,
					line->tick, mark);
		}
		else if (last > mark + 1)
		{
			fail("spawn %ld: its wave's calls end at tick %ld, after its mark %ld", spawn, last, mark);
		}
		if (first < mark - most_earlier - TICKS_PER_SECOND)
			fail("spawn %ld: its calls start at tick %ld, more than allowed before its mark %ld", spawn, first, mark);
		if (!strcmp(check_detail, "minimal") && (beeps || names != 1))
			fail("spawn %ld: %d beeps and %d lines before it, not one line", spawn, beeps, names);
	}
}

static void check_detail_level(char const *detail, long most_earlier)
{
	int index;
	struct played const *call;

	play(detail, 1);
	waves_print_and_check(most_earlier);

	/* (an overshield alone at 1:00: on its mark and its spawn; the clock's
	"ten" said by it; "nine" .. "one" go on; "is up", then the minute) */
	call = played_at(1500, NULL);
	if (!call || strcmp(call->clip, "overshield_in_ten"))
		fail("overshield_in_ten not on its mark (tick 1500): %s", call ? call->clip : "nothing");
	if (played_due(1500, "ten"))
		fail("the clock's ten said with an item's call due then");
	if (strcmp(detail, "minimal") && !left_out_ten)
		fail("no clock ten left out for an item call");
	for (index = 3; strcmp(detail, "minimal") && index <= 9; index++)
	{
		static char const *const numbers[] = { "", "nine", "eight", "seven", "six", "five", "four", "three", "two",
			"one" };

		if (!played_at(1500 + index * TICKS_PER_SECOND, numbers[index]))
			fail("the clock's %s not at tick %ld", numbers[index], 1500 + index * TICKS_PER_SECOND);
	}
	call = played_at(1800, NULL);
	if (!call || strcmp(call->clip, "overshield_up"))
		fail("overshield_up not on its spawn's tick 1800: %s", call ? call->clip : "nothing");
	call = played_due(1815, "one_minute");
	if (!call || call->tick < 1800 + 30)
		fail("one_minute not said after overshield_up");
	else if (call->tick > 1800 + 30 + 2 * TICKS_PER_SECOND)
		fail("one_minute at tick %ld: long after overshield_up", call->tick);
	/* (the rockets alone at 1:30: the same; "five" .. "one" before them but
	for MINIMAL) */
	if (!played_at(2400, "rockets_in_ten"))
		fail("rockets_in_ten not on its mark (tick 2400)");
	if (!played_at(2700, "rockets_up"))
		fail("rockets_up not on its spawn's tick 2700");
	if (!strcmp(detail, "minimal") == !!played_due(2550, "five"))
		fail("the rockets' five %s", strcmp(detail, "minimal") ? "missing" : "said in MINIMAL");

	for (index = 0; index < played_count; index++)
	{
		if (!strcmp(detail, "minimal") && (is_beep(played[index].clip) || !strcmp(played[index].clip, "twenty_seconds") ||
			!strcmp(played[index].clip, "nine")))
		{
			fail("MINIMAL said %s at tick %ld", played[index].clip, played[index].tick);
			break;
		}
		if (strcmp(detail, "verbose") && (!strcmp(played[index].clip, "and") || !strcmp(played[index].clip, "in_ten")))
		{
			fail("%s said at tick %ld outside VERBOSE", played[index].clip, played[index].tick);
			break;
		}
	}

	/* (the wave at 3:00, every item: its spawn's calls from the tick) */
	call = played_at(5400, NULL);
	if (!call || call->spawn != 5400 || !strstr(call->clip, "_up"))
		fail("no is up on the 3:00 wave's tick 5400: %s", call ? call->clip : "nothing");
	if (!strcmp(detail, "minimal"))
	{
		int ups = 0;

		for (index = 0; index < played_count; index++)
			ups += played[index].spawn == 5400 && played[index].due == 5400;
		if (ups != 1)
			fail("MINIMAL's 3:00 wave said %d is up calls, not one", ups);
	}
	if (!strcmp(detail, "verbose"))
	{
		if (!played_due(5400 - 300, "and"))
			fail("VERBOSE's 3:00 wave has no list");
		if (played_due(5400, "overshield_camo_up"))
			fail("VERBOSE said overshield_camo_up");
	}
	if (!strcmp(detail, "standard") && !played_due(5400 - 300, "weapons_and_power_items_in_ten"))
		fail("STANDARD's 3:00 wave not weapons_and_power_items_in_ten");
	if (!strcmp(detail, "standard") && !played_due(5400, "weapons_and_power_items_up"))
		fail("STANDARD's 3:00 wave not weapons_and_power_items_up");
}

int main(int argc, char **argv)
{
	struct played const *call;

	if (argc < 2)
	{
		printf("usage: callouts_check <voice pack folder>\n");
		return 2;
	}
	voice_folder = argv[1];
	if (wav_milliseconds(voice_folder, "overshield_in_ten") <= 0)
	{
		printf("FAIL: no voice pack in %s\n", voice_folder);
		return 1;
	}
	timer_add("ROCKETS", 90, _item_timer_rockets, 1 << _item_timer_rockets);
	timer_add("SNIPER", 120, _item_timer_sniper, 1 << _item_timer_sniper);
	timer_add("OS", 60, _item_timer_overshield, 1 << _item_timer_overshield);
	timer_add("CAMO", 120, _item_timer_camo, 1 << _item_timer_camo);
	timer_add("OS/CAMO", 180, _item_timer_overshield, (1 << _item_timer_overshield) | (1 << _item_timer_camo));

	check_detail_level("minimal", 6 * TICKS_PER_SECOND);
	check_detail_level("standard", 6 * TICKS_PER_SECOND);
	check_detail_level("verbose", 8 * TICKS_PER_SECOND);

	/* (the OS/CAMO spot's item seen at once: said up at once, by its item) */
	check_mixed_class = _item_timer_camo;
	play("verbose", 1);
	call = played_at(5400, NULL);
	if (!call || !strstr(call->clip, "_up"))
		fail("the 3:00 wave's first is up not on tick 5400");
	if (played_due(5400, "powerup_up"))
		fail("powerup_up said for a spot whose item was seen");
	/* (not seen: "powerup is up" half a second after the spawn's others) */
	play("verbose", 0);
	call = played_due(5400, "powerup_up");
	if (!call)
		fail("no powerup_up for a spot whose item was not seen");
	else if (call->tick < 5400 + TICKS_PER_SECOND / 2)
		fail("powerup_up at tick %ld: before its half second wait", call->tick);

	if (failures)
	{
		printf("%d failures\n", failures);
		return 1;
	}
	printf("PASS\n");
	return 0;
}
