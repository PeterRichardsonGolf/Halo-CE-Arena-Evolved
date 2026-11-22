/*
AE_MAP_REPO.C

The community map repository's offer as AE's dialogs (0.7.8; ae_map_repo.h). browser_screen.c's offer flow runs the
asking, the download and the game joined again; this follows its phase each frame and shows the attention dialog
(ae_screen_attention.h) that fits: DOWNLOAD / NO, the download's progress with CANCEL, why it failed, the game's own
maps' note, JOINING with CANCEL. Each button is the classic offer's A or B. "Always" and "never" are
SETTINGS > ONLINE > MAPS > MAP DOWNLOADS (network.map_downloads), which the offer follows as the classic screen does.
*/

#include <stdio.h>
#include <string.h>

#include "cseries.h"
#include "ae_map_repo.h"
#include "ae_screen_attention.h"
#include "ae_setup_flow.h"

void platform_log(char const *format, ...);

#ifdef HALO_GAME_BROWSER

static struct
{
	/* the phase the dialog shown is for, AE_REPO_NONE: none shown */
	int phase;
	/* the player answered this phase's dialog: it is not opened again for the same phase */
	int answered;
	/* the player chose no or cancel: the offer's end says nothing more */
	int declined;
	char file[48];
} repo;

static void density_of(struct ae_density *density)
{
	struct ae_flow_screen flow;

	memset(&flow, 0, sizeof(flow));
	ae_flow_full_density(&flow);
	*density = flow.density;
}

static void picked(short button, void *context)
{
	(void)context;
	repo.answered = 1;
	switch (repo.phase)
	{
	case AE_REPO_ASKING:
		repo.declined = button != 0;
		browser_screen_ae_offer_answer(button == 0 ? AE_REPO_YES : AE_REPO_NO);
		break;
	case AE_REPO_DOWNLOADING:
	case AE_REPO_JOINING:
		repo.declined = 1;
		browser_screen_ae_offer_answer(AE_REPO_NO);
		break;
	default:
		browser_screen_ae_offer_answer(AE_REPO_YES);
		break;
	}
}

static void show(struct ae_map_repo_view const *view)
{
	struct ae_attention_spec spec;
	char body[300], item[64];

	memset(&spec, 0, sizeof(spec));
	spec.danger = -1;
	spec.back = 0;
	spec.button_count = 1;
	spec.picked = picked;
	spec.context = &repo;
	density_of(&spec.density);
	snprintf(item, sizeof(item), "%s.map", view->file);
	switch (view->phase)
	{
	case AE_REPO_ASKING:
		snprintf(body, sizeof(body), "The game you joined plays a map you don't have. Download %s from the community "
			"map repository, then join again?", item);
		spec.title = "MISSING MAP";
		spec.body = body;
		spec.buttons[0] = "DOWNLOAD";
		spec.buttons[1] = "NO";
		spec.button_count = 2;
		spec.back = 1;
		spec.back_label = "No";
		spec.note = "To always download, or never be asked: SETTINGS, ONLINE, MAPS, MAP DOWNLOADS.";
		break;
	case AE_REPO_DOWNLOADING:
		spec.title = "DOWNLOADING MAP";
		spec.body = "From the community map repository. The game is joined again when the map is in place.";
		spec.item_title = item;
		spec.item_line1 = view->line1;
		spec.item_line2 = "The map is checked against the host's";
		spec.item_tile = "DOWNLOADING";
		spec.buttons[0] = "CANCEL";
		spec.back_label = "Cancel";
		break;
	case AE_REPO_FAILED:
		snprintf(body, sizeof(body), "%s.map was not downloaded: %s", view->file,
			view->message[0] ? view->message : "the repositories did not answer");
		spec.title = "DOWNLOAD FAILED";
		spec.body = body;
		spec.buttons[0] = "OK";
		spec.back_label = "Back";
		break;
	case AE_REPO_STOCK:
		snprintf(body, sizeof(body), "%s %s", view->line1, view->line2);
		spec.title = "INSTALL THIS MAP YOURSELF";
		spec.body = body;
		spec.buttons[0] = "OK";
		spec.back_label = "Back";
		break;
	case AE_REPO_JOINING:
		spec.title = "JOINING";
		spec.body = "The map is in place. Joining the game again.";
		spec.item_title = item;
		spec.item_line1 = "Downloaded";
		spec.item_line2 = "";
		spec.item_tile = "READY";
		spec.buttons[0] = "CANCEL";
		spec.back_label = "Cancel";
		break;
	default:
		return;
	}
	if (!ae_attention_open(&spec))
		platform_log("map repository: the dialog could not open");
}

void ae_map_repo_tick(void)
{
	struct ae_map_repo_view view;
	int mine;

	browser_screen_ae_offer_step();
	browser_screen_ae_offer_view(&view);
	mine = ae_attention_context() == &repo;
	if (view.phase == AE_REPO_NONE)
	{
		if (mine)
			ae_attention_close(&repo);
		memset(&repo, 0, sizeof(repo));
		return;
	}
	if (view.phase == AE_REPO_ENDED)
	{
		char message[200];

		/* (it ended without a join: the status says why, unless the player gave it up) */
		snprintf(message, sizeof(message), "%s", view.message);
		if (mine)
			ae_attention_close(&repo);
		browser_screen_ae_offer_answer(AE_REPO_CLOSE);
		if (message[0] && !repo.declined)
		{
			struct ae_attention_spec spec;

			memset(&spec, 0, sizeof(spec));
			spec.title = "COULDN'T JOIN";
			spec.body = message;
			spec.buttons[0] = "OK";
			spec.button_count = 1;
			spec.danger = -1;
			spec.back = 0;
			spec.back_label = "Back";
			density_of(&spec.density);
			ae_attention_open(&spec);
		}
		memset(&repo, 0, sizeof(repo));
		return;
	}
	if (view.phase != repo.phase)
	{
		/* (the dialog of the phase before is replaced) */
		repo.phase = view.phase;
		repo.answered = 0;
		snprintf(repo.file, sizeof(repo.file), "%s", view.file);
		platform_log("map repository: %s.map: phase %d", view.file, view.phase);
		show(&view);
		return;
	}
	if (view.phase == AE_REPO_DOWNLOADING && mine)
		ae_attention_set_item(view.line1, "DOWNLOADING");
	/* (closed by something else, not answered: shown again) */
	else if (!mine && !repo.answered)
		show(&view);
}

#else

void ae_map_repo_tick(void)
{
}

#endif
