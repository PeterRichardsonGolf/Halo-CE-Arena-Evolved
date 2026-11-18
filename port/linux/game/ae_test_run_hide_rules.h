/*
AE_TEST_RUN_HIDE_RULES.H

TEST RUN card hiding: a short press toggles it and a long press hides only
while held. Pure: no engine headers.
*/

#ifndef AE_TEST_RUN_HIDE_RULES_H
#define AE_TEST_RUN_HIDE_RULES_H

#define AE_TEST_RUN_HIDE_TOGGLE_MS 300UL

struct ae_test_run_hide
{
	int toggled;
	int held;
	int swallow;
	unsigned long pressed_at;
};

/* the card is hidden after this input sample; reveal clears a toggle and
shows it for this sample */
int ae_test_run_hide_update(struct ae_test_run_hide *state, int held, unsigned long now, int reveal);

#endif
