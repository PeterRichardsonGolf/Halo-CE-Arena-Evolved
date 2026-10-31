#include <math.h>
#include <stdio.h>
#include "ae_lobby_countdown.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)
#define NEAR(a, b) (fabsf((a) - (b)) < 0.002f)

static short samples[AE_BEEP_FRAMES];

int main(void)
{
	struct ae_countdown c;
	unsigned long t;
	short n;

	/* idle: no countdown, NONE changes nothing; the bar empty */
	ae_countdown_reset(&c);
	CHECK(!ae_countdown_running(&c));
	CHECK(ae_countdown_update(&c, AE_COUNTDOWN_SECONDS_NONE, 0) == AE_COUNTDOWN_NOTHING);
	CHECK(ae_countdown_update(&c, AE_COUNTDOWN_SECONDS_STARTED, 0) == AE_COUNTDOWN_NOTHING);
	CHECK(ae_countdown_left(&c, 0) == 0.0f);

	/* the host's START GAME at 5999 ms: 5 shown at once, the bar full (5.999 of 6 s); running before any message */
	ae_countdown_begin(&c, 5999, 1000);
	CHECK(ae_countdown_running(&c) && c.phase == AE_COUNTDOWN_PENDING && c.seconds == 5 && c.whole == 6);
	CHECK(NEAR(ae_countdown_left(&c, 1000), 5.999f / 6.0f));
	/* (no number yet: still pending, the message is a frame away) */
	CHECK(ae_countdown_update(&c, AE_COUNTDOWN_SECONDS_NONE, 1016) == AE_COUNTDOWN_NOTHING && ae_countdown_running(&c));
	/* the server's 5: the first number, its beep; then 4, 3, 2, 1 a second apart, each once */
	CHECK(ae_countdown_update(&c, 5, 1032) == AE_COUNTDOWN_NUMBER && c.phase == AE_COUNTDOWN_COUNTING);
	CHECK(ae_countdown_update(&c, 5, 1500) == AE_COUNTDOWN_NOTHING);
	/* (smooth: half a second into 5, 5.499 s of 6 left) */
	CHECK(NEAR(ae_countdown_left(&c, 1532), 5.499f / 6.0f));
	/* (a late message: the bar stalls at the number's own second, never below it) */
	CHECK(NEAR(ae_countdown_left(&c, 3000), 5.0f / 6.0f));
	for (n = 4, t = 2040; n >= 1; n--, t += 1010)
	{
		CHECK(ae_countdown_update(&c, n, t) == AE_COUNTDOWN_NUMBER && c.seconds == n);
		CHECK(ae_countdown_update(&c, n, t + 500) == AE_COUNTDOWN_NOTHING);
	}
	/* 0: GO, the bar's last second; GO is not the hold, until 1.5 s of 0 */
	CHECK(ae_countdown_update(&c, 0, t) == AE_COUNTDOWN_ZERO && c.phase == AE_COUNTDOWN_GO && ae_countdown_running(&c));
	CHECK(NEAR(ae_countdown_left(&c, t), 0.999f / 6.0f));
	CHECK(ae_countdown_left(&c, t + 999) == 0.0f);
	CHECK(ae_countdown_update(&c, 0, t + 1000) == AE_COUNTDOWN_NOTHING && c.phase == AE_COUNTDOWN_GO);
	/* the game under way (the client past its pregame: its seconds cleared): started, not cancelled */
	CHECK(ae_countdown_update(&c, AE_COUNTDOWN_SECONDS_STARTED, t + 1100) == AE_COUNTDOWN_STARTED);
	CHECK(!ae_countdown_running(&c));

	/* the hold: 0 for 1.5 s (a joiner still loading the map), once; the bar gone; NONE then is a cancel */
	ae_countdown_reset(&c);
	CHECK(ae_countdown_update(&c, 1, 0) == AE_COUNTDOWN_NUMBER);
	CHECK(ae_countdown_update(&c, 0, 1000) == AE_COUNTDOWN_ZERO);
	CHECK(ae_countdown_update(&c, 0, 2499) == AE_COUNTDOWN_NOTHING);
	CHECK(ae_countdown_update(&c, 0, 2500) == AE_COUNTDOWN_HOLD && c.phase == AE_COUNTDOWN_HELD);
	CHECK(ae_countdown_update(&c, 0, 9000) == AE_COUNTDOWN_NOTHING && ae_countdown_running(&c));
	CHECK(ae_countdown_left(&c, 9000) == 0.0f);
	CHECK(ae_countdown_update(&c, AE_COUNTDOWN_SECONDS_NONE, 9500) == AE_COUNTDOWN_CANCELLED && !ae_countdown_running(&c));

	/* a joiner: its first number starts it (the bar's whole from it: one that came in at 3 drains from 4 s); NONE after a
	number is the host's cancel (or the server's) */
	ae_countdown_reset(&c);
	CHECK(ae_countdown_update(&c, 3, 100) == AE_COUNTDOWN_NUMBER && c.whole == 4);
	CHECK(NEAR(ae_countdown_left(&c, 100), 3.999f / 4.0f));
	CHECK(ae_countdown_update(&c, AE_COUNTDOWN_SECONDS_NONE, 600) == AE_COUNTDOWN_CANCELLED);
	CHECK(c.phase == AE_COUNTDOWN_IDLE);
	/* time added (another machine's stock "slower"): the new number shows, the whole grows with it */
	ae_countdown_reset(&c);
	CHECK(ae_countdown_update(&c, 2, 0) == AE_COUNTDOWN_NUMBER && c.whole == 3);
	CHECK(ae_countdown_update(&c, 7, 500) == AE_COUNTDOWN_NUMBER && c.whole == 8 && c.seconds == 7);

	/* the host cancels: START GAME at once; its client's last number (stale until the NONE comes back) is no new
	countdown; NONE ends it quietly */
	ae_countdown_begin(&c, 5999, 0);
	CHECK(ae_countdown_update(&c, 5, 10) == AE_COUNTDOWN_NUMBER);
	CHECK(ae_countdown_update(&c, 4, 1020) == AE_COUNTDOWN_NUMBER);
	ae_countdown_cancel(&c, 1300);
	CHECK(!ae_countdown_running(&c) && ae_countdown_left(&c, 1300) == 0.0f);
	CHECK(ae_countdown_update(&c, 4, 1316) == AE_COUNTDOWN_NOTHING && !ae_countdown_running(&c));
	CHECK(ae_countdown_update(&c, AE_COUNTDOWN_SECONDS_NONE, 1332) == AE_COUNTDOWN_NOTHING && c.phase == AE_COUNTDOWN_IDLE);
	/* (then a new START GAME counts again) */
	ae_countdown_begin(&c, 5999, 2000);
	CHECK(ae_countdown_update(&c, 5, 2016) == AE_COUNTDOWN_NUMBER);
	/* (a NONE that never comes back: the stale number is let go after a while, then numbers count again) */
	ae_countdown_cancel(&c, 3000);
	CHECK(ae_countdown_update(&c, 5, 3000 + AE_COUNTDOWN_PENDING_MS - 1) == AE_COUNTDOWN_NOTHING);
	CHECK(ae_countdown_update(&c, 5, 3000 + AE_COUNTDOWN_PENDING_MS) == AE_COUNTDOWN_NOTHING && c.phase == AE_COUNTDOWN_IDLE);
	CHECK(ae_countdown_update(&c, 5, 6000) == AE_COUNTDOWN_NUMBER);

	/* the host's start the server never counted (no number in 2.5 s): cancelled; a STARTED while pending: started */
	ae_countdown_begin(&c, 5999, 0);
	CHECK(ae_countdown_update(&c, AE_COUNTDOWN_SECONDS_NONE, AE_COUNTDOWN_PENDING_MS - 1) == AE_COUNTDOWN_NOTHING);
	CHECK(ae_countdown_update(&c, AE_COUNTDOWN_SECONDS_NONE, AE_COUNTDOWN_PENDING_MS) == AE_COUNTDOWN_CANCELLED);
	ae_countdown_begin(&c, 5999, 0);
	CHECK(ae_countdown_update(&c, AE_COUNTDOWN_SECONDS_STARTED, 10) == AE_COUNTDOWN_STARTED);
	/* (a short count: 1999 ms shows 1, its bar from 2 s) */
	ae_countdown_begin(&c, 1999, 0);
	CHECK(c.seconds == 1 && c.whole == 2);

	/* the beep rule (owner 10-28): no PRE-GAME COUNTDOWN, a tick a number and GO at 0; with it, a soft tick a number
	and nothing at 0 (the game's own 3-2-1 beeps); none for NONE */
	CHECK(ae_countdown_beep(5, 0) == AE_BEEP_TICK && ae_countdown_beep(1, 0) == AE_BEEP_TICK);
	CHECK(ae_countdown_beep(0, 0) == AE_BEEP_GO);
	CHECK(ae_countdown_beep(5, 1) == AE_BEEP_SOFT && ae_countdown_beep(1, 1) == AE_BEEP_SOFT);
	CHECK(ae_countdown_beep(0, 1) == AE_BEEP_NONE);
	CHECK(ae_countdown_beep(-1, 0) == AE_BEEP_NONE && ae_countdown_beep(-1, 1) == AE_BEEP_NONE);

	/* the tones: their lengths, silent ends (the fades), GO louder than SOFT, nothing past maximum */
	{
		unsigned long frames, index;
		int peak_tick = 0, peak_soft = 0;

		frames = ae_countdown_tone(AE_BEEP_TICK, samples, AE_BEEP_FRAMES, AE_BEEP_RATE);
		CHECK(frames == AE_BEEP_RATE * 90 / 1000);
		CHECK(samples[0] == 0 && samples[frames - 1] == 0);
		for (index = 0; index < frames; index++)
			peak_tick = samples[index] > peak_tick ? samples[index] : peak_tick;
		frames = ae_countdown_tone(AE_BEEP_SOFT, samples, AE_BEEP_FRAMES, AE_BEEP_RATE);
		CHECK(frames == AE_BEEP_RATE * 60 / 1000);
		for (index = 0; index < frames; index++)
			peak_soft = samples[index] > peak_soft ? samples[index] : peak_soft;
		CHECK(peak_tick > 10000 && peak_tick < 32767 && peak_soft > 0 && peak_soft * 2 < peak_tick);
		CHECK(ae_countdown_tone(AE_BEEP_GO, samples, AE_BEEP_FRAMES, AE_BEEP_RATE) == AE_BEEP_RATE * 220 / 1000);
		CHECK(ae_countdown_tone(AE_BEEP_GO, samples, 100, AE_BEEP_RATE) == 100);
		CHECK(ae_countdown_tone(AE_BEEP_NONE, samples, AE_BEEP_FRAMES, AE_BEEP_RATE) == 0);
		CHECK(ae_countdown_tone(AE_BEEP_TICK, NULL, AE_BEEP_FRAMES, AE_BEEP_RATE) == 0);
	}

	if (failures)
		printf("%d failure(s)\n", failures);
	else
		printf("ok\n");
	return failures != 0;
}
