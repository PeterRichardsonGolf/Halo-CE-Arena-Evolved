#include <stdio.h>

#include "ae_callout_countdown_rules.h"

static int failures;
static int checks;
#define CHECK(c) do { checks++; if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

#define TPS 30L
#define SPEECH (TPS / 2)

/* the call at tick of a game ending at end, and its number */
static int at(long end, long tick, int minimal, int *number)
{
	return ae_callout_countdown_at(end, tick, TPS, SPEECH, minimal, number);
}

/* the calls of a whole game ending at end, from tick 1 on: how many of each */
static void count_calls(long end, int minimal, int counts[8], int numbers[11])
{
	long tick;
	int index;

	for (index = 0; index < 8; index++)
		counts[index] = 0;
	for (index = 0; index < 11; index++)
		numbers[index] = 0;
	for (tick = 1; tick <= end + 10 * TPS; tick++)
	{
		int number;
		int call = at(end, tick, minimal, &number);

		counts[call]++;
		if (call == AE_CALLOUT_COUNTDOWN_NUMBER && number >= 0 && number <= 10)
			numbers[number]++;
	}
}

int main(void)
{
	long end = ae_callout_countdown_end(10, 0, TPS);
	int number = -1;
	int counts[8];
	int numbers[11];
	int index;

	/* the end: minutes of ticks, plus a time left set by hand; none without a time limit */
	CHECK(end == 10 * 60 * TPS);
	CHECK(ae_callout_countdown_end(10, 450, TPS) == 10 * 60 * TPS + 450);
	CHECK(ae_callout_countdown_end(2, -1000, TPS) == 2 * 60 * TPS - 1000);
	CHECK(ae_callout_countdown_end(0, 0, TPS) == AE_CALLOUT_COUNTDOWN_NO_END);
	CHECK(ae_callout_countdown_end(-3, 900, TPS) == AE_CALLOUT_COUNTDOWN_NO_END);
	CHECK(ae_callout_countdown_end(5, 0, 0) == AE_CALLOUT_COUNTDOWN_NO_END);

	/* the marks: the beep on it, the words a speech delay after */
	CHECK(at(end, end - 300 * TPS, 0, &number) == AE_CALLOUT_COUNTDOWN_BEEP_MINUTE);
	CHECK(number == 0);
	CHECK(at(end, end - 300 * TPS + SPEECH, 0, NULL) == AE_CALLOUT_COUNTDOWN_FIVE_MINUTES);
	CHECK(at(end, end - 60 * TPS, 0, NULL) == AE_CALLOUT_COUNTDOWN_BEEP_MINUTE);
	CHECK(at(end, end - 60 * TPS + SPEECH, 0, NULL) == AE_CALLOUT_COUNTDOWN_ONE_MINUTE);
	CHECK(at(end, end - 30 * TPS, 0, NULL) == AE_CALLOUT_COUNTDOWN_BEEP_TICK);
	CHECK(at(end, end - 30 * TPS + SPEECH, 0, NULL) == AE_CALLOUT_COUNTDOWN_THIRTY_SECONDS);
	/* (a tick off the mark: nothing) */
	CHECK(at(end, end - 300 * TPS - 1, 0, NULL) == AE_CALLOUT_COUNTDOWN_NONE);
	CHECK(at(end, end - 300 * TPS + 1, 0, NULL) == AE_CALLOUT_COUNTDOWN_NONE);
	CHECK(at(end, end - 60 * TPS + SPEECH + 1, 0, NULL) == AE_CALLOUT_COUNTDOWN_NONE);
	/* (no other minute is called: 2, 3, 4, 6 minutes left) */
	CHECK(at(end, end - 120 * TPS, 0, NULL) == AE_CALLOUT_COUNTDOWN_NONE);
	CHECK(at(end, end - 240 * TPS + SPEECH, 0, NULL) == AE_CALLOUT_COUNTDOWN_NONE);
	CHECK(at(end, end - 360 * TPS, 0, NULL) == AE_CALLOUT_COUNTDOWN_NONE);
	CHECK(at(end, end - 20 * TPS, 0, NULL) == AE_CALLOUT_COUNTDOWN_NONE);

	/* "ten" .. "one" on the last ten seconds, nothing at the end or after */
	for (index = 10; index >= 1; index--)
	{
		CHECK(at(end, end - index * TPS, 0, &number) == AE_CALLOUT_COUNTDOWN_NUMBER);
		CHECK(number == index);
	}
	CHECK(at(end, end - 11 * TPS, 0, &number) == AE_CALLOUT_COUNTDOWN_NONE);
	CHECK(number == 0);
	CHECK(at(end, end - 5 * TPS + 1, 0, NULL) == AE_CALLOUT_COUNTDOWN_NONE);
	CHECK(at(end, end, 0, NULL) == AE_CALLOUT_COUNTDOWN_NONE);
	CHECK(at(end, end + TPS, 0, NULL) == AE_CALLOUT_COUNTDOWN_NONE);
	CHECK(at(end, end + 30 * TPS, 0, NULL) == AE_CALLOUT_COUNTDOWN_NONE);

	/* no time limit: nothing */
	CHECK(at(AE_CALLOUT_COUNTDOWN_NO_END, 100, 0, NULL) == AE_CALLOUT_COUNTDOWN_NONE);
	CHECK(ae_callout_countdown_at(end, end - 10 * TPS, 0, SPEECH, 0, &number) == AE_CALLOUT_COUNTDOWN_NONE);

	/* MINIMAL: the words only */
	CHECK(at(end, end - 300 * TPS, 1, NULL) == AE_CALLOUT_COUNTDOWN_NONE);
	CHECK(at(end, end - 300 * TPS + SPEECH, 1, NULL) == AE_CALLOUT_COUNTDOWN_FIVE_MINUTES);
	CHECK(at(end, end - 60 * TPS, 1, NULL) == AE_CALLOUT_COUNTDOWN_NONE);
	CHECK(at(end, end - 60 * TPS + SPEECH, 1, NULL) == AE_CALLOUT_COUNTDOWN_ONE_MINUTE);
	CHECK(at(end, end - 30 * TPS + SPEECH, 1, NULL) == AE_CALLOUT_COUNTDOWN_THIRTY_SECONDS);
	CHECK(at(end, end - 10 * TPS, 1, NULL) == AE_CALLOUT_COUNTDOWN_NONE);
	CHECK(at(end, end - 1 * TPS, 1, NULL) == AE_CALLOUT_COUNTDOWN_NONE);

	/* a whole 10-minute game: each beep and words once, each number once */
	count_calls(end, 0, counts, numbers);
	CHECK(counts[AE_CALLOUT_COUNTDOWN_BEEP_MINUTE] == 2);
	CHECK(counts[AE_CALLOUT_COUNTDOWN_BEEP_TICK] == 1);
	CHECK(counts[AE_CALLOUT_COUNTDOWN_FIVE_MINUTES] == 1);
	CHECK(counts[AE_CALLOUT_COUNTDOWN_ONE_MINUTE] == 1);
	CHECK(counts[AE_CALLOUT_COUNTDOWN_THIRTY_SECONDS] == 1);
	CHECK(counts[AE_CALLOUT_COUNTDOWN_NUMBER] == 10);
	for (index = 1; index <= 10; index++)
		CHECK(numbers[index] == 1);
	CHECK(numbers[0] == 0);
	count_calls(end, 1, counts, numbers);
	CHECK(counts[AE_CALLOUT_COUNTDOWN_BEEP_MINUTE] == 0 && counts[AE_CALLOUT_COUNTDOWN_BEEP_TICK] == 0);
	CHECK(counts[AE_CALLOUT_COUNTDOWN_FIVE_MINUTES] == 1 && counts[AE_CALLOUT_COUNTDOWN_ONE_MINUTE] == 1);
	CHECK(counts[AE_CALLOUT_COUNTDOWN_THIRTY_SECONDS] == 1 && counts[AE_CALLOUT_COUNTDOWN_NUMBER] == 0);

	/* a 5-minute game has no "five minutes" (its mark is the start); a 1-minute game no "one minute" */
	end = ae_callout_countdown_end(5, 0, TPS);
	count_calls(end, 0, counts, numbers);
	CHECK(counts[AE_CALLOUT_COUNTDOWN_FIVE_MINUTES] == 0);
	CHECK(counts[AE_CALLOUT_COUNTDOWN_BEEP_MINUTE] == 1);
	CHECK(counts[AE_CALLOUT_COUNTDOWN_ONE_MINUTE] == 1);
	CHECK(counts[AE_CALLOUT_COUNTDOWN_THIRTY_SECONDS] == 1);
	CHECK(counts[AE_CALLOUT_COUNTDOWN_NUMBER] == 10);
	end = ae_callout_countdown_end(1, 0, TPS);
	count_calls(end, 0, counts, numbers);
	CHECK(counts[AE_CALLOUT_COUNTDOWN_FIVE_MINUTES] == 0 && counts[AE_CALLOUT_COUNTDOWN_ONE_MINUTE] == 0);
	CHECK(counts[AE_CALLOUT_COUNTDOWN_BEEP_MINUTE] == 0);
	CHECK(counts[AE_CALLOUT_COUNTDOWN_THIRTY_SECONDS] == 1 && counts[AE_CALLOUT_COUNTDOWN_NUMBER] == 10);

	/* a time left set by hand to 70 s at tick 1000: the end moves, "one minute" 10 s later */
	end = 1000 + 70 * TPS;
	CHECK(at(end, 1000 + 10 * TPS, 0, NULL) == AE_CALLOUT_COUNTDOWN_BEEP_MINUTE);
	CHECK(at(end, 1000 + 10 * TPS + SPEECH, 0, NULL) == AE_CALLOUT_COUNTDOWN_ONE_MINUTE);
	CHECK(at(end, 1000 + 40 * TPS, 0, NULL) == AE_CALLOUT_COUNTDOWN_BEEP_TICK);
	CHECK(at(end, 1000 + 69 * TPS, 0, &number) == AE_CALLOUT_COUNTDOWN_NUMBER && number == 1);
	/* (an end off the second: the count follows the end, not the game's seconds) */
	end = 1007 + 70 * TPS;
	CHECK(at(end, 1007 + 60 * TPS, 0, &number) == AE_CALLOUT_COUNTDOWN_NUMBER && number == 10);
	CHECK(at(end, 1000 + 60 * TPS, 0, NULL) == AE_CALLOUT_COUNTDOWN_NONE);

	printf("%d checks, %d failures\n", checks, failures);
	return failures ? 1 : 0;
}
