/*
AE_INVITE_SHARE.C

The invite link every copy row takes (ae_invite_share.h).
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cseries.h"
#include "cseries/cseries_windows.h"
#include "ae_invite_share.h"
#include "ae_widgets.h"

void platform_log(char const *format, ...);
int p2p_invite_link(char *link, int size);
#ifdef HALO_GAME_BROWSER
int p2p_joined_invite(char *text, int size);
#endif

static unsigned long copied_at;

int ae_invite_share_link(char *link, int size)
{
	if (size <= 0)
		return 0;
	link[0] = 0;
	/* (a test's link: HALO_AE_TEST_INVITE_LINK, for the headless runs that have no internet game to copy from) */
	{
		char const *test = getenv("HALO_AE_TEST_INVITE_LINK");

		if (test && test[0])
		{
			snprintf(link, (size_t)size, "%s", test);
			return 1;
		}
	}
	if (p2p_invite_link(link, size) && link[0])
		return 1;
#ifdef HALO_GAME_BROWSER
	{
		char code[160];

		if (p2p_joined_invite(code, sizeof(code)) && strlen(code) >= 12)
		{
			snprintf(link, (size_t)size, "halo://join/%s", code);
			return 1;
		}
	}
#endif
	link[0] = 0;
	return 0;
}

int ae_invite_share_copy(void)
{
	char link[200];

	if (!ae_invite_share_link(link, sizeof(link)))
		return 0;
	ae_host_clipboard_set(link);
	copied_at = system_milliseconds();
	if (!copied_at)
		copied_at = 1;
	platform_log("ae invite: copied");
	return 1;
}

int ae_invite_share_copied(void)
{
	return copied_at && system_milliseconds() - copied_at < AE_INVITE_COPIED_MS;
}

unsigned long ae_invite_share_notice_until(void)
{
	return ae_invite_share_copied() ? copied_at + AE_INVITE_COPIED_MS : 0;
}
