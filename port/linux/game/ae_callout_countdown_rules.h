/*
AE_CALLOUT_COUNTDOWN_RULES.H

CALLOUTS: ITEMS + COUNTDOWN (game.callouts "items_countdown"): the talking clock that calls the time left of the
gametype's time limit, beside ITEMS + CLOCK's count of the game time. These pure functions are its rules
(port/linux/tests/ae_callout_countdown_rules_test.c); source/game/callouts.c plans the calls they name.

- The end: the game-time tick the time limit ends the game at, its minutes plus the ticks a time left set by hand adds
  (DEV MODE's time, a server's playlist); none without a time limit.
- The marks: 5 minutes, 1 minute and 30 seconds before the end, a beep on the mark (the minute's beep at 5 minutes and
  1 minute, the tick's at 30 seconds) and the words a speech delay after it ("five minutes", "one minute", "thirty
  seconds left"); then "ten" .. "one" on the last ten seconds. A mark at or past the start (a game of 5 minutes or
  less has no "five minutes") is not called.
- MINIMAL: the marks' words only, no beeps and no "ten" .. "one".
- Nothing after the end (sudden death, overtime).
*/

#ifndef AE_CALLOUT_COUNTDOWN_RULES_H
#define AE_CALLOUT_COUNTDOWN_RULES_H

/* no end: no time limit */
#define AE_CALLOUT_COUNTDOWN_NO_END (-1L)

enum ae_callout_countdown_call
{
	AE_CALLOUT_COUNTDOWN_NONE = 0,
	AE_CALLOUT_COUNTDOWN_BEEP_MINUTE,	/* on the 5 minutes and 1 minute marks */
	AE_CALLOUT_COUNTDOWN_BEEP_TICK,	/* on the 30 seconds mark */
	AE_CALLOUT_COUNTDOWN_FIVE_MINUTES,	/* the marks' words, a speech delay after their beep */
	AE_CALLOUT_COUNTDOWN_ONE_MINUTE,
	AE_CALLOUT_COUNTDOWN_THIRTY_SECONDS,
	AE_CALLOUT_COUNTDOWN_NUMBER	/* "ten" .. "one": the number in *number */
};

/* the tick the time limit ends the game at: minutes of ticks_per_second, plus extra_ticks; AE_CALLOUT_COUNTDOWN_NO_END
for no time limit (minutes 0 or less) */
long ae_callout_countdown_end(long time_limit_minutes, long extra_ticks, long ticks_per_second);

/* the call with its moment at tick, for a game ending at end (enum ae_callout_countdown_call); speech_ticks after a
mark its words; minimal: MINIMAL's; *number (when not NULL) the number of AE_CALLOUT_COUNTDOWN_NUMBER, else 0 */
int ae_callout_countdown_at(long end, long tick, long ticks_per_second, long speech_ticks, int minimal, int *number);

#endif /* AE_CALLOUT_COUNTDOWN_RULES_H */
