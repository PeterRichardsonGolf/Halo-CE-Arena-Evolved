#include <stdio.h>
#include "ae_lobby_profile_choices.h"

static int failures;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); failures++; } } while (0)

int main(void)
{
	int profiles[] = { 10, 20, 30 };
	int used[] = { 20 };
	int choices[4];
	short count, focus;

	/* Another local player's profile is left out; GUEST is always last. */
	count = ae_lobby_profile_choices(profiles, 3, used, 1, 30, 0, choices, 4, &focus);
	CHECK(count == 3 && choices[0] == 10 && choices[1] == 30 && choices[2] == AE_LOBBY_PROFILE_GUEST);
	CHECK(focus == 1);
	/* Today's fallback: a taken or missing profile, and an existing guest, focus GUEST. */
	count = ae_lobby_profile_choices(profiles, 3, used, 1, 20, 0, choices, 4, &focus);
	CHECK(count == 3 && focus == 2);
	count = ae_lobby_profile_choices(profiles, 3, used, 1, 30, 1, choices, 4, &focus);
	CHECK(count == 3 && focus == 2);
	count = ae_lobby_profile_choices(profiles, 3, used, 1, -1, 0, choices, 4, &focus);
	CHECK(count == 3 && focus == 2);
	/* The room includes GUEST even when saved profiles have to be cut. */
	count = ae_lobby_profile_choices(profiles, 3, NULL, 0, 30, 0, choices, 2, &focus);
	CHECK(count == 2 && choices[0] == 10 && choices[1] == AE_LOBBY_PROFILE_GUEST && focus == 1);
	count = ae_lobby_profile_choices(NULL, 0, NULL, 0, -1, 0, choices, 4, &focus);
	CHECK(count == 1 && choices[0] == AE_LOBBY_PROFILE_GUEST && focus == 0);
	CHECK(ae_lobby_profile_choices(profiles, 3, used, 1, 10, 0, NULL, 4, &focus) == 0);
	CHECK(ae_lobby_profile_choices(profiles, 3, used, 1, 10, 0, choices, 0, &focus) == 0);
	/* Two pads choosing at once: the profile the first one picked drops out of the second's list, and the focus stays on
	its own profile, or on the place of the one that went. */
	{
		int both[] = { 10, 20, 30 };
		int first_picked[] = { 20 };
		int second[4];
		short second_count, second_focus;

		second_count = ae_lobby_profile_choices(both, 3, NULL, 0, 10, 0, second, 4, &second_focus);
		CHECK(second_count == 4 && second[1] == 20);
		CHECK(ae_lobby_profile_refocus(second, second_count, 30, 2) == 2);
		second_count = ae_lobby_profile_choices(both, 3, first_picked, 1, 10, 0, second, 4, &second_focus);
		CHECK(second_count == 3 && second[0] == 10 && second[1] == 30 && second[2] == AE_LOBBY_PROFILE_GUEST);
		/* its own focus is still found; the taken one's place passes to the next; the last place clamps to GUEST */
		CHECK(ae_lobby_profile_refocus(second, second_count, 30, 2) == 1);
		CHECK(ae_lobby_profile_refocus(second, second_count, 20, 1) == 1);
		CHECK(ae_lobby_profile_refocus(second, second_count, 20, 3) == 2);
		CHECK(ae_lobby_profile_refocus(second, second_count, AE_LOBBY_PROFILE_GUEST, 0) == 2);
		CHECK(ae_lobby_profile_refocus(second, 0, 20, 2) == 0 && ae_lobby_profile_refocus(NULL, 3, 20, 2) == 0);
	}
	if (failures)
		printf("%d failures\n", failures);
	return failures ? 1 : 0;
}
