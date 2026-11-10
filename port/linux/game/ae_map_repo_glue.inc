/*
AE_MAP_REPO_GLUE.INC

(0.7.8) Included at the end of browser_screen.c (one AE hook line): the offer of a Custom Edition map from the
community map repositories, which the classic Online Games screen draws, drawn by AE's dialogs when AE's menus are on
(ae_map_repo.c). The screen is opened over the main menu as before (browser_screen_open_map_offer: a game left for
want of the map), but not drawn and taking no buttons itself (ae_offer_mode); AE's dialog answers go to the same
offer_button the classic buttons do, so the asking, the download, its checks against the host's version, and the game
joined again are the classic code's, unchanged.
*/

#include "ae_map_repo.h"

/* one step of the flow, as browser_screen_process takes it (without its game list, which the offer does not show) */
static void ae_offer_step(void)
{
	if (browser_screen.offer == _offer_downloading)
		follow_download();
	else if (browser_screen.connecting && browser_screen.offer == _offer_none)
		wait_for_host();
}

/* the classic process, in AE's mode: the flow goes on and no button is the classic screen's (AE's dialogs answer) */
static boolean ae_offer_drain(void)
{
	struct event_record event;

	ae_offer_step();
	while (get_next_event(&event, NONE))
		;
	event_manager_flush();
	return TRUE;
}

int browser_screen_ae_offer(void)
{
	return browser_screen.active && ae_offer_mode;
}

void browser_screen_ae_offer_step(void)
{
	if (browser_screen_ae_offer())
		ae_offer_step();
}

void browser_screen_ae_offer_view(
	struct ae_map_repo_view *view)
{
	struct map_download_progress const *progress = &browser_screen.offer_progress;

	memset(view, 0, sizeof(*view));
	if (!browser_screen_ae_offer())
		return;
	snprintf(view->file, sizeof(view->file), "%s", browser_screen.offer_display);
	switch (browser_screen.offer)
	{
	case _offer_asking:
		view->phase = AE_REPO_ASKING;
		break;
	case _offer_downloading:
		view->phase = AE_REPO_DOWNLOADING;
		switch (progress->phase)
		{
		case MAP_DOWNLOAD_RECEIVING:
			if (progress->total)
				snprintf(view->line1, sizeof(view->line1), "%.1f of %.1f MB", progress->received / 1048576.0,
					progress->total / 1048576.0);
			else
				snprintf(view->line1, sizeof(view->line1), "%.1f MB", progress->received / 1048576.0);
			break;
		case MAP_DOWNLOAD_UNPACKING:
			snprintf(view->line1, sizeof(view->line1), "Unpacking\xE2\x80\xA6 %.1f MB", progress->received / 1048576.0);
			break;
		case MAP_DOWNLOAD_CHECKING:
			snprintf(view->line1, sizeof(view->line1), "Checking\xE2\x80\xA6");
			break;
		default:
			snprintf(view->line1, sizeof(view->line1), "Connecting to the map repository\xE2\x80\xA6");
			break;
		}
		break;
	case _offer_failed:
		view->phase = AE_REPO_FAILED;
		snprintf(view->message, sizeof(view->message), "%s", progress->message);
		break;
	case _offer_stock:
		view->phase = AE_REPO_STOCK;
		snprintf(view->line1, sizeof(view->line1), "%s", browser_screen.offer_stock_first);
		snprintf(view->line2, sizeof(view->line2), "%s", browser_screen.offer_stock_second);
		break;
	default:
		if (browser_screen.connecting)
			view->phase = AE_REPO_JOINING;
		else
		{
			view->phase = AE_REPO_ENDED;
			snprintf(view->message, sizeof(view->message), "%s", browser_screen.status);
		}
		break;
	}
}

void browser_screen_ae_offer_answer(
	int answer)
{
	if (!browser_screen_ae_offer())
		return;
	if (answer == AE_REPO_CLOSE)
	{
		browser_screen.offer = _offer_none;
		browser_screen.connecting = FALSE;
		browser_screen.rejoin_pending = FALSE;
		browser_screen.active = FALSE;
		map_download_reset();
		return;
	}
	switch (browser_screen.offer)
	{
	case _offer_asking:
		offer_button(answer == AE_REPO_YES ? _gamepad_analog_button_a : _gamepad_analog_button_b);
		break;
	case _offer_downloading:
		if (answer == AE_REPO_NO)
			offer_button(_gamepad_analog_button_b);
		break;
	case _offer_failed:
	case _offer_stock:
		offer_button(_gamepad_analog_button_a);
		break;
	default:
		/* (B while the host is waited for: the wait given up) */
		if (answer == AE_REPO_NO && browser_screen.connecting)
			browser_screen.connecting = FALSE;
		break;
	}
}
