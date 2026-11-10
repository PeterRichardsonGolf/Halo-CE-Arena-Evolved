/*
AE_MCC_OFFER.C

MCC FOUND / MCC NOT FOUND over the main menu (ae_mcc_offer.h), Steam's MCC only: the detection and the settings are
ae_mcc_platform.c's. LOOK AGAIN runs the detection once more and opens the dialog that is true then.
*/

#include <stdlib.h>
#include <string.h>

#include "cseries.h"
#include "../src/ae_mcc.h"
#include "ae_mcc_offer.h"
#include "ae_screen_attention.h"

void platform_log(char const *format, ...);

static int offered;
static struct ae_density offer_density;
static int have_density;
static char found_root[AE_MCC_PATH_SIZE];
static char found_line[128];

static void found_picked(short button, void *context)
{
	(void)context;
	ae_mcc_offer_answered(button == 0);
	platform_log("mcc offer: MCC FOUND answered %s", button == 0 ? "USE MCC FILES" : "NOT NOW");
}

static int open_found(void)
{
	struct ae_attention_spec spec;
	char const *scripted = getenv("HALO_MCC_OFFER_ANSWER");

	ae_mcc_short_path(found_root, found_line, sizeof(found_line), 60);
	memset(&spec, 0, sizeof(spec));
	spec.title = "MCC FOUND";
	spec.body = "Halo: The Master Chief Collection is in Steam on this PC, but Arena Evolved isn't using its files. "
		"They play MCC maps and MCC servers, and give the menus MCC's music and announcers.";
	spec.item_title = "STEAM";
	spec.item_line1 = found_line;
	spec.item_line2 = "Only read: nothing in MCC is changed.";
	spec.item_tile = "FOUND";
	spec.buttons[0] = "USE MCC FILES";
	spec.buttons[1] = "NOT NOW";
	spec.button_count = 2;
	spec.focus = 0;
	spec.danger = -1;
	spec.back = 1;
	spec.note = "SETTINGS > SYSTEM > FILES > MCC FILES changes it later.";
	spec.back_label = "Not now";
	if (have_density)
		spec.density = offer_density;
	spec.picked = found_picked;
	if (!ae_attention_open(&spec))
		return 0;
	platform_log("mcc offer: MCC FOUND shown (%s)", found_root);
	if (scripted && (!strcmp(scripted, "use") || !strcmp(scripted, "not_now")))
	{
		platform_log("mcc offer: scripted answer %s", scripted);
		ae_attention_pick(!strcmp(scripted, "use") ? 0 : 1);
	}
	return 1;
}

static int open_missing(void);

static void missing_picked(short button, void *context)
{
	(void)context;
	if (button != 0)
	{
		platform_log("mcc offer: MCC NOT FOUND answered PLAY WITHOUT IT");
		return;
	}
	/* LOOK AGAIN: what is true now (found and used, found and off, still missing) */
	switch (ae_mcc_redetect())
	{
	case AE_MCC_FOUND:
		if (ae_mcc_offer_pending(found_root, sizeof(found_root)))
			open_found();
		else
			platform_log("mcc offer: LOOK AGAIN found MCC (game.mcc_use \"%s\")", ae_mcc_use() == AE_MCC_USE_YES ?
				"yes" : "not yes");
		break;
	default:
		platform_log("mcc offer: LOOK AGAIN: still not found");
		open_missing();
		break;
	}
}

static int open_missing(void)
{
	struct ae_attention_spec spec;

	memset(&spec, 0, sizeof(spec));
	spec.title = "MCC NOT FOUND";
	spec.body = "MCC maps and servers, and MCC's music and announcers in the menus, need Halo: The Master Chief "
		"Collection from Steam with Halo: Combat Evolved Anniversary installed. Arena Evolved looked in every Steam "
		"library on this PC.";
	spec.strong = "Everything else plays without it.";
	spec.buttons[0] = "LOOK AGAIN";
	spec.buttons[1] = "PLAY WITHOUT IT";
	spec.button_count = 2;
	spec.focus = 1;
	spec.danger = -1;
	spec.back = 1;
	spec.note = "Installed in another folder? SETTINGS > SYSTEM > FILES > MCC FILES > BROWSE.";
	spec.back_label = "Play without it";
	if (have_density)
		spec.density = offer_density;
	spec.picked = missing_picked;
	if (!ae_attention_open(&spec))
		return 0;
	ae_mcc_missing_told();
	platform_log("mcc offer: MCC NOT FOUND shown");
	return 1;
}

int ae_mcc_offer_open(struct ae_density const *density)
{
	if (offered)
		return 0;
	offered = 1;
	if (density)
	{
		offer_density = *density;
		have_density = 1;
	}
	if (ae_mcc_offer_pending(found_root, sizeof(found_root)))
		return open_found();
	if (ae_mcc_missing_pending())
		return open_missing();
	return 0;
}
