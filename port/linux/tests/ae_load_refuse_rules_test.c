/* ae_load_refuse_rules_test.c: a refused map's rotation entry and the words for a failed MCC resource */
#include <stdio.h>
#include <string.h>

#include "ae_load_refuse_rules.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

int main(void)
{
	struct ae_lr_state state;
	double soonest;
	char text[512];

	ae_lr_reset(&state);
	CHECK(!ae_lr_refused(&state, 0));
	CHECK(ae_lr_waiting(&state, 4, 10.0, &soonest) == 0 && soonest < 0.0);

	/* the first refusal leaves the entry out at once, for 5 minutes */
	CHECK(ae_lr_note_failure(&state, 1, 100.0) == 300.0);
	CHECK(ae_lr_refused(&state, 1) && !ae_lr_refused(&state, 0));
	CHECK(!ae_lr_due(&state, 1, 399.0) && ae_lr_due(&state, 1, 400.0));
	CHECK(ae_lr_waiting(&state, 4, 150.0, &soonest) == 1 && soonest == 250.0);

	/* released for its retry, refused again: twice as long, then 20, 40, 60 minutes at most */
	ae_lr_release(&state, 1);
	CHECK(!ae_lr_refused(&state, 1));
	CHECK(ae_lr_note_failure(&state, 1, 500.0) == 600.0);
	CHECK(ae_lr_note_failure(&state, 1, 500.0) == 1200.0);
	CHECK(ae_lr_note_failure(&state, 1, 500.0) == 2400.0);
	CHECK(ae_lr_note_failure(&state, 1, 500.0) == 3600.0);
	CHECK(ae_lr_note_failure(&state, 1, 500.0) == 3600.0);
	for (int i = 0; i < 5000; i++)
		ae_lr_note_failure(&state, 1, 500.0);
	CHECK(ae_lr_note_failure(&state, 1, 500.0) == 3600.0);

	/* an entry that played is forgotten */
	ae_lr_note_success(&state, 1);
	CHECK(!ae_lr_refused(&state, 1));
	CHECK(ae_lr_note_failure(&state, 1, 0.0) == 300.0);

	/* every entry refused: all wait, the soonest is named */
	ae_lr_reset(&state);
	ae_lr_note_failure(&state, 0, 0.0);
	ae_lr_note_failure(&state, 1, 100.0);
	CHECK(ae_lr_waiting(&state, 2, 200.0, &soonest) == 2 && soonest == 100.0);
	CHECK(ae_lr_waiting(&state, 2, 350.0, &soonest) == 2 && soonest == 0.0);

	/* bad indexes change nothing */
	CHECK(ae_lr_note_failure(&state, -1, 0.0) == 0.0);
	CHECK(ae_lr_note_failure(&state, AE_LR_ENTRIES, 0.0) == 0.0);
	CHECK(!ae_lr_refused(&state, -1) && !ae_lr_due(&state, AE_LR_ENTRIES, 1e9));

	/* the wait is said once a minute, the first time at once */
	ae_lr_reset(&state);
	CHECK(ae_lr_log_due(&state, 0.0));
	CHECK(!ae_lr_log_due(&state, 30.0));
	CHECK(!ae_lr_log_due(&state, 59.9));
	CHECK(ae_lr_log_due(&state, 60.0));
	CHECK(!ae_lr_log_due(&state, 61.0));

	/* the words name the file that failed */
	CHECK(!strcmp(ae_lr_resource_name(1), "bitmaps.map"));
	CHECK(!strcmp(ae_lr_resource_name(2), "sounds.map"));
	ae_lr_resource_failure_text(text, sizeof(text), 1, "/x/halo1/maps/bitmaps.map", 85614690ul,
		"levels\\a\\b_3 (offset 16, 64 bytes)", 12ul, "gephyrophobia@mcc");
	CHECK(strstr(text, "bitmaps.map (bitmaps.map, 85614690 bytes) does not satisfy gephyrophobia@mcc"));
	CHECK(strstr(text, "12 references fail, the first is levels\\a\\b_3"));
	CHECK(strstr(text, "update MCC in Steam"));
	CHECK(!strstr(text, "sounds.map"));
	ae_lr_resource_failure_text(text, sizeof(text), 2, "C:\\mcc\\halo1\\maps\\custom_edition\\sounds.map", 41718640ul,
		NULL, 0ul, NULL);
	CHECK(strstr(text, "sounds.map (sounds.map, 41718640 bytes) does not satisfy the map."));
	CHECK(!strstr(text, "bitmaps.map") && !strstr(text, "reference"));
	ae_lr_resource_failure_text(text, 24, 2, "s.map", 1ul, "x", 1ul, "m"); /* a short buffer is cut, never overrun */
	CHECK(strlen(text) < 24);
	ae_lr_resource_failure_text(NULL, 0, 2, NULL, 0, NULL, 0, NULL);

	if (!failures)
		printf("ok\n");
	return failures ? 1 : 0;
}
